// made by fokkonaut

#include "advanced_entity.h"
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>

CAdvancedEntity::CAdvancedEntity(CGameWorld *pGameWorld, int Objtype, vec2 Pos, vec2 Size, int Owner)
: CEntity(pGameWorld, Objtype, Pos)
{
	SetSize(Size);
	m_Pos = Pos;
	m_Owner = Owner;
	m_TeleCheckpoint = GetOwner() ? GetOwner()->m_TeleCheckpoint : 0;
	m_PrevPos = m_Pos;
	m_DDTeam = GetOwner() ? GetOwner()->Team() : 0;
	m_TeamMask = ((CGameControllerDDRace*)GameServer()->m_pController)->m_Teams.TeamMask(m_DDTeam);
	m_Elasticity = vec2(0.5f, 0.5f);
	m_LastInOutTeleporter = 0;
	m_Flags = EFlags::CHECK_DEATH | EFlags::APPLY_GRAVITY | EFlags::APPLY_GROUND_VEL | EFlags::APPLY_AIR_VEL | EFlags::ALLOW_VIP_PLUS | EFlags::CHECK_GAME_LAYER_CLIPPED;
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
	{
		m_Owner = -1;
		m_DDTeam = 0;
	}

	m_TeamMask = ((CGameControllerDDRace*)GameServer()->m_pController)->m_Teams.TeamMask(m_DDTeam);

	// weapon hits death-tile or left the game layer, reset it
	if ((IsFlags(EFlags::CHECK_GAME_LAYER_CLIPPED) && GameLayerClipped(m_Pos)) || (IsFlags(EFlags::CHECK_DEATH) && (GameServer()->Collision()->GetCollisionAt(m_Pos.x, m_Pos.y) == TILE_DEATH || GameServer()->Collision()->GetFCollisionAt(m_Pos.x, m_Pos.y) == TILE_DEATH)))
	{
		Reset();
		return;
	}
}

bool CAdvancedEntity::IsGrounded(bool GroundVel, bool AirVel)
{
	if (m_Size.x > GameServer()->Collision()->ms_MinStaticPhysSize || m_Size.y > GameServer()->Collision()->ms_MinStaticPhysSize)
	{
		if (GameServer()->Collision()->IsBoxGrounded(m_Pos, m_Size))
		{
			if (GroundVel)
				m_Vel.x *= 0.75f;
			return true;
		}
	}
	else
	{
		if ((GameServer()->Collision()->CheckPoint(m_Pos.x + GetProximityRadius(), m_Pos.y + GetProximityRadius() + 5))
		|| (GameServer()->Collision()->CheckPoint(m_Pos.x - GetProximityRadius(), m_Pos.y + GetProximityRadius() + 5)))
		{
			if (GroundVel)
				m_Vel.x *= 0.75f;
			return true;
		}
	}

	// dont return true like the characters, just set the vel
	if (m_MoveRestrictions&CANTMOVE_DOWN_LASERDOOR)
	{
		if (GroundVel)
			m_Vel.x *= 0.925f;
	}

	if (AirVel)
		m_Vel.x *= 0.98f;
	return false;
}

