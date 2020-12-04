// made by fokkonaut

#include "advanced_entity.h"
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>

CAdvancedEntity::CAdvancedEntity(CGameWorld *pGameWorld, int Objtype, vec2 Pos, int ProximityRadius, int Owner, bool CheckDeath)
: CEntity(pGameWorld, Objtype, Pos, ProximityRadius)
{
	m_Pos = Pos;
	m_Owner = Owner;
	m_CheckDeath = CheckDeath;
	m_TeleCheckpoint = GetOwner() ? GetOwner()->m_TeleCheckpoint : 0;
	m_PrevPos = m_Pos;
}

void CAdvancedEntity::Reset()
{
	GameServer()->UnsetTelekinesis(this);
	GameWorld()->DestroyEntity(this);
}

CCharacter *CAdvancedEntity::GetOwner()
{
	return GameServer()->GetPlayerChar(m_Owner);
}

void CAdvancedEntity::Tick()
{
	if (m_Owner >= 0 && !GameServer()->m_apPlayers[m_Owner])
		m_Owner = -1;

	// weapon hits death-tile or left the game layer, reset it
	if (GameLayerClipped(m_Pos) || (m_CheckDeath && (GameServer()->Collision()->GetCollisionAt(m_Pos.x, m_Pos.y) == TILE_DEATH || GameServer()->Collision()->GetFCollisionAt(m_Pos.x, m_Pos.y) == TILE_DEATH)))
	{
		Reset();
		return;
	}
}

bool CAdvancedEntity::IsGrounded(bool SetVel)
{
	if ((GameServer()->Collision()->CheckPoint(m_Pos.x + GetProximityRadius(), m_Pos.y + GetProximityRadius() + 5))
		|| (GameServer()->Collision()->CheckPoint(m_Pos.x - GetProximityRadius(), m_Pos.y + GetProximityRadius() + 5)))
	{
		if (SetVel)
			m_Vel.x *= 0.75f;
		return true;
	}

	int MoveRestrictionsBelow = GameServer()->Collision()->GetMoveRestrictions(m_Pos + vec2(0, GetProximityRadius() + 4), 0.0f, GetOwner() ? GetOwner()->Core()->m_MoveRestrictionExtra : CCollision::MoveRestrictionExtra());
	if ((MoveRestrictionsBelow&CANTMOVE_DOWN) || GameServer()->Collision()->GetDTileIndex(GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x, m_Pos.y + GetProximityRadius() + 4))) == TILE_STOPA)
	{
		if (SetVel)
			m_Vel.x *= 0.925f;
		return true;
	}

	if (SetVel)
		m_Vel.x *= 0.98f;
	return false;
}

void CAdvancedEntity::HandleDropped()
{
	//Gravity
	if (!m_TuneZone)
		m_Vel.y += GameServer()->Tuning()->m_Gravity;
	else
		m_Vel.y += GameServer()->TuningList()[m_TuneZone].m_Gravity;

	//Speedups
	if (GameServer()->Collision()->IsSpeedup(GameServer()->Collision()->GetMapIndex(m_Pos)))
	{
		vec2 Direction, MaxVel, TempVel = m_Vel;
		int Force, MaxSpeed = 0;
		float TeeAngle, SpeederAngle, DiffAngle, SpeedLeft, TeeSpeed;
		GameServer()->Collision()->GetSpeedup(GameServer()->Collision()->GetMapIndex(m_Pos), &Direction, &Force, &MaxSpeed);
		if (Force == 255 && MaxSpeed)
		{
			m_Vel = Direction * (MaxSpeed / 5);
		}
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
				if (abs((int)SpeedLeft) > Force && SpeedLeft > 0.0000001f)
					TempVel += Direction * Force;
				else if (abs((int)SpeedLeft) > Force)
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
	GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(GetProximityRadius(), GetProximityRadius()), 0.5f);
}

bool CAdvancedEntity::IsSwitchActiveCb(int Number, void* pUser)
{
	CAdvancedEntity *pThis = (CAdvancedEntity *)pUser;
	CCollision* pCollision = pThis->GameServer()->Collision();
	int Team = 0;
	if (pThis->GetOwner())
		Team = pThis->GetOwner()->Team();
	return pCollision->m_pSwitchers && pCollision->m_pSwitchers[Number].m_Status[Team] && Team != TEAM_SUPER;
}

void CAdvancedEntity::HandleTiles(int Index)
{
	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
	int MapIndex = Index;
	m_TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	m_TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);
	m_MoveRestrictions = GameServer()->Collision()->GetMoveRestrictions(IsSwitchActiveCb, this, m_Pos, 18.0f, -1, GetOwner() ? GetOwner()->Core()->m_MoveRestrictionExtra : CCollision::MoveRestrictionExtra());

	// stopper
	m_Vel = ClampVel(m_MoveRestrictions, m_Vel);

	// teleporters
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
}
