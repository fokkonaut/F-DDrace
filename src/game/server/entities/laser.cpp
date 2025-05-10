/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <generated/server_data.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>

#include "character.h"
#include "laser.h"
#include "taser_shield.h"

#include <engine/shared/config.h>
#include <game/server/teams.h>

CLaser::CLaser(CGameWorld* pGameWorld, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, int Type, int TaserStrength)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, Pos)
{
	m_Pos = Pos;
	m_Owner = Owner;
	m_Energy = StartEnergy;
	m_Dir = Direction;
	m_Bounces = 0;
	m_EvalTick = 0;
	m_TelePos = vec2(0, 0);
	m_WasTele = false;
	m_Type = Type;
	m_TaserStrength = TaserStrength;
	m_TeleportCancelled = false;
	m_IsBlueTeleport = false;
	m_TeamMask = GameServer()->GetPlayerChar(Owner) ? GameServer()->GetPlayerChar(Owner)->TeamMask() : Mask128();

	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Pos));
	CTuningParams *pTuning = GameServer()->TuningFromChrOrZone(m_Owner, m_TuneZone);
	m_ShotgunStrength = pTuning->m_ShotgunStrength;
	m_BounceNum = pTuning->m_LaserBounceNum;
	m_BounceCost = pTuning->m_LaserBounceCost;
	m_BounceDelay = pTuning->m_LaserBounceDelay;

	GameWorld()->InsertEntity(this);
	DoBounce();
}

