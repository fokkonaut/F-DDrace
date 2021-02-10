/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <generated/server_data.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>

#include "character.h"
#include "laser.h"
#include "projectile.h"

// F-DDrace
#include "flag.h"
#include "custom_projectile.h"
#include "meteor.h"
#include "pickup_drop.h"
#include "atom.h"
#include "trail.h"
#include "portal.h"
#include "money.h"

#include "dummy/blmapchill_police.h"
#include "dummy/house.h"
#include "dummy/v3_blocker.h"
#include "dummy/chillblock5_police.h"

#include <game/server/gamemodes/DDRace.h>
#include <game/server/score.h>
#include <generated/protocol.h>

//input count
struct CInputCount
{
	int m_Presses;
	int m_Releases;
};

CInputCount CountInput(int Prev, int Cur)
{
	CInputCount c = {0, 0};
	Prev &= INPUT_STATE_MASK;
	Cur &= INPUT_STATE_MASK;
	int i = Prev;

	while(i != Cur)
	{
		i = (i+1)&INPUT_STATE_MASK;
		if(i&1)
			c.m_Presses++;
		else
			c.m_Releases++;
	}

	return c;
}


MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CLIENTS)

// Character, "physical" player's part
CCharacter::CCharacter(CGameWorld *pWorld)
: CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER, vec2(0, 0), ms_PhysSize), m_DrawEditor(this)
{
	m_Health = 0;
	m_Armor = 0;
	m_TriggeredEvents = 0;
	m_StrongWeakID = 0;
}

CCharacter::~CCharacter()
{
	if (m_pDummyHandle)
		delete m_pDummyHandle;
}

void CCharacter::Reset()
{
	Destroy();
}

bool CCharacter::Spawn(CPlayer *pPlayer, vec2 Pos)
{
	m_EmoteStop = -1;
	m_LastAction = -1;
	m_LastNoAmmoSound = -1;
	m_LastWeapon = WEAPON_HAMMER;
	m_QueuedWeapon = -1;

	m_pPlayer = pPlayer;
	m_Pos = Pos;

	m_Core.Reset();
	m_Core.Init(&GameWorld()->m_Core, GameServer()->Collision(), &((CGameControllerDDRace*)GameServer()->m_pController)->m_Teams.m_Core, &((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts);
	m_Core.m_Pos = m_Pos;
	SetActiveWeapon(WEAPON_GUN);
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;

	m_ReckoningTick = 0;
	mem_zero(&m_SendCore, sizeof(m_SendCore));
	mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));

	GameWorld()->InsertEntity(this);
	m_Alive = true;

	FDDraceInit();
	GameServer()->m_pController->OnCharacterSpawn(this);
	Teams()->OnCharacterSpawn(GetPlayer()->GetCID());
	DDraceInit();

	m_pPlayer->LoadMinigameTee();
	GameServer()->CheckShutdownSaved(m_pPlayer->GetCID());

	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(Pos));
	m_TuneZoneOld = -1; // no zone leave msg on spawn
	SendZoneMsgs(); // we want a entermessage also on spawn
	GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);

	return true;
}

void CCharacter::Destroy()
{
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	m_Alive = false;
}

void CCharacter::SetWeapon(int W)
{
	// F-DDrace
	if (!GetWeaponGot(W))
	{
		SetAvailableWeapon();
		return;
	}

	if(W == GetActiveWeapon())
		return;

	m_LastWeapon = GetActiveWeapon();
	m_QueuedWeapon = -1;
	SetActiveWeapon(W);
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));

	if(GetActiveWeapon() < 0 || GetActiveWeapon() >= NUM_WEAPONS)
		SetActiveWeapon(WEAPON_GUN);
}

void CCharacter::SetSolo(bool Solo)
{
	m_Solo = Solo;
	Teams()->m_Core.SetSolo(m_pPlayer->GetCID(), Solo);
}

bool CCharacter::IsGrounded()
{
	if (GameServer()->Collision()->CheckPoint(m_Pos.x + GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 5))
		return true;
	if (GameServer()->Collision()->CheckPoint(m_Pos.x - GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 5))
		return true;

	int MoveRestrictionsBelow = GameServer()->Collision()->GetMoveRestrictions(m_Pos + vec2(0, GetProximityRadius() / 2 + 4), 0.0f, m_Core.m_MoveRestrictionExtra);
	if(MoveRestrictionsBelow&CANTMOVE_DOWN)
	{
		return true;
	}

	return false;
}

