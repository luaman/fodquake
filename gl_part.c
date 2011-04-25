
#include <math.h>

#include "gl_local.h"

#include "particles.h"

static float r_partscale;
static vec3_t up, right;

#define NUMBUFFEREDPARTVERTICES 60
#if (NUMBUFFEREDPARTVERTICES%3) != 0
#error Fail.
#endif

static float particlevertices[3*NUMBUFFEREDPARTVERTICES] __attribute__((aligned(64)));
static unsigned int particlecolours[NUMBUFFEREDPARTVERTICES] __attribute__((aligned(64)));
static int particleindex;

static const float particletexcoords[2*NUMBUFFEREDPARTVERTICES] __attribute__((aligned(64))) =
{
	0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
	0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
	0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
	0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
	0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
};

void GL_DrawParticleBegin()
{
	r_partscale = 0.004 * tan(r_refdef.fov_x * (M_PI / 180) * 0.5f);

	GL_Bind(particletexture);

	glEnable(GL_BLEND);
	if (!gl_solidparticles.value)
		glDepthMask (GL_FALSE);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, particlevertices);
	glTexCoordPointer(2, GL_FLOAT, 0, particletexcoords);
	glColorPointer(4, GL_UNSIGNED_BYTE, 0, particlecolours);

	particleindex = 0;

	VectorScale(vup, 1.5, up);
	VectorScale(vright, 1.5, right);
}

void GL_DrawParticleEnd()
{
	if (particleindex)
		glDrawArrays(GL_TRIANGLES, 0, particleindex);

	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor3ubv(color_white);
}

void GL_DrawParticle(particle_t *p)
{
	float *pv;
	unsigned int *pc;
	unsigned char *at, theAlpha;
	float dist, scale;
	float *lup;
	float lup0;
	float lup1;
	float lup2;
	float *lright;
	float lright0;
	float lright1;
	float lright2;
	unsigned int i;
	union
	{
		unsigned char uc[4];
		unsigned int ui;
	} col;

	// hack a scale up to keep particles from disapearing
	dist = (p->org[0] - r_origin[0]) * vpn[0] + (p->org[1] - r_origin[1]) * vpn[1] + (p->org[2] - r_origin[2]) * vpn[2];
	scale = 1 + dist * r_partscale;

	at = (byte *) &d_8to24table[(int)p->color];
	if (p->type == pt_fire)
		theAlpha = 255 * (6 - p->ramp) / 6;
	else
		theAlpha = 255;

	col.uc[0] = at[0];
	col.uc[1] = at[1];
	col.uc[2] = at[2];
	col.uc[3] = theAlpha;

	i = particleindex;

	pv = particlevertices + i * 3;

	lup = up;
	lup0 = lup[0] * scale;
	lup1 = lup[1] * scale;
	lup2 = lup[2] * scale;

	lright = right;
	lright0 = lright[0] * scale;
	lright1 = lright[1] * scale;
	lright2 = lright[2] * scale;

	pv[0] = p->org[0];
	pv[1] = p->org[1];
	pv[2] = p->org[2];

	pv[3] = p->org[0] + lup0;
	pv[4] = p->org[1] + lup1;
	pv[5] = p->org[2] + lup2;

	pv[6] = p->org[0] + lright0;
	pv[7] = p->org[1] + lright1;
	pv[8] = p->org[2] + lright2;

	pc = particlecolours + i;

	pc[0] = col.ui;
	pc[1] = col.ui;
	pc[2] = col.ui;

	particleindex = i + 3;

	if (particleindex == NUMBUFFEREDPARTVERTICES)
	{
		glDrawArrays(GL_TRIANGLES, 0, particleindex);
		particleindex = 0;
	}
}

