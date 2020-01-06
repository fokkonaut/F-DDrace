/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <generated/server_data.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include "character.h"
#include "pickup.h"

#include <game/server/teams.h>
#include <engine/shared/config.h>

CPickup::CPickup(CGameWorld* pGameWorld, vec2 Pos, int Type, int SubType, int Layer, int Number, int Owner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, Pos, PickupPhysSize)
{
	m_Type = Type;
	m_Subtype = SubType;

	m_Layer = Layer;
	m_Number = Number;

	m_Owner = Owner;
	m_SnapPos = m_Pos;

	Reset();

	m_ID2 = Server()->SnapNewID();
	GameWorld()->InsertEntity(this);
}

void CPickup::Reset(bool Destroy)
{
	if (g_pData->m_aPickups[m_Type].m_Spawndelay > 0 && g_Config.m_SvVanillaModeStart)
		m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * g_pData->m_aPickups[m_Type].m_Spawndelay;
	else
		m_SpawnTick = -1;

	if (Destroy)
	{
		Server()->SnapFreeID(m_ID2);
		GameWorld()->DestroyEntity(this);
	}
}

void CPickup::Tick()
{
	if (m_Owner >= 0)
	{
		CCharacter* pChr = GameServer()->GetPlayerChar(m_Owner);
		if (pChr && (pChr->m_pPassiveShield == this || pChr->m_pItem == this))
		{
			m_Pos.x = pChr->GetPos().x;
			m_Pos.y = pChr->GetPos().y - 50;
		}
		else
			Reset(true);
	}
	else
	{
		Move();
		// wait for respawn
		if(m_SpawnTick > 0)
		{
			if(Server()->Tick() > m_SpawnTick)
			{
				// respawn
				m_SpawnTick = -1;

				if(m_Type == POWERUP_WEAPON)
					GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN);
			}
			else
				return;
		}
		// Check if a player intersected us
		CCharacter* apEnts[MAX_CLIENTS];
		int Num = GameWorld()->FindEntities(m_Pos, 20.0f, (CEntity * *)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for (int i = 0; i < Num; ++i) {
			CCharacter* pChr = apEnts[i];
			if (pChr && pChr->IsAlive())
			{
				if (m_Layer == LAYER_SWITCH && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[pChr->Team()]) continue;
				bool Sound = false;
				// player picked us up, is someone was hooking us, let them go
				int RespawnTime = -1;
				switch (m_Type)
				{
					case POWERUP_HEALTH:
						if (pChr->GetPlayer()->m_Gamemode == GAMEMODE_VANILLA)
						{
							if (pChr->IncreaseHealth(1))
							{
								GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, pChr->Teams()->TeamMask(pChr->Team()));
								RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
							}
						}
						else if (pChr->Freeze()) GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, pChr->Teams()->TeamMask(pChr->Team()));
						break;

					case POWERUP_ARMOR:
						if (pChr->Team() == TEAM_SUPER) continue;
						if (pChr->GetPlayer()->m_Gamemode == GAMEMODE_VANILLA)
						{
							if (pChr->IncreaseArmor(1))
							{
								GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->Teams()->TeamMask(pChr->Team()));
								RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
							}
						}
						else if (pChr->GetPlayer()->m_SpookyGhost)
						{
							for (int i = WEAPON_SHOTGUN; i < NUM_WEAPONS; i++)
							{
								if (!Sound && pChr->m_aWeaponsBackupGot[i][BACKUP_SPOOKY_GHOST])
								{
									GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->Teams()->TeamMask(pChr->Team()));
									Sound = true;
								}
								pChr->m_aWeaponsBackupGot[i][BACKUP_SPOOKY_GHOST] = false;
							}
						}
						else
						{
							for (int i = WEAPON_SHOTGUN; i < NUM_WEAPONS; i++)
							{
								if (pChr->GetWeaponGot(i))
								{
									if (!(pChr->m_FreezeTime && i == WEAPON_NINJA))
									{
										pChr->SetWeaponGot(i, false);
										pChr->SetWeaponAmmo(i, 0);
										Sound = true;
									}
								}
							}
							pChr->SetNinjaActivationDir(vec2(0, 0));
							pChr->SetNinjaActivationTick(-500);
							pChr->SetNinjaCurrentMoveTime(0);
							if (Sound)
							{
								pChr->SetLastWeapon(WEAPON_GUN);
								GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->Teams()->TeamMask(pChr->Team()));
							}
							if (!pChr->m_FreezeTime && pChr->GetActiveWeapon() >= WEAPON_SHOTGUN)
								pChr->SetActiveWeapon(WEAPON_HAMMER);
						}
						break;

					case POWERUP_WEAPON:
						if (m_Subtype >= 0 && m_Subtype < NUM_WEAPONS && (!pChr->GetWeaponGot(m_Subtype) || pChr->GetWeaponAmmo(m_Subtype) != -1))
						{
							if (pChr->GetPlayer()->m_SpookyGhost && GameServer()->GetRealWeapon(m_Subtype) != WEAPON_GUN)
								break;

							if (pChr->GetPlayer()->m_Gamemode == GAMEMODE_VANILLA && (pChr->GetWeaponAmmo(m_Subtype) < 10 || !pChr->GetWeaponGot(m_Subtype)))
								pChr->GiveWeapon(m_Subtype, false, 10);
							else if (pChr->GetPlayer()->m_Gamemode == GAMEMODE_DDRACE)
								pChr->GiveWeapon(m_Subtype);
							else break;

							RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;

							if (m_Subtype == WEAPON_GRENADE || m_Subtype == WEAPON_STRAIGHT_GRENADE || m_Subtype == WEAPON_BALL_GRENADE)
								GameServer()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE, pChr->Teams()->TeamMask(pChr->Team()));
							else if (m_Subtype == WEAPON_SHOTGUN || m_Subtype == WEAPON_LASER || m_Subtype == WEAPON_PLASMA_RIFLE || m_Subtype == WEAPON_TELE_RIFLE || m_Subtype == WEAPON_PROJECTILE_RIFLE)
								GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, pChr->Teams()->TeamMask(pChr->Team()));
							else if (m_Subtype == WEAPON_HAMMER || m_Subtype == WEAPON_GUN || m_Subtype == WEAPON_HEART_GUN)
								GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->Teams()->TeamMask(pChr->Team()));
							else if (m_Subtype == WEAPON_TELEKINESIS)
								GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA, pChr->Teams()->TeamMask(pChr->Team()));
							else if (m_Subtype == WEAPON_LIGHTSABER)
								GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, pChr->Teams()->TeamMask(pChr->Team()));

							GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), m_Subtype);

						}
						break;

					case POWERUP_NINJA:
					{
						if (pChr->GetPlayer()->m_SpookyGhost)
							break;

						// activate ninja on target player
						pChr->GiveNinja();
						if (pChr->GetPlayer()->m_Gamemode == GAMEMODE_VANILLA)
						{
							RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;

							// loop through all players, setting their emotes
							CCharacter* pC = static_cast<CCharacter*>(GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER));
							for (; pC; pC = (CCharacter*)pC->TypeNext())
							{
								if (pC != pChr)
									pC->SetEmote(EMOTE_SURPRISE, Server()->Tick() + Server()->TickSpeed());
							}
						}
						break;
					}

					case POWERUP_AMMO:

						if (true)
						{
							//nothing here yet

							if (pChr->GetPlayer()->m_Gamemode == GAMEMODE_VANILLA)
								RespawnTime = g_pData->m_aPickups[POWERUP_WEAPON].m_Respawntime;

							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->Teams()->TeamMask(pChr->Team()));
						}
						break;

					default:
						break;
				};

				if (pChr->GetPlayer()->m_Gamemode == GAMEMODE_VANILLA)
				{
					if (RespawnTime >= 0)
					{
						char aBuf[256];
						str_format(aBuf, sizeof(aBuf), "pickup player='%d:%s' item=%d/%d",
							pChr->GetPlayer()->GetCID(), Server()->ClientName(pChr->GetPlayer()->GetCID()), m_Type, m_Subtype);
						GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
						m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * RespawnTime;
					}
				}
			}
		}
	}
}