void CCharacter::HandleJetpack()
{
	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	bool FullAuto = m_Jetpack && GetActiveWeapon() == WEAPON_GUN;

	// check if we gonna fire
	bool WillFire = false;
	if (CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		WillFire = true;

	if (FullAuto && (m_LatestInput.m_Fire & 1) && m_aWeapons[GetActiveWeapon()].m_Ammo)
		WillFire = true;

	for (int i = 0; i < NUM_HOUSES; i++)
		if (GameServer()->m_pHouses[i]->CanChangePage(m_pPlayer->GetCID()))
			return;

	if (!WillFire)
		return;

	// check for ammo
	if (!m_aWeapons[GetActiveWeapon()].m_Ammo || m_FreezeTime)
	{
		return;
	}

	switch (GetActiveWeapon())
	{
	case WEAPON_GUN:
	{
		if (m_Jetpack)
		{
			float Strength;
			if (!m_TuneZone)
				Strength = GameServer()->Tuning()->m_JetpackStrength;
			else
				Strength = GameServer()->TuningList()[m_TuneZone].m_JetpackStrength;
			TakeDamage(Direction * -1.0f * (Strength / 100.0f / 6.11f), vec2(0, 0), 0, m_pPlayer->GetCID(), WEAPON_GUN);
		}
	}
	}
}

void CCharacter::HandleNinja()
{
	if(GetActiveWeapon() != WEAPON_NINJA)
		return;

	if (!m_ScrollNinja)
	{
		if ((Server()->Tick() - m_Ninja.m_ActivationTick) > (g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000))
		{
			RemoveNinja();
			return;
		}

		int NinjaTime = m_Ninja.m_ActivationTick + (g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000) - Server()->Tick();

		if (NinjaTime % Server()->TickSpeed() == 0 && NinjaTime / Server()->TickSpeed() <= 5)
		{
			GameServer()->CreateDamage(m_Pos, m_pPlayer->GetCID(), vec2(0, 0), NinjaTime / Server()->TickSpeed(), 0, true, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
		}

		// force ninja Weapon
		SetWeapon(WEAPON_NINJA);
	}

	m_Ninja.m_CurrentMoveTime--;

	if (m_Ninja.m_CurrentMoveTime == 0)
	{
		// reset velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir*m_Ninja.m_OldVelAmount;
	}

	if (m_Ninja.m_CurrentMoveTime > 0)
	{
		// Set velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir * g_pData->m_Weapons.m_Ninja.m_Velocity;
		vec2 OldPos = m_Pos;
		GameServer()->Collision()->MoveBox(&m_Core.m_Pos, &m_Core.m_Vel, vec2(GetProximityRadius(), GetProximityRadius()), 0.f);

		// reset velocity so the client doesn't predict stuff
		m_Core.m_Vel = vec2(0.f, 0.f);

		// check if we Hit anything along the way
		{
			CCharacter *aEnts[MAX_CLIENTS];
			vec2 Dir = m_Pos - OldPos;
			float Radius = GetProximityRadius() * 2.0f;
			vec2 Center = OldPos + Dir * 0.5f;
			int Num = GameWorld()->FindEntities(Center, Radius, (CEntity**)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

			for (int i = 0; i < Num; ++i)
			{
				if (aEnts[i] == this)
					continue;

				// check that we can collide with the other player
				if (!CanCollide(aEnts[i]->m_pPlayer->GetCID()))
					continue;

				// make sure we haven't Hit this object before
				bool bAlreadyHit = false;
				for (int j = 0; j < m_NumObjectsHit; j++)
				{
					if (m_apHitObjects[j] == aEnts[i])
						bAlreadyHit = true;
				}
				if (bAlreadyHit)
					continue;

				// check so we are sufficiently close
				if (distance(aEnts[i]->m_Pos, m_Pos) > (GetProximityRadius() * 2.0f))
					continue;

				// Hit a player, give him damage and stuffs...
				GameServer()->CreateSound(aEnts[i]->m_Pos, SOUND_NINJA_HIT, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
				// set his velocity to fast upward (for now)
				if(m_NumObjectsHit < 10)
					m_apHitObjects[m_NumObjectsHit++] = aEnts[i];

				aEnts[i]->TakeDamage(vec2(0, -10.0f), m_Ninja.m_ActivationDir*-1, g_pData->m_Weapons.m_Ninja.m_pBase->m_Damage, m_pPlayer->GetCID(), WEAPON_NINJA);
			}
		}
	}
}


void CCharacter::DoWeaponSwitch()
{
	// make sure we can switch
	if (m_ReloadTimer != 0 || m_QueuedWeapon == -1 || (m_aWeapons[WEAPON_NINJA].m_Got && !m_ScrollNinja) || !m_aWeapons[m_QueuedWeapon].m_Got || m_DrawEditor.Selecting())
		return;

	// switch Weapon
	SetWeapon(m_QueuedWeapon);
}

void CCharacter::HandleWeaponSwitch()
{
	int WantedWeapon = GetActiveWeapon();
	if(m_QueuedWeapon != -1)
		WantedWeapon = m_QueuedWeapon;

	bool Anything = false;
	for (int i = 0; i < NUM_WEAPONS; ++i)
		if (i != WEAPON_NINJA && m_aWeapons[i].m_Got)
			Anything = true;
	if (!Anything)
		return;

	// select Weapon
	int Next = CountInput(m_LatestPrevInput.m_NextWeapon, m_LatestInput.m_NextWeapon).m_Presses;
	int Prev = CountInput(m_LatestPrevInput.m_PrevWeapon, m_LatestInput.m_PrevWeapon).m_Presses;

	if(Next < 128) // make sure we only try sane stuff
	{
		while(Next) // Next Weapon selection
		{
			WantedWeapon = (WantedWeapon+1)%NUM_WEAPONS;
			if(m_aWeapons[WantedWeapon].m_Got)
				Next--;
		}
	}

	if(Prev < 128) // make sure we only try sane stuff
	{
		while(Prev) // Prev Weapon selection
		{
			WantedWeapon = (WantedWeapon-1)<0?NUM_WEAPONS-1:WantedWeapon-1;
			if(m_aWeapons[WantedWeapon].m_Got)
				Prev--;
		}
	}

	// F-DDrace
	// client keep sending the direct weapon input, and for the draweditor this is bad because weaponswitch means you exit the editor, means it automatically switches
	// back to the wanted weapon after you entered the editor, so we only allow setting the directly wanted weapon when its not the same as the last request
	if (m_LatestInput.m_WantedWeapon)
	{
		if (m_DrawEditor.Active() && m_Input.m_WantedWeapon == m_LastWantedWeapon)
			m_LatestInput.m_WantedWeapon = 0; // pretend we dont have a direct weapon selection
		m_LastWantedWeapon = m_Input.m_WantedWeapon;
	}

	// Direct Weapon selection
	if(m_LatestInput.m_WantedWeapon)
		WantedWeapon = m_Input.m_WantedWeapon-1;

	// check for insane values
	if(WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != GetActiveWeapon() && m_aWeapons[WantedWeapon].m_Got)
		m_QueuedWeapon = WantedWeapon;

	DoWeaponSwitch();
}

void CCharacter::FireWeapon()
{
	if(m_ReloadTimer != 0)
		return;

	DoWeaponSwitch();
	vec2 TempDirection = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	bool FullAuto = false;
	if
	(
		GetActiveWeapon() == WEAPON_GRENADE
		|| GetActiveWeapon() == WEAPON_SHOTGUN
		|| GetActiveWeapon() == WEAPON_LASER
		|| GetActiveWeapon() == WEAPON_PLASMA_RIFLE
		|| GetActiveWeapon() == WEAPON_STRAIGHT_GRENADE
		|| GetActiveWeapon() ==	WEAPON_TASER
		|| GetActiveWeapon() == WEAPON_PROJECTILE_RIFLE
		|| GetActiveWeapon() == WEAPON_BALL_GRENADE
	)
		FullAuto = true;
	if(m_Jetpack && GetActiveWeapon() == WEAPON_GUN)
		FullAuto = true;
	// allow firing directly after coming out of freeze or being unfrozen
	// by something
	if(m_FrozenLastTick)
		FullAuto = true;

	// don't fire hammer when player is deep and sv_deepfly is disabled
	if (!Config()->m_SvDeepfly && GetActiveWeapon() == WEAPON_HAMMER && m_DeepFreeze)
		return;

	bool ClickedFire = false;

	// check if we gonna fire
	bool WillFire = false;
	if(CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
	{
		WillFire = true;
		ClickedFire = true;
	}

	// shop window
	for (int i = 0; i < NUM_HOUSES; i++)
	{
		if (GameServer()->m_pHouses[i]->CanChangePage(m_pPlayer->GetCID()))
		{
			if (ClickedFire)
				GameServer()->m_pHouses[i]->DoPageChange(m_pPlayer->GetCID(), GetAimDir());
			return;
		}
	}

	if(FullAuto && (m_LatestInput.m_Fire&1) && m_aWeapons[GetActiveWeapon()].m_Ammo && !m_FreezeTime)
		WillFire = true;

	if(!WillFire)
		return;

	if (m_FreezeTime)
	{
		if (m_PainSoundTimer <= 0)
		{
			m_PainSoundTimer = 1 * Server()->TickSpeed();
			GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
		}
		return;
	}

	// check for ammo
	if(!m_aWeapons[GetActiveWeapon()].m_Ammo)
	{
		// 125ms is a magical limit of how fast a human can click
		m_ReloadTimer = 125 * Server()->TickSpeed() / 1000;
		if (m_LastNoAmmoSound + Server()->TickSpeed() <= Server()->Tick())
		{
			GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
			m_LastNoAmmoSound = Server()->Tick();
		}
		return;
	}

	// if we have aimbot or spinbot on and shoot or hook, we want to put the mouse angle in the correct position for some time, so that we dont end up shooting in a weird direction graphically
	m_Core.m_UpdateAngle = UPDATE_ANGLE_TIME;

	if (m_DrawEditor.Active())
	{
		if(ClickedFire)
			m_DrawEditor.OnPlayerFire();
		return;
	}

	// F-DDrace
	vec2 ProjStartPos = m_Pos+TempDirection*GetProximityRadius()*0.75f;

	float Spread[] = { 0, -0.1f, 0.1f, -0.2f, 0.2f, -0.3f, 0.3f, -0.4f, 0.4f };
	if (Config()->m_SvNumSpreadShots % 2 == 0)
		for (unsigned int i = 0; i < (sizeof(Spread)/sizeof(*Spread)); i++)
			Spread[i] += 0.05f;

	int NumShots = m_aSpreadWeapon[GetActiveWeapon()] ? Config()->m_SvNumSpreadShots : 1;
	if (m_pPlayer->IsMinigame() || (GetActiveWeapon() == WEAPON_SHOTGUN && m_pPlayer->m_Gamemode == GAMEMODE_VANILLA))
		NumShots = 1;
	bool Sound = true;

	for (int i = 0; i < NumShots; i++)
	{
		float Angle = GetAngle(TempDirection);
		Angle += Spread[i];
		vec2 Direction = vec2(cosf(Angle), sinf(Angle));

		switch (GetActiveWeapon())
		{
			case WEAPON_HAMMER:
			{
				// 4 x 3 = 12 (reachable tiles x (game layer, front layer, switch layer))
				CDoor* apDoors[12];
				int NumDoors = GameWorld()->FindEntities(ProjStartPos, GetProximityRadius(), (CEntity * *)apDoors, 12, CGameWorld::ENTTYPE_DOOR);
				for (int i = 0; i < NumDoors; i++)
				{
					CDoor* pDoor = apDoors[i];

					// number 0 can't be opened and also functions as plot walls that can't be opened
					// plotid -1: map objects
					// plotid > 0: plot doors
					if (pDoor->m_Number == 0 ||
						(!m_DoorHammer && (pDoor->m_PlotID == -1 || (pDoor->m_PlotID >= PLOT_START && pDoor->m_PlotID != GameServer()->GetPlotID(m_pPlayer->GetAccID())))))
						continue;

					if (Team() != TEAM_SUPER && GameServer()->Collision()->m_pSwitchers)
					{
						bool Status = GameServer()->Collision()->m_pSwitchers[pDoor->m_Number].m_Status[Team()];
						if (pDoor->m_PlotID > 0)
						{
							GameServer()->SetPlotDoorStatus(pDoor->m_PlotID, !Status);
						}
						else
						{
							GameServer()->Collision()->m_pSwitchers[pDoor->m_Number].m_Status[Team()] = !Status;
							GameServer()->Collision()->m_pSwitchers[pDoor->m_Number].m_EndTick[Team()] = 0;
							GameServer()->Collision()->m_pSwitchers[pDoor->m_Number].m_Type[Team()] = Status ? TILE_SWITCHCLOSE : TILE_SWITCHOPEN;
						}
					}
				}

				// reset objects Hit
				m_NumObjectsHit = 0;
				if (Sound)
					GameServer()->CreateSound(m_Pos, SOUND_HAMMER_FIRE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));

				if (m_Hit & DISABLE_HIT_HAMMER) break;

				CCharacter* apEnts[MAX_CLIENTS];
				int Hits = 0;
				int Num = GameWorld()->FindEntities(ProjStartPos, GetProximityRadius() * 0.5f, (CEntity * *)apEnts,
					MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

				for (int i = 0; i < Num; ++i)
				{
					CCharacter* pTarget = apEnts[i];

					if ((pTarget == this || (pTarget->IsAlive() && !CanCollide(pTarget->GetPlayer()->GetCID()))))
						continue;

					// set his velocity to fast upward (for now)
					if (length(pTarget->m_Pos - ProjStartPos) > 0.0f)
						GameServer()->CreateHammerHit(pTarget->m_Pos - normalize(pTarget->m_Pos - ProjStartPos) * GetProximityRadius() * 0.5f, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
					else
						GameServer()->CreateHammerHit(ProjStartPos, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));

					// police catch gangster
					if (GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_PoliceLevel &&
						pTarget->m_FreezeTime && !pTarget->GetPlayer()->IsMinigame() && pTarget->GetPlayer()->m_EscapeTime)
					{
						int TargetCID = pTarget->GetPlayer()->GetCID();

						char aBuf[256];
						int Minutes = clamp((int)(pTarget->GetPlayer()->m_EscapeTime / Server()->TickSpeed()) / 100, 10, 20);
						int Corrupt = clamp(pTarget->GetPlayer()->m_SpawnBlockScore * 500, 500, 10000);
						if (pTarget->GetPlayer()->GetAccID() >= ACC_START && GameServer()->m_Accounts[pTarget->GetPlayer()->GetAccID()].m_Money > Corrupt)
						{
							str_format(aBuf, sizeof(aBuf), "corrupted officer '%s'", Server()->ClientName(m_pPlayer->GetCID()));
							pTarget->GetPlayer()->BankTransaction(-Corrupt, aBuf);
							str_format(aBuf, sizeof(aBuf), "corrupted by gangster '%s'", Server()->ClientName(TargetCID));
							m_pPlayer->BankTransaction(Corrupt, aBuf);
							str_format(aBuf, sizeof(aBuf), "You paid %d money to '%s' to reduce your jailtime by 5 minutes", Corrupt, Server()->ClientName(m_pPlayer->GetCID()));
							GameServer()->SendChatTarget(TargetCID, aBuf);
							str_format(aBuf, sizeof(aBuf), "You got %d money from '%s' to reduce his jailtime by 5 minutes", Corrupt, Server()->ClientName(TargetCID));
							GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
							Minutes -= 5;
						}
						str_format(aBuf, sizeof(aBuf), "You caught the gangster '%s' (%d minutes arrest)", Server()->ClientName(TargetCID), Minutes);
						GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);

						str_format(aBuf, sizeof(aBuf), "You were arrested for %d minutes by '%s'", Minutes, Server()->ClientName(m_pPlayer->GetCID()));
						GameServer()->SendChatTarget(TargetCID, aBuf);
						GameServer()->JailPlayer(TargetCID, Minutes * 60); // minimum 5 maximum 20 minutes jail
					}
					else
					{
						vec2 Dir;
						if (length(pTarget->m_Pos - m_Pos) > 0.0f)
							Dir = normalize(pTarget->m_Pos - m_Pos);
						else
							Dir = vec2(0.f, -1.f);

						float Strength;
						if (!m_TuneZone)
							Strength = GameServer()->Tuning()->m_HammerStrength;
						else
							Strength = GameServer()->TuningList()[m_TuneZone].m_HammerStrength;

						vec2 Temp = pTarget->m_Core.m_Vel + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f;
						Temp = ClampVel(pTarget->m_MoveRestrictions, Temp);
						Temp -= pTarget->m_Core.m_Vel;

						pTarget->TakeDamage((vec2(0.f, -1.0f) + Temp) * Strength, Dir * -1, g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage,
							m_pPlayer->GetCID(), GetActiveWeapon());

						pTarget->UnFreeze();

						if (m_FreezeHammer)
							pTarget->Freeze();
					}

					Hits++;
				}

				// if we Hit anything, we have to wait for the reload
				if (Hits)
					m_ReloadTimer = Server()->TickSpeed() / 3;

			} break;

			case WEAPON_GUN:
			{
				if (!m_Jetpack || !m_pPlayer->m_NinjaJetpack)
				{
					if (m_pPlayer->m_SpookyGhost)
					{
						new CCustomProjectile
						(
							GameWorld(),
							m_pPlayer->GetCID(),	//owner
							ProjStartPos,			//pos
							Direction,				//dir
							false,					//freeze
							false,					//explosive
							false,					//unfreeze
							true,					//bloody
							true,					//ghost
							true,					//spooky
							WEAPON_GUN				//type
						);
					}
					else
					{
						int Lifetime;
						if (!m_TuneZone)
							Lifetime = (int)(Server()->TickSpeed() * (m_pPlayer->m_Gamemode == GAMEMODE_VANILLA ? GameServer()->Tuning()->m_VanillaGunLifetime : GameServer()->Tuning()->m_GunLifetime));
						else
							Lifetime = (int)(Server()->TickSpeed() * (m_pPlayer->m_Gamemode == GAMEMODE_VANILLA ? GameServer()->TuningList()[m_TuneZone].m_VanillaGunLifetime : GameServer()->TuningList()[m_TuneZone].m_GunLifetime));

						new CProjectile
						(
							GameWorld(),
							WEAPON_GUN,//Type
							m_pPlayer->GetCID(),//Owner
							ProjStartPos,//Pos
							Direction,//Dir
							Lifetime,//Span
							false,//Freeze
							false,//Explosive
							0,//Force
							-1,//SoundImpact
							0,
							0,
							m_pPlayer->m_SpookyGhost
						);
					}

					if (Sound)
						GameServer()->CreateSound(m_Pos, m_pPlayer->m_SpookyGhost ? SOUND_PICKUP_HEALTH : (m_Jetpack && !Config()->m_SvOldJetpackSound) ? SOUND_HOOK_LOOP : SOUND_GUN_FIRE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
				}
			} break;

			case WEAPON_SHOTGUN:
			{
				if (m_pPlayer->m_Gamemode == GAMEMODE_VANILLA)
				{
					int ShotSpread = 2;

					for (int i = -ShotSpread; i <= ShotSpread; ++i)
					{
						float Spreading[] = { -0.185f, -0.070f, 0, 0.070f, 0.185f };
						float a = GetAngle(Direction);
						a += Spreading[i + 2];
						float v = 1 - (absolute(i) / (float)ShotSpread);
						float Speed = mix((float)GameServer()->Tuning()->m_ShotgunSpeeddiff, 1.0f, v);
						new CProjectile(GameWorld(), WEAPON_SHOTGUN,
							m_pPlayer->GetCID(),
							ProjStartPos,
							vec2(cosf(a), sinf(a)) * Speed,
							(int)(Server()->TickSpeed() * GameServer()->Tuning()->m_ShotgunLifetime),
							false, false, 0, -1, 0, 0, false);
					}
				}
				else
				{
					float LaserReach;
					if (!m_TuneZone)
						LaserReach = GameServer()->Tuning()->m_LaserReach;
					else
						LaserReach = GameServer()->TuningList()[m_TuneZone].m_LaserReach;

					new CLaser(GameWorld(), m_Pos, Direction, LaserReach, m_pPlayer->GetCID(), WEAPON_SHOTGUN);
				}
				if (Sound)
					GameServer()->CreateSound(m_Pos, SOUND_SHOTGUN_FIRE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
			} break;

			case WEAPON_STRAIGHT_GRENADE:
			case WEAPON_GRENADE:
			{
				int Lifetime;
				if (!m_TuneZone)
					Lifetime = (int)(Server()->TickSpeed() * (GetActiveWeapon() == WEAPON_STRAIGHT_GRENADE ? GameServer()->Tuning()->m_StraightGrenadeLifetime : GameServer()->Tuning()->m_GrenadeLifetime));
				else
					Lifetime = (int)(Server()->TickSpeed() * (GetActiveWeapon() == WEAPON_STRAIGHT_GRENADE ? GameServer()->TuningList()[m_TuneZone].m_StraightGrenadeLifetime : GameServer()->TuningList()[m_TuneZone].m_GrenadeLifetime));

				new CProjectile
				(
					GameWorld(),
					GetActiveWeapon(),//Type
					m_pPlayer->GetCID(),//Owner
					ProjStartPos,//Pos
					Direction,//Dir
					Lifetime,//Span
					false,//Freeze
					true,//Explosive
					0,//Force
					SOUND_GRENADE_EXPLODE,//SoundImpact
					0,//Layer
					0,//Number
					false//Spooky
				);

				if (Sound)
					GameServer()->CreateSound(m_Pos, SOUND_GRENADE_FIRE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
			} break;

			case WEAPON_TASER:
			case WEAPON_LASER:
			{
				float LaserReach;
				if (!m_TuneZone)
					LaserReach = GameServer()->Tuning()->m_LaserReach;
				else
					LaserReach = GameServer()->TuningList()[m_TuneZone].m_LaserReach;

				new CLaser(GameWorld(), m_Pos, Direction, LaserReach, m_pPlayer->GetCID(), GetActiveWeapon());
				if (Sound)
					GameServer()->CreateSound(m_Pos, SOUND_LASER_FIRE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
			} break;

			case WEAPON_NINJA:
			{
				// reset Hit objects
				m_NumObjectsHit = 0;

				m_Ninja.m_ActivationDir = Direction;
				m_Ninja.m_CurrentMoveTime = g_pData->m_Weapons.m_Ninja.m_Movetime * Server()->TickSpeed() / 1000;
				m_Ninja.m_OldVelAmount = length(m_Core.m_Vel);

				if (Sound)
					GameServer()->CreateSound(m_Pos, SOUND_NINJA_FIRE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
			} break;

			case WEAPON_PLASMA_RIFLE:
			{
				new CCustomProjectile
				(
					GameWorld(),
					m_pPlayer->GetCID(),	//owner
					ProjStartPos,			//pos
					Direction,				//dir
					false,					//freeze
					true,					//explosive
					true,					//unfreeze
					false,					//bloody
					false,					//ghost
					false,					//spooky
					WEAPON_PLASMA_RIFLE	//type
				);
				if (Sound)
					GameServer()->CreateSound(m_Pos, SOUND_LASER_FIRE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
			} break;

			case WEAPON_HEART_GUN:
			{
				new CCustomProjectile
				(
					GameWorld(),
					m_pPlayer->GetCID(),	//owner
					ProjStartPos,			//pos
					Direction,				//dir
					false,					//freeze
					false,					//explosive
					false,					//unfreeze
					m_pPlayer->m_SpookyGhost,//bloody
					m_pPlayer->m_SpookyGhost,//ghost
					m_pPlayer->m_SpookyGhost,//spooky
					WEAPON_HEART_GUN		//type
				);
				if (Sound)
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
			} break;

			case WEAPON_TELEKINESIS:
			{
				bool TelekinesisSound = false;

				if (!m_pTelekinesisEntity)
				{
					int Types = (1<<CGameWorld::ENTTYPE_CHARACTER) | (1<<CGameWorld::ENTTYPE_FLAG) | (1<<CGameWorld::ENTTYPE_PICKUP_DROP) | (1<<CGameWorld::ENTTYPE_MONEY);
					CEntity *pEntity = GameWorld()->ClosestEntityTypes(m_CursorPos, 20.f, Types, this, m_pPlayer->GetCID(), !m_Passive);

					CCharacter *pChr = 0;
					CFlag *pFlag = 0;
					if (pEntity)
					{
						switch (pEntity->GetObjType())
						{
						case CGameWorld::ENTTYPE_CHARACTER: pChr = (CCharacter *)pEntity; break;
						case CGameWorld::ENTTYPE_FLAG: pFlag = (CFlag *)pEntity; break;
						}
					}

					if ((pChr && pChr->GetPlayer()->GetCID() != m_pPlayer->GetCID() && pChr->m_pTelekinesisEntity != this) || (pEntity && pEntity != pChr))
					{
						bool IsTelekinesed = false;
						for (int i = 0; i < MAX_CLIENTS; i++)
							if (GameServer()->GetPlayerChar(i) && GameServer()->GetPlayerChar(i)->m_pTelekinesisEntity == pEntity)
							{
								IsTelekinesed = true;
								break;
							}

						if (!IsTelekinesed)
						{
							if (!pFlag || !pFlag->GetCarrier())
							{
								m_pTelekinesisEntity = pEntity;
								TelekinesisSound = true;

								if (pFlag)
									pFlag->SetAtStand(false);
							}

						}
					}
				}
				else
				{
					m_pTelekinesisEntity = 0;
					TelekinesisSound = true;
				}

				if (Sound && TelekinesisSound)
					GameServer()->CreateSound(m_Pos, SOUND_NINJA_HIT, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
			} break;

			case WEAPON_LIGHTSABER:
			{
				if (!m_pLightsaber)
				{
					m_pLightsaber = new CLightsaber(GameWorld(), m_Pos, m_pPlayer->GetCID());
					m_pLightsaber->Extend();
				}
				else
				{
					m_pLightsaber->Retract();
				}
			} break;

			case WEAPON_PORTAL_RIFLE:
			{
				vec2 PortalPos = m_CursorPos;
				bool Found = true;
				if (GameServer()->Collision()->TestBox(m_CursorPos, vec2(CCharacterCore::PHYS_SIZE, CCharacterCore::PHYS_SIZE)))
					Found = GetNearestAirPos(m_CursorPos, m_Pos, &PortalPos);

				bool PlotDoorOnly = GameServer()->GetTilePlotID(m_Pos) < PLOT_START && GameServer()->GetTilePlotID(PortalPos) < PLOT_START;

				if (!Found || !PortalPos
					|| distance(PortalPos, m_Pos) > Config()->m_SvPortalMaxDistance
					|| (m_pPlayer->m_pPortal[PORTAL_FIRST] && m_pPlayer->m_pPortal[PORTAL_SECOND])
					|| (m_LastLinkedPortals + Server()->TickSpeed() * Config()->m_SvPortalRifleDelay > Server()->Tick())
					|| GameServer()->Collision()->IntersectLinePortalRifleStop(m_Pos, PortalPos, 0, 0)
					|| GameServer()->IntersectedLineDoor(m_Pos, PortalPos, Team(), PlotDoorOnly)
					|| GameWorld()->ClosestCharacter(PortalPos, Config()->m_SvPortalRadius, 0, m_pPlayer->GetCID(), false) // dont allow to place portals too close to other tees
					)
				{
					GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
					return;
				}

				for (int i = 0; i < NUM_PORTALS; i++)
				{
					if (!m_pPlayer->m_pPortal[i])
					{
						m_pPlayer->m_pPortal[i] = new CPortal(GameWorld(), PortalPos, m_pPlayer->GetCID());
						if (i == PORTAL_SECOND)
						{
							m_pPlayer->m_pPortal[PORTAL_FIRST]->SetLinkedPortal(m_pPlayer->m_pPortal[PORTAL_SECOND]);
							m_pPlayer->m_pPortal[PORTAL_SECOND]->SetLinkedPortal(m_pPlayer->m_pPortal[PORTAL_FIRST]);
							m_LastLinkedPortals = Server()->Tick();
						}
						break;
					}
				}

			} break;

			case WEAPON_PROJECTILE_RIFLE:
			{
				int Lifetime;
				if (!m_TuneZone)
					Lifetime = (int)(Server()->TickSpeed() * GameServer()->Tuning()->m_GunLifetime);
				else
					Lifetime = (int)(Server()->TickSpeed() * GameServer()->TuningList()[m_TuneZone].m_GunLifetime);

				new CProjectile
				(
					GameWorld(),
					WEAPON_PROJECTILE_RIFLE,//Type
					m_pPlayer->GetCID(),//Owner
					ProjStartPos,//Pos
					Direction,//Dir
					Lifetime,//Span
					false,//Freeze
					true,//Explosive
					0,//Force
					SOUND_GRENADE_EXPLODE,//SoundImpact
					0,
					0,
					m_pPlayer->m_SpookyGhost
				);

				if (Sound)
					GameServer()->CreateSound(m_Pos, SOUND_HOOK_LOOP, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
			} break;

			case WEAPON_BALL_GRENADE:
			{
				int Lifetime;
				if (!m_TuneZone)
					Lifetime = (int)(Server()->TickSpeed() * GameServer()->Tuning()->m_GrenadeLifetime);
				else
					Lifetime = (int)(Server()->TickSpeed() * GameServer()->TuningList()[m_TuneZone].m_GrenadeLifetime);

				for (int i = 0; i < 7; i++)
				{
					new CProjectile
					(
						GameWorld(),
						WEAPON_BALL_GRENADE,//Type
						m_pPlayer->GetCID(),//Owner
						ProjStartPos,//Pos
						Direction,//Dir
						Lifetime,//Span
						false,//Freeze
						i < 3,//Explosive
						0,//Force
						i == 0 ? SOUND_GRENADE_EXPLODE : -1//SoundImpact
					);
				}

				if (Sound)
					GameServer()->CreateSound(m_Pos, SOUND_GRENADE_FIRE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
			} break;
		}

		Sound = false;
	}

	if (GetActiveWeapon() != WEAPON_LIGHTSABER) // we don't want the client to render the fire animation
		m_AttackTick = Server()->Tick();

	if(m_aWeapons[GetActiveWeapon()].m_Ammo > 0) // -1 == unlimited
	{
		m_aWeapons[GetActiveWeapon()].m_Ammo--;

		if (GetActiveWeapon() == WEAPON_TASER && m_pPlayer->GetAccID() >= ACC_START)
		{
			GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_TaserBattery--;
			UpdateWeaponIndicator();
		}

		int W = GetSpawnWeaponIndex(GetActiveWeapon());
		if (W != -1 && m_aSpawnWeaponActive[W] && m_aWeapons[GetActiveWeapon()].m_Ammo == 0)
			GiveWeapon(GetActiveWeapon(), true);
	}

	//spooky ghost
	if (m_pPlayer->m_PlayerFlags&PLAYERFLAG_SCOREBOARD && GameServer()->GetWeaponType(GetActiveWeapon()) == WEAPON_GUN)
	{
		if (CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		{
			m_NumGhostShots++;
			if ((m_pPlayer->m_HasSpookyGhost || GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_SpookyGhost) && m_NumGhostShots == 2 && !m_pPlayer->m_SpookyGhost)
			{
				SetSpookyGhost();
				m_NumGhostShots = 0;
			}
			else if (m_NumGhostShots == 2 && m_pPlayer->m_SpookyGhost)
			{
				UnsetSpookyGhost();
				m_NumGhostShots = 0;
			}
		}
	}

	if (!m_ReloadTimer)
	{
		float FireDelay;
		if (!m_TuneZone)
			GameServer()->Tuning()->Get(OLD_TUNES + GetActiveWeapon(), &FireDelay);
		else
			GameServer()->TuningList()[m_TuneZone].Get(OLD_TUNES + GetActiveWeapon(), &FireDelay);
		m_ReloadTimer = FireDelay * Server()->TickSpeed() / 1000;
	}
}

void CCharacter::HandleWeapons()
{
	//ninja
	HandleNinja();
	HandleJetpack();

	if (m_PainSoundTimer > 0)
		m_PainSoundTimer--;

	// check reload timer
	if(m_ReloadTimer)
	{
		m_ReloadTimer--;
		return;
	}

	// fire Weapon, if wanted
	FireWeapon();

	// ammo regen
	int AmmoRegenTime = g_pData->m_Weapons.m_aId[GetActiveWeapon()].m_Ammoregentime;
	if (GetActiveWeapon() == WEAPON_HEART_GUN || GetActiveWeapon() == WEAPON_PROJECTILE_RIFLE)
		AmmoRegenTime = g_pData->m_Weapons.m_aId[WEAPON_GUN].m_Ammoregentime;
	if (AmmoRegenTime && m_pPlayer->m_Gamemode == GAMEMODE_VANILLA && !m_FreezeTime && m_aWeapons[GetActiveWeapon()].m_Ammo != -1)
	{
		// If equipped and not active, regen ammo?
		if (m_ReloadTimer <= 0)
		{
			if (m_aWeapons[GetActiveWeapon()].m_AmmoRegenStart < 0)
				m_aWeapons[GetActiveWeapon()].m_AmmoRegenStart = Server()->Tick();

			if ((Server()->Tick() - m_aWeapons[GetActiveWeapon()].m_AmmoRegenStart) >= AmmoRegenTime * Server()->TickSpeed() / 1000)
			{
				// Add some ammo
				m_aWeapons[GetActiveWeapon()].m_Ammo = min(m_aWeapons[GetActiveWeapon()].m_Ammo + 1, 10);
				m_aWeapons[GetActiveWeapon()].m_AmmoRegenStart = -1;
			}
		}
		else
		{
			m_aWeapons[GetActiveWeapon()].m_AmmoRegenStart = -1;
		}
	}

	return;
}

void CCharacter::GiveWeapon(int Weapon, bool Remove, int Ammo, bool PortalRifleByAcc)
{
	if (m_InitializedSpawnWeapons)
	{
		int W = GetSpawnWeaponIndex(Weapon);
		if (W != -1)
			m_aSpawnWeaponActive[W] = false;
	}

	if (Weapon == WEAPON_TASER && !Remove)
	{
		if (!m_aWeapons[WEAPON_LASER].m_Got
			|| m_aSpawnWeaponActive[GetSpawnWeaponIndex(WEAPON_LASER)]
			|| GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_TaserLevel < 1
			|| m_pPlayer->IsMinigame())
			return;
	}

	for (int i = 0; i < NUM_BACKUPS; i++)
	{
		m_aWeaponsBackupGot[Weapon][i] = !Remove;
		m_aWeaponsBackup[Weapon][i] = Ammo;
	}

	if (m_pPlayer->m_SpookyGhost && GameServer()->GetWeaponType(Weapon) != WEAPON_GUN)
		return;

	if (Weapon == WEAPON_PORTAL_RIFLE && !PortalRifleByAcc)
		m_CollectedPortalRifle = !Remove;

	if (Weapon == WEAPON_NINJA)
	{
		if (Remove)
			RemoveNinja();
		else
			GiveNinja();
		return;
	}

	m_aWeapons[Weapon].m_Got = !Remove;

	if (Remove)
	{
		if (GetActiveWeapon() == Weapon)
			SetWeapon(WEAPON_GUN);
	}
	else
	{
		m_aWeapons[Weapon].m_Ammo = Ammo;
	}

	if (Weapon == WEAPON_LASER)
		GiveWeapon(WEAPON_TASER, Remove, GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_TaserBattery);
}

void CCharacter::GiveAllWeapons()
{
	for (int i = WEAPON_GUN; i < NUM_WEAPONS; i++)
		if (i != WEAPON_NINJA)
			GiveWeapon(i);
}

void CCharacter::GiveNinja()
{
	for (int i = 0; i < NUM_BACKUPS; i++)
	{
		m_aWeaponsBackupGot[WEAPON_NINJA][i] = true;
		m_aWeaponsBackup[WEAPON_NINJA][i] = -1;
	}

	m_aWeapons[WEAPON_NINJA].m_Ammo = -1;
	m_aWeapons[WEAPON_NINJA].m_Got = true;

	if (m_ScrollNinja)
		return;

	m_Ninja.m_ActivationTick = Server()->Tick();
	if (GetActiveWeapon() != WEAPON_NINJA)
		m_LastWeapon = GetActiveWeapon();
	SetActiveWeapon(WEAPON_NINJA);

	if (!m_aWeapons[WEAPON_NINJA].m_Got && m_pPlayer->m_Gamemode == GAMEMODE_VANILLA)
		GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
}

void CCharacter::RemoveNinja()
{
	m_Ninja.m_CurrentMoveTime = 0;
	if (GetActiveWeapon() == WEAPON_NINJA)
		SetWeapon(m_LastWeapon);
	m_aWeapons[WEAPON_NINJA].m_Got = false;
	for (int i = 0; i < NUM_BACKUPS; i++)
		m_aWeaponsBackupGot[WEAPON_NINJA][i] = false;
}

void CCharacter::SetEmote(int Emote, int Tick)
{
	m_EmoteType = Emote;
	m_EmoteStop = Tick;
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
{
	if (m_DrawEditor.Active())
	{
		m_DrawEditor.OnInput(pNewInput);
		pNewInput->m_Direction = 0;
		pNewInput->m_Jump = 0;
		pNewInput->m_Hook = 0;
	}

	// check for changes
	if(mem_comp(&m_SavedInput, pNewInput, sizeof(CNetObj_PlayerInput)) != 0)
		m_LastAction = Server()->Tick();

	// copy new input
	mem_copy(&m_Input, pNewInput, sizeof(m_Input));
	m_NumInputs++;

	// it is not allowed to aim in the center
	if(m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		m_Input.m_TargetY = -1;

	mem_copy(&m_SavedInput, &m_Input, sizeof(m_SavedInput));
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));

	// it is not allowed to aim in the center
	if(m_LatestInput.m_TargetX == 0 && m_LatestInput.m_TargetY == 0)
		m_LatestInput.m_TargetY = -1;

	if(m_NumInputs > 2 && m_pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		HandleWeaponSwitch();
		FireWeapon();
	}

	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

void CCharacter::ResetInput()
{
	m_Input.m_Direction = 0;
	//m_Input.m_Hook = 0;
	// simulate releasing the fire button
	if((m_Input.m_Fire&1) != 0)
		m_Input.m_Fire++;
	m_Input.m_Fire &= INPUT_STATE_MASK;
	m_Input.m_Jump = 0;
	m_LatestPrevInput = m_LatestInput = m_SavedInput = m_Input;
}

void CCharacter::Tick()
{
	if(m_Paused)
		return;

	// F-DDrace
	if (MinigameRequestTick())
		return; // We need to return here because in SetMinigame() the player is killed already

	DummyTick();
	FDDraceTick();
	HandleLastIndexTiles();

	DDraceTick();

	// F-DDrace
	for (int i = 0; i < 2; i++)
	{
		CFlag* F = ((CGameControllerDDRace*)GameServer()->m_pController)->m_apFlags[i];
		if (F) m_Core.SetFlagInfo(i, F->GetPos(), F->IsAtStand(), F->GetVel(), F->GetCarrier());
	}

	m_Core.m_Input = m_Input;
	m_Core.Tick(true);

	// F-DDrace
	if (m_Core.m_UpdateFlagVel == HOOK_FLAG_RED || m_Core.m_UpdateFlagVel == HOOK_FLAG_BLUE)
	{
		int Flag = m_Core.m_UpdateFlagVel == HOOK_FLAG_RED ? TEAM_RED : TEAM_BLUE;
		((CGameControllerDDRace*)GameServer()->m_pController)->m_apFlags[Flag]->SetVel(m_Core.m_UFlagVel);
	}

	if (m_Core.m_UpdateFlagAtStand == HOOK_FLAG_RED || m_Core.m_UpdateFlagAtStand == HOOK_FLAG_BLUE)
	{
		int Flag = m_Core.m_UpdateFlagAtStand == HOOK_FLAG_RED ? TEAM_RED : TEAM_BLUE;
		((CGameControllerDDRace*)GameServer()->m_pController)->m_apFlags[Flag]->SetAtStand(false);
	}

	// handle Weapons
	HandleWeapons();

	DDracePostCoreTick();

	m_PrevPos = m_Core.m_Pos;
}

void CCharacter::TickDefered()
{
	static const vec2 ColBox(CCharacterCore::PHYS_SIZE, CCharacterCore::PHYS_SIZE);
	// advance the dummy
	{
		CWorldCore TempWorld;
		m_ReckoningCore.Init(&TempWorld, GameServer()->Collision(), &Teams()->m_Core, &((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts);
		m_ReckoningCore.m_Id = m_pPlayer->GetCID();
		m_ReckoningCore.Tick(false);
		m_ReckoningCore.Move();
		m_ReckoningCore.Quantize();
	}

	// apply drag velocity when the player is not firing ninja
	// and set it back to 0 for the next tick
	if(m_ActiveWeapon != WEAPON_NINJA || m_Ninja.m_CurrentMoveTime < 0)
		m_Core.AddDragVelocity();
	m_Core.ResetDragVelocity();

	//lastsentcore
	vec2 StartPos = m_Core.m_Pos;
	vec2 StartVel = m_Core.m_Vel;
	bool StuckBefore = GameServer()->Collision()->TestBox(m_Core.m_Pos, ColBox);

	m_Core.m_Id = m_pPlayer->GetCID();
	m_Core.Move();

	bool StuckAfterMove = GameServer()->Collision()->TestBox(m_Core.m_Pos, ColBox);
	m_Core.Quantize();
	bool StuckAfterQuant = GameServer()->Collision()->TestBox(m_Core.m_Pos, ColBox);
	m_Pos = m_Core.m_Pos;

	if(!StuckBefore && (StuckAfterMove || StuckAfterQuant))
	{
		// Hackish solution to get rid of strict-aliasing warning
		union
		{
			float f;
			unsigned u;
		}StartPosX, StartPosY, StartVelX, StartVelY;

		StartPosX.f = StartPos.x;
		StartPosY.f = StartPos.y;
		StartVelX.f = StartVel.x;
		StartVelY.f = StartVel.y;

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x",
			StuckBefore,
			StuckAfterMove,
			StuckAfterQuant,
			StartPos.x, StartPos.y,
			StartVel.x, StartVel.y,
			StartPosX.u, StartPosY.u,
			StartVelX.u, StartVelY.u);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	m_TriggeredEvents |= m_Core.m_TriggeredEvents;

	// F-DDrace
	int Events = m_Core.m_TriggeredEvents;

	{
		int CID = m_pPlayer->GetCID();

		// Some sounds are triggered client-side for the acting player so we need to avoid duplicating them
		// hook sounds are clientside in 0.7, thats why we pass true here, to have sevendown only
		Mask128 TeamMask = Teams()->TeamMask(Team(), -1, CID, true);
		Mask128 TeamMaskExceptSelf = Teams()->TeamMask(Team(), CID, CID, true);

		if(Events&COREEVENTFLAG_GROUND_JUMP) GameServer()->CreateSound(m_Pos, SOUND_PLAYER_JUMP, TeamMaskExceptSelf);
		if(Events&COREEVENTFLAG_HOOK_ATTACH_PLAYER) GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_PLAYER, TeamMask);
		if(Events&COREEVENTFLAG_HOOK_ATTACH_GROUND) GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_GROUND, TeamMaskExceptSelf);
		if(Events&COREEVENTFLAG_HOOK_HIT_NOHOOK) GameServer()->CreateSound(m_Pos, SOUND_HOOK_NOATTACH, TeamMaskExceptSelf);
	}

	if(Events&COREEVENTFLAG_HOOK_ATTACH_FLAG)
	{
		GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_PLAYER, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
		m_TriggeredEvents &= ~COREEVENTFLAG_HOOK_ATTACH_FLAG;
	}

	if(Events&COREEVENTFLAG_HOOK_ATTACH_PLAYER)
		OnPlayerHook();

	if(m_pPlayer->GetTeam() == TEAM_SPECTATORS)
	{
		m_Pos.x = m_Input.m_TargetX;
		m_Pos.y = m_Input.m_TargetY;
	}

	// update the m_SendCore if needed
	{
		CNetObj_Character Predicted;
		CNetObj_Character Current;
		mem_zero(&Predicted, sizeof(Predicted));
		mem_zero(&Current, sizeof(Current));
		m_ReckoningCore.Write(&Predicted);
		m_Core.Write(&Current);

		// only allow dead reackoning for a top of 3 seconds
		if(m_ReckoningTick+Server()->TickSpeed()*3 < Server()->Tick() || mem_comp(&Predicted, &Current, sizeof(CNetObj_Character)) != 0)
		{
			m_ReckoningTick = Server()->Tick();
			m_SendCore = m_Core;
			m_ReckoningCore = m_Core;
		}
	}
}

void CCharacter::TickPaused()
{
	++m_AttackTick;
	++m_Ninja.m_ActivationTick;
	++m_ReckoningTick;
	if(m_LastAction != -1)
		++m_LastAction;
	if(m_aWeapons[GetActiveWeapon()].m_AmmoRegenStart > -1)
		++m_aWeapons[GetActiveWeapon()].m_AmmoRegenStart;
	if(m_EmoteStop > -1)
		++m_EmoteStop;
}

bool CCharacter::IncreaseHealth(int Amount)
{
	if(m_Health >= 10)
		return false;
	m_Health = clamp(m_Health+Amount, 0, 10);
	return true;
}

bool CCharacter::IncreaseArmor(int Amount)
{
	if(m_Armor >= 10)
		return false;
	m_Armor = clamp(m_Armor+Amount, 0, 10);
	return true;
}

void CCharacter::Die(int Weapon, bool UpdateTeeControl)
{
	// make sure we are not saved as killer for someone else after we joined a minigame, so we cant take the flag to a minigame
	if (Weapon == WEAPON_MINIGAME_CHANGE)
		GameServer()->UnsetKiller(m_pPlayer->GetCID());

	m_DrawEditor.OnPlayerDeath();

	// drop armor, hearts and weapons
	DropLoot(Weapon);

	// reset if killed by the game or if not killed by a player or deathtile while unfrozen
	if (Weapon == WEAPON_MINIGAME_CHANGE || Weapon == WEAPON_GAME || (!m_FreezeTime && Weapon != WEAPON_PLAYER && Weapon != WEAPON_WORLD))
	{
		m_Core.m_Killer.m_ClientID = -1;
		m_Core.m_Killer.m_Weapon = -1;
	}

	bool CountKill = true;
	if (GameServer()->Collision()->m_pSwitchers && m_LastTouchedSwitcher != -1)
	{
		if (GameServer()->Collision()->m_pSwitchers[m_LastTouchedSwitcher].m_ClientID[Team()] == m_Core.m_Killer.m_ClientID)
			CountKill = false;
	}
	if (m_LastTouchedPortalBy == m_Core.m_Killer.m_ClientID)
		CountKill = false;
	if (GameServer()->SameIP(m_pPlayer->GetCID(), m_Core.m_Killer.m_ClientID))
		CountKill = false;

	// if no killer exists its a selfkill
	if (m_Core.m_Killer.m_ClientID == -1)
		m_Core.m_Killer.m_ClientID = m_pPlayer->GetCID();

	// dont set a weapon if we dont have a tee for it
	int Killer = m_Core.m_Killer.m_ClientID;
	if (Killer != m_pPlayer->GetCID() && (Weapon >= 0 || Weapon == WEAPON_PLAYER))
		Weapon = m_Core.m_Killer.m_Weapon;

	// unset anyones telekinesis on us
	GameServer()->UnsetTelekinesis(this);

	// update tee controlling
	if (UpdateTeeControl)
	{
		m_pPlayer->ResumeFromTeeControl();
		if (m_pPlayer->m_TeeControllerID != -1)
			GameServer()->m_apPlayers[m_pPlayer->m_TeeControllerID]->ResumeFromTeeControl();
	}

	CPlayer *pKiller = (Killer >= 0 && Killer != m_pPlayer->GetCID() && GameServer()->m_apPlayers[Killer]) ? GameServer()->m_apPlayers[Killer] : 0;

	// account kills and deaths
	if (pKiller)
	{
		// killing spree
		char aBuf[128];
		bool IsBlock = !pKiller->IsMinigame() || pKiller->m_Minigame == MINIGAME_BLOCK;
		CCharacter* pKillerChar = pKiller->GetCharacter();
		if (CountKill && pKillerChar && (!m_pPlayer->m_IsDummy || Config()->m_SvDummyBlocking))
		{
			pKillerChar->m_KillStreak++;
			if (pKillerChar->m_KillStreak % 5 == 0)
			{
				str_format(aBuf, sizeof(aBuf), "%s is on a killing spree with %d %s", Server()->ClientName(Killer), pKillerChar->m_KillStreak, IsBlock ? "blocks" : "kills");
				GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
			}
		}

		if (m_KillStreak >= 5)
		{
			str_format(aBuf, sizeof(aBuf), "%s's killing spree was ended by %s (%d %s)", Server()->ClientName(m_pPlayer->GetCID()), Server()->ClientName(Killer), m_KillStreak, IsBlock ? "blocks" : "kills");
			GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
			pKiller->GiveXP(250, "end a killing spree");
		}

		if (CountKill && pKiller->GetAccID() >= ACC_START && (!m_pPlayer->m_IsDummy || Config()->m_SvDummyBlocking))
		{
			CGameContext::AccountInfo *pKillerAccount = &GameServer()->m_Accounts[pKiller->GetAccID()];

			// kill streak;
			if (pKillerChar && pKillerChar->m_KillStreak > pKillerAccount->m_KillingSpreeRecord)
				pKillerAccount->m_KillingSpreeRecord = pKillerChar->m_KillStreak;

			if (pKiller->m_Minigame == MINIGAME_SURVIVAL && pKiller->m_SurvivalState > SURVIVAL_LOBBY)
			{
				pKillerAccount->m_SurvivalKills++;
			}
			else if (pKiller->m_Minigame == MINIGAME_INSTAGIB_BOOMFNG || pKiller->m_Minigame == MINIGAME_INSTAGIB_FNG)
			{
				pKillerAccount->m_InstagibKills++;
			}
			else
			{
				pKillerAccount->m_Kills++;

				if (Server()->Tick() >= m_SpawnTick + Server()->TickSpeed() * Config()->m_SvBlockPointsDelay)
					if (!m_pPlayer->m_IsDummy || Config()->m_SvDummyBlocking)
						pKiller->GiveBlockPoints(1);
			}
		}

		if (m_pPlayer->GetAccID() >= ACC_START)
		{
			CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[m_pPlayer->GetAccID()];

			if (m_pPlayer->m_Minigame == MINIGAME_SURVIVAL && m_pPlayer->m_SurvivalState > SURVIVAL_LOBBY)
			{
				pAccount->m_SurvivalDeaths++;
			}
			else if (m_pPlayer->m_Minigame == MINIGAME_INSTAGIB_BOOMFNG || m_pPlayer->m_Minigame == MINIGAME_INSTAGIB_FNG)
			{
				pAccount->m_InstagibDeaths++;
			}
			else
			{
				pAccount->m_Deaths++;
			}
		}

		GameServer()->ProcessSpawnBlockProtection(m_pPlayer->GetCID());
	}

	int ModeSpecial = GameServer()->m_pController->OnCharacterDeath(this, GameServer()->m_apPlayers[Killer], Weapon);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "kill killer='%d:%s' victim='%d:%s' weapon=%d special=%d killer_team:%d victim_team:%d",
		Killer, Server()->ClientName(Killer),
		m_pPlayer->GetCID(), Server()->ClientName(m_pPlayer->GetCID()), Weapon, ModeSpecial,
		GameServer()->m_apPlayers[Killer]->GetTeam(),
		m_pPlayer->GetTeam()
	);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// construct kill message
	CNetMsg_Sv_KillMsg Msg;
	Msg.m_Killer = Killer;
	Msg.m_Victim = m_pPlayer->GetCID();
	Msg.m_Weapon = GameServer()->GetWeaponType(Weapon);
	Msg.m_ModeSpecial = ModeSpecial;

	// send the kill message
	if (!m_pPlayer->m_ShowName || (pKiller && !pKiller->m_ShowName))
	{
		if (pKiller && !pKiller->m_ShowName)
			pKiller->FixForNoName(FIX_SET_NAME_ONLY);

		m_pPlayer->m_KillMsgFix.m_Killer = Msg.m_Killer;
		m_pPlayer->m_KillMsgFix.m_Victim = Msg.m_Victim;
		m_pPlayer->m_KillMsgFix.m_Weapon = Msg.m_Weapon;
		m_pPlayer->m_KillMsgFix.m_ModeSpecial = Msg.m_ModeSpecial;
		m_pPlayer->FixForNoName(FIX_KILL_MSG);
	}
	else
	{
		// only send kill message to players in the same minigame
		for (int i = 0; i < MAX_CLIENTS; i++)
			if (GameServer()->m_apPlayers[i] && (!Config()->m_SvHideMinigamePlayers || (m_pPlayer->m_Minigame == GameServer()->m_apPlayers[i]->m_Minigame)))
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
	}

	// character doesnt exist, print some messages and set states
	// if the player is in deathmatch mode, or simply playing
	if (GameServer()->m_SurvivalGameState > SURVIVAL_LOBBY && m_pPlayer->m_SurvivalState > SURVIVAL_LOBBY && Killer != WEAPON_GAME)
	{
		// check for players in the current game state
		if (m_pPlayer->GetCID() != GameServer()->m_SurvivalWinner)
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You lost, you can wait for another round or leave the lobby using '/leave'");
		if (GameServer()->CountSurvivalPlayers(GameServer()->m_SurvivalGameState) > 2)
		{
			// if there are more than just two players left, you will watch your killer or a random player
			m_pPlayer->Pause(CPlayer::PAUSE_PAUSED, true);
			m_pPlayer->SetSpectatorID(SPEC_PLAYER, pKiller ? Killer : GameServer()->GetRandomSurvivalPlayer(GameServer()->m_SurvivalGameState, m_pPlayer->GetCID()));

			// update the ones that are watching you
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				CPlayer *pPlayer = GameServer()->m_apPlayers[i];
				if (i == m_pPlayer->GetCID() || !pPlayer || pPlayer->m_Minigame != MINIGAME_SURVIVAL || pPlayer->GetSpectatorID() != m_pPlayer->GetCID())
					continue;
				pPlayer->SetSpectatorID(SPEC_PLAYER, m_pPlayer->GetSpectatorID());
			}

			// printing a message that you died and informing about remaining players
			char aKillMsg[128];
			str_format(aKillMsg, sizeof(aKillMsg), "'%s' died\nAlive players: %d", Server()->ClientName(m_pPlayer->GetCID()), GameServer()->CountSurvivalPlayers(GameServer()->m_SurvivalGameState) -1 /* -1 because we have to exclude the currently dying*/);
			GameServer()->SendSurvivalBroadcast(aKillMsg, true);
		}
		// sending you back to lobby
		m_pPlayer->m_SurvivalState = SURVIVAL_LOBBY;
		m_pPlayer->m_ShowName = true;
		m_pPlayer->m_SurvivalDieTick = Server()->Tick();
	}

	// a nice sound
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));

	// this is for auto respawn after 3 secs
	m_pPlayer->m_PreviousDieTick = m_pPlayer->m_DieTick;
	m_pPlayer->m_DieTick = Server()->Tick();

	m_Alive = false;

	// F-DDrace
	if (m_Passive)
		Passive(false, -1, true);
	UnsetSpookyGhost();

	// unset skin specific stuff
	m_pPlayer->ResetSkin();

	GameWorld()->RemoveEntity(this);
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID(), Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
	Teams()->OnCharacterDeath(m_pPlayer->GetCID(), Weapon);

	// reset gamemode if it got changed by a tile but the setting says something different
	m_pPlayer->m_Gamemode = m_SavedGamemode;
}

bool CCharacter::TakeDamage(vec2 Force, vec2 Source, int Dmg, int From, int Weapon)
{
	if (GameServer()->m_apPlayers[From] && From != m_pPlayer->GetCID())
	{
		m_Core.m_Killer.m_ClientID = From;
		m_Core.m_Killer.m_Weapon = Weapon;
	}

	if (Dmg && m_pPlayer->m_Gamemode == GAMEMODE_VANILLA)
	{
		// m_pPlayer only inflicts half damage on self
		if (From == m_pPlayer->GetCID())
			Dmg = max(1, Dmg / 2);

		int OldHealth = m_Health, OldArmor = m_Armor;
		if (Dmg)
		{
			if (m_Armor)
			{
				if (Dmg > 1)
				{
					m_Health--;
					Dmg--;
				}

				if (Dmg > m_Armor)
				{
					Dmg -= m_Armor;
					m_Armor = 0;
				}
				else
				{
					m_Armor -= Dmg;
					Dmg = 0;
				}
			}

			m_Health -= Dmg;
		}

		// create healthmod indicator
		GameServer()->CreateDamage(m_Pos, m_pPlayer->GetCID(), Source, OldHealth - m_Health, OldArmor - m_Armor, From == m_pPlayer->GetCID());

		// do damage Hit sound
		if(From >= 0 && From != m_pPlayer->GetCID() && GameServer()->m_apPlayers[From])
		{
			Mask128 Mask = CmaskOne(From);
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS && GameServer()->m_apPlayers[i]->GetSpectatorID() == From)
					Mask |= CmaskOne(i);
			}
			GameServer()->CreateSound(GameServer()->m_apPlayers[From]->m_ViewPos, SOUND_HIT, Mask);
		}

		// check for death
		if(m_Health <= 0)
		{
			Die(WEAPON_PLAYER);

			// set attacker's face to happy (taunt!)
			if (From >= 0 && From != m_pPlayer->GetCID() && GameServer()->m_apPlayers[From])
			{
				CCharacter *pChr = GameServer()->m_apPlayers[From]->GetCharacter();
				if (pChr)
				{
					pChr->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
				}
			}

			return false;
		}

		if (Dmg > 2)
			GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG);
		else
			GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_SHORT);
	}

	if ((Dmg && m_pPlayer->m_Gamemode == GAMEMODE_VANILLA)
		|| Weapon == WEAPON_HAMMER
		|| Weapon == WEAPON_GRENADE
		|| Weapon == WEAPON_STRAIGHT_GRENADE
		|| Weapon == WEAPON_PLASMA_RIFLE
		|| Weapon == WEAPON_LIGHTSABER
		|| Weapon == WEAPON_TASER
		|| Weapon == WEAPON_PROJECTILE_RIFLE
		|| Weapon == WEAPON_BALL_GRENADE)
	{
		SetEmote(EMOTE_PAIN, Server()->Tick() + 500 * Server()->TickSpeed() / 1000);
	}

	vec2 Temp = m_Core.m_Vel + Force;
	m_Core.m_Vel = ClampVel(m_MoveRestrictions, Temp);

	return true;
}

void CCharacter::Snap(int SnappingClient)
{
	int id = m_pPlayer->GetCID();

	if (SnappingClient > -1 && !Server()->Translate(id, SnappingClient))
		return;

	if(NetworkClipped(SnappingClient))
		return;

	if (SnappingClient > -1)
	{
		CCharacter* SnapChar = GameServer()->GetPlayerChar(SnappingClient);
		CPlayer* SnapPlayer = GameServer()->m_apPlayers[SnappingClient];

		if ((SnapPlayer->GetTeam() == TEAM_SPECTATORS || SnapPlayer->IsPaused()) && SnapPlayer->GetSpectatorID() != -1
			&& !CanCollide(SnapPlayer->GetSpectatorID(), false) && !SnapPlayer->m_ShowOthers)
			return;

		if (SnapPlayer->GetTeam() != TEAM_SPECTATORS && !SnapPlayer->IsPaused() && SnapChar && !SnapChar->m_Super
			&& !CanCollide(SnappingClient, false) && !SnapPlayer->m_ShowOthers)
			return;

		if ((SnapPlayer->GetTeam() == TEAM_SPECTATORS || SnapPlayer->IsPaused()) && SnapPlayer->GetSpecMode() == SPEC_FREEVIEW
			&& !CanCollide(SnappingClient, false) && SnapPlayer->m_SpecTeam)
			return;

		// F-DDrace
		if (m_Invisible && SnappingClient != m_pPlayer->GetCID())
			if (!Server()->GetAuthedState(SnappingClient) || Server()->Tick() % 200 == 0)
				return;
	}

	if (m_Paused)
		return;

	CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, id, sizeof(CNetObj_Character)));
	if(!pCharacter)
		return;

	// F-DDrace
	if (m_StrongBloody)
	{
		for (int i = 0; i < 3; i++)
			GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID(), Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
	}
	else if (m_Bloody || m_pPlayer->IsHooked(BLOODY))
	{
		if (Server()->Tick() % 3 == 0)
			GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID(), Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
	}

	// write down the m_Core
	if(!m_ReckoningTick || GameWorld()->m_Paused)
	{
		// no dead reckoning when paused because the client doesn't know
		// how far to perform the reckoning
		pCharacter->m_Tick = 0;
		m_Core.Write(pCharacter);
	}
	else
	{
		pCharacter->m_Tick = m_ReckoningTick;
		m_SendCore.Write(pCharacter);
	}

	// set emote
	if (m_EmoteStop < Server()->Tick())
	{
		SetEmote(m_pPlayer->m_DefEmote, -1);
	}

	if (pCharacter->m_HookedPlayer != -1)
	{
		if (!Server()->Translate(pCharacter->m_HookedPlayer, SnappingClient))
			pCharacter->m_HookedPlayer = -1;
	}

	pCharacter->m_Emote = m_pPlayer->m_SpookyGhost ? EMOTE_SURPRISE : m_EmoteType;

	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;
	pCharacter->m_TriggeredEvents = m_TriggeredEvents;

	pCharacter->m_Weapon = GameServer()->GetWeaponType(m_ActiveWeapon);
	pCharacter->m_AttackTick = m_AttackTick;

	pCharacter->m_Direction = m_Input.m_Direction;

	// change eyes and use ninja graphic if player is freeze
	if (m_DeepFreeze)
	{
		if (pCharacter->m_Emote == EMOTE_NORMAL)
			pCharacter->m_Emote = EMOTE_PAIN;
		pCharacter->m_Weapon = WEAPON_NINJA;
	}
	else if (m_FreezeTime > 0 || m_FreezeTime == -1)
	{
		if (pCharacter->m_Emote == EMOTE_NORMAL)
			pCharacter->m_Emote = EMOTE_BLINK;
		pCharacter->m_Weapon = WEAPON_NINJA;
	}

	// change eyes, use ninja graphic and set ammo count if player has ninjajetpack
	if (m_pPlayer->m_NinjaJetpack && m_Jetpack && GetActiveWeapon() == WEAPON_GUN && !m_DeepFreeze && !(m_FreezeTime > 0 || m_FreezeTime == -1))
	{
		if (pCharacter->m_Emote == EMOTE_NORMAL)
			pCharacter->m_Emote = EMOTE_HAPPY;
		pCharacter->m_Weapon = WEAPON_NINJA;
		pCharacter->m_AmmoCount = 10;
	}

	if(m_pPlayer->GetCID() == SnappingClient || SnappingClient == -1 ||
		(!Config()->m_SvStrictSpectateMode && m_pPlayer->GetCID() == GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID())
		|| m_pPlayer->m_TeeControllerID == SnappingClient)
	{
		pCharacter->m_Health = m_Health;
		pCharacter->m_Armor = m_Armor;

		if(m_aWeapons[GetActiveWeapon()].m_Ammo > 0)
		{
			int Ammo = m_aWeapons[GetActiveWeapon()].m_Ammo;
			if (GetActiveWeapon() == WEAPON_TASER)
				Ammo /= 10;
			pCharacter->m_AmmoCount = Ammo;
		}

		if (!Server()->IsSevendown(SnappingClient))
		{
			if (m_FreezeTime > 0 || m_FreezeTime == -1 || m_DeepFreeze)
				pCharacter->m_AmmoCount = m_FreezeTick + Config()->m_SvFreezeDelay * Server()->TickSpeed();
			else if(GetActiveWeapon() == WEAPON_NINJA)
				pCharacter->m_AmmoCount = m_Ninja.m_ActivationTick + g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000;
		}
	}

	if (GetPlayer()->m_Afk || GetPlayer()->IsPaused())
	{
		if (m_FreezeTime > 0 || m_FreezeTime == -1 || m_DeepFreeze)
			pCharacter->m_Emote = EMOTE_NORMAL;
		else
			pCharacter->m_Emote = EMOTE_BLINK;
	}

	if(pCharacter->m_Emote == EMOTE_NORMAL)
	{
		if(250 - ((Server()->Tick() - m_LastAction)%(250)) < 5)
			pCharacter->m_Emote = EMOTE_BLINK;
	}

	if (m_pPlayer->m_Halloween)
	{
		if (1200 - ((Server()->Tick() - m_LastAction) % (1200)) < 5)
		{
			GameServer()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_GHOST);
		}
	}

	if(Server()->IsSevendown(SnappingClient))
	{
		int PlayerFlags = 0;
		if (m_pPlayer->m_PlayerFlags&PLAYERFLAG_CHATTING) PlayerFlags |= 4;
		if (m_pPlayer->m_PlayerFlags&PLAYERFLAG_SCOREBOARD) PlayerFlags |= 8;
		if (m_pPlayer->m_PlayerFlags&PLAYERFLAG_AIM) PlayerFlags |= 16;

		int Health = pCharacter->m_Health;
		int Armor = pCharacter->m_Armor;
		int AmmoCount = pCharacter->m_AmmoCount;
		int Weapon = pCharacter->m_Weapon;
		int Emote = pCharacter->m_Emote;
		int AttackTick = pCharacter->m_AttackTick;

		int Offset = sizeof(CNetObj_CharacterCore) / 4;
		((int*)pCharacter)[Offset+0] = PlayerFlags;
		((int*)pCharacter)[Offset+1] = Health;
		((int*)pCharacter)[Offset+2] = Armor;
		((int*)pCharacter)[Offset+3] = AmmoCount;
		((int*)pCharacter)[Offset+4] = Weapon;
		((int*)pCharacter)[Offset+5] = Emote;
		((int*)pCharacter)[Offset+6] = AttackTick;
	}

	CNetObj_DDNetCharacter *pDDNetCharacter = static_cast<CNetObj_DDNetCharacter *>(Server()->SnapNewItem(NETOBJTYPE_DDNETCHARACTER, id, sizeof(CNetObj_DDNetCharacter)));
	if(!pDDNetCharacter)
		return;

	pDDNetCharacter->m_Flags = 0;
	if(m_Solo)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_SOLO;
	if(m_Super)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_SUPER;
	if(m_EndlessHook)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_ENDLESS_HOOK;
	if(!m_Core.m_Collision || !GameServer()->Tuning()->m_PlayerCollision || m_Passive)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_NO_COLLISION;
	if(!m_Core.m_Hook || !GameServer()->Tuning()->m_PlayerHooking || m_Passive)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_NO_HOOK;
	if(m_SuperJump)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_ENDLESS_JUMP;
	if(m_Jetpack && GetActiveWeapon() == WEAPON_GUN)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_JETPACK;
	if(m_Hit & DISABLE_HIT_GRENADE)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_NO_GRENADE_HIT;
	if(m_Hit & DISABLE_HIT_HAMMER)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_NO_HAMMER_HIT;
	if(m_Hit & DISABLE_HIT_RIFLE)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_NO_LASER_HIT;
	if(m_Hit & DISABLE_HIT_SHOTGUN)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_NO_SHOTGUN_HIT;
	if(m_HasTeleGun)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_TELEGUN_GUN;
	if(m_HasTeleGrenade)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_TELEGUN_GRENADE;
	if(m_HasTeleLaser)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_TELEGUN_LASER;
	if(m_aWeapons[WEAPON_HAMMER].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_HAMMER;
	if(m_aWeapons[WEAPON_GUN].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_GUN;
	if(m_aWeapons[WEAPON_SHOTGUN].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_SHOTGUN;
	if(m_aWeapons[WEAPON_GRENADE].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_GRENADE;
	if(m_aWeapons[WEAPON_LASER].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_LASER;
	if(GetActiveWeapon() == WEAPON_NINJA)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_NINJA;

	pDDNetCharacter->m_FreezeEnd = m_DeepFreeze ? -1 : m_FreezeTime == 0 ? 0 : Server()->Tick() + m_FreezeTime;
	pDDNetCharacter->m_Jumps = m_Core.m_Jumps;
	pDDNetCharacter->m_TeleCheckpoint = m_TeleCheckpoint;
	pDDNetCharacter->m_StrongWeakID = m_StrongWeakID;
}

void CCharacter::PostSnap()
{
	m_TriggeredEvents = 0;
}

// DDRace

int CCharacter::NetworkClipped(int SnappingClient, bool CheckShowAll)
{
	return NetworkClipped(SnappingClient, m_Pos, CheckShowAll);
}

int CCharacter::NetworkClipped(int SnappingClient, vec2 CheckPos, bool CheckShowAll)
{
	if (SnappingClient == -1 || (CheckShowAll && GameServer()->m_apPlayers[SnappingClient]->m_ShowAll))
		return 0;

	float dx = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.x-CheckPos.x;
	if(absolute(dx) > GameServer()->m_apPlayers[SnappingClient]->m_ShowDistance.x)
		return 1;

	float dy = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.y-CheckPos.y;
	if(absolute(dy) > GameServer()->m_apPlayers[SnappingClient]->m_ShowDistance.y)
		return 1;

	return 0;
}

bool CCharacter::CanCollide(int ClientID, bool CheckPassive)
{
	return Teams()->m_Core.CanCollide(GetPlayer()->GetCID(), ClientID, CheckPassive);
}
bool CCharacter::SameTeam(int ClientID)
{
	return Teams()->m_Core.SameTeam(GetPlayer()->GetCID(), ClientID);
}

int CCharacter::Team()
{
	return Teams()->m_Core.Team(m_pPlayer->GetCID());
}

CGameTeams* CCharacter::Teams()
{
	return &((CGameControllerDDRace*)GameServer()->m_pController)->m_Teams;
}

void CCharacter::HandleBroadcast()
{
	CPlayerData* pData = GameServer()->Score()->PlayerData(m_pPlayer->GetCID());

	if (m_DDRaceState == DDRACE_STARTED && m_CpLastBroadcast != m_CpActive &&
		m_CpActive > -1 && m_CpTick > Server()->Tick() && pData->m_BestTime && pData->m_aBestCpTime[m_CpActive] != 0)
	{
		char aBroadcast[128];
		float Diff = m_CpCurrent[m_CpActive] - pData->m_aBestCpTime[m_CpActive];
		const char* pColor = (Diff <= 0) ? "^090" : "^900";
		str_format(aBroadcast, sizeof(aBroadcast), "%sCheckpoint | Diff : %+5.2f", pColor, Diff);
		GameServer()->SendBroadcast(aBroadcast, m_pPlayer->GetCID());
		m_CpLastBroadcast = m_CpActive;
		m_LastBroadcast = Server()->Tick();
	}
}

void CCharacter::HandleSkippableTiles(int Index)
{
	// handle death-tiles and leaving gamelayer
	if ((GameServer()->Collision()->GetCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
		GameServer()->Collision()->GetCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH ||
		GameServer()->Collision()->GetCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
		GameServer()->Collision()->GetFCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
		GameServer()->Collision()->GetFCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH ||
		GameServer()->Collision()->GetFCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
		GameServer()->Collision()->GetCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH) &&
		!m_Super && !(Team() && Teams()->TeeFinished(m_pPlayer->GetCID())))
	{
		Die(WEAPON_WORLD);
		return;
	}

	if (GameLayerClipped(m_Pos))
	{
		Die(WEAPON_WORLD);
		return;
	}

	if (Index < 0)
		return;

	// handle speedup tiles
	if (GameServer()->Collision()->IsSpeedup(Index))
	{
		vec2 Direction, MaxVel, TempVel = m_Core.m_Vel;
		int Force, MaxSpeed = 0;
		float TeeAngle, SpeederAngle, DiffAngle, SpeedLeft, TeeSpeed;
		GameServer()->Collision()->GetSpeedup(Index, &Direction, &Force, &MaxSpeed);
		if (Force == 255 && MaxSpeed)
		{
			m_Core.m_Vel = Direction * (MaxSpeed / 5);
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

			m_Core.m_Vel = ClampVel(m_MoveRestrictions, TempVel);
		}
	}
}

bool CCharacter::IsSwitchActiveCb(int Number, void *pUser)
{
	CCharacter *pThis = (CCharacter *)pUser;
	CCollision *pCollision = pThis->GameServer()->Collision();
	return pCollision->m_pSwitchers && pCollision->m_pSwitchers[Number].m_Status[pThis->Team()] && pThis->Team() != TEAM_SUPER;
}

void CCharacter::HandleTiles(int Index)
{
	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
	int MapIndex = Index;
	//int PureMapIndex = GameServer()->Collision()->GetPureMapIndex(m_Pos);
	m_TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	m_TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);

	int LastMoveRestrictions = m_MoveRestrictions;
	m_MoveRestrictions = GameServer()->Collision()->GetMoveRestrictions(IsSwitchActiveCb, this, m_Pos, 18.0f, MapIndex, m_Core.m_MoveRestrictionExtra);
	// update prediction
	if (((m_MoveRestrictions&CANTMOVE_DOWN_LASERDOOR) && !(LastMoveRestrictions&CANTMOVE_DOWN_LASERDOOR))
		|| (!(m_MoveRestrictions&CANTMOVE_DOWN_LASERDOOR) && (LastMoveRestrictions&CANTMOVE_DOWN_LASERDOOR)))
		GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);

	//Sensitivity
	int S1 = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f));
	int S2 = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f));
	int S3 = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f));
	int S4 = GameServer()->Collision()->GetPureMapIndex(vec2(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f));
	int Tile1 = GameServer()->Collision()->GetTileIndex(S1);
	int Tile2 = GameServer()->Collision()->GetTileIndex(S2);
	int Tile3 = GameServer()->Collision()->GetTileIndex(S3);
	int Tile4 = GameServer()->Collision()->GetTileIndex(S4);
	int FTile1 = GameServer()->Collision()->GetFTileIndex(S1);
	int FTile2 = GameServer()->Collision()->GetFTileIndex(S2);
	int FTile3 = GameServer()->Collision()->GetFTileIndex(S3);
	int FTile4 = GameServer()->Collision()->GetFTileIndex(S4);
	if (Index < 0)
	{
		m_LastRefillJumps = false;
		m_LastPenalty = false;
		m_LastBonus = false;
		return;
	}
	int cp = GameServer()->Collision()->IsCheckpoint(MapIndex);
	if (cp != -1 && m_DDRaceState == DDRACE_STARTED && cp > m_CpActive)
	{
		m_CpActive = cp;
		m_CpCurrent[cp] = m_Time;
		m_CpTick = Server()->Tick() + Server()->TickSpeed() * 2;
	}
	int cpf = GameServer()->Collision()->IsFCheckpoint(MapIndex);
	if (cpf != -1 && m_DDRaceState == DDRACE_STARTED && cpf > m_CpActive)
	{
		m_CpActive = cpf;
		m_CpCurrent[cpf] = m_Time;
		m_CpTick = Server()->Tick() + Server()->TickSpeed() * 2;
	}
	int tcp = GameServer()->Collision()->IsTCheckpoint(MapIndex);
	if (tcp)
		m_TeleCheckpoint = tcp;

	// start
	if (((m_TileIndex == TILE_BEGIN) || (m_TileFIndex == TILE_BEGIN) || FTile1 == TILE_BEGIN || FTile2 == TILE_BEGIN || FTile3 == TILE_BEGIN || FTile4 == TILE_BEGIN || Tile1 == TILE_BEGIN || Tile2 == TILE_BEGIN || Tile3 == TILE_BEGIN || Tile4 == TILE_BEGIN) && (m_DDRaceState == DDRACE_NONE || m_DDRaceState == DDRACE_FINISHED || (m_DDRaceState == DDRACE_STARTED && !Team() && Config()->m_SvTeam != 3)))
	{
		if (Config()->m_SvResetPickups)
		{
			for (int i = WEAPON_SHOTGUN; i < NUM_WEAPONS; ++i)
			{
				m_aWeapons[i].m_Got = false;
				if (GetActiveWeapon() == i)
					SetActiveWeapon(WEAPON_GUN);
			}
		}
		if (Config()->m_SvTeam == 2 && (Team() == TEAM_FLOCK || Teams()->Count(Team()) <= 1))
		{
			if (m_LastStartWarning < Server()->Tick() - 3 * Server()->TickSpeed())
			{
				GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You have to be in a team with other tees to start");
				m_LastStartWarning = Server()->Tick();
			}
			Die(WEAPON_WORLD);
			return;
		}

		Teams()->OnCharacterStart(m_pPlayer->GetCID());
		m_CpActive = -2;

		// F-DDrace
		// allow re-finishing the special race after touching start tiles without a kill
		m_HasFinishedSpecialRace = false;
	}

	// finish
	if (((m_TileIndex == TILE_END) || (m_TileFIndex == TILE_END) || FTile1 == TILE_END || FTile2 == TILE_END || FTile3 == TILE_END || FTile4 == TILE_END || Tile1 == TILE_END || Tile2 == TILE_END || Tile3 == TILE_END || Tile4 == TILE_END) && m_DDRaceState == DDRACE_STARTED)
	{
		Controller->m_Teams.OnCharacterFinish(m_pPlayer->GetCID());
		m_pPlayer->GiveXP(500, "finish the race");
	}

	// freeze
	if (((m_TileIndex == TILE_FREEZE) || (m_TileFIndex == TILE_FREEZE)) && !m_Super && !m_DeepFreeze)
		Freeze();
	else if (((m_TileIndex == TILE_UNFREEZE) || (m_TileFIndex == TILE_UNFREEZE)) && !m_DeepFreeze)
		UnFreeze();

	// deep freeze
	if (((m_TileIndex == TILE_DFREEZE) || (m_TileFIndex == TILE_DFREEZE)) && !m_Super && !m_DeepFreeze)
		m_DeepFreeze = true;
	else if (((m_TileIndex == TILE_DUNFREEZE) || (m_TileFIndex == TILE_DUNFREEZE)) && !m_Super && m_DeepFreeze)
		m_DeepFreeze = false;

	// endless hook
	if (((m_TileIndex == TILE_EHOOK_START) || (m_TileFIndex == TILE_EHOOK_START)) && !m_EndlessHook)
		EndlessHook();

	else if (((m_TileIndex == TILE_EHOOK_END) || (m_TileFIndex == TILE_EHOOK_END)) && m_EndlessHook)
		EndlessHook(false);

	// hit others
	if (((m_TileIndex == TILE_HIT_END) || (m_TileFIndex == TILE_HIT_END)) && m_Hit != (DISABLE_HIT_GRENADE | DISABLE_HIT_HAMMER | DISABLE_HIT_RIFLE | DISABLE_HIT_SHOTGUN))
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't hit others");
		m_Hit = DISABLE_HIT_GRENADE | DISABLE_HIT_HAMMER | DISABLE_HIT_RIFLE | DISABLE_HIT_SHOTGUN;
	}
	else if (((m_TileIndex == TILE_HIT_START) || (m_TileFIndex == TILE_HIT_START)) && m_Hit != HIT_ALL)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can hit others");
		m_Hit = HIT_ALL;
	}

	// collide with others
	if (((m_TileIndex == TILE_NPC_END) || (m_TileFIndex == TILE_NPC_END)) && m_Core.m_Collision)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't collide with others");
		m_Core.m_Collision = false;
	}
	else if (((m_TileIndex == TILE_NPC_START) || (m_TileFIndex == TILE_NPC_START)) && !m_Core.m_Collision)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can collide with others");
		m_Core.m_Collision = true;
	}

	// hook others
	if (((m_TileIndex == TILE_NPH_END) || (m_TileFIndex == TILE_NPH_END)) && m_Core.m_Hook)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't hook others");
		m_Core.m_Hook = false;
	}
	else if (((m_TileIndex == TILE_NPH_START) || (m_TileFIndex == TILE_NPH_START)) && !m_Core.m_Hook)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can hook others");
		m_Core.m_Hook = true;
	}

	// unlimited air jumps
	if (((m_TileIndex == TILE_SUPER_START) || (m_TileFIndex == TILE_SUPER_START)) && !m_SuperJump)
		InfiniteJumps();

	else if (((m_TileIndex == TILE_SUPER_END) || (m_TileFIndex == TILE_SUPER_END)) && m_SuperJump)
		InfiniteJumps(false);

	// walljump
	if ((m_TileIndex == TILE_WALLJUMP) || (m_TileFIndex == TILE_WALLJUMP))
	{
		if (m_Core.m_Vel.y > 0 && m_Core.m_Colliding && m_Core.m_LeftWall)
		{
			m_Core.m_LeftWall = false;
			m_Core.m_JumpedTotal = m_Core.m_Jumps - 1;
			m_Core.m_Jumped = 1;
		}
	}

	// jetpack gun
	if (((m_TileIndex == TILE_JETPACK_START) || (m_TileFIndex == TILE_JETPACK_START)) && !m_Jetpack)
		Jetpack();

	else if (((m_TileIndex == TILE_JETPACK_END) || (m_TileFIndex == TILE_JETPACK_END)) && m_Jetpack)
		Jetpack(false);

	// F-DDrace

	//jetpack toggle
	if ((m_TileIndex == TILE_JETPACK) || (m_TileFIndex == TILE_JETPACK))
	{
		if ((m_LastIndexTile != TILE_JETPACK) && (m_LastIndexFrontTile != TILE_JETPACK))
			Jetpack(!m_Jetpack);
	}

	//rainbow toggle
	if ((m_TileIndex == TILE_RAINBOW) || (m_TileFIndex == TILE_RAINBOW))
	{
		if ((m_LastIndexTile != TILE_RAINBOW) && (m_LastIndexFrontTile != TILE_RAINBOW))
			Rainbow(!(m_Rainbow || m_pPlayer->m_InfRainbow));
	}

	//atom toggle
	if ((m_TileIndex == TILE_ATOM) || (m_TileFIndex == TILE_ATOM))
	{
		if ((m_LastIndexTile != TILE_ATOM) && (m_LastIndexFrontTile != TILE_ATOM))
			Atom(!m_Atom);
	}

	//trail toggle
	if ((m_TileIndex == TILE_TRAIL) || (m_TileFIndex == TILE_TRAIL))
	{
		if ((m_LastIndexTile != TILE_TRAIL) && (m_LastIndexFrontTile != TILE_TRAIL))
			Trail(!m_Trail);
	}

	//spooky ghost toggle
	if ((m_TileIndex == TILE_SPOOKY_GHOST) || (m_TileFIndex == TILE_SPOOKY_GHOST))
	{
		if ((m_LastIndexTile != TILE_SPOOKY_GHOST) && (m_LastIndexFrontTile != TILE_SPOOKY_GHOST))
			SpookyGhost(!m_pPlayer->m_SpookyGhost);
	}

	//add meteor
	if ((m_TileIndex == TILE_ADD_METEOR) || (m_TileFIndex == TILE_ADD_METEOR))
	{
		if ((m_LastIndexTile != TILE_ADD_METEOR) && (m_LastIndexFrontTile != TILE_ADD_METEOR))
			Meteor();
	}

	//remove meteors
	if ((m_TileIndex == TILE_REMOVE_METEORS) || (m_TileFIndex == TILE_REMOVE_METEORS))
	{
		if ((m_LastIndexTile != TILE_REMOVE_METEORS) && (m_LastIndexFrontTile != TILE_REMOVE_METEORS))
			Meteor(false);
	}

	//passive toggle
	if ((m_TileIndex == TILE_PASSIVE) || (m_TileFIndex == TILE_PASSIVE))
	{
		if ((m_LastIndexTile != TILE_PASSIVE) && (m_LastIndexFrontTile != TILE_PASSIVE))
			Passive(!m_Passive);
	}

	//vanilla mode
	if ((m_TileIndex == TILE_VANILLA_MODE) || (m_TileFIndex == TILE_VANILLA_MODE))
	{
		if ((m_LastIndexTile != TILE_VANILLA_MODE) && (m_LastIndexFrontTile != TILE_VANILLA_MODE))
			VanillaMode();
	}

	//ddrace mode
	if ((m_TileIndex == TILE_DDRACE_MODE) || (m_TileFIndex == TILE_DDRACE_MODE))
	{
		if ((m_LastIndexTile != TILE_DDRACE_MODE) && (m_LastIndexFrontTile != TILE_DDRACE_MODE))
			DDraceMode();
	}

	//bloody toggle
	if ((m_TileIndex == TILE_BLOODY) || (m_TileFIndex == TILE_BLOODY))
	{
		if ((m_LastIndexTile != TILE_BLOODY) && (m_LastIndexFrontTile != TILE_BLOODY))
			Bloody(!(m_Bloody || m_StrongBloody));
	}

	//add jump
	if ((m_TileIndex == TILE_JUMP_ADD) || (m_TileFIndex == TILE_JUMP_ADD))
	{
		if ((m_LastIndexTile != TILE_JUMP_ADD) && (m_LastIndexFrontTile != TILE_JUMP_ADD))
		{
			m_Core.m_Jumps++;
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "You got +1 jump and can jump %d times now", m_Core.m_Jumps);
			GameServer()->SendChatTarget(GetPlayer()->GetCID(), aBuf);
		}
	}

	//shop
	for (int i = 0; i < NUM_HOUSES; i++)
	{
		int Index = -1;
		switch (i)
		{
		case HOUSE_SHOP: Index = TILE_SHOP; break;
		case HOUSE_PLOT_SHOP: Index = TILE_PLOT_SHOP; break;
		case HOUSE_BANK: Index = TILE_BANK; break;
		}

		if (m_TileIndex == Index || m_TileFIndex == Index)
		{
			if (m_LastIndexTile != Index && m_LastIndexFrontTile != Index)
				GameServer()->m_pHouses[i]->OnEnter(m_pPlayer->GetCID());
		}
	}

	// helper only
	if ((m_TileIndex == TILE_HELPERS_ONLY) || (m_TileFIndex == TILE_HELPERS_ONLY))
	{
		if (Server()->GetAuthedState(m_pPlayer->GetCID()) < AUTHED_HELPER)
		{
			GameServer()->SendChatTarget(GetPlayer()->GetCID(), "This area is for helpers only");
			Die(WEAPON_WORLD);
			return;
		}
	}

	// moderator only
	if ((m_TileIndex == TILE_MODERATORS_ONLY) || (m_TileFIndex == TILE_MODERATORS_ONLY))
	{
		if (Server()->GetAuthedState(m_pPlayer->GetCID()) < AUTHED_MOD)
		{
			GameServer()->SendChatTarget(GetPlayer()->GetCID(), "This area is for moderators only");
			Die(WEAPON_WORLD);
			return;
		}
	}

	// admin only
	if ((m_TileIndex == TILE_ADMINS_ONLY) || (m_TileFIndex == TILE_ADMINS_ONLY))
	{
		if (Server()->GetAuthedState(m_pPlayer->GetCID()) < AUTHED_ADMIN)
		{
			GameServer()->SendChatTarget(GetPlayer()->GetCID(), "This area is for admins only");
			Die(WEAPON_WORLD);
			return;
		}
	}

	bool MoneyTile = m_TileIndex == TILE_MONEY || m_TileFIndex == TILE_MONEY;
	bool PoliceMoneyTile = m_TileIndex == TILE_MONEY_POLICE || m_TileFIndex == TILE_MONEY_POLICE;
	if (MoneyTile || PoliceMoneyTile)
	{
		m_MoneyTile = true;

		bool Plot = GetCurrentTilePlotID() >= PLOT_START;
		int Seconds = Server()->TickSpeed();
		if (m_pPlayer->m_JailTime) // every 3 seconds only while arrested
			Seconds *= 3;
		else if (Plot) // every 2 seconds only on plot money tile
			Seconds *= 2;

		if (Server()->Tick() % Seconds == 0)
		{
			if (m_pPlayer->GetAccID() < ACC_START)
			{
				GameServer()->SendBroadcast("You need to be logged in to use moneytiles.\nGet an account with '/register <name> <pw> <pw>'", m_pPlayer->GetCID(), false);
				return;
			}

			CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[m_pPlayer->GetAccID()];

			int XP = 0;
			int Money = 0;
			int AliveState = Plot ? 0 : GetAliveState(); // disallow survival bonus on plot money tile

			// default
			Money += 1;
			XP += AliveState + 1;

			// vip bonus
			if (pAccount->m_VIP)
			{
				XP += 2;
				Money += 2;
			}

			// police bonus
			if (PoliceMoneyTile)
			{
				XP += 1;
				Money += pAccount->m_PoliceLevel;
			}

			//flag bonus
			bool FlagBonus = false;
			if (!PoliceMoneyTile && !Plot && HasFlag() != -1)
			{
				XP += 1;
				FlagBonus = true;
			}

			// give money and xp
			m_pPlayer->WalletTransaction(Money);
			m_pPlayer->GiveXP(XP);

			// broadcast
			{
				char aMsg[256];
				char aSurvival[32];
				char aPolice[32];
				char aPlusXP[128];

				str_format(aSurvival, sizeof(aSurvival), " +%d survival", AliveState);
				str_format(aPolice, sizeof(aPolice), " +%d police", pAccount->m_PoliceLevel);
				str_format(aPlusXP, sizeof(aPlusXP), " +%d%s%s%s", PoliceMoneyTile ? 2 : 1, FlagBonus ? " +1 flag" : "", pAccount->m_VIP ? " +2 vip" : "", AliveState ? aSurvival : "");
				str_format(aMsg, sizeof(aMsg),
						"Money [%lld] +1%s%s\n"
						"XP [%lld/%lld]%s\n"
						"Level [%d]",
						m_pPlayer->GetWalletMoney(), (PoliceMoneyTile && pAccount->m_PoliceLevel) ? aPolice : "", pAccount->m_VIP ? " +2 vip" : "",
						pAccount->m_XP, GameServer()->GetNeededXP(pAccount->m_Level), aPlusXP,
						pAccount->m_Level
					);

				// message gets cut off otherwise
				if (Server()->IsSevendown(m_pPlayer->GetCID()) && AliveState && FlagBonus)
				{
					for (int i = 0; i < 32; i++)
						str_append(aMsg, " ", sizeof(aMsg));
				}
				GameServer()->SendBroadcast(GameServer()->FormatExperienceBroadcast(aMsg, m_pPlayer->GetCID()), m_pPlayer->GetCID(), false);
			}
		}
	}

	// money xp bomb
	if (!m_GotMoneyXPBomb && (m_TileIndex == TILE_MONEY_XP_BOMB || m_TileFIndex == TILE_MONEY_XP_BOMB))
	{
		if (m_pPlayer->GetAccID() < ACC_START)
		{
			if (Server()->Tick() % 50 == 0)
				GameServer()->SendBroadcast("You need to be logged in to use moneytiles.\nGet an account with '/register <name> <pw> <pw>'", m_pPlayer->GetCID(), false);
		}
		else if (m_pPlayer->m_LastMoneyXPBomb < Server()->Tick() - Server()->TickSpeed() * Config()->m_SvPortalDetonationLinked)
		{
			m_pPlayer->WalletTransaction(500, "from money-xp bomb");
			m_pPlayer->GiveXP(2500);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "+2500 XP, +500 money (money-xp bomb)");

			m_GotMoneyXPBomb = true;
			m_pPlayer->m_LastMoneyXPBomb = Server()->Tick();
		}
	}

	// special finish
	if (!m_HasFinishedSpecialRace && m_DDRaceState != DDRACE_CHEAT && (m_TileIndex == TILE_SPECIAL_FINISH || m_TileFIndex == TILE_SPECIAL_FINISH || FTile1 == TILE_SPECIAL_FINISH || FTile2 == TILE_SPECIAL_FINISH || FTile3 == TILE_SPECIAL_FINISH || FTile4 == TILE_SPECIAL_FINISH || Tile1 == TILE_SPECIAL_FINISH || Tile2 == TILE_SPECIAL_FINISH || Tile3 == TILE_SPECIAL_FINISH || Tile4 == TILE_SPECIAL_FINISH))
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "'%s' finished the special race!", Server()->ClientName(m_pPlayer->GetCID()));
		GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
		m_pPlayer->GiveXP(750, "finish the special race");

		m_HasFinishedSpecialRace = true;
	}

	m_LastIndexTile = m_TileIndex;
	m_LastIndexFrontTile = m_TileFIndex;

	// unlock team
	if (((m_TileIndex == TILE_UNLOCK_TEAM) || (m_TileFIndex == TILE_UNLOCK_TEAM)) && Teams()->TeamLocked(Team()))
	{
		Teams()->SetTeamLock(Team(), false);

		for (int i = 0; i < MAX_CLIENTS; i++)
			if (Teams()->m_Core.Team(i) == Team())
				GameServer()->SendChatTarget(i, "Your team was unlocked by an unlock team tile");
	}

	// solo part
	if (((m_TileIndex == TILE_SOLO_START) || (m_TileFIndex == TILE_SOLO_START)) && !Teams()->m_Core.GetSolo(m_pPlayer->GetCID()))
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You are now in a solo part");
		SetSolo(true);
	}
	else if (((m_TileIndex == TILE_SOLO_END) || (m_TileFIndex == TILE_SOLO_END)) && Teams()->m_Core.GetSolo(m_pPlayer->GetCID()))
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You are now out of the solo part");
		SetSolo(false);
	}

	// refill jumps
	if (((m_TileIndex == TILE_REFILL_JUMPS) || (m_TileFIndex == TILE_REFILL_JUMPS)) && !m_LastRefillJumps)
	{
		m_Core.m_JumpedTotal = 0;
		m_Core.m_Jumped = 0;
		m_LastRefillJumps = true;
	}
	if ((m_TileIndex != TILE_REFILL_JUMPS) && (m_TileFIndex != TILE_REFILL_JUMPS))
	{
		m_LastRefillJumps = false;
	}

	// Teleport gun
	if (((m_TileIndex == TILE_TELE_GUN_ENABLE) || (m_TileFIndex == TILE_TELE_GUN_ENABLE)) && !m_HasTeleGun)
	{
		m_HasTeleGun = true;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Teleport gun enabled");
	}
	else if (((m_TileIndex == TILE_TELE_GUN_DISABLE) || (m_TileFIndex == TILE_TELE_GUN_DISABLE)) && m_HasTeleGun)
	{
		m_HasTeleGun = false;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Teleport gun disabled");
	}

	if (((m_TileIndex == TILE_TELE_GRENADE_ENABLE) || (m_TileFIndex == TILE_TELE_GRENADE_ENABLE)) && !m_HasTeleGrenade)
	{
		m_HasTeleGrenade = true;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Teleport grenade enabled");
	}
	else if (((m_TileIndex == TILE_TELE_GRENADE_DISABLE) || (m_TileFIndex == TILE_TELE_GRENADE_DISABLE)) && m_HasTeleGrenade)
	{
		m_HasTeleGrenade = false;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Teleport grenade disabled");
	}

	if (((m_TileIndex == TILE_TELE_LASER_ENABLE) || (m_TileFIndex == TILE_TELE_LASER_ENABLE)) && !m_HasTeleLaser)
	{
		m_HasTeleLaser = true;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Teleport laser enabled");
	}
	else if (((m_TileIndex == TILE_TELE_LASER_DISABLE) || (m_TileFIndex == TILE_TELE_LASER_DISABLE)) && m_HasTeleLaser)
	{
		m_HasTeleLaser = false;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Teleport laser disabled");
	}

	if ((m_MoveRestrictions&CANTMOVE_ROOM) && m_RoomAntiSpamTick < Server()->Tick())
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You need a key to enter this room, buy one in the shop");
		m_RoomAntiSpamTick = Server()->Tick() + Server()->TickSpeed() * 5;
	}

	// stopper
	if(m_Core.m_Vel.y > 0 && (m_MoveRestrictions&CANTMOVE_DOWN))
	{
		m_Core.m_Jumped = 0;
		m_Core.m_JumpedTotal = 0;
	}
	m_Core.m_Vel = ClampVel(m_MoveRestrictions, m_Core.m_Vel);

	// handle switch tiles
	if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCHOPEN && Team() != TEAM_SUPER && GameServer()->Collision()->GetSwitchNumber(MapIndex) > 0)
	{
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()] = true;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_EndTick[Team()] = 0;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Type[Team()] = TILE_SWITCHOPEN;
		// F-DDrace
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_ClientID[Team()] = m_pPlayer->GetCID();
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_StartTick[Team()] = Server()->Tick();
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCHTIMEDOPEN && Team() != TEAM_SUPER && GameServer()->Collision()->GetSwitchNumber(MapIndex) > 0)
	{
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()] = true;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_EndTick[Team()] = Server()->Tick() + 1 + GameServer()->Collision()->GetSwitchDelay(MapIndex) * Server()->TickSpeed();
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Type[Team()] = TILE_SWITCHTIMEDOPEN;
		// F-DDrace
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_ClientID[Team()] = m_pPlayer->GetCID();
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_StartTick[Team()] = Server()->Tick();
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCHTIMEDCLOSE && Team() != TEAM_SUPER && GameServer()->Collision()->GetSwitchNumber(MapIndex) > 0)
	{
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()] = false;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_EndTick[Team()] = Server()->Tick() + 1 + GameServer()->Collision()->GetSwitchDelay(MapIndex) * Server()->TickSpeed();
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Type[Team()] = TILE_SWITCHTIMEDCLOSE;
		// F-DDrace
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_ClientID[Team()] = m_pPlayer->GetCID();
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_StartTick[Team()] = Server()->Tick();
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCHCLOSE && Team() != TEAM_SUPER && GameServer()->Collision()->GetSwitchNumber(MapIndex) > 0)
	{
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()] = false;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_EndTick[Team()] = 0;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Type[Team()] = TILE_SWITCHCLOSE;
		// F-DDrace
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_ClientID[Team()] = m_pPlayer->GetCID();
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_StartTick[Team()] = Server()->Tick();
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_FREEZE && Team() != TEAM_SUPER)
	{
		if (GameServer()->Collision()->GetSwitchNumber(MapIndex) == 0 || GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()])
			Freeze(GameServer()->Collision()->GetSwitchDelay(MapIndex));
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_DFREEZE && Team() != TEAM_SUPER)
	{
		if (GameServer()->Collision()->GetSwitchNumber(MapIndex) == 0 || GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()])
			m_DeepFreeze = true;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_DUNFREEZE && Team() != TEAM_SUPER)
	{
		if (GameServer()->Collision()->GetSwitchNumber(MapIndex) == 0 || GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()])
			m_DeepFreeze = false;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_START && m_Hit & DISABLE_HIT_HAMMER && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_HAMMER)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can hammer hit others");
		m_Hit &= ~DISABLE_HIT_HAMMER;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_END && !(m_Hit & DISABLE_HIT_HAMMER) && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_HAMMER)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't hammer hit others");
		m_Hit |= DISABLE_HIT_HAMMER;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_START && m_Hit & DISABLE_HIT_SHOTGUN && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_SHOTGUN)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can shoot others with shotgun");
		m_Hit &= ~DISABLE_HIT_SHOTGUN;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_END && !(m_Hit & DISABLE_HIT_SHOTGUN) && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_SHOTGUN)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't shoot others with shotgun");
		m_Hit |= DISABLE_HIT_SHOTGUN;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_START && m_Hit & DISABLE_HIT_GRENADE && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_GRENADE)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can shoot others with grenade");
		m_Hit &= ~DISABLE_HIT_GRENADE;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_END && !(m_Hit & DISABLE_HIT_GRENADE) && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_GRENADE)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't shoot others with grenade");
		m_Hit |= DISABLE_HIT_GRENADE;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_START && m_Hit & DISABLE_HIT_RIFLE && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_LASER)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can shoot others with rifle");
		m_Hit &= ~DISABLE_HIT_RIFLE;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_END && !(m_Hit & DISABLE_HIT_RIFLE) && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_LASER)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't shoot others with rifle");
		m_Hit |= DISABLE_HIT_RIFLE;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_JUMP)
	{
		int newJumps = GameServer()->Collision()->GetSwitchDelay(MapIndex);

		if (newJumps != m_Core.m_Jumps)
		{
			char aBuf[256];
			if (newJumps == 1)
				str_format(aBuf, sizeof(aBuf), "You can jump %d time", newJumps);
			else
				str_format(aBuf, sizeof(aBuf), "You can jump %d times", newJumps);
			GameServer()->SendChatTarget(GetPlayer()->GetCID(), aBuf);

			if (newJumps > m_MaxJumps && m_DDRaceState != DDRACE_CHEAT)
			{
				m_pPlayer->GiveXP(newJumps * 100, "upgrading jumps");
				m_MaxJumps = newJumps;
			}
			m_Core.m_Jumps = newJumps;
		}
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_PENALTY && !m_LastPenalty)
	{
		int min = GameServer()->Collision()->GetSwitchDelay(MapIndex);
		int sec = GameServer()->Collision()->GetSwitchNumber(MapIndex);
		int Team = Teams()->m_Core.Team(m_Core.m_Id);

		m_StartTime -= (min * 60 + sec) * Server()->TickSpeed();

		if (Team != TEAM_FLOCK && Team != TEAM_SUPER)
		{
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				if (Teams()->m_Core.Team(i) == Team && i != m_Core.m_Id && GameServer()->m_apPlayers[i])
				{
					CCharacter* pChar = GameServer()->m_apPlayers[i]->GetCharacter();

					if (pChar)
						pChar->m_StartTime = m_StartTime;
				}
			}
		}

		m_LastPenalty = true;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_BONUS && !m_LastBonus)
	{
		int min = GameServer()->Collision()->GetSwitchDelay(MapIndex);
		int sec = GameServer()->Collision()->GetSwitchNumber(MapIndex);
		int Team = Teams()->m_Core.Team(m_Core.m_Id);

		m_StartTime += (min * 60 + sec) * Server()->TickSpeed();
		if (m_StartTime > Server()->Tick())
			m_StartTime = Server()->Tick();

		if (Team != TEAM_FLOCK && Team != TEAM_SUPER)
		{
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				if (Teams()->m_Core.Team(i) == Team && i != m_Core.m_Id && GameServer()->m_apPlayers[i])
				{
					CCharacter* pChar = GameServer()->m_apPlayers[i]->GetCharacter();

					if (pChar)
						pChar->m_StartTime = m_StartTime;
				}
			}
		}

		m_LastBonus = true;
	}

	if (GameServer()->Collision()->IsSwitch(MapIndex) != TILE_PENALTY)
	{
		m_LastPenalty = false;
	}

	if (GameServer()->Collision()->IsSwitch(MapIndex) != TILE_BONUS)
	{
		m_LastBonus = false;
	}

	int z = GameServer()->Collision()->IsTeleport(MapIndex);
	if (!Config()->m_SvOldTeleportHook && !Config()->m_SvOldTeleportWeapons && z && Controller->m_TeleOuts[z - 1].size())
	{
		if (m_Super)
			return;
		int Num = Controller->m_TeleOuts[z - 1].size();
		m_Core.m_Pos = Controller->m_TeleOuts[z - 1][(!Num) ? Num : rand() % Num];
		if (!Config()->m_SvTeleportHoldHook)
		{
			m_Core.m_HookedPlayer = -1;
			m_Core.m_HookState = HOOK_RETRACTED;
			m_Core.m_HookPos = m_Core.m_Pos;
		}
		if (Config()->m_SvTeleportLoseWeapons)
		{
			for (int i = WEAPON_SHOTGUN; i < NUM_WEAPONS; i++)
				if (i != WEAPON_NINJA)
					m_aWeapons[i].m_Got = false;
		}
		return;
	}
	int evilz = GameServer()->Collision()->IsEvilTeleport(MapIndex);
	if (evilz && Controller->m_TeleOuts[evilz - 1].size())
	{
		if (m_Super)
			return;
		int Num = Controller->m_TeleOuts[evilz - 1].size();
		m_Core.m_Pos = Controller->m_TeleOuts[evilz - 1][(!Num) ? Num : rand() % Num];
		if (!Config()->m_SvOldTeleportHook && !Config()->m_SvOldTeleportWeapons)
		{
			m_Core.m_Vel = vec2(0, 0);

			if (!Config()->m_SvTeleportHoldHook)
			{
				ReleaseHook();
			}
			if (Config()->m_SvTeleportLoseWeapons)
			{
				for(int i=WEAPON_SHOTGUN;i<NUM_WEAPONS;i++)
					if (i != WEAPON_NINJA)
						m_aWeapons[i].m_Got = false;
			}
		}
		return;
	}
	if (GameServer()->Collision()->IsCheckEvilTeleport(MapIndex))
	{
		if (m_Super)
			return;
		// first check if there is a TeleCheckOut for the current recorded checkpoint, if not check previous checkpoints
		for (int k = m_TeleCheckpoint - 1; k >= 0; k--)
		{
			if (Controller->m_TeleCheckOuts[k].size())
			{
				int Num = Controller->m_TeleCheckOuts[k].size();
				m_Core.m_Pos = Controller->m_TeleCheckOuts[k][(!Num) ? Num : rand() % Num];
				m_Core.m_Vel = vec2(0, 0);

				if (!Config()->m_SvTeleportHoldHook)
				{
					ReleaseHook();
				}

				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if (GameServer()->m_pController->CanSpawn(&SpawnPos, ENTITY_SPAWN))
		{
			m_Core.m_Pos = SpawnPos;
			m_Core.m_Vel = vec2(0, 0);

			if (!Config()->m_SvTeleportHoldHook)
			{
				ReleaseHook();
			}
		}
		return;
	}
	if (GameServer()->Collision()->IsCheckTeleport(MapIndex))
	{
		if (m_Super)
			return;
		// first check if there is a TeleCheckOut for the current recorded checkpoint, if not check previous checkpoints
		for (int k = m_TeleCheckpoint - 1; k >= 0; k--)
		{
			if (Controller->m_TeleCheckOuts[k].size())
			{
				int Num = Controller->m_TeleCheckOuts[k].size();
				m_Core.m_Pos = Controller->m_TeleCheckOuts[k][(!Num) ? Num : rand() % Num];

				if (!Config()->m_SvTeleportHoldHook)
				{
					m_Core.m_HookedPlayer = -1;
					m_Core.m_HookState = HOOK_RETRACTED;
					m_Core.m_HookPos = m_Core.m_Pos;
				}

				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if (GameServer()->m_pController->CanSpawn(&SpawnPos, ENTITY_SPAWN))
		{
			m_Core.m_Pos = SpawnPos;

			if (!Config()->m_SvTeleportHoldHook)
			{
				m_Core.m_HookedPlayer = -1;
				m_Core.m_HookState = HOOK_RETRACTED;
				m_Core.m_HookPos = m_Core.m_Pos;
			}
		}
		return;
	}
}

void CCharacter::HandleTuneLayer()
{	
	m_TuneZoneOld = m_TuneZone;
	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_Pos);
	m_TuneZone = GameServer()->Collision()->IsTune(CurrentIndex);

	if (m_TuneZone)
		m_Core.m_pWorld->m_Tuning = GameServer()->TuningList()[m_TuneZone]; // throw tunings from specific zone into gamecore
	else
		m_Core.m_pWorld->m_Tuning = *GameServer()->Tuning();

	if (m_TuneZone != m_TuneZoneOld) // don't send tunigs all the time
	{
		// send zone msgs
		SendZoneMsgs();
	}
}

void CCharacter::SendZoneMsgs()
{
	// send zone leave msg
	// (m_TuneZoneOld >= 0: avoid zone leave msgs on spawn)
	if (m_TuneZoneOld >= 0 && GameServer()->m_aaZoneLeaveMsg[m_TuneZoneOld])
	{
		const char* pCur = GameServer()->m_aaZoneLeaveMsg[m_TuneZoneOld];
		const char* pPos;
		while ((pPos = str_find(pCur, "\\n")))
		{
			char aBuf[256];
			str_copy(aBuf, pCur, pPos - pCur + 1);
			aBuf[pPos - pCur + 1] = '\0';
			pCur = pPos + 2;
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		}
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), pCur);
	}
	// send zone enter msg
	if (GameServer()->m_aaZoneEnterMsg[m_TuneZone])
	{
		const char* pCur = GameServer()->m_aaZoneEnterMsg[m_TuneZone];
		const char* pPos;
		while ((pPos = str_find(pCur, "\\n")))
		{
			char aBuf[256];
			str_copy(aBuf, pCur, pPos - pCur + 1);
			aBuf[pPos - pCur + 1] = '\0';
			pCur = pPos + 2;
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		}
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), pCur);
	}
}

