#include <stdlib.h>
#include <string.h>

#include "sys_thread.h"
#include "sys_net.h"
#include "serverscanner.h"

#define MAXCONCURRENTSCANS 16

struct masterserver
{
	char *hostname;
	struct netaddr addr;
};

struct qwserverpriv
{
	struct qwserverpriv *next;
	struct qwserverpriv *nextinprogress;
	struct qwserverpriv *nextwaiting;
	unsigned long long packetsendtime;
	struct QWServer pub;
};

struct ServerScanner
{
	struct SysNetData *netdata;
	struct SysSocket *sockets[NA_NUMTYPES];
	struct SysThread *thread;
	struct SysMutex *mutex;
	struct masterserver *masterservers;
	struct qwserverpriv *qwservers;
	struct qwserverpriv *qwserversinprogress;
	struct qwserverpriv *qwserverswaiting;
	volatile unsigned int quit;
	unsigned int nummasterservers;
	unsigned int numvalidmasterservers;
	unsigned int numqwservers;
	unsigned int numqwserversinprogress;
	unsigned int numqwserverswaiting;
#warning Remove this
	unsigned int numqwserverstoscan;
	volatile unsigned int done;
	unsigned int updated;
};

static int ServerScanner_Thread_Init(struct ServerScanner *serverscanner)
{
	serverscanner->netdata = Sys_Net_Init();

	if (serverscanner->netdata)
	{
		return 1;
	}

	return 0;
}

static void ServerScanner_Thread_Deinit(struct ServerScanner *serverscanner)
{
	Sys_Net_Shutdown(serverscanner->netdata);
}

static void ServerScanner_Thread_LookUpMasters(struct ServerScanner *serverscanner)
{
	unsigned int i;

	for(i=0;i<serverscanner->nummasterservers;i++)
	{
		if (!NET_StringToAdr(serverscanner->netdata, serverscanner->masterservers[i].hostname, &serverscanner->masterservers[i].addr))
			serverscanner->numvalidmasterservers--;
	}
}

static void ServerScanner_Thread_OpenSockets(struct ServerScanner *serverscanner)
{
	unsigned int i;
	enum netaddrtype addrtype;

	for(i=0;i<serverscanner->nummasterservers;i++)
	{
		addrtype = serverscanner->masterservers[i].addr.type;

		if (addrtype == NA_LOOPBACK)
			continue;

		if (serverscanner->sockets[addrtype] == 0)
			serverscanner->sockets[addrtype] = Sys_Net_CreateSocket(serverscanner->netdata, addrtype);

		if (serverscanner->sockets[addrtype] == 0)
			serverscanner->numvalidmasterservers--;
	}
}

static void ServerScanner_Thread_CloseSockets(struct ServerScanner *serverscanner)
{
	unsigned int i;

	for(i=0;i<NA_NUMTYPES;i++)
	{
		if (serverscanner->sockets[i])
			Sys_Net_DeleteSocket(serverscanner->netdata, serverscanner->sockets[i]);
	}
}

static void ServerScanner_Thread_QueryMasters(struct ServerScanner *serverscanner)
{
	static const char querystring[] = "c\n";
	unsigned int i;

	for(i=0;i<serverscanner->nummasterservers;i++)
	{
		if (serverscanner->sockets[serverscanner->masterservers[i].addr.type])
		{
			Sys_Net_Send(serverscanner->netdata, serverscanner->sockets[serverscanner->masterservers[i].addr.type], querystring, 3, &serverscanner->masterservers[i].addr);
		}
	}
}

