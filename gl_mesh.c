/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// gl_mesh.c: triangle model functions

#include <string.h>

#include "quakedef.h"

#include "gl_local.h"

/*
=================================================================

ALIAS MODEL DISPLAY LIST GENERATION

=================================================================
*/

static qboolean	used[8192];

// the command list holds counts and s/t values that are valid for
// every frame
static int		commands[8192];
static int		numcommands;

// all frames will have their vertexes rearranged and expanded
// so they are in the order expected by the command list
static int		vertexorder[8192];
static int		numorder;

static int		allverts, alltris;

static int		stripverts[128];
static int		striptris[128];

/*
================
StripLength
================
*/
static int StripLength(aliashdr_t *pheader, int starttri, int startv)
{
	int			m1, m2;
	int			j;
	mtriangle_t	*last, *check;
	int			k;
	int stripcount;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv)%3];
	stripverts[1] = last->vertindex[(startv+1)%3];
	stripverts[2] = last->vertindex[(startv+2)%3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv+2)%3];
	m2 = last->vertindex[(startv+1)%3];

	// look for a matching triangle
nexttri:
	for (j=starttri+1, check=&triangles[starttri+1] ; j<pheader->numtris && stripcount + 2 + 2 < (sizeof(stripverts)/sizeof(*stripverts)) && stripcount + 2 < (sizeof(striptris)/sizeof(*striptris)) ; j++, check++)
	{
		if (check->facesfront != last->facesfront)
			continue;
		for (k=0 ; k<3 ; k++)
		{
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[ (k+1)%3 ] != m2)
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (used[j])
				goto done;

			// the new edge
			if (stripcount & 1)
				m2 = check->vertindex[ (k+2)%3 ];
			else
				m1 = check->vertindex[ (k+2)%3 ];

			stripverts[stripcount+2] = check->vertindex[ (k+2)%3 ];
			striptris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}
done:

	// clear the temp used flags
	for (j=starttri+1 ; j<pheader->numtris ; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}

/*
===========
FanLength
===========
*/
static int FanLength(aliashdr_t *pheader, int starttri, int startv)
{
	int		m1, m2;
	int		j;
	mtriangle_t	*last, *check;
	int		k;
	int fancount;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv)%3];
	stripverts[1] = last->vertindex[(startv+1)%3];
	stripverts[2] = last->vertindex[(startv+2)%3];

	striptris[0] = starttri;
	fancount = 1;

	m1 = last->vertindex[(startv+0)%3];
	m2 = last->vertindex[(startv+2)%3];


	// look for a matching triangle
nexttri:
	for (j=starttri+1, check=&triangles[starttri+1] ; j<pheader->numtris && fancount + 2 + 2 < (sizeof(stripverts)/sizeof(*stripverts)) && fancount + 2 < (sizeof(striptris)/sizeof(*striptris)); j++, check++)
	{
		if (check->facesfront != last->facesfront)
			continue;
		for (k=0 ; k<3 ; k++)
		{
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[ (k+1)%3 ] != m2)
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (used[j])
				goto done;

			// the new edge
			m2 = check->vertindex[ (k+2)%3 ];

			stripverts[fancount+2] = m2;
			striptris[fancount] = j;
			fancount++;

			used[j] = 2;
			goto nexttri;
		}
	}
done:

	// clear the temp used flags
	for (j=starttri+1 ; j<pheader->numtris ; j++)
		if (used[j] == 2)
			used[j] = 0;

	return fancount;
}