void CCharacter::DDraceTick()
{
	if (!m_pPlayer->m_IsDummy || m_pPlayer->m_TeeControllerID != -1)
		mem_copy(&m_Input, &m_SavedInput, sizeof(m_Input));

	if (m_Input.m_Direction != 0 || m_Input.m_Jump != 0)
		m_LastMove = Server()->Tick();

	if (m_FreezeTime > 0 || m_FreezeTime == -1)
	{
		if (m_FreezeTime % Server()->TickSpeed() == Server()->TickSpeed() - 1 || m_FreezeTime == -1)
		{
			GameServer()->CreateDamage(m_Pos, m_pPlayer->GetCID(), vec2(0, 0), (m_FreezeTime + 1) / Server()->TickSpeed(), 0, true, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
		}
		if (m_FreezeTime > 0)
			m_FreezeTime--;
		else
			m_Ninja.m_ActivationTick = Server()->Tick();
		m_Input.m_Direction = 0;
		m_Input.m_Jump = 0;
		m_Input.m_Hook = 0;
		if (m_FreezeTime == 1)
			UnFreeze();
	}

	HandleTuneLayer(); // need this before coretick

	// look for save position for rescue feature
	if(Config()->m_SvRescue || (Team() > TEAM_FLOCK && Team() < TEAM_SUPER)) {
		int index = GameServer()->Collision()->GetPureMapIndex(m_Pos);
		int tile = GameServer()->Collision()->GetTileIndex(index);
		int ftile = GameServer()->Collision()->GetFTileIndex(index);
		if (IsGrounded() && tile != TILE_FREEZE && tile != TILE_DFREEZE && ftile != TILE_FREEZE && ftile != TILE_DFREEZE) {
			m_PrevSavePos = m_Pos;
			for(int i = 0; i< NUM_WEAPONS; i++)
			{
				m_aPrevSaveWeapons[i].m_AmmoRegenStart = m_aWeapons[i].m_AmmoRegenStart;
				m_aPrevSaveWeapons[i].m_Ammo = m_aWeapons[i].m_Ammo;
				m_aPrevSaveWeapons[i].m_Got = m_aWeapons[i].m_Got;
			}
			m_SetSavePos = true;
		}
	}

	CheckMoved();

	m_Core.m_Id = GetPlayer()->GetCID();
}

void CCharacter::DDracePostCoreTick()
{
	m_IsFrozen = false;

	m_Time = (float)(Server()->Tick() - m_StartTime) / ((float)Server()->TickSpeed());

	if (m_pPlayer->m_DefEmoteReset >= 0 && m_pPlayer->m_DefEmoteReset <= Server()->Tick())
	{
		m_pPlayer->m_DefEmoteReset = -1;
		m_EmoteType = m_pPlayer->m_DefEmote = EMOTE_NORMAL;
		m_EmoteStop = -1;
	}

	if (m_EndlessHook || (m_Super && Config()->m_SvEndlessSuperHook))
		m_Core.m_HookTick = 0;

	m_FrozenLastTick = false;

	if ((m_DeepFreeze || m_pPlayer->m_ClanProtectionPunished) && !m_Super)
		Freeze();

	if (m_Core.m_Jumps == 0 && !m_Super)
		m_Core.m_Jumped = 3;
	else if (m_Core.m_Jumps == 1 && m_Core.m_Jumped > 0)
		m_Core.m_Jumped = 3;
	else if (m_Core.m_JumpedTotal < m_Core.m_Jumps - 1 && m_Core.m_Jumped > 1)
		m_Core.m_Jumped = 1;

	if ((m_Super || m_SuperJump) && m_Core.m_Jumped > 1)
		m_Core.m_Jumped = 1;

	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_Pos);
	HandleSkippableTiles(CurrentIndex);
	if (!m_Alive)
		return;

	// handle Anti-Skip tiles
	std::list < int > Indices = GameServer()->Collision()->GetMapIndices(m_PrevPos, m_Pos);
	if (!Indices.empty())
	{
		for (std::list < int >::iterator i = Indices.begin(); i != Indices.end(); i++)
		{
			HandleTiles(*i);
			if (!m_Alive)
				return;
		}
	}
	else
	{
		HandleTiles(CurrentIndex);
		m_LastIndexTile = 0;
		m_LastIndexFrontTile = 0;
		if (!m_Alive)
			return;
	}

	// teleport gun
	if (m_TeleGunTeleport)
	{
		GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID(), Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
		m_Core.m_Pos = m_TeleGunPos;
		if (!m_IsBlueTeleGunTeleport)
			m_Core.m_Vel = vec2(0, 0);
		GameServer()->CreateDeath(m_TeleGunPos, m_pPlayer->GetCID(), Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
		GameServer()->CreateSound(m_TeleGunPos, SOUND_WEAPON_SPAWN, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
		m_TeleGunTeleport = false;
		m_IsBlueTeleGunTeleport = false;
	}

	HandleBroadcast();

	if (!m_IsFrozen)
		m_FirstFreezeTick = 0;
}

bool CCharacter::Freeze(float Seconds)
{
	m_IsFrozen = true;

	if ((Seconds <= 0 || m_Super || m_FreezeTime == -1 || m_FreezeTime > Seconds * Server()->TickSpeed()) && Seconds != -1)
		return false;
	if (m_FreezeTick < Server()->Tick() - Server()->TickSpeed() || Seconds == -1)
	{
		if (m_FreezeTick == 0 || m_FirstFreezeTick == 0)
			m_FirstFreezeTick = Server()->Tick();
		m_FreezeTime = Seconds == -1 ? Seconds : Seconds * Server()->TickSpeed();
		m_FreezeTick = Server()->Tick();

		GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);
		return true;
	}
	return false;
}

bool CCharacter::Freeze()
{
	return Freeze(Config()->m_SvFreezeDelay);
}

bool CCharacter::UnFreeze()
{
	if (m_FreezeTime > 0)
	{
		if(!m_aWeapons[GetActiveWeapon()].m_Got)
			SetActiveWeapon(WEAPON_GUN);
		m_FreezeTime = 0;
		m_FreezeTick = 0;
		m_FrozenLastTick = true;
		m_FirstFreezeTick = 0;

		GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);

		if (!m_GotLasered)
		{
			m_Core.m_Killer.m_ClientID = -1;
			m_Core.m_Killer.m_Weapon = -1;
		}
		else
			m_GotLasered = false;

		return true;
	}
	return false;
}

