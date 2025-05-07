// original version has been created by noby

#ifndef GAME_SERVER_WHOIS_H
#define GAME_SERVER_WHOIS_H

#include <generated/protocol.h>
#include <engine/server.h>

class CGameContext;

class CWhoIs
{
	CGameContext *m_pGameServer;
	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;

	#define NAMEC 50
	struct player
	{
		char name[16];
		struct nameaa
		{
			char addr[16];
			int count;
			int first3;
		} names[NAMEC];
		int numn, count, first3;
	};

	int num_ips, num_nms;
	struct player *ipplayers, *nmplayers;

	struct player *FindEntry(struct player *src, int num, const char *name);
	int AddName(struct player *src, const char *name);
	int AddGeneric(struct player *names, struct player *addrs, int *ppl, int *ppl1, const char *name_buf, const char *addr_buf);
	int AddEntry(const char *name, const char *addr);

public:
	void Init(CGameContext *pGameServer);
	void Run(const char *pName, int Mode, int Cutoff);
	void AddEntry(int ClientID);
};
#endif //GAME_SERVER_WHOIS_H
