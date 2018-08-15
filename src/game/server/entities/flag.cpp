/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <game/server/gamecontext.h>
#include <engine/shared/config.h>
#include <game/server/teams.h>
#include "flag.h"
#include <game/server/gamemodes/ddrace.h>
#include "character.h"

CFlag::CFlag(CGameWorld *pGameWorld, int Team, vec2 Pos)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_FLAG, Pos, ms_PhysSize)
{
	m_Pos = Pos;
	m_StandPos = Pos;
	m_Team = Team;
	m_PrevPos = m_Pos;
	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Pos));

	GameWorld()->InsertEntity(this);

	Reset(true);
}

void CFlag::Reset(bool Init)
{
	if (!Init)
	{
		if (g_Config.m_SvFlagSounds)
			GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "flag_return");
		GameServer()->CreateDeath(m_Pos, m_pCarrier ? m_pCarrier->GetPlayer()->GetCID() : m_pLastCarrier ? m_pLastCarrier->GetPlayer()->GetCID() : -1);
	}
	m_pCarrier = NULL;
	m_pLastCarrier = NULL;
	m_AtStand = 1;
	m_Pos = m_StandPos;
	m_Vel = vec2(0,0);
	m_GrabTick = 0;
	m_DropFreezeTick = 0;
	m_TeleCheckpoint = 0;
}

void CFlag::TickPaused()
{
	++m_DropTick;
	if(m_GrabTick)
		++m_GrabTick;
}

void CFlag::TickDefered()
{
	if (m_pCarrier && m_pCarrier->IsAlive())
		m_Pos = m_pCarrier->GetPos();
}

void CFlag::Drop(int Dir)
{
	if (g_Config.m_SvFlagSounds)
		GameServer()->CreateSoundGlobal(SOUND_CTF_DROP);
	m_DropTick = Server()->Tick();
	m_DropFreezeTick = Server()->Tick();
	if (m_pCarrier)
		GameServer()->SendBroadcast("", m_pCarrier->GetPlayer()->GetCID(), false);
	m_pLastCarrier = m_pCarrier;
	m_pCarrier = NULL;
	m_Vel = vec2(5 * Dir, Dir == 0 ? 0 : -5);
}

void CFlag::Grab(CCharacter *pChr)
{
	if (g_Config.m_SvFlagSounds)
		GameServer()->CreateSoundGlobal(m_Team == TEAM_RED ? SOUND_CTF_GRAB_EN : SOUND_CTF_GRAB_PL);
	if (m_AtStand)
		m_GrabTick = Server()->Tick();
	m_AtStand = false;
	m_pCarrier = pChr;
	m_pCarrier->m_FirstFreezeTick = 0;
}

void CFlag::Tick()
{
	// flag hits death-tile or left the game layer, reset it
	if (GameServer()->Collision()->GetCollisionAt(m_Pos.x, m_Pos.y) == TILE_DEATH || GameServer()->Collision()->GetFCollisionAt(m_Pos.x, m_Pos.y) == TILE_DEATH || GameLayerClipped(m_Pos))
	{
		Reset();
		return;
	}

	if (!m_pLastCarrier || !m_pLastCarrier->IsAlive())
		m_pLastCarrier = NULL;

	if (m_pCarrier && m_pCarrier->IsAlive())
	{
		if (m_pCarrier->m_IsFrozen && m_pCarrier->m_FirstFreezeTick != 0)
			if (Server()->Tick() > m_pCarrier->m_FirstFreezeTick + Server()->TickSpeed() * 8)
				Drop(m_pCarrier->GetAimDir());
	}
	else
	{
		CCharacter *apCloseCCharacters[MAX_CLIENTS];
		int Num = GameWorld()->FindEntities(m_Pos, ms_PhysSize, (CEntity**)apCloseCCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for (int i = 0; i < Num; i++)
		{
			if (!apCloseCCharacters[i] || apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_SPECTATORS || GameServer()->Collision()->IntersectLine(m_Pos, apCloseCCharacters[i]->GetPos(), NULL, NULL))
				continue;

			if (m_pCarrier == apCloseCCharacters[i] || (m_pLastCarrier == apCloseCCharacters[i] && (m_DropFreezeTick + Server()->TickSpeed() * 2) > Server()->Tick()))
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
		if (Server()->Tick() > m_DropTick + Server()->TickSpeed() * 90)
		{
			Reset();
			return;
		}
		else
			HandleDropped();
	}

	m_PrevPos = m_Pos;
}

bool CFlag::IsGrounded(bool SetVel)
{
	if ((GameServer()->Collision()->CheckPoint(m_Pos.x + ms_PhysSize, m_Pos.y + ms_PhysSize + 5))
		|| (GameServer()->Collision()->CheckPoint(m_Pos.x - ms_PhysSize, m_Pos.y + ms_PhysSize + 5)))
	{
		if (SetVel)
			m_Vel.x *= 0.75f;
		return true;
	}

	int MoveRestrictionsBelow = GameServer()->Collision()->GetMoveRestrictions(m_Pos + vec2(0, GetProximityRadius() + 4), 0.0f);
	if (MoveRestrictionsBelow & CANTMOVE_DOWN || GameServer()->Collision()->GetDTileIndex(GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x, m_Pos.y + GetProximityRadius() + 4))) == TILE_STOPA)
	{
		if (SetVel)
			m_Vel.x *= 0.925f;
		return true;
	}

	if (SetVel)
		m_Vel.x *= 0.98f;
	return false;
}

