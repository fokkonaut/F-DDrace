// made by fokkonaut

#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "pickup_drop.h"
#include "pickup.h"
#include <game/server/teams.h>
#include <engine/shared/config.h>
#include <game/server/gamemodes/DDRace.h>
#include "character.h"
#include <game/server/player.h>

CPickupDrop::CPickupDrop(CGameWorld *pGameWorld, vec2 Pos, int Type, int Owner, float Direction, int Lifetime, int Weapon, int Bullets, int Special)
: CAdvancedEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP_DROP, Pos, vec2(ms_PhysSize, ms_PhysSize), Owner)
{
	m_Type = Type;
	m_Weapon = Weapon;
	m_Lifetime = Server()->TickSpeed() * Lifetime;
	m_Special = Special;
	m_Bullets = Bullets;
	m_Vel = vec2(5*Direction, -5);
	m_PickupDelay = Server()->TickSpeed() * (Config()->m_SvDropsPickupDelay / 1000.f);
	m_DDraceMode = GameServer()->m_apPlayers[Owner]->m_Gamemode == GAMEMODE_DDRACE;

	m_Snap.m_Pos = m_Pos;
	m_Snap.m_Time = 0.f;
	m_Snap.m_LastTime = Server()->Tick();

	for (int i = 0; i < 4; i++)
		m_aID[i] = Server()->SnapNewID();
	GameWorld()->InsertEntity(this);
}

CPickupDrop::~CPickupDrop()
{
	for (int i = 0; i < 4; i++)
		Server()->SnapFreeID(m_aID[i]);
}

void CPickupDrop::Reset(bool Picked)
{
	if (m_Type == POWERUP_WEAPON || m_Type == POWERUP_BATTERY)
	{
		CPlayer *pOwner = GameServer()->m_apPlayers[m_Owner];
		if (m_Owner >= 0 && pOwner)
			for (unsigned i = 0; i < pOwner->m_vWeaponLimit[m_Weapon].size(); i++)
				if (pOwner->m_vWeaponLimit[m_Weapon][i] == this)
					pOwner->m_vWeaponLimit[m_Weapon].erase(pOwner->m_vWeaponLimit[m_Weapon].begin() + i);
	}
	else
	{
		for (unsigned i = 0; i < GameServer()->m_vPickupDropLimit.size(); i++)
			if (GameServer()->m_vPickupDropLimit[i] == this)
				GameServer()->m_vPickupDropLimit.erase(GameServer()->m_vPickupDropLimit.begin() + i);
	}

	if (!Picked)
		GameServer()->CreateDeath(m_Pos, m_Owner, m_TeamMask);

	CAdvancedEntity::Reset();
}

void CPickupDrop::Tick()
{
	// check this before the CAdvancedEntity tick to not lose our m_Owner for this check
	if (m_Owner >= 0 && !GameServer()->m_apPlayers[m_Owner] && Config()->m_SvDestroyDropsOnLeave)
	{
		Reset();
		return;
	}

	CAdvancedEntity::Tick();

	m_Lifetime--;
	if (m_Lifetime <= 0)
	{
		Reset();
		return;
	}

	if (m_PickupDelay > 0)
		m_PickupDelay--;

	Pickup();
	IsShieldNear();
	HandleDropped();

	m_PrevPos = m_Pos;
}

void CPickupDrop::Pickup()
{
	int ID = IsCharacterNear();
	if (ID != -1)
	{
		CCharacter* pChr = GameServer()->GetPlayerChar(ID);

		if (m_Type == POWERUP_WEAPON)
		{
			// only give the weapon if its not an extra
			if (m_Special == 0)
			{
				if (pChr->GetPlayer()->m_Gamemode == GAMEMODE_VANILLA && m_Bullets == -1
					&& m_Weapon != WEAPON_HAMMER && m_Weapon != WEAPON_TELEKINESIS && m_Weapon != WEAPON_LIGHTSABER)
					m_Bullets = 10;

				pChr->GiveWeapon(m_Weapon, false, m_Bullets);
				pChr->WeaponMoneyReward(m_Weapon);
			}

			if (m_Special&SPECIAL_JETPACK)
				pChr->Jetpack();
			if (m_Special&SPECIAL_SPREADWEAPON)
				pChr->SpreadWeapon(m_Weapon);
			if (m_Special&SPECIAL_TELEWEAPON)
				pChr->TeleWeapon(m_Weapon);
			if (m_Special&SPECIAL_DOORHAMMER)
				pChr->DoorHammer();
			if (m_Special&SPECIAL_SCROLLNINJA)
				pChr->ScrollNinja();

			GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), m_Weapon);

			if (m_Weapon == WEAPON_SHOTGUN || m_Weapon == WEAPON_LASER || m_Weapon == WEAPON_TASER || m_Weapon == WEAPON_PLASMA_RIFLE || m_Weapon == WEAPON_PORTAL_RIFLE || m_Weapon == WEAPON_PROJECTILE_RIFLE || m_Weapon == WEAPON_TELE_RIFLE || m_Weapon == WEAPON_LIGHTNING_LASER)
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, pChr->TeamMask());
			else if (m_Weapon == WEAPON_GRENADE || m_Weapon == WEAPON_STRAIGHT_GRENADE || m_Weapon == WEAPON_BALL_GRENADE)
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE, pChr->TeamMask());
			else if (m_Weapon == WEAPON_HAMMER || m_Weapon == WEAPON_GUN || m_Weapon == WEAPON_HEART_GUN)
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());
			else if (m_Weapon == WEAPON_TELEKINESIS)
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA, pChr->TeamMask());
			else if (m_Weapon == WEAPON_LIGHTSABER)
				GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, pChr->TeamMask());
		}
		else if (m_Type == POWERUP_BATTERY)
			GameServer()->CreateSound(m_Pos, SOUND_HOOK_LOOP, pChr->TeamMask());
		else if (m_Type == POWERUP_HEALTH)
			GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, pChr->TeamMask());
		else if (m_Type == POWERUP_ARMOR)
			GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());

		Reset(true);
	}
}

