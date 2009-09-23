
struct x11mode
{
	unsigned int dotclock;
	unsigned short hdisplay;
	unsigned short hsyncstart;
	unsigned short hsyncend;
	unsigned short htotal;
	unsigned short hskew;
	unsigned short vdisplay;
	unsigned short vsyncstart;
	unsigned short vsyncend;
	unsigned short vtotal;
	unsigned int flags;
};

int modeline_to_x11mode(const char *modeline, struct x11mode *x11mode);

