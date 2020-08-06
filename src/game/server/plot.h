// made by fokkonaut

#ifndef GAME_SERVER_PLOT_H
#define GAME_SERVER_PLOT_H

#include <generated/protocol.h>

class CPlot
{
	class CGameContext *m_pGameServer;
	void SetStatus(bool Close);

public:
	CPlot(CGameContext *pGameServer);

	int m_ID;
	char m_aOwner[24];

	void Open() { SetStatus(false); }
	void Close() { SetStatus(true); }
};
#endif //GAME_SERVER_PLOT_H