bool CLaser::HitEntity(vec2 From, vec2 To)
{
	vec2 At;
	CCharacter* pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	bool pDontHitSelf = Config()->m_SvOldLaser || (m_Bounces == 0 && !m_WasTele);

	bool CheckPlotTaserDestroy = false;
	bool PlotDoorOnly = true;
	int Types = (1<<CGameWorld::ENTTYPE_CHARACTER);
	if (m_Type == WEAPON_SHOTGUN)
	{
		if (Config()->m_SvInteractiveDrops)
		{
			Types |= (1<<CGameWorld::ENTTYPE_FLAG) | (1<<CGameWorld::ENTTYPE_PICKUP_DROP) | (1<<CGameWorld::ENTTYPE_MONEY) | (1<<CGameWorld::ENTTYPE_GROG) | (1<<CGameWorld::ENTTYPE_HELICOPTER);
		}
	}
	else if (m_Type == WEAPON_TASER)
	{
		if (Config()->m_SvInteractiveDrops)
		{
			Types |= (1<<CGameWorld::ENTTYPE_HELICOPTER);
		}
		CPlayer *pOwner = m_Owner >= 0 ? GameServer()->m_apPlayers[m_Owner] : 0;
		if (pOwner)
		{
			CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[pOwner->GetAccID()];
			if (pAccount->m_PoliceLevel >= 4)
			{
				Types |= (1<<CGameWorld::ENTTYPE_DOOR);
				CheckPlotTaserDestroy = true;
			}
			if (pAccount->m_PoliceLevel >= 5)
			{
				Types |= (1<<CGameWorld::ENTTYPE_PICKUP) | (1<<CGameWorld::ENTTYPE_BUTTON) | (1<<CGameWorld::ENTTYPE_SPEEDUP) | (1<<CGameWorld::ENTTYPE_TELEPORTER);
				CheckPlotTaserDestroy = true;
				// Allow only tasering the door with police 4. police 5 can also destroy objects ON the plot
				PlotDoorOnly = false;
			}
		}
	}
	else if (m_Type == WEAPON_LASER)
	{
		if (Config()->m_SvInteractiveDrops)
		{
			Types |= (1<<CGameWorld::ENTTYPE_HELICOPTER);
		}
	}
	CCharacter *pChr = 0;
	CAdvancedEntity *pEnt = 0;
	CEntity *pIntersected = 0;

	if (pOwnerChar ? (!(pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_RIFLE) && (m_Type == WEAPON_LASER || m_Type == WEAPON_TASER)) || (!(pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_SHOTGUN) && m_Type == WEAPON_SHOTGUN) : Config()->m_SvHit)
		pIntersected = GameWorld()->IntersectEntityTypes(m_Pos, To, 0.f, At, pDontHitSelf ? pOwnerChar : 0, m_Owner, Types, 0, CheckPlotTaserDestroy, PlotDoorOnly);
	else
		pIntersected = GameWorld()->IntersectEntityTypes(m_Pos, To, 0.f, At, pDontHitSelf ? pOwnerChar : 0, m_Owner, Types, pOwnerChar, CheckPlotTaserDestroy, PlotDoorOnly);

	bool IsCharacter = false;
	if (pIntersected)
	{
		IsCharacter = pIntersected->GetObjType() == CGameWorld::ENTTYPE_CHARACTER;
		if (IsCharacter)
		{
			pChr = (CCharacter *)pIntersected;
		}
		else if (m_Type == WEAPON_SHOTGUN || m_Type == WEAPON_LASER || m_Type == WEAPON_TASER)
		{
			pEnt = (CAdvancedEntity *)pIntersected;
			if (pEnt->GetObjType() == CGameWorld::ENTTYPE_FLAG)
			{
				if (((CFlag *)pEnt)->GetCarrier())
					return false;
			}
			else
				pChr = pEnt->GetOwner();
		}
	}

	bool IsPlotTaser = CheckPlotTaserDestroy && pIntersected && pIntersected->m_PlotID >= PLOT_START;
	if (!IsPlotTaser)
	{
		if ((!IsCharacter && !pEnt) || ((IsCharacter && !pChr) || (IsCharacter && pChr == pOwnerChar && Config()->m_SvOldLaser) || (pChr != pOwnerChar && pOwnerChar ? (pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_RIFLE && (m_Type == WEAPON_LASER || m_Type == WEAPON_TASER)) || (pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_SHOTGUN && m_Type == WEAPON_SHOTGUN) : !Config()->m_SvHit)))
			return false;

		m_Energy = -1;
	}
	else
	{
		m_Energy -= distance(From, At);
	}
	m_From = From;
	m_Pos = At;

	if (m_Type == WEAPON_SHOTGUN)
	{
		static const vec2 StackedLaserShotgunBugSpeed = vec2(-2147483648.0f, -2147483648.0f);
		vec2 Vel = IsCharacter ? pChr->Core()->m_Vel : pEnt->GetVel();
		vec2 Pos = IsCharacter ? pChr->Core()->m_Pos : pEnt->GetPos();
		vec2 Temp = Vel;
		bool ShotgunBug = false;

		if (!Config()->m_SvOldLaser)
		{
			Temp = Vel + normalize(m_PrevPos - Pos) * m_ShotgunStrength;
			ShotgunBug = m_PrevPos == Pos;
		}
		else if (pOwnerChar)
		{
			Temp = Vel + normalize(pOwnerChar->Core()->m_Pos - Pos) * m_ShotgunStrength;
			ShotgunBug = pOwnerChar->Core()->m_Pos == Pos;
		}

		if (ShotgunBug && Config()->m_SvShotgunBug)
			Temp = StackedLaserShotgunBugSpeed;

		if (IsCharacter)
		{
			if (!GameServer()->Durak()->ActivelyPlaying(pChr->GetPlayer()->GetCID()))
			{
				pChr->Core()->m_Vel = ClampVel(pChr->m_MoveRestrictions, Temp);
			}
		}
		else
		{
			if (pEnt->GetObjType() == CGameWorld::ENTTYPE_FLAG)
				((CFlag *)pEnt)->SetAtStand(false);
			else if (pEnt->GetObjType() == CGameWorld::ENTTYPE_HELICOPTER)
				Temp *= 0.5f;

			pEnt->SetVel(ClampVel(pEnt->GetMoveRestrictions(), Temp));
			return true;
		}
	}
	else if (m_Type == WEAPON_LASER)
	{
		if (IsCharacter && pChr->m_IsZombie)
		{
			vec2 Pos = At + normalize(At - From) * vec2(-32.f, -32.f);
			GameServer()->CreateExplosion(Pos, m_Owner, WEAPON_LASER, true, pOwnerChar ? pOwnerChar->Team() : pChr->Team(), m_TeamMask);
		}
		else if (pChr)
		{
			pChr->m_GotLasered = true;
			pChr->UnFreeze();
		}
	}
	else if (m_Type == WEAPON_TASER)
	{
		if (pChr)
		{
			int RandomPercentage = random(0, 100);
			if (pChr->GetPlayer()->m_TaserShield > 0 && pChr->GetPlayer()->m_TaserShield >= RandomPercentage)
			{
				pChr->GetPlayer()->m_TaserShield = max(pChr->GetPlayer()->m_TaserShield - 5, 0);
				new CTaserShield(GameWorld(), pChr->GetPos(), pChr->GetPlayer()->GetCID());
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), pChr->GetPlayer()->Localize("Taser shield has been used, -5%%, new current: %d%%"), pChr->GetPlayer()->m_TaserShield);
				GameServer()->SendChatTarget(pChr->GetPlayer()->GetCID(), aBuf);
			}
			else
			{
				pChr->Freeze(m_TaserStrength / 10.f);
				pChr->m_GotLasered = true;
			}
		}
	 	else if (IsPlotTaser)
		{
			// taser destroy
			if (pIntersected->IsPlotDoor())
			{
				if (GameServer()->OnPlotDoorTaser(pIntersected->m_PlotID, m_TaserStrength, m_Owner, At))
				{
					GameServer()->SetPlotDoorStatus(pIntersected->m_PlotID, false);
				}
				m_Energy = -1;
			}
			else if (pOwnerChar && pOwnerChar->m_DrawEditor.SafelyDestroyDrawEntity(pIntersected))
			{
				GameServer()->CreateDeath(At, m_Owner);
			}

			m_TaserStrength -= 2;
			if (m_TaserStrength <= 0)
				m_Energy = -1;
			return true;
		}

		if (pEnt && pEnt->GetObjType() == CGameWorld::ENTTYPE_HELICOPTER)
		{
			CHelicopter* pHelicopter = (CHelicopter*)pEnt;
			pHelicopter->TakeDamage((float)(m_TaserStrength + 1) / 1.5f, At, pOwnerChar ? pOwnerChar->GetPlayer()->GetCID() : -1);
			return true;
		}
	}

	if (IsCharacter)
	{
		pChr->TakeDamage(vec2(0.f, 0.f), vec2(0, 0), g_pData->m_Weapons.m_aId[GameServer()->GetWeaponType(m_Type)].m_Damage, m_Owner, m_Type);
	}
	return true;
}

