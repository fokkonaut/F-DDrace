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
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if (!pPlayer)
		return false;

	int SpectatorID = pPlayer->GetSpectatorID();
	bool IsSpecPlayer = (pPlayer->GetTeam() == TEAM_SPECTATORS || pPlayer->IsPaused()) && SpectatorID != -1 && GameServer()->m_apPlayers[SpectatorID];
	return m_aInfo[ClientID].m_UpdateTeams && IsSpecPlayer && GameServer()->m_apPlayers[SpectatorID]->m_RainbowName;
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

		// process rainbow name
		Update(i);

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

	int OwnMapID = ClientID;
	if (!Server()->Translate(OwnMapID, ClientID))
		OwnMapID = -1;

	SInfo *pInfo = &m_aInfo[ClientID];
	bool NoScoreboard = !(pPlayer->m_PlayerFlags&PLAYERFLAG_SCOREBOARD);
	int DummyID = Server()->GetDummy(ClientID);

	int SpectatorID = pPlayer->GetSpectatorID();
	bool IsSpectating = (pPlayer->GetTeam() == TEAM_SPECTATORS || pPlayer->IsPaused());

	// reset team color, do it here to not override anything in the loop
	for (int i = 0; i < VANILLA_MAX_CLIENTS; i++)
		pInfo->m_aTeam[i] = -1;

	for (int i = 0; i < VANILLA_MAX_CLIENTS; i++)
	{
		int ID = i;
		if (!Server()->ReverseTranslate(ID, ClientID))
			continue;

		CPlayer *pOther = GameServer()->m_apPlayers[ID];
		if (!pOther)
			continue;

		CPlayer *pAffected = GameServer()->m_apPlayers[ClientID];
		int AffectedID = ClientID;
		int AffectedMapID = OwnMapID;

		// when speccing someone cylce the list as him. only the player we are watching gets highlighted as TEAM_SUPER
		if (IsSpectating && SpectatorID != -1 && GameServer()->m_apPlayers[SpectatorID] && !GameServer()->m_apPlayers[SpectatorID]->m_RainbowName)
		{
			pAffected = GameServer()->m_apPlayers[SpectatorID];
			AffectedID = SpectatorID;
			AffectedMapID = SpectatorID;
			if (!Server()->Translate(AffectedMapID, ClientID))
				AffectedMapID = -1;
		}

		if (!pAffected->m_RainbowName && !pOther->m_RainbowName)
			continue;

		bool InRange = ID == ClientID || (pOther->GetCharacter() && pOther->GetCharacter()->CanSnapCharacter(ClientID) && !pOther->GetCharacter()->NetworkClipped(ClientID));
		if (InRange || ID == DummyID)
		{
			CTeamsCore *pCore = &((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams.m_Core;
			bool SetSuper = NoScoreboard && InRange && pCore->Team(AffectedID) == pCore->Team(ID);
			bool NoSpecOrHim = !IsSpectating || ID == SpectatorID;
			bool SpecSelf = IsSpectating && AffectedID == SpectatorID;

			if (pOther->m_RainbowName)
			{
				if (NoScoreboard)
				{
					pInfo->m_aTeam[i] = m_Color;
					pInfo->m_UpdateTeams = true;
				}

				if (((!pAffected->m_RainbowName && NoSpecOrHim) || SpecSelf) && SetSuper && AffectedMapID != -1)
					pInfo->m_aTeam[AffectedMapID] = TEAM_SUPER;
			}
			else if (pAffected->m_RainbowName && SetSuper && NoSpecOrHim)
			{
				pInfo->m_aTeam[i] = m_aInfo[ID].m_ResetChatColor ? pCore->Team(ID) : TEAM_SUPER;
				pInfo->m_UpdateTeams = true;
			}
		}
	}

	// if a player close to a rainbow name player sent a chat message, we send himself to t0 for one run, cuz that resets the chat color from TEAM_SUPER to grey
	if (pInfo->m_ResetChatColor)
	{
		if (OwnMapID != -1)
			pInfo->m_aTeam[OwnMapID] = 0;
		pInfo->m_ResetChatColor = false;
		pInfo->m_UpdateTeams = true;
	}

	// always also update the dummy. if the dummy gets out of view while we are standing next to a rainbow name person, and then swithc to dummy, the teams wont update and we have garbage
	if (pInfo->m_UpdateTeams && DummyID != -1)
		m_aInfo[DummyID].m_UpdateTeams = true;
}
