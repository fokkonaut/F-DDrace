//
// Created by Matq on 14/04/2025.
//

#include "game/server/gamemodes/DDRace.h"
#include "missile.h"

CSpark::CSpark(CMissile *pMissile, int ResetLifespan, int Lifespan)
{
	m_pMissile = pMissile;
	m_ResetLifespan = ResetLifespan;
	m_ID = Server()->SnapNewID();

	ResetFromMissile();

	m_Lifespan = Lifespan;
}

CSpark::~CSpark()
{
	Server()->SnapFreeID(m_ID);
}

IServer *CSpark::Server()
{
	return m_pMissile->Server();
}

void CSpark::ResetFromMissile()
{
	vec2 Direction = m_pMissile->GetVel();
	vec2 Perpendicular = normalize(vec2(-Direction.y, Direction.x));
	m_Pos = m_pMissile->GetPos() + vec2((float)(rand() % 21 - 10), (float)(rand() % 21 - 10));
	m_Vel = -Direction * ((float)(25 + rand() % 35) / 100.f) + Perpendicular * (float)(rand() % 101 - 50) / 15.f;
}

void CSpark::Tick()
{
	if (!m_pMissile->IsIgnited())
		return;

	// Only make `new` spark when old one has died and the missile is flying(not exploding)
	if (m_Lifespan <= 0 && !m_pMissile->IsExploding())
	{
		ResetFromMissile();
		m_Lifespan = m_ResetLifespan;
	}

	m_Pos += m_Vel;
	m_Lifespan--;
}

void CSpark::Snap(int SnappingClient)
{
	if (!IsStillVisible())
		return;

	CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID, sizeof(CNetObj_Projectile)));
	if (!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_VelX = 0;
	pObj->m_VelY = 0;
	pObj->m_StartTick = 0;
	pObj->m_Type = WEAPON_HAMMER;
}

void CMissile::ApplyAcceleration()
{
	if (m_ExplosionsLeft >= 0)
		return;

	m_IgnitionTime--;
	if (m_IgnitionTime <= 0)
	{
		// Accelerates quickly, maximum speed 50.f
		float Speed = length(m_Vel);
		if (Speed <= 49.99f)
		{
			m_Vel = normalize(m_Vel + m_Direction) * Speed * 1.03f;
			if (length(m_Vel) > 50.f)
				m_Vel = normalize(m_Vel) * 50.f;
		}
	}

	m_Pos += m_Vel;
}

void CMissile::HandleCollisions()
{
	if (m_ExplosionsLeft >= 0)
		return;

	// Yeaaaaaaaaaaaaaaaaaaa
	vec2 collisionPos, newPos;
	int Collide = GameServer()->Collision()->IntersectLine(m_PrevPos, m_Pos, &collisionPos, &newPos);

	CCharacter *pOwnerChar = nullptr;
	if (m_Owner >= 0)
		pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

	CCharacter *pTargetChr = nullptr;
	if (pOwnerChar ? !(pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_GRENADE) : Config()->m_SvHit)
		pTargetChr = GameWorld()->IntersectCharacter(m_PrevPos, collisionPos, 6.0f, collisionPos, pOwnerChar, m_Owner);

	if (m_LifeSpan > -1)
		m_LifeSpan--;

	bool IsWeaponCollide = false;
	if (pOwnerChar && pTargetChr &&
		pOwnerChar->IsAlive() &&
		pTargetChr->IsAlive() &&
		!pTargetChr->CanCollide(m_Owner))
		IsWeaponCollide = true;

	if ((pTargetChr || Collide || GameLayerClipped(m_Pos)) && !IsWeaponCollide)
	{
		m_Pos = collisionPos;
		TriggerExplosions();
		return;
	}

	if (m_LifeSpan == -1)
	{
		TriggerExplosions();
		return;
	}

	int x = GameServer()->Collision()->GetIndex(m_PrevPos, m_Pos);
	int z;
	if (Config()->m_SvOldTeleportWeapons)
		z = GameServer()->Collision()->IsTeleport(x);
	else
		z = GameServer()->Collision()->IsTeleportWeapon(x);
	if (z && !((CGameControllerDDRace *)GameServer()->m_pController)->m_TeleOuts[z - 1].empty())
	{
		int Num = (int)((CGameControllerDDRace *)GameServer()->m_pController)->m_TeleOuts[z - 1].size();
		m_Pos = ((CGameControllerDDRace *)GameServer()->m_pController)->m_TeleOuts[z - 1][(!Num) ? Num : rand() % Num];
		m_StartTick = Server()->Tick();
	}
}

