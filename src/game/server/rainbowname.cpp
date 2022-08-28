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

void CRainbowName::Tick()
{
	if (Server()->Tick() % 2 != 0)
		return;

	m_Color = m_Color % (VANILLA_MAX_CLIENTS-1) + 1;

	bool aLastUpdate[MAX_CLIENTS];
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		aLastUpdate[i] = m_aInfo[i].m_UpdateTeams;
		m_aInfo[i].m_UpdateTeams = false;
		for (int j = 0; j < MAX_CLIENTS; j++)
			m_aInfo[i].m_aTeam[j] = -1;
	}

	for (int i = 0; i < MAX_CLIENTS; i++)
		Update(i);

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_aInfo[i].m_ResetChatColor)
		{
			m_aInfo[i].m_aTeam[i] = 0;
			m_aInfo[i].m_ResetChatColor = false;
		}
	}

	for (int i = 0; i < MAX_CLIENTS; i++)
		if (m_aInfo[i].m_UpdateTeams || aLastUpdate[i])
			((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams.SendTeamsState(i);
}

void CRainbowName::Update(int ClientID)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if (!pPlayer)
		return;

	for (int i = 0; i < VANILLA_MAX_CLIENTS; i++)
	{
		int ID = i;
		if (!Server()->ReverseTranslate(ID, ClientID))
			continue;

		CPlayer *pOther = GameServer()->m_apPlayers[ID];
		if (!pOther || (!pPlayer->m_RainbowName && !pOther->m_RainbowName))
			continue;

		bool InRange = ID == ClientID || !pPlayer->GetCharacter() || !pPlayer->GetCharacter()->NetworkClipped(ID);
		if (InRange)
		{
			bool NoScoreboard = !(pPlayer->m_PlayerFlags&PLAYERFLAG_SCOREBOARD);
			if (pOther->m_RainbowName)
			{
				if (GameServer()->Config()->m_SvRainbowNameScoreboard || NoScoreboard)
				{
					m_aInfo[ClientID].m_aTeam[ID] = m_Color;
					m_aInfo[ClientID].m_UpdateTeams = true;
				}

				if (!pPlayer->m_RainbowName && NoScoreboard)
					m_aInfo[ClientID].m_aTeam[ClientID] = TEAM_SUPER;
			}
			else if (pPlayer->m_RainbowName && NoScoreboard)
			{
				m_aInfo[ClientID].m_aTeam[ID] = TEAM_SUPER;
				m_aInfo[ClientID].m_UpdateTeams = true;
			}
		}
	}
}
