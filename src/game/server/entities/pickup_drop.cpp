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
: CAdvancedEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP_DROP, Pos, ms_PhysSize, Owner)
{
	m_Type = Type;
	m_Weapon = Weapon;
	m_Lifetime = Server()->TickSpeed() * Lifetime;
	m_Special = Special;
	m_Bullets = Bullets;
	m_Vel = vec2(5*Direction, -5);
	m_PickupDelay = Server()->TickSpeed() * 2;
	m_SnapPos = m_Pos;
	m_DDraceMode = GameServer()->m_apPlayers[Owner]->m_Gamemode == GAMEMODE_DDRACE;

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
		GameServer()->CreateDeath(m_Pos, m_Owner);

	GameServer()->UnsetTelekinesis(this);
	GameWorld()->DestroyEntity(this);
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

	m_TeamMask = GetOwner() ? GetOwner()->Teams()->TeamMask(GetOwner()->Team(), -1, m_Owner) : Mask128();

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

			if (m_Weapon == WEAPON_SHOTGUN || m_Weapon == WEAPON_LASER || m_Weapon == WEAPON_PLASMA_RIFLE || m_Weapon == WEAPON_PORTAL_RIFLE || m_Weapon == WEAPON_PROJECTILE_RIFLE)
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, pChr->Teams()->TeamMask(pChr->Team(), -1, ID));
			else if (m_Weapon == WEAPON_GRENADE || m_Weapon == WEAPON_STRAIGHT_GRENADE || m_Weapon == WEAPON_BALL_GRENADE)
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE, pChr->Teams()->TeamMask(pChr->Team(), -1, ID));
			else if (m_Weapon == WEAPON_HAMMER || m_Weapon == WEAPON_GUN || m_Weapon == WEAPON_HEART_GUN)
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->Teams()->TeamMask(pChr->Team(), -1, ID));
			else if (m_Weapon == WEAPON_TELEKINESIS)
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA, pChr->Teams()->TeamMask(pChr->Team(), -1, ID));
			else if (m_Weapon == WEAPON_LIGHTSABER)
				GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, pChr->Teams()->TeamMask(pChr->Team(), -1, ID));
		}
		else if (m_Type == POWERUP_BATTERY)
			GameServer()->CreateSound(m_Pos, SOUND_HOOK_LOOP, pChr->Teams()->TeamMask(pChr->Team(), -1, ID));
		else if (m_Type == POWERUP_HEALTH)
			GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, pChr->Teams()->TeamMask(pChr->Team(), -1, ID));
		else if (m_Type == POWERUP_ARMOR)
			GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->Teams()->TeamMask(pChr->Team(), -1, ID));

		Reset(true);
	}
}

