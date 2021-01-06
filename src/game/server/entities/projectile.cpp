/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>
#include "projectile.h"

#include <engine/shared/config.h>
#include <game/server/teams.h>
#include <generated/server_data.h>

#include "character.h"

CProjectile::CProjectile
(
	CGameWorld* pGameWorld,
	int Type,
	int Owner,
	vec2 Pos,
	vec2 Dir,
	int Span,
	bool Freeze,
	bool Explosive,
	float Force,
	int SoundImpact,
	int Layer,
	int Number,
	bool Spooky
)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE, vec2(round_to_int(Pos.x), round_to_int(Pos.y)))
{
	m_Type = Type;
	m_Pos = Pos;

	/* vanilla 0.7 has this instead of `m_Direction = Dir` to sync grenade curvature with client, but this made flappy stop working
	and I think that this issue needs a clientside fix, not a server side one
	m_Direction.x = round_to_int(Dir.x*100.0f) / 100.0f;
	m_Direction.y = round_to_int(Dir.y*100.0f) / 100.0f;*/
	m_Direction = Dir;

	m_LifeSpan = Span;
	m_Owner = Owner;
	m_Force = Force;
	m_SoundImpact = SoundImpact;
	m_StartTick = Server()->Tick();
	m_Explosive = Explosive;

	m_Layer = Layer;
	m_Number = Number;
	m_Freeze = Freeze;

	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Pos));

	// F-DDrace
	m_Spooky = Spooky;

	// activate faked tuning for tunezones, vanilla shotgun and gun, straightgrenade
	CPlayer *pOwner = m_Owner >= 0 ? GameServer()->m_apPlayers[m_Owner] : 0;
	m_DDrace = !pOwner || pOwner->m_Gamemode == GAMEMODE_DDRACE || (m_Type != WEAPON_GUN && m_Type != WEAPON_SHOTGUN);
	m_DefaultTuning = IsDefaultTuning() && m_DDrace;

	m_LastResetPos = Pos;
	m_LastResetTick = Server()->Tick();
	m_CalculatedVel = false;
	m_CurPos = GetPos((Server()->Tick() - m_StartTick) / (float)Server()->TickSpeed());

	GameWorld()->InsertEntity(this);
}

void CProjectile::Reset()
{
	if (m_LifeSpan > -2)
		GameWorld()->DestroyEntity(this);
}

vec2 CProjectile::GetPos(float Time)
{
	float Curvature;
	float Speed;
	GetTunings(&Curvature, &Speed);
	return CalcPos(m_Pos, m_Direction, Curvature, Speed, Time);
}

