#include <stdlib.h>

#include "gl_local.h"
#include "gl_shader.h"

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

int GL_SetupShaderProgram(int vertexobject, const char *vertexshader, int fragmentobject, const char *fragmentshader)
{
	int programobject;
	int linked;

	if ((vertexobject && vertexshader) || (fragmentobject && fragmentshader))
		return 0;

	if (vertexshader)
	{
		vertexobject = GL_CompileShader(GL_VERTEX_SHADER_ARB, vertexshader);
	}

	if (fragmentshader)
	{
		fragmentobject = GL_CompileShader(GL_FRAGMENT_SHADER_ARB, fragmentshader);
	}

	if ((vertexobject || vertexshader == 0) && (fragmentobject || fragmentshader == 0))
	{
		programobject = qglCreateProgramObjectARB();

		if (vertexobject)
			qglAttachObjectARB(programobject, vertexobject);

		if (fragmentobject)
			qglAttachObjectARB(programobject, fragmentobject);
	}
	else
		programobject = 0;

	if (vertexshader && vertexobject)
		qglDeleteObjectARB(vertexobject);

	if (fragmentshader && fragmentobject)
		qglDeleteObjectARB(fragmentobject);

	if (!programobject)
		return 0;

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

