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

	// F-DDrace
	m_Spooky = Spooky;

	// activate faked tuning for tunezones, vanilla shotgun and gun, straightgrenade
	CPlayer *pOwner = m_Owner >= 0 ? GameServer()->m_apPlayers[m_Owner] : 0;
	m_DDrace = !pOwner || pOwner->m_Gamemode == GAMEMODE_DDRACE || (m_Type != WEAPON_GUN && m_Type != WEAPON_SHOTGUN);
	DetermineTuning();
	m_DefaultTuning = IsDefaultTuning() && m_DDrace;

	m_TeamMask = Mask128();
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
	return CalcPos(m_Pos, m_Direction, m_Curvature, m_Speed, Time);
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
	CCharacter *pOwnerChar = m_Owner >= 0 ? GameServer()->GetPlayerChar(m_Owner) : 0;
	CCharacter *pTargetChr = 0;
	CAdvancedEntity *pTargetEntity = 0;
	if (pOwnerChar ? !(pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_GRENADE) : Config()->m_SvHit)
	{
		int Types = (1<<CGameWorld::ENTTYPE_CHARACTER);
		if (Config()->m_SvInteractiveDrops)
		{
			Types |= (1<<CGameWorld::ENTTYPE_FLAG) | (1<<CGameWorld::ENTTYPE_PICKUP_DROP) | (1<<CGameWorld::ENTTYPE_MONEY) | (1<<CGameWorld::ENTTYPE_HELICOPTER) | (1<<CGameWorld::ENTTYPE_GROG);
		}
		CEntity *pNotThis = pOwnerChar && pOwnerChar->m_pHelicopter ? (CEntity *)pOwnerChar->m_pHelicopter : (CEntity *)pOwnerChar;
		CEntity *pEnt = GameWorld()->IntersectEntityTypes(PrevPos, ColPos, m_Freeze ? 1.0f : 6.0f, ColPos, pNotThis, m_Owner, Types);
		if (pEnt)
		{
			if (pEnt->GetObjType() == CGameWorld::ENTTYPE_CHARACTER)
			{
				pTargetChr = (CCharacter *)pEnt;
			}
			else if (pEnt->IsAdvancedEntity())
			{
				pTargetEntity = (CHelicopter *)pEnt;
				pTargetChr = pTargetEntity->GetOwner();
			}
		}
	}

	if (m_LifeSpan > -1)
		m_LifeSpan--;

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
	m_TeamMask = Mask128();
	if (pOwnerChar && pOwnerChar->IsAlive())
	{
		m_TeamMask = pOwnerChar->TeamMask();
	}
	else if (m_Owner >= 0 && (GameServer()->GetProjectileType(m_Type) != WEAPON_GRENADE || Config()->m_SvDestroyBulletsOnDeath || GameServer()->Arenas()->FightStarted(m_Owner)))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	if (!IsWeaponCollide && (Collide || GameLayerClipped(m_CurPos) || (pTargetEntity && !pTargetChr) ||
		(pTargetChr && (m_Owner == -1 || pTargetChr == pOwnerChar || (pOwnerChar ? !(pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_GRENADE) : Config()->m_SvHit)))
		))
	{
		if (m_Explosive/*??*/ && (!pTargetChr || (pTargetChr && (!m_Freeze || (m_Type == WEAPON_SHOTGUN && Collide)))))
		{
			int ActivatedTeam = pTargetChr ? pTargetChr->Team() : (pTargetEntity ? pTargetEntity->GetDDTeam() : -1);
			GameServer()->CreateExplosion(ColPos, m_Owner, m_Type, m_Owner == -1, ActivatedTeam, m_TeamMask);
			GameServer()->CreateSound(ColPos, m_SoundImpact, m_TeamMask);
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
		if (pTargetEntity)
		{
			// Don't trigger a double explosion. Explosive is checked above.
			/*if (m_Explosive)
			{
				GameServer()->CreateExplosion(ColPos, m_Owner, m_Type, m_Owner == -1, pTargetEntity->GetDDTeam(), m_TeamMask);
				GameServer()->CreateSound(ColPos, m_SoundImpact, m_TeamMask);
			}*/
		}
		else if (pTargetChr)
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
				GameServer()->CreateDamage(m_CurPos, m_Owner, m_Direction, 1, 0, (pTargetChr && m_Owner == pTargetChr->GetPlayer()->GetCID()), m_TeamMask, 10);
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
			GameServer()->CreateExplosion(ColPos, m_Owner, m_Type, m_Owner == -1, (!pOwnerChar ? -1 : pOwnerChar->Team()), m_TeamMask);
			GameServer()->CreateSound(ColPos, m_SoundImpact, m_TeamMask);
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

	if(m_LifeSpan == -2)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(SnappingClient);
		if (pChr && pChr->SendExtendedEntity(this))
		{
			CNetObj_EntityEx *pEntData = static_cast<CNetObj_EntityEx *>(Server()->SnapNewItem(NETOBJTYPE_ENTITYEX, GetID(), sizeof(CNetObj_EntityEx)));
			if(!pEntData)
				return;

			pEntData->m_SwitchNumber = m_Number;
			pEntData->m_Layer = m_Layer;
			pEntData->m_EntityClass = ENTITYCLASS_PROJECTILE;
		}
	}

	if (SnappingClient != -1 && GameServer()->GetClientDDNetVersion(SnappingClient) < VERSION_DDNET_SWITCH)
	{
		CCharacter* pSnapChar = GameServer()->GetPlayerChar(SnappingClient);
		int Tick = (Server()->Tick() % Server()->TickSpeed()) % ((m_Explosive) ? 6 : 20);
		if (pSnapChar && pSnapChar->IsAlive() && (m_Layer == LAYER_SWITCH && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[pSnapChar->Team()] && (!Tick)))
			return;
	}

	if (!CmaskIsSet(m_TeamMask, SnappingClient))
		return;

	CNetObj_DDNetProjectile DDNetProjectile;
	if(SnappingClient != -1 && GameServer()->GetClientDDNetVersion(SnappingClient) >= VERSION_DDNET_PROJECTILE && FillExtraInfo(&DDNetProjectile, SnappingClient))
	{
		CNetObj_DDNetProjectile *pProj = static_cast<CNetObj_DDNetProjectile *>(Server()->SnapNewItem(NETOBJTYPE_DDNETPROJECTILE, GetID(), sizeof(CNetObj_DDNetProjectile)));
		if(!pProj)
			return;
		mem_copy(pProj, &DDNetProjectile, sizeof(DDNetProjectile));
	}
	else
	{
		CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
		if(!pProj)
			return;
		FillInfo(pProj);
	}
}