void CCharacter::Pause(bool Pause)
{
	m_Paused = Pause;
	if (Pause)
	{
		GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
		GameWorld()->RemoveEntity(this);

		if (m_Core.m_HookedPlayer != -1) // Keeping hook would allow cheats
		{
			m_Core.m_HookedPlayer = -1;
			m_Core.m_HookState = HOOK_RETRACTED;
		}
	}
	else
	{
		m_Core.m_Vel = vec2(0, 0);
		GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;
		GameWorld()->InsertEntity(this);
	}
}

void CCharacter::DDraceInit()
{
	m_LastRefillJumps = false;
	m_LastPenalty = false;
	m_LastBonus = false;

	m_HasTeleGun = false;
	m_HasTeleLaser = false;
	m_HasTeleGrenade = false;
	m_TeleGunTeleport = false;
	m_IsBlueTeleGunTeleport = false;
	m_Solo = false;

	m_Paused = false;
	m_DDRaceState = DDRACE_NONE;
	m_PrevPos = m_Pos;
	m_SetSavePos = false;
	m_LastBroadcast = 0;
	m_TeamBeforeSuper = 0;
	m_Core.m_Id = GetPlayer()->GetCID();
	m_TeleCheckpoint = 0;
	m_EndlessHook = Config()->m_SvEndlessDrag;
	m_Hit = Config()->m_SvHit ? HIT_ALL : DISABLE_HIT_GRENADE | DISABLE_HIT_HAMMER | DISABLE_HIT_RIFLE | DISABLE_HIT_SHOTGUN;
	m_SuperJump = false;
	m_Jetpack = false;
	m_Core.m_Jumps = 2;
	m_FreezeHammer = false;

	int Team = Teams()->m_Core.Team(m_Core.m_Id);

	if (Teams()->TeamLocked(Team))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (Teams()->m_Core.Team(i) == Team && i != m_Core.m_Id && GameServer()->m_apPlayers[i])
			{
				CCharacter* pChar = GameServer()->m_apPlayers[i]->GetCharacter();

				if (pChar)
				{
					m_DDRaceState = pChar->m_DDRaceState;
					m_StartTime = pChar->m_StartTime;
				}
			}
		}
	}

	if (Config()->m_SvTeam == 2 && Team == TEAM_FLOCK)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Please join a team before you start");
		m_LastStartWarning = Server()->Tick();
	}
}