void CFlag::HandleDropped()
{
	//Gravity
	if (!m_TuneZone)
		m_Vel.y += GameServer()->Tuning()->m_Gravity;
	else
		m_Vel.y += GameServer()->TuningList()[m_TuneZone].m_Gravity;

	//Speedups
	if (GameServer()->Collision()->IsSpeedup(GameServer()->Collision()->GetMapIndex(m_Pos)))
	{
		int Force, MaxSpeed = 0;
		vec2 Direction, MaxVel;
		vec2 TempVel = m_Vel;
		float TeeAngle, SpeederAngle, DiffAngle, SpeedLeft, TeeSpeed;
		GameServer()->Collision()->GetSpeedup(GameServer()->Collision()->GetMapIndex(m_Pos), &Direction, &Force, &MaxSpeed);

		if (Force == 255 && MaxSpeed)
			m_Vel = Direction * (MaxSpeed / 5);
		else
		{
			if (MaxSpeed > 0 && MaxSpeed < 5) MaxSpeed = 5;
			if (MaxSpeed > 0)
			{
				if (Direction.x > 0.0000001f)
					SpeederAngle = -atan(Direction.y / Direction.x);
				else if (Direction.x < 0.0000001f)
					SpeederAngle = atan(Direction.y / Direction.x) + 2.0f * asin(1.0f);
				else if (Direction.y > 0.0000001f)
					SpeederAngle = asin(1.0f);
				else
					SpeederAngle = asin(-1.0f);

				if (SpeederAngle < 0)
					SpeederAngle = 4.0f * asin(1.0f) + SpeederAngle;

				if (TempVel.x > 0.0000001f)
					TeeAngle = -atan(TempVel.y / TempVel.x);
				else if (TempVel.x < 0.0000001f)
					TeeAngle = atan(TempVel.y / TempVel.x) + 2.0f * asin(1.0f);
				else if (TempVel.y > 0.0000001f)
					TeeAngle = asin(1.0f);
				else
					TeeAngle = asin(-1.0f);

				if (TeeAngle < 0)
					TeeAngle = 4.0f * asin(1.0f) + TeeAngle;

				TeeSpeed = sqrt(pow(TempVel.x, 2) + pow(TempVel.y, 2));

				DiffAngle = SpeederAngle - TeeAngle;
				SpeedLeft = MaxSpeed / 5.0f - cos(DiffAngle) * TeeSpeed;
				if (abs(SpeedLeft) > Force && SpeedLeft > 0.0000001f)
					TempVel += Direction * Force;
				else if (abs(SpeedLeft) > Force)
					TempVel += Direction * -Force;
				else
					TempVel += Direction * SpeedLeft;
			}
			else
				TempVel += Direction * Force;
			m_Vel = TempVel;
		}
	}

	// tiles
	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_Pos);
	m_TuneZone = GameServer()->Collision()->IsTune(CurrentIndex);

	std::list < int > Indices = GameServer()->Collision()->GetMapIndices(m_PrevPos, m_Pos);
	if (!Indices.empty())
		for (std::list < int >::iterator i = Indices.begin(); i != Indices.end(); i++)
			HandleTiles(*i);
	else
	{
		HandleTiles(CurrentIndex);
	}
	IsGrounded(true);
	GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(ms_PhysSize, ms_PhysSize), 0.5f);
}