static void ServerScanner_Thread_ParseQWServerReply(struct ServerScanner *serverscanner, struct qwserverpriv *qwserver, unsigned char *data, unsigned int datalen)
{
	struct QWPlayer *qwplayers;
	struct QWSpectator *qwspectators;
	char *p3;
	unsigned int i;
	unsigned int j;
	unsigned int k;
	unsigned int extralength;
	unsigned char *p;
	unsigned char *p2;
	unsigned char *key;
	unsigned char *value;
	unsigned int numplayers;
	unsigned int numspectators;
	unsigned char *name[32];
	unsigned char *team[32];
	int spectator[32];
	int frags[32];
	int time[32];
	unsigned int ping[32];
	unsigned int topcolour[32];
	unsigned int bottomcolour[32];

	qwserver->pub.status = QWSS_FAILED;

	if (datalen < 2 || data[0] != 'n')
		return;

	data++;
	datalen--;

	p = memchr(data, '\n', datalen);
	if (p == 0)
		return;

	p2 = memchr(data, 0, datalen);
	if (p2 && p2 < p)
		return;

	*p = 0;

	p = data + 1;
	while(*p)
	{
		key = p;

		p = strchr(key, '\\');
		if (p == 0)
			break;

		*p = 0;
		value = p + 1;

		p = strchr(value, '\\');
		if (p)
		{
			*p = 0;
			p++;
		}
		else
			p = value + strlen(value);

#if 0
		printf("%s = %s\n", key, value);
#endif

		if (strcmp(key, "maxclients") == 0)
			qwserver->pub.maxclients = strtoul(value, 0, 0);
		else if (strcmp(key, "maxspectators") == 0)
			qwserver->pub.maxspectators = strtoul(value, 0, 0);
		else if (strcmp(key, "hostname") == 0)
			qwserver->pub.hostname = strdup(value);
	}

	if (!p)
		return;

	datalen -= (p - data);
	data = p;

	if (datalen == 0)
		return;

	datalen--;
	data++;

	qwserver->pub.status = QWSS_DONE;

	numplayers = 0;
	numspectators = 0;
	extralength = 0;

	i = 0;
	while(1)
	{
		if (numplayers + numspectators >= 32)
			break;

		p = memchr(data, '\n', datalen);
		if (p == 0)
			break;

		p2 = memchr(data, 0, datalen);
		if (p2 && p2 < p)
			break;

		*p = 0;

#ifdef DEBUG
		printf("%s\n", data);
#endif

		p2 = data;

		p2 = strchr(p2, ' ');
		if (!p2)
			break;

		p2++;
		frags[i] = atoi(p2);

		p2 = strchr(p2, ' ');
		if (!p2)
			break;

		p2++;
		time[i] = atoi(p2);

		p2 = strchr(p2, ' ');
		if (!p2)
			break;

		p2++;
		ping[i] = atoi(p2);

		p2 = strchr(p2, ' ');
		if (!p2)
			break;

		p2++;
		if (*p2 != '"')
			break;

		p2++;
		name[i] = p2;

		p2 = strchr(p2, '"');
		if (!p2)
			break;

		*p2 = 0;
		p2++;
		if (*p2 != ' ')
			break;

		p2++;

		if (*p2 != '"')
			break;

		p2++;

		p2 = strchr(p2, '"');
		if (!p2)
			break;

		p2++;
		topcolour[i] = atoi(p2);

		p2 = strchr(p2, ' ');
		if (!p2)
			break;

		p2++;
		bottomcolour[i] = atoi(p2);

		p2 = strchr(p2, ' ');
		if (!p2)
			break;

		p2++;

		if (*p2 == '"')
		{
			p2++;
			team[i] = p2;

			p2 = strchr(p2, '"');
			if (!p2)
				break;

			*p2 = 0;
		}
		else
			team[i] = 0;

		if (strncmp(name[i], "\\s\\", 3) == 0)
		{
			numspectators++;
			spectator[i] = 1;
			time[i] = -time[i];
			name[i] += 3;
		}
		else
		{
			numplayers++;
			spectator[i] = 0;
		}

#ifdef DEBUG
		printf("frags: %d time: %d ping: %d name: %s topcolour: %d bottomcolour: %d team: %s\n", frags[i], ping[i], time[i], name[i], topcolour[i], bottomcolour[i], team[i]);
#endif

		if (name[i])
			extralength += strlen(name[i]) + 1;
		if (team[i])
			extralength += strlen(team[i]) + 1;

		datalen -= (p - data) + 1;
		data = p + 1;

		i++;
	}

	qwplayers = malloc(sizeof(*qwplayers) * numplayers + sizeof(*qwspectators) * numspectators + extralength);
	if (qwplayers)
	{
		qwspectators = (struct QWSpectator *)(qwplayers + numplayers);
		p3 = (char *)(qwspectators + numspectators);

		j = 0;
		k = 0;
		for(i=0;i<numplayers+numspectators;i++)
		{
			if (spectator[i])
			{
				if (name[i])
				{
					strcpy(p3, name[i]);
					qwspectators[j].name = p3;
					p3 += strlen(name[i]) + 1;
				}
				else
					qwspectators[j].name = 0;

				if (team[i])
				{
					strcpy(p3, team[i]);
					qwspectators[j].team = p3;
					p3 += strlen(team[i]) + 1;
				}
				else
					qwspectators[j].team = 0;

				qwspectators[j].time = time[i];
				qwspectators[j].ping = ping[i];

				j++;
			}
			else
			{
				if (name[i])
				{
					strcpy(p3, name[i]);
					qwplayers[k].name = p3;
					p3 += strlen(name[i]) + 1;
				}
				else
					qwplayers[k].name = 0;

				if (team[i])
				{
					strcpy(p3, team[i]);
					qwplayers[k].team = p3;
					p3 += strlen(team[i]) + 1;
				}
				else
					qwplayers[k].team = 0;

				qwplayers[k].frags = frags[i];
				qwplayers[k].time = time[i];
				qwplayers[k].ping = ping[i];
				qwplayers[k].topcolor = topcolour[i];
				qwplayers[k].bottomcolor = bottomcolour[i];

				k++;
			}
		}

		qwserver->pub.players = qwplayers;
		qwserver->pub.numplayers = numplayers;
		qwserver->pub.spectators = qwspectators;
		qwserver->pub.numspectators = numspectators;
	}
}