void CCharacter::Rescue()
{
	if (m_SetSavePos && !m_Super) {
		if (m_LastRescue + Config()->m_SvRescueDelay * Server()->TickSpeed() > Server()->Tick())
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "You have to wait %d seconds until you can rescue yourself", (int)((m_LastRescue + Config()->m_SvRescueDelay * Server()->TickSpeed() - Server()->Tick()) / Server()->TickSpeed()));
			GameServer()->SendChatTarget(GetPlayer()->GetCID(), aBuf);
			return;
		}

		m_LastRescue = Server()->Tick();
		m_Core.m_Pos = m_PrevSavePos;
		m_Pos = m_PrevSavePos;
		m_PrevPos = m_PrevSavePos;
		m_Core.m_Vel = vec2(0, 0);
		ReleaseHook();
		m_DeepFreeze = false;
		UnFreeze();

		for(int i = 0; i< NUM_WEAPONS; i++)
		{
			m_aWeapons[i].m_AmmoRegenStart = m_aPrevSaveWeapons[i].m_AmmoRegenStart;
			m_aWeapons[i].m_Ammo = m_aPrevSaveWeapons[i].m_Ammo;
			m_aWeapons[i].m_Got = m_aPrevSaveWeapons[i].m_Got;
		}
	}
}

// F-DDrace

