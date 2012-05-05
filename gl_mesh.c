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

#include <stdlib.h>
#include <string.h>

#include "quakedef.h"

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

	besttype = 0;

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

static int MakeCollisionMap(aliashdr_t *hdr, unsigned short **collisionrevmap_ret, unsigned char **backside_ret)
{
	unsigned short *collisionmap;
	unsigned short *collisionrevmap;
	unsigned int collisions;
	unsigned char *backside;
	unsigned int vert;
	unsigned int tri;
	unsigned int i;
	char new;
	int ret;

	collisionmap = malloc(hdr->numverts*sizeof(*collisionmap));
	collisionrevmap = malloc(hdr->numverts*sizeof(*collisionrevmap));
	backside = malloc(hdr->numverts);

	ret = 0;

	if (collisionmap && collisionrevmap && backside)
	{
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
					collisionmap[collisions] = vert;
					collisionrevmap[vert] = hdr->numverts + collisions;
					backside[vert] = 2;
					collisions++;
				}
			}
		}

		hdr->totalverts = hdr->numverts + collisions;
		hdr->collisions = collisions;
		hdr->collisionmap = malloc(collisions * sizeof(*hdr->collisionmap));
		if (hdr->collisionmap)
		{
			for(i=0;i<collisions;i++)
			{
				hdr->collisionmap[i] = collisionmap[i];
			}

			*collisionrevmap_ret = collisionrevmap;
			*backside_ret = backside;

			ret = 1;
		}
	}

	free(collisionmap);
	if (!ret)
	{
		free(collisionrevmap);
		free(backside);
	}

	return ret;
}

static int MakeIndices(aliashdr_t *hdr, unsigned short *collisionrevmap, unsigned char *backside)
{
	unsigned short *indices;
	unsigned short min;
	unsigned short max;
	unsigned int vert;
	unsigned int tri;
	unsigned int i;

	indices = malloc(hdr->numtris*3*sizeof(*indices));
	if (indices)
	{
		min = 65535;
		max = 0;
		for(tri=0;tri<hdr->numtris;tri++)
		{
			for(i=0;i<3;i++)
			{
				vert = triangles[tri].vertindex[i];

				if (!triangles[tri].facesfront && stverts[vert].onseam && backside[vert] != 1)
					vert = collisionrevmap[vert];

				indices[tri*3 + i] = vert;

				if (vert > max)
					max = vert;
				if (vert < min)
					min = vert;
			}
		}

		hdr->indices = indices;
		hdr->indexmin = min;
		hdr->indexmax = max;

		return 1;
	}

	return 0;
}

static int MakeTextureCoordinates(aliashdr_t *hdr, unsigned char *backside)
{
	int vert;
	float s;
	float t;
	float *texcoords;

	texcoords = malloc(hdr->totalverts * 2 * sizeof(texcoords));
	if (texcoords)
	{
		for(vert=0;vert<hdr->numverts;vert++)
		{
			s = stverts[vert].s;
			t = stverts[vert].t;
			if (backside[vert] == 1)
				s += hdr->skinwidth / 2;

			texcoords[vert*2+0] = (s + 0.5) / hdr->skinwidth;
			texcoords[vert*2+1] = (t + 0.5) / hdr->skinheight;
		}

		for(;vert<hdr->totalverts;vert++)
		{
			s = stverts[hdr->collisionmap[vert-hdr->numverts]].s;
			t = stverts[hdr->collisionmap[vert-hdr->numverts]].t;
			s += hdr->skinwidth / 2;

			texcoords[vert*2+0] = (s + 0.5) / hdr->skinwidth;
			texcoords[vert*2+1] = (t + 0.5) / hdr->skinheight;
		}

		hdr->texcoords = texcoords;

		return 1;
	}

	return 0;
}

/*
================
GL_MakeAliasModelDisplayLists
================
*/
void GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr)
{
	aliashdr_t *paliashdr;
	unsigned short *collisionrevmap;
	unsigned char *backside;
	int		i, j;
	int			*cmds;
	trivertx_t	*verts;

	paliashdr = hdr;	// (aliashdr_t *)Mod_Extradata (m);

	// Tonik: don't cache anything, because it seems just as fast
	// (if not faster) to rebuild the tris instead of loading them from disk
	BuildTris(paliashdr);		// trifans or lists

	if (!MakeCollisionMap(hdr, &collisionrevmap, &backside))
		Sys_Error("GL_MakeAliasModelDisplayLists: MakeCollisionMap() failed.");

	if (!MakeIndices(hdr, collisionrevmap, backside))
		Sys_Error("GL_MakeAliasModelDisplayLists: MakeIndices() failed.");

	if (!MakeTextureCoordinates(hdr, backside))
		Sys_Error("GL_MakeAliasModelDisplayLists: MakeTextureCoordinates() failed.");

	free(collisionrevmap);
	free(backside);

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

	paliashdr->realposeverts = malloc(paliashdr->numverts * paliashdr->numposes * sizeof(*paliashdr->realposeverts));
	for(i=0;i<paliashdr->numposes;i++)
	{
		memcpy(paliashdr->realposeverts + i * paliashdr->numverts, poseverts[i], paliashdr->numverts * sizeof(*paliashdr->realposeverts));
	}
}