static void ServerScanner_Thread_SendQWRequest(struct ServerScanner *serverscanner, struct qwserverpriv *qwserver)
{
	static const char querystring[] = "\xff\xff\xff\xffstatus 23\n";

	Sys_Net_Send(serverscanner->netdata, serverscanner->sockets[NA_IPV4], querystring, sizeof(querystring), &qwserver->pub.addr);
	if (qwserver->pub.status != QWSS_REQUESTSENT)
	{
		qwserver->pub.status = QWSS_REQUESTSENT;
		serverscanner->numqwserversinprogress++;
		qwserver->nextinprogress = serverscanner->qwserversinprogress;
		serverscanner->qwserversinprogress = qwserver;
	}
}

static void ServerScanner_Thread_HandlePacket(struct ServerScanner *serverscanner, unsigned char *data, unsigned int datalen, struct netaddr *addr)
{
	struct qwserverpriv *qwserver;
	struct qwserverpriv *prevqwserver;
	unsigned int i;
	struct netaddr newaddr;

	if (datalen < 4 || data[0] != 255 || data[1] != 255 || data[2] != 255 || data[3] != 255)
		return;

	data += 4;
	datalen -= 4;

	for(i=0;i<serverscanner->nummasterservers;i++)
	{
		if (NET_CompareAdr(&serverscanner->masterservers[i].addr, addr))
		{
			if (datalen < 2 || data[0] != 'd' || data[1] != '\n')
				return;

			data += 2;
			datalen -= 2;

			if ((datalen%6) != 0)
				return;

			for(i=0;i<datalen;i+=6)
			{
				if (serverscanner->numqwservers > 10000)
					return;

				newaddr.type = NA_IPV4;
				newaddr.addr.ipv4.address[0] = data[i + 0];
				newaddr.addr.ipv4.address[1] = data[i + 1];
				newaddr.addr.ipv4.address[2] = data[i + 2];
				newaddr.addr.ipv4.address[3] = data[i + 3];
				newaddr.addr.ipv4.port = (data[i + 4]<<8)|data[i + 5];

#warning Slow as fucking hell.
				qwserver = serverscanner->qwservers;
				while(qwserver)
				{
					if (NET_CompareAdr(&newaddr, &qwserver->pub.addr))
						break;

					qwserver = qwserver->next;
				}

				if (qwserver && NET_CompareAdr(&newaddr, &qwserver->pub.addr))
				{
					continue;
				}

				qwserver = malloc(sizeof(*qwserver));
				if (qwserver == 0)
					break;

				memset(qwserver, 0, sizeof(*qwserver));

				qwserver->pub.addr = newaddr;
				qwserver->pub.status = QWSS_WAITING;

				if (serverscanner->numqwserversinprogress < MAXCONCURRENTSCANS)
				{
					ServerScanner_Thread_SendQWRequest(serverscanner, qwserver);
				}
				else
				{
					qwserver->nextwaiting = serverscanner->qwserverswaiting;
					serverscanner->qwserverswaiting = qwserver;
					serverscanner->numqwserverswaiting++;
				}

				Sys_Thread_LockMutex(serverscanner->mutex);

				qwserver->next = serverscanner->qwservers;
				serverscanner->qwservers = qwserver;
				serverscanner->numqwservers++;
				serverscanner->numqwserverstoscan++;

				Sys_Thread_UnlockMutex(serverscanner->mutex);
			}

			Sys_Thread_LockMutex(serverscanner->mutex);
			serverscanner->updated = 1;
			Sys_Thread_UnlockMutex(serverscanner->mutex);

#ifdef DEBUG
			printf("%d servers to scan\n", serverscanner->numqwserverstoscan);
#endif

			return;
		}
	}

	prevqwserver = 0;
	qwserver = serverscanner->qwserversinprogress;
	while(qwserver)
	{
		if (NET_CompareAdr(&qwserver->pub.addr, addr) && qwserver->pub.status == QWSS_REQUESTSENT)
		{
			Sys_Thread_LockMutex(serverscanner->mutex);

			serverscanner->updated = 1;
			serverscanner->numqwserverstoscan--;

			ServerScanner_Thread_ParseQWServerReply(serverscanner, qwserver, data, datalen);

			Sys_Thread_UnlockMutex(serverscanner->mutex);

			if (prevqwserver)
				prevqwserver->nextinprogress = qwserver->nextinprogress;
			else
				serverscanner->qwserversinprogress = qwserver->nextinprogress;

			qwserver->nextinprogress = 0;
			serverscanner->numqwserversinprogress--;

			if (serverscanner->numqwserversinprogress < MAXCONCURRENTSCANS && serverscanner->qwserverswaiting)
			{
				qwserver = serverscanner->qwserverswaiting;
				ServerScanner_Thread_SendQWRequest(serverscanner, qwserver);
				serverscanner->qwserverswaiting = qwserver->nextwaiting;
				qwserver->nextwaiting = 0;
				serverscanner->numqwserverswaiting--;
			}

			return;
		}

		prevqwserver = qwserver;
		qwserver = qwserver->nextinprogress;
	}
}