void CCharacter::FDDraceInit()
{
	m_LastIndexTile = 0;
	m_LastIndexFrontTile = 0;

	m_Invisible = false;
	m_Rainbow = false;
	m_Atom = false;
	m_Trail = false;
	m_Meteors = 0;
	m_Bloody = false;
	m_StrongBloody = false;
	m_ScrollNinja = false;
	m_HookPower = HOOK_NORMAL;
	m_IsRainbowHooked = false;
	for (int i = 0; i < NUM_WEAPONS; i++)
		m_aSpreadWeapon[i] = false;
	m_FakeTuneCollision = false;
	m_OldFakeTuneCollision = false;
	m_Passive = false;
	m_pPassiveShield = 0;
	m_PoliceHelper = false;
	m_pTelekinesisEntity = 0;
	m_pLightsaber = 0;
	m_Item = -3;
	m_pItem = 0;
	m_DoorHammer = false;

	m_AlwaysTeleWeapon = Config()->m_SvAlwaysTeleWeapon;

	m_pPlayer->m_Gamemode = (Config()->m_SvVanillaModeStart || m_pPlayer->m_Gamemode == GAMEMODE_VANILLA) ? GAMEMODE_VANILLA : GAMEMODE_DDRACE;
	m_SavedGamemode = m_pPlayer->m_Gamemode;
	m_Armor = m_pPlayer->m_Gamemode == GAMEMODE_VANILLA ? 0 : 10;

	m_NumGhostShots = 0;

	int64 Now = Server()->Tick();

	if (m_pPlayer->m_HasRoomKey)
		m_Core.m_MoveRestrictionExtra.m_CanEnterRoom = true;
	m_RoomAntiSpamTick = Now;

	m_CollectedPortalRifle = false;

	m_InitializedSpawnWeapons = false;
	for (int i = 0; i < 3; i++)
		m_aSpawnWeaponActive[i] = false;

	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[m_pPlayer->GetAccID()];
	if (!m_pPlayer->IsMinigame() && !m_pPlayer->m_JailTime)
	{
		for (int i = 0; i < 3; i++)
			if (pAccount->m_SpawnWeapon[i])
			{
				m_aSpawnWeaponActive[i] = true;
				GiveWeapon(i == 0 ? WEAPON_SHOTGUN : i == 1 ? WEAPON_GRENADE : WEAPON_LASER, false, pAccount->m_SpawnWeapon[i]);
			}
		m_InitializedSpawnWeapons = true;

		if (pAccount->m_PortalRifle)
			GiveWeapon(WEAPON_PORTAL_RIFLE, false, -1, true);
	}

	m_HasFinishedSpecialRace = false;
	m_GotMoneyXPBomb = false;
	m_SpawnTick = Now;
	m_MoneyTile = false;
	m_GotLasered = false;
	m_KillStreak = 0;
	m_pTeeControlCursor = 0;

	for (int i = 0; i < NUM_HOUSES; i++)
		GameServer()->m_pHouses[i]->Reset(m_pPlayer->GetCID());

	m_LastTouchedSwitcher = -1;
	m_LastTouchedPortalBy = -1;

	m_LastLinkedPortals = Now;
	m_LastWantedWeapon = 0;
	m_LastWantedLogout = 0;
	m_MaxJumps = m_Core.m_Jumps;

	m_LastMoneyDrop = 0;

	for (int i = 0; i < NUM_WEAPONS; i++)
		m_aHadWeapon[i] = false;

	// Set this to MINIGAME_NONE so we dont have a timer when we want to leave a minigame, just when we enter
	m_RequestedMinigame = MINIGAME_NONE;
	m_LastMinigameRequest = 0;

	m_pDummyHandle = 0;
	CreateDummyHandle(m_pPlayer->GetDummyMode());
}