int CPickupDrop::IsCharacterNear()
{
	CCharacter *apEnts[MAX_CLIENTS];
	int Num = GameWorld()->FindEntities(m_Pos, 20.0f, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

	for (int i = 0; i < Num; i++)
	{
		CCharacter* pChr = apEnts[i];

		if ((m_PickupDelay > 0 && pChr == GetOwner()) || (m_Owner >= 0 && !pChr->CanCollide(m_Owner, false)))
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
				|| (pChr->GetWeaponGot(m_Weapon) && m_Special == 0 && (pChr->GetWeaponAmmo(m_Weapon) == -1 || (pChr->GetWeaponAmmo(m_Weapon) >= m_Bullets && m_Bullets >= 0)))
				|| (m_Special != 0 && !AcceptSpecial)
				)
				continue;
		}
		else if (m_Type == POWERUP_BATTERY && !pChr->GetPlayer()->GiveTaserBattery(m_Bullets))
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
	int Num = GameWorld()->FindEntities(m_Pos, 20.0f, (CEntity**)apEnts, 9, CGameWorld::ENTTYPE_PICKUP);

	for (int i = 0; i < Num; i++)
	{
		if (apEnts[i]->GetType() == POWERUP_ARMOR && apEnts[i]->GetOwner() < 0)
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

	if (GameServer()->GetPlayerChar(SnappingClient))
	{
		if (!CmaskIsSet(m_TeamMask, SnappingClient))
			return;
	}

	if (m_Type == POWERUP_BATTERY || m_Weapon == WEAPON_LIGHTSABER)
	{
		CNetObj_Projectile* pProj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
		if (!pProj)
			return;

		static float s_Time = 0.0f;
		static float s_LastLocalTime = Server()->Tick();

		s_Time += (Server()->Tick() - s_LastLocalTime) / Server()->TickSpeed();

		float Offset = m_SnapPos.y / 32.0f + m_SnapPos.x / 32.0f;
		m_SnapPos.x = m_Pos.x + cosf(s_Time * 2.0f + Offset) * 2.5f;
		m_SnapPos.y = m_Pos.y + sinf(s_Time * 2.0f + Offset) * 2.5f;
		s_LastLocalTime = Server()->Tick();

		pProj->m_X = m_SnapPos.x;
		pProj->m_Y = m_SnapPos.y;

		pProj->m_VelX = 0;
		pProj->m_VelY = 0;
		pProj->m_StartTick = 0;
		pProj->m_Type = WEAPON_LASER;
	}
	else
	{
		int Size = Server()->IsSevendown(SnappingClient) ? 4*4 : sizeof(CNetObj_Pickup);
		CNetObj_Pickup* pP = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), Size));
		if (!pP)
			return;

		pP->m_X = (int)m_Pos.x;
		pP->m_Y = (int)m_Pos.y;
		if (Server()->IsSevendown(SnappingClient))
		{
			pP->m_Type = m_Type;
			((int*)pP)[3] = GameServer()->GetWeaponType(m_Weapon);
		}
		else
			pP->m_Type = GameServer()->GetPickupType(m_Type, m_Weapon);
	}

	bool Gun = (m_Weapon == WEAPON_GUN && (m_Special&SPECIAL_JETPACK || m_Special&SPECIAL_TELEWEAPON)) || m_Weapon == WEAPON_PROJECTILE_RIFLE || (m_Weapon == WEAPON_HAMMER && m_Special&SPECIAL_DOORHAMMER);
	bool Plasma = m_Weapon == WEAPON_PLASMA_RIFLE || m_Weapon == WEAPON_LIGHTSABER || m_Weapon == WEAPON_PORTAL_RIFLE || (m_Weapon == WEAPON_LASER && m_Special&SPECIAL_TELEWEAPON);
	bool Heart = m_Weapon == WEAPON_HEART_GUN;
	bool Grenade = m_Weapon == WEAPON_STRAIGHT_GRENADE || m_Weapon == WEAPON_BALL_GRENADE || (m_Weapon == WEAPON_GRENADE && m_Special&SPECIAL_TELEWEAPON);

	int ExtraBulletOffset = 30;
	int SpreadOffset = -20;
	if (m_Special&SPECIAL_SPREADWEAPON && (Gun || Plasma || Heart || Grenade))
		ExtraBulletOffset = 50;

	if (m_Special&SPECIAL_SPREADWEAPON)
	{
		for (int i = 1; i < 4; i++)
		{
			CNetObj_Projectile* pSpreadIndicator = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_aID[i], sizeof(CNetObj_Projectile)));
			if (!pSpreadIndicator)
				return;

			pSpreadIndicator->m_X = (int)m_Pos.x + SpreadOffset;
			pSpreadIndicator->m_Y = (int)m_Pos.y - 30;
			pSpreadIndicator->m_Type = WEAPON_SHOTGUN;
			pSpreadIndicator->m_StartTick = Server()->Tick();

			SpreadOffset += 20;
		}
	}

	if (Gun)
	{
		CNetObj_Projectile* pShotgunBullet = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_aID[0], sizeof(CNetObj_Projectile)));
		if (!pShotgunBullet)
			return;

		pShotgunBullet->m_X = (int)m_Pos.x;
		pShotgunBullet->m_Y = (int)m_Pos.y - ExtraBulletOffset;
		pShotgunBullet->m_Type = WEAPON_SHOTGUN;
		pShotgunBullet->m_StartTick = Server()->Tick();
	}
	else if (Plasma)
	{
		CNetObj_Laser* pLaser = static_cast<CNetObj_Laser*>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_aID[0], sizeof(CNetObj_Laser)));
		if (!pLaser)
			return;

		pLaser->m_X = (int)m_Pos.x;
		pLaser->m_Y = (int)m_Pos.y - ExtraBulletOffset;
		pLaser->m_FromX = (int)m_Pos.x;
		pLaser->m_FromY = (int)m_Pos.y - ExtraBulletOffset;
		pLaser->m_StartTick = Server()->Tick();
	}
	else if (Heart)
	{
		int Size = Server()->IsSevendown(SnappingClient) ? 4*4 : sizeof(CNetObj_Pickup);
		CNetObj_Pickup* pPickup = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_aID[0], Size));
		if (!pPickup)
			return;

		pPickup->m_X = (int)m_Pos.x;
		pPickup->m_Y = (int)m_Pos.y - ExtraBulletOffset;
		pPickup->m_Type = POWERUP_HEALTH;
	}
	else if (Grenade)
	{
		CNetObj_Projectile* pProj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_aID[0], sizeof(CNetObj_Projectile)));
		if (!pProj)
			return;

		pProj->m_X = (int)m_Pos.x;
		pProj->m_Y = (int)m_Pos.y - ExtraBulletOffset;
		pProj->m_StartTick = Server()->Tick();
		pProj->m_Type = WEAPON_GRENADE;
	}
}
