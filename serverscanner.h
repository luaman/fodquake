#include "net.h"

enum QWServerStatus
{
	QWSS_WAITING,
	QWSS_REQUESTSENT,
	QWSS_DONE,
	QWSS_FAILED
};

struct QWPlayer
{
	const char *name; /* Can be 0 */
	const char *team; /* Ditto */
	unsigned int frags;
	unsigned int time;
	unsigned int ping;
	unsigned int topcolor;
	unsigned int bottomcolor;
};

struct QWSpectator
{
	const char *name; /* Can be 0 */
	const char *team; /* Ditto */
	unsigned int time;
	unsigned int ping;
};

struct QWServer
{
	struct netaddr addr;
	enum QWServerStatus status;
	unsigned int pingtime; /* In microseconds */
	const char *hostname; /* Note, this is the _QW_ text 'hostname' */
	unsigned int maxclients;
	unsigned int maxspectators;
	const struct QWPlayer *players;
	unsigned int numplayers;
	const struct QWSpectator *spectators;
	unsigned int numspectators;
};

struct ServerScanner *ServerScanner_Create(const char *masters);
void ServerScanner_Delete(struct ServerScanner *serverquery);

void ServerScanner_DoStuff(struct ServerScanner *serverquery);
int ServerScanner_DataUpdated(struct ServerScanner *serverquery);
struct QWServer **ServerScanner_GetServers(struct ServerScanner *serverquery, unsigned int *numservers);
void ServerScanner_FreeServers(struct ServerScanner *serverscanner, struct QWServer **servers);
int ServerScanner_ScanningComplete(struct ServerScanner *serverquery);


