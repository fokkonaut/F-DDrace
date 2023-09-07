/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <generated/server_data.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <base/math.h>
#include "character.h"
#include "pickup.h"

#include <game/server/teams.h>
#include <engine/shared/config.h>

CPickup::CPickup(CGameWorld* pGameWorld, vec2 Pos, int Type, int SubType, int Layer, int Number, int Owner, bool Collision)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, Pos, PickupPhysSize, Collision)
{
	m_Type = Type;
	m_Subtype = SubType;

	m_Layer = Layer;
	m_Number = Number;

	m_Owner = Owner;
	
	m_Snap.m_Pos = m_Pos;
	m_Snap.m_Time = 0.f;
	m_Snap.m_LastTime = Server()->Tick();

	for (int i = 0; i < MAX_CLIENTS; i++)
		m_aLastRespawnMsg[i] = 0;

	m_PickupTick = 0;

	Reset();

	m_ID2 = Server()->SnapNewID();
	GameWorld()->InsertEntity(this);
}

CPickup::~CPickup()
{
	Server()->SnapFreeID(m_ID2);
}

void CPickup::Reset(bool Destroy)
{
	m_SpawnTick = -1;
	SetRespawnTime(true);

	if (Destroy)
		GameWorld()->DestroyEntity(this);
}

void CPickup::SetRespawnTime(bool Init)
{
	if (!m_Collision)
		return;

	int RespawnTime = -1;
	if (Init)
	{
		if (g_pData->m_aPickups[m_Type].m_Spawndelay > 0 && Config()->m_SvVanillaModeStart)
			RespawnTime = g_pData->m_aPickups[GameServer()->GetPickupType(m_Type, m_Subtype)].m_Spawndelay;
	}
	else
	{
		RespawnTime = g_pData->m_aPickups[GameServer()->GetPickupType(m_Type, m_Subtype)].m_Respawntime;
	}

	if (m_Type == POWERUP_BATTERY)
	{
		if (m_Subtype == WEAPON_TASER)
		{
			RespawnTime = Config()->m_SvBatteryRespawnTime * 60;
		}
		else if (m_Subtype == WEAPON_PORTAL_RIFLE && Config()->m_SvPortalRifleAmmo)
		{
			// between 1 and 5 hours to respawn, and reduce time the more players are connected (1 player = 1 min)
			int Minutes = ((rand() % (300 - 60) + 60) - GameServer()->CountConnectedPlayers(false, true));
			RespawnTime = max(Minutes * 60, 30 * 60);
		}
	}
	else if (m_Subtype == WEAPON_PORTAL_RIFLE) // weapon
		RespawnTime = Config()->m_SvPortalRifleRespawnTime * 60;

	if (RespawnTime >= 0)
		m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * RespawnTime;
}

