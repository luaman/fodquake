
struct SysNetData;
struct SysSocket;

struct SysNetData *Sys_Net_Init(void);
void Sys_Net_Shutdown(struct SysNetData *netdata);

qboolean Sys_Net_ResolveName(struct SysNetData *netdata, const char *name, struct netaddr *address);
qboolean Sys_Net_ResolveAddress(struct SysNetData *netdata, const struct netaddr *address, char *output, unsigned int outputsize);

struct SysSocket *Sys_Net_CreateSocket(struct SysNetData *netdata, enum netaddrtype addrtype);
void Sys_Net_DeleteSocket(struct SysNetData *netdata, struct SysSocket *socket);
qboolean Sys_Net_Bind(struct SysNetData *netdata, struct SysSocket *socket, unsigned short port);
int Sys_Net_Send(struct SysNetData *netdata, struct SysSocket *socket, const void *data, int datalen, const struct netaddr *address);
int Sys_Net_Receive(struct SysNetData *netdata, struct SysSocket *socket, void *data, int datalen, struct netaddr *address);

