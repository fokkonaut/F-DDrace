// made by fokkonaut

#ifndef GAME_BANK_H
#define GAME_BANK_H

#include <engine/shared/protocol.h>
#include <game/mapitems.h>

class CGameContext;

class CBank
{
private:
	class CGameContext *m_pGameServer;
	class IServer *m_pServer;

	bool m_InBank[MAX_CLIENTS];

public:
	CBank(CGameContext *pGameServer);

	bool IsInBank(int ClientID) { return m_InBank[ClientID]; }
};

#endif