void CAdvancedEntity::HandleDropped()
{
	CTuningParams *pTuning = m_TuneZone ? &GameServer()->TuningList()[m_TuneZone] : GameServer()->Tuning();

	//Gravity
	if (IsFlags(EFlags::APPLY_GRAVITY))
	{
		m_Vel.y += pTuning->m_Gravity;
	}

	//Speedups
	if (GameServer()->Collision()->IsSpeedup(GameServer()->Collision()->GetMapIndex(m_Pos)))
	{
		vec2 Direction, MaxVel, TempVel = m_Vel;
		int Force, Type, MaxSpeed = 0;
		GameServer()->Collision()->GetSpeedup(GameServer()->Collision()->GetMapIndex(m_Pos), &Direction, &Force, &MaxSpeed, &Type);

		if(Type == TILE_SPEED_BOOST_OLD)
		{
			float TeeAngle, SpeederAngle, DiffAngle, SpeedLeft, TeeSpeed;
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
		else if(Type == TILE_SPEED_BOOST)
		{
			constexpr float MaxSpeedScale = 5.0f;
			if(MaxSpeed == 0)
			{
				float MaxRampSpeed = pTuning->m_VelrampRange / (50 * log(max((float)pTuning->m_VelrampCurvature, 1.01f)));
				MaxSpeed = max(MaxRampSpeed, pTuning->m_VelrampStart / 50) * MaxSpeedScale;
			}

			// (signed) length of projection
			float CurrentDirectionalSpeed = dot(Direction, m_Vel);
			float TempMaxSpeed = MaxSpeed / MaxSpeedScale;
			if(CurrentDirectionalSpeed + Force > TempMaxSpeed)
				TempVel += Direction * (TempMaxSpeed - CurrentDirectionalSpeed);
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
		m_LastInOutTeleporter = 0;
	}
	IsGrounded(IsFlags(EFlags::APPLY_GROUND_VEL), IsFlags(EFlags::APPLY_AIR_VEL));
	GameServer()->Collision()->MoveBox(IsSwitchActiveCb, this, &m_Pos, &m_Vel, m_Size, m_Elasticity, !Config()->m_SvStoppersPassthrough, GetMoveRestrictionExtra());
}

bool CAdvancedEntity::IsSwitchActiveCb(int Number, void* pUser)
{
	CAdvancedEntity *pThis = (CAdvancedEntity *)pUser;
	CCollision* pCollision = pThis->GameServer()->Collision();
	return pCollision->m_pSwitchers && pThis->m_DDTeam != TEAM_SUPER && pCollision->m_pSwitchers[Number].m_Status[pThis->m_DDTeam];
}

CCollision::MoveRestrictionExtra CAdvancedEntity::GetMoveRestrictionExtra()
{
	CCollision::MoveRestrictionExtra Extra = GetOwner() ? GetOwner()->Core()->m_MoveRestrictionExtra : CCollision::MoveRestrictionExtra();
	if (!IsFlags(EFlags::ALLOW_VIP_PLUS))
		Extra.m_VipPlus = false; // explicitly disallow passing the vip room if the entity wants to force it
	return Extra;
}

void CAdvancedEntity::HandleTiles(int Index)
{
	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
	int MapIndex = Index;
	m_TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	m_TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);

	m_MoveRestrictions = GameServer()->Collision()->GetMoveRestrictions(IsSwitchActiveCb, this, m_Pos, 18.0f, -1, GetMoveRestrictionExtra());

	// stopper
	m_Vel = ClampVel(m_MoveRestrictions, m_Vel);

	// teleporters
	int tcp = GameServer()->Collision()->IsTCheckpoint(MapIndex);
	if (tcp)
		m_TeleCheckpoint = tcp;

	int z = GameServer()->Collision()->IsTeleport(MapIndex);
	if (z && Controller->m_TeleOuts[z - 1].size())
	{
		int Num = Controller->m_TeleOuts[z - 1].size();
		vec2 NewPos = Controller->m_TeleOuts[z - 1][(!Num) ? Num : rand() % Num];
		if (GameServer()->Collision()->IsTeleportInOut(MapIndex))
		{
			if (m_LastInOutTeleporter == z || Num <= 1) // dont teleport when only 1 is there, bcs its our current tile then
				return;
			m_LastInOutTeleporter = z;

			while (GameServer()->Collision()->GetPureMapIndex(NewPos) == MapIndex)
				NewPos = Controller->m_TeleOuts[z - 1][rand() % Num];
		}
		m_PrevPos = m_Pos = NewPos;
		return;
	}
	int evilz = GameServer()->Collision()->IsEvilTeleport(MapIndex);
	if (evilz && Controller->m_TeleOuts[evilz - 1].size())
	{
		int Num = Controller->m_TeleOuts[evilz - 1].size();
		vec2 NewPos = Controller->m_TeleOuts[evilz - 1][(!Num) ? Num : rand() % Num];
		if (GameServer()->Collision()->IsTeleportInOut(MapIndex))
		{
			if (m_LastInOutTeleporter == evilz || Num <= 1) // dont teleport when only 1 is there, bcs its our current tile then
				return;
			m_LastInOutTeleporter = evilz;

			while (GameServer()->Collision()->GetPureMapIndex(NewPos) == MapIndex)
				NewPos = Controller->m_TeleOuts[evilz - 1][rand() % Num];
		}
		m_PrevPos = m_Pos = NewPos;
		m_Vel = vec2(0, 0);
		if (!Config()->m_SvTeleportHoldHook)
			ReleaseHooked();
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
				m_PrevPos = m_Pos = Controller->m_TeleCheckOuts[k][(!Num) ? Num : rand() % Num];
				m_Vel = vec2(0, 0);
				if (!Config()->m_SvTeleportHoldHook)
					ReleaseHooked();
				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if (GameServer()->m_pController->CanSpawn(&SpawnPos, ENTITY_SPAWN, m_DDTeam))
		{
			m_PrevPos = m_Pos = SpawnPos;
			m_Vel = vec2(0, 0);
			if (!Config()->m_SvTeleportHoldHook)
				ReleaseHooked();
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
				m_PrevPos = m_Pos = Controller->m_TeleCheckOuts[k][(!Num) ? Num : rand() % Num];
				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if (GameServer()->m_pController->CanSpawn(&SpawnPos, ENTITY_SPAWN, m_DDTeam))
			m_PrevPos = m_Pos = SpawnPos;
		return;
	}
}
