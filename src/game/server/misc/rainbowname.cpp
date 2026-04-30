// made by fokkonaut

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
		mem_zero(&m_aInfo[i].m_aTeam, sizeof(m_aInfo[i].m_aTeam));
	}
}

void CRainbowName::OnChatMessage(int ClientID)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if (pPlayer && pPlayer->m_ShowName && (pPlayer->m_RainbowName || m_aInfo[ClientID].m_UpdateTeams))
		m_aInfo[ClientID].m_ResetChatColor = true;
}

bool CRainbowName::IsAffected(int ClientID)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if (!pPlayer || !m_aInfo[ClientID].m_UpdateTeams)
		return false;

	int SpectatorID = pPlayer->GetSpectatorID();
	bool IsSpecPlayer = (pPlayer->GetTeam() == TEAM_SPECTATORS || pPlayer->IsPaused()) && SpectatorID != -1 && GameServer()->m_apPlayers[SpectatorID];
	return IsSpecPlayer;
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
		if (Server()->IsSevendown(i) && GameServer()->GetClientDDNetVersion(i) < VERSION_DDNET_SUPER_PREDICTION)
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

	// need to reset it after everyone has used the values of other people too
	for (int i = 0; i < MAX_CLIENTS; i++)
		m_aInfo[i].m_ResetChatColor = false;
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
	if (OwnMapID != -1)
		pInfo->m_aTeam[OwnMapID] = -1; // reset ourselves here so we dont override reset us when our id is greater than another and we got processed already

	int DummyID = Server()->GetDummy(ClientID);
	CTeamsCore *pCore = &((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams.m_Core;

	for (int i = 0; i < Server()->GetMaxClients(ClientID); i++)
	{
		if (i == OwnMapID)
			continue;

		pInfo->m_aTeam[i] = -1;

		bool IsPaused = pPlayer->GetTeam() == TEAM_SPECTATORS || pPlayer->IsPaused();
		if (pPlayer->m_PlayerFlags&PLAYERFLAG_SCOREBOARD || (IsPaused && !GameServer()->Config()->m_SvRainbowNameSpec))
			continue;

		if (GameServer()->Durak()->OnRainbowName(ClientID, i))
		{
			pInfo->m_aTeam[i] = m_Color;
			pInfo->m_UpdateTeams = true;
		}
		else
		{
			int ID = i;
			if (!Server()->ReverseTranslate(ID, ClientID))
				continue;

			CPlayer *pOther = GameServer()->m_apPlayers[ID];
			if (!pOther || !pOther->m_RainbowName)
				continue;

			bool InRange = pOther->GetCharacter() && pOther->GetCharacter()->CanSnapCharacter(ClientID) && !pOther->GetCharacter()->NetworkClipped(ClientID);
			if (InRange || m_aInfo[ID].m_ResetChatColor)
			{
				pInfo->m_aTeam[i] = m_Color;
				pInfo->m_UpdateTeams = true;
			}

			if (!InRange || pCore->Team(ClientID) != pCore->Team(ID))
				continue;
		}

		if (OwnMapID == -1)
			continue;

		int SpectatorID = pPlayer->GetSpectatorID();
		bool NoSpecOrFollow = !IsPaused || (SpectatorID != -1 && GameServer()->m_apPlayers[SpectatorID]);
		if (NoSpecOrFollow)
			pInfo->m_aTeam[OwnMapID] = VANILLA_MAX_CLIENTS; // TODO: TEAM_SUPER, but it's 128 due to increased client capability // might have to adapt this in the future when teams are expanded to 128
	}

	// if a player close to a rainbow name player sent a chat message, we send himself to t0 for one run, cuz that resets the chat color from TEAM_SUPER to grey
	if (pInfo->m_ResetChatColor && OwnMapID != -1)
	{
		pInfo->m_aTeam[OwnMapID] = pPlayer->m_RainbowName ? m_Color : pCore->Team(ClientID);
		pInfo->m_UpdateTeams = true;
	}

	// always also update the dummy. if the dummy gets out of view while we are standing next to a rainbow name person, and then swithc to dummy, the teams wont update and we have garbage
	if (pInfo->m_UpdateTeams && DummyID != -1)
		m_aInfo[DummyID].m_UpdateTeams = true;
}
