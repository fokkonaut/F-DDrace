#include "rainbowname.h"
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include <engine/shared/config.h>
#include <game/server/gamemodes/DDRace.h>

CGameContext *CRainbowName::GameServer() const { return m_pGameServer; }
IServer *CRainbowName::Server() const { return GameServer()->Server(); }

void CRainbowName::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_Color = 0;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aInfo[i].m_UpdateTeams = false;
		m_aInfo[i].m_ResetChatColor = false;
	}
}

void CRainbowName::OnChatMessage(int ClientID)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if (pPlayer && !pPlayer->m_RainbowName)
		m_aInfo[ClientID].m_ResetChatColor = true;
}

bool CRainbowName::IsAffected(int ClientID)
{
	return m_aInfo[ClientID].m_UpdateTeams;
}

void CRainbowName::Tick()
{
	if (Server()->Tick() % 2 != 0)
		return;

	m_Color = m_Color % (VANILLA_MAX_CLIENTS-1) + 1;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!GameServer()->m_apPlayers[i])
			continue;

		// reset everything, keep track whether we had an update last run, so after that we update one more time
		bool UpdatedLastRun = m_aInfo[i].m_UpdateTeams;
		m_aInfo[i].m_UpdateTeams = false;

		// reset teams
		for (int j = 0; j < MAX_CLIENTS; j++)
			m_aInfo[i].m_aTeam[j] = -1;

		// process rainbow name
		Update(i);

		// if a player close to a rainbow name player sent a chat message, we send himself to t0 for one run, cuz that resets the chat color from TEAM_SUPER to grey
		if (m_aInfo[i].m_ResetChatColor)
		{
			m_aInfo[i].m_aTeam[i] = 0;
			m_aInfo[i].m_ResetChatColor = false;
		}

		// send and enjoy
		if (m_aInfo[i].m_UpdateTeams || UpdatedLastRun)
			((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams.SendTeamsState(i);
	}
}

void CRainbowName::Update(int ClientID)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if (!pPlayer)
		return;

	SInfo *pInfo = &m_aInfo[ClientID];
	bool NoScoreboard = !(pPlayer->m_PlayerFlags&PLAYERFLAG_SCOREBOARD);
	int DummyID = Server()->GetDummy(ClientID);

	for (int i = 0; i < VANILLA_MAX_CLIENTS; i++)
	{
		int ID = i;
		if (!Server()->ReverseTranslate(ID, ClientID))
			continue;

		CPlayer *pOther = GameServer()->m_apPlayers[ID];
		if (!pOther || (!pPlayer->m_RainbowName && !pOther->m_RainbowName))
			continue;

		bool InRange = ID == ClientID || !pOther->GetCharacter() || !pOther->GetCharacter()->NetworkClipped(ClientID);
		if (InRange || ID == DummyID)
		{
			if (pOther->m_RainbowName)
			{
				if (NoScoreboard)
				{
					pInfo->m_aTeam[ID] = m_Color;
					pInfo->m_UpdateTeams = true;
				}

				if (!pPlayer->m_RainbowName && NoScoreboard && InRange)
					pInfo->m_aTeam[ClientID] = TEAM_SUPER;
			}
			else if (pPlayer->m_RainbowName && NoScoreboard && InRange)
			{
				pInfo->m_aTeam[ID] = TEAM_SUPER;
				pInfo->m_UpdateTeams = true;
			}
		}
	}
}
