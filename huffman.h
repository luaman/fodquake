
void *Huff_Init(unsigned int tablecrc);
void Huff_CompressPacket(void *huffcontext, sizebuf_t *msg, int offset);
void Huff_DecompressPacket(void *huffcontext, sizebuf_t *msg, int offset);

