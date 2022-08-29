// made by fokkonaut

#ifndef GAME_SERVER_RAINBOW_NAME_H
#define GAME_SERVER_RAINBOW_NAME_H

#include <engine/shared/protocol.h>
#include <generated/protocol.h>

class CGameContext;
class IServer;

class CRainbowName
{
	CGameContext *m_pGameServer;
	CGameContext *GameServer() const;
	IServer *Server() const;

	int m_Color;

	struct SInfo
	{
		bool m_UpdateTeams;
		bool m_ResetChatColor;
		int m_aTeam[MAX_CLIENTS];
	} m_aInfo[MAX_CLIENTS];

	void Update(int ClientID);

public:
	void Init(CGameContext *pGameServer);
	void Tick();

	void OnChatMessage(int ClientID);

	bool IsAffected(int ClientID);
	int GetColor(int ClientID, int OtherID) { return m_aInfo[ClientID].m_aTeam[OtherID]; }
};
#endif //GAME_SERVER_RAINBOW_NAME_H
