struct huffenctable_s
{
	unsigned char len;
	unsigned short code;
};

struct huffdectable_s
{
	unsigned char value;
	unsigned char len;
};

struct hufftables
{
	struct huffenctable_s *huffenctable;
	struct huffdectable_s *huffdectable;
};