void CCharacter::CreateDummyHandle(int Dummymode)
{
	if (m_pDummyHandle)
		delete m_pDummyHandle;
	m_pDummyHandle = 0;

	switch (Dummymode)
	{
	case DUMMYMODE_IDLE: m_pDummyHandle = new CDummyBase(this, DUMMYMODE_IDLE); break;
	case DUMMYMODE_BLMAPCHILL_POLICE: m_pDummyHandle = new CDummyBlmapChillPolice(this); break;
	case DUMMYMODE_SHOP_DUMMY: // fallthrough
	case DUMMYMODE_PLOT_SHOP_DUMMY: // fallthrough
	case DUMMYMODE_BANK_DUMMY: m_pDummyHandle = new CDummyHouse(this, Dummymode); break;
	case DUMMYMODE_V3_BLOCKER: m_pDummyHandle = new CDummyV3Blocker(this); break;
	case DUMMYMODE_CHILLBLOCK5_POLICE: m_pDummyHandle = new CDummyChillBlock5Police(this); break;
	}
}

void CCharacter::FDDraceTick()
{
	// set cursorpos
	m_CursorPos = vec2(m_Pos.x+m_Input.m_TargetX, m_Pos.y+m_Input.m_TargetY);

	// fake tune collision
	if (!Server()->IsSevendown(m_pPlayer->GetCID()))
	{
		CCharacter* pChr = GameWorld()->ClosestCharacter(m_Pos, GetProximityRadius()*2 + 10.f, this);
		m_FakeTuneCollision = (!m_Super && (m_Solo || m_Passive)) || (pChr && !pChr->m_Super && (pChr->m_Solo || pChr->m_Passive || (Team() != pChr->Team() && m_pPlayer->m_ShowOthers)));

		if (m_FakeTuneCollision != m_OldFakeTuneCollision)
			GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);

		m_OldFakeTuneCollision = m_FakeTuneCollision;
	}

	// update telekinesis entitiy position
	if (m_pTelekinesisEntity)
	{
		if (GetActiveWeapon() == WEAPON_TELEKINESIS && !m_FreezeTime && !m_pPlayer->IsPaused())
		{
			vec2 Vel = vec2(0.f, 0.f);

			switch (m_pTelekinesisEntity->GetObjType())
			{
				case (CGameWorld::ENTTYPE_CHARACTER): 
				{
					CCharacter *pChr = (CCharacter *)m_pTelekinesisEntity;
					pChr->Core()->m_Pos = m_CursorPos;
					pChr->Core()->m_Vel = Vel;
					break;
				}
				case (CGameWorld::ENTTYPE_FLAG):
				{
					CFlag *pFlag = (CFlag *)m_pTelekinesisEntity;
					pFlag->SetPos(m_CursorPos);
					pFlag->SetVel(Vel);
					break;
				}
				case (CGameWorld::ENTTYPE_PICKUP_DROP):
				{
					CPickupDrop *pPickup = (CPickupDrop *)m_pTelekinesisEntity;
					pPickup->SetPos(m_CursorPos);
					pPickup->SetVel(Vel);
					break;
				}
				case (CGameWorld::ENTTYPE_MONEY):
				{
					CMoney *pMoney = (CMoney *)m_pTelekinesisEntity;
					pMoney->SetPos(m_CursorPos);
					pMoney->SetVel(Vel);
					break;
				}
			}
		}
		else
			m_pTelekinesisEntity = 0;
	}

	// retract lightsaber
	if (m_pLightsaber && (m_FreezeTime || GetActiveWeapon() != WEAPON_LIGHTSABER))
		m_pLightsaber->Retract();

	// flag bonus
	if (HasFlag() != -1 && Server()->Tick() % 50 == 0)
	{
		CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[m_pPlayer->GetAccID()];

		if (m_pPlayer->GetAccID() >= ACC_START)
		{
			if (!m_MoneyTile)
			{
				int AliveState = GetAliveState();
				int XP = 0;
				XP += AliveState + 1;

				if (pAccount->m_VIP)
					XP += 2;

				m_pPlayer->GiveXP(XP);

				char aSurvival[32];
				char aMsg[128];
				str_format(aSurvival, sizeof(aSurvival), " +%d survival", AliveState);
				str_format(aMsg, sizeof(aMsg), " \n \nXP [%lld/%lld] +1 flag%s%s",
					pAccount->m_XP, GameServer()->GetNeededXP(pAccount->m_Level), pAccount->m_VIP ? " +2 vip" : "", AliveState ? aSurvival : "");

				GameServer()->SendBroadcast(GameServer()->FormatExperienceBroadcast(aMsg, m_pPlayer->GetCID()), m_pPlayer->GetCID(), false);
			}
		}
	}

	// stop spinning when we are paused
	if (m_pPlayer->IsPaused())
		m_Core.m_UpdateAngle = UPDATE_ANGLE_TIME;

	// set aim bot pos
	if (m_Core.m_AimClosest)
	{
		CCharacter *pClosest = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID());
		m_Core.m_AimClosestPos = pClosest ? pClosest->m_Pos : vec2(0, 0);
	}

	// set cursor pos when controlling another tee
	if (m_pTeeControlCursor && m_pPlayer->m_pControlledTee)
	{
		CCharacter *pControlledTee = m_pPlayer->m_pControlledTee->GetCharacter();
		if (pControlledTee)
			m_pTeeControlCursor->SetPos(pControlledTee->m_CursorPos);
	}

	if (Server()->Tick() % 50 == 0 && SendingPortalCooldown())
	{
		char aBuf[64];
		int Seconds = Config()->m_SvPortalRifleDelay - ((Server()->Tick() - m_LastLinkedPortals) / Server()->TickSpeed());

		if (Seconds <= 0)
			str_copy(aBuf, "[Portals unlocked]", sizeof(aBuf));
		else
			str_format(aBuf, sizeof(aBuf), "[Next portal: %ds]", Seconds);
		GameServer()->SendBroadcast(aBuf, m_pPlayer->GetCID(), false);
	}

	if (m_IsRainbowHooked && !m_pPlayer->IsHooked(RAINBOW))
	{
		m_IsRainbowHooked = false;
		m_pPlayer->ResetSkin();
	}

	// update
	m_DrawEditor.Tick();
	if (m_pDummyHandle)
		m_pDummyHandle->Tick();
}

void CCharacter::HandleLastIndexTiles()
{
	for (int i = 0; i < NUM_HOUSES; i++)
	{
		int Index = -1;
		switch (i)
		{
		case HOUSE_SHOP: Index = TILE_SHOP; break;
		case HOUSE_PLOT_SHOP: Index = TILE_PLOT_SHOP; break;
		case HOUSE_BANK: Index = TILE_BANK; break;
		}

		if (m_TileIndex != Index && m_TileFIndex != Index)
			GameServer()->m_pHouses[i]->OnLeave(m_pPlayer->GetCID());
	}

	if (m_MoneyTile)
	{
		if (m_TileIndex != TILE_MONEY && m_TileFIndex != TILE_MONEY && m_TileIndex != TILE_MONEY_POLICE && m_TileFIndex != TILE_MONEY_POLICE)
		{
			GameServer()->SendBroadcast("", m_pPlayer->GetCID(), false);
			m_MoneyTile = false;
		}
	}
}

bool CCharacter::SendingPortalCooldown()
{
	return GetActiveWeapon() == WEAPON_PORTAL_RIFLE && (m_LastLinkedPortals + Server()->TickSpeed() * (Config()->m_SvPortalRifleDelay+1) > Server()->Tick());
}

bool CCharacter::RequestMinigameChange(int RequestedMinigame)
{
	if (m_LastMinigameRequest && m_LastMinigameRequest > Server()->Tick() - Server()->TickSpeed() * 5)
		return true;

	if (RequestedMinigame == m_RequestedMinigame)
		return false; // only return false here, to actually join the minigame

	if (m_FreezeTime)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You can't join a minigame while being frozen");
		return true;
	}

	if (m_pPlayer->m_JailTime)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You can't join a minigame while being arrested");
		return true;
	}

	m_RequestedMinigame = RequestedMinigame;
	m_LastMinigameRequest = Server()->Tick();
	GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Minigame request sent, please don't move for 5 seconds");
	return true;
}

bool CCharacter::MinigameRequestTick()
{
	if (!m_LastMinigameRequest || m_RequestedMinigame == m_pPlayer->m_Minigame)
		return false;

	if (m_LastMinigameRequest < Server()->Tick() - Server()->TickSpeed() * 5)
	{
		GameServer()->SetMinigame(m_pPlayer->GetCID(), m_RequestedMinigame);
		m_RequestedMinigame = MINIGAME_NONE;
		m_LastMinigameRequest = 0;
		return true;
	}
	else if (m_Pos != m_PrevPos)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Your minigame request was cancelled because you moved");
		m_RequestedMinigame = MINIGAME_NONE;
		m_LastMinigameRequest = 0;
	}
	else if ((Server()->Tick() - m_LastMinigameRequest - 1) % Server()->TickSpeed() == 0)
	{
		int Remaining = ((m_LastMinigameRequest + Server()->TickSpeed() * 5) - Server()->Tick()) / Server()->TickSpeed();
		char aBuf[4];
		str_format(aBuf, sizeof(aBuf), "%d", Remaining+1);
		GameServer()->CreateLaserText(m_Pos, m_pPlayer->GetCID(), aBuf, 1);
	}

	return false;
}

int CCharacter::GetCurrentTilePlotID()
{
	return GameServer()->GetTilePlotID(m_Pos);
}

void CCharacter::TeleOutOfPlot(int PlotID)
{
	if (PlotID < PLOT_START)
		return;

	if (m_pPlayer->IsMinigame() && m_pPlayer->m_SavedMinigameTee)
	{
		vec2 SavedPos = m_pPlayer->m_MinigameTee.GetPos();
		int SavedTilePlotID = GameServer()->GetTilePlotID(SavedPos);
		int SavedPlotDoor = GameServer()->Collision()->GetPlotBySwitch(GameServer()->Collision()->GetDoorNumber(SavedPos));

		if (SavedTilePlotID == PlotID || SavedPlotDoor == PlotID)
			m_pPlayer->m_MinigameTee.TeleOutOfPlot(GameServer()->m_aPlots[PlotID].m_ToTele);
	}

	int TilePlotID = GetCurrentTilePlotID();
	int PlotDoor = GameServer()->Collision()->GetPlotBySwitch(GameServer()->Collision()->GetDoorNumber(m_Pos));
	if (TilePlotID == PlotID || PlotDoor == PlotID)
	{
		m_Core.m_Pos = m_Pos = m_PrevPos = GameServer()->m_aPlots[PlotID].m_ToTele;
		GiveWeapon(WEAPON_DRAW_EDITOR, true);
	}
}

void CCharacter::BackupWeapons(int Type)
{
	if (!m_WeaponsBackupped[Type])
	{
		for (int i = 0; i < NUM_WEAPONS; i++)
		{
			m_aWeaponsBackupGot[i][Type] = m_aWeapons[i].m_Got;
			m_aWeaponsBackup[i][Type] = m_aWeapons[i].m_Ammo;
		}
		m_WeaponsBackupped[Type] = true;
	}
}

void CCharacter::LoadWeaponBackup(int Type)
{
	if (m_WeaponsBackupped[Type])
	{
		for (int i = 0; i < NUM_WEAPONS; i++)
		{
			m_aWeapons[i].m_Got = m_aWeaponsBackupGot[i][Type];
			m_aWeapons[i].m_Ammo = m_aWeaponsBackup[i][Type];
			if (i == WEAPON_NINJA)
				m_aWeapons[i].m_Ammo = -1;
		}
		m_WeaponsBackupped[Type] = false;
	}
}

void CCharacter::SetAvailableWeapon(int PreferedWeapon)
{
	if (GetWeaponGot(PreferedWeapon))
	{
		SetWeapon(PreferedWeapon);
		return;
	}

	for (int i = 0; i < NUM_WEAPONS; i++)
	{
		if (i == WEAPON_NINJA || (i == WEAPON_DRAW_EDITOR && m_DrawEditor.Active()))
			continue;

		if (GetWeaponGot(i))
		{
			SetWeapon(i);
			break;
		}
	}
}

void CCharacter::SetLastTouchedSwitcher(int Number)
{
	if (Number <= 0 || Team() == TEAM_SUPER)
		return;

	int SwitchID = GameServer()->Collision()->m_pSwitchers[Number].m_ClientID[Team()];
	if (SwitchID >= 0 && GameServer()->m_apPlayers[SwitchID])
	{
		Core()->m_Killer.m_ClientID = SwitchID;
		Core()->m_Killer.m_Weapon = -1;
		m_LastTouchedSwitcher = Number;
	}
}

void CCharacter::DropMoney(int64 Amount, int Dir)
{
	if (Amount <= 0 || Amount > m_pPlayer->GetWalletMoney())
		return;

	if (Dir == -3)
		Dir = GetAimDir();
	else
		Dir = ((rand() % 50 - 25 + 1) * 0.1); // in a range of -2.5 to +2.5
	new CMoney(GameWorld(), m_Pos, Amount, m_pPlayer->GetCID(), Dir);
	m_pPlayer->WalletTransaction(-Amount, "dropped");

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "-%lld", Amount);
	GameServer()->CreateLaserText(m_Pos, m_pPlayer->GetCID(), aBuf, GameServer()->MoneyLaserTextTime(Amount));
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
}

void CCharacter::DropFlag()
{
	for (int i = 0; i < 2; i++)
	{
		CFlag *F = ((CGameControllerDDRace*)GameServer()->m_pController)->m_apFlags[i];
		if (F && F->GetCarrier() == this)
			F->Drop(GetAimDir());
	}
}

void CCharacter::DropWeapon(int WeaponID, bool OnDeath, float Dir)
{
	// Do not drop spawnweapons
	int W = GetSpawnWeaponIndex(WeaponID);
	if (W != -1 && m_aSpawnWeaponActive[W])
		return;

	if ((!OnDeath && (m_FreezeTime || !Config()->m_SvDropWeapons)) || Config()->m_SvMaxWeaponDrops == 0 || !m_aWeapons[WeaponID].m_Got
		|| (WeaponID == WEAPON_NINJA && !m_ScrollNinja) || (WeaponID == WEAPON_PORTAL_RIFLE && !m_CollectedPortalRifle) || WeaponID == WEAPON_DRAW_EDITOR || (WeaponID == WEAPON_TASER && GetWeaponAmmo(WeaponID) == 0))
		return;

	int Count = 0;
	for (int i = 0; i < NUM_WEAPONS; i++)
		if (i != WEAPON_NINJA && i != WEAPON_TASER && (i != WEAPON_PORTAL_RIFLE || m_CollectedPortalRifle) && i != WEAPON_DRAW_EDITOR && m_aWeapons[i].m_Got)
			Count++;
	if (Count < 2)
		return;

	if (m_pPlayer->m_vWeaponLimit[WeaponID].size() == (unsigned)Config()->m_SvMaxWeaponDrops)
	{
		if (WeaponID == WEAPON_TASER)
			return; // make sure we dont destroy valuable taser battery drops
		m_pPlayer->m_vWeaponLimit[WeaponID][0]->Reset(false);
	}

	int Special = GetWeaponSpecial(WeaponID);
	int Type = POWERUP_WEAPON;
	int Ammo = GetWeaponAmmo(WeaponID);

	if (WeaponID == WEAPON_TASER)
	{
		Type = POWERUP_BATTERY;
		Ammo = OnDeath ? GetAliveState() : min(GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_TaserBattery, 10);

		if (!m_pPlayer->GiveTaserBattery(-Ammo))
			return;

		UpdateWeaponIndicator();
	}

	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
	CPickupDrop *pWeapon = new CPickupDrop(GameWorld(), m_Pos, Type, m_pPlayer->GetCID(), Dir == -3 ? GetAimDir() : Dir, 300, WeaponID, Ammo, Special);
	m_pPlayer->m_vWeaponLimit[WeaponID].push_back(pWeapon);

	if (Special == 0)
	{
		if (WeaponID != WEAPON_TASER)
		{
			GiveWeapon(WeaponID, true);
			SetWeapon(WEAPON_GUN);
		}
	}
	if (Special&SPECIAL_SPREADWEAPON)
		SpreadWeapon(WeaponID, false, -1, OnDeath);
	if (Special&SPECIAL_JETPACK)
		Jetpack(false, -1, OnDeath);
	if (Special&SPECIAL_TELEWEAPON)
		TeleWeapon(WeaponID, false, -1, OnDeath);
	if (Special&SPECIAL_DOORHAMMER)
		DoorHammer(false, -1, OnDeath);
	if (Special&SPECIAL_SCROLLNINJA)
		ScrollNinja(false, -1, OnDeath);
}