int CPickupDrop::IsCharacterNear()
{
	CCharacter *apEnts[MAX_CLIENTS];
	int Num = GameWorld()->FindEntities(m_Pos, 20.0f, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER, m_DDTeam);

	for (int i = 0; i < Num; i++)
	{
		CCharacter* pChr = apEnts[i];

		if (m_PickupDelay > 0 && pChr == GetOwner())
			continue;

		if (m_Type == POWERUP_WEAPON)
		{
			int ChrSpecial = pChr->GetWeaponSpecial(m_Weapon);
			bool AcceptSpecial = true;

			if (
				(m_Special&SPECIAL_JETPACK && (ChrSpecial&SPECIAL_JETPACK || !pChr->GetWeaponGot(WEAPON_GUN)))
				|| (m_Special&SPECIAL_SPREADWEAPON && (ChrSpecial&SPECIAL_SPREADWEAPON || !pChr->GetWeaponGot(m_Weapon)))
				|| (m_Special&SPECIAL_TELEWEAPON && (ChrSpecial& SPECIAL_TELEWEAPON || !pChr->GetWeaponGot(m_Weapon)))
				|| (m_Special&SPECIAL_DOORHAMMER && (ChrSpecial&SPECIAL_DOORHAMMER || !pChr->GetWeaponGot(WEAPON_HAMMER)))
				|| (m_Special&SPECIAL_SCROLLNINJA && ChrSpecial&SPECIAL_SCROLLNINJA)
				)
				AcceptSpecial = false;

			if (
				(pChr->GetPlayer()->m_SpookyGhost && GameServer()->GetWeaponType(m_Weapon) != WEAPON_GUN)
				|| (pChr->m_IsZombie && m_Weapon != WEAPON_HAMMER)
				|| (m_Weapon == WEAPON_TASER && GameServer()->m_Accounts[pChr->GetPlayer()->GetAccID()].m_TaserLevel < 1)
				|| (pChr->GetWeaponGot(m_Weapon) && m_Special == 0 && (pChr->GetWeaponAmmo(m_Weapon) == -1 || (pChr->GetWeaponAmmo(m_Weapon) >= m_Bullets && m_Bullets >= 0)))
				|| (m_Special != 0 && !AcceptSpecial)
				)
				continue;
		}
		else if (m_Type == POWERUP_BATTERY && m_Weapon == WEAPON_TASER && !pChr->GetPlayer()->GiveTaserBattery(m_Bullets))
			continue;
		else if (m_Type == POWERUP_BATTERY && m_Weapon == WEAPON_PORTAL_RIFLE && !pChr->GetPlayer()->GivePortalBattery(m_Bullets))
			continue;
		else if (m_Type == POWERUP_HEALTH && !pChr->IncreaseHealth(1))
			continue;
		else if (m_Type == POWERUP_ARMOR && !pChr->IncreaseArmor(1))
			continue;

		return pChr->GetPlayer()->GetCID();
	}

	return -1;
}

void CPickupDrop::IsShieldNear()
{
	if (!m_DDraceMode || (m_Type != POWERUP_WEAPON && m_Type != POWERUP_BATTERY))
		return;

	CPickup *apEnts[9];
	int Num = GameWorld()->FindEntities(m_Pos, 20.0f, (CEntity**)apEnts, 9, CGameWorld::ENTTYPE_PICKUP, m_DDTeam);

	for (int i = 0; i < Num; i++)
	{
		if (apEnts[i]->GetType() == POWERUP_ARMOR && apEnts[i]->GetOwner() < 0 && apEnts[i]->m_BrushCID == -1)
		{
			GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, m_TeamMask);
			Reset();
			break;
		}
	}
}

void CPickupDrop::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	CCharacter *pSnapChar = GameServer()->GetPlayerChar(SnappingClient);
	if (pSnapChar && !CmaskIsSet(m_TeamMask, SnappingClient))
		return;

	vec2 SnapPos = m_Pos;
	if (m_Type == POWERUP_BATTERY)
	{
		m_Snap.m_Time += (Server()->Tick() - m_Snap.m_LastTime) / Server()->TickSpeed();
		m_Snap.m_LastTime = Server()->Tick();

		float Offset = m_Snap.m_Pos.y / 32.0f + m_Snap.m_Pos.x / 32.0f;
		m_Snap.m_Pos.x = m_Pos.x + cosf(m_Snap.m_Time * 2.0f + Offset) * 2.5f;
		m_Snap.m_Pos.y = m_Pos.y + sinf(m_Snap.m_Time * 2.0f + Offset) * 2.5f;

		SnapPos = m_Snap.m_Pos;
	}

	int PickupFlags = 0;
	if ((m_PickupDelay > 0 && SnappingClient == m_Owner) || !m_DDraceMode || (pSnapChar && pSnapChar->m_IsZombie && m_Weapon != WEAPON_HAMMER))
		PickupFlags = PICKUPFLAG_NO_PREDICT;

	int SnappingClientVersion = GameServer()->GetClientDDNetVersion(SnappingClient);
	GameServer()->SnapPickup(CSnapContext(SnappingClientVersion, Server()->IsSevendown(SnappingClient), SnappingClient), GetID(),
		SnapPos, m_Type, m_Weapon, m_Number, PickupFlags, m_Special, m_aID);
}
