// made by fokkonaut

#include "minigame.h"
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include <engine/shared/config.h>

IServer *CMinigame::Server() const { return GameServer()->Server(); }

CMinigame::CMinigame(CGameContext *pGameServer, int Type)
{
	m_pGameServer = pGameServer;
	m_Type = Type;
}

int CMinigame::SpawnIndex(int ClientID) const
{
	if (IsType(MINIGAME_BLOCK))
	{
		return TILE_MINIGAME_BLOCK;
	}
	// safe fallback
	return ENTITY_SPAWN;
}
