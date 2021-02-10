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
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (GameServer()->GetPlayerChar(i))
		{
			int Flag = m_Team == TEAM_RED ? HOOK_FLAG_RED : HOOK_FLAG_BLUE;
			if (GameServer()->GetPlayerChar(i)->Core()->m_HookedPlayer == Flag)
			{
				GameServer()->GetPlayerChar(i)->Core()->m_HookedPlayer = -1;
				GameServer()->GetPlayerChar(i)->Core()->m_HookState = HOOK_RETRACTED;
			}
		}
	}

	if (!Init)
	{
		if (Config()->m_SvFlagSounds)
			GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "flag_return");
		GameServer()->CreateDeath(m_Pos, GetCarrier() ? m_Carrier : GetLastCarrier() ? m_LastCarrier : -1);
	}
	m_Carrier = -1;
	m_LastCarrier = -1;
	m_AtStand = true;
	m_Pos = m_StandPos;
	m_Vel = vec2(0,0);
	m_DropTick = 0;
	m_GrabTick = 0;
	m_TeleCheckpoint = 0;
	m_SoundTick = 0;
	m_CanPlaySound = true;
}

void CFlag::SetAtStand(bool AtStand)
{
	if (m_AtStand && !AtStand)
		m_DropTick = Server()->Tick();
	m_AtStand = AtStand;
}

CCharacter *CFlag::GetCarrier()
{
	return GameServer()->GetPlayerChar(m_Carrier);
}

CCharacter *CFlag::GetLastCarrier()
{
	return GameServer()->GetPlayerChar(m_LastCarrier);
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
			CCharacter *pChr = GetCarrier() ? GetCarrier() : GetLastCarrier() ? GetLastCarrier() : 0;
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
	if (GetCarrier())
		m_Pos = GetCarrier()->GetPos();
}

void CFlag::Drop(int Dir)
{
	PlaySound(SOUND_CTF_DROP);
	m_DropTick = Server()->Tick();
	if (GetCarrier())
		GameServer()->SendBroadcast("", m_Carrier, false);
	m_LastCarrier = m_Carrier;
	m_Carrier = -1;
	m_Vel = vec2(5 * Dir, Dir == 0 ? 0 : -5);
	UpdateSpectators(-1);
}

void CFlag::Grab(int NewCarrier)
{
	PlaySound(m_Team == TEAM_RED ? SOUND_CTF_GRAB_EN : SOUND_CTF_GRAB_PL);
	if (m_AtStand)
		m_GrabTick = Server()->Tick();
	m_AtStand = false;
	m_Carrier = NewCarrier;
	GetCarrier()->m_FirstFreezeTick = 0;
	GameServer()->UnsetTelekinesis(this);
	UpdateSpectators(m_Carrier);
}

void CFlag::Tick()
{
	// for the CAdvancedEntity part
	m_Owner = GetCarrier() ? m_Carrier : GetLastCarrier() ? m_LastCarrier : -1;
	CAdvancedEntity::Tick();

	// plots
	int PlotID = GameServer()->GetTilePlotID(m_Pos);
	if (PlotID >= PLOT_START)
	{
		GameServer()->CreateDeath(m_Pos, GetCarrier() ? m_Carrier : GetLastCarrier() ? m_LastCarrier : -1);
		if (GetCarrier())
			Drop();
		m_Vel = vec2(0, 0);
		m_Pos = m_PrevPos = GameServer()->m_aPlots[PlotID].m_ToTele;
	}

	if (GetCarrier())
	{
		if (GetCarrier()->m_IsFrozen && GetCarrier()->m_FirstFreezeTick != 0)
			if (Server()->Tick() > GetCarrier()->m_FirstFreezeTick + Server()->TickSpeed() * 8)
				Drop(GetCarrier()->GetAimDir());
	}
	else
	{
		CCharacter *apCloseCCharacters[MAX_CLIENTS];
		int Num = GameWorld()->FindEntities(m_Pos, GetProximityRadius(), (CEntity**)apCloseCCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for (int i = 0; i < Num; i++)
		{
			if (!apCloseCCharacters[i] || apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_SPECTATORS || GameServer()->Collision()->IntersectLine(m_Pos, apCloseCCharacters[i]->GetPos(), NULL, NULL))
				continue;

			if (GetCarrier() == apCloseCCharacters[i] || (GetLastCarrier() == apCloseCCharacters[i] && (m_DropTick + Server()->TickSpeed() * 2) > Server()->Tick()))
				continue;

			// take the flag
			if (apCloseCCharacters[i]->HasFlag() == -1)
			{
				Grab(apCloseCCharacters[i]->GetPlayer()->GetCID());
				break;
			}
		}
	}

	if (!GetCarrier() && !m_AtStand)
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

	if (GameServer()->GetPlayerChar(SnappingClient) && GetCarrier())
	{
		if (GetCarrier()->IsPaused())
			return;

		if (!CmaskIsSet(GetCarrier()->Teams()->TeamMask(GetCarrier()->Team(), -1, GetCarrier()->GetPlayer()->GetCID()), SnappingClient))
			return;
	}

	CNetObj_Flag *pFlag = (CNetObj_Flag *)Server()->SnapNewItem(NETOBJTYPE_FLAG, m_Team, sizeof(CNetObj_Flag));
	if (!pFlag)
		return;

	pFlag->m_X = round_to_int(m_Pos.x);
	pFlag->m_Y = round_to_int(m_Pos.y);
	pFlag->m_Team = m_Team;
}