/*
================
BuildTris

Generate a list of trifans or strips
for the model, which holds for all frames
================
*/
static void BuildTris(aliashdr_t *pheader)
{
	int		i, j, k;
	int		startv;
	float	s, t;
	int		len, bestlen, besttype;
	int		bestverts[1024];
	int		besttris[1024];
	int		type;

	if (pheader->numtris > (sizeof(used)/sizeof(*used)))
		Sys_Error("Sorry, a 'tard wrote this code.");

	//
	// build tristrips
	//
	numorder = 0;
	numcommands = 0;
	memset (used, 0, sizeof(used));
	for (i=0 ; i<pheader->numtris ; i++)
	{
		// pick an unused triangle and start the trifan
		if (used[i])
			continue;

		bestlen = 0;
		for (type = 0 ; type < 2 ; type++)
//	type = 1;
		{
			for (startv =0 ; startv < 3 ; startv++)
			{
				if (type == 1)
					len = StripLength(pheader, i, startv);
				else
					len = FanLength(pheader, i, startv);

				if (len > bestlen)
				{
					if (len + 2 >= (sizeof(bestverts)/sizeof(*bestverts)))
						Sys_Error("Sorry, a 'tard wrote this code.");

					if (len >= (sizeof(besttris)/sizeof(*besttris)))
						Sys_Error("Sorry, a 'tard wrote this code.");

					besttype = type;
					bestlen = len;
					for (j=0 ; j<bestlen+2 ; j++)
						bestverts[j] = stripverts[j];
					for (j=0 ; j<bestlen ; j++)
						besttris[j] = striptris[j];
				}
			}
		}

		// mark the tris on the best strip as used
		for (j=0 ; j<bestlen ; j++)
			used[besttris[j]] = 1;

		if (numcommands + 2 >= (sizeof(commands)/sizeof(*commands)))
			Sys_Error("Sorry, a 'tard wrote this code.");

		if (besttype == 1)
			commands[numcommands++] = (bestlen+2);
		else
			commands[numcommands++] = -(bestlen+2);

		for (j=0 ; j<bestlen+2 ; j++)
		{
			// emit a vertex into the reorder buffer
			k = bestverts[j];
			vertexorder[numorder++] = k;

			// emit s/t coords into the commands stream
			s = stverts[k].s;
			t = stverts[k].t;
			if (!triangles[besttris[0]].facesfront && stverts[k].onseam)
				s += pheader->skinwidth / 2;	// on back side
			s = (s + 0.5) / pheader->skinwidth;
			t = (t + 0.5) / pheader->skinheight;

			if (numcommands + 3 >= (sizeof(commands)/sizeof(*commands)))
				Sys_Error("Sorry, a 'tard wrote this code.");

			*(float *)&commands[numcommands++] = s;
			*(float *)&commands[numcommands++] = t;
		}
	}

	commands[numcommands++] = 0;		// end of list marker

	Com_DPrintf ("%3i tri %3i vert %3i cmd\n", pheader->numtris, numorder, numcommands);

	allverts += numorder;
	alltris += pheader->numtris;
}