// DDRace

void CProjectile::SetBouncing(int Value)
{
	m_Bouncing = Value;
}

bool CProjectile::FillExtraInfo(CNetObj_DDNetProjectile *pProj, int SnappingClient)
{
	if (!m_DefaultTuning)
		return false;

	const int MaxPos = 0x7fffffff / 100;
	if(abs((int)m_Pos.y) + 1 >= MaxPos || abs((int)m_Pos.x) + 1 >= MaxPos)
	{
		//If the modified data would be too large to fit in an integer, send normal data instead
		return false;
	}
	//Send additional/modified info, by modifiying the fields of the netobj
	float Angle = -atan2f(m_Direction.x, m_Direction.y);

	int Owner = m_Owner;
	if (!Server()->Translate(Owner, SnappingClient))
		Owner = -1;

	int Data = 0;
	Data |= (abs(Owner) & 255) << 0;
	if(Owner < 0)
		Data |= PROJECTILEFLAG_NO_OWNER;
	//This bit tells the client to use the extra info
	Data |= PROJECTILEFLAG_IS_DDNET;
	// PROJECTILEFLAG_BOUNCE_HORIZONTAL, PROJECTILEFLAG_BOUNCE_VERTICAL
	Data |= (m_Bouncing & 3) << 10;
	if(m_Explosive)
		Data |= PROJECTILEFLAG_EXPLOSIVE;
	if(m_Freeze)
		Data |= PROJECTILEFLAG_FREEZE;

	pProj->m_X = (int)(m_Pos.x * 100.0f);
	pProj->m_Y = (int)(m_Pos.y * 100.0f);
	pProj->m_Angle = (int)(Angle * 1000000.0f);
	pProj->m_Data = Data;
	pProj->m_StartTick = m_StartTick;
	pProj->m_Type = GameServer()->GetProjectileType(m_Type);
	return true;
}

void CProjectile::TickDeferred()
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

void CProjectile::GetOriginalTunings(float *pCurvature, float *pSpeed)
{
	*pCurvature = 0;
	*pSpeed = 0;
	CTuningParams *pTuning = GameServer()->Tuning();
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

void CProjectile::DetermineTuning()
{
	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Pos));
	CTuningParams *pTuning = GameServer()->TuningFromChrOrZone(m_Owner, m_TuneZone);
	if (m_Type == WEAPON_SHOTGUN && !m_DDrace)
	{
		m_Curvature = pTuning->m_VanillaShotgunCurvature;
		m_Speed = pTuning->m_VanillaShotgunSpeed;
	}
	else if (m_Type == WEAPON_GUN && !m_DDrace)
	{
		m_Curvature = pTuning->m_VanillaGunCurvature;
		m_Speed = pTuning->m_VanillaGunSpeed;
	}
	else if (m_Type == WEAPON_STRAIGHT_GRENADE)
	{
		m_Curvature = 0;
		m_Speed = pTuning->m_StraightGrenadeSpeed;
	}
	else
	{
		switch (GameServer()->GetProjectileType(m_Type))
		{
		case WEAPON_GRENADE:
			m_Curvature = pTuning->m_GrenadeCurvature;
			m_Speed = pTuning->m_GrenadeSpeed;
			break;

		case WEAPON_SHOTGUN:
			m_Curvature = pTuning->m_ShotgunCurvature;
			m_Speed = pTuning->m_ShotgunSpeed;
			break;

		case WEAPON_GUN:
			m_Curvature = pTuning->m_GunCurvature;
			m_Speed = pTuning->m_GunSpeed;
			break;
		}
	}
}

bool CProjectile::IsDefaultTuning()
{
	CTuningParams *pDefaultTuning = GameServer()->Tuning();
	switch (m_Type)
	{
	case WEAPON_GUN:
	case WEAPON_PROJECTILE_RIFLE:
		return m_Curvature == pDefaultTuning->m_GunCurvature && m_Speed == pDefaultTuning->m_GunSpeed;
	case WEAPON_SHOTGUN:
		return m_Curvature == pDefaultTuning->m_ShotgunCurvature && m_Speed == pDefaultTuning->m_ShotgunSpeed;
	case WEAPON_GRENADE:
		return m_Curvature == pDefaultTuning->m_GrenadeCurvature && m_Speed == pDefaultTuning->m_GrenadeSpeed;
	case WEAPON_STRAIGHT_GRENADE:
		return false;
	}
	return true;
}