void CMissile::UpdateStableProjectiles()
{
	// When ignition started, teleport/reset smoke trail to the missile
	if (m_IgnitionTime == 0)
		for (int i = 0; i < NUM_SPARKS; i++)
			m_apSparks[i]->ResetFromMissile();

	for (int i = 1; i < NUM_SPARKS; i++)
		m_apSparks[i]->Tick();

	if (m_ExplosionsLeft >= 0)
		return;

	m_pStableRocket->SetPos(m_Pos);
}

void CMissile::TriggerExplosions()
{
	m_ExplosionsLeft = 5;

	GameWorld()->DestroyEntity(m_pStableRocket);
}

void CMissile::HandleExplosions()
{
	if (m_ExplosionsLeft < 0 || Server()->Tick() % 4 != 0)
		return;

	if (IsIgnited())
	{
		vec2 nearbyPos = m_Pos + vec2((float)(rand() % 151 - 75), (float)(rand() % 151 - 75));
		GameServer()->CreateExplosion(nearbyPos, m_Owner, WEAPON_GRENADE, m_Owner == -1, m_DDTeam, m_TeamMask);
		GameServer()->CreateSound(nearbyPos, SOUND_GRENADE_EXPLODE, m_TeamMask);
	}
	else
	{
		// Explode only once
		m_ExplosionsLeft = 0;
		GameServer()->CreateExplosion(m_Pos, m_Owner, WEAPON_GRENADE, m_Owner == -1, m_DDTeam, m_TeamMask);
		GameServer()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE, m_TeamMask);
	}

	if (m_ExplosionsLeft == 0)
		GameWorld()->DestroyEntity(this);

	m_ExplosionsLeft--;
}

CMissile::CMissile(CGameWorld *pGameWorld, int Owner, vec2 Pos, vec2 Vel, vec2 Direction, int Span)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_MISSILE, Pos)
{
	m_Owner = Owner;
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	m_DDTeam = pOwnerChar ? pOwnerChar->Team() : 0;
	m_TeamMask = ((CGameControllerDDRace*)GameServer()->m_pController)->m_Teams.TeamMask(m_DDTeam);
	m_LifeSpan = Span;
	m_StartTick = Server()->Tick();

	m_IgnitionTime = 20;
	m_PrevPos = Pos;
	m_Vel = Vel;
	m_Direction = Direction;

	m_pStableRocket = new CStableProjectile(pGameWorld, WEAPON_GRENADE, Owner, Pos, false, false);
	for (int i = 0; i < NUM_SPARKS; i++) // Lifespan = sparks : one spark per tick
		m_apSparks[i] = new CSpark(this, NUM_SPARKS, i);

	m_ExplosionsLeft = -1;

	GameWorld()->InsertEntity(this);
}

CMissile::~CMissile()
{
	for (int i = 0; i < NUM_SPARKS; i++)
		delete m_apSparks[i];
}

void CMissile::Tick()
{
	m_TeamMask = ((CGameControllerDDRace*)GameServer()->m_pController)->m_Teams.TeamMask(m_DDTeam);

	ApplyAcceleration();
	HandleCollisions();

	UpdateStableProjectiles();

	// Entity destroyed at m_ExplosionsLeft == 0
	HandleExplosions();

	m_PrevPos = m_Pos;
}

void CMissile::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient, m_Pos) || !IsIgnited())
		return;

	if (!CmaskIsSet(m_TeamMask, SnappingClient))
		return;

	// Smoke trail
	for (int i = 0; i < NUM_SPARKS; i++)
		m_apSparks[i]->Snap(SnappingClient);
}