void CCharacter::DropPickup(int Type, int Amount)
{
	if (Type > POWERUP_ARMOR || Config()->m_SvMaxPickupDrops == 0 || Amount <= 0)
		return;

	for (int i = 0; i < Amount; i++)
	{
		if (GameServer()->m_vPickupDropLimit.size() == (unsigned)Config()->m_SvMaxPickupDrops)
			GameServer()->m_vPickupDropLimit[0]->Reset(false);

		float Dir = ((rand() % 50 - 25) * 0.1); // in a range of -2.5 to +2.5
		CPickupDrop *pPickupDrop = new CPickupDrop(GameWorld(), m_Pos, Type, m_pPlayer->GetCID(), Dir);
		GameServer()->m_vPickupDropLimit.push_back(pPickupDrop);
	}
	GameServer()->CreateSound(m_Pos, Type == POWERUP_HEALTH ? SOUND_PICKUP_HEALTH : SOUND_PICKUP_ARMOR, Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID()));
}

void CCharacter::DropLoot(int Weapon)
{
	if (!Config()->m_SvDropsOnDeath)
		return;

	// Drop money even if killed by the game, e.g. team change, but never when leaving a minigame (joining and being frozen drops too)
	if ((Weapon != WEAPON_MINIGAME_CHANGE || m_RequestedMinigame != MINIGAME_NONE) && m_FreezeTime)
		DropMoney(m_pPlayer->GetWalletMoney());

	if (Weapon == WEAPON_GAME || Weapon == WEAPON_MINIGAME_CHANGE)
		return;

	if ((m_pPlayer->m_Minigame == MINIGAME_SURVIVAL && m_pPlayer->m_SurvivalState > SURVIVAL_LOBBY)
		|| (m_pPlayer->m_Minigame != MINIGAME_SURVIVAL && m_pPlayer->m_Gamemode == GAMEMODE_VANILLA))
	{
		// drop 0 to 5 armor and hearts
		DropPickup(POWERUP_HEALTH, rand() % 6);
		DropPickup(POWERUP_ARMOR, rand() % 6);

		// drop all your weapons, in various directions (excluding hammer, ninja and extra weapons)
		for (int i = WEAPON_GUN; i < NUM_VANILLA_WEAPONS-1; i++)
		{
			float Dir = ((rand() % 50 - 25 + 1) * 0.1); // in a range of -2.5 to +2.5
			DropWeapon(i, true, Dir);
		}
	}
	else if (!m_pPlayer->IsMinigame())
	{
		// we dont want to spam spawn with hundreds of weapons
		if (GetAliveState())
		{
			// up to two normal weapons
			for (int i = 0; i < 2; i++)
			{
				int Weapon = rand() % NUM_VANILLA_WEAPONS;
				if ((Weapon == WEAPON_GUN || Weapon == WEAPON_HAMMER) && GetWeaponSpecial(Weapon) == 0)
					continue;

				float Dir = ((rand() % 50 - 25 + 1) * 0.1); // in a range of -2.5 to +2.5
				DropWeapon(Weapon, true, Dir);
			}

			// up to one extra weapon
			float Dir = ((rand() % 50 - 25 + 1) * 0.1); // in a range of -2.5 to +2.5
			DropWeapon((rand() % (NUM_WEAPONS-NUM_VANILLA_WEAPONS)) + NUM_VANILLA_WEAPONS, true, Dir);
		}
	}
}

int CCharacter::GetWeaponSpecial(int Type)
{
	int Special = 0;
	if (Type == WEAPON_GUN && m_Jetpack)
		Special |= SPECIAL_JETPACK;
	if (m_aSpreadWeapon[Type])
		Special |= SPECIAL_SPREADWEAPON;
	if ((Type == WEAPON_GUN && m_HasTeleGun) || (Type == WEAPON_GRENADE && m_HasTeleGrenade) || (Type == WEAPON_LASER && m_HasTeleLaser))
		Special |= SPECIAL_TELEWEAPON;
	if (Type == WEAPON_HAMMER && m_DoorHammer)
		Special |= SPECIAL_DOORHAMMER;
	if (Type == WEAPON_NINJA && m_ScrollNinja)
		Special |= SPECIAL_SCROLLNINJA;
	return Special;
}

void CCharacter::SetSpookyGhost()
{
	if (m_pPlayer->m_SpookyGhost || m_pPlayer->IsMinigame())
		return;

	BackupWeapons(BACKUP_SPOOKY_GHOST);
	for (int i = 0; i < NUM_WEAPONS; i++)
		if (GameServer()->GetWeaponType(i) != WEAPON_GUN)
			m_aWeapons[i].m_Got = false;
	m_pPlayer->m_ShowName = false;
	m_pPlayer->SetSkin(SKIN_SPOOKY_GHOST);
	m_pPlayer->m_SpookyGhost = true; // set m_SpookyGhost after we set the skin
}

void CCharacter::UnsetSpookyGhost()
{
	if (!m_pPlayer->m_SpookyGhost)
		return;

	LoadWeaponBackup(BACKUP_SPOOKY_GHOST);
	m_pPlayer->m_ShowName = true;
	m_pPlayer->m_SpookyGhost = false; // set m_SpookyGhost before we reset the skin
	m_pPlayer->ResetSkin();
}

void CCharacter::SetActiveWeapon(int Weapon)
{
	m_ActiveWeapon = Weapon;
	UpdateWeaponIndicator();
	m_DrawEditor.OnWeaponSwitch();
}

int CCharacter::GetWeaponAmmo(int Type)
{
	if (m_pPlayer->m_SpookyGhost)
		return m_aWeaponsBackup[Type][BACKUP_SPOOKY_GHOST];
	return m_aWeapons[Type].m_Ammo;
}

void CCharacter::SetWeaponAmmo(int Type, int Value)
{
	if (m_pPlayer->m_SpookyGhost)
		m_aWeaponsBackup[Type][BACKUP_SPOOKY_GHOST] = Value;
	m_aWeapons[Type].m_Ammo = Value;
}

void CCharacter::SetWeaponGot(int Type, bool Value)
{
	if (m_pPlayer->m_SpookyGhost)
		m_aWeaponsBackupGot[Type][BACKUP_SPOOKY_GHOST] = Value;
	m_aWeapons[Type].m_Got = Value;
}

int CCharacter::GetSpawnWeaponIndex(int Weapon)
{
	switch (Weapon)
	{
	case WEAPON_SHOTGUN: return 0;
	case WEAPON_GRENADE: return 1;
	case WEAPON_LASER: return 2;
	}
	return -1;
}

void CCharacter::UpdateWeaponIndicator()
{
	if (!m_pPlayer->m_WeaponIndicator || m_MoneyTile
		|| (m_pPlayer->m_Minigame == MINIGAME_SURVIVAL && GameServer()->m_SurvivalBackgroundState < BACKGROUND_DEATHMATCH_COUNTDOWN))
		return;
	for (int i = 0; i < NUM_HOUSES; i++)
		if (GameServer()->m_pHouses[i]->IsInside(m_pPlayer->GetCID()))
			return;

	char aTaserBattery[16] = "";
	if (GetActiveWeapon() == WEAPON_TASER)
		str_format(aTaserBattery, sizeof(aTaserBattery), " [%d]", GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_TaserBattery);

	char aBuf[256];
	if (Server()->IsSevendown(m_pPlayer->GetCID()))
	{
		str_format(aBuf, sizeof(aBuf), "Weapon: %s%s", GameServer()->GetWeaponName(GetActiveWeapon()), aTaserBattery);
		for (int i = 0; i < 128; i++)
			str_append(aBuf, " ", sizeof(aBuf));
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), " \n \n> %s%s <", GameServer()->GetWeaponName(GetActiveWeapon()), aTaserBattery);
	}
	GameServer()->SendBroadcast(aBuf, m_pPlayer->GetCID(), false);
}

int CCharacter::HasFlag()
{
	return ((CGameControllerDDRace*)GameServer()->m_pController)->HasFlag(this);
}

void CCharacter::CheckMoved()
{
	if (!m_pPlayer->m_ResumeMoved || !m_pPlayer->IsPaused() || m_PrevPos == m_Pos)
		return;

	m_pPlayer->Pause(CPlayer::PAUSE_NONE, false);
}

void CCharacter::ForceSetPos(vec2 Pos)
{
	int CurrentPlotID = GetCurrentTilePlotID();
	if (CurrentPlotID >= PLOT_START && CurrentPlotID != GameServer()->GetTilePlotID(Pos))
		m_pPlayer->StopPlotEditing();
	m_Core.m_Pos = m_Pos = m_PrevPos = Pos;
}

int CCharacter::GetAliveState()
{
	for (int i = 4; i > 0; i--)
	{
		int Offset=i<3?0:1;
		int Seconds = 300*(i*(i+Offset)); // 300 (5min), 1200 (20min), 3600 (60min), 6000 (100min)
		if (Server()->Tick() >= m_SpawnTick + Server()->TickSpeed() * Seconds)
			return i;
	}
	return 0;
}

void CCharacter::SetTeeControlCursor()
{
	if (m_pTeeControlCursor || !m_pPlayer->m_pControlledTee || Server()->IsSevendown(m_pPlayer->GetCID()))
		return;

	m_pTeeControlCursor = new CStableProjectile(GameWorld(), WEAPON_SHOTGUN, m_pPlayer->GetCID(), vec2(), false, true);
}

void CCharacter::RemoveTeeControlCursor()
{
	if (!m_pTeeControlCursor)
		return;

	m_pTeeControlCursor->Reset();
	m_pTeeControlCursor = 0;
}

void CCharacter::OnPlayerHook()
{
	CCharacter *pHookedTee = GameServer()->GetPlayerChar(m_Core.m_HookedPlayer);
	if (!pHookedTee)
		return;

	// set hook extra stuff
	if (m_HookPower == ATOM && !pHookedTee->m_Atom)
		new CAtom(GameWorld(), pHookedTee->m_Pos, m_Core.m_HookedPlayer);
	if (m_HookPower == TRAIL && !pHookedTee->m_Trail)
		new CTrail(GameWorld(), pHookedTee->m_Pos, m_Core.m_HookedPlayer);
}

void CCharacter::ReleaseHook(bool Other)
{
	m_Core.m_HookedPlayer = -1;
	m_Core.m_HookState = HOOK_RETRACTED;
	m_Core.m_HookPos = m_Core.m_Pos;
	if (Other)
		GameWorld()->ReleaseHooked(GetPlayer()->GetCID());
}

void CCharacter::WeaponMoneyReward(int Weapon)
{
	if (m_aHadWeapon[Weapon])
		return;
	m_aHadWeapon[Weapon] = true;
	m_pPlayer->WalletTransaction(1);
}

void CCharacter::Jetpack(bool Set, int FromID, bool Silent)
{
	m_Jetpack = Set;
	GameServer()->SendExtraMessage(JETPACK, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::Rainbow(bool Set, int FromID, bool Silent)
{
	m_Rainbow = Set;
	m_pPlayer->m_InfRainbow = false;
	if (!Set)
		m_pPlayer->ResetSkin();
	GameServer()->SendExtraMessage(RAINBOW, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::InfRainbow(bool Set, int FromID, bool Silent)
{
	m_pPlayer->m_InfRainbow = Set;
	m_Rainbow = false;
	if (!Set)
		m_pPlayer->ResetSkin();
	GameServer()->SendExtraMessage(INF_RAINBOW, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::Atom(bool Set, int FromID, bool Silent)
{
	m_Atom = Set;
	if (Set)
		new CAtom(GameWorld(), m_Pos, m_pPlayer->GetCID());
	GameServer()->SendExtraMessage(ATOM, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::Trail(bool Set, int FromID, bool Silent)
{
	m_Trail = Set;
	if (Set)
		new CTrail(GameWorld(), m_Pos, m_pPlayer->GetCID());
	GameServer()->SendExtraMessage(TRAIL, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::SpookyGhost(bool Set, int FromID, bool Silent)
{
	m_pPlayer->m_HasSpookyGhost = Set;
	GameServer()->SendExtraMessage(SPOOKY_GHOST, m_pPlayer->GetCID(), Set, FromID, Silent);
	if (!Silent && Set)
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "For more info, say '/spookyghostinfo'");
}

void CCharacter::Meteor(bool Set, int FromID, bool Infinite, bool Silent)
{
	if (Set)
	{
		if (m_pPlayer->m_InfMeteors + m_Meteors >= 50)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You already have the maximum of 50 meteors");
			return;
		}

		vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));
		vec2 ProjStartPos = m_Pos + Direction * GetProximityRadius()*0.75f;

		Infinite ? m_pPlayer->m_InfMeteors++ : m_Meteors++;
		new CMeteor(GameWorld(), ProjStartPos, m_pPlayer->GetCID(), Infinite);
	}
	else
	{
		if (!m_Meteors && !m_pPlayer->m_InfMeteors)
			return;

		m_Meteors = 0;
		m_pPlayer->m_InfMeteors = 0;
	}
	GameServer()->SendExtraMessage(Infinite ? INF_METEOR : METEOR, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::Passive(bool Set, int FromID, bool Silent)
{
	m_Passive = Set;
	Teams()->m_Core.SetPassive(m_pPlayer->GetCID(), Set);
	GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);

	m_pPassiveShield = !Set ? 0 : new CPickup(GameWorld(), m_Pos, POWERUP_ARMOR, 0, 0, 0, m_pPlayer->GetCID());
	GameServer()->SendExtraMessage(PASSIVE, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::VanillaMode(int FromID, bool Silent)
{
	if (m_pPlayer->m_Gamemode == GAMEMODE_VANILLA)
		return;

	// set a new saved gamemode, forced by a command means this is the new default gamemode for this player
	if (FromID >= 0)
		m_SavedGamemode = GAMEMODE_VANILLA;

	m_pPlayer->m_Gamemode = GAMEMODE_VANILLA;
	m_Armor = 0;
	for (int j = 0; j < NUM_BACKUPS; j++)
	{
		for (int i = 0; i < NUM_WEAPONS; i++)
		{
			if (i != WEAPON_HAMMER)
			{
				m_aWeaponsBackup[i][j] = 10;
				m_aWeapons[i].m_Ammo = 10;
			}
		}
	}
	GameServer()->SendExtraMessage(VANILLA_MODE, m_pPlayer->GetCID(), true, FromID, Silent);
}

void CCharacter::DDraceMode(int FromID, bool Silent)
{
	if (m_pPlayer->m_Gamemode == GAMEMODE_DDRACE)
		return;

	// set a new saved gamemode, forced by a command means this is the new default gamemode for this player
	if (FromID >= 0)
		m_SavedGamemode = GAMEMODE_DDRACE;

	m_pPlayer->m_Gamemode = GAMEMODE_DDRACE;
	m_Health = 10;
	m_Armor = 10;
	for (int j = 0; j < NUM_BACKUPS; j++)
	{
		for (int i = 0; i < NUM_WEAPONS; i++)
		{
			m_aWeaponsBackup[i][j] = -1;
			m_aWeapons[i].m_Ammo = -1;
		}
	}
	GameServer()->SendExtraMessage(DDRACE_MODE, m_pPlayer->GetCID(), true, FromID, Silent);
}

void CCharacter::Bloody(bool Set, int FromID, bool Silent)
{
	m_Bloody = Set;
	m_StrongBloody = false;
	GameServer()->SendExtraMessage(BLOODY, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::StrongBloody(bool Set, int FromID, bool Silent)
{
	m_StrongBloody = Set;
	m_Bloody = false;
	GameServer()->SendExtraMessage(STRONG_BLOODY, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::ScrollNinja(bool Set, int FromID, bool Silent)
{
	m_ScrollNinja = Set;
	if (Set)
		GiveNinja();
	else
		RemoveNinja();
	GameServer()->SendExtraMessage(SCROLL_NINJA, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::HookPower(int Extra, int FromID, bool Silent)
{
	if (m_HookPower == HOOK_NORMAL && Extra == HOOK_NORMAL)
		return;
	m_HookPower = Extra;
	GameServer()->SendExtraMessage(HOOK_POWER, m_pPlayer->GetCID(), true, FromID, Silent, Extra);
}

void CCharacter::EndlessHook(bool Set, int FromID, bool Silent)
{
	m_EndlessHook = Set;
	GameServer()->SendExtraMessage(ENDLESS_HOOK, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::InfiniteJumps(bool Set, int FromID, bool Silent)
{
	m_SuperJump = Set;
	GameServer()->SendExtraMessage(INFINITE_JUMPS, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::SpreadWeapon(int Type, bool Set, int FromID, bool Silent)
{
	if (Type == WEAPON_HAMMER || Type == WEAPON_NINJA || Type == WEAPON_TELEKINESIS || Type == WEAPON_LIGHTSABER || Type == WEAPON_PORTAL_RIFLE)
		return;
	m_aSpreadWeapon[Type] = Set;
	GameServer()->SendExtraMessage(SPREAD_WEAPON, m_pPlayer->GetCID(), Set, FromID, Silent, Type);
}

void CCharacter::FreezeHammer(bool Set, int FromID, bool Silent)
{
	m_FreezeHammer = Set;
	GameServer()->SendExtraMessage(FREEZE_HAMMER, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::Invisible(bool Set, int FromID, bool Silent)
{
	m_Invisible = Set;
	GameServer()->SendExtraMessage(INVISIBLE, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::Item(int Item, int FromID, bool Silent)
{
	if ((m_Item == -3 && Item == -3) || Item >= NUM_WEAPONS || Item < -3)
		return;
	int Type = Item == -2 ? POWERUP_HEALTH : Item == -1 ? POWERUP_ARMOR : Item >= 0 ? POWERUP_WEAPON : 0;
	int SubType = Item >= 0 ? Item : 0;
	m_Item = Item;
	m_pItem = Item == -3 ? 0 : new CPickup(GameWorld(), m_Pos, Type, SubType, 0, 0, m_pPlayer->GetCID());
	GameServer()->SendExtraMessage(ITEM, m_pPlayer->GetCID(), Item == -3 ? false : true, FromID, Silent, Item);
}

void CCharacter::TeleWeapon(int Type, bool Set, int FromID, bool Silent)
{
	if (Type != WEAPON_GUN && Type != WEAPON_GRENADE && Type != WEAPON_LASER)
		return;
	switch (Type)
	{
	case WEAPON_GUN: m_HasTeleGun = Set; break;
	case WEAPON_GRENADE: m_HasTeleGrenade = Set; break;
	case WEAPON_LASER: m_HasTeleLaser = Set; break;
	}
	GameServer()->SendExtraMessage(TELE_WEAPON, m_pPlayer->GetCID(), Set, FromID, Silent, Type);
}

void CCharacter::AlwaysTeleWeapon(bool Set, int FromID, bool Silent)
{
	m_AlwaysTeleWeapon = Set;
	GameServer()->SendExtraMessage(ALWAYS_TELE_WEAPON, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::DoorHammer(bool Set, int FromID, bool Silent)
{
	m_DoorHammer = Set;
	GameServer()->SendExtraMessage(DOOR_HAMMER, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::AimClosest(bool Set, int FromID, bool Silent)
{
	m_Core.m_SpinBot = false;
	m_Core.m_AimClosest = Set;
	GameServer()->SendExtraMessage(AIM_CLOSEST, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::SpinBot(bool Set, int FromID, bool Silent)
{
	m_Core.m_AimClosest = false;
	m_Core.m_SpinBot = Set;
	GameServer()->SendExtraMessage(SPIN_BOT, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::TeeControl(bool Set, int ForcedID, int FromID, bool Silent)
{
	m_pPlayer->m_HasTeeControl = Set;
	m_pPlayer->m_TeeControlForcedID = ForcedID;
	if (!Set)
		m_pPlayer->ResumeFromTeeControl();
	GameServer()->SendExtraMessage(TEE_CONTROL, m_pPlayer->GetCID(), Set, FromID, Silent);
}