void CProjectile::Tick()
{
	float Pt = (Server()->Tick() - m_StartTick - 1) / (float)Server()->TickSpeed();
	float Ct = (Server()->Tick() - m_StartTick) / (float)Server()->TickSpeed();
	vec2 PrevPos = GetPos(Pt);
	m_CurPos = GetPos(Ct);
	vec2 ColPos;
	vec2 NewPos;
	int Collide = GameServer()->Collision()->IntersectLine(PrevPos, m_CurPos, &ColPos, &NewPos);
	CCharacter* pOwnerChar = 0;

	if (m_Owner >= 0)
		pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

	CCharacter* pTargetChr = 0;

	if (pOwnerChar ? !(pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_GRENADE) : Config()->m_SvHit)
		pTargetChr = GameWorld()->IntersectCharacter(PrevPos, ColPos, m_Freeze ? 1.0f : 6.0f, ColPos, pOwnerChar, m_Owner);

	if (m_LifeSpan > -1)
		m_LifeSpan--;

	Mask128 TeamMask = Mask128();
	bool IsWeaponCollide = false;
	if
		(
			pOwnerChar &&
			pTargetChr &&
			pOwnerChar->IsAlive() &&
			pTargetChr->IsAlive() &&
			!pTargetChr->CanCollide(m_Owner)
			)
	{
		IsWeaponCollide = true;
	}
	if (pOwnerChar && pOwnerChar->IsAlive())
	{
		TeamMask = pOwnerChar->Teams()->TeamMask(pOwnerChar->Team(), -1, m_Owner);
	}
	else if (m_Owner >= 0 && (GameServer()->GetProjectileType(m_Type) != WEAPON_GRENADE || Config()->m_SvDestroyBulletsOnDeath))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	if (((pTargetChr && (pOwnerChar ? !(pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_GRENADE) : Config()->m_SvHit || m_Owner == -1 || pTargetChr == pOwnerChar)) || Collide || GameLayerClipped(m_CurPos)) && !IsWeaponCollide)
	{
		if (m_Explosive/*??*/ && (!pTargetChr || (pTargetChr && (!m_Freeze || (m_Type == WEAPON_SHOTGUN && Collide)))))
		{
			GameServer()->CreateExplosion(ColPos, m_Owner, m_Type, m_Owner == -1, (!pTargetChr ? -1 : pTargetChr->Team()),
				(m_Owner != -1) ? TeamMask : Mask128());
			GameServer()->CreateSound(ColPos, m_SoundImpact,
				(m_Owner != -1) ? TeamMask : Mask128());
		}
		else if (m_Freeze)
		{
			CCharacter* apEnts[MAX_CLIENTS];
			int Num = GameWorld()->FindEntities(m_CurPos, 1.0f, (CEntity * *)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for (int i = 0; i < Num; ++i)
				if (apEnts[i] && (m_Layer != LAYER_SWITCH || (m_Layer == LAYER_SWITCH && GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[apEnts[i]->Team()])))
					apEnts[i]->Freeze();
		}
		// F-DDrace
		if (pTargetChr)
		{
			if (!m_Explosive)
			{
				pTargetChr->TakeDamage(m_Direction * max(0.001f, m_Force), m_Direction*-1, g_pData->m_Weapons.m_aId[GameServer()->GetProjectileType(m_Type)].m_Damage, m_Owner, m_Type);
			}
			if (m_Spooky)
			{
				pTargetChr->SetEmote(EMOTE_SURPRISE, Server()->Tick() + 2 * Server()->TickSpeed());
				GameServer()->SendEmoticon(pTargetChr->GetPlayer()->GetCID(), EMOTICON_GHOST);
			}
		}

		if (pOwnerChar && ColPos && !GameLayerClipped(ColPos) &&
			((m_Type == WEAPON_GRENADE && pOwnerChar->m_HasTeleGrenade) || (m_Type == WEAPON_GUN && pOwnerChar->m_HasTeleGun)))
		{
			int MapIndex = GameServer()->Collision()->GetPureMapIndex(pTargetChr ? pTargetChr->GetPos() : ColPos);
			int TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);
			bool IsSwitchTeleGun = GameServer()->Collision()->IsSwitch(MapIndex) == TILE_ALLOW_TELE_GUN || pOwnerChar->m_AlwaysTeleWeapon;
			bool IsBlueSwitchTeleGun = GameServer()->Collision()->IsSwitch(MapIndex) == TILE_ALLOW_BLUE_TELE_GUN;

			if (IsSwitchTeleGun || IsBlueSwitchTeleGun) {
				// Delay specifies which weapon the tile should work for.
				// Delay = 0 means all.
				int delay = GameServer()->Collision()->GetSwitchDelay(MapIndex);

				if (delay == 1 && m_Type != WEAPON_GUN)
					IsSwitchTeleGun = IsBlueSwitchTeleGun = false;
				if (delay == 2 && m_Type != WEAPON_GRENADE)
					IsSwitchTeleGun = IsBlueSwitchTeleGun = false;
				if (delay == 3 && m_Type != WEAPON_LASER)
					IsSwitchTeleGun = IsBlueSwitchTeleGun = false;
			}

			if (TileFIndex == TILE_ALLOW_TELE_GUN
				|| TileFIndex == TILE_ALLOW_BLUE_TELE_GUN
				|| IsSwitchTeleGun
				|| IsBlueSwitchTeleGun
				|| pTargetChr)
			{
				bool Found;
				vec2 PossiblePos;

				if (!Collide)
					Found = GetNearestAirPosPlayer(pTargetChr->GetPos(), &PossiblePos);
				else
					Found = GetNearestAirPos(NewPos, m_CurPos, &PossiblePos);

				if (Found && PossiblePos)
				{
					pOwnerChar->m_TeleGunPos = PossiblePos;
					pOwnerChar->m_TeleGunTeleport = true;
					pOwnerChar->m_IsBlueTeleGunTeleport = TileFIndex == TILE_ALLOW_BLUE_TELE_GUN || IsBlueSwitchTeleGun;
				}
			}
		}

		if (Collide && m_Bouncing != 0)
		{
			m_StartTick = Server()->Tick();
			m_Pos = NewPos + (-(m_Direction * 4));
			if (m_Bouncing == 1)
				m_Direction.x = -m_Direction.x;
			else if (m_Bouncing == 2)
				m_Direction.y = -m_Direction.y;
			if (fabs(m_Direction.x) < 1e-6)
				m_Direction.x = 0;
			if (fabs(m_Direction.y) < 1e-6)
				m_Direction.y = 0;
			m_Pos += m_Direction;
		}
		else if (GameServer()->GetProjectileType(m_Type) == WEAPON_GUN)
		{
			if (pOwnerChar && (pOwnerChar->GetPlayer()->m_Gamemode == GAMEMODE_DDRACE || m_Type != WEAPON_GUN))
				GameServer()->CreateDamage(m_CurPos, m_Owner, m_Direction, 1, 0, (pTargetChr && m_Owner == pTargetChr->GetPlayer()->GetCID()), m_Owner != -1 ? TeamMask : CmaskAll(), 10);
			GameWorld()->DestroyEntity(this);
			return;
		}
		else
		{
			if (!m_Freeze)
			{
				GameWorld()->DestroyEntity(this);
				return;
			}
		}
	}
	if (m_LifeSpan == -1)
	{
		if (m_Explosive)
		{
			if (m_Owner >= 0)
				pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

			Mask128 TeamMask = Mask128();
			if (pOwnerChar && pOwnerChar->IsAlive())
			{
				TeamMask = pOwnerChar->Teams()->TeamMask(pOwnerChar->Team(), -1, m_Owner);
			}

			GameServer()->CreateExplosion(ColPos, m_Owner, m_Type, m_Owner == -1, (!pOwnerChar ? -1 : pOwnerChar->Team()),
				(m_Owner != -1) ? TeamMask : Mask128());
			GameServer()->CreateSound(ColPos, m_SoundImpact,
				(m_Owner != -1) ? TeamMask : Mask128());
		}
		GameWorld()->DestroyEntity(this);
		return;
	}

	int x = GameServer()->Collision()->GetIndex(PrevPos, m_CurPos);
	int z;
	if (Config()->m_SvOldTeleportWeapons)
		z = GameServer()->Collision()->IsTeleport(x);
	else
		z = GameServer()->Collision()->IsTeleportWeapon(x);
	if (z && ((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts[z - 1].size())
	{
		int Num = ((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts[z - 1].size();
		m_Pos = ((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts[z - 1][(!Num) ? Num : rand() % Num];
		m_StartTick = Server()->Tick();
	}
}

void CProjectile::TickPaused()
{
	++m_StartTick;
}

void CProjectile::FillInfo(CNetObj_Projectile* pProj)
{
	pProj->m_Type = GameServer()->GetProjectileType(m_Type);

	// F-DDrace
	if (m_DefaultTuning)
	{
		pProj->m_X = round_to_int(m_Pos.x);
		pProj->m_Y = round_to_int(m_Pos.y);
		pProj->m_VelX = round_to_int(m_Direction.x*100.0f);
		pProj->m_VelY = round_to_int(m_Direction.y*100.0f);
		pProj->m_StartTick = m_StartTick;
	}
	else
	{
		if (!m_CalculatedVel)
			CalculateVel();

		pProj->m_X = round_to_int(m_LastResetPos.x);
		pProj->m_Y = round_to_int(m_LastResetPos.y);
		pProj->m_VelX = round_to_int(m_Vel.x);
		pProj->m_VelY = round_to_int(m_Vel.y);
		pProj->m_StartTick = m_LastResetTick;
	}
}

void CProjectile::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient, m_CurPos))
		return;

	CCharacter* pSnapChar = GameServer()->GetPlayerChar(SnappingClient);
	int Tick = (Server()->Tick() % Server()->TickSpeed()) % ((m_Explosive) ? 6 : 20);
	if (pSnapChar && pSnapChar->IsAlive() && (m_Layer == LAYER_SWITCH && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[pSnapChar->Team()] && (!Tick)))
		return;

	CCharacter* pOwnerChar = 0;
	Mask128 TeamMask = Mask128();

	if (m_Owner >= 0)
		pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

	if (pOwnerChar && pOwnerChar->IsAlive())
		TeamMask = pOwnerChar->Teams()->TeamMask(pOwnerChar->Team(), -1, m_Owner);

	if (m_Owner != -1 && !CmaskIsSet(TeamMask, SnappingClient))
		return;

	CNetObj_Projectile* pProj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
	if (!pProj)
		return;

	FillInfo(pProj);
}

// DDRace

void CProjectile::SetBouncing(int Value)
{
	m_Bouncing = Value;
}

void CProjectile::TickDefered()
{
	if (Server()->Tick() % 4 == 1)
	{
		m_LastResetPos = m_CurPos;
		m_LastResetTick = Server()->Tick();
	}
	m_CalculatedVel = false;
}

void CProjectile::CalculateVel()
{
	float Time = (Server()->Tick() - m_LastResetTick) / (float)Server()->TickSpeed();
	float Curvature;
	float Speed;
	GetOriginalTunings(&Curvature, &Speed);

	m_Vel.x = ((m_CurPos.x - m_LastResetPos.x) / Time / Speed) * 100;
	m_Vel.y = ((m_CurPos.y - m_LastResetPos.y) / Time / Speed - Time * Speed * Curvature / 10000) * 100;

	m_CalculatedVel = true;
}

void CProjectile::GetOriginalTunings(float *pCurvature, float *pSpeed, bool TuneZone)
{
	*pCurvature = 0;
	*pSpeed = 0;
	CTuningParams *pTuning = TuneZone && m_TuneZone ? &GameServer()->TuningList()[m_TuneZone] : GameServer()->Tuning();

	switch (GameServer()->GetProjectileType(m_Type))
	{
	case WEAPON_GRENADE:
		*pCurvature = pTuning->m_GrenadeCurvature;
		*pSpeed = pTuning->m_GrenadeSpeed;
		break;

	case WEAPON_SHOTGUN:
		*pCurvature = pTuning->m_ShotgunCurvature;
		*pSpeed = pTuning->m_ShotgunSpeed;
		break;

	case WEAPON_GUN:
		*pCurvature = pTuning->m_GunCurvature;
		*pSpeed = pTuning->m_GunSpeed;
		break;
	}
}

void CProjectile::GetTunings(float *pCurvature, float *pSpeed)
{
	*pCurvature = 0;
	*pSpeed = 0;
	CTuningParams *pTuning = m_TuneZone ? &GameServer()->TuningList()[m_TuneZone] : GameServer()->Tuning();

	if (m_Type == WEAPON_SHOTGUN && !m_DDrace)
	{
		*pCurvature = pTuning->m_VanillaShotgunCurvature;
		*pSpeed = pTuning->m_VanillaShotgunSpeed;
	}
	else if (m_Type == WEAPON_GUN && !m_DDrace)
	{
		*pCurvature = pTuning->m_VanillaGunCurvature;
		*pSpeed = pTuning->m_VanillaGunSpeed;
	}
	else if (m_Type == WEAPON_STRAIGHT_GRENADE)
	{
		*pCurvature = 0;
		*pSpeed = pTuning->m_StraightGrenadeSpeed;
	}
	else
	{
		GetOriginalTunings(pCurvature, pSpeed, m_TuneZone);
	}
}

bool CProjectile::IsDefaultTuning()
{
	if (!m_TuneZone && m_Type < NUM_VANILLA_WEAPONS)
		return true;

	CTuningParams *pProjTuning = &GameServer()->TuningList()[m_TuneZone];
	CTuningParams *pDefaultTuning = GameServer()->Tuning();
	switch (m_Type)
	{
	case WEAPON_GUN:
	case WEAPON_PROJECTILE_RIFLE:
		return pProjTuning->m_GunCurvature == pDefaultTuning->m_GunCurvature && pProjTuning->m_GunSpeed == pDefaultTuning->m_GunSpeed;
	case WEAPON_SHOTGUN:
		return pProjTuning->m_ShotgunCurvature == pDefaultTuning->m_ShotgunCurvature && pProjTuning->m_ShotgunSpeed == pDefaultTuning->m_ShotgunSpeed;
	case WEAPON_GRENADE:
		return pProjTuning->m_GrenadeCurvature == pDefaultTuning->m_GrenadeCurvature && pProjTuning->m_GrenadeSpeed == pDefaultTuning->m_GrenadeSpeed;
	case WEAPON_STRAIGHT_GRENADE:
		return false;
	}
	return true;
}
