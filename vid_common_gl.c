/*
Copyright (C) 2002-2003 A Nourai

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

// vid_common_gl.c -- Common code for vid_wgl.c and vid_glx.c

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "quakedef.h"
#include "gl_local.h"
#include "gl_state.h"

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qboolean gl_mtexable = false;
int gl_textureunits = 1;
void (*qglBindBufferARB)(GLenum, GLuint);
void (*qglBufferDataARB)(GLenum, GLsizeiptrARB, const GLvoid *, GLenum);

/* GLSL stuff */
void (*qglAttachObjectARB)(GLhandleARB, GLhandleARB);
void (*qglCompileShaderARB)(GLhandleARB);
GLhandleARB (*qglCreateProgramObjectARB)(void);
GLhandleARB (*qglCreateShaderObjectARB)(GLenum);
void (*qglDeleteObjectARB)(GLhandleARB);
void (*qglGetInfoLogARB)(GLhandleARB, GLsizei, GLsizei *, GLcharARB *);
void (*qglGetObjectParameterivARB)(GLhandleARB, GLenum, GLint *);
GLint (*qglGetUniformLocationARB)(GLhandleARB, const GLcharARB *);
void (*qglLinkProgramARB)(GLhandleARB);
void (*qglShaderSourceARB)(GLhandleARB, GLsizei, const GLcharARB* *, const GLint *);
void (*qglUniform1fARB)(GLint, GLfloat);
void (*qglUseProgramObjectARB)(GLhandleARB);

qboolean gl_combine = false;

qboolean gl_add_ext = false;

qboolean gl_npot;

qboolean gl_vbo = false;

qboolean gl_fs;

float gldepthmin, gldepthmax;

float vid_gamma = 1.0;
byte vid_gamma_table[256];

unsigned short d_8to16table[256];
unsigned d_8to24table[256];
unsigned d_8to24table2[256];

byte color_white[4] = {255, 255, 255, 0};
byte color_black[4] = {0, 0, 0, 0};

qboolean OnChange_gl_ext_texture_compression(cvar_t *, char *);

cvar_t gl_strings = {"gl_strings", "", CVAR_ROM};
cvar_t gl_ext_texture_compression = {"gl_ext_texture_compression", "0", 0, OnChange_gl_ext_texture_compression};

/************************************* EXTENSIONS *************************************/

qboolean CheckExtension (const char *extension)
{
	const char *start;
	char *where, *terminator;

	if (!gl_extensions && !(gl_extensions = (char *)glGetString (GL_EXTENSIONS)))
		return false;

	if (!extension || *extension == 0 || strchr (extension, ' '))
		return false;

	for (start = gl_extensions; (where = strstr(start, extension)); start = terminator)
	{
		terminator = where + strlen (extension);
		if ((where == start || *(where - 1) == ' ') && (*terminator == 0 || *terminator == ' '))
			return true;
	}

	return false;
}

void CheckMultiTextureExtensions (void)
{
	if (COM_CheckParm("-nomtex"))
		return;

	if (strstr(gl_renderer, "Savage"))
	{
		Com_Printf("Multitexturing disabled for this graphics card.\n");
		return;
	}

	glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &gl_textureunits);

	if (COM_CheckParm("-maxtmu2")/* || !strcmp(gl_vendor, "ATI Technologies Inc.")*/)
		gl_textureunits = min(gl_textureunits, 2);

	gl_textureunits = min(gl_textureunits, 4);
	gl_mtexable = true;

	if (gl_textureunits < 2)
		gl_mtexable = false;

	if (!gl_mtexable)
		Com_Printf("OpenGL multitexturing extensions not available.\n");
}

