#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

#include <stdlib.h>
#include <string.h>

#include "quakedef.h"
#include "sys_net.h"

struct SysSocket
{
	int s;
	int domain;
};

struct SysNetData *Sys_Net_Init()
{
	return (struct SysNetData *)-1;
}

void Sys_Net_Shutdown(struct SysNetData *netdata)
{
}

qboolean Sys_Net_ResolveName(struct SysNetData *netdata, const char *name, struct netaddr *address)
{
	int r;
	struct addrinfo *addr;
	struct addrinfo *origaddr;
	qboolean ret;

	ret = false;

	r = getaddrinfo(name, 0, 0, &origaddr);
	if (r == 0)
	{
		addr = origaddr;
		while(addr)
		{
			if (addr->ai_family == AF_INET)
			{
				address->type = NA_IPV4;
				*(unsigned int *)address->addr.ipv4.address = *(unsigned int *)&((struct sockaddr_in *)addr->ai_addr)->sin_addr.s_addr;
				ret = true;
				break;
			}
			if (addr->ai_family == AF_INET6)
			{
				address->type = NA_IPV6;
				memcpy(address->addr.ipv6.address, &((struct sockaddr_in6 *)addr->ai_addr)->sin6_addr, sizeof(address->addr.ipv6.address));
				ret = true;
				break;
			}

			addr = addr->ai_next;
		}

		freeaddrinfo(origaddr);
	}

	return ret;
}

struct SysSocket *Sys_Net_CreateSocket(struct SysNetData *netdata, enum netaddrtype addrtype)
{
	struct SysSocket *s;
	int domain;
	int r;
	int one;

	one = 1;

	if (addrtype == NA_IPV4)
		domain = AF_INET;
	else if (addrtype == NA_IPV6)
		domain = AF_INET6;
	else
		return 0;

	s = malloc(sizeof(*s));
	if (s)
	{
		s->s = socket(domain, SOCK_DGRAM, 0);
		if (s->s != -1)
		{
			r = ioctl(s->s, FIONBIO, &one);
			if (r == 0)
			{
				s->domain = domain;

				return s;
			}
		}

		free(s);
	}

	return 0;
}

void Sys_Net_DeleteSocket(struct SysNetData *netdata, struct SysSocket *socket)
{
	close(socket->s);
	free(socket);
}

qboolean Sys_Net_Bind(struct SysNetData *netdata, struct SysSocket *socket, unsigned short port)
{
	return false;
}

int Sys_Net_Send(struct SysNetData *netdata, struct SysSocket *socket, const void *data, int datalen, const struct netaddr *address)
{
	int r;

	if (address)
	{
		unsigned int addrsize;
		union
		{
			struct sockaddr_in addr;
			struct sockaddr_in6 addr6;
		} addr;

		if (socket->domain == AF_INET)
		{
			addr.addr.sin_family = AF_INET;
			addr.addr.sin_port = htons(address->addr.ipv4.port);
			*(unsigned int *)&addr.addr.sin_addr.s_addr = *(unsigned int *)address->addr.ipv4.address;
			addrsize = sizeof(addr.addr);
		}
		else if (socket->domain == AF_INET6)
		{
			addr.addr6.sin6_family = AF_INET6;
			addr.addr6.sin6_port = htons(address->addr.ipv6.port);
			memcpy(&addr.addr6.sin6_addr, address->addr.ipv6.address, sizeof(addr.addr6.sin6_addr));
			addr.addr6.sin6_flowinfo = 0;
			addr.addr6.sin6_scope_id = 0;
			addrsize = sizeof(addr.addr6);
		}

		r = sendto(socket->s, data, datalen, 0, (struct sockaddr *)&addr, addrsize);
	}
	else
		r = send(socket->s, data, datalen, 0);

	if (r == -1)
	{
		if (errno == EWOULDBLOCK)
			return 0;
	}

	return r;
}

int Sys_Net_Receive(struct SysNetData *netdata, struct SysSocket *socket, void *data, int datalen, struct netaddr *address)
{
	int r;

	if (address)
	{
		socklen_t fromlen;
		union
		{
			struct sockaddr_in addr;
			struct sockaddr_in6 addr6;
		} addr;

		if (socket->domain == AF_INET)
			fromlen = sizeof(addr.addr);
		else if (socket->domain == AF_INET6)
			fromlen = sizeof(addr.addr6);

		r = recvfrom(socket->s, data, datalen, 0, (struct sockaddr *)&addr, &fromlen);

		if (r >= 0)
		{
			if (socket->domain == AF_INET)
			{
				if (fromlen != sizeof(addr.addr))
					return -1;

				address->type = NA_IPV4;
				address->addr.ipv4.port = htons(addr.addr.sin_port);
				*(unsigned int *)address->addr.ipv4.address = *(unsigned int *)&addr.addr.sin_addr.s_addr;
			}
			else if (socket->domain == AF_INET6)
			{
				if (fromlen != sizeof(addr.addr6))
					return -1;

				address->type = NA_IPV6;
				address->addr.ipv6.port = htons(addr.addr6.sin6_port);
				memcpy(address->addr.ipv6.address, &addr.addr6.sin6_addr, sizeof(address->addr.ipv6.address));
			}
		}
	}
	else
		r = recv(socket->s, data, datalen, 0);

	if (r == -1)
	{
		if (errno == EWOULDBLOCK)
			return 0;
	}

	return r;
}

