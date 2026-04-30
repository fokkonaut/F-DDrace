// made by fokkonaut

#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include "custom_projectile.h"
#include <game/server/gamemodes/DDRace.h>
#include <engine/shared/config.h>
#include <game/server/entities/character.h>
#include <generated/server_data.h>

CCustomProjectile::CCustomProjectile(CGameWorld *pGameWorld, int Owner, vec2 Pos, vec2 Dir, bool Freeze,
		bool Explosive, bool Unfreeze, bool Bloody, bool Ghost, bool Spooky, int Type, float Lifetime, float Accel, float Speed)
		: CEntity(pGameWorld, CGameWorld::ENTTYPE_CUSTOM_PROJECTILE, Pos)
{
	m_Owner = Owner;
	m_Pos = Pos;
	m_Speed = Speed;
	m_Direction = Dir;
	m_Core = normalize(m_Direction) * m_Speed;
	m_Freeze = Freeze;
	m_Explosive = Explosive;
	m_Unfreeze = Unfreeze;
	m_Bloody = Bloody;
	m_Ghost = Ghost;
	m_Spooky = Spooky;
	m_EvalTick = Server()->Tick();
	m_LifeTime = Server()->TickSpeed() * Lifetime;
	m_InitialLifeTime = m_LifeTime;
	m_Type = Type;
	m_Accel = Accel;

	m_pHammerHitChr = 0;
	m_PrevPos = m_Pos;

	GameWorld()->InsertEntity(this);
}

void CCustomProjectile::Reset()
{
	GameWorld()->DestroyEntity(this);
}

void CCustomProjectile::HitProjectile(CCharacter *pFrom, vec2 Direction)
{
	// allow hitting owner when someone redirected the projectile.
	// Make it behave as before when owner redirected again
	m_pHammerHitChr = pFrom;
	m_Direction = Direction;
	m_Core = normalize(m_Direction) * m_Speed;
	m_EvalTick = Server()->Tick();
	if (Config()->m_SvResetProjLifetimeAfterHit)
	{
		m_InitialLifeTime *= 0.95f; // dont keep it around forever
		m_LifeTime = m_InitialLifeTime;
	}
}

int CCustomProjectile::DDTeam()
{
	return m_pOwner ? m_pOwner->Team() : -1;
}

void CCustomProjectile::Tick()
{
	m_pOwner = 0;
	if (GameServer()->GetPlayerChar(m_Owner))
		m_pOwner = GameServer()->GetPlayerChar(m_Owner);

	if (m_Owner >= 0 && !m_pOwner && Config()->m_SvDestroyBulletsOnDeath)
	{
		Reset();
		return;
	}

	m_TeamMask = m_pOwner ? m_pOwner->TeamMask() : Mask128();

	m_LifeTime--;
	if (m_LifeTime <= 0)
	{
		Reset();
		return;
	}

	Move();
	HitCharacter();

	if (GameServer()->Collision()->IsSolid(m_Pos.x, m_Pos.y))
	{
		if (m_Explosive)
		{
			GameServer()->CreateExplosion(m_Pos, m_Owner, m_Type, m_Owner == -1, m_pOwner ? m_pOwner->Team() : -1, m_TeamMask);
			GameServer()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE, m_TeamMask);
		}

		if (m_Bloody)
		{
			if (m_Ghost && m_CollisionState == COLLIDED_ONCE)
			{
				if (Server()->Tick() % 10 == 0)
					GameServer()->CreateDeath(m_PrevPos, m_Owner, m_TeamMask);
			}
			else
				GameServer()->CreateDeath(m_PrevPos, m_Owner, m_TeamMask);
		}

		if (m_CollisionState == NOT_COLLIDED)
			m_CollisionState = COLLIDED_ONCE;

		if (m_CollisionState == COLLIDED_TWICE || !m_Ghost)
		{
			Reset();
			return;
		}
	}
	else if(m_CollisionState == COLLIDED_ONCE)
		m_CollisionState = COLLIDED_TWICE;

	// weapon teleport
	int x = GameServer()->Collision()->GetIndex(m_PrevPos, m_Pos);
	int z;
	if (Config()->m_SvOldTeleportWeapons)
		z = GameServer()->Collision()->IsTeleport(x);
	else
		z = GameServer()->Collision()->IsTeleportWeapon(x);
	if (z && ((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts[z - 1].size())
	{
		int Num = ((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts[z - 1].size();
		m_Pos = ((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts[z - 1][(!Num) ? Num : rand() % Num];
		m_EvalTick = Server()->Tick();
	}

	m_PrevPos = m_Pos;
}

void CCustomProjectile::Move()
{
	m_Pos += m_Core;
	m_Core *= m_Accel;
}

void CCustomProjectile::HitCharacter()
{
	vec2 NewPos = m_Pos + m_Core;
	int CollideWith = m_Owner;
	CCharacter *pNotThis = m_pOwner;
	if (m_pHammerHitChr && m_pHammerHitChr->IsAlive())
	{
		pNotThis = m_pHammerHitChr;
		CollideWith = m_pHammerHitChr->GetPlayer()->GetCID();
	}
	CCharacter* pHit = GameWorld()->IntersectCharacter(m_PrevPos, NewPos, 6.0f, NewPos, pNotThis, CollideWith);
	if (!pHit)
		return;

	if (m_Bloody)
		GameServer()->CreateDeath(pHit->GetPos(), pHit->GetPlayer()->GetCID(), m_TeamMask);

	if (m_Freeze)
		pHit->Freeze();
	else if (m_Unfreeze)
		pHit->UnFreeze();

	if (m_Explosive)
	{
		GameServer()->CreateExplosion(m_Pos, m_Owner, m_Type, m_Owner == -1, pHit->Team(), m_TeamMask);
		GameServer()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE, m_TeamMask);
	}
	else
		pHit->TakeDamage(vec2(0, 0), vec2(0, 0), g_pData->m_Weapons.m_aId[GameServer()->GetWeaponType(m_Type)].m_Damage, m_Owner, m_Type);

	if (m_Type == WEAPON_HEART_GUN)
	{
		pHit->SetEmote(EMOTE_HAPPY, Server()->Tick() + 2 * Server()->TickSpeed());
		GameServer()->SendEmoticon(pHit->GetPlayer()->GetCID(), EMOTICON_HEARTS);
	}
	else if (m_Spooky)
	{
		pHit->SetEmote(EMOTE_SURPRISE, Server()->Tick() + 2 * Server()->TickSpeed());
		GameServer()->SendEmoticon(pHit->GetPlayer()->GetCID(), EMOTICON_GHOST);
	}

	Reset();
}

void CCustomProjectile::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	if (!CmaskIsSet(m_TeamMask, SnappingClient))
		return;

	int SnappingClientVersion = GameServer()->GetClientDDNetVersion(SnappingClient);
	CSnapContext Context(SnappingClientVersion, Server()->IsSevendown(SnappingClient), SnappingClient);
	if (m_Type == WEAPON_PLASMA_RIFLE || m_Type == WEAPON_GUN)
	{
		GameServer()->SnapLaserObject(Context, GetID(), m_Pos, m_Pos, m_EvalTick, m_Owner, LASERTYPE_RIFLE, -1, -1, LASERFLAG_NO_PREDICT);
	}
	else if (m_Type == WEAPON_HEART_GUN)
	{
		GameServer()->SnapPickupObject(Context, GetID(), m_Pos, POWERUP_HEALTH, 0, -1, PICKUPFLAG_NO_PREDICT);
	}
}
