
#include <math.h>

#include "gl_local.h"

#include "particles.h"

static float r_partscale;
static vec3_t up, right;

void GL_DrawParticleBegin()
{
	r_partscale = 0.004 * tan(r_refdef.fov_x * (M_PI / 180) * 0.5f);

	GL_Bind(particletexture);

	glEnable(GL_BLEND);
	if (!gl_solidparticles.value)
		glDepthMask (GL_FALSE);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBegin(GL_TRIANGLES);

	VectorScale(vup, 1.5, up);
	VectorScale(vright, 1.5, right);
}

void GL_DrawParticleEnd()
{
	glEnd();
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor3ubv(color_white);
}

void GL_DrawParticle(particle_t *p)
{
	unsigned char *at, theAlpha;
	float dist, scale;

	// hack a scale up to keep particles from disapearing
	dist = (p->org[0] - r_origin[0]) * vpn[0] + (p->org[1] - r_origin[1]) * vpn[1] + (p->org[2] - r_origin[2]) * vpn[2];
	scale = 1 + dist * r_partscale;

	at = (byte *) &d_8to24table[(int)p->color];
	if (p->type == pt_fire)
		theAlpha = 255 * (6 - p->ramp) / 6;
	else
		theAlpha = 255;
	glColor4ub (*at, *(at + 1), *(at + 2), theAlpha);
	glTexCoord2f(0, 0); glVertex3fv(p->org);
	glTexCoord2f(1, 0); glVertex3f(p->org[0] + up[0] * scale, p->org[1] + up[1] * scale, p->org[2] + up[2] * scale);
	glTexCoord2f(0, 1); glVertex3f(p->org[0] + right[0] * scale, p->org[1] + right[1] * scale, p->org[2] + right[2] * scale);
}

