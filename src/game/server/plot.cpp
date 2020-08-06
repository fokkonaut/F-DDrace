#include "plot.h"
#include <game/server/gamecontext.h>

CPlot::CPlot(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_ID = 0;
	m_aOwner[0] = '\0';
}

void CPlot::SetStatus(bool Close)
{
	if (m_ID <= 0)
		return;

	int Switch = m_pGameServer->Collision()->GetSwitchIDByPlotID(m_ID);
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		m_pGameServer->Collision()->m_pSwitchers[Switch].m_Status[i] = Close;
	}
}