static void ServerScanner_Thread(void *arg)
{
	struct ServerScanner *serverscanner;
	unsigned int i;
	unsigned long long curtime;
	unsigned long long starttime;
	int r;
	unsigned char buf[8192];
	struct netaddr addr;

	serverscanner = arg;

	if (ServerScanner_Thread_Init(serverscanner))
	{
		ServerScanner_Thread_LookUpMasters(serverscanner);

		ServerScanner_Thread_OpenSockets(serverscanner);

		if (serverscanner->numvalidmasterservers)
		{
			starttime = Sys_IntTime();

			ServerScanner_Thread_QueryMasters(serverscanner);

			while(!serverscanner->quit)
			{
				if (!serverscanner->done)
				{
					if (serverscanner->numqwserverstoscan == 0 && Sys_IntTime() > starttime + 2000000)
						serverscanner->done = 1;
				}

#warning Fixme.
				Sys_Net_Wait(serverscanner->netdata, serverscanner->sockets[NA_IPV4], 50000);

				for(i=1;i<NA_NUMTYPES;i++)
				{
					if (!serverscanner->sockets[i])
						continue;

					while((r = Sys_Net_Receive(serverscanner->netdata, serverscanner->sockets[i], buf, sizeof(buf), &addr)) > 0)
					{
						ServerScanner_Thread_HandlePacket(serverscanner, buf, r, &addr);
					}
				}
			}
		}

		ServerScanner_Thread_CloseSockets(serverscanner);

		ServerScanner_Thread_Deinit(serverscanner);
	}

	serverscanner->done = 1;
}

struct ServerScanner *ServerScanner_Create(const char *masters)
{
	struct ServerScanner *serverscanner;
	unsigned int nummasterservers;
	unsigned int i;
	const char *p;
	const char *p2;

	nummasterservers = 0;
	p = masters;
	while(*p)
	{
		p2 = strchr(p, ' ');
		if (p2 == 0)
			p2 = p + strlen(p);

		if (p2 != p)
			nummasterservers++;

		p = p2;
		if (*p)
			p++;
	}

	if (nummasterservers == 0)
		return 0;

	if (nummasterservers > 1000)
		return 0;

	serverscanner = malloc(sizeof(*serverscanner));
	if (serverscanner)
	{
		memset(serverscanner, 0, sizeof(*serverscanner));

		serverscanner->masterservers = malloc(sizeof(*serverscanner->masterservers)*nummasterservers);
		if (serverscanner->masterservers)
		{
			memset(serverscanner->masterservers, 0, sizeof(*serverscanner->masterservers)*nummasterservers);

			p = masters;
			for(i=0;i<nummasterservers;)
			{
				p2 = strchr(p, ' ');
				if (p2 == 0)
					p2 = p + strlen(p);

				if (p2 != p)
				{
					serverscanner->masterservers[i].hostname = malloc((p2 - p) + 1);
					if (serverscanner->masterservers[i].hostname == 0)
						break;

					memcpy(serverscanner->masterservers[i].hostname, p, p2 - p);
					serverscanner->masterservers[i].hostname[p2 - p] = 0;

					i++;
				}

				p = p2;
				if (*p)
					p++;
			}

			if (i == nummasterservers)
			{
				serverscanner->mutex = Sys_Thread_CreateMutex();
				if (serverscanner->mutex)
				{
					serverscanner->nummasterservers = nummasterservers;
					serverscanner->numvalidmasterservers = nummasterservers;

					serverscanner->thread = Sys_Thread_CreateThread(ServerScanner_Thread, serverscanner);
					if (serverscanner->thread)
						return serverscanner;

					Sys_Thread_DeleteMutex(serverscanner->mutex);
				}
			}

			for(i=0;i<nummasterservers;i++)
			{
				free(serverscanner->masterservers[i].hostname);
			}
		}

		free(serverscanner);
	}