void CLaser::DoBounce()
{
	m_EvalTick = Server()->Tick();

	if (m_Energy < 0)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}
	m_PrevPos = m_Pos;
	vec2 Coltile;

	int Res;
	int z;

	if (m_WasTele)
	{
		m_PrevPos = m_TelePos;
		m_Pos = m_TelePos;
		m_TelePos = vec2(0, 0);
	}

	vec2 To = m_Pos + m_Dir * m_Energy;

	Res = GameServer()->Collision()->IntersectLineTeleWeapon(m_Pos, To, &Coltile, &To, &z);

	if (Res)
	{
		if (!HitEntity(m_Pos, To))
		{
			// intersected
			m_From = m_Pos;
			m_Pos = To;

			vec2 TempPos = m_Pos;
			vec2 TempDir = m_Dir * 4.0f;

			int f = 0;
			if (Res == -1)
			{
				f = GameServer()->Collision()->GetTile(round_to_int(Coltile.x), round_to_int(Coltile.y));
				GameServer()->Collision()->SetCollisionAt(round_to_int(Coltile.x), round_to_int(Coltile.y), TILE_SOLID);
			}
			GameServer()->Collision()->MovePoint(&TempPos, &TempDir, 1.0f, 0);
			if (Res == -1)
			{
				GameServer()->Collision()->SetCollisionAt(round_to_int(Coltile.x), round_to_int(Coltile.y), f);
			}
			m_Pos = TempPos;
			m_Dir = normalize(TempDir);

			m_Energy -= distance(m_From, m_Pos) + m_BounceCost;

			if (Res == TILE_TELEINWEAPON && ((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts[z - 1].size())
			{
				int Num = ((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts[z - 1].size();
				m_TelePos = ((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts[z - 1][(!Num) ? Num : rand() % Num];
				m_WasTele = true;
			}
			else
			{
				m_Bounces++;
				m_WasTele = false;
			}

			if (m_Bounces > m_BounceNum)
				m_Energy = -1;

			GameServer()->CreateSound(m_Pos, SOUND_LASER_BOUNCE, m_TeamMask);
		}
	}
	else
	{
		if (!HitEntity(m_Pos, To))
		{
			m_From = m_Pos;
			m_Pos = To;
			m_Energy = -1;
		}
	}

	CCharacter* pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	if (m_Owner >= 0 && m_Energy <= 0 && m_Pos && !m_TeleportCancelled && pOwnerChar &&
		pOwnerChar->IsAlive() && pOwnerChar->m_HasTeleLaser && m_Type == WEAPON_LASER)
	{
		vec2 PossiblePos;
		bool Found = false;

		// Check if the laser hits a player.
		bool pDontHitSelf = Config()->m_SvOldLaser || (m_Bounces == 0 && !m_WasTele);
		vec2 At;
		CCharacter* pHit;
		if (pOwnerChar ? (!(pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_RIFLE) && m_Type == WEAPON_LASER) : Config()->m_SvHit)
			pHit = GameWorld()->IntersectCharacter(m_Pos, To, 0.f, At, pDontHitSelf ? pOwnerChar : 0, m_Owner);
		else
			pHit = GameWorld()->IntersectCharacter(m_Pos, To, 0.f, At, pDontHitSelf ? pOwnerChar : 0, m_Owner, pOwnerChar);

		if (pHit)
			Found = GetNearestAirPosPlayer(pHit->GetPos(), &PossiblePos);
		else
			Found = GetNearestAirPos(m_Pos, m_From, &PossiblePos);

		if (Found && PossiblePos)
		{
			pOwnerChar->m_TeleGunPos = PossiblePos;
			pOwnerChar->m_TeleGunTeleport = true;
			pOwnerChar->m_IsBlueTeleGunTeleport = m_IsBlueTeleport;
		}
	}
	else if (m_Owner >= 0 && m_Pos)
	{
		int MapIndex = GameServer()->Collision()->GetPureMapIndex(Coltile);
		int TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);
		bool IsSwitchTeleGun = GameServer()->Collision()->IsSwitch(MapIndex) == TILE_ALLOW_TELE_GUN || (pOwnerChar && pOwnerChar->m_AlwaysTeleWeapon);
		bool IsBlueSwitchTeleGun = GameServer()->Collision()->IsSwitch(MapIndex) == TILE_ALLOW_BLUE_TELE_GUN;
		int IsTeleInWeapon = GameServer()->Collision()->IsTeleportWeapon(MapIndex);

		if (!IsTeleInWeapon)
		{
			if (IsSwitchTeleGun || IsBlueSwitchTeleGun) {
				// Delay specifies which weapon the tile should work for.
				// Delay = 0 means all.
				int delay = GameServer()->Collision()->GetSwitchDelay(MapIndex);

				if ((delay != 3 && delay != 0) && m_Type == WEAPON_LASER) {
					IsSwitchTeleGun = IsBlueSwitchTeleGun = false;
				}
			}

			m_IsBlueTeleport = TileFIndex == TILE_ALLOW_BLUE_TELE_GUN || IsBlueSwitchTeleGun;

			// Teleport is canceled if the last bounce tile is not a TILE_ALLOW_TELE_GUN.
			// Teleport also works if laser didn't bounce.
			m_TeleportCancelled =
				m_Type == WEAPON_LASER
				&& (TileFIndex != TILE_ALLOW_TELE_GUN
					&& TileFIndex != TILE_ALLOW_BLUE_TELE_GUN
					&& !IsSwitchTeleGun
					&& !IsBlueSwitchTeleGun);
		}
	}

	//m_Owner = -1;
}

void CLaser::Reset()
{
	GameWorld()->DestroyEntity(this);
}

void CLaser::Tick()
{
	if (m_Owner >= 0 && (Config()->m_SvDestroyLasersOnDeath || GameServer()->Arenas()->FightStarted(m_Owner)))
	{
		CCharacter* pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
		if (!(pOwnerChar && pOwnerChar->IsAlive()))
		{
			Reset();
		}
	}

	if ((Server()->Tick() - m_EvalTick) > (Server()->TickSpeed() * m_BounceDelay / 1000.0f))
		DoBounce();
}

void CLaser::TickPaused()
{
	++m_EvalTick;
}

void CLaser::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient, m_Pos) && NetworkClipped(SnappingClient, m_From))
		return;
	CCharacter* OwnerChar = 0;
	if (m_Owner >= 0)
		OwnerChar = GameServer()->GetPlayerChar(m_Owner);
	if (!OwnerChar)
		return;

	CCharacter* pOwnerChar = 0;
	Mask128 TeamMask = Mask128();

	if (m_Owner >= 0)
		pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

	if (pOwnerChar && pOwnerChar->IsAlive())
		TeamMask = pOwnerChar->TeamMask();

	if (!CmaskIsSet(TeamMask, SnappingClient))
		return;

	if(GameServer()->GetClientDDNetVersion(SnappingClient) >= VERSION_DDNET_MULTI_LASER)
	{
		CNetObj_DDNetLaser *pObj = static_cast<CNetObj_DDNetLaser *>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, GetID(), sizeof(CNetObj_DDNetLaser)));
		if(!pObj)
			return;

		int Owner = m_Owner;
		if (!Server()->Translate(Owner, SnappingClient))
			Owner = -1;

		pObj->m_ToX = round_to_int(m_Pos.x);
		pObj->m_ToY = round_to_int(m_Pos.y);
		pObj->m_FromX = round_to_int(m_From.x);
		pObj->m_FromY = round_to_int(m_From.y);
		pObj->m_StartTick = m_EvalTick;
		pObj->m_Owner = Owner;
		pObj->m_Type = m_Type == WEAPON_LASER ? LASERTYPE_RIFLE : m_Type == WEAPON_SHOTGUN ? LASERTYPE_SHOTGUN : m_Type == WEAPON_TASER ? LASERTYPE_FREEZE : -1;
	}
	else
	{
		CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
		if(!pObj)
			return;

		pObj->m_X = round_to_int(m_Pos.x);
		pObj->m_Y = round_to_int(m_Pos.y);
		pObj->m_FromX = round_to_int(m_From.x);
		pObj->m_FromY = round_to_int(m_From.y);
		pObj->m_StartTick = m_EvalTick;
	}
}