void GL_CheckExtensions (void)
{
	CheckMultiTextureExtensions ();

	gl_combine = CheckExtension("GL_ARB_texture_env_combine");
	gl_add_ext = CheckExtension("GL_ARB_texture_env_add");
	gl_npot = CheckExtension("GL_ARB_texture_non_power_of_two");
	gl_vbo = CheckExtension("GL_ARB_vertex_buffer_object");
	gl_fs = CheckExtension("GL_ARB_fragment_shader");

	if (gl_vbo)
	{
		qglBindBufferARB = VID_GetProcAddress("glBindBufferARB");
		qglBufferDataARB = VID_GetProcAddress("glBufferDataARB");

		if (qglBindBufferARB == 0 || qglBufferDataARB == 0)
			gl_vbo = false;
	}

	if (gl_fs)
	{
		qglAttachObjectARB = VID_GetProcAddress("glAttachObjectARB");
		qglCompileShaderARB = VID_GetProcAddress("glCompileShaderARB");
		qglCreateProgramObjectARB = VID_GetProcAddress("glCreateProgramObjectARB");
		qglCreateShaderObjectARB = VID_GetProcAddress("glCreateShaderObjectARB");
		qglDeleteObjectARB = VID_GetProcAddress("glDeleteObjectARB");
		qglGetInfoLogARB = VID_GetProcAddress("glGetInfoLogARB");
		qglGetObjectParameterivARB = VID_GetProcAddress("glGetObjectParameterivARB");
		qglGetUniformLocationARB = VID_GetProcAddress("glGetUniformLocationARB");
		qglLinkProgramARB = VID_GetProcAddress("glLinkProgramARB");
		qglShaderSourceARB = VID_GetProcAddress("glShaderSourceARB");
		qglUniform1fARB = VID_GetProcAddress("glUniform1fARB");
		qglUseProgramObjectARB = VID_GetProcAddress("glUseProgramObjectARB");

		if (qglAttachObjectARB == 0
		 || qglCompileShaderARB == 0
		 || qglCreateProgramObjectARB == 0
		 || qglCreateShaderObjectARB == 0
		 || qglDeleteObjectARB == 0
		 || qglGetInfoLogARB == 0
		 || qglGetObjectParameterivARB == 0
		 || qglGetUniformLocationARB == 0
		 || qglLinkProgramARB == 0
		 || qglShaderSourceARB == 0
		 || qglUniform1fARB == 0
		 || qglUseProgramObjectARB == 0)
		{
			gl_fs = 0;
		}
	}

	if (CheckExtension("GL_ARB_texture_compression"))
	{
		Com_Printf("Texture compression extensions found\n");
	}
}

qboolean OnChange_gl_ext_texture_compression(cvar_t *var, char *string)
{
	float newval = Q_atof(string);

	if (!newval == !var->value)
		return false;

	gl_alpha_format = newval ? GL_COMPRESSED_RGBA_ARB : GL_RGBA;
	gl_solid_format = newval ? GL_COMPRESSED_RGB_ARB : GL_RGB;

	return false;
}

/************************************** GL INIT **************************************/

void GL_CvarInit()
{
	Cvar_Register (&gl_strings);
	Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
	Cvar_Register (&gl_ext_texture_compression);	
	Cvar_ResetCurrentGroup();
}

void GL_Init (void)
{
	gl_vendor = (char *)glGetString(GL_VENDOR);
	Com_Printf("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = (char *)glGetString(GL_RENDERER);
	Com_Printf("GL_RENDERER: %s\n", gl_renderer);
	gl_version = (char *)glGetString(GL_VERSION);
	Com_Printf("GL_VERSION: %s\n", gl_version);
	gl_extensions = (char *)glGetString(GL_EXTENSIONS);
	if (COM_CheckParm("-gl_ext"))
		Com_Printf("GL_EXTENSIONS: %s\n", gl_extensions);

	Cvar_ForceSet (&gl_strings, va("GL_VENDOR: %s\nGL_RENDERER: %s\n"
		"GL_VERSION: %s\nGL_EXTENSIONS: %s", gl_vendor, gl_renderer, gl_version, gl_extensions));

	glClearColor (1,0,0,0);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_FLAT);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	GL_CheckExtensions();

	GL_SetArrays(0);
}

/************************************* VID GAMMA *************************************/

void Check_Gamma (unsigned char *pal)
{
	float inf;
	unsigned char palette[768];
	int i;

	if ((i = COM_CheckParm("-gamma")) != 0 && i + 1 < com_argc)
		vid_gamma = bound (0.3, Q_atof(com_argv[i + 1]), 1);
	else
		vid_gamma = 1;

	Cvar_SetDefault (&v_gamma, vid_gamma);

	if (vid_gamma != 1)
	{
		for (i = 0; i < 256; i++)
		{
			inf = 255 * pow((i + 0.5) / 255.5, vid_gamma) + 0.5;
			if (inf > 255)
				inf = 255;

			vid_gamma_table[i] = inf;
		}
	}
	else
	{
		for (i = 0; i < 256; i++)
			vid_gamma_table[i] = i;
	}

	for (i = 0; i < 768; i++)
		palette[i] = vid_gamma_table[pal[i]];

	memcpy (pal, palette, sizeof(palette));
}