	return 0;
}

void ServerScanner_Delete(struct ServerScanner *serverscanner)
{
	struct qwserverpriv *qwserver, *nextqwserver;
	unsigned int i;

	serverscanner->quit = 1;

	Sys_Thread_DeleteThread(serverscanner->thread);

	Sys_Thread_DeleteMutex(serverscanner->mutex);

	qwserver = serverscanner->qwservers;
	while(qwserver)
	{
		nextqwserver = qwserver->next;

		free((void *)qwserver->pub.hostname);
		free((void *)qwserver->pub.players);
		free(qwserver);

		qwserver = nextqwserver;
	}

	for(i=0;i<serverscanner->nummasterservers;i++)
	{
		free(serverscanner->masterservers[i].hostname);
	}

	free(serverscanner->masterservers);

	free(serverscanner);
}

void ServerScanner_DoStuff(struct ServerScanner *serverscanner)
{
	if (!serverscanner->thread)
	{
	}
}

int ServerScanner_DataUpdated(struct ServerScanner *serverscanner)
{
	int ret;

	Sys_Thread_LockMutex(serverscanner->mutex);

	ret = serverscanner->updated;
	serverscanner->updated = 0;

	Sys_Thread_UnlockMutex(serverscanner->mutex);

	return ret;
}

struct QWServer **ServerScanner_GetServers(struct ServerScanner *serverscanner, unsigned int *numservers)
{
	struct qwserverpriv *qwserver;
	struct QWServer **servers;
	unsigned int i;

	servers = 0;

	Sys_Thread_LockMutex(serverscanner->mutex);

	if (serverscanner->numqwservers <= 10000)
	{
		servers = malloc(sizeof(*servers)*serverscanner->numqwservers);
		if (servers)
		{
			qwserver = serverscanner->qwservers;
			for(i=0;i<serverscanner->numqwservers;i++)
			{
				servers[i] = &qwserver->pub;

				qwserver = qwserver->next;
			}

			*numservers = serverscanner->numqwservers;
		}
	}

	Sys_Thread_UnlockMutex(serverscanner->mutex);

	return servers;
}

void ServerScanner_FreeServers(struct ServerScanner *serverscanner, struct QWServer **servers)
{
	free(servers);
}

int ServerScanner_ScanningComplete(struct ServerScanner *serverscanner)
{
	return serverscanner->done;
}

#ifdef DEBUG
int main()
{
	unsigned int i;
	struct ServerScanner *serverscanner;
	struct QWServer **servers;
	unsigned int numservers;
	unsigned int numplayers;
	unsigned int numspectators;
	unsigned int numup;
	unsigned int numdown;

#if 1
	serverscanner = ServerScanner_Create("asgaard.morphos-team.net:27000 master.quakeservers.net:27000");
#else
	serverscanner = ServerScanner_Create("127.0.0.1:27000");
#endif
	if (serverscanner)
	{
		i = 0;

		while(!ServerScanner_ScanningComplete(serverscanner))
		{
			i++;
			if (i == 7)
			{
				printf("Fail!\n");
				break;
			}

			printf("Waiting...\n");

			sleep(1);
		}

		printf("Done!\n");

		servers = ServerScanner_GetServers(serverscanner, &numservers);
		if (servers)
		{
			numplayers = 0;
			numspectators = 0;
			numup = 0;
			numdown = 0;
			for(i=0;i<numservers;i++)
			{
				if (servers[i]->status == QWSS_DONE)
				{
					numup++;
					numplayers += servers[i]->numplayers;
					numspectators += servers[i]->numspectators;
				}
				else
					numdown++;
			}

			printf("%d servers, %d up, %d down, %d players, %d spectators\n", numservers, numup, numdown, numplayers, numspectators);

			ServerScanner_FreeServers(serverscanner, servers);
		}

		ServerScanner_Delete(serverscanner);
	}

	return 0;
}
#endif