static void MakeVBO(aliashdr_t *hdr)
{
	int *vertposes;
	float *vboverts;
	float *vbotexcoords;
	float s;
	float t;
	unsigned int *colours;
	unsigned short *vbotris;
	unsigned short min;
	unsigned short max;
	int pose;
	int vert;
	int tri;
	int i;
	int j;
	char *backside;
	unsigned int *collisionverts;
	unsigned int *collisionmap;
	unsigned int totalverts;
	unsigned char **lnis;
	unsigned char **lnimap;
	unsigned char *lnicount;
	unsigned char lnirevmap[256];
	unsigned char lniused[256];
	unsigned char uniquelnis;

	if (!gl_vbo)
		return;

	collisionverts = malloc(hdr->numverts*sizeof(*collisionverts));
	collisionmap = malloc(hdr->numverts*sizeof(*collisionmap));
	backside = malloc(hdr->numverts);

	if (collisionverts && collisionmap && backside)
	{
		char new;
		unsigned int collisions;

		collisions = 0;

		memset(backside, 3, hdr->numverts);

		for(tri=0;tri<hdr->numtris;tri++)
		{
			for(i=0;i<3;i++)
			{
				vert = triangles[tri].vertindex[i];
				if (!triangles[tri].facesfront && stverts[vert].onseam)
					new = 1;
				else
					new = 0;

				if (backside[vert] == 3)
					backside[vert] = new;
				else if (backside[vert] != 2 && backside[vert] != new)
				{
					collisionverts[collisions] = vert;
					collisionmap[vert] = hdr->numverts + collisions;
					backside[vert] = 2;
					collisions++;
				}
			}
		}

		totalverts = hdr->numverts + collisions;

		lnis = malloc(hdr->numposes * sizeof(*lnis));
		lnimap = malloc(hdr->numposes * sizeof(*lnimap));
		lnicount = malloc(hdr->numposes * sizeof(*hdr->lnicount));

		vboverts = malloc(hdr->numposes * totalverts * 3 * sizeof(*vboverts));
		vbotexcoords = malloc(totalverts * 2 * sizeof(vbotexcoords));
		vbotris = malloc(hdr->numtris*3*sizeof(*vbotris));
		vertposes = malloc(hdr->numposes*sizeof(*vertposes));

		colours = malloc(totalverts*sizeof(*hdr->colours));

		for(pose=0;pose<hdr->numposes;pose++)
		{
			lnimap[pose] = malloc(totalverts * sizeof(**lnimap));
		}

		if (lnis && lnimap && lnicount && vboverts && vbotexcoords && vbotris && vertposes)
		{
			for(pose=0;pose<hdr->numposes;pose++)
			{
				memset(lniused, 0, sizeof(lniused));
				memset(lnirevmap, 0, sizeof(lnirevmap));
				uniquelnis = 0;
				max = 0;
				min = 255;

				for(vert=0;vert<hdr->numverts;vert++)
				{
					if (poseverts[pose][vert].lightnormalindex < min)
						min = poseverts[pose][vert].lightnormalindex;
					if (poseverts[pose][vert].lightnormalindex > max)
						max = poseverts[pose][vert].lightnormalindex;

					if (!lniused[poseverts[pose][vert].lightnormalindex])
					{
						lniused[poseverts[pose][vert].lightnormalindex] = 1;
						uniquelnis++;
					}
				}

				lnis[pose] = malloc(uniquelnis * sizeof(**lnis));

				for(i=0,j=0;i<256;i++)
				{
					if (lniused[i])
					{
						lnirevmap[i] = j;
						lnis[pose][j++] = i;
					}
				}

				for(vert=0;vert<hdr->numverts;vert++)
				{
					lnimap[pose][vert] = lnirevmap[poseverts[pose][vert].lightnormalindex];
				}

				for(;vert<totalverts;vert++)
				{
					lnimap[pose][vert] = lnirevmap[poseverts[pose][collisionverts[vert-hdr->numverts]].lightnormalindex];
				}

				lnicount[pose] = uniquelnis;
			}

			for(pose=0;pose<hdr->numposes;pose++)
			{
				for(vert=0;vert<hdr->numverts;vert++)
				{
					vboverts[pose*totalverts*3 + vert*3 + 0] = poseverts[pose][vert].v[0];
					vboverts[pose*totalverts*3 + vert*3 + 1] = poseverts[pose][vert].v[1];
					vboverts[pose*totalverts*3 + vert*3 + 2] = poseverts[pose][vert].v[2];
				}

				for(;vert<totalverts;vert++)
				{
					vboverts[pose*totalverts*3 + vert*3 + 0] = poseverts[pose][collisionverts[vert-hdr->numverts]].v[0];
					vboverts[pose*totalverts*3 + vert*3 + 1] = poseverts[pose][collisionverts[vert-hdr->numverts]].v[1];
					vboverts[pose*totalverts*3 + vert*3 + 2] = poseverts[pose][collisionverts[vert-hdr->numverts]].v[2];
				}
			}

			for(vert=0;vert<hdr->numverts;vert++)
			{
				s = stverts[vert].s;
				t = stverts[vert].t;
				if (backside[vert] == 1)
					s += hdr->skinwidth / 2;

				vbotexcoords[vert*2+0] = (s + 0.5) / hdr->skinwidth;
				vbotexcoords[vert*2+1] = (t + 0.5) / hdr->skinheight;
			}

			for(;vert<totalverts;vert++)
			{
				s = stverts[collisionverts[vert-hdr->numverts]].s;
				t = stverts[collisionverts[vert-hdr->numverts]].t;
				s += hdr->skinwidth / 2;

				vbotexcoords[vert*2+0] = (s + 0.5) / hdr->skinwidth;
				vbotexcoords[vert*2+1] = (t + 0.5) / hdr->skinheight;
			}

			min = 65535;
			max = 0;
			for(tri=0;tri<hdr->numtris;tri++)
			{
				for(i=0;i<3;i++)
				{
					vert = triangles[tri].vertindex[i];

					if (!triangles[tri].facesfront && stverts[vert].onseam && backside[vert] != 1)
						vert = collisionmap[vert];

					vbotris[tri*3 + i] = vert;

					if (vert > max)
						max = vert;
					if (vert < min)
						min = vert;
				}
			}

			hdr->colours = colours;

			hdr->indices = vbotris;
			hdr->indexmin = min;
			hdr->indexmax = max;
			vbotris = 0;

			hdr->vbovertcount = totalverts;

			hdr->lnis = lnis;
			hdr->lnimap = lnimap;
			hdr->lnicount = lnicount;
			lnis = 0;
			lnimap = 0;
			lnicount = 0;

			hdr->vert_vbo_number = vertposes;
			vertposes = 0;

			for(i=0;i<hdr->numposes;i++)
			{
				hdr->vert_vbo_number[i] = vbo_number++;

				qglBindBufferARB(GL_ARRAY_BUFFER_ARB, hdr->vert_vbo_number[i]);
				qglBufferDataARB(GL_ARRAY_BUFFER_ARB, totalverts*3*sizeof(*vboverts), vboverts + totalverts*3*i, GL_STATIC_DRAW_ARB);
			}

			hdr->texcoord_vbo_number = vbo_number++;

			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, hdr->texcoord_vbo_number);
			qglBufferDataARB(GL_ARRAY_BUFFER_ARB, totalverts*2*sizeof(*vbotexcoords), vbotexcoords, GL_STATIC_DRAW_ARB);

			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		}

		free(vertposes);
		free(vbotris);
		free(vbotexcoords);
		free(vboverts);
		free(lnicount);
		free(lnimap);
		free(lnis);
	}

	free(backside);
	free(collisionmap);
	free(collisionverts);
}

/*
================
GL_MakeAliasModelDisplayLists
================
*/
void GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr)
{
	aliashdr_t *paliashdr;
	int		i, j;
	int			*cmds;
	trivertx_t	*verts;

	paliashdr = hdr;	// (aliashdr_t *)Mod_Extradata (m);

	// Tonik: don't cache anything, because it seems just as fast
	// (if not faster) to rebuild the tris instead of loading them from disk
	BuildTris(paliashdr);		// trifans or lists

	MakeVBO(hdr);

	// save the data out

	paliashdr->poseverts = numorder;

	cmds = malloc(numcommands * 4);
	if (cmds == 0)
		Sys_Error("GL_MakeAliasModelDisplayLists: Out of memory\n");

	paliashdr->commands = cmds;
	memcpy (cmds, commands, numcommands * 4);

	verts = malloc(paliashdr->numposes * paliashdr->poseverts * sizeof(trivertx_t));
	if (verts == 0)
		Sys_Error("GL_MakeAliasModelDisplayLists: Out of memory\n");

	paliashdr->posedata = verts;
	for (i=0 ; i<paliashdr->numposes ; i++)
		for (j=0 ; j<numorder ; j++)
			*verts++ = poseverts[i][vertexorder[j]];
}

