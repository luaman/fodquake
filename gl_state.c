#include "gl_local.h"
#include "gl_state.h"

void GL_SetArrays(unsigned int arrays)
{
	static unsigned int old_arrays;
	unsigned int diff;

	diff = arrays ^ old_arrays;

	if (diff)
	{
		if ((diff & FQ_GL_VERTEX_ARRAY))
		{
			if ((arrays & FQ_GL_VERTEX_ARRAY))
				glEnableClientState(GL_VERTEX_ARRAY);
			else
				glDisableClientState(GL_VERTEX_ARRAY);
		}

		if ((diff & FQ_GL_COLOR_ARRAY))
		{
			if ((arrays & FQ_GL_COLOR_ARRAY))
				glEnableClientState(GL_COLOR_ARRAY);
			else
				glDisableClientState(GL_COLOR_ARRAY);
		}

		if ((diff & FQ_GL_TEXTURE_COORD_ARRAY))
		{
			if ((arrays & FQ_GL_TEXTURE_COORD_ARRAY))
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			else
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}

		old_arrays = arrays;
	}
}