bool CFlag::IsSwitchActiveCb(int Number, void* pUser)
{
	CFlag* pThis = (CFlag*)pUser;
	CCollision* pCollision = pThis->GameServer()->Collision();
	int Team = 0;
	if (pThis->m_pLastCarrier)
		Team = pThis->m_pLastCarrier->Team();
	return pCollision->m_pSwitchers && pCollision->m_pSwitchers[Number].m_Status[Team] && Team != TEAM_SUPER;
}

void CFlag::HandleTiles(int Index)
{
	CGameControllerDDrace* Controller = (CGameControllerDDrace*)GameServer()->m_pController;
	int MapIndex = Index;
	m_TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	m_TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);
	m_MoveRestrictions = GameServer()->Collision()->GetMoveRestrictions(IsSwitchActiveCb, this, m_Pos);

	// teleporters
	int tcp = GameServer()->Collision()->IsTCheckpoint(MapIndex);
	if (tcp)
		m_TeleCheckpoint = tcp;

	int z = GameServer()->Collision()->IsTeleport(MapIndex);
	if (z && Controller->m_TeleOuts[z - 1].size())
	{
		int Num = Controller->m_TeleOuts[z - 1].size();
		m_Pos = Controller->m_TeleOuts[z - 1][(!Num) ? Num : rand() % Num];
		return;
	}
	int evilz = GameServer()->Collision()->IsEvilTeleport(MapIndex);
	if (evilz && Controller->m_TeleOuts[evilz - 1].size())
	{
		int Num = Controller->m_TeleOuts[evilz - 1].size();
		m_Pos = Controller->m_TeleOuts[evilz - 1][(!Num) ? Num : rand() % Num];
		m_Vel.x = 0;
		m_Vel.y = 0;
		return;
	}
	if (GameServer()->Collision()->IsCheckEvilTeleport(MapIndex))
	{
		// first check if there is a TeleCheckOut for the current recorded checkpoint, if not check previous checkpoints
		for (int k = m_TeleCheckpoint - 1; k >= 0; k--)
		{
			if (Controller->m_TeleCheckOuts[k].size())
			{
				int Num = Controller->m_TeleCheckOuts[k].size();
				m_Pos = Controller->m_TeleCheckOuts[k][(!Num) ? Num : rand() % Num];
				m_Vel.x = 0;
				m_Vel.y = 0;
				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if (GameServer()->m_pController->CanSpawn(&SpawnPos, ENTITY_SPAWN))
		{
			m_Pos = SpawnPos;
			m_Vel.x = 0;
			m_Vel.y = 0;
		}
		return;
	}
	if (GameServer()->Collision()->IsCheckTeleport(MapIndex))
	{
		// first check if there is a TeleCheckOut for the current recorded checkpoint, if not check previous checkpoints
		for (int k = m_TeleCheckpoint - 1; k >= 0; k--)
		{
			if (Controller->m_TeleCheckOuts[k].size())
			{
				int Num = Controller->m_TeleCheckOuts[k].size();
				m_Pos = Controller->m_TeleCheckOuts[k][(!Num) ? Num : rand() % Num];
				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if (GameServer()->m_pController->CanSpawn(&SpawnPos, ENTITY_SPAWN))
			m_Pos = SpawnPos;
		return;
	}

	// stopper
	m_Vel = ClampVel(m_MoveRestrictions, m_Vel);
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

	pFlag->m_X = (int)m_Pos.x;
	pFlag->m_Y = (int)m_Pos.y;
	pFlag->m_Team = m_Team;
}