
struct SysNetData *Sys_Socket_Init()
{
	WSADATA winsockdata;
	WORD wVersionRequested;
	int r;

	wVersionRequested = MAKEWORD(1, 1);
	r = WSAStartup(wVersionRequested, &winsockdata);
	if (r)
	{
		return (struct SysNetData *)1;
	}

	return 0;
}

void Sys_Socket_Shutdown(struct SysNetData *netdata)
{
	WSACleanup();
}

