// made by fokkonaut

#ifndef GAME_HOUSE_H
#define GAME_HOUSE_H

#include <engine/shared/protocol.h>

enum HouseTypes
{
	HOUSE_SHOP,
	HOUSE_PLOT_SHOP,
	HOUSE_BANK,
	NUM_HOUSES
};

enum HouseStates
{
	STATE_NONE = 0,
	STATE_OPENED_WINDOW,
	STATE_CONFIRM,
};

enum HousePages
{
	PAGE_NONE = -1,
	PAGE_MAIN,
};

class CGameContext;
class IServer;

class CHouse
{
protected:
	class CGameContext *m_pGameServer;
	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server();

	int m_Type;
	const char *m_pHeadline;

	struct
	{
		bool m_Inside;
		int m_Page;
		int m_State;
		int64 m_NextMsg;
		int64 m_LastMotd;
	} m_aClients[MAX_CLIENTS];

	void SendWindow(int ClientID, const char *pMsg, const char *pFooterMsg = "");
	virtual int NumPages() { return 0; }

public:

	CHouse(CGameContext *pGameServer, int Type);
	virtual ~CHouse() {};

	void Tick(int ClientID);
	void Reset(int ClientID);

	bool IsInside(int ClientID) { return m_aClients[ClientID].m_Inside; }

	void ResetLastMotd(int ClientID) { m_aClients[ClientID].m_LastMotd = 0; }
	void OnEnter(int ClientID);
	void OnLeave(int ClientID);
	bool CanChangePage(int ClientID);
	void DoPageChange(int ClientID, int Dir);
	virtual void OnPageChange(int ClientID) {}
	virtual void OnKeyPress(int ClientID, int Dir) {}
	virtual const char *GetWelcomeMessage(int ClientID) { return ""; }

	int GetType() { return m_Type; }
	bool IsType(int Type) { return m_Type == Type; }
};

#endif