/************************************* HW GAMMA *************************************/

void VID_SetPalette (unsigned char *palette)
{
	int i;
	byte *pal;
	unsigned r,g,b, v, *table;

	// 8 8 8 encoding
	pal = palette;
	table = d_8to24table;
	for (i = 0; i < 256; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

		v = (255 << 24) + (b << 16) + (g << 8) + (r << 0);
		*table++ = LittleLong(v);
	}
	d_8to24table[255] = 0;	// 255 is transparent

	// Tonik: create a brighter palette for bmodel textures
	pal = palette;
	table = d_8to24table2;

	for (i = 0; i < 256; i++)
	{
		r = pal[0] * (2.0 / 1.5); if (r > 255) r = 255;
		g = pal[1] * (2.0 / 1.5); if (g > 255) g = 255;
		b = pal[2] * (2.0 / 1.5); if (b > 255) b = 255;
		pal += 3;
		v = (255 << 24) + (b << 16) + (g << 8) + (r << 0);
		*table++ = LittleLong(v);
	}

	d_8to24table2[255] = 0;	// 255 is transparent
}

static int GL_CompileShader(GLenum type, const char *shader)
{
	int object;
	int compiled;

	object = qglCreateShaderObjectARB(type);
	qglShaderSourceARB(object, 1, &shader, 0);
	qglCompileShaderARB(object);
	qglGetObjectParameterivARB(object, GL_OBJECT_COMPILE_STATUS_ARB, &compiled);
	if (!compiled)
	{
		char *log;
		int loglength;

		fprintf(stderr, "Failed to compile the following shader:\n%s\n", shader);

		qglGetObjectParameterivARB(object, GL_OBJECT_INFO_LOG_LENGTH_ARB, &loglength);
		log = malloc(loglength);
		if (log)
		{
			qglGetInfoLogARB(object, loglength, NULL, log);

			fprintf(stderr, "OpenGL returned:\n%s\n", log);

			free(log);
		}

		qglDeleteObjectARB(object);

		return 0;
	}

	return object;
}

int GL_SetupShaderProgram(const char *vertexshader, const char *fragmentshader)
{
	int programobject;
	int shaderobject;
	int linked;
	int ok;

	ok = 1;

	programobject = qglCreateProgramObjectARB();

	if (vertexshader)
	{
		shaderobject = GL_CompileShader(GL_VERTEX_SHADER_ARB, vertexshader);
		if (shaderobject)
		{
			qglAttachObjectARB(programobject, shaderobject);
			qglDeleteObjectARB(shaderobject);
		}
		else
			ok = 0;
	}

	if (ok && fragmentshader)
	{
		shaderobject = GL_CompileShader(GL_FRAGMENT_SHADER_ARB, fragmentshader);
		if (shaderobject)
		{
			qglAttachObjectARB(programobject, shaderobject);
			qglDeleteObjectARB(shaderobject);
		}
		else
			ok = 0;
	}

	if (!ok)
	{
		qglDeleteObjectARB(programobject);
		return 0;
	}

	qglLinkProgramARB(programobject);
	qglGetObjectParameterivARB(programobject, GL_OBJECT_LINK_STATUS_ARB, &linked);

	if (!linked)
	{
		char *log;
		int loglength;

		fprintf(stderr, "Failed to link the following shader(s):\n");
		if (vertexshader)
			fprintf(stderr, "Vertex shader:\n%s\n", vertexshader);

		if (fragmentshader)
			fprintf(stderr, "Fragment shader:\n%s\n", fragmentshader);

		qglGetObjectParameterivARB(programobject, GL_OBJECT_INFO_LOG_LENGTH_ARB, &loglength);
		log = malloc(loglength);
		if (log)
		{
			qglGetInfoLogARB(programobject, loglength, NULL, log);

			fprintf(stderr, "OpenGL returned:\n%s\n", log);

			free(log);
		}

		qglDeleteObjectARB(programobject);

		return 0;
	}

	return programobject;
}

