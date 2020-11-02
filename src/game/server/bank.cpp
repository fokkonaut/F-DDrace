// made by fokkonaut

#include "bank.h"
#include "gamecontext.h"


CBank::CBank(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pServer = m_pGameServer->Server();
}