void CPickup::Tick()
{
	// no affect on players, just a preview for the brushing client
	if (!m_Collision)
		return;

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

		return;
	}

	Move();
	// wait for respawn
	if(m_SpawnTick > 0)
	{
		if(Server()->Tick() > m_SpawnTick)
		{
			// respawn
			m_SpawnTick = -1;

			if(m_Type == POWERUP_WEAPON || m_Type == POWERUP_BATTERY)
				GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN);
		}
		else if (m_Type != POWERUP_BATTERY && (m_Subtype != WEAPON_PORTAL_RIFLE || !Config()->m_SvPortalRifleAmmo))
			return;
	}

	// Check if a player intersected us
	CCharacter* apEnts[MAX_CLIENTS];
	int Num = GameWorld()->FindEntities(m_Pos, 20.0f, (CEntity * *)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for (int i = 0; i < Num; ++i)
	{
		CCharacter* pChr = apEnts[i];
		if (!pChr || !pChr->IsAlive())
			continue;

		// avoid affecting players through a wall when pickups on plots are placed at the edge of the wall without position rounding
		// also means pickups placed right next to the plot door are not pickable by someone that might be close enough to the pickup because they are not on the same plotid
		if (m_PlotID >= PLOT_START && pChr->GetCurrentTilePlotID(true) != m_PlotID)
			continue;

		if (m_Layer == LAYER_SWITCH && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[pChr->Team()]) continue;
		bool Sound = false;
		// player picked us up, is someone was hooking us, let them go
		bool Picked = false;
		if (m_SpawnTick <= 0)
		{
			switch (m_Type)
			{
				case POWERUP_HEALTH:
					if (pChr->GetPlayer()->m_Gamemode == GAMEMODE_VANILLA)
					{
						if (pChr->IncreaseHealth(1))
						{
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, pChr->TeamMask());
							Picked = true;
						}
					}
					else if (pChr->Freeze()) GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, pChr->TeamMask());
					break;

				case POWERUP_ARMOR:
					if (pChr->GetPlayer()->m_Gamemode == GAMEMODE_VANILLA)
					{
						if (pChr->IncreaseArmor(1))
						{
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());
							Picked = true;
						}
					}
					else if (pChr->Team() == TEAM_SUPER) continue;
					else if (pChr->GetPlayer()->m_SpookyGhost)
					{
						for (int i = WEAPON_SHOTGUN; i < NUM_WEAPONS; i++)
						{
							if (!Sound && pChr->m_aWeaponsBackupGot[i][BACKUP_SPOOKY_GHOST])
							{
								GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());
								Sound = true;
							}
							pChr->m_aWeaponsBackupGot[i][BACKUP_SPOOKY_GHOST] = false;
						}
					}
					else
					{
						for (int i = WEAPON_SHOTGUN; i < NUM_WEAPONS; i++)
						{
							if (pChr->GetWeaponGot(i) && i != WEAPON_DRAW_EDITOR)
							{
								pChr->SetWeaponGot(i, false);
								pChr->SetWeaponAmmo(i, 0);
								Sound = true;
							}
						}
						pChr->SetNinjaActivationDir(vec2(0, 0));
						pChr->SetNinjaActivationTick(-500);
						pChr->SetNinjaCurrentMoveTime(0);
						if (pChr->m_ScrollNinja)
							pChr->ScrollNinja(false);
						if (pChr->GetActiveWeapon() >= WEAPON_SHOTGUN && pChr->GetActiveWeapon() != WEAPON_DRAW_EDITOR)
							pChr->SetActiveWeapon(WEAPON_HAMMER);
						if (Sound)
						{
							pChr->SetLastWeapon(WEAPON_GUN);
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());
						}
					}
					break;

				case POWERUP_WEAPON:
					if (m_Subtype >= 0 && m_Subtype < NUM_WEAPONS && (!pChr->GetWeaponGot(m_Subtype) || pChr->GetWeaponAmmo(m_Subtype) != -1))
					{
						if (pChr->GetPlayer()->m_SpookyGhost && GameServer()->GetWeaponType(m_Subtype) != WEAPON_GUN)
							break;

						bool FightStarted = GameServer()->Arenas()->FightStarted(pChr->GetPlayer()->GetCID());
						if (m_Subtype == WEAPON_PORTAL_RIFLE && Config()->m_SvPortalRifleAmmo && FightStarted)
							break;
						if (m_Subtype == WEAPON_TASER && (GameServer()->m_Accounts[pChr->GetPlayer()->GetAccID()].m_TaserLevel < 1 || FightStarted))
							break;

						pChr->WeaponMoneyReward(m_Subtype);
						if (pChr->GetPlayer()->m_Gamemode == GAMEMODE_VANILLA && (pChr->GetWeaponAmmo(m_Subtype) < 10 || !pChr->GetWeaponGot(m_Subtype)))
							pChr->GiveWeapon(m_Subtype, false, 10);
						else if (pChr->GetPlayer()->m_Gamemode == GAMEMODE_DDRACE)
							pChr->GiveWeapon(m_Subtype);
						else break;

						Picked = true;

						if (m_Subtype == WEAPON_GRENADE || m_Subtype == WEAPON_STRAIGHT_GRENADE || m_Subtype == WEAPON_BALL_GRENADE)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE, pChr->TeamMask());
						else if (m_Subtype == WEAPON_SHOTGUN || m_Subtype == WEAPON_LASER || m_Subtype == WEAPON_TASER || m_Subtype == WEAPON_PLASMA_RIFLE || m_Subtype == WEAPON_PORTAL_RIFLE || m_Subtype == WEAPON_PROJECTILE_RIFLE || m_Subtype == WEAPON_TELE_RIFLE || m_Subtype == WEAPON_LIGHTNING_LASER)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, pChr->TeamMask());
						else if (m_Subtype == WEAPON_HAMMER || m_Subtype == WEAPON_GUN || m_Subtype == WEAPON_HEART_GUN)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());
						else if (m_Subtype == WEAPON_TELEKINESIS)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA, pChr->TeamMask());
						else if (m_Subtype == WEAPON_LIGHTSABER)
							GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, pChr->TeamMask());

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
						Picked = true;

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

				case POWERUP_BATTERY:
				{
					if (GameServer()->Arenas()->FightStarted(pChr->GetPlayer()->GetCID()))
						break;

					if ((m_Subtype == WEAPON_TASER && pChr->GetPlayer()->GiveTaserBattery(10))
						|| (m_Subtype == WEAPON_PORTAL_RIFLE && Config()->m_SvPortalRifleAmmo && pChr->GetPlayer()->GivePortalBattery(1)))
					{
						Picked = true;
						GameServer()->CreateSound(m_Pos, SOUND_HOOK_LOOP, pChr->TeamMask());
					}
					break;
				}

				default:
					break;
			};

			if (pChr->GetPlayer()->m_Gamemode == GAMEMODE_VANILLA || m_Type == POWERUP_BATTERY || (m_Subtype == WEAPON_PORTAL_RIFLE && Config()->m_SvPortalRifleAmmo))
			{
				if (Picked)
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "pickup player='%d:%s' item=%d/%d",
						pChr->GetPlayer()->GetCID(), Server()->ClientName(pChr->GetPlayer()->GetCID()), m_Type, m_Subtype);
					GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

					m_PickupTick = Server()->Tick();
					SetRespawnTime();
				}
			}
		}
		else
		{
			if (m_Type == POWERUP_BATTERY || (m_Subtype == WEAPON_PORTAL_RIFLE && Config()->m_SvPortalRifleAmmo))
			{
				int ClientID = pChr->GetPlayer()->GetCID();
				if (m_aLastRespawnMsg[ClientID] + Server()->TickSpeed() * 5 > Server()->Tick())
					continue;

				m_aLastRespawnMsg[ClientID] = Server()->Tick();

				bool RespawnTimer = true;
				const char *pType = "";
				if (m_Type == POWERUP_BATTERY)
				{
					pType = "battery";
					RespawnTimer = m_Subtype != WEAPON_PORTAL_RIFLE;
				}
				else if (m_Subtype == WEAPON_PORTAL_RIFLE && Config()->m_SvPortalRifleAmmo)
				{
					pType = "portal rifle";
				}

				if (!pType[0])
					continue;

				char aBuf[64] = "";
				if (RespawnTimer)
				{
					int Seconds = (m_SpawnTick - Server()->Tick()) / Server()->TickSpeed();
					if (Seconds <= 60)
						str_format(aBuf, sizeof(aBuf), "This %s will respawn in %d seconds", pType, Seconds);
					else
						str_format(aBuf, sizeof(aBuf), "This %s will respawn in %d minutes", pType, Seconds / 60);
				}
				else
				{
					int Seconds = (Server()->Tick() - m_PickupTick) / Server()->TickSpeed();
					if (Seconds <= 60)
						str_format(aBuf, sizeof(aBuf), "This %s got picked up %d seconds ago", pType, Seconds);
					else
						str_format(aBuf, sizeof(aBuf), "This %s got picked up %d minutes ago", pType, Seconds / 60);
				}
				GameServer()->SendChatTarget(ClientID, aBuf);
				continue;
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

	CCharacter *pChr = GameServer()->GetPlayerChar(SnappingClient);
	if (pChr && pChr->m_DrawEditor.OnSnapPreview(this))
		return;

	CCharacter* pOwner = GameServer()->GetPlayerChar(m_Owner);

	if(SnappingClient > -1 && (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == -1
				|| GameServer()->m_apPlayers[SnappingClient]->IsPaused())
			&& GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID() != -1)
		pChr = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID());

	if (pOwner && pChr)
	{
		if (!CmaskIsSet(pOwner->TeamMask(), SnappingClient))
			return;
	}

	CNetObj_EntityEx *pEntData = 0;
	if (m_Layer == LAYER_SWITCH || length(m_Core) > 0)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(SnappingClient);
		if (pChr && pChr->SendExtendedEntity(this))
			pEntData = static_cast<CNetObj_EntityEx *>(Server()->SnapNewItem(NETOBJTYPE_ENTITYEX, GetID(), sizeof(CNetObj_EntityEx)));
	}

	if (pEntData)
	{
		pEntData->m_SwitchNumber = m_Number;
		pEntData->m_Layer = m_Layer;
		pEntData->m_EntityClass = ENTITYCLASS_PICKUP;
	}
	else
	{
		int Tick = (Server()->Tick() % Server()->TickSpeed()) % 11;
		if (pChr && pChr->IsAlive() && (m_Layer == LAYER_SWITCH && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[pChr->Team()]) && (!Tick))
			return;
	}

	if (m_Type == POWERUP_BATTERY)
	{
		CNetObj_Projectile* pProj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
		if (!pProj)
			return;

		m_Snap.m_Time += (Server()->Tick() - m_Snap.m_LastTime) / Server()->TickSpeed();
		m_Snap.m_LastTime = Server()->Tick();

		float Offset = m_Snap.m_Pos.y / 32.0f + m_Snap.m_Pos.x / 32.0f;
		m_Snap.m_Pos.x = m_Pos.x + cosf(m_Snap.m_Time * 2.0f + Offset) * 2.5f;
		m_Snap.m_Pos.y = m_Pos.y + sinf(m_Snap.m_Time * 2.0f + Offset) * 2.5f;

		pProj->m_X = m_Snap.m_Pos.x;
		pProj->m_Y = m_Snap.m_Pos.y;

		pProj->m_X = round_to_int(m_Snap.m_Pos.x);
		pProj->m_Y = round_to_int(m_Snap.m_Pos.y);

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

		pP->m_X = round_to_int(m_Pos.x);
		pP->m_Y = round_to_int(m_Pos.y);
		if (Server()->IsSevendown(SnappingClient))
		{
			int Subtype = GameServer()->GetWeaponType(m_Subtype);
			pP->m_Type = Subtype == WEAPON_NINJA ? POWERUP_NINJA : m_Type;
			((int*)pP)[3] = Subtype;
		}
		else
			pP->m_Type = GameServer()->GetPickupType(m_Type, m_Subtype);
	}

	bool Gun = m_Subtype == WEAPON_PROJECTILE_RIFLE;
	bool Plasma = m_Subtype == WEAPON_PLASMA_RIFLE || m_Subtype == WEAPON_LIGHTSABER || m_Subtype == WEAPON_PORTAL_RIFLE || m_Subtype == WEAPON_TELE_RIFLE
		|| m_Subtype == WEAPON_LIGHTNING_LASER || (m_Subtype == WEAPON_TASER && m_Type == POWERUP_WEAPON);
	bool Heart = m_Subtype == WEAPON_HEART_GUN;
	bool Grenade = m_Subtype == WEAPON_STRAIGHT_GRENADE || m_Subtype == WEAPON_BALL_GRENADE;

	if (Gun)
	{
		CNetObj_Projectile* pShotgunBullet = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID2, sizeof(CNetObj_Projectile)));
		if (!pShotgunBullet)
			return;

		pShotgunBullet->m_X = round_to_int(m_Pos.x);
		pShotgunBullet->m_Y = round_to_int(m_Pos.y - 30);
		pShotgunBullet->m_Type = WEAPON_SHOTGUN;
		pShotgunBullet->m_StartTick = 0;
	}
	else if (Plasma)
	{
		if(GameServer()->GetClientDDNetVersion(SnappingClient) >= VERSION_DDNET_MULTI_LASER)
		{
			CNetObj_DDNetLaser * pLaser = static_cast<CNetObj_DDNetLaser *>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, m_ID2, sizeof(CNetObj_DDNetLaser)));
			if(!pLaser)
				return;

			pLaser->m_ToX = round_to_int(m_Pos.x);
			pLaser->m_ToY = round_to_int(m_Pos.y - 30);
			pLaser->m_FromX = round_to_int(m_Pos.x);
			pLaser->m_FromY = round_to_int(m_Pos.y - 30);
			pLaser->m_StartTick = Server()->Tick();
			pLaser->m_Owner = -1;
			pLaser->m_Type = (m_Subtype == WEAPON_TASER || m_Subtype == WEAPON_LIGHTNING_LASER) ? LASERTYPE_FREEZE : LASERTYPE_RIFLE;
		}
		else
		{
			CNetObj_Laser * pLaser = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ID2, sizeof(CNetObj_Laser)));
			if (!pLaser)
				return;

			pLaser->m_X = round_to_int(m_Pos.x);
			pLaser->m_Y = round_to_int(m_Pos.y - 30);
			pLaser->m_FromX = round_to_int(m_Pos.x);
			pLaser->m_FromY = round_to_int(m_Pos.y - 30);
			pLaser->m_StartTick = Server()->Tick();
		}
	}
	else if (Heart)
	{
		int Size = Server()->IsSevendown(SnappingClient) ? 4*4 : sizeof(CNetObj_Pickup);
		CNetObj_Pickup* pPickup = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_ID2, Size));
		if (!pPickup)
			return;

		pPickup->m_X = round_to_int(m_Pos.x);
		pPickup->m_Y = round_to_int(m_Pos.y - 30);
		pPickup->m_Type = POWERUP_HEALTH;
	}
	else if (Grenade)
	{
		CNetObj_Projectile* pProj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID2, sizeof(CNetObj_Projectile)));
		if (!pProj)
			return;

		pProj->m_X = round_to_int(m_Pos.x);
		pProj->m_Y = round_to_int(m_Pos.y - 30);
		pProj->m_StartTick = Server()->Tick() - 2;
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