void CPickup::TickPaused()
{
	if(m_SpawnTick != -1)
		++m_SpawnTick;
}

void CPickup::Snap(int SnappingClient)
{
	if(m_SpawnTick != -1 || NetworkClipped(SnappingClient))
		return;

	CCharacter* pOwner = GameServer()->GetPlayerChar(m_Owner);
	CCharacter* Char = GameServer()->GetPlayerChar(SnappingClient);

	if(SnappingClient > -1 && (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == -1
				|| GameServer()->m_apPlayers[SnappingClient]->IsPaused())
			&& GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID() != -1)
		Char = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID());

	int Tick = (Server()->Tick()%Server()->TickSpeed())%11;
	if (Char && Char->IsAlive() &&
			(m_Layer == LAYER_SWITCH &&
					!GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[Char->Team()])
					&& (!Tick))
		return;

	if (pOwner && Char)
	{
		if (!CmaskIsSet(pOwner->Teams()->TeamMask(pOwner->Team(), -1, m_Owner), SnappingClient))
			return;
	}

	if (m_Type == POWERUP_AMMO)
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
		CNetObj_Pickup* pP = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
		if (!pP)
			return;

		pP->m_X = (int)m_Pos.x;
		pP->m_Y = (int)m_Pos.y;
		pP->m_Type = GameServer()->GetPickupType(m_Type, m_Subtype);
	}

	bool Gun = m_Type == WEAPON_PROJECTILE_RIFLE;
	bool Plasma = m_Type == WEAPON_PLASMA_RIFLE || m_Type == WEAPON_LIGHTSABER || m_Type == WEAPON_TELE_RIFLE;
	bool Heart = m_Type == WEAPON_HEART_GUN;
	bool Grenade = m_Type == WEAPON_STRAIGHT_GRENADE || m_Type == WEAPON_BALL_GRENADE;

	if (Gun)
	{
		CNetObj_Projectile* pShotgunBullet = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID2, sizeof(CNetObj_Projectile)));
		if (!pShotgunBullet)
			return;

		pShotgunBullet->m_X = (int)m_Pos.x;
		pShotgunBullet->m_Y = (int)m_Pos.y - 30;
		pShotgunBullet->m_Type = WEAPON_SHOTGUN;
		pShotgunBullet->m_StartTick = Server()->Tick();
	}
	else if (Plasma)
	{
		CNetObj_Laser* pLaser = static_cast<CNetObj_Laser*>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ID2, sizeof(CNetObj_Laser)));
		if (!pLaser)
			return;

		pLaser->m_X = (int)m_Pos.x;
		pLaser->m_Y = (int)m_Pos.y - 30;
		pLaser->m_FromX = (int)m_Pos.x;
		pLaser->m_FromY = (int)m_Pos.y - 30;
		pLaser->m_StartTick = Server()->Tick();
	}
	else if (Heart)
	{
		CNetObj_Pickup* pPickup = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_ID2, sizeof(CNetObj_Pickup)));
		if (!pPickup)
			return;

		pPickup->m_X = (int)m_Pos.x;
		pPickup->m_Y = (int)m_Pos.y - 30;
		pPickup->m_Type = POWERUP_HEALTH;
	}
	else if (Grenade)
	{
		CNetObj_Projectile* pProj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID2, sizeof(CNetObj_Projectile)));
		if (!pProj)
			return;

		pProj->m_X = (int)m_Pos.x;
		pProj->m_Y = (int)m_Pos.y - 30;
		pProj->m_StartTick = Server()->Tick();
		pProj->m_Type = WEAPON_GRENADE;
	}
}

void CPickup::Move()
{
	if (Server()->Tick() % int(Server()->TickSpeed() * 0.15f) == 0)
	{
		int Flags;
		int index = GameServer()->Collision()->IsMover(m_Pos.x, m_Pos.y, &Flags);
		if (index)
		{
			m_Core = GameServer()->Collision()->CpSpeed(index, Flags);
		}
		m_Pos += m_Core;
	}
}
