/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <game/server/gamecontext.h>
#include <engine/shared/config.h>
#include <game/server/teams.h>
#include "flag.h"
#include <game/server/gamemodes/DDRace.h>
#include "character.h"

CFlag::CFlag(CGameWorld *pGameWorld, int Team, vec2 Pos)
: CAdvancedEntity(pGameWorld, CGameWorld::ENTTYPE_FLAG, Pos, ms_PhysSize)
{
	m_Pos = Pos;
	m_StandPos = Pos;
	m_Team = Team;
	Reset(true);

	GameWorld()->InsertEntity(this);
}

void CFlag::Reset(bool Init)
{
	if (!Init)
	{
		if (Config()->m_SvFlagSounds)
			GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "flag_return");
		GameServer()->CreateDeath(m_Pos, m_pCarrier ? m_pCarrier->GetPlayer()->GetCID() : m_pLastCarrier ? m_pLastCarrier->GetPlayer()->GetCID() : -1);
	}
	m_pCarrier = NULL;
	m_pLastCarrier = NULL;
	m_AtStand = true;
	m_Pos = m_StandPos;
	m_Vel = vec2(0,0);
	m_DropTick = 0;
	m_GrabTick = 0;
	m_TeleCheckpoint = 0;
	m_SoundTick = 0;
	m_CanPlaySound = true;
}

void CFlag::PlaySound(int Sound)
{
	if (!Config()->m_SvFlagSounds)
		return;

	if (!m_SoundTick)
		m_CanPlaySound = true;

	if (m_SoundTick < 10 && m_CanPlaySound)
	{
		m_SoundTick++;

		if (Config()->m_SvFlagSounds == 1)
		{
			GameServer()->CreateSoundGlobal(Sound);
		}
		else if (Config()->m_SvFlagSounds == 2)
		{
			Mask128 TeamMask = Mask128();
			CCharacter *pChr = m_pCarrier ? m_pCarrier : m_pLastCarrier ? m_pLastCarrier : 0;
			if (pChr)
				TeamMask = pChr->Teams()->TeamMask(pChr->Team(), -1, pChr->GetPlayer()->GetCID());
			GameServer()->CreateSound(m_Pos, Sound, TeamMask);
		}
	}
	else
		m_CanPlaySound = false;
}

void CFlag::UpdateSpectators(int SpectatorID)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if (pPlayer && ((m_Team == TEAM_RED && pPlayer->GetSpecMode() == SPEC_FLAGRED) || (m_Team == TEAM_BLUE && pPlayer->GetSpecMode() == SPEC_FLAGBLUE)))
			pPlayer->ForceSetSpectatorID(SpectatorID);
	}
}

void CFlag::TickPaused()
{
	if(m_DropTick)
		m_DropTick++;
	if(m_GrabTick)
		m_GrabTick++;
}

void CFlag::TickDefered()
{
	if (m_pCarrier && m_pCarrier->IsAlive())
		m_Pos = m_pCarrier->GetPos();
}

void CFlag::Drop(int Dir)
{
	PlaySound(SOUND_CTF_DROP);
	m_DropTick = Server()->Tick();
	if (m_pCarrier)
		GameServer()->SendBroadcast("", m_pCarrier->GetPlayer()->GetCID(), false);
	m_pLastCarrier = m_pCarrier;
	m_pCarrier = NULL;
	m_Vel = vec2(5 * Dir, Dir == 0 ? 0 : -5);
	UpdateSpectators(-1);
}

void CFlag::Grab(CCharacter *pChr)
{
	PlaySound(m_Team == TEAM_RED ? SOUND_CTF_GRAB_EN : SOUND_CTF_GRAB_PL);
	if (m_AtStand)
		m_GrabTick = Server()->Tick();
	m_AtStand = false;
	m_pCarrier = pChr;
	m_pCarrier->m_FirstFreezeTick = 0;
	GameServer()->UnsetTelekinesis(this);
	UpdateSpectators(m_pCarrier->GetPlayer()->GetCID());
}

void CFlag::Tick()
{
	CAdvancedEntity::Tick();
	// for the CAdvancedEntity part
	m_pOwner = m_pLastCarrier;

	// plots
	int PlotID = GameServer()->GetTilePlotID(m_Pos);
	if (PlotID >= PLOT_START)
	{
		GameServer()->CreateDeath(m_Pos, m_pCarrier ? m_pCarrier->GetPlayer()->GetCID() : m_pLastCarrier ? m_pLastCarrier->GetPlayer()->GetCID() : -1);
		if (m_pCarrier)
			Drop();
		m_Vel = vec2(0, 0);
		m_Pos = m_PrevPos = GameServer()->m_aPlots[PlotID].m_ToTele;
	}

	if (m_pCarrier && m_pCarrier->IsAlive())
	{
		if (m_pCarrier->m_IsFrozen && m_pCarrier->m_FirstFreezeTick != 0)
			if (Server()->Tick() > m_pCarrier->m_FirstFreezeTick + Server()->TickSpeed() * 8)
				Drop(m_pCarrier->GetAimDir());
	}
	else
	{
		CCharacter *apCloseCCharacters[MAX_CLIENTS];
		int Num = GameWorld()->FindEntities(m_Pos, GetProximityRadius(), (CEntity**)apCloseCCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for (int i = 0; i < Num; i++)
		{
			if (!apCloseCCharacters[i] || apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_SPECTATORS || GameServer()->Collision()->IntersectLine(m_Pos, apCloseCCharacters[i]->GetPos(), NULL, NULL))
				continue;

			if (m_pCarrier == apCloseCCharacters[i] || (m_pLastCarrier == apCloseCCharacters[i] && (m_DropTick + Server()->TickSpeed() * 2) > Server()->Tick()))
				continue;

			// take the flag
			if (apCloseCCharacters[i]->HasFlag() == -1)
			{
				Grab(apCloseCCharacters[i]);
				break;
			}
		}
	}

	if (!m_pCarrier && !m_AtStand)
	{
		if (m_DropTick && Server()->Tick() > m_DropTick + Server()->TickSpeed() * 90)
		{
			Reset();
			return;
		}
		else
			HandleDropped();
	}

	if (m_SoundTick && Server()->Tick() % Server()->TickSpeed() == 0)
		m_SoundTick--;

	m_PrevPos = m_Pos;
}

void CFlag::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	if (GameServer()->GetPlayerChar(SnappingClient) && m_pCarrier)
	{
		if (m_pCarrier->IsPaused())
			return;

		if (!CmaskIsSet(m_pCarrier->Teams()->TeamMask(m_pCarrier->Team(), -1, m_pCarrier->GetPlayer()->GetCID()), SnappingClient))
			return;
	}

	CNetObj_Flag *pFlag = (CNetObj_Flag *)Server()->SnapNewItem(NETOBJTYPE_FLAG, m_Team, sizeof(CNetObj_Flag));
	if (!pFlag)
		return;

	pFlag->m_X = round_to_int(m_Pos.x);
	pFlag->m_Y = round_to_int(m_Pos.y);
	pFlag->m_Team = m_Team;
}