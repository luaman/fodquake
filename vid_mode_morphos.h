struct modeinfo
{
	char monitorname[64]; /* Mneh, this is longer than it can be anyway :P */
	unsigned int width;
	unsigned int height;
	unsigned int depth;
};

int modeline_to_modeinfo(const char *modeline, struct modeinfo *modeinfo);
