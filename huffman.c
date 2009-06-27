/*
Copyright (C) 2006-2007 Mark Olsen

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

#include <string.h>

#include "huffmantable.h"
#include "huffmantable_q3.h"
#include "bitswap.h"
#include "common.h"
#include "huffman.h"

static void huffencbyte(struct huffenctable_s *huffenctable, unsigned char inbyte, unsigned char *outbuf, unsigned int *len)
{
	unsigned char *o;
	unsigned char bitsleft;
	unsigned int l;
	unsigned char size; 
	unsigned short code;
	unsigned char bitstocopy;

	l = *len;
	o = outbuf+l/8;
	bitsleft = 8-(l%8);

	if (bitsleft == 8)
		*o = 0;

	size = huffenctable[inbyte].len;
	code = huffenctable[inbyte].code;
	while(size)
	{
		bitstocopy = size<bitsleft?size:bitsleft;
		*o|= bitswaptable[code>>(16-bitsleft)];
		code<<= (bitstocopy);
		size-= bitstocopy;
		o++;
		*o = 0;
		l+= bitstocopy;
		bitsleft = 8;
	}

	*len = l;
}

static unsigned char huffdecbyte(struct huffdectable_s *huffdectable, unsigned char *inbuf, unsigned int *len)
{
	unsigned char *o;
	unsigned short index;
	unsigned int l;
	
	l = *len;

	o = inbuf+l/8;
	index = bitswaptable[*o++]<<((l%8)+8);
	index|= bitswaptable[*o++]<<(l%8);
	index|= bitswaptable[*o]>>(8-(l%8));

	index>>= 16-11;

	*len+= huffdectable[index].len;

	return huffdectable[index].value;
}

struct HuffContext *Huff_Init(unsigned int tablecrc)
{
	/* The first value is from FTE with broken CRC generation */
	if (tablecrc == 0x5ed5c4e4 || tablecrc == 0x286f2e8d)
		return (struct HuffContext *)&hufftables_q3;
	else
		return 0;
}

void Huff_CompressPacket(struct HuffContext *huffcontext, sizebuf_t *msg, int offset)
{
	struct hufftables *ht = (struct hufftables *)huffcontext;
	unsigned char *decmsg;
	unsigned char buffer[MAX_MSGLEN+1];
	unsigned int inlen, outlen;
	int i;

	inlen = msg->cursize-offset;
	decmsg = msg->data+offset;
	if (inlen >= MAX_MSGLEN)
		return;

	outlen = 0;
	for(i=0;i<inlen&&outlen/8<inlen;i++)
		huffencbyte(ht->huffenctable, decmsg[i], buffer, &outlen);

	if (outlen/8 >= inlen)
	{
		memmove(decmsg+1, decmsg, inlen);
		decmsg[0] = 0x80;
		msg->cursize+= 1;
		return;
	}

	/* Wasting 1 byte, but we must be compatible... */
	decmsg[0] = 8-(outlen%8);
	outlen+= 8;
	outlen/= 8;

	memcpy(decmsg+1, buffer, outlen);
	msg->cursize = offset+outlen+1;
}

void Huff_DecompressPacket(struct HuffContext *huffcontext, sizebuf_t *msg, int offset)
{
	struct hufftables *ht = (struct hufftables *)huffcontext;
	unsigned char *encmsg;
	unsigned char buffer[MAX_MSGLEN+1];
	unsigned int inlen, outlen;
	int i;

	inlen = msg->cursize-offset;
	encmsg = msg->data+offset;
	if (inlen >= MAX_MSGLEN || inlen <= 0)
		return;

	if (encmsg[0] == 0x80)
	{
		memmove(encmsg, encmsg+1, inlen-1);
		msg->cursize--;
		return;
	}

	encmsg++;
	inlen--;
	inlen*= 8;
	inlen-= encmsg[-1];

	outlen = 0;
	i = 0;
	while(outlen < inlen && i < MAX_MSGLEN)
	{
		buffer[i++] = huffdecbyte(ht->huffdectable, encmsg, &outlen);
	}

	memcpy(msg->data+offset, buffer, i);
	msg->cursize = offset+i;
}

