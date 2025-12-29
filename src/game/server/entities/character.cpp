/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <antibot/antibot_data.h>
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
#include "lovely.h"
#include "rotating_ball.h"
#include "epic_circle.h"
#include "staff_ind.h"
#include "flyingpoint.h"
#include "lightninglaser.h"

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
: CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER, vec2(0, 0), ms_PhysSize)
{
	m_Health = 0;
	m_Armor = 0;
	m_TriggeredEvents = 0;
	m_StrongWeakID = 0;

	// never intilize both to zero
	m_Input.m_TargetX = 0;
	m_Input.m_TargetY = -1;

	m_LatestPrevPrevInput = m_LatestPrevInput = m_LatestInput = m_PrevInput = m_SavedInput = m_Input;
}

CCharacter::~CCharacter()
{
	if (m_pDummyHandle)
		delete m_pDummyHandle;
	Server()->SnapFreeID(m_ViewCursorSnapID);
	Server()->SnapFreeID(m_PortalBlockerIndSnapID);
	for (int i = 0; i < EUntranslatedMap::NUM_IDS; i++)
		Server()->SnapFreeID(m_aUntranslatedID[i]);
	Server()->SnapFreeID(m_PassiveSnapID);
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
	m_Core.Init(&GameWorld()->m_Core, GameServer()->Collision(), &((CGameControllerDDRace*)GameServer()->m_pController)->m_Teams.m_Core, &((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts, IsSwitchActiveCb, this);
	m_Core.m_Pos = m_Pos;
	m_Core.m_Id = m_pPlayer->GetCID();
	SetActiveWeapon(WEAPON_GUN);
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;

	m_ReckoningTick = 0;
	m_SendCore = CCharacterCore();
	m_ReckoningCore = CCharacterCore();

	GameWorld()->InsertEntity(this);
	m_Alive = true;

	FDDraceInit();
	Teams()->OnCharacterSpawn(GetPlayer()->GetCID());
	GameServer()->m_pController->OnCharacterSpawn(this);
	DDraceInit();
	if (!m_pPlayer->LoadMinigameTee() && !m_pPlayer->IsMinigame())
	{
		if (m_pPlayer->m_DoubleXpLifesLeft)
		{
			m_pPlayer->UpdateDoubleXpLifes();
		}
		
		if (GameServer()->Config()->m_SvSpawnAsZombie && !m_pPlayer->m_JailTime)
		{
			SetZombieHuman(true);
		}
	}

	mem_zero(&m_LatestPrevPrevInput, sizeof(m_LatestPrevPrevInput));
	m_LatestPrevPrevInput.m_TargetY = -1;
	Antibot()->OnSpawn(m_pPlayer->GetCID());

	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(Pos));
	m_TuneZoneOld = -1; // no zone leave msg on spawn
	SendTuneMsg(GameServer()->m_aaZoneEnterMsg[m_TuneZone]); // we want a entermessage also on spawn
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
	if (W != -1 && !GetWeaponGot(W))
	{
		SetAvailableWeapon();
		return;
	}

	if(W == GetActiveWeaponUnclamped())
		return;

	m_LastWeapon = GetActiveWeaponUnclamped();
	m_QueuedWeapon = -1;
	SetActiveWeapon(W);
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH, TeamMask());

	if(GetActiveWeapon() < 0 || GetActiveWeapon() >= NUM_WEAPONS)
		SetActiveWeapon(WEAPON_GUN);
}

void CCharacter::SetSolo(bool Solo)
{
	m_Solo = Solo;
	Teams()->m_Core.SetSolo(m_pPlayer->GetCID(), Solo);
	if (m_Solo)
	{
		DropFlag(0);
		if (Config()->m_SvFlagHooking == 2 && (m_Core.m_HookedPlayer == HOOK_FLAG_BLUE || m_Core.m_HookedPlayer == HOOK_FLAG_RED))
		{
			ReleaseHook(false);
		}
	}
}

bool CCharacter::IsGrounded(bool CheckDoor)
{
	if (GameServer()->Collision()->CheckPoint(m_Pos.x + GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 5))
		return true;
	if (GameServer()->Collision()->CheckPoint(m_Pos.x - GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 5))
		return true;

	if (CheckDoor)
	{
		int MoveRestrictionsBelow = GameServer()->Collision()->GetMoveRestrictions(m_Pos + vec2(0, GetProximityRadius() / 2 + 4), 0.0f, m_Core.m_MoveRestrictionExtra);
		if (MoveRestrictionsBelow & CANTMOVE_DOWN)
		{
			return true;
		}
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
			TakeDamage(Direction * -1.0f * (Tuning()->m_JetpackStrength / 100.0f / 6.11f), vec2(0, 0), 0, m_pPlayer->GetCID(), WEAPON_GUN);
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
			GameServer()->CreateDamage(m_Pos, m_pPlayer->GetCID(), vec2(0, 0), NinjaTime / Server()->TickSpeed(), 0, true, TeamMask() & GameServer()->ClientsMaskExcludeClientVersionAndHigher(VERSION_DDNET_NEW_HUD));
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
		GameServer()->Collision()->MoveBox(IsSwitchActiveCb, this, &m_Core.m_Pos, &m_Core.m_Vel, vec2(GetProximityRadius(), GetProximityRadius()), 0.f, false, m_Core.m_MoveRestrictionExtra);

		// Don't skip no-bonus tile with ninja
		if (GameServer()->Collision()->IntersectLineNoBonus(m_Pos, m_Core.m_Pos, 0, 0, !m_NoBonusContext.m_InArea))
		{
			OnNoBonusArea(!m_NoBonusContext.m_InArea);
		}

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
				GameServer()->CreateSound(aEnts[i]->m_Pos, SOUND_NINJA_HIT, TeamMask());
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
	if (m_ReloadTimer != 0 || m_QueuedWeapon == -1 || (m_QueuedWeapon != -2 && !m_aWeapons[m_QueuedWeapon].m_Got) || (m_aWeapons[WEAPON_NINJA].m_Got && !m_ScrollNinja)
		|| m_DrawEditor.Selecting() || ((m_NumGrogsHolding || m_pHelicopter) && !m_DrawEditor.Active()) || m_BirthdayGiftEndTick)
		return;

	if (m_QueuedWeapon == -2)
	{
		SetAvailableWeapon();
		return;
	}

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
	{
		if(m_LatestInput.m_Fire & 1)
		{
			Antibot()->OnHammerFireReloading(m_pPlayer->GetCID());
		}
		return;
	}

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
		|| GetActiveWeapon() == WEAPON_TELE_RIFLE
		|| GetActiveWeapon() == WEAPON_LIGHTNING_LASER
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

	if (m_DrawEditor.Active())
	{
		if(ClickedFire)
			m_DrawEditor.OnPlayerFire();
		return;
	}

	if (m_FreezeTime)
	{
		if (m_PainSoundTimer <= 0)
		{
			m_PainSoundTimer = 1 * Server()->TickSpeed();
			GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG, TeamMask());
		}
		return;
	}

	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[m_pPlayer->GetAccID()];

	// check for ammo
	bool NoTaserAmmo = (GetActiveWeapon() == WEAPON_TASER && m_pPlayer->GetAccID() >= ACC_START && pAccount->m_TaserBattery <= 0);
	if(!m_aWeapons[GetActiveWeapon()].m_Ammo || NoTaserAmmo)
	{
		// 125ms is a magical limit of how fast a human can click
		m_ReloadTimer = 125 * Server()->TickSpeed() / 1000;
		if (m_LastNoAmmoSound + Server()->TickSpeed() <= Server()->Tick())
		{
			GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, TeamMask());
			m_LastNoAmmoSound = Server()->Tick();
		}
		return;
	}

	// F-DDrace
	vec2 ProjStartPos = m_Pos+TempDirection*GetProximityRadius()*0.75f;

	// doing this before the loop so that m_LastTaserUse is not yet updated when having spread taser
	int TaserStrength = 0;
	if (GetActiveWeapon() == WEAPON_TASER)
	{
		TaserStrength = GetTaserStrength();
		m_LastTaserUse = Server()->Tick();
	}

	int NumShots = !m_aSpreadWeapon[GetActiveWeapon()] ? 1 : clamp(round_to_int(Tuning()->m_NumSpreadShots), 2, 9);

	float Spread[] = { 0, -0.1f, 0.1f, -0.2f, 0.2f, -0.3f, 0.3f, -0.4f, 0.4f };
	if (NumShots % 2 == 0)
		for (unsigned int i = 0; i < (sizeof(Spread)/sizeof(*Spread)); i++)
			Spread[i] += 0.05f;

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
				if (m_NumGrogsHolding)
				{
					if (m_pGrog)
					{
						m_pGrog->OnSip();
					}
					break;
				}
				else if (m_IsPortalBlocker)
				{
					// Only process portal blocker when scoreboard is closed, as it is used for toggling so we dont place accidentally
					if (!(m_pPlayer->m_PlayerFlags&PLAYERFLAG_SCOREBOARD) && (!pAccount->m_PortalBlocker || !m_pPortalBlocker || !m_pPortalBlocker->OnPlace()))
					{
						GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, TeamMask());
						return;
					}
					break;
				}

				if (Sound)
					GameServer()->CreateSound(m_Pos, SOUND_HAMMER_FIRE, TeamMask());

				// no active weapon
				if (GetActiveWeaponUnclamped() == -1)
				{
					break;
				}

				// 4 x 3 = 12 (reachable tiles x (game layer, front layer, switch layer))
				CDoor *apDoors[12];
				int NumDoors = GameWorld()->IntersectDoorsUniqueNumbers(ProjStartPos, GetProximityRadius() * 0.75f, apDoors, 12);
				for (int i = 0; i < NumDoors; i++)
				{
					CDoor *pDoor = apDoors[i];

					// number 0 can't be opened and also functions as plot walls that can't be opened
					// plotid -1: map objects
					// plotid > 0: plot doors
					bool IsPlotDrawDoor = GameServer()->Collision()->IsPlotDrawDoor(pDoor->m_Number);
					bool CanHammerPlotDoor = pDoor->m_PlotID == GameServer()->GetPlotID(m_pPlayer->GetAccID()) && GameServer()->Collision()->IsPlotDoor(pDoor->m_Number);
					if (pDoor->m_Number == 0 || (!m_DoorHammer && (pDoor->m_PlotID == -1 || IsPlotDrawDoor || !CanHammerPlotDoor)))
						continue;

					if (Team() != TEAM_SUPER && GameServer()->Collision()->m_pSwitchers)
					{
						bool Status = GameServer()->Collision()->m_pSwitchers[pDoor->m_Number].m_Status[Team()];
						if (pDoor->m_PlotID > 0 && !IsPlotDrawDoor)
						{
							if (!Status && GameServer()->PlotDoorDestroyed(pDoor->m_PlotID))
							{
								GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("You can't close your door because the police destroyed it"));
							}
							else
							{
								GameServer()->SetPlotDoorStatus(pDoor->m_PlotID, !Status);
							}
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

				Antibot()->OnHammerFire(m_pPlayer->GetCID());

				if (m_Hit & DISABLE_HIT_HAMMER)
					break;

				CCharacter* apEnts[MAX_CLIENTS];
				int Types = (1<<CGameWorld::ENTTYPE_CHARACTER);
				if (Config()->m_SvInteractiveDrops)
				{
					Types |= (1<<CGameWorld::ENTTYPE_FLAG) | (1<<CGameWorld::ENTTYPE_PICKUP_DROP) | (1<<CGameWorld::ENTTYPE_MONEY) | (1<<CGameWorld::ENTTYPE_HELICOPTER) | (1<<CGameWorld::ENTTYPE_GROG);
				}
				int Num = GameWorld()->FindEntitiesTypes(ProjStartPos, GetProximityRadius() * 0.5f, (CEntity * *)apEnts, MAX_CLIENTS, Types, Team());

				int Hits = 0;
				for (int i = 0; i < Num; ++i)
				{
					CEntity *pEnt = apEnts[i];
					CCharacter *pTarget = 0;
					CAdvancedEntity *pEntity = 0;
					if (pEnt->GetObjType() == CGameWorld::ENTTYPE_CHARACTER)
					{
						pTarget = (CCharacter *)pEnt;
					}
					else if (pEnt->IsAdvancedEntity())
					{
						pEntity = (CAdvancedEntity *)pEnt;
					}

					// set his velocity to fast upward (for now)
					vec2 Dir;
					if (length(pEnt->GetPos() - m_Pos) > 0.0f)
						Dir = normalize(pEnt->GetPos() - m_Pos);
					else
						Dir = vec2(0.f, -1.f);

					vec2 EffectPos = ProjStartPos;
					if (length(pEnt->GetPos() - ProjStartPos) > 0.0f)
						EffectPos = pEnt->GetPos() - normalize(pEnt->GetPos() - ProjStartPos) * GetProximityRadius() * 0.5f;

					if (pTarget)
					{
						if (pTarget == this || !pTarget->IsAlive() || !CanCollide(pTarget->GetPlayer()->GetCID()))
							continue;

						GameServer()->CreateHammerHit(EffectPos, TeamMask());

						int TargetCID = pTarget->GetPlayer()->GetCID();
						// transformation
						bool TransformSuccess = false;
						// Important feature from blockZ where you can hit a tee without transforming by looking a little down, aaand.. of course for not hitting through walls
						if (!GameServer()->Collision()->IntersectLine(ProjStartPos, pTarget->m_Pos, NULL, NULL))
						{
							TransformSuccess = TryHumanTransformation(pTarget);
						}

						if (!TryCatchingWanted(TargetCID, EffectPos))
						{
							vec2 Temp = pTarget->m_Core.m_Vel + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f;
							Temp = ClampVel(pTarget->m_MoveRestrictions, Temp);
							Temp -= pTarget->m_Core.m_Vel;

							// dont unfreeze when we just got frozen from transformation
							if (!TransformSuccess)
							{
								// do unfreeze before takedamage, so that we update the weapon and killer to hammer when we are still in freeze, so we dont reset it
								pTarget->UnFreeze();
							}

							pTarget->TakeDamage((vec2(0.f, -1.0f) + Temp) * Tuning()->m_HammerStrength, Dir * -1, g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage, m_pPlayer->GetCID(), GetActiveWeapon());

							if (m_FreezeHammer)
								pTarget->Freeze();
						}

						Antibot()->OnHammerHit(m_pPlayer->GetCID(), TargetCID);
						Hits++;
					}
					else if (pEntity)
					{
						vec2 Temp = normalize(Dir + vec2(0.f, -1.1f)) * 10.0f;
						Temp = pEntity->GetVel() + (vec2(0.f, -1.0f) + Temp) * Tuning()->m_HammerStrength;

						if (pEntity->GetObjType() == CGameWorld::ENTTYPE_FLAG)
						{
							if (((CFlag *)pEntity)->GetCarrier())
								continue; // carrier is getting hit
						}
						else if (pEntity->GetObjType() == CGameWorld::ENTTYPE_HELICOPTER)
						{
							CHelicopter *pHelicopter = (CHelicopter*)pEntity;
							if(pHelicopter->IsBuilding() || pHelicopter->IsExploding())
								continue;

							Temp *= 0.5f;
							pHelicopter->TakeDamage((float)g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage, EffectPos, m_pPlayer->GetCID());
							if (length(pEnt->GetPos() - ProjStartPos) > 0.0f)
							{
								EffectPos = pEnt->GetPos() - normalize(pEnt->GetPos() - ProjStartPos) * pEnt->GetProximityRadius() * 0.5f;
							}
						}

						GameServer()->CreateHammerHit(EffectPos, TeamMask());
						pEntity->SetVel(ClampVel(pEntity->GetMoveRestrictions(), Temp));
						Hits++;
					}
				}

				// if we Hit anything, we have to wait for the reload
				if (Hits)
				{
					m_ReloadTimer = Tuning()->m_HammerHitFireDelay * Server()->TickSpeed() / 1000;
				}

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
						int Lifetime = (int)(Server()->TickSpeed() * (m_pPlayer->m_Gamemode == GAMEMODE_VANILLA ? Tuning()->m_VanillaGunLifetime : Tuning()->m_GunLifetime));

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
						GameServer()->CreateSound(m_Pos, m_pPlayer->m_SpookyGhost ? SOUND_PICKUP_HEALTH : (m_Jetpack && !Config()->m_SvOldJetpackSound) ? SOUND_HOOK_LOOP : SOUND_GUN_FIRE, TeamMask());
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
						float Speed = mix((float)Tuning()->m_ShotgunSpeeddiff, 1.0f, v);
						new CProjectile(GameWorld(), WEAPON_SHOTGUN,
							m_pPlayer->GetCID(),
							ProjStartPos,
							vec2(cosf(a), sinf(a)) * Speed,
							(int)(Server()->TickSpeed() * Tuning()->m_ShotgunLifetime),
							false, false, 0, -1, 0, 0, false);
					}
				}
				else
				{
					new CLaser(GameWorld(), m_Pos, Direction, Tuning()->m_LaserReach, m_pPlayer->GetCID(), WEAPON_SHOTGUN);
				}
				if (Sound)
					GameServer()->CreateSound(m_Pos, SOUND_SHOTGUN_FIRE, TeamMask());
			} break;

			case WEAPON_STRAIGHT_GRENADE:
			case WEAPON_GRENADE:
			{
				int Lifetime = (int)(Server()->TickSpeed() * (GetActiveWeapon() == WEAPON_STRAIGHT_GRENADE ? Tuning()->m_StraightGrenadeLifetime : Tuning()->m_GrenadeLifetime));

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
					GameServer()->CreateSound(m_Pos, SOUND_GRENADE_FIRE, TeamMask());
			} break;

			case WEAPON_TASER:
			case WEAPON_LASER:
			{
				new CLaser(GameWorld(), m_Pos, Direction, Tuning()->m_LaserReach, m_pPlayer->GetCID(), GetActiveWeapon(), TaserStrength);

				if (Sound)
					GameServer()->CreateSound(m_Pos, SOUND_LASER_FIRE, TeamMask());
			} break;

			case WEAPON_NINJA:
			{
				// reset Hit objects
				m_NumObjectsHit = 0;

				m_Ninja.m_ActivationDir = Direction;
				m_Ninja.m_CurrentMoveTime = g_pData->m_Weapons.m_Ninja.m_Movetime * Server()->TickSpeed() / 1000;
				m_Ninja.m_OldVelAmount = length(m_Core.m_Vel);

				if (Sound)
					GameServer()->CreateSound(m_Pos, SOUND_NINJA_FIRE, TeamMask());
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
					GameServer()->CreateSound(m_Pos, SOUND_LASER_FIRE, TeamMask());
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
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, TeamMask());
			} break;

			case WEAPON_TELEKINESIS:
			{
				bool TelekinesisSound = false;

				if (!m_pTelekinesisEntity)
				{
					int Types = (1<<CGameWorld::ENTTYPE_CHARACTER) | (1<<CGameWorld::ENTTYPE_FLAG) | (1<<CGameWorld::ENTTYPE_PICKUP_DROP) | (1<<CGameWorld::ENTTYPE_MONEY) | (1<<CGameWorld::ENTTYPE_HELICOPTER);
					CEntity *pEntity = GameWorld()->ClosestEntityTypes(GetCursorPos(), 20.f * max(1.f, m_pPlayer->GetZoomLevel()), Types, this, m_pPlayer->GetCID(), !m_Passive);

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

					if ((pChr && pChr->GetPlayer()->GetCID() != m_pPlayer->GetCID() && pChr->m_pTelekinesisEntity != this && !pChr->m_InSnake) || (pEntity && pEntity != pChr))
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
					GameServer()->CreateSound(m_Pos, SOUND_NINJA_HIT, TeamMask());
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
				vec2 PortalPos = GetCursorPos();
				bool Found = true;
				if (GameServer()->Collision()->TestBox(PortalPos, vec2(CCharacterCore::PHYS_SIZE, CCharacterCore::PHYS_SIZE)))
					Found = GetNearestAirPos(PortalPos, m_Pos, &PortalPos);

				bool PlotDoorOnly = GetCurrentTilePlotID() < PLOT_START && GameServer()->GetTilePlotID(PortalPos) < PLOT_START && Config()->m_SvPortalThroughDoor;
				bool BatteryRequired = Config()->m_SvPortalRifleAmmo && !pAccount->m_PortalRifle;

				if (!Found || !PortalPos
					|| (pAccount->m_PortalRifle && !m_pPlayer->m_aSecurityPin[0])
					|| (BatteryRequired && !pAccount->m_PortalBattery)
					|| distance(PortalPos, m_Pos) > Config()->m_SvPortalMaxDistance
					|| (m_pPlayer->m_pPortal[PORTAL_FIRST] && m_pPlayer->m_pPortal[PORTAL_SECOND])
					|| (m_LastLinkedPortals + Server()->TickSpeed() * Config()->m_SvPortalRifleDelay > Server()->Tick())
					|| GameLayerClipped(PortalPos)
					|| GameServer()->Collision()->IntersectLinePortalRifleStop(m_Pos, PortalPos, 0, 0)
					|| GameServer()->IntersectedLineDoor(m_Pos, PortalPos, Team(), PlotDoorOnly)
					|| GameWorld()->ClosestCharacter(PortalPos, Config()->m_SvPortalRadius, 0, m_pPlayer->GetCID(), false, true) // dont allow to place portals too close to other tees
					)
				{
					GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, TeamMask());
					return;
				}

				vec2 IntersectedPos = vec2(-1, -1);
				if (m_pPlayer->m_pPortal[PORTAL_FIRST] && GameWorld()->IntersectLinePortalBlocker(m_pPlayer->m_pPortal[PORTAL_FIRST]->GetPos(), PortalPos))
					IntersectedPos = m_pPlayer->m_pPortal[PORTAL_FIRST]->GetPos();
				else if (GameWorld()->IntersectLinePortalBlocker(m_Pos, PortalPos))
					IntersectedPos = m_Pos;

				if (IntersectedPos != vec2(-1, -1))
				{
					new CFlyingPoint(GameWorld(), IntersectedPos, -1, m_pPlayer->GetCID(), vec2((float)random(0, 400)/100.f, (float)random(0, 400)/100.f), PortalPos, true);
					GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, TeamMask());
					return;
				}

				for (int i = 0; i < NUM_PORTALS; i++)
				{
					if (!m_pPlayer->m_pPortal[i])
					{
						int PlotDoorNumber = GameServer()->Collision()->GetPlotBySwitch(GameServer()->IntersectedLineDoor(m_Pos, PortalPos, Team(), true, false));
						if (i == PORTAL_SECOND)
						{
							if (PlotDoorNumber < PLOT_START)
							{
								int OtherThroughPlotDoor = m_pPlayer->m_pPortal[PORTAL_FIRST]->GetThroughPlotDoor();
								if (OtherThroughPlotDoor >= PLOT_START)
									PlotDoorNumber = OtherThroughPlotDoor;
							}
						}

						bool IsPortalInNoBonusArea = m_NoBonusContext.m_InArea;
						int NoBonusTile = GameServer()->Collision()->IntersectLineNoBonus(m_Pos, PortalPos, 0, 0, !m_NoBonusContext.m_InArea);
						if (NoBonusTile != 0)
							IsPortalInNoBonusArea = NoBonusTile == TILE_NO_BONUS_AREA; // else TILE_NO_BONUS_AREA_LEAVE

						m_pPlayer->m_pPortal[i] = new CPortal(GameWorld(), PortalPos, m_pPlayer->GetCID(), PlotDoorNumber, IsPortalInNoBonusArea);
						if (i == PORTAL_SECOND)
						{
							m_pPlayer->m_pPortal[PORTAL_FIRST]->SetLinkedPortal(m_pPlayer->m_pPortal[PORTAL_SECOND]);
							m_pPlayer->m_pPortal[PORTAL_SECOND]->SetLinkedPortal(m_pPlayer->m_pPortal[PORTAL_FIRST]);
							m_LastLinkedPortals = Server()->Tick();

							if (BatteryRequired && m_pPlayer->GetAccID() >= ACC_START)
							{
								pAccount->m_PortalBattery--;
								UpdateWeaponIndicator();
							}
						}
						break;
					}
				}

			} break;

			case WEAPON_PROJECTILE_RIFLE:
			{
				int Lifetime = (int)(Server()->TickSpeed() * Tuning()->m_GunLifetime);

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
					GameServer()->CreateSound(m_Pos, SOUND_HOOK_LOOP, TeamMask());
			} break;

			case WEAPON_BALL_GRENADE:
			{
				int Lifetime = (int)(Server()->TickSpeed() * Tuning()->m_GrenadeLifetime);

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
					GameServer()->CreateSound(m_Pos, SOUND_GRENADE_FIRE, TeamMask());
			} break;

			case WEAPON_TELE_RIFLE:
			{
				vec2 NewPos = GetCursorPos();
				if (!Config()->m_SvTeleRifleAllowBlocks && GameServer()->Collision()->TestBox(NewPos, vec2(GetProximityRadius(), GetProximityRadius())))
				{
					bool Found = GetNearestAirPos(NewPos, m_Pos, &NewPos);
					if (!Found || !NewPos)
					{
						if (ClickedFire)
							GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, TeamMask());
						return;
					}
				}

				GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID(), TeamMask());
				GameServer()->CreatePlayerSpawn(NewPos, TeamMask());
				ForceSetPos(NewPos);
				m_DDRaceState = DDRACE_CHEAT;

				if (Sound)
					GameServer()->CreateSound(m_Pos, SOUND_LASER_FIRE, TeamMask());
			} break;

			case WEAPON_LIGHTNING_LASER:
			{
				new CLightningLaser(GameWorld(), ProjStartPos, Direction, m_pPlayer->GetCID());

				if (Sound)
					GameServer()->CreateSound(m_Pos, SOUND_LASER_FIRE, TeamMask());
			} break;
		}

		Sound = false;
	}

	if (GetActiveWeapon() != WEAPON_LIGHTSABER) // we don't want the client to render the fire animation
		m_AttackTick = Server()->Tick();

	if (!m_ReloadTimer)
	{
		m_ReloadTimer = GetFireDelay(GetActiveWeapon()) * Server()->TickSpeed() / 1000;
	}

	if(m_aWeapons[GetActiveWeapon()].m_Ammo > 0) // -1 == unlimited
	{
		m_aWeapons[GetActiveWeapon()].m_Ammo--;

		int W = GetSpawnWeaponIndex(GetActiveWeapon());
		if (W != -1 && m_aSpawnWeaponActive[W] && m_aWeapons[GetActiveWeapon()].m_Ammo == 0)
			GiveWeapon(GetActiveWeapon(), true);
	}

	// Do this here and not in the switch case so that spread taser will only take 1 battery either. Portal can't be spread, so it can be handled there
	if (GetActiveWeapon() == WEAPON_TASER && m_pPlayer->GetAccID() >= ACC_START && pAccount->m_TaserBattery > 0)
	{
		pAccount->m_TaserBattery--;
		UpdateWeaponIndicator();
	}

	bool IsTypeGun = GameServer()->GetWeaponType(GetActiveWeapon()) == WEAPON_GUN;
	bool CanTabDoubleClick = IsTypeGun || GetActiveWeapon() == WEAPON_HAMMER;
	if (CanTabDoubleClick && m_pPlayer->m_PlayerFlags&PLAYERFLAG_SCOREBOARD && !m_pPlayer->IsMinigame() && CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
	{
		m_TabDoubleClickCount++;
		if (m_TabDoubleClickCount == 2)
		{
			if (IsTypeGun)
			{
				if (m_pPlayer->m_SpookyGhost)
					UnsetSpookyGhost();
				else if (pAccount->m_SpookyGhost || m_pPlayer->m_HasSpookyGhost)
					SetSpookyGhost();
			}
			else if (GetActiveWeapon() == WEAPON_HAMMER)
			{
				m_IsPortalBlocker = m_IsPortalBlocker ? false : pAccount->m_PortalBlocker;
				UpdateWeaponIndicator();

				// Create portal blocker preview
				if (m_IsPortalBlocker && !m_pPortalBlocker)
					m_pPortalBlocker = new CPortalBlocker(GameWorld(), m_Pos, m_pPlayer->GetCID());
			}

			m_TabDoubleClickCount = 0;
		}
	}
}

float CCharacter::GetFireDelay(int Weapon)
{
	int Tune = OLD_TUNES + Weapon;
	if (Weapon >= NUM_VANILLA_WEAPONS)
		Tune++; // the hammer hit fire delay got inserted inbetween, so we have to go one entry further

	float FireDelay;
	Tuning()->Get(Tune, &FireDelay);
	return FireDelay;
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

	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[m_pPlayer->GetAccID()];

	if (Weapon == WEAPON_LASER && !Remove && !m_aWeapons[WEAPON_PORTAL_RIFLE].m_Got && !m_pPlayer->IsMinigame() && pAccount->m_PortalRifle)
		GiveWeapon(WEAPON_PORTAL_RIFLE, false, -1, true);

	if (m_pPlayer->m_SpookyGhost && GameServer()->GetWeaponType(Weapon) != WEAPON_GUN)
		return;

	if (m_IsZombie && Weapon != WEAPON_HAMMER && !Remove)
		return;

	for (int i = 0; i < NUM_BACKUPS; i++)
	{
		m_aWeaponsBackupGot[Weapon][i] = !Remove;
		m_aWeaponsBackup[Weapon][i] = Ammo;
	}

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
		{
			// dont instantly switch weapon when we shot last shot of spawnweapons
			if (m_ReloadTimer != 0)
				m_QueuedWeapon = -2;
			else
				SetWeapon(WEAPON_GUN);
		}
	}
	else
	{
		m_aWeapons[Weapon].m_Ammo = Ammo;
	}
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
	{
		m_Ninja.m_ActivationTick = 0; // for ddrace hud
		return;
	}

	m_Ninja.m_ActivationTick = Server()->Tick();
	if (GetActiveWeapon() != WEAPON_NINJA)
		m_LastWeapon = GetActiveWeapon();
	SetActiveWeapon(WEAPON_NINJA);

	if (!m_aWeapons[WEAPON_NINJA].m_Got && m_pPlayer->m_Gamemode == GAMEMODE_VANILLA)
		GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA, TeamMask());
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
	int ResetInput = 0;
	if (GameServer()->Arenas()->IsConfiguring(m_pPlayer->GetCID()))
	{
		GameServer()->Arenas()->OnInput(m_pPlayer->GetCID(), pNewInput);
		ResetInput |= 2;
	}
	else if (GameServer()->Durak()->ActivelyPlaying(m_pPlayer->GetCID()))
	{
		GameServer()->Durak()->OnInput(this, pNewInput);
		ResetInput |= 1;
		ResetInput |= 4;
	}
	else if (m_DrawEditor.Active())
	{
		m_DrawEditor.OnInput(pNewInput);
		ResetInput |= 1;
	}
	else if (m_Snake.Active())
	{
		m_Snake.OnInput(pNewInput);
		ResetInput |= 1;
	}
	else if (m_pHelicopter)
	{
		m_pHelicopter->OnInput(pNewInput);
		ResetInput |= 1;
	}
	else if (m_GrogBalancePosX != GROG_BALANCE_POS_UNSET)
	{
		pNewInput->m_Direction = m_GrogDirection;
	}
	else if (m_GrogDirDelayEnd && m_GrogDirDelayEnd >= Server()->Tick())
	{
		pNewInput->m_Direction = m_SavedInput.m_Direction;
	}

	if (ResetInput)
	{
		pNewInput->m_Direction = 0;
		pNewInput->m_Jump = 0;
		if (!(ResetInput & 4))
		{
			pNewInput->m_Hook = 0;
		}
		if (ResetInput & 2)
		{
			pNewInput->m_TargetX = m_Input.m_TargetX;
			pNewInput->m_TargetY = m_Input.m_TargetY;
		}
	}

	UpdateMovementTick(pNewInput);

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

	if (!m_pTelekinesisEntity)
		Antibot()->OnDirectInput(m_pPlayer->GetCID());

	if(m_NumInputs > 2 && m_pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		HandleWeaponSwitch();
		FireWeapon();
	}

	mem_copy(&m_LatestPrevPrevInput, &m_LatestPrevInput, sizeof(m_LatestInput));
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
	m_LatestPrevInput = m_LatestInput = m_Input;
}

bool CCharacter::IsIdle()
{
	return !m_SavedInput.m_Direction && !m_SavedInput.m_Hook && !m_SavedInput.m_Jump && !(m_SavedInput.m_Fire&1);
}

void CCharacter::UpdateMovementTick(CNetObj_PlayerInput *pNewInput)
{
	// Specifically only check for movement, not mouse pos change or anything else, for minigame auto leave, which could before be abused with dummy hammer
	if (pNewInput->m_Direction == m_SavedInput.m_Direction && pNewInput->m_Jump == m_SavedInput.m_Jump)
		return;

	// dont update when the dummy is idle (this is basically only to catch dummy control when an input happens, at that time the function below would return false still
	if (Server()->IsIdleDummy(m_pPlayer->GetCID()))
		return;

	// Don't update when dummy copy is activated or dummy control has been used
	if (Server()->DummyControlOrCopyMoves(m_pPlayer->GetCID()))
		return;

	m_pPlayer->m_LastMovementTick = Server()->Tick();
}

void CCharacter::Tick()
{
	if(m_Paused)
		return;

	DummyTick();
	FDDraceTick();
	// process grog and permille, return if player got killed due to excessive drinking
	if (GrogTick())
		return;
	HandleLastIndexTiles();

	DDraceTick();

	if (!m_pTelekinesisEntity && !m_pPlayer->m_IsDummy)
		Antibot()->OnCharacterTick(m_pPlayer->GetCID());

	// F-DDrace
	for (int i = 0; i < 2; i++)
	{
		CFlag* F = ((CGameControllerDDRace*)GameServer()->m_pController)->m_apFlags[i];
		if (F) m_Core.SetFlagInfo(i, F->GetPos(), F->IsAtStand(), F->GetVel(), F->GetCarrier());
	}

	// We don't want to reset the hooktick to 0, because we need it in order to detect hook duration for no-bonus punishment
	m_Core.m_EndlessHook = m_EndlessHook || (m_Super && Config()->m_SvEndlessSuperHook);
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

	if(!m_PrevInput.m_Hook && m_Input.m_Hook && !(m_Core.m_TriggeredEvents & COREEVENTFLAG_HOOK_ATTACH_PLAYER))
	{
		Antibot()->OnHookAttach(m_pPlayer->GetCID(), false);
	}

	// handle Weapons
	HandleWeapons();
	DDracePostCoreTick();

	if(m_Core.m_TriggeredEvents & COREEVENTFLAG_HOOK_ATTACH_PLAYER)
	{
		if(m_Core.m_HookedPlayer != -1 && GameServer()->m_apPlayers[m_Core.m_HookedPlayer]->GetTeam() != -1)
		{
			Antibot()->OnHookAttach(m_pPlayer->GetCID(), true);
		}
	}

	// Previnput
	m_PrevInput = m_Input;

	m_PrevPos = m_Core.m_Pos;
}

void CCharacter::TickDeferred()
{
	static const vec2 ColBox(CCharacterCore::PHYS_SIZE, CCharacterCore::PHYS_SIZE);
	// advance the dummy
	{
		CWorldCore TempWorld;
		m_ReckoningCore.Init(&TempWorld, GameServer()->Collision(), &Teams()->m_Core, &((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts, IsSwitchActiveCb, this);
		m_ReckoningCore.m_Id = m_pPlayer->GetCID();
		m_ReckoningCore.Tick(false);
		m_ReckoningCore.Move(false);
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
	m_Core.Move(Config()->m_SvStoppersPassthrough);

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

	// Some sounds are triggered client-side for the acting player so we need to avoid duplicating them
	// hook sounds are clientside in 0.7, thats why we pass true here, to have sevendown only

	if(Events&COREEVENTFLAG_HOOK_ATTACH_PLAYER)
	{
		GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_PLAYER, TeamMask(true));
		OnPlayerHook();
	}

	if (Events&(COREEVENTFLAG_GROUND_JUMP|COREEVENTFLAG_HOOK_ATTACH_GROUND|COREEVENTFLAG_HOOK_HIT_NOHOOK))
	{
		Mask128 TeamMask = TeamMaskExceptSelf(true);
		if(Events&COREEVENTFLAG_GROUND_JUMP) GameServer()->CreateSound(m_Pos, SOUND_PLAYER_JUMP, TeamMask);
		if(Events&COREEVENTFLAG_HOOK_ATTACH_GROUND) GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_GROUND, TeamMask);
		if(Events&COREEVENTFLAG_HOOK_HIT_NOHOOK) GameServer()->CreateSound(m_Pos, SOUND_HOOK_NOATTACH, TeamMask);
	}

	if(Events&COREEVENTFLAG_HOOK_ATTACH_FLAG)
	{
		GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_PLAYER, TeamMask());
		m_TriggeredEvents &= ~COREEVENTFLAG_HOOK_ATTACH_FLAG;
	}

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

void CCharacter::Die(int Weapon, bool UpdateTeeControl, bool OnArenaDie)
{
	// make sure we are not saved as killer for someone else after we joined a minigame, so we cant take the flag to a minigame
	if (Weapon == WEAPON_MINIGAME_CHANGE)
		GameServer()->UnsetKiller(m_pPlayer->GetCID());

	if (OnArenaDie)
		GameServer()->Arenas()->OnPlayerDie(m_pPlayer->GetCID());

	m_DrawEditor.OnPlayerDeath();
	m_Snake.OnPlayerDeath();

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
	if (Killer != m_pPlayer->GetCID() && (Weapon == WEAPON_SELF || Weapon == WEAPON_PLAYER))
		Weapon = m_Core.m_Killer.m_Weapon;
	else if (Weapon < WEAPON_GAME)
		Weapon = WEAPON_GAME; // don't send weird stuff to the clients

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
		bool IsBlock = !pKiller->IsMinigame() || pKiller->m_Minigame == MINIGAME_BLOCK;
		CCharacter* pKillerChar = pKiller->GetCharacter();
		if (CountKill && pKillerChar && (!m_pPlayer->m_IsDummy || Config()->m_SvDummyBlocking))
		{
			pKillerChar->m_KillStreak++;
			if (pKillerChar->m_KillStreak % 5 == 0)
			{
				if (IsBlock)
					GameServer()->SendChatFormat(-1, CHAT_ALL, -1, CGameContext::CHATFLAG_ALL, Localizable("%s is on a killing spree with %d blocks"), Server()->ClientName(Killer), pKillerChar->m_KillStreak);
				else
					GameServer()->SendChatFormat(-1, CHAT_ALL, -1, CGameContext::CHATFLAG_ALL, Localizable("%s is on a killing spree with %d kills"), Server()->ClientName(Killer), pKillerChar->m_KillStreak);
				GameServer()->CreateFinishConfetti(pKillerChar->GetPos(), pKillerChar->TeamMask());
			}
		}

		if (m_KillStreak >= 5)
		{
			if (IsBlock)
			{
				GameServer()->SendChatFormat(-1, CHAT_ALL, -1, CGameContext::CHATFLAG_ALL, Localizable("%s's blocking spree was ended by %s (%d blocks)"),
					Server()->ClientName(m_pPlayer->GetCID()), Server()->ClientName(Killer), m_KillStreak);
			}
			else
			{
				GameServer()->SendChatFormat(-1, CHAT_ALL, -1, CGameContext::CHATFLAG_ALL, Localizable("%s's blocking spree was ended by %s (%d kills)"),
					Server()->ClientName(m_pPlayer->GetCID()), Server()->ClientName(Killer), m_KillStreak);
			}
			pKiller->GiveXP(250, "for ending a killing spree");
			if (pKillerChar)
			{
				GameServer()->CreateFinishConfetti(pKillerChar->GetPos(), pKillerChar->TeamMask());
			}
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
						pKiller->GiveBlockPoints(1, m_pPlayer->GetCID());
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
			pKiller->ShowNameShort();
		m_pPlayer->KillMsgNoName(&Msg);
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
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("You lost, you can wait for another round or leave the lobby using '/leave'"));
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
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE, TeamMask());

	// this is for auto respawn after 3 secs
	m_pPlayer->m_PreviousDieTick = m_pPlayer->m_DieTick;
	m_pPlayer->m_DieTick = Server()->Tick();

	m_Alive = false;

	// F-DDrace
	if (m_Passive)
		Passive(false, -1, true);
	UnsetSpookyGhost();
	SetZombieHuman(false);

	// unset skin specific stuff
	m_pPlayer->ResetSkin();

	// dismount helicopter
	if (m_pHelicopter)
		m_pHelicopter->Dismount();

	GameWorld()->RemoveEntity(this);
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID(), TeamMask());
	Teams()->OnCharacterDeath(m_pPlayer->GetCID(), Weapon);

	// reset gamemode if it got changed by a tile but the setting says something different
	m_pPlayer->m_Gamemode = m_pPlayer->m_SavedGamemode;
}

bool CCharacter::TakeDamage(vec2 Force, vec2 Source, int Dmg, int From, int Weapon)
{
	// avoid farming by shooting people with gun for example, because it doesnt change velocity so it doesnt influence whether you died or not
	bool SetKiller = (m_pPlayer->m_Gamemode == GAMEMODE_VANILLA || (Weapon != WEAPON_GUN && Weapon != WEAPON_HEART_GUN && Weapon != WEAPON_LIGHTSABER));
	if (SetKiller && GameServer()->m_apPlayers[From] && From != m_pPlayer->GetCID())
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

	if (!GameServer()->Durak()->ActivelyPlaying(m_pPlayer->GetCID()))
	{
		vec2 Temp = m_Core.m_Vel + Force;
		m_Core.m_Vel = ClampVel(m_MoveRestrictions, Temp);
	}

	return true;
}

bool CCharacter::CanSnapCharacter(int SnappingClient)
{
	if (SnappingClient < 0)
		return true;

	CCharacter *pSnapChar = GameServer()->GetPlayerChar(SnappingClient);
	CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];

	if ((pSnapPlayer->GetTeam() == TEAM_SPECTATORS || pSnapPlayer->IsPaused()) && pSnapPlayer->GetSpectatorID() != -1
		&& !CanCollide(pSnapPlayer->GetSpectatorID(), false) && !pSnapPlayer->m_ShowOthers)
		return false;

	if (pSnapPlayer->GetTeam() != TEAM_SPECTATORS && !pSnapPlayer->IsPaused() && pSnapChar && !pSnapChar->m_Super
		&& !CanCollide(SnappingClient, false) && !pSnapPlayer->m_ShowOthers)
		return false;

	if ((pSnapPlayer->GetTeam() == TEAM_SPECTATORS || pSnapPlayer->IsPaused()) && pSnapPlayer->GetSpecMode() == SPEC_FREEVIEW
		&& !CanCollide(SnappingClient, false) && pSnapPlayer->m_SpecTeam)
		return false;

	return true;
}

bool CCharacter::IsSnappingCharacterInView(int SnappingClientId)
{
	int Id = m_pPlayer->GetCID();

	// explicitly check for /showall in NetworkClippedLine, in case a client that doesnt support showdistance wants to see characters while zooming out
	// only characters will be sent over large distances when showall is used, other entities are only snapped in a close range or
	// when showdistance is supported, then all entities within that showdistance range are being sent

	// A player may not be clipped away if his hook or a hook attached to him is in the field of view
	bool PlayerAndHookNotInView = NetworkClippedLine(SnappingClientId, m_Pos, m_Core.m_HookPos, true);
	bool AttachedHookInView = false;
	if(PlayerAndHookNotInView)
	{
		for(const auto &AttachedPlayerId : m_Core.m_AttachedPlayers)
		{
			const CCharacter *pOtherPlayer = GameServer()->GetPlayerChar(AttachedPlayerId);
			if(pOtherPlayer && pOtherPlayer->m_Core.m_HookedPlayer == Id)
			{
				if(!NetworkClippedLine(SnappingClientId, m_Pos, pOtherPlayer->m_Pos, true))
				{
					AttachedHookInView = true;
					break;
				}
			}
		}
	}
	if(PlayerAndHookNotInView && !AttachedHookInView)
	{
		return false;
	}
	return true;
}

void CCharacter::Snap(int SnappingClient)
{
	if (!CanSnapCharacter(SnappingClient))
		return;

	bool RainbowNameAffected = SnappingClient == m_pPlayer->GetCID() && GameServer()->m_RainbowName.IsAffected(SnappingClient);
	if(!IsSnappingCharacterInView(SnappingClient) && !RainbowNameAffected && SendDroppedFlagCooldown(SnappingClient) == -1)
		return;

	// F-DDrace
	if (SnappingClient == m_pPlayer->GetCID())
		m_DrawEditor.Snap();

	// invisibility
	if (m_Invisible && SnappingClient != m_pPlayer->GetCID() && SnappingClient != -1)
		if (!Server()->GetAuthedState(SnappingClient) || Server()->Tick() % 200 == 0)
			return;

	// Draw cursor
	CPlayer *pSnap = SnappingClient >= 0 ? GameServer()->m_apPlayers[SnappingClient] : 0;
	if (pSnap && (pSnap->m_ViewCursorID == -1 || pSnap->m_ViewCursorID == m_pPlayer->GetCID()))
	{
		CNetObj_Laser *pCursor = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ViewCursorSnapID, sizeof(CNetObj_Laser)));
		if (!pCursor)
			return;

		vec2 CursorPos = pSnap->m_ViewCursorZoomed ? m_CursorPosZoomed : m_CursorPos;
		pCursor->m_X = round_to_int(CursorPos.x);
		pCursor->m_Y = round_to_int(CursorPos.y);
		pCursor->m_FromX = round_to_int(m_Pos.x);
		pCursor->m_FromY = round_to_int(m_Pos.y);
		pCursor->m_StartTick = Server()->Tick() - 5;
	}

	if (m_IsPortalBlocker)
	{
		if(GameServer()->GetClientDDNetVersion(SnappingClient) >= VERSION_DDNET_MULTI_LASER)
		{
			CNetObj_DDNetLaser *pInd = static_cast<CNetObj_DDNetLaser *>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, m_PortalBlockerIndSnapID, sizeof(CNetObj_DDNetLaser)));
			if(!pInd)
				return;

			pInd->m_ToX = round_to_int(m_Pos.x);
			pInd->m_ToY = round_to_int(m_Pos.y - 80);
			pInd->m_FromX = round_to_int(m_Pos.x);
			pInd->m_FromY = round_to_int(m_Pos.y - 80);
			pInd->m_StartTick = Server()->Tick();
			pInd->m_Owner = m_pPlayer->GetCID();
			pInd->m_Type = LASERTYPE_SHOTGUN;
		}
		else
		{
			CNetObj_Laser *pInd = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_PortalBlockerIndSnapID, sizeof(CNetObj_Laser)));
			if (!pInd)
				return;

			pInd->m_X = round_to_int(m_Pos.x);
			pInd->m_Y = round_to_int(m_Pos.y - 80);
			pInd->m_FromX = round_to_int(m_Pos.x);
			pInd->m_FromY = round_to_int(m_Pos.y - 80);
			pInd->m_StartTick = Server()->Tick();
		}
	}

	if (m_Passive && !GameServer()->Durak()->InDurakGame(m_pPlayer->GetCID()))
	{
		int Size = Server()->IsSevendown(SnappingClient) ? 4*4 : sizeof(CNetObj_Pickup);
		CNetObj_Pickup* pP = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_PassiveSnapID, Size));
		if (!pP)
			return;

		pP->m_X = round_to_int(m_Pos.x);
		pP->m_Y = round_to_int(m_Pos.y - 50.f);
		if (Server()->IsSevendown(SnappingClient))
		{
			int Subtype = 0;
			pP->m_Type = POWERUP_ARMOR;
			((int*)pP)[3] = Subtype;
		}
		else
			pP->m_Type = PICKUP_ARMOR;
	}

	// translate id, if we are not in the map of the other person display us as weapon and our hook as a laser
	int ID = m_pPlayer->GetCID();
	if (SnappingClient > -1 && !Server()->Translate(ID, SnappingClient))
	{
		int Size = Server()->IsSevendown(SnappingClient) ? 4*4 : sizeof(CNetObj_Pickup);
		CNetObj_Pickup* pPickup = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_aUntranslatedID[EUntranslatedMap::ID_WEAPON], Size));
		if (!pPickup)
			return;

		int Subtype = GameServer()->GetWeaponType(GetActiveWeapon());
		int Type = Subtype == WEAPON_NINJA ? POWERUP_NINJA : POWERUP_WEAPON;

		pPickup->m_X = round_to_int(m_Pos.x);
		pPickup->m_Y = round_to_int(m_Pos.y);
		if (Server()->IsSevendown(SnappingClient))
		{
			pPickup->m_Type = Type;
			((int*)pPickup)[3] = Subtype;
		}
		else
			pPickup->m_Type = GameServer()->GetPickupType(Type, Subtype);

		if (m_Core.m_HookState != HOOK_IDLE && m_Core.m_HookState != HOOK_RETRACTED)
		{
			CNetObj_Laser *pLaser = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_aUntranslatedID[EUntranslatedMap::ID_HOOK], sizeof(CNetObj_Laser)));
			if (!pLaser)
				return;

			pLaser->m_X = round_to_int(m_Core.m_HookPos.x);
			pLaser->m_Y = round_to_int(m_Core.m_HookPos.y);
			pLaser->m_FromX = round_to_int(m_Pos.x);
			pLaser->m_FromY = round_to_int(m_Pos.y);
			pLaser->m_StartTick = Server()->Tick() - 3;
		}
		return;
	}

	// otherwise show our normal tee and send ddnet character stuff
	SnapCharacter(SnappingClient, ID);
	SnapDDNetCharacter(SnappingClient, ID);
}

void CCharacter::SnapDDNetCharacter(int SnappingClient, int ID)
{
	CNetObj_DDNetCharacter *pDDNetCharacter = static_cast<CNetObj_DDNetCharacter *>(Server()->SnapNewItem(NETOBJTYPE_DDNETCHARACTER, ID, sizeof(CNetObj_DDNetCharacter)));
	if(!pDDNetCharacter)
		return;

	// Send ddnet info of the guy we are spectating, because rainbowname is hacky and sets spectatorid to ourselve if rainbowname is close to us
	{
		bool RainbowNameAffected = SnappingClient == m_pPlayer->GetCID() && GameServer()->m_RainbowName.IsAffected(SnappingClient);
		CCharacter *pSpectating = GameServer()->GetPlayerChar(m_pPlayer->GetSpectatorID());
		int Flags, Jumps, JumpedTotal, NinjaActivationTick;
		if (RainbowNameAffected && pSpectating)
		{
			Flags = pSpectating->GetDDNetCharacterFlags(SnappingClient);
			Jumps = pSpectating->m_Core.m_Jumps;
			JumpedTotal = pSpectating->m_Core.m_JumpedTotal;
			NinjaActivationTick = pSpectating->GetDDNetCharacterNinjaActivationTick();

			// Add one jump when we're grounded and he isn't, remove one jump when we're not grounded but he is
			JumpedTotal += (int)m_IsGrounded - (int)pSpectating->m_IsGrounded;
		}
		else
		{
			Flags = GetDDNetCharacterFlags(SnappingClient);
			Jumps = m_Core.m_Jumps;
			JumpedTotal = m_Core.m_JumpedTotal;
			NinjaActivationTick = GetDDNetCharacterNinjaActivationTick();
		}

		pDDNetCharacter->m_Flags = Flags;
		pDDNetCharacter->m_Jumps = Jumps;
		pDDNetCharacter->m_JumpedTotal = JumpedTotal;
		pDDNetCharacter->m_NinjaActivationTick = NinjaActivationTick;
	}

	CPlayer *pSnap = SnappingClient >= 0 ? GameServer()->m_apPlayers[SnappingClient] : 0;
	pDDNetCharacter->m_StrongWeakID = pSnap ? (Config()->m_SvWeakHook ? pSnap->m_aStrongWeakID[ID] : SnappingClient == m_pPlayer->GetCID() ? 1 : 0) : m_StrongWeakID;
	pDDNetCharacter->m_FreezeEnd = m_DeepFreeze ? -1 : m_FreezeTime == 0 ? 0 : Server()->Tick() + m_FreezeTime;
	pDDNetCharacter->m_FreezeStart = m_FreezeTick;
	pDDNetCharacter->m_TeleCheckpoint = m_TeleCheckpoint;
	pDDNetCharacter->m_TargetX = m_Core.m_Input.m_TargetX;
	pDDNetCharacter->m_TargetY = m_Core.m_Input.m_TargetY;
}

int CCharacter::GetDDNetCharacterNinjaActivationTick()
{
	bool NinjaBarFull = m_DrawEditor.Active() || (GetActiveWeapon() == WEAPON_NINJA && m_ScrollNinja) || GetActiveWeapon() == WEAPON_TELEKINESIS;;
	return NinjaBarFull ? Server()->Tick() : m_Ninja.m_ActivationTick;
}

int CCharacter::GetDDNetCharacterFlags(int SnappingClient)
{
	int Flags = 0;

	bool aGotWeapon[NUM_VANILLA_WEAPONS] = { false };
	for (int i = 0; i < NUM_WEAPONS; i++)
		if (m_aWeapons[i].m_Got)
			aGotWeapon[GameServer()->GetWeaponType(i)] = true;

	bool Helicopter = SnappingClient == m_pPlayer->GetCID() && m_pHelicopter;
	if(m_Solo)
		Flags |= CHARACTERFLAG_SOLO;
	if(m_Super)
		Flags |= CHARACTERFLAG_SUPER;
	if(m_EndlessHook)
		Flags |= CHARACTERFLAG_ENDLESS_HOOK;
	if(!m_Core.m_Collision || !Tuning()->m_PlayerCollision || (m_Passive && !m_Super) || Helicopter)
		Flags |= CHARACTERFLAG_NO_COLLISION;
	if(!m_Core.m_Hook || !Tuning()->m_PlayerHooking || (m_Passive && !m_Super) || Helicopter)
		Flags |= CHARACTERFLAG_NO_HOOK;
	if(m_SuperJump && !Helicopter)
		Flags |= CHARACTERFLAG_ENDLESS_JUMP;
	if(m_Jetpack && (GameServer()->GetWeaponType(GetActiveWeapon()) != WEAPON_GUN || GetActiveWeapon() == WEAPON_GUN))
		Flags |= CHARACTERFLAG_JETPACK;
	if(m_Hit & DISABLE_HIT_GRENADE && m_aWeapons[WEAPON_GRENADE].m_Got)
		Flags |= CHARACTERFLAG_NO_GRENADE_HIT;
	if(m_Hit & DISABLE_HIT_HAMMER && m_aWeapons[WEAPON_HAMMER].m_Got)
		Flags |= CHARACTERFLAG_NO_HAMMER_HIT;
	if(m_Hit & DISABLE_HIT_RIFLE && m_aWeapons[WEAPON_LASER].m_Got)
		Flags |= CHARACTERFLAG_NO_LASER_HIT;
	if(m_Hit & DISABLE_HIT_SHOTGUN && m_aWeapons[WEAPON_SHOTGUN].m_Got)
		Flags |= CHARACTERFLAG_NO_SHOTGUN_HIT;
	if(m_HasTeleGun && m_aWeapons[WEAPON_GUN].m_Got)
		Flags |= CHARACTERFLAG_TELEGUN_GUN;
	if(m_HasTeleGrenade && m_aWeapons[WEAPON_GRENADE].m_Got)
		Flags |= CHARACTERFLAG_TELEGUN_GRENADE;
	if(m_HasTeleLaser && m_aWeapons[WEAPON_LASER].m_Got)
		Flags |= CHARACTERFLAG_TELEGUN_LASER;
	if(aGotWeapon[WEAPON_HAMMER])
		Flags |= CHARACTERFLAG_WEAPON_HAMMER;
	if(aGotWeapon[WEAPON_GUN])
		Flags |= CHARACTERFLAG_WEAPON_GUN;
	if(aGotWeapon[WEAPON_SHOTGUN])
		Flags |= CHARACTERFLAG_WEAPON_SHOTGUN;
	if(aGotWeapon[WEAPON_GRENADE])
		Flags |= CHARACTERFLAG_WEAPON_GRENADE;
	if(aGotWeapon[WEAPON_LASER])
		Flags |= CHARACTERFLAG_WEAPON_LASER;
	if(aGotWeapon[WEAPON_NINJA])
		Flags |= CHARACTERFLAG_WEAPON_NINJA;
	//if(m_Core.m_LiveFrozen)
	//	Flags |= CHARACTERFLAG_NO_MOVEMENTS;
	if(m_IsFrozen)
		Flags |= CHARACTERFLAG_IN_FREEZE;
	if(Teams()->IsPractice(Team()))
		Flags |= CHARACTERFLAG_PRACTICE_MODE;
	if(Teams()->TeamLocked(Team()) || m_NoBonusContext.m_SavedBonus.NonEmpty())
		Flags |= CHARACTERFLAG_LOCK_MODE;
	//if(Teams()->TeamLocked(Team()))
	//	Flags |= CHARACTERFLAG_TEAM0_MODE;
	if(m_pPlayer->m_Sparkle)
		Flags |= CHARACTERFLAG_INVINCIBLE;

	return Flags;
}

void CCharacter::SnapCharacter(int SnappingClient, int ID)
{
	CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, ID, sizeof(CNetObj_Character)));
	if(!pCharacter)
		return;

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
		if (SnappingClient == m_pPlayer->GetCID() && Server()->IsSevendown(SnappingClient) &&
			(pCharacter->m_HookedPlayer == HOOK_FLAG_BLUE || pCharacter->m_HookedPlayer == HOOK_FLAG_RED))
		{
			pCharacter->m_HookedPlayer = Server()->GetMaxClients(SnappingClient) - 1;
		}
		else if (!Server()->Translate(pCharacter->m_HookedPlayer, SnappingClient))
			pCharacter->m_HookedPlayer = -1;

		if (m_Core.m_EndlessHook)
			pCharacter->m_HookTick = 0;
	}

	if (m_pPlayer->GetDummyMode() == DUMMYMODE_TAVERN_DUMMY)
	{
		pCharacter->m_Emote = EMOTE_HAPPY;
	}
	else
	{
		pCharacter->m_Emote = m_pPlayer->m_SpookyGhost ? EMOTE_SURPRISE : m_EmoteType;
	}

	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;
	pCharacter->m_Direction = m_Input.m_Direction;

	bool RainbowNameAffected = SnappingClient == m_pPlayer->GetCID() && GameServer()->m_RainbowName.IsAffected(SnappingClient);
	int Events = m_TriggeredEvents;
	// jump is used for flying up, annoying air jump effect otherwise
	if (SnappingClient == m_pPlayer->GetCID() && (m_pHelicopter || m_Snake.Active() || RainbowNameAffected))
	{
		pCharacter->m_Jumped |= 2;
		Events |= COREEVENTFLAG_AIR_JUMP;

		if (m_pHelicopter)
		{
			vec2 Vel = m_pHelicopter->GetVel();
			pCharacter->m_VelX = round_to_int(Vel.x * 256.0f);
			pCharacter->m_VelY = round_to_int(Vel.y * 256.0f);
		}
	}
	else if (SnappingClient >= 0 && GameServer()->m_apPlayers[SnappingClient]->SilentFarmActive())
	{
		Events &= ~COREEVENTFLAG_GROUND_JUMP;
		Events &= ~COREEVENTFLAG_AIR_JUMP;
		Events &= ~COREEVENTFLAG_HOOK_ATTACH_GROUND;
		Events &= ~COREEVENTFLAG_HOOK_HIT_NOHOOK;
		Events &= ~COREEVENTFLAG_HOOK_ATTACH_PLAYER;

		// 0.6 clients detect air jumps manually, so we pretend to never do an airjump :D
		if (Server()->IsSevendown(SnappingClient))
			pCharacter->m_Jumped &= ~2;

		// skidding, from game/client/components/players.cpp
		if (SnappingClient != m_pPlayer->GetCID() && m_IsGrounded && ((pCharacter->m_Direction == -1 && (pCharacter->m_VelX/256.f) > 0.f) || (pCharacter->m_Direction == 1 && (pCharacter->m_VelX/256.f) < 0.f)))
		{
			pCharacter->m_Direction = 0;
			pCharacter->m_VelX = 0;
		}
	}
	pCharacter->m_TriggeredEvents = Events;

	if (RainbowNameAffected)
	{
		CCharacter *pSpectating = GameServer()->GetPlayerChar(m_pPlayer->GetSpectatorID());
		if (pSpectating)
		{
			pCharacter->m_Weapon = GameServer()->GetWeaponType(pSpectating->m_ActiveWeapon);
		}
	}
	else
	{
		pCharacter->m_Weapon = GameServer()->GetWeaponType(m_ActiveWeapon);
	}

	if (pCharacter->m_Weapon == -1 && GameServer()->GetClientDDNetVersion(SnappingClient) < VERSION_DDNET_TEE_NO_WEAPON)
	{
		pCharacter->m_Weapon = m_NumGrogsHolding ? WEAPON_HAMMER : WEAPON_GUN;
	}

	pCharacter->m_AttackTick = m_AttackTick;

	// change eyes and use ninja graphic if player is freeze
	if (m_DeepFreeze || m_FreezeTime > 0 || m_FreezeTime == -1)
	{
		if (pCharacter->m_Emote == EMOTE_NORMAL)
			pCharacter->m_Emote = m_DeepFreeze ? EMOTE_PAIN : EMOTE_BLINK;

		if (GameServer()->GetClientDDNetVersion(SnappingClient) < VERSION_DDNET_NEW_HUD)
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

		int Ammo = m_aWeapons[GetActiveWeapon()].m_Ammo;
		if (GetActiveWeapon() == WEAPON_TASER)
		{
			Ammo = GetTaserStrength();
		}
		else if (GetActiveWeapon() == WEAPON_PORTAL_RIFLE)
		{
			if (distance(GetCursorPos(), m_Pos) > Config()->m_SvPortalMaxDistance)
			{
				// indicate that we can't place portal right now
				Ammo = 0;
			}
			else
			{
				int Seconds = (Server()->Tick() - m_LastLinkedPortals) / Server()->TickSpeed();
				Ammo = (Seconds*10/max(Config()->m_SvPortalRifleDelay, 1));
			}
		}
		pCharacter->m_AmmoCount = clamp(Ammo, 0, 10);

		if (!Server()->IsSevendown(SnappingClient))
		{
			if (m_FreezeTime > 0 || m_FreezeTime == -1 || m_DeepFreeze)
				pCharacter->m_AmmoCount = m_FreezeTick + Config()->m_SvFreezeDelay * Server()->TickSpeed();
			else if(GetActiveWeapon() == WEAPON_NINJA)
				pCharacter->m_AmmoCount = m_Ninja.m_ActivationTick + g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000;
		}
	}

	int Flag = SendDroppedFlagCooldown(SnappingClient);
	if (Flag != -1)
	{
		CFlag *pFlag = ((CGameControllerDDRace*)GameServer()->m_pController)->m_apFlags[Flag];
		if (pFlag && !pFlag->IsAtStand())
		{
			int DroppedSinceSeconds = (Server()->Tick() - pFlag->GetDropTick()) / Server()->TickSpeed();
			int Amount = 10 - (DroppedSinceSeconds*10/max(Config()->m_SvFlagRespawnDropped, 1));
			pCharacter->m_Health = pCharacter->m_Armor = Amount;
		}
		else
		{
			pCharacter->m_Health = pCharacter->m_Armor = 10;
		}
		pCharacter->m_AmmoCount = 0;
	}

	if (GetPlayer()->m_Afk || GetPlayer()->IsPaused() || GameServer()->Arenas()->IsConfiguring(m_pPlayer->GetCID()) || Server()->DesignChanging(m_pPlayer->GetCID()))
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

	if (m_pPlayer->m_Halloween && SnappingClient == m_pPlayer->GetCID())
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
		if (m_pPlayer->m_PlayerFlags&PLAYERFLAG_SPEC_CAM) PlayerFlags |= 32;

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
	else
	{
		if(pCharacter->m_Angle > (int)(pi * 256.0f))
		{
			pCharacter->m_Angle -= (int)(2.0f * pi * 256.0f);
		}
	}
}

void CCharacter::PostSnap()
{
	m_TriggeredEvents = 0;
}

// DDRace

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

Mask128 CCharacter::TeamMask(bool SevendownOnly)
{
	return Teams()->TeamMask(Team(), -1, m_pPlayer->GetCID(), SevendownOnly);
}

Mask128 CCharacter::TeamMaskExceptSelf(bool SevendownOnly)
{
	return Teams()->TeamMask(Team(), m_pPlayer->GetCID(), m_pPlayer->GetCID(), SevendownOnly);
}

CGameTeams* CCharacter::Teams()
{
	return &((CGameControllerDDRace*)GameServer()->m_pController)->m_Teams;
}

void CCharacter::ApplyLockedTunings(bool SendTuningParams)
{
	CTuningParams* pTunings = m_TuneZone > 0 ? &GameServer()->TuningList()[m_TuneZone] : GameServer()->Tuning();
	m_Core.m_Tuning = *GameServer()->ApplyLockedTunings(pTunings, m_LockedTunings);
	if (SendTuningParams)
		GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);
}

CTuningParams *CCharacter::Tuning()
{
	return &m_Core.m_Tuning;
}

IAntibot *CCharacter::Antibot()
{
	return GameServer()->Antibot();
}

void CCharacter::FillAntibot(CAntibotCharacterData *pData)
{
	pData->m_Pos = m_Pos;
	pData->m_Vel = m_Core.m_Vel;
	pData->m_Angle = m_Core.m_Angle;
	pData->m_HookedPlayer = m_Core.m_HookedPlayer;
	pData->m_SpawnTick = m_SpawnTick;
	pData->m_WeaponChangeTick = m_WeaponChangeTick;
	pData->m_aLatestInputs[0].m_TargetX = m_LatestInput.m_TargetX;
	pData->m_aLatestInputs[0].m_TargetY = m_LatestInput.m_TargetY;
	pData->m_aLatestInputs[1].m_TargetX = m_LatestPrevInput.m_TargetX;
	pData->m_aLatestInputs[1].m_TargetY = m_LatestPrevInput.m_TargetY;
	pData->m_aLatestInputs[2].m_TargetX = m_LatestPrevPrevInput.m_TargetX;
	pData->m_aLatestInputs[2].m_TargetY = m_LatestPrevPrevInput.m_TargetY;
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
		GameServer()->Collision()->GetCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH ||
		GameServer()->Collision()->GetFCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
		GameServer()->Collision()->GetFCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH ||
		GameServer()->Collision()->GetFCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
		GameServer()->Collision()->GetFCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH) &&
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
	CArenas *pArenas = pThis->GameServer()->Arenas();
	if (Number > pCollision->GetNumAllSwitchers())
	{
		if (pArenas->FightStarted(pThis->m_pPlayer->GetCID()))
		{
			int Fight = pArenas->GetClientFight(pThis->m_pPlayer->GetCID());
			if (Number - 1 - pCollision->GetNumAllSwitchers() == Fight)
			{
				if (pArenas->IsKillBorder(Fight))
					pThis->m_pPlayer->ThreadKillCharacter(WEAPON_SELF); // Dont call Die() here, because the door array in CCollision::GetMoveRestrictions is not updated while looping, means when we die and the arena is removed it stil loops
				return true;
			}
		}
		return false;
	}
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
	{
		m_TeleCheckpoint = tcp;
		AddCheckpointList(Config()->m_SvPort, m_TeleCheckpoint);
	}

	// 1vs1 over the whole map, we need to avoid some stuff here
	bool FightStarted = GameServer()->Arenas()->FightStarted(m_pPlayer->GetCID());
	if (!FightStarted && !GameServer()->Durak()->InDurakGame(m_pPlayer->GetCID()))
	{
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
					GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You have to be in a team with other tees to start"));
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
			m_pPlayer->GiveXP(500, "for finishing the race");
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
			case HOUSE_TAVERN: Index = TILE_TAVERN; break;
			}

			if (m_TileIndex == Index || m_TileFIndex == Index)
			{
				if (m_LastIndexTile != Index && m_LastIndexFrontTile != Index)
					GameServer()->m_pHouses[i]->OnEnter(m_pPlayer->GetCID());
			}
		}

		bool MoneyTile = m_TileIndex == TILE_MONEY || m_TileFIndex == TILE_MONEY;
		bool PoliceMoneyTile = m_TileIndex == TILE_MONEY_POLICE || m_TileFIndex == TILE_MONEY_POLICE;
		bool ExtraMoneyTile = m_TileIndex == TILE_MONEY_EXTRA || m_TileFIndex == TILE_MONEY_EXTRA;
		if ((MoneyTile || PoliceMoneyTile || ExtraMoneyTile) && !m_ProcessedMoneyTile)
		{
			m_ProcessedMoneyTile = true; // when multiple speedups on a moneytile face into each other the player skips multiple tiles in one tick leading to doubled xp and money

			// Disallow money farm in ddrace team
			if (Config()->m_SvMoneyFarmTeam == 0 && Team() != TEAM_FLOCK)
				return;

			if (MoneyTile)
			{
				m_MoneyTile = MONEYTILE_NORMAL;
			}
			else if (PoliceMoneyTile)
			{
				m_MoneyTile = MONEYTILE_POLICE;

				bool IsPoliceFarmActive = GameWorld()->m_PoliceFarm.IsActive();
				if (IsPoliceFarmActive && !m_LastPoliceFarmActive)
					GameServer()->SendBroadcast("", m_pPlayer->GetCID(), false);

				if (!IsPoliceFarmActive && (m_LastPoliceFarmActive || Server()->Tick() % Server()->TickSpeed() == 0))
				{
					GameServer()->SendBroadcastFormat(m_pPlayer->GetCID(), false, Localizable("Too many players on police tiles [%d/%d]"),
						GameWorld()->m_PoliceFarm.m_NumPoliceTilePlayers, GameWorld()->m_PoliceFarm.m_MaxPoliceTilePlayers);
					m_LastPoliceFarmActive = IsPoliceFarmActive;
					return;
				}
				m_LastPoliceFarmActive = IsPoliceFarmActive;

				// disallow passive farming with grog on policefarm. use it for tactics, but not to get around the limit. passive players are also not counted to the numpolicetileplayers
				if (Config()->m_SvPoliceFarmLimit && m_Passive)
				{
					return;
				}
			}
			else if (ExtraMoneyTile)
			{
				m_MoneyTile = MONEYTILE_EXTRA;
			}

			bool Plot = GetCurrentTilePlotID() >= PLOT_START;
			int Ticks = Server()->TickSpeed();
			if (m_pPlayer->m_JailTime) // every 3 seconds only while arrested
				Ticks *= 3;
			else if (Plot) // every 2 seconds only on plot money tile
				Ticks *= 2;

			if (Server()->Tick() % Ticks == 0)
			{
				if (m_pPlayer->GetAccID() < ACC_START)
				{
					if (!IsWeaponIndicator())
						GameServer()->SendBroadcast(Localizable("You need to be logged in to use moneytiles.\nGet an account with '/register <name> <pw> <pw>'"), m_pPlayer->GetCID(), false);
					return;
				}

				CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[m_pPlayer->GetAccID()];

				int TileXP = 1;
				int TileMoney = 1;
				if (PoliceMoneyTile)
				{
					TileXP = 2;
				}
				else if (ExtraMoneyTile)
				{
					TileXP = 3;
					TileMoney = 2;
				}

				int AliveState = Plot ? 0 : GetAliveState(); // disallow survival bonus on plot money tile
				int XP = AliveState + TileXP + m_GrogSpirit;
				int Money = TileMoney;

				// police bonus
				if (PoliceMoneyTile)
					Money += pAccount->m_PoliceLevel;

				// vip bonus
				if (pAccount->m_VIP)
				{
					XP += 2;
					Money += 2;
				}

				//flag bonus
				bool FlagBonus = false;
				if (!PoliceMoneyTile && !ExtraMoneyTile && !Plot && HasFlag() != -1)
				{
					XP += 1;
					FlagBonus = true;
				}

				//grog permille
				if (m_pPlayer->m_Permille)
				{
					Money += 1;
				}

				// give money and xp
				const int BankMode = Config()->m_SvMoneyBankMode;
				if (BankMode == 2)
				{
					m_pPlayer->BankTransaction(Money);
				}
				else
				{
					m_pPlayer->WalletTransaction(Money);
				}
				m_pPlayer->GiveXP(XP);

				// broadcast
				char aSurvival[32];
				char aSpirit[32];
				char aPolice[32];
				char aPlusXP[64];

				str_format(aSurvival, sizeof(aSurvival), " +%dsurvival", AliveState);
				str_format(aSpirit, sizeof(aSpirit), " +%dspirit", m_GrogSpirit);
				str_format(aPlusXP, sizeof(aPlusXP), " +%d%s%s%s%s%s", TileXP,
					FlagBonus ? " +1flag" : "",
					pAccount->m_VIP ? " +2vip" : "",
					AliveState ? aSurvival : "",
					m_GrogSpirit ? aSpirit : "",
					m_IsDoubleXp ? " (x2)" : "");
				str_format(m_aLineExp, sizeof(m_aLineExp), "XP [%lld/%lld]%s", pAccount->m_XP, GameServer()->GetNeededXP(pAccount->m_Level), aPlusXP);

				str_format(aPolice, sizeof(aPolice), " +%dpolice", pAccount->m_PoliceLevel);
				str_format(m_aLineMoney, sizeof(m_aLineMoney), "%s [%lld] +%d%s%s%s", BankMode == 2 ? m_pPlayer->Localize("Bank") : m_pPlayer->Localize("Wallet"), BankMode == 1 ? m_pPlayer->GetWalletMoney() : pAccount->m_Money,
					TileMoney, (PoliceMoneyTile && pAccount->m_PoliceLevel) ? aPolice : "", pAccount->m_VIP ? " +2vip" : "", m_pPlayer->m_Permille ? " +1grog" : "");

				if (!IsWeaponIndicator() && !m_pPlayer->m_HideBroadcasts)
				{
					char aMsg[512];
					str_format(aMsg, sizeof(aMsg), "%s\n%s\nLevel [%d]", m_aLineMoney, m_aLineExp, pAccount->m_Level);
					SendBroadcastHud(GameServer()->FormatExperienceBroadcast(aMsg, m_pPlayer->GetCID()));
				}
			}
		}

		// taser shield
		if (m_TileIndex == TILE_TASER_SHIELD_PLUS || m_TileFIndex == TILE_TASER_SHIELD_PLUS)
		{
			m_pPlayer->m_TaserShield = min(m_pPlayer->m_TaserShield + 20, 100);
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), m_pPlayer->Localize("Congratulations, +20%% taser shield, current: %d%%. Use '/taser' to check later."), m_pPlayer->m_TaserShield);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		}

		// double-xp for +2 lifes
		if (m_TileIndex == TILE_ADD_2X_XP_TWO_LIFES || m_TileFIndex == TILE_ADD_2X_XP_TWO_LIFES)
		{
			bool FirstlyAdded = m_pPlayer->m_DoubleXpLifesLeft == 0;
			m_pPlayer->m_DoubleXpLifesLeft = min(m_pPlayer->m_DoubleXpLifesLeft + 2, 99);
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), m_pPlayer->Localize("Congratulations, double-xp has been activated for %d lifes"), m_pPlayer->m_DoubleXpLifesLeft);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
			if (FirstlyAdded)
			{
				// first enable, use one life.
				m_pPlayer->UpdateDoubleXpLifes();
			}
		}

		// money xp bomb
		if (m_TileIndex == TILE_TRANSFORM_HUMAN && m_TileFIndex == TILE_MONEY_XP_BOMB)
		{
			// only process if a zombie actually is transforming
			if (m_IsZombie && m_pPlayer->GetAccID() >= ACC_START)
			{
				m_pPlayer->GiveXP(5000, "for force human transformation");
			}
		}
		else if (!m_GotMoneyXPBomb && (m_TileIndex == TILE_MONEY_XP_BOMB || m_TileFIndex == TILE_MONEY_XP_BOMB))
		{
			if (m_pPlayer->GetAccID() < ACC_START)
			{
				if (Server()->Tick() % 50 == 0)
					GameServer()->SendBroadcast(Localizable("You need to be logged in to use moneytiles.\nGet an account with '/register <name> <pw> <pw>'"), m_pPlayer->GetCID(), false);
			}
			else if (m_pPlayer->m_LastMoneyXPBomb < Server()->Tick() - Server()->TickSpeed() * 5)
			{
				m_pPlayer->WalletTransaction(500, "from money-xp bomb");
				m_pPlayer->GiveXP(2500, "+500 money (from money-xp bomb)");

				m_GotMoneyXPBomb = true;
				m_pPlayer->m_LastMoneyXPBomb = Server()->Tick();
			}
		}

		// special finish
		if (!m_HasFinishedSpecialRace && m_DDRaceState != DDRACE_NONE && m_DDRaceState != DDRACE_CHEAT && (m_TileIndex == TILE_SPECIAL_FINISH || m_TileFIndex == TILE_SPECIAL_FINISH || FTile1 == TILE_SPECIAL_FINISH || FTile2 == TILE_SPECIAL_FINISH || FTile3 == TILE_SPECIAL_FINISH || FTile4 == TILE_SPECIAL_FINISH || Tile1 == TILE_SPECIAL_FINISH || Tile2 == TILE_SPECIAL_FINISH || Tile3 == TILE_SPECIAL_FINISH || Tile4 == TILE_SPECIAL_FINISH))
		{
			GameServer()->SendChatFormat(-1, CHAT_ALL, -1, CGameContext::CHATFLAG_ALL, Localizable("'%s' finished the special race!"), Server()->ClientName(m_pPlayer->GetCID()));
			m_pPlayer->GiveXP(750, "for finishing the special race");

			m_HasFinishedSpecialRace = true;
			GameServer()->CreateFinishConfetti(m_Pos, TeamMask());
		}
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
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You can't hit others"));
		m_Hit = DISABLE_HIT_GRENADE | DISABLE_HIT_HAMMER | DISABLE_HIT_RIFLE | DISABLE_HIT_SHOTGUN;
	}
	else if (((m_TileIndex == TILE_HIT_START) || (m_TileFIndex == TILE_HIT_START)) && m_Hit != HIT_ALL)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You can hit others"));
		m_Hit = HIT_ALL;
	}

	// collide with others
	if (((m_TileIndex == TILE_NPC_END) || (m_TileFIndex == TILE_NPC_END)) && m_Core.m_Collision)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You can't collide with others"));
		m_Core.m_Collision = false;
	}
	else if (((m_TileIndex == TILE_NPC_START) || (m_TileFIndex == TILE_NPC_START)) && !m_Core.m_Collision)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You can collide with others"));
		m_Core.m_Collision = true;
	}

	// hook others
	if (((m_TileIndex == TILE_NPH_END) || (m_TileFIndex == TILE_NPH_END)) && m_Core.m_Hook)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You can't hook others"));
		m_Core.m_Hook = false;
	}
	else if (((m_TileIndex == TILE_NPH_START) || (m_TileFIndex == TILE_NPH_START)) && !m_Core.m_Hook)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You can hook others"));
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
			m_Core.m_JumpedTotal = m_Core.m_Jumps >= 2 ? m_Core.m_Jumps - 2 : 0;
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
			SpookyGhost(!m_pPlayer->m_HasSpookyGhost);
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
		{
			if (m_PassiveEndTick)
			{
				// simply don't deactivate passive when we hit a tile after being redirected
				m_PassiveEndTick = 0;
			}
			else
			{
				Passive(!m_Passive);
			}
		}
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
			str_format(aBuf, sizeof(aBuf), m_pPlayer->Localize("You got +1 jump and can jump %d times now"), m_Core.m_Jumps);
			GameServer()->SendChatTarget(GetPlayer()->GetCID(), aBuf);
		}
	}

	// helper only
	if ((m_TileIndex == TILE_HELPERS_ONLY) || (m_TileFIndex == TILE_HELPERS_ONLY))
	{
		if (Server()->GetAuthedState(m_pPlayer->GetCID()) < AUTHED_HELPER)
		{
			GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("This area is for helpers only"));
			Die(WEAPON_WORLD);
			return;
		}
	}

	// moderator only
	if ((m_TileIndex == TILE_MODERATORS_ONLY) || (m_TileFIndex == TILE_MODERATORS_ONLY))
	{
		if (Server()->GetAuthedState(m_pPlayer->GetCID()) < AUTHED_MOD)
		{
			GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("This area is for moderators only"));
			Die(WEAPON_WORLD);
			return;
		}
	}

	// admin only
	if ((m_TileIndex == TILE_ADMINS_ONLY) || (m_TileFIndex == TILE_ADMINS_ONLY))
	{
		if (Server()->GetAuthedState(m_pPlayer->GetCID()) < AUTHED_ADMIN)
		{
			GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("This area is for admins only"));
			Die(WEAPON_WORLD);
			return;
		}
	}

	// vip plus only
	if ((m_TileIndex == TILE_VIP_PLUS_ONLY) || (m_TileFIndex == TILE_VIP_PLUS_ONLY) || (m_TileIndex == TILE_REM_FIRST_PORTAL) || (m_TileFIndex == TILE_REM_FIRST_PORTAL))
	{
		// only reset portal if we have only shot the first one. if both are there already it means they are either both inside, or both outside.
		ResetOnlyFirstPortal();
	}

	// gets checked in flag.cpp already, but in case a flag stop tile is placed in a teleporter, we wanna drop it
	if ((m_TileIndex == TILE_FLAG_STOP) || (m_TileFIndex == TILE_FLAG_STOP))
	{
		int Flag = HasFlag();
		if (Flag != -1)
		{
			CFlag *pFlag = ((CGameControllerDDRace*)GameServer()->m_pController)->m_apFlags[Flag];
			pFlag->ResetPrevPos();
		}
	}

	int PlotDoor = GameServer()->Collision()->GetPlotBySwitch(GameServer()->Collision()->CheckPointDoor(m_Pos, Team(), true, false));
	if (PlotDoor >= PLOT_START && m_pPlayer->m_pPortal[PORTAL_FIRST] && !m_pPlayer->m_pPortal[PORTAL_SECOND])
	{
		m_pPlayer->m_pPortal[PORTAL_FIRST]->SetThroughPlotDoor(PlotDoor);
	}

	if ((m_TileIndex == TILE_NO_BONUS_AREA) || (m_TileFIndex == TILE_NO_BONUS_AREA))
		OnNoBonusArea(true);
	else if ((m_TileIndex == TILE_NO_BONUS_AREA_LEAVE) || (m_TileFIndex == TILE_NO_BONUS_AREA_LEAVE))
		OnNoBonusArea(false);

	if ((m_TileIndex == TILE_BIRTHDAY_ENABLE) || (m_TileFIndex == TILE_BIRTHDAY_ENABLE))
	{
		m_pPlayer->m_IsBirthdayGift = true;
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("Seems like it's your birthday! You may pick up your present at any time."));
	}
	else if ((m_TileIndex == TILE_BIRTHDAY_JETPACK_RECV || m_TileFIndex == TILE_BIRTHDAY_JETPACK_RECV) && m_LastBirthdayMsg + Server()->TickSpeed() < Server()->Tick())
	{
		m_LastBirthdayMsg = Server()->Tick();
		if (m_IsZombie)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("This gift is only dedicated to humans."));
		}
		else if (m_pPlayer->m_IsBirthdayGift)
		{
			if (m_BirthdayGiftEndTick || m_Jetpack)
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("You've picked up your present already! Come back next year."));
			}
			else
			{
				SetBirthdayJetpack(true);
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("Happy birthday! :) You have jetpack for 45 seconds"));
			}
		}
		else
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("Sorry, someting is missing to get the gift."));
		}
	}

	bool Zombie = (m_TileIndex == TILE_TRANSFORM_ZOMBIE || m_TileFIndex == TILE_TRANSFORM_ZOMBIE) && HasFlag() == -1;
	bool DoTransformation = Zombie || (m_TileIndex == TILE_TRANSFORM_HUMAN || m_TileFIndex == TILE_TRANSFORM_HUMAN);
	if (DoTransformation)
	{
		SetZombieHuman(Zombie);
	}

	// update this AFTER you are done using this var above
	m_LastIndexTile = m_TileIndex;
	m_LastIndexFrontTile = m_TileFIndex;

	// unlock team
	if (((m_TileIndex == TILE_UNLOCK_TEAM) || (m_TileFIndex == TILE_UNLOCK_TEAM)) && Teams()->TeamLocked(Team()))
	{
		Teams()->SetTeamLock(Team(), false);
		GameServer()->SendChatTeam(Team(), Localizable("Your team was unlocked by an unlock team tile"));
	}

	// solo part
	if (((m_TileIndex == TILE_SOLO_START) || (m_TileFIndex == TILE_SOLO_START)) && !Teams()->m_Core.GetSolo(m_pPlayer->GetCID()))
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You are now in a solo part"));
		SetSolo(true);
	}
	else if (((m_TileIndex == TILE_SOLO_END) || (m_TileFIndex == TILE_SOLO_END)) && Teams()->m_Core.GetSolo(m_pPlayer->GetCID()))
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You are now out of the solo part"));
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
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("Teleport gun enabled"));
	}
	else if (((m_TileIndex == TILE_TELE_GUN_DISABLE) || (m_TileFIndex == TILE_TELE_GUN_DISABLE)) && m_HasTeleGun)
	{
		m_HasTeleGun = false;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("Teleport gun disabled"));
	}

	if (((m_TileIndex == TILE_TELE_GRENADE_ENABLE) || (m_TileFIndex == TILE_TELE_GRENADE_ENABLE)) && !m_HasTeleGrenade)
	{
		m_HasTeleGrenade = true;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("Teleport grenade enabled"));
	}
	else if (((m_TileIndex == TILE_TELE_GRENADE_DISABLE) || (m_TileFIndex == TILE_TELE_GRENADE_DISABLE)) && m_HasTeleGrenade)
	{
		m_HasTeleGrenade = false;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("Teleport grenade disabled"));
	}

	if (((m_TileIndex == TILE_TELE_LASER_ENABLE) || (m_TileFIndex == TILE_TELE_LASER_ENABLE)) && !m_HasTeleLaser)
	{
		m_HasTeleLaser = true;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("Teleport laser enabled"));
	}
	else if (((m_TileIndex == TILE_TELE_LASER_DISABLE) || (m_TileFIndex == TILE_TELE_LASER_DISABLE)) && m_HasTeleLaser)
	{
		m_HasTeleLaser = false;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("Teleport laser disabled"));
	}

	if ((m_MoveRestrictions&CANTMOVE_ROOM) && m_RoomAntiSpamTick < Server()->Tick())
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("You need a key to enter this room, buy one in the shop"));
		m_RoomAntiSpamTick = Server()->Tick() + Server()->TickSpeed() * 5;
	}

	if ((m_MoveRestrictions&CANTMOVE_VIP_PLUS_ONLY) && m_VipPlusAntiSpamTick < Server()->Tick())
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("This area is for VIP+ only, buy it in the shop"));
		m_VipPlusAntiSpamTick = Server()->Tick() + Server()->TickSpeed() * 5;
	}

	// stopper
	if(m_Core.m_Vel.y > 0 && (m_MoveRestrictions&CANTMOVE_DOWN))
	{
		m_Core.m_Jumped = 0;
		m_Core.m_JumpedTotal = 0;
	}
	m_Core.m_Vel = ClampVel(m_MoveRestrictions, m_Core.m_Vel);

	// handle switch tiles
	std::vector<int> vCurrentNumbers;
	std::vector<int> vButtonNumbers = GameServer()->Collision()->GetButtonNumbers(MapIndex);
	for (unsigned int i = 0; i < vButtonNumbers.size(); i++)
	{
		if (vButtonNumbers[i] > 0 && Team() != TEAM_SUPER)
		{
			if (std::find(m_vLastButtonNumbers.begin(), m_vLastButtonNumbers.end(), vButtonNumbers[i]) == m_vLastButtonNumbers.end())
			{
				GameServer()->Collision()->m_pSwitchers[vButtonNumbers[i]].m_Status[Team()] ^= 1;
				GameServer()->Collision()->m_pSwitchers[vButtonNumbers[i]].m_EndTick[Team()] = 0;
				GameServer()->Collision()->m_pSwitchers[vButtonNumbers[i]].m_ClientID[Team()] = m_pPlayer->GetCID();
				GameServer()->Collision()->m_pSwitchers[vButtonNumbers[i]].m_StartTick[Team()] = Server()->Tick();

				// little sound for the button
				if (GameServer()->Collision()->IsPlotDrawDoor(vButtonNumbers[i]))
					GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, TeamMask());
			}

			vCurrentNumbers.push_back(vButtonNumbers[i]);
		}
	}
	m_vLastButtonNumbers = vCurrentNumbers;

	int SwitchNumber = GameServer()->Collision()->GetSwitchNumber(MapIndex);
	CCollision::SSwitchers *pSwitcher = SwitchNumber > 0 ? &GameServer()->Collision()->m_pSwitchers[SwitchNumber] : 0;
	if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCHOPEN && Team() != TEAM_SUPER && SwitchNumber > 0)
	{
		pSwitcher->m_Status[Team()] = true;
		pSwitcher->m_EndTick[Team()] = 0;
		pSwitcher->m_Type[Team()] = TILE_SWITCHOPEN;
		pSwitcher->m_LastUpdateTick[Team()] = Server()->Tick();
		// F-DDrace
		pSwitcher->m_ClientID[Team()] = m_pPlayer->GetCID();
		pSwitcher->m_StartTick[Team()] = Server()->Tick();
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCHTIMEDOPEN && Team() != TEAM_SUPER && SwitchNumber > 0)
	{
		pSwitcher->m_Status[Team()] = true;
		pSwitcher->m_EndTick[Team()] = Server()->Tick() + 1 + GameServer()->Collision()->GetSwitchDelay(MapIndex) * Server()->TickSpeed();
		pSwitcher->m_Type[Team()] = TILE_SWITCHTIMEDOPEN;
		pSwitcher->m_LastUpdateTick[Team()] = Server()->Tick();
		// F-DDrace
		pSwitcher->m_ClientID[Team()] = m_pPlayer->GetCID();
		pSwitcher->m_StartTick[Team()] = Server()->Tick();
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCHTIMEDCLOSE && Team() != TEAM_SUPER && SwitchNumber > 0)
	{
		pSwitcher->m_Status[Team()] = false;
		pSwitcher->m_EndTick[Team()] = Server()->Tick() + 1 + GameServer()->Collision()->GetSwitchDelay(MapIndex) * Server()->TickSpeed();
		pSwitcher->m_Type[Team()] = TILE_SWITCHTIMEDCLOSE;
		pSwitcher->m_LastUpdateTick[Team()] = Server()->Tick();
		// F-DDrace
		pSwitcher->m_ClientID[Team()] = m_pPlayer->GetCID();
		pSwitcher->m_StartTick[Team()] = Server()->Tick();
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCHCLOSE && Team() != TEAM_SUPER && SwitchNumber > 0)
	{
		pSwitcher->m_Status[Team()] = false;
		pSwitcher->m_EndTick[Team()] = 0;
		pSwitcher->m_Type[Team()] = TILE_SWITCHCLOSE;
		pSwitcher->m_LastUpdateTick[Team()] = Server()->Tick();
		// F-DDrace
		pSwitcher->m_ClientID[Team()] = m_pPlayer->GetCID();
		pSwitcher->m_StartTick[Team()] = Server()->Tick();
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_FREEZE && Team() != TEAM_SUPER)
	{
		if (SwitchNumber == 0 || pSwitcher->m_Status[Team()])
			Freeze(GameServer()->Collision()->GetSwitchDelay(MapIndex));
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_DFREEZE && Team() != TEAM_SUPER)
	{
		if (SwitchNumber == 0 || pSwitcher->m_Status[Team()])
			m_DeepFreeze = true;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_DUNFREEZE && Team() != TEAM_SUPER)
	{
		if (SwitchNumber == 0 || pSwitcher->m_Status[Team()])
			m_DeepFreeze = false;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_START && m_Hit & DISABLE_HIT_HAMMER && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_HAMMER)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You can hammer hit others"));
		m_Hit &= ~DISABLE_HIT_HAMMER;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_END && !(m_Hit & DISABLE_HIT_HAMMER) && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_HAMMER)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You can't hammer hit others"));
		m_Hit |= DISABLE_HIT_HAMMER;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_START && m_Hit & DISABLE_HIT_SHOTGUN && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_SHOTGUN)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You can shoot others with shotgun"));
		m_Hit &= ~DISABLE_HIT_SHOTGUN;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_END && !(m_Hit & DISABLE_HIT_SHOTGUN) && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_SHOTGUN)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You can't shoot others with shotgun"));
		m_Hit |= DISABLE_HIT_SHOTGUN;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_START && m_Hit & DISABLE_HIT_GRENADE && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_GRENADE)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You can shoot others with grenade"));
		m_Hit &= ~DISABLE_HIT_GRENADE;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_END && !(m_Hit & DISABLE_HIT_GRENADE) && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_GRENADE)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You can't shoot others with grenade"));
		m_Hit |= DISABLE_HIT_GRENADE;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_START && m_Hit & DISABLE_HIT_RIFLE && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_LASER)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You can shoot others with rifle"));
		m_Hit &= ~DISABLE_HIT_RIFLE;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_END && !(m_Hit & DISABLE_HIT_RIFLE) && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_LASER)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("You can't shoot others with rifle"));
		m_Hit |= DISABLE_HIT_RIFLE;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_JUMP)
	{
		int NewJumps = GameServer()->Collision()->GetSwitchDelay(MapIndex);
		if(NewJumps == 255)
		{
			NewJumps = -1;
		}
		SetJumps(NewJumps);
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
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCH_REDIRECT_SERVER_FROM && Team() != TEAM_SUPER && SwitchNumber > 0)
	{
		if (FightStarted || m_pPlayer->m_IsDummy)
		{
			Die(WEAPON_SELF);
			return;
		}

		int Port = GameServer()->GetRediretListPort(SwitchNumber);
		if (!TrySafelyRedirectClient(Port))
			LoadRedirectTile(Port);
		return;
	}
	else if (GameServer()->Collision()->IsSwitch(MapIndex) == TILE_DURAK_SEAT && SwitchNumber > 0 && GameServer()->Collision()->GetMapIndex(m_Pos) == MapIndex) // check for mapindex so m_Pos..m_PrevPos intersection doesnt trigger
	{
		if (FightStarted)
		{
			Die(WEAPON_SELF);
			return;
		}

		int Delay = GameServer()->Collision()->GetSwitchDelay(MapIndex);
		GameServer()->Durak()->OnCharacterSeat(m_pPlayer->GetCID(), SwitchNumber, Delay-1);
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
	int evilz = GameServer()->Collision()->IsEvilTeleport(MapIndex);
	// Reset inout teleporter if we aint on one
	if ((!z || z != m_LastInOutTeleporter) && (!evilz || evilz != m_LastInOutTeleporter))
	{
		m_LastInOutTeleporter = 0;
	}

	if (!Config()->m_SvOldTeleportHook && !Config()->m_SvOldTeleportWeapons && z && Controller->m_TeleOuts[z - 1].size())
	{
		if (m_Super)
			return;

		if (FightStarted)
		{
			Die(WEAPON_SELF);
			return;
		}

		int Num = Controller->m_TeleOuts[z - 1].size();
		vec2 NewPos = Controller->m_TeleOuts[z - 1][(!Num) ? Num : rand() % Num];
		if (GameServer()->Collision()->IsTeleportInOut(MapIndex))
		{
			if (m_LastInOutTeleporter == z || Num <= 1) // dont teleport when only 1 is there, bcs its our current tile then
				return;
			m_LastInOutTeleporter = z;

			while (GameServer()->Collision()->GetPureMapIndex(NewPos) == MapIndex)
				NewPos = Controller->m_TeleOuts[z - 1][rand() % Num];
		}

		ForceSetPos(NewPos);
		if (!Config()->m_SvTeleportHoldHook)
		{
			m_Core.SetHookedPlayer(-1);
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
	if (evilz && Controller->m_TeleOuts[evilz - 1].size())
	{
		if (m_Super)
			return;

		if (FightStarted)
		{
			Die(WEAPON_SELF);
			return;
		}

		int Num = Controller->m_TeleOuts[evilz - 1].size();
		vec2 NewPos = Controller->m_TeleOuts[evilz - 1][(!Num) ? Num : rand() % Num];
		if (GameServer()->Collision()->IsTeleportInOut(MapIndex))
		{
			if (m_LastInOutTeleporter == evilz || Num <= 1) // dont teleport when only 1 is there, bcs its our current tile then
				return;
			m_LastInOutTeleporter = evilz;

			while (GameServer()->Collision()->GetPureMapIndex(NewPos) == MapIndex)
				NewPos = Controller->m_TeleOuts[evilz - 1][rand() % Num];
		}

		ForceSetPos(NewPos);
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
				if (FightStarted)
				{
					Die(WEAPON_SELF);
					return;
				}

				int Num = Controller->m_TeleCheckOuts[k].size();
				ForceSetPos(Controller->m_TeleCheckOuts[k][(!Num) ? Num : rand() % Num]);
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
		if (GameServer()->m_pController->CanSpawn(&SpawnPos, ENTITY_SPAWN, Team()))
		{
			if (FightStarted)
			{
				Die(WEAPON_SELF);
				return;
			}

			ForceSetPos(SpawnPos);
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
				if (FightStarted)
				{
					Die(WEAPON_SELF);
					return;
				}

				int Num = Controller->m_TeleCheckOuts[k].size();
				ForceSetPos(Controller->m_TeleCheckOuts[k][(!Num) ? Num : rand() % Num]);

				if (!Config()->m_SvTeleportHoldHook)
				{
					m_Core.SetHookedPlayer(-1);
					m_Core.m_HookState = HOOK_RETRACTED;
					m_Core.m_HookPos = m_Core.m_Pos;
				}
				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if (GameServer()->m_pController->CanSpawn(&SpawnPos, ENTITY_SPAWN, Team()))
		{
			if (FightStarted)
			{
				Die(WEAPON_SELF);
				return;
			}

			ForceSetPos(SpawnPos);

			if (!Config()->m_SvTeleportHoldHook)
			{
				m_Core.SetHookedPlayer(-1);
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

	// -1 resets tune lock
	int TuneLock = GameServer()->Collision()->IsTuneLock(CurrentIndex);
	if(TuneLock)
	{
		GameServer()->ApplyTuneLock(&m_LockedTunings, TuneLock);

		if(m_LockedTunings != m_LastLockedTunings)
		{
			ApplyLockedTunings(); // Update before sending new tuning params, or it will create prediction errors
			SendTuneMsg(GameServer()->m_aaTuneLockMsg[TuneLock == -1 ? 0 : TuneLock]); // -1 = tune lock reset, number 0 is used to set the message
			m_LastLockedTunings = m_LockedTunings;
		}
	}

	ApplyLockedTunings(false); // throw tunings from specific zone into gamecore

	if (m_TuneZone != m_TuneZoneOld) // don't send tunigs all the time
	{
		// send zone leave msg
		SendTuneMsg(GameServer()->m_aaZoneLeaveMsg[m_TuneZoneOld]);

		// send zone enter msg
		SendTuneMsg(GameServer()->m_aaZoneEnterMsg[m_TuneZone]);
	}
}

void CCharacter::SendTuneMsg(const char *pMessage)
{
	if(!pMessage[0])
		return;

	const char *pCur = pMessage;
	const char *pPos;
	while((pPos = str_find(pCur, "\\n")))
	{
		char aBuf[256];
		str_copy(aBuf, pCur, pPos - pCur + 1);
		aBuf[pPos - pCur + 1] = '\0';
		pCur = pPos + 2;
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
	}
	GameServer()->SendChatTarget(m_pPlayer->GetCID(), pCur);
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
			GameServer()->CreateDamage(m_Pos, m_pPlayer->GetCID(), vec2(0, 0), (m_FreezeTime + 1) / Server()->TickSpeed(), 0, true, TeamMask() & GameServer()->ClientsMaskExcludeClientVersionAndHigher(VERSION_DDNET_NEW_HUD));
		}
		if (m_FreezeTime > 0)
			m_FreezeTime--;
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

	m_FrozenLastTick = false;

	if ((m_DeepFreeze || m_pPlayer->m_ClanProtectionPunished) && !m_Super)
		Freeze();

	// following jump rules can be overridden by tiles, like Refill Jumps, Stopper and Wall Jump
	if(m_Core.m_Jumps == -1)
	{
		// The player has only one ground jump, so his feet are always dark
		m_Core.m_Jumped |= 2;
	}
	else if(m_Core.m_Jumps == 0)
	{
		// The player has no jumps at all, so his feet are always dark
		m_Core.m_Jumped |= 2;
	}
	else if(m_Core.m_Jumps == 1 && m_Core.m_Jumped > 0)
	{
		// If the player has only one jump, each jump is the last one
		m_Core.m_Jumped |= 2;
	}
	else if(m_Core.m_JumpedTotal < m_Core.m_Jumps - 1 && m_Core.m_Jumped > 1)
	{
		// The player has not yet used up all his jumps, so his feet remain light
		m_Core.m_Jumped = 1;
	}

	if((m_Super || m_SuperJump) && m_Core.m_Jumped > 1)
	{
		// Super players and players with infinite jumps always have light feet
		m_Core.m_Jumped = 1;
	}

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

		// Reset
		m_LastIndexTile = 0;
		m_LastIndexFrontTile = 0;
		m_vLastButtonNumbers.clear();
		m_LastInOutTeleporter = 0;

		if (!m_Alive)
			return;
	}

	m_ProcessedMoneyTile = false;

	// teleport gun
	if (m_TeleGunTeleport)
	{
		GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID(), TeamMask());
		m_Core.m_Pos = m_TeleGunPos;
		if (!m_IsBlueTeleGunTeleport)
			m_Core.m_Vel = vec2(0, 0);
		GameServer()->CreateDeath(m_TeleGunPos, m_pPlayer->GetCID(), TeamMask());
		GameServer()->CreateSound(m_TeleGunPos, SOUND_WEAPON_SPAWN, TeamMask());
		m_TeleGunTeleport = false;
		m_IsBlueTeleGunTeleport = false;
	}

	HandleBroadcast();

	if (!m_IsFrozen)
		m_FirstFreezeTick = 0;

	m_IsGrounded = IsGrounded();
}

bool CCharacter::Freeze(float Seconds)
{
	m_IsFrozen = true;

	if ((Seconds <= 0 || m_Super || m_FreezeTime == -1 || m_FreezeTime > Seconds * Server()->TickSpeed()) && Seconds != -1)
		return false;
	if (m_FreezeTick < Server()->Tick() - Server()->TickSpeed() || Seconds == -1)
	{
		if (m_FreezeTick == 0 || m_FirstFreezeTick == 0)
		{
			m_FirstFreezeTick = Server()->Tick();

			if (!Server()->IsSevendown(m_pPlayer->GetCID()) && Config()->m_SvFreezePrediction)
			{
				GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);
			}
		}
		m_FreezeTime = Seconds == -1 ? Seconds : Seconds * Server()->TickSpeed();
		m_FreezeTick = Server()->Tick();
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

		if (!Server()->IsSevendown(m_pPlayer->GetCID()) && Config()->m_SvFreezePrediction)
		{
			GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);
		}

		if (!m_GotLasered && !GameServer()->Arenas()->FightStarted(m_pPlayer->GetCID()))
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
			m_Core.SetHookedPlayer(-1);
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
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), m_pPlayer->Localize("Please join a team before you start"));
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
	m_PowerHookedID = -1;
	m_IsRainbowHooked = false;
	for (int i = 0; i < NUM_WEAPONS; i++)
		m_aSpreadWeapon[i] = false;
	m_FakeTuneCollision = false;
	m_OldFakeTuneCollision = false;
	m_Passive = false;
	m_PassiveSnapID = Server()->SnapNewID();
	m_PoliceHelper = false;
	m_pTelekinesisEntity = 0;
	m_pLightsaber = 0;
	m_Item = -3;
	m_DoorHammer = false;
	m_pHelicopter = 0;

	m_AlwaysTeleWeapon = Config()->m_SvAlwaysTeleWeapon;

	m_pPlayer->m_Gamemode = (Config()->m_SvVanillaModeStart || m_pPlayer->m_Gamemode == GAMEMODE_VANILLA) ? GAMEMODE_VANILLA : GAMEMODE_DDRACE;
	m_pPlayer->m_SavedGamemode = m_pPlayer->m_Gamemode;
	m_Armor = m_pPlayer->m_Gamemode == GAMEMODE_VANILLA ? 0 : 10;

	m_TabDoubleClickCount = 0;
	m_IsPortalBlocker = false;
	m_PortalBlockerIndSnapID = Server()->SnapNewID();

	m_CollectedPortalRifle = false;
	m_LastBatteryDrop = 0;

	m_InitializedSpawnWeapons = false;
	for (int i = 0; i < 3; i++)
		m_aSpawnWeaponActive[i] = false;

	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[m_pPlayer->GetAccID()];
	if (!m_pPlayer->IsMinigame() && !m_pPlayer->m_JailTime)
	{
		if (Config()->m_SvSpawnWeapons)
		{
			for (int i = 0; i < 3; i++)
			{
				if (pAccount->m_SpawnWeapon[i])
				{
					m_aSpawnWeaponActive[i] = true;
					GiveWeapon(i == 0 ? WEAPON_SHOTGUN : i == 1 ? WEAPON_GRENADE : WEAPON_LASER, false, pAccount->m_SpawnWeapon[i]);
				}
			}
			m_InitializedSpawnWeapons = true;
		}

		if (pAccount->m_PortalRifle)
			GiveWeapon(WEAPON_PORTAL_RIFLE, false, -1, true);
	}

	int64 Now = Server()->Tick();

	if (pAccount->m_VIP == VIP_PLUS)
		m_Core.m_MoveRestrictionExtra.m_VipPlus = true;
	m_VipPlusAntiSpamTick = Now;

	if (m_pPlayer->m_HasRoomKey)
		m_Core.m_MoveRestrictionExtra.m_RoomKey = true;
	m_RoomAntiSpamTick = Now;

	m_HasFinishedSpecialRace = false;
	m_GotMoneyXPBomb = false;
	m_SpawnTick = Now;
	m_WeaponChangeTick = Now;
	m_MoneyTile = MONEYTILE_NONE;
	m_ProcessedMoneyTile = false;
	m_LastPoliceFarmActive = true;
	m_GotLasered = false;
	m_KillStreak = 0;
	m_pTeeControlCursor = 0;

	m_LastTouchedSwitcher = -1;
	m_LastTouchedPortalBy = -1;

	m_LastLinkedPortals = Now;
	m_LastWantedWeapon = 0;
	m_LastWantedLogout = 0;
	m_MaxJumps = m_Core.m_Jumps;

	m_LastMoneyDrop = 0;

	for (int i = 0; i < NUM_WEAPONS; i++)
		m_aHadWeapon[i] = false;

	m_LastTaserUse = Now;
	m_FirstFreezeTick = 0;

	if (GameServer()->Arenas()->FightStarted(m_pPlayer->GetCID()))
		m_Core.m_FightStarted = true;

	m_ViewCursorSnapID = Server()->SnapNewID();
	m_DynamicCamera = false;
	m_CameraMaxLength = 0.f;

	m_LastWeaponIndTick = 0;
	m_LastInOutTeleporter = 0;

	m_DrawEditor.Init(this);
	m_Snake.Init(this);
	m_InSnake = false;
	m_Lovely = false;
	m_RotatingBall = false;
	m_EpicCircle = false;
	m_StaffInd = false;
	m_Confetti = false;

	for (int i = 0; i < EUntranslatedMap::NUM_IDS; i++)
		m_aUntranslatedID[i] = Server()->SnapNewID();

	m_pPortalBlocker = 0;
	m_LastNoBonusTick = 0;
	m_RedirectTilePort = 0;
	m_PassiveEndTick = 0;
	m_LastJumpedTotal = 0;
	m_HookExceededTick = 0;
	m_IsGrounded = false;
	m_aLineExp[0] = '\0';
	m_aLineMoney[0] = '\0';

	int Permille = m_pPlayer->m_Permille;
	m_pPlayer->m_Permille = 0;
	if (m_pPlayer->m_JailTime)
		IncreasePermille(Permille);
	m_pGrog = 0;
	m_LastGrogHoldMsg = 0;
	m_NumGrogsHolding = 0;
	m_FirstPermilleTick = 0;
	m_DeadlyPermilleDieTick = 0;
	m_GrogSpirit = 0;
	m_NextGrogEmote = 0;
	m_NextGrogBalance = 0;
	m_NextGrogDirDelay = 0;
	m_GrogDirDelayEnd = 0;
	m_GrogBalancePosX = GROG_BALANCE_POS_UNSET;
	m_GrogDirection = -1;

	m_BirthdayGiftEndTick = 0;
	m_LastBirthdayMsg = 0;
	m_IsZombie = false;

	m_IsDoubleXp = false;

	m_HitSaved = m_Hit;
	m_vCheckpoints.clear();
	m_LockedTunings.clear();
	m_LastLockedTunings.clear();

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
	case DUMMYMODE_TAVERN_DUMMY: // fallthrough
	case DUMMYMODE_BANK_DUMMY: m_pDummyHandle = new CDummyHouse(this, Dummymode); break;
	case DUMMYMODE_V3_BLOCKER: m_pDummyHandle = new CDummyV3Blocker(this); break;
	case DUMMYMODE_CHILLBLOCK5_POLICE: m_pDummyHandle = new CDummyChillBlock5Police(this); break;
	}
}

void CCharacter::HandleCursor()
{
	// check whether player uses dynamic camera, dynamic camera sends about 633 as maximal range
	float CameraLength = length(vec2(m_Input.m_TargetX, m_Input.m_TargetY));
	if (CameraLength > m_CameraMaxLength)
	{
		m_DynamicCamera = CameraLength > 632.0f && CameraLength < 634.0f;
		m_CameraMaxLength = CameraLength;
	}

	// normal cusor position for zoom 1.0 (level 10)
	m_CursorPos = vec2(m_Pos.x + m_Input.m_TargetX, m_Pos.y + m_Input.m_TargetY);

	// cursor pos that matches the current zoom level, this is perfectly on the cursor rendered by the client
	CalculateCursorPosZoomed();
}

void CCharacter::CalculateCursorPosZoomed()
{
	vec2 Pos = m_Pos;
	vec2 MousePos = vec2(m_Input.m_TargetX, m_Input.m_TargetY);

	int DDNetVersion = GameServer()->GetClientDDNetVersion(m_pPlayer->GetCID());
	if (DDNetVersion >= VERSION_DDNET_PLAYERFLAG_SPEC_CAM)
	{
		m_CursorPosZoomed = m_pPlayer->m_CameraInfo.ConvertTargetToWorld(Pos, MousePos);
		return;
	}

	if (m_DynamicCamera)
	{
		vec2 TargetCameraOffset(0, 0);
		float l = length(MousePos);
		if(l > 0.0001f) // make sure that this isn't 0
		{
			float FollowFactor = m_pPlayer->m_CameraInfo.m_FollowFactor / 100.0f;
			float OffsetAmount = max(l - m_pPlayer->m_CameraInfo.m_Deadzone, 0.0f) * FollowFactor;

			TargetCameraOffset = normalize(MousePos) * OffsetAmount;
			Pos -= TargetCameraOffset * (m_pPlayer->GetZoomLevel() - 1.f);
		}
	}

	float TargetX = m_Input.m_TargetX;
	float TargetY = m_Input.m_TargetY;
	if (DDNetVersion < VERSION_DDNET_ZOOM_CURSOR)
	{
		TargetX *= m_pPlayer->GetZoomLevel();
		TargetY *= m_pPlayer->GetZoomLevel();
	}
	m_CursorPosZoomed = vec2(Pos.x + TargetX, Pos.y + TargetY);
}

vec2 CCharacter::GetCursorPos()
{
	if (m_pPlayer->m_ZoomCursor && !m_pPlayer->RestrictZoom())
		return m_CursorPosZoomed;
	return m_CursorPos;
}

void CCharacter::FDDraceTick()
{
	// cursor calculation
	HandleCursor();

	// fake tune collision
	if (!Server()->IsSevendown(m_pPlayer->GetCID()) && m_Core.m_FakeTuneCID != -1)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(m_Core.m_FakeTuneCID);
		m_FakeTuneCollision = (!m_Super && (m_Solo || m_Passive)) || (pChr && !pChr->m_Super && (pChr->m_Solo || pChr->m_Passive || (Team() != pChr->Team() && m_pPlayer->m_ShowOthers)));

		if (m_FakeTuneCollision != m_OldFakeTuneCollision)
			GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);

		m_OldFakeTuneCollision = m_FakeTuneCollision;
		m_Core.m_FakeTuneCID = -1;
	}

	// update telekinesis entitiy position
	if (m_pTelekinesisEntity)
	{
		if (GetActiveWeapon() == WEAPON_TELEKINESIS && !m_FreezeTime && !m_pPlayer->IsPaused())
		{
			vec2 Vel = vec2(0.f, 0.f);

			if (m_pTelekinesisEntity->GetObjType() == CGameWorld::ENTTYPE_CHARACTER)
			{
				CCharacter *pChr = (CCharacter *)m_pTelekinesisEntity;
				pChr->Core()->m_Pos = GetCursorPos();
				pChr->Core()->m_Vel = Vel;
			}
			else if (m_pTelekinesisEntity->IsAdvancedEntity())
			{
				CAdvancedEntity* pEntity = (CAdvancedEntity*)m_pTelekinesisEntity;
				pEntity->SetPos(GetCursorPos());
				pEntity->SetVel(Vel);
			}
		}
		else
			m_pTelekinesisEntity = 0;
	}

	// retract lightsaber
	if (m_pLightsaber && (m_FreezeTime || GetActiveWeapon() != WEAPON_LIGHTSABER))
		m_pLightsaber->Retract();

	// flag bonus
	if (HasFlag() != -1 && m_pPlayer->GetAccID() >= ACC_START)
	{
		if (!m_MoneyTile && Server()->Tick() % 50 == 0)
		{
			// Reset here for voting menu
			m_aLineMoney[0] = '\0';
			CGameContext::AccountInfo* pAccount = &GameServer()->m_Accounts[m_pPlayer->GetAccID()];

			int AliveState = GetAliveState();
			int XP = 0;
			XP += AliveState + 1 + m_GrogSpirit;

			if (pAccount->m_VIP)
				XP += 2;

			m_pPlayer->GiveXP(XP);

			char aSurvival[32];
			char aSpirit[32];
			str_format(aSurvival, sizeof(aSurvival), " +%dsurvival", AliveState);
			str_format(aSpirit, sizeof(aSpirit), " +%dspirit", m_GrogSpirit);
			str_format(m_aLineExp, sizeof(m_aLineExp), "XP [%lld/%lld] +1flag%s%s%s%s", pAccount->m_XP, GameServer()->GetNeededXP(pAccount->m_Level),
				pAccount->m_VIP ? " +2vip" : "",
				AliveState ? aSurvival : "",
				m_GrogSpirit ? aSpirit : "",
				m_IsDoubleXp ? " (x2)" : "");
			if (!IsWeaponIndicator() && !m_pPlayer->m_HideBroadcasts)
			{
				SendBroadcastHud(GameServer()->FormatExperienceBroadcast(m_aLineExp, m_pPlayer->GetCID()));
			}
		}
	}
	else if (!m_MoneyTile)
	{
		// reset them here, so that they are either null or the formatted line so votingmenu knows when to show this or simple
		m_aLineExp[0] = '\0';
		m_aLineMoney[0] = '\0';
	}

	// set cursor pos when controlling another tee
	if (m_pTeeControlCursor && m_pPlayer->m_pControlledTee)
	{
		CCharacter *pControlledTee = m_pPlayer->m_pControlledTee->GetCharacter();
		if (pControlledTee)
			m_pTeeControlCursor->SetPos(pControlledTee->m_CursorPos); // explicitly use m_CursorPos, as thats the real and normal position
	}

	if (m_IsRainbowHooked && !m_pPlayer->m_InfRainbow && !m_Rainbow && GetPowerHooked() != RAINBOW)
	{
		m_IsRainbowHooked = false;
		m_pPlayer->ResetSkin();
	}

	if (m_StrongBloody)
	{
		GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID(), TeamMask());
	}
	else if (m_Bloody || GetPowerHooked() == BLOODY)
	{
		if (Server()->Tick() % 6 == 0)
			GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID(), TeamMask());
	}

	if (m_Confetti || (m_pPlayer->m_ConfettiWinEffectTick && m_pPlayer->m_ConfettiWinEffectTick > Server()->Tick() - Server()->TickSpeed() * 5))
	{
		if (Server()->Tick() % (Server()->TickSpeed() / 3) == 0)
			GameServer()->CreateFinishConfetti(m_Pos, TeamMask());
	}

	if (m_PassiveEndTick && m_PassiveEndTick < Server()->Tick())
	{
		m_PassiveEndTick = 0;
		Passive(false, -1, true);
	}

	// Decrease no-bonus score so we are back to 0 at some point
	if (m_NoBonusContext.m_Score > 0 && Server()->Tick() % (Server()->TickSpeed() * Config()->m_SvNoBonusScoreDecrease) == 0)
	{
		m_NoBonusContext.m_Score--;
	}

	// No-bonus area bonus using punishment
	if (m_Core.m_JumpedTotal >= Config()->m_SvNoBonusMaxJumps)
	{
		if (m_Core.m_JumpedTotal != m_LastJumpedTotal)
		{
			// add 2 when doing the first illegal air jump
			bool FirstlyExceeded = m_Core.m_JumpedTotal == Config()->m_SvNoBonusMaxJumps;
			IncreaseNoBonusScore(FirstlyExceeded ? 2 : 1);
		}
	}
	m_LastJumpedTotal = m_Core.m_JumpedTotal;

	// endless hook
	if (m_Core.HookDurationExceeded(Server()->TickSpeed() / 5))
	{
		// add 2 when firstly exceeding the hook duration
		bool FirstlyExceeded = false;
		if (!m_HookExceededTick)
		{
			m_HookExceededTick = Server()->Tick();
			FirstlyExceeded = true;
		}

		if ((Server()->Tick() - m_HookExceededTick) % (Server()->TickSpeed() / 2) == 0)
		{
			// Add a score every .5 seconds when duration exceeded, endless is op
			IncreaseNoBonusScore(FirstlyExceeded ? 2 : 1);
		}
	}
	else
	{
		m_HookExceededTick = 0;
	}

	if (m_pPlayer->m_DoSeeOthersByVote && !IsIdle())
	{
		GameWorld()->ResetSeeOthers(m_pPlayer->GetCID());
		m_pPlayer->m_DoSeeOthersByVote = false;
	}

	if (m_BirthdayGiftEndTick && m_BirthdayGiftEndTick <= Server()->Tick())
	{
		SetBirthdayJetpack(false);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("Your jetpack is now disabled"));
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
		case HOUSE_TAVERN: Index = TILE_TAVERN; break;
		}

		if (m_TileIndex != Index && m_TileFIndex != Index)
			GameServer()->m_pHouses[i]->OnLeave(m_pPlayer->GetCID());
	}

	if (m_MoneyTile)
	{
		if (m_TileIndex != TILE_MONEY && m_TileFIndex != TILE_MONEY && m_TileIndex != TILE_MONEY_POLICE && m_TileFIndex != TILE_MONEY_POLICE
			&& m_TileIndex != TILE_MONEY_EXTRA && m_TileFIndex != TILE_MONEY_EXTRA)
		{
			GameServer()->SendBroadcast("", m_pPlayer->GetCID(), false);
			m_MoneyTile = MONEYTILE_NONE;
		}
	}
}

int CCharacter::SendDroppedFlagCooldown(int SnappingClient)
{
	if (SnappingClient != m_pPlayer->GetCID() || (!m_pPlayer->IsPaused() && m_pPlayer->GetTeam() != TEAM_SPECTATORS))
		return -1;

	int Team = -1;
	if (m_pPlayer->GetSpecMode() == SPEC_FLAGRED)
		Team = TEAM_RED;
	else if (m_pPlayer->GetSpecMode() == SPEC_FLAGBLUE)
		Team = TEAM_BLUE;

	if (Team == -1)
		return -1;

	CFlag *pFlag = ((CGameControllerDDRace*)GameServer()->m_pController)->m_apFlags[Team];
	if (pFlag && !pFlag->GetCarrier())
		return Team;
	return -1;
}

int CCharacter::GetPowerHooked()
{
	if (m_PowerHookedID == -1)
		return HOOK_NORMAL;

	CCharacter *pHooker = GameServer()->GetPlayerChar(m_PowerHookedID);
	if (!pHooker || pHooker->Core()->m_HookedPlayer != m_pPlayer->GetCID())
	{
		m_PowerHookedID = -1;
		return HOOK_NORMAL;
	}

	return pHooker->m_HookPower;
}

int CCharacter::GetCurrentTilePlotID(bool CheckDoor)
{
	return GameServer()->GetTilePlotID(m_Pos, CheckDoor);
}

void CCharacter::TeleOutOfPlot(int PlotID)
{
	if (PlotID < PLOT_START)
		return;

	if (m_pPlayer->IsMinigame() && m_pPlayer->m_SavedMinigameTee)
	{
		vec2 SavedPos = m_pPlayer->m_MinigameTee.GetPos();
		int SavedTilePlotID = GameServer()->GetTilePlotID(SavedPos, true);

		if (SavedTilePlotID == PlotID)
			m_pPlayer->m_MinigameTee.TeleOutOfPlot(GameServer()->m_aPlots[PlotID].m_ToTele);
	}

	if (GetCurrentTilePlotID(true) == PlotID)
	{
		ForceSetPos(GameServer()->m_aPlots[PlotID].m_ToTele);
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
			return; // weapon found, return
		}
	}

	if (Config()->m_SvAllowEmptyInventory)
	{
		// DDNet can show no weapon at all now. haha.
		m_ActiveWeapon = -1;
	}
	else
	{
		// no weapon found --> force gun so we dont end up with weird behaviour
		GiveWeapon(WEAPON_GUN);
		SetWeapon(WEAPON_GUN);
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

void CCharacter::IncreasePermille(int Permille)
{
	if (Permille <= 0)
		return;

	if (!m_FirstPermilleTick)
	{
		m_FirstPermilleTick = Server()->Tick();
	}

	m_pPlayer->m_Permille += Permille;
	GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID(), TeamMask());

	if (m_pPlayer->m_Permille > GetPermilleLimit() && !m_pPlayer->m_JailTime)
	{
		// +5 minutes escape time initially, add 2 minutes for each extra drink over
		m_pPlayer->m_EscapeTime += Server()->TickSpeed() * (m_pPlayer->m_EscapeTime ? 120 : 300);
		GameServer()->SendChatPoliceFormat(Localizable("'%s' has exceeded his legal drinking limit. Catch him!"), Server()->ClientName(m_pPlayer->GetCID()));
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("Police is searching you because you have exceeded your legal drinking limit"));
	}

	if (m_pPlayer->m_Permille <= Config()->m_SvGrogMinPermilleLimit)
	{
		// 10 minutes passive, if you dont drink in this time, ur gonna have a ratio of 2/3, cuz 1 drink = passive + 0.3, so 15 min to decrease 0.3, but 10 min passive
		UpdatePassiveEndTick(Server()->Tick() + Server()->TickSpeed() * 60 * 10);
	}
	else
	{
		// Disable when we drank more than allowed
		if (m_PassiveEndTick)
		{
			m_PassiveEndTick = 0;
			Passive(false, -1, true);
		}
	}
}

int CCharacter::GetPermilleLimit()
{
	// 3.9 permille at most. As soon as reaching 4.0 ur wanted. 3.9/0.3 per grog = 13 grogs, if acc is older than 4 months
	float MonthsSinceReg = GameServer()->MonthsPassedSinceRegister(m_pPlayer->GetAccID());
	return clamp((int)(MonthsSinceReg * 10), Config()->m_SvGrogMinPermilleLimit, 39);
}

int CCharacter::DetermineGrogSpirit()
{
	int GrogSpirit = 0;
	if (m_pPlayer->m_Permille >= 5) // 0.5
		GrogSpirit++;
	if (m_pPlayer->m_Permille >= 10) // 1.0
		GrogSpirit++;
	if (m_pPlayer->m_Permille >= 20) // 2.0
		GrogSpirit++;
	if (m_pPlayer->m_Permille >= 30) // 3.0
		GrogSpirit++;
	return GrogSpirit;
}

bool CCharacter::AddGrog()
{
	if (m_NumGrogsHolding >= Config()->m_SvGrogHoldLimit)
	{
		if (m_LastGrogHoldMsg + Server()->TickSpeed() * 3 < Server()->Tick())
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), m_pPlayer->Localize("You can not hold more than %d grogs at once"), Config()->m_SvGrogHoldLimit);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
			m_LastGrogHoldMsg = Server()->Tick();
		}
		return false;
	}

	m_NumGrogsHolding++;
	UpdateWeaponIndicator();
	if (!m_pGrog)
		m_pGrog = new CGrog(GameWorld(), m_Pos, m_pPlayer->GetCID());

	if (Config()->m_SvGrogForceHammer)
	{
		// Force hammer while holding grog
		GiveWeapon(WEAPON_HAMMER);
		SetWeapon(WEAPON_HAMMER);
	}
	else
	{
		// re-implement bug-behaviour which works in Config()->m_SvGrogForceHammer because SetWeapon(WEAPON_HAMMER) does nothing, because spookyghost doesnt have hammer.
		// so this if-catch will literally do the same as the behaviour above. used to get jetpack with grog, for transportation, etc.
		if (!m_pPlayer->m_SpookyGhost)
		{
			// DDNet client allows showing no weapon now.
			SetWeapon(-1);
		}
	}
	return true;
}

void CCharacter::DropMoney(int64 Amount, int Dir, bool GlobalPickupDelay, bool OnDeath)
{
	if (Amount <= 0 || Amount > m_pPlayer->GetUsableMoney() || GameServer()->Durak()->OnDropMoney(m_pPlayer->GetCID(), Amount, OnDeath))
		return;

	if (Dir == -3)
		Dir = GetAimDir();
	else
		Dir = ((rand() % 50 - 25 + 1) * 0.1); // in a range of -2.5 to +2.5
	new CMoney(GameWorld(), m_Pos, Amount, m_pPlayer->GetCID(), Dir, GlobalPickupDelay);
	m_pPlayer->WalletTransaction(-Amount, "dropped");

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "-%lld", Amount);
	GameServer()->CreateLaserText(m_Pos, m_pPlayer->GetCID(), aBuf, GameServer()->MoneyLaserTextTime(Amount));
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, TeamMask());
}

void CCharacter::DropFlag(int Dir)
{
	for (int i = 0; i < 2; i++)
	{
		CFlag *F = ((CGameControllerDDRace*)GameServer()->m_pController)->m_apFlags[i];
		if (F && F->GetCarrier() == this)
			F->Drop(Dir == -3 ? GetAimDir() : Dir);
	}
}

bool CCharacter::CanDropWeapon(int Type)
{
	// Dissallow weapon drop while jetpack 45sec
	if (m_BirthdayGiftEndTick)
		return false;
	// Do not drop spawnweapons
	int W = GetSpawnWeaponIndex(Type);
	if (W != -1 && m_aSpawnWeaponActive[W])
		return false;
	if (Type == -1 || !m_aWeapons[Type].m_Got || Type == WEAPON_DRAW_EDITOR || (Type == WEAPON_NINJA && !m_ScrollNinja) || (Type == WEAPON_PORTAL_RIFLE && !m_CollectedPortalRifle))
		return false;
	return true;
}

void CCharacter::DropWeapon(int WeaponID, bool OnDeath, float Dir)
{
	if (!CanDropWeapon(WeaponID) || Config()->m_SvMaxWeaponDrops == 0 || (!OnDeath && (m_FreezeTime || !Config()->m_SvDropWeapons)) || m_pPlayer->m_Minigame == MINIGAME_1VS1 || m_NumGrogsHolding)
		return;

	if (!Config()->m_SvAllowEmptyInventory)
	{
		int Count = 0;
		for (int i = 0; i < NUM_WEAPONS; i++)
		{
			int W = GetSpawnWeaponIndex(i);
			if (W != -1 && m_aSpawnWeaponActive[W])
				continue;

			if (i != WEAPON_NINJA && (i != WEAPON_PORTAL_RIFLE || m_CollectedPortalRifle) && i != WEAPON_DRAW_EDITOR && m_aWeapons[i].m_Got)
				Count++;
		}
		if (Count < 2)
			return;
	}

	if (m_pPlayer->m_vWeaponLimit[WeaponID].size() == (unsigned)Config()->m_SvMaxWeaponDrops)
		m_pPlayer->m_vWeaponLimit[WeaponID][0]->Reset(false);

	int Special = GetWeaponSpecial(WeaponID);

	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, TeamMask());
	CPickupDrop *pWeapon = new CPickupDrop(GameWorld(), m_Pos, POWERUP_WEAPON, m_pPlayer->GetCID(), Dir == -3 ? GetAimDir() : Dir, 300, WeaponID, GetWeaponAmmo(WeaponID), Special);
	m_pPlayer->m_vWeaponLimit[WeaponID].push_back(pWeapon);

	if (Special == 0)
	{
		GiveWeapon(WeaponID, true);
		SetWeapon(WEAPON_GUN);
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
	GameServer()->CreateSound(m_Pos, Type == POWERUP_HEALTH ? SOUND_PICKUP_HEALTH : SOUND_PICKUP_ARMOR, TeamMask());
}

void CCharacter::DropBattery(int WeaponID, int Amount, bool OnDeath, float Dir)
{
	if (m_LastBatteryDrop > Server()->Tick() - Server()->TickSpeed() || Amount <= 0 || m_pPlayer->m_Minigame == MINIGAME_1VS1)
		return;

	if (WeaponID == WEAPON_TASER)
	{
		if (OnDeath)
			Amount = GetAliveState();
		if (!m_pPlayer->GiveTaserBattery(-Amount))
			return;
	}
	else if (WeaponID == WEAPON_PORTAL_RIFLE)
	{
		if (!m_pPlayer->GivePortalBattery(-Amount))
			return;
	}
	else
		return;

	UpdateWeaponIndicator();

	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, TeamMask());
	new CPickupDrop(GameWorld(), m_Pos, POWERUP_BATTERY, m_pPlayer->GetCID(), Dir == -3 ? GetAimDir() : Dir, 300, WeaponID, Amount);
	m_LastBatteryDrop = Server()->Tick();
}

bool CCharacter::DropGrog(float Dir, bool OnDeath)
{
	return m_NumGrogsHolding && m_pGrog && m_pGrog->Drop(Dir, OnDeath);
}

void CCharacter::DropLoot(int Weapon)
{
	bool ZombieHit = Weapon == WEAPON_ZOMBIE_HIT;
	if (!ZombieHit && (!Config()->m_SvDropsOnDeath || m_pPlayer->m_Minigame == MINIGAME_1VS1))
		return;

	// Drop money even if killed by the game, e.g. team change, but never when leaving a minigame (joining and being frozen drops too)
	if ((Weapon != WEAPON_MINIGAME_CHANGE || m_pPlayer->m_RequestedMinigame != MINIGAME_NONE) && m_FreezeTime)
		DropMoney(m_pPlayer->GetWalletMoney(), -3, ZombieHit, true);

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
			std::vector<int> vExtraWeapons;
			for (int i = NUM_VANILLA_WEAPONS; i < NUM_WEAPONS; i++)
				if (GetWeaponGot(i))
					vExtraWeapons.push_back(i);

			if (vExtraWeapons.size())
			{
				int Index = rand() % vExtraWeapons.size();
				int Weapon = vExtraWeapons[Index];

				float Dir = ((rand() % 50 - 25 + 1) * 0.1); // in a range of -2.5 to +2.5
				DropWeapon(Weapon, true, Dir);
				if (Weapon == WEAPON_TASER)
					DropBattery(WEAPON_TASER, 0, true, Dir);
			}
		}

		// m_NumGrogsHolding will update when grog got dropped
		int NumGrogs = m_NumGrogsHolding;
		for (int i = 0; i < NumGrogs; i++)
		{
			float Dir = ((rand() % 50 - 25 + 1) * 0.1); // in a range of -2.5 to +2.5
			DropGrog(Dir, true);
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
	m_pPlayer->m_ShowName = !m_InSnake;
	m_pPlayer->m_SpookyGhost = false; // set m_SpookyGhost before we reset the skin
	m_pPlayer->ResetSkin();
}

void CCharacter::SetActiveWeapon(int Weapon)
{
	int GrogWeapon = Config()->m_SvGrogForceHammer ? WEAPON_HAMMER : -1;
	if (m_NumGrogsHolding && Weapon != GrogWeapon && Weapon != WEAPON_DRAW_EDITOR)
	{
		if (GrogWeapon == WEAPON_HAMMER && !GetWeaponGot(WEAPON_HAMMER))
			GiveWeapon(WEAPON_HAMMER);
		SetActiveWeapon(GrogWeapon);
		return;
	}

	if (Weapon != -1 && !GetWeaponGot(Weapon))
		return;

	m_ActiveWeapon = Weapon;
	m_IsPortalBlocker = false;
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

int CCharacter::GetTaserStrength()
{
	// If player isnt logged in set his taserlevel to 10, so that he can use taser when given via rcon. he is not able to pick a taser pickup up, so he doesnt have level 10 to abuse
	int AccID = m_pPlayer->GetAccID();
	int TaserLevel = AccID >= ACC_START ? GameServer()->m_Accounts[AccID].m_TaserLevel : Config()->m_SvTaserStrengthDefault;
	return clamp((int)((Server()->Tick() - m_LastTaserUse) * 2 / Server()->TickSpeed()), 0, TaserLevel);
}

bool CCharacter::ShowAmmoHud()
{
	CCharacter *pChr = this;
	if ((m_pPlayer->GetTeam() == TEAM_SPECTATORS || m_pPlayer->IsPaused()) && m_pPlayer->GetSpectatorID() >= 0 && GameServer()->GetPlayerChar(m_pPlayer->GetSpectatorID()))
		pChr = GameServer()->GetPlayerChar(m_pPlayer->GetSpectatorID());
	return pChr->GetWeaponAmmo(pChr->GetActiveWeapon()) != -1 || pChr->GetActiveWeapon() == WEAPON_TASER || pChr->GetActiveWeapon() == WEAPON_PORTAL_RIFLE;
}

int CCharacter::NumDDraceHudRows()
{
	if (!Server()->IsSevendown(m_pPlayer->GetCID()) || GameServer()->GetClientDDNetVersion(m_pPlayer->GetCID()) < VERSION_DDNET_NEW_HUD)
		return 0;

	CCharacter *pChr = this;
	if ((m_pPlayer->GetTeam() == TEAM_SPECTATORS || m_pPlayer->IsPaused()) && m_pPlayer->GetSpectatorID() >= 0 && GameServer()->GetPlayerChar(m_pPlayer->GetSpectatorID()))
		pChr = GameServer()->GetPlayerChar(m_pPlayer->GetSpectatorID());

	int Flags = pChr->GetDDNetCharacterFlags(pChr->GetPlayer()->GetCID());
	int Rows = 0;
	if (ShowAmmoHud() && pChr->GetPlayer()->ShowDDraceHud()) // when we dont show ddrace hud we have either nothing or health/armor. then we dont need to add a row
		Rows++;
	if (Flags&(CHARACTERFLAG_ENDLESS_JUMP|CHARACTERFLAG_ENDLESS_HOOK|CHARACTERFLAG_JETPACK|CHARACTERFLAG_TELEGUN_GUN|CHARACTERFLAG_TELEGUN_GRENADE|CHARACTERFLAG_TELEGUN_LASER))
		Rows++;
	if (Flags&(CHARACTERFLAG_SOLO|CHARACTERFLAG_NO_COLLISION|CHARACTERFLAG_NO_HOOK|CHARACTERFLAG_NO_HAMMER_HIT|CHARACTERFLAG_NO_SHOTGUN_HIT|CHARACTERFLAG_NO_GRENADE_HIT|CHARACTERFLAG_NO_LASER_HIT))
		Rows++;
	if (Flags&(CHARACTERFLAG_PRACTICE_MODE|CHARACTERFLAG_LOCK_MODE) || pChr->m_DeepFreeze/* || pChr->Core()->m_LiveFreeze*/)
		Rows++;
	return Rows;
}

void CCharacter::SendBroadcastHud(const char *pMessage)
{
	char aBuf[256] = "";
	for (int i = 0; i < NumDDraceHudRows(); i++)
		str_append(aBuf, "\n", sizeof(aBuf));

	str_append(aBuf, pMessage, sizeof(aBuf));

	if (Server()->IsSevendown(m_pPlayer->GetCID()))
		for (int i = 0; i < 128; i++)
			str_append(aBuf, " ", sizeof(aBuf));

	GameServer()->SendBroadcast(aBuf, m_pPlayer->GetCID(), false);
}

bool CCharacter::IsWeaponIndicator()
{
	// 2 seconds of showing weapon indicator instead of money broadcast
	return m_LastWeaponIndTick > Server()->Tick() - Server()->TickSpeed() * 2;
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
	if (!m_pPlayer->m_WeaponIndicator
		|| (m_pPlayer->m_Minigame == MINIGAME_SURVIVAL && GameServer()->m_SurvivalBackgroundState < BACKGROUND_DEATHMATCH_COUNTDOWN))
		return;
	for (int i = 0; i < NUM_HOUSES; i++)
		if (GameServer()->m_pHouses[i]->IsInside(m_pPlayer->GetCID()))
			return;

	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[m_pPlayer->GetAccID()];

	char aAmmo[32] = "";
	const char *pName = GameServer()->GetWeaponName(GetActiveWeapon());
	if (GetActiveWeapon() == WEAPON_TASER && pAccount->m_TaserLevel)
	{
		str_format(aAmmo, sizeof(aAmmo), " [%d]", pAccount->m_TaserBattery);
	}
	else if (GetActiveWeapon() == WEAPON_PORTAL_RIFLE)
	{
		if (!pAccount->m_PortalRifle && Config()->m_SvPortalRifleAmmo)
			str_format(aAmmo, sizeof(aAmmo), " [%d]", pAccount->m_PortalBattery);
		else if (pAccount->m_PortalRifle && !m_pPlayer->m_aSecurityPin[0])
			str_copy(aAmmo, " [NOT VERIFIED → '/pin']", sizeof(aAmmo));
	}
	else if (m_IsPortalBlocker)
	{
		pName = "Portal Blocker";
		str_format(aAmmo, sizeof(aAmmo), " [%d]", pAccount->m_PortalBlocker);
	}
	else if (m_NumGrogsHolding)
	{
		pName = "Grog";
		str_format(aAmmo, sizeof(aAmmo), " [%d]", m_NumGrogsHolding);
	}

	char aBuf[256] = "";
	if (Server()->IsSevendown(m_pPlayer->GetCID()))
	{
		if (GameServer()->GetClientDDNetVersion(m_pPlayer->GetCID()) < VERSION_DDNET_NEW_HUD)
		{
			str_format(aBuf, sizeof(aBuf), "%s: %s%s", m_pPlayer->Localize("Weapon"), pName, aAmmo);
		}
		else
		{
			if (GetActiveWeapon() >= NUM_VANILLA_WEAPONS || m_IsPortalBlocker || m_NumGrogsHolding)
				str_format(aBuf, sizeof(aBuf), "> %s%s", pName, aAmmo);
		}
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "> %s%s <", pName, aAmmo);
	}

	// dont update, when we change between vanilla weapons, so that no "" is being sent to remove another broadcast, for example a money broadcast
	if (aBuf[0] || IsWeaponIndicator())
		SendBroadcastHud(aBuf);

	// dont update when vanilla weapon got triggered and we have new hud
	if (aBuf[0])
		m_LastWeaponIndTick = Server()->Tick();
}

void CCharacter::ResetOnlyFirstPortal()
{
	if (m_pPlayer->m_pPortal[PORTAL_FIRST] && !m_pPlayer->m_pPortal[PORTAL_SECOND])
		m_pPlayer->m_pPortal[PORTAL_FIRST]->Reset();
}

int CCharacter::HasFlag()
{
	return ((CGameControllerDDRace*)GameServer()->m_pController)->HasFlag(this);
}

bool CCharacter::SendExtendedEntity(CEntity *pEntity)
{
	return GameServer()->GetClientDDNetVersion(m_pPlayer->GetCID()) >= VERSION_DDNET_SWITCH
		&& !m_pPlayer->IsPaused() && m_pPlayer->GetTeam() != TEAM_SPECTATORS && !pEntity->NetworkClipped(m_pPlayer->GetCID(), false, true)
		&& (pEntity->m_PlotID < PLOT_START || pEntity->m_PlotID == GetCurrentTilePlotID() || pEntity->IsPlotDoor());
}

void CCharacter::CheckMoved()
{
	if (!m_pPlayer->m_ResumeMoved || !m_pPlayer->IsPaused() || m_PrevPos == m_Pos)
		return;

	m_pPlayer->Pause(CPlayer::PAUSE_NONE, false);
}

void CCharacter::ForceSetPos(vec2 Pos)
{
	int CurrentPlotID = GetCurrentTilePlotID(true);
	if (CurrentPlotID >= PLOT_START && CurrentPlotID != GameServer()->GetTilePlotID(Pos))
		m_pPlayer->StopPlotEditing();

	m_Core.m_Pos = m_Pos = m_PrevPos = Pos;

	int Flag = HasFlag();
	if (Flag != -1)
	{
		// this is so that when we get teleported we also force update the prevpos so the flag doesnt find weird things between its new pos and the last prevpos before the tp
		((CGameControllerDDRace *)GameServer()->m_pController)->m_apFlags[Flag]->SetPos(Pos);
		((CGameControllerDDRace *)GameServer()->m_pController)->m_apFlags[Flag]->SetPrevPos(Pos);
	}
}

void CCharacter::IncreaseNoBonusScore(int Summand)
{
	if (!m_NoBonusContext.m_InArea || Config()->m_SvNoBonusScoreTreshold == 0 || m_pPlayer->m_IsDummy)
		return;

	m_NoBonusContext.m_Score += Summand;

	bool Wanted = m_NoBonusContext.m_Score >= Config()->m_SvNoBonusScoreTreshold;
	bool ForceSendMessage = Wanted && !m_pPlayer->m_EscapeTime;
	if (Wanted)
	{
		// +2 minutes escape time initially, add 30 seconds for each extra score
		m_pPlayer->m_EscapeTime += Server()->TickSpeed() * (m_pPlayer->m_EscapeTime ? 30 : 120);
	}

	// treshold to span: [1] = warn when we got fucked up already, [2,4] = warn one before reaching treshold, [5...] = warn on every 4th
	int MessageSpan = min(max(Config()->m_SvNoBonusScoreTreshold-1, 1), 4);

	// When we get our initial escape time we always want to send a message
	if (ForceSendMessage || m_NoBonusContext.m_Score % MessageSpan == 0)
	{
		if (Wanted)
		{
			if (!m_NoBonusContext.m_LastAlertTick || m_NoBonusContext.m_LastAlertTick + Server()->TickSpeed() * 20 < Server()->Tick())
			{
				GameServer()->SendChatPoliceFormat(Localizable("'%s' is using bonus illegally. Catch him!"), Server()->ClientName(m_pPlayer->GetCID()));
				m_NoBonusContext.m_LastAlertTick = Server()->Tick();
			}
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("Police is searching you because of illegal bonus use"));
		}
		else
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("[WARNING] Using bonus in no-bonus area is illegal"));
		}
	}
}

bool CCharacter::OnNoBonusArea(bool Enter, bool Silent)
{
	// We check whether it got set this tick already, because that can happen when someone tries to skip the tile using ninja.
	if ((Enter && m_NoBonusContext.m_InArea) || (!Enter && !m_NoBonusContext.m_InArea) || m_LastNoBonusTick == Server()->Tick())
		return false;

	m_NoBonusContext.m_InArea = !m_NoBonusContext.m_InArea;
	m_NoBonusContext.m_SavedBonus.m_NoBonusMaxJumps = Config()->m_SvNoBonusMaxJumps;
	m_LastNoBonusTick = Server()->Tick();

	// If treshold is set, we don't disable bonus and we keep track of the treshold to see when we want to be searched by the police, more fun
	if (Config()->m_SvNoBonusScoreTreshold > 0)
		return true;

	// Save or load previous bonuses
	if (Enter)
	{
		m_NoBonusContext.m_SavedBonus.m_EndlessHook = m_EndlessHook;
		m_NoBonusContext.m_SavedBonus.m_InfiniteJumps = m_SuperJump;
		m_NoBonusContext.m_SavedBonus.m_Jumps = m_Core.m_Jumps;
		EndlessHook(false, -1, Silent);
		InfiniteJumps(false, -1, Silent);
		SetJumps(min(m_Core.m_Jumps, Config()->m_SvNoBonusMaxJumps), Silent);
	}
	else
	{
		EndlessHook(m_NoBonusContext.m_SavedBonus.m_EndlessHook, -1, Silent);
		InfiniteJumps(m_NoBonusContext.m_SavedBonus.m_InfiniteJumps, -1, Silent);
		SetJumps(m_NoBonusContext.m_SavedBonus.m_Jumps, Silent);
		m_NoBonusContext.m_SavedBonus.m_EndlessHook = false;
		m_NoBonusContext.m_SavedBonus.m_InfiniteJumps = false;
		m_NoBonusContext.m_SavedBonus.m_Jumps = 0;
	}

	return true;
}

bool CCharacter::TrySafelyRedirectClient(int Port, bool Force)
{
	if (Port == 0 || Port == Config()->m_SvPort)
		return false;

	int DummyID = Server()->GetDummy(m_pPlayer->GetCID());
	CPlayer *pDummy = DummyID != -1 ? GameServer()->m_apPlayers[DummyID] : 0;
	if (!Force && pDummy && (!pDummy->m_LastRedirectTryTick || pDummy->m_LastRedirectTryTick + Server()->TickSpeed() * 30 < Server()->Tick()))
	{
		m_pPlayer->m_LastRedirectTryTick = Server()->Tick();
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("[WARNING] You need to enter this teleporter with your dummy within 30 seconds in order to get moved safely"));
		return false;
	}

	CCharacter *pDummyChar = GameServer()->GetPlayerChar(DummyID);
	if (!Force && pDummy && !pDummyChar)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("[WARNING] Your dummy has to be alive in order to get moved safely"));
		return false;
	}

	if (TrySafelyRedirectClientImpl(Port))
	{
		// Check if we're alive in case we got kicked and killed by unsupported redirect
		if (m_Alive)
		{
			m_pPlayer->KillCharacter(WEAPON_GAME);
		}
		// Forcefully move the dummy asell
		if (pDummyChar && pDummyChar->TrySafelyRedirectClientImpl(Port) && pDummyChar->m_Alive)
		{
			pDummy->KillCharacter(WEAPON_GAME);
		}
		return true;
	}
	return false;
}

bool CCharacter::TrySafelyRedirectClientImpl(int Port)
{
	if (Port == 0 || Port == Config()->m_SvPort)
		return false;

	// We need the port here so it gets saved aswell. If saving didn't work, we reset it
	m_RedirectTilePort = Port;
	int IdentityIndex = GameServer()->SaveCharacter(m_pPlayer->GetCID(), SAVE_REDIRECT|SAVE_WALLET, Config()->m_SvShutdownSaveTeeExpire);
	if (IdentityIndex != -1)
	{
		// Send msg
		GameServer()->SendChatFormat(-1, CHAT_ALL, -1, CGameContext::CHATFLAG_ALL, Localizable("'%s' has been moved to another map"), Server()->ClientName(m_pPlayer->GetCID()));
		Server()->SendRedirectSaveTeeAdd(m_RedirectTilePort, GameServer()->GetSavedIdentityHash(GameServer()->m_vSavedIdentities[IdentityIndex]));
		Server()->RedirectClient(m_pPlayer->GetCID(), m_RedirectTilePort);
		return true;
	}
	m_RedirectTilePort = 0;
	return false;
}

bool CCharacter::LoadRedirectTile(int Port)
{
	if (!Port)
		return false;

	vec2 Pos;
	const char *pList = Config()->m_SvRedirectServerTilePorts;
	while (1)
	{
		if (!pList || !pList[0])
			break;

		int EntryNumber = 0;
		int EntryPort = 0;
		sscanf(pList, "%d:%d", &EntryNumber, &EntryPort);
		if (EntryNumber > 0 && EntryPort == Port)
		{
			Pos = GameServer()->Collision()->GetRandomRedirectTile(EntryNumber);
			if (Pos != vec2(-1, -1))
			{
				ForceSetPos(Pos);

				int64 NewEndTick = Server()->Tick() + Server()->TickSpeed() * 3;
				UpdatePassiveEndTick(NewEndTick);
				return true;
			}
		}

		// jump to next comma, if it exists skip it so we can start the next loop run with the next data
		if ((pList = str_find(pList, ",")))
			pList++;
	}

	// nothing found, send to spawn tile
	if (GameServer()->m_pController->CanSpawn(&Pos, ENTITY_SPAWN, Team()))
		ForceSetPos(Pos);
	return false;
}

bool CCharacter::UpdatePassiveEndTick(int64 NewEndTick)
{
	if (m_Passive && (!m_PassiveEndTick || m_PassiveEndTick >= NewEndTick))
		return false;

	m_PassiveEndTick = NewEndTick;
	Passive(true, -1, true);
	return true;
}

void CCharacter::AddCheckpointList(int Port, int Checkpoint)
{
	for (unsigned int i = 0; i < m_vCheckpoints.size(); i++)
	{
		int PortMatch = GameServer()->GetRediretListPort(m_vCheckpoints[i].first);
		if (PortMatch == Port)
		{
			m_vCheckpoints[i].second = Checkpoint;
			return;
		}
	}

	int SwitchNumber = GameServer()->GetRediretListSwitch(Port);
	if (SwitchNumber > 0)
	{
		std::pair<int, int> Pair;
		Pair.first = SwitchNumber;
		Pair.second = Checkpoint;
		m_vCheckpoints.push_back(Pair);
	}
}

void CCharacter::SetCheckpointList(std::vector< std::pair<int, int> > vCheckpoints)
{
	// don't reset checkpoint if we have nothing to use anyways.
	if (!vCheckpoints.size())
		return;

	m_vCheckpoints = vCheckpoints;
	m_TeleCheckpoint = 0;

	for (unsigned int i = 0; i < m_vCheckpoints.size(); i++)
	{
		int PortMatch = GameServer()->GetRediretListPort(m_vCheckpoints[i].first);
		if (PortMatch == Config()->m_SvPort)
		{
			m_TeleCheckpoint = m_vCheckpoints[i].second;
			break;
		}
	}
}

bool CCharacter::TryMountHelicopter()
{
	if (m_FreezeTime)
		return false;

	CHelicopter *pHelicopter = (CHelicopter *)GameWorld()->ClosestEntity(m_Pos, 300.f, CGameWorld::ENTTYPE_HELICOPTER, 0, true, Team());
	return pHelicopter && pHelicopter->Mount(m_pPlayer->GetCID());
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

bool CCharacter::GrogTick()
{
	int64 Now = Server()->Tick();
	if (m_FirstPermilleTick)
	{
		// Decrease by 0.1 permille every 5 minutes
		int64 StartTickDiff = Now - m_FirstPermilleTick;
		if (StartTickDiff % (Server()->TickSpeed() * 60 * 5) == 0)
		{
			m_pPlayer->m_Permille--;
			if (m_pPlayer->m_Permille <= 0)
			{
				m_FirstPermilleTick = 0;
				m_NextGrogEmote = 0;
			}
		}
		else
		{
			// Random grog actions
			if (!m_NextGrogEmote || m_NextGrogEmote - Now < 0)
			{
				int Random = random(0, 2);
				GameServer()->SendEmoticon(m_pPlayer->GetCID(), Random == 0 ? EMOTICON_HEARTS : Random == 1 ? EMOTICON_EYES : EMOTICON_MUSIC);
				SetEmote(EMOTE_HAPPY, Now + Server()->TickSpeed() * 2);
				m_NextGrogEmote = Now + Server()->TickSpeed() * random(5, 60);
			}

			// Balance impaired
			if (m_pPlayer->m_Permille >= 8) // 0.8
			{
				if (!m_NextGrogBalance)
				{
					m_NextGrogBalance = GetNextGrogActionTick();
				}

				if (m_NextGrogBalance - Now < 0)
				{
					// 3/4 second
					if (Now - m_NextGrogBalance > Server()->TickSpeed() / 4*3)
					{
						m_NextGrogBalance = 0;
						if (m_GrogBalancePosX != GROG_BALANCE_POS_UNSET)
						{
							// Reset, so GrogDirDelay below is not triggered
							m_SavedInput.m_Direction = m_PrevInput.m_Direction = m_Input.m_Direction = 0;
						}
						m_GrogBalancePosX = GROG_BALANCE_POS_UNSET;
					}
					else
					{
						if (m_GrogBalancePosX == GROG_BALANCE_POS_UNSET && m_Input.m_Direction == 0 && m_IsGrounded)
						{
							m_GrogBalancePosX = m_Pos.x;
							int Emoticon = EMOTICON_SPLATTEE + random(0, 2); // one of the three angry emotes
							GameServer()->SendEmoticon(m_pPlayer->GetCID(), Emoticon);
							SetEmote(EMOTE_ANGRY, Now + Server()->TickSpeed() * 2);
						}

						int Dir = m_Pos.x > m_GrogBalancePosX+1 ? -1 : m_Pos.x < m_GrogBalancePosX-1 ? 1 : 0;
						if (Dir != m_GrogDirection && Dir != 0)
							m_GrogDirection = Dir;
					}
				}
			}
			else
			{
				// Reset so we don't take weird values and keep an input or smth
				m_NextGrogBalance = 0;
				if (m_GrogBalancePosX != GROG_BALANCE_POS_UNSET)
				{
					// Reset, so GrogDirDelay below is not triggered
					m_SavedInput.m_Direction = m_PrevInput.m_Direction = m_Input.m_Direction = 0;
				}
				m_GrogBalancePosX = GROG_BALANCE_POS_UNSET;
			}

			// Input delayed
			if (m_pPlayer->m_Permille >= 15) // 1.5
			{
				if (!m_NextGrogDirDelay)
				{
					m_NextGrogDirDelay = GetNextGrogActionTick();
				}

				if (m_NextGrogDirDelay - Now < 0)
				{
					if (m_GrogDirDelayEnd && m_GrogDirDelayEnd < Server()->Tick())
					{
						m_NextGrogDirDelay = 0;
						m_GrogDirDelayEnd = 0;
					}
					else if (m_SavedInput.m_Direction != m_PrevInput.m_Direction)
					{
						// Don't set yet when we're stil balancing
						if (!m_GrogDirDelayEnd && m_GrogBalancePosX == GROG_BALANCE_POS_UNSET)
						{
							int Emoticon = EMOTICON_SPLATTEE + random(0, 2); // one of the three angry emotes
							GameServer()->SendEmoticon(m_pPlayer->GetCID(), Emoticon);
							SetEmote(EMOTE_ANGRY, Now + Server()->TickSpeed() * 2);
							// 1/3 second delayed
							m_GrogDirDelayEnd = Now + Server()->TickSpeed() / 3;
							// keep previous input for now
							m_SavedInput.m_Direction = m_PrevInput.m_Direction;
						}
					}
				}
			}
			else
			{
				// Reset so we don't take weird values and keep an input or smth
				m_NextGrogDirDelay = 0;
				m_GrogDirDelayEnd = 0;
			}
		}
	}

	// Grog spirit xp boost for wise men
	int GrogSpirit = DetermineGrogSpirit();
	// little confetti when we reached a higher grogspirit lvl :)
	if (GrogSpirit > m_GrogSpirit)
		GameServer()->CreateFinishConfetti(m_Pos, TeamMask());
	m_GrogSpirit = GrogSpirit;

	// After 3 seconds of freeze, disable passive
	if (m_pPlayer->m_Permille && m_PassiveEndTick && m_FirstFreezeTick && m_FirstFreezeTick + Server()->TickSpeed() * 3 < Server()->Tick())
	{
		m_PassiveEndTick = 0;
		Passive(false, -1, true);
	}

	// Very deadly alcohol!!! stupid alcohol!! dont drink, smoke weed instead 420
	if (m_pPlayer->m_Permille >= 35) // 3.5
	{
		// long term alcoholics can handle it better
		// let's add 0.5 permille on top, so 4.4permille is the ABSOLUTE maximum without dying, depending on register date
		// for other people, 3.5 will be deadly limit
		int DeadlyLimit = max(GetPermilleLimit() + 5, 35);
		if (m_pPlayer->m_Permille > DeadlyLimit)
		{
			if (!m_DeadlyPermilleDieTick)
			{
				// Random timer between 3sec and 10min
				m_DeadlyPermilleDieTick = Now + Server()->TickSpeed() * random(3, 600);
			}

			// 5 minutes above the deadly limit will kill you
			if (m_DeadlyPermilleDieTick < Server()->Tick())
			{
				m_DeadlyPermilleDieTick = 0;
				// Reset escape time when a player died from grog, no matter his crimes
				m_pPlayer->m_EscapeTime = 0;
				Die(WEAPON_SELF);
				GameServer()->SendChatFormat(-1, CHAT_ALL, -1, CGameContext::CHATFLAG_ALL, Localizable("'%s' died as a result of excessive grog consumption (%.1f‰ / %.1f‰)"),
					Server()->ClientName(m_pPlayer->GetCID()), m_pPlayer->m_Permille / 10.f, GetPermilleLimit() / 10.f);
				return true;
			}
		}
		else
		{
			// If we're under this limit again, we're fine.
			m_DeadlyPermilleDieTick = 0;
		}
	}
	return false;
}

int64 CCharacter::GetNextGrogActionTick()
{
	int Seconds = random(20, 60);
	float Multiplier = random(5, 10) / 10.f;
	int DecreaseSeconds = min((int)(m_pPlayer->m_Permille * 3 * Multiplier), Seconds - 1);
	return Server()->Tick() + Server()->TickSpeed() * (Seconds - DecreaseSeconds);
}

int CCharacter::GetCorruptionScore()
{
	int Score = m_pPlayer->m_SpawnBlockScore + m_NoBonusContext.m_Score;
	// add 1 score per 0.2 permille
	int PermilleLimit = GetPermilleLimit();
	if (m_pPlayer->m_Permille > PermilleLimit)
	{
		int Diff = m_pPlayer->m_Permille - PermilleLimit;
		Score += Diff / 2;
	}
	return Score;
}

bool CCharacter::TryCatchingWanted(int TargetCID, vec2 EffectPos)
{
	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[m_pPlayer->GetAccID()];
	CCharacter *pTarget = GameServer()->GetPlayerChar(TargetCID);
	// police catch gangster // disallow catching while being wanted yourself
	if (!pTarget->GetPlayer()->m_EscapeTime || pTarget->GetPlayer()->IsMinigame() || !pTarget->m_FreezeTime || !pAccount->m_PoliceLevel || m_pPlayer->m_EscapeTime)
		return false;

	char aBuf[256];
	int Minutes = clamp((int)(pTarget->GetPlayer()->m_EscapeTime / Server()->TickSpeed()) / 100, 10, 20);
	int Corrupt = clamp(pTarget->GetCorruptionScore() * 500, 500, 10000);

	if (pTarget->GetPlayer()->GetAccID() >= ACC_START && GameServer()->m_Accounts[pTarget->GetPlayer()->GetAccID()].m_Money >= Corrupt)
	{
		str_format(aBuf, sizeof(aBuf), "corrupted officer '%s'", Server()->ClientName(m_pPlayer->GetCID()));
		pTarget->GetPlayer()->BankTransaction(-Corrupt, aBuf);
		str_format(aBuf, sizeof(aBuf), "corrupted by gangster '%s'", Server()->ClientName(TargetCID));
		m_pPlayer->BankTransaction(Corrupt, aBuf);

		str_format(aBuf, sizeof(aBuf), pTarget->GetPlayer()->Localize("You paid %d money to '%s' to reduce your jailtime by 5 minutes"), Corrupt, Server()->ClientName(m_pPlayer->GetCID()));
		GameServer()->SendChatTarget(TargetCID, aBuf);

		str_format(aBuf, sizeof(aBuf), m_pPlayer->Localize("You got %d money from '%s' to reduce his jailtime by 5 minutes"), Corrupt, Server()->ClientName(TargetCID));
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		Minutes -= 5;
	}

	GameServer()->SendChatPoliceFormat(Localizable("'%s' has been caught by '%s' (%d minutes arrest)"), Server()->ClientName(TargetCID), Server()->ClientName(m_pPlayer->GetCID()), Minutes);
	if (pTarget->m_pPlayer->m_Permille)
	{
		GameServer()->SendChatPoliceFormat(Localizable("Grog testing results: %.1f‰ / %.1f‰"), pTarget->m_pPlayer->m_Permille / 10.f, pTarget->GetPermilleLimit() / 10.f);
	}

	str_format(aBuf, sizeof(aBuf), pTarget->GetPlayer()->Localize("You were arrested for %d minutes by '%s'"), Minutes, Server()->ClientName(m_pPlayer->GetCID()));
	GameServer()->SendChatTarget(TargetCID, aBuf);
	GameServer()->CreateFinishConfetti(EffectPos, TeamMask());
	GameServer()->JailPlayer(TargetCID, Minutes * 60); // minimum 5 maximum 20 minutes jail
	return true;
}

bool CCharacter::SetZombieHuman(bool Zombie, bool GiveGun)
{
	if (m_IsZombie == Zombie)
		return false;

	m_IsZombie = Zombie;
	if (Zombie)
	{
		UnsetSpookyGhost();
		for (int i = WEAPON_GUN; i < NUM_WEAPONS; i++)
			GiveWeapon(i, true);
		Jetpack(false, -1, true);
		SetWeapon(WEAPON_HAMMER);

		m_pPlayer->SaveDefEmote();
		m_pPlayer->m_DefEmote = EMOTE_ANGRY;
		m_pPlayer->m_DefEmoteReset = -1;

		CTeeInfo Info("cammo", 1, CGameControllerDDRace::ZombieBodyValue, CGameControllerDDRace::ZombieFeetValue);
		Info.Translate(true);
		m_pPlayer->m_CurrentInfo.m_TeeInfos = Info;
		GameServer()->SendSkinChange(Info, m_pPlayer->GetCID(), -1);
		GameServer()->CreatePlayerSpawn(m_Pos, TeamMask());

		CNetMsg_Sv_KillMsg Msg;
		Msg.m_Killer = m_pPlayer->GetCID();
		Msg.m_Victim = m_pPlayer->GetCID();
		Msg.m_Weapon = WEAPON_WORLD;
		Msg.m_ModeSpecial = HasFlag() != -1 ? 1 : 0;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

		m_pPlayer->SetClan("Zombie");
	}
	else
	{
		m_pPlayer->m_DisableCustomColorsTick = 0;
		m_pPlayer->ResetSkin();
		m_pPlayer->LoadDefEmote();
		m_pPlayer->m_DefEmoteReset = -1;
		m_pPlayer->SetClan(Server()->ClientClan(m_pPlayer->GetCID()));
		if (GiveGun)
		{
			GiveWeapon(WEAPON_GUN);
		}
	}
	return true;
}

bool CCharacter::TryHumanTransformation(CCharacter *pTarget)
{
	if (!m_IsZombie || !pTarget || !pTarget->m_Alive || pTarget->m_IsZombie)
		return false;

	SetZombieHuman(false, false);
	for (int i = 0; i < NUM_WEAPONS; i++)
	{
		if (pTarget->CanDropWeapon(i))
		{
			GiveWeapon(i);

			int Special = pTarget->GetWeaponSpecial(i);
			if (Special & SPECIAL_JETPACK)
			{
				Jetpack(true, -1, true);
				pTarget->Jetpack(false, -1, true);
			}
			if (Special & SPECIAL_SPREADWEAPON)
			{
				SpreadWeapon(i, true, -1, true);
				pTarget->SpreadWeapon(i, false, -1, true);
			}
			if (Special & SPECIAL_TELEWEAPON)
			{
				TeleWeapon(i, true, -1, true);
				pTarget->TeleWeapon(i, false, -1, true);
			}
			if (Special & SPECIAL_DOORHAMMER)
			{
				DoorHammer(true, -1, true);
				pTarget->DoorHammer(false, -1, true);
			}
			if (Special & SPECIAL_SCROLLNINJA)
			{
				ScrollNinja(true, -1, true);
				pTarget->ScrollNinja(false, -1, true);
			}
		}
	}

	// Transfer hammer the other way around, since players can have no weapons now. means:
	// if you as a zombie hammer a human, you get human + their weapons, but you give your weapons (hammer) to them and they turn zombie. if both have hammer, np.
	if (!pTarget->GetWeaponGot(WEAPON_HAMMER))
	{
		pTarget->GiveWeapon(WEAPON_HAMMER);
		GiveWeapon(WEAPON_HAMMER, true);
	}

	if (!m_EndlessHook && pTarget->m_EndlessHook)
	{
		m_EndlessHook = pTarget->m_EndlessHook;
		pTarget->m_EndlessHook = false;
	}
	if (!m_SuperJump && pTarget->m_SuperJump)
	{
		m_SuperJump = pTarget->m_SuperJump;
		pTarget->m_SuperJump = false;
	}

	// transform other guy to zombie
	pTarget->SetZombieHuman(true);
	// Freeze before droploot, so that wallet get's dropped :)
	pTarget->Freeze(3);
	pTarget->DropLoot(WEAPON_ZOMBIE_HIT);
	pTarget->DropFlag();
	return true;
}

void CCharacter::SetBirthdayJetpack(bool Set)
{
	if (Set)
	{
		SetWeapon(WEAPON_GUN);
		m_BirthdayGiftEndTick = Server()->Tick() + Server()->TickSpeed() * CPlayer::BIRTHDAY_JETPACK_GIFT_TIME;
	}
	else
	{
		m_BirthdayGiftEndTick = 0;
	}

	m_Jetpack = Set;
	GameServer()->m_pController->UpdateGameInfo(m_pPlayer->GetCID());
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

	if (m_HookPower != HOOK_NORMAL)
		pHookedTee->m_PowerHookedID = m_pPlayer->GetCID();

	// set hook extra stuff
	if (m_HookPower == ATOM && !pHookedTee->m_Atom)
		new CAtom(GameWorld(), pHookedTee->m_Pos, m_Core.m_HookedPlayer);
	if (m_HookPower == TRAIL && !pHookedTee->m_Trail)
		new CTrail(GameWorld(), pHookedTee->m_Pos, m_Core.m_HookedPlayer);
}

void CCharacter::ReleaseHook(bool Other)
{
	m_Core.SetHookedPlayer(-1);
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
	if (m_IsZombie)
		return;
	SetBirthdayJetpack(false);
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
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("For more info, say '/helptoggle'"));
}

void CCharacter::Meteor(bool Set, int FromID, bool Infinite, bool Silent)
{
	if (Set)
	{
		if (m_pPlayer->m_InfMeteors + m_Meteors >= 50)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("You already have the maximum of 50 meteors"));
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
	if (m_Passive == Set)
		return;
	m_Passive = Set;
	Teams()->m_Core.SetPassive(m_pPlayer->GetCID(), Set);
	GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);
	if (m_Passive)
		DropFlag(0);
	GameServer()->SendExtraMessage(PASSIVE, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::VanillaMode(int FromID, bool Silent)
{
	if (m_pPlayer->m_Gamemode == GAMEMODE_VANILLA)
		return;

	// set a new saved gamemode, forced by a command means this is the new default gamemode for this player
	if (FromID >= 0)
		m_pPlayer->m_SavedGamemode = GAMEMODE_VANILLA;

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
		m_pPlayer->m_SavedGamemode = GAMEMODE_DDRACE;

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
	if (m_EndlessHook == Set)
		return;
	m_EndlessHook = Set;
	GameServer()->SendExtraMessage(ENDLESS_HOOK, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::InfiniteJumps(bool Set, int FromID, bool Silent)
{
	if (m_SuperJump == Set)
		return;
	m_SuperJump = Set;
	GameServer()->SendExtraMessage(INFINITE_JUMPS, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::SpreadWeapon(int Type, bool Set, int FromID, bool Silent)
{
	if (Type == WEAPON_HAMMER || Type == WEAPON_NINJA || Type == WEAPON_TELEKINESIS || Type == WEAPON_LIGHTSABER || Type == WEAPON_PORTAL_RIFLE
		|| Type == WEAPON_DRAW_EDITOR || Type == WEAPON_TELE_RIFLE || Type == WEAPON_LIGHTNING_LASER)
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

void CCharacter::TeeControl(bool Set, int ForcedID, int FromID, bool Silent)
{
	m_pPlayer->m_HasTeeControl = Set;
	m_pPlayer->m_TeeControlForcedID = ForcedID;
	if (!Set)
		m_pPlayer->ResumeFromTeeControl();
	GameServer()->SendExtraMessage(TEE_CONTROL, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::Snake(bool Set, int FromID, bool Silent)
{
	if (m_Snake.SetActive(Set))
		GameServer()->SendExtraMessage(SNAKE, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::Lovely(bool Set, int FromID, bool Silent)
{
	m_Lovely = Set;
	if (m_Lovely)
		new CLovely(GameWorld(), m_Pos, m_pPlayer->GetCID());
	GameServer()->SendExtraMessage(LOVELY, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::RotatingBall(bool Set, int FromID, bool Silent)
{
	m_RotatingBall = Set;
	if (m_RotatingBall)
		new CRotatingBall(GameWorld(), m_Pos, m_pPlayer->GetCID());
	GameServer()->SendExtraMessage(ROTATING_BALL, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::EpicCircle(bool Set, int FromID, bool Silent)
{
	if (m_EpicCircle == Set)
		return;
	m_EpicCircle = Set;
	if (m_EpicCircle)
		new CEpicCircle(GameWorld(), m_Pos, m_pPlayer->GetCID());
	GameServer()->SendExtraMessage(EPIC_CIRCLE, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::StaffInd(bool Set, int FromID, bool Silent)
{
	m_StaffInd = Set;
	if (m_StaffInd)
		new CStaffInd(GameWorld(), m_Pos, m_pPlayer->GetCID());
	GameServer()->SendExtraMessage(STAFF_IND, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::RainbowName(bool Set, int FromID, bool Silent)
{
	m_pPlayer->m_RainbowName = Set;
	GameServer()->SendExtraMessage(RAINBOW_NAME, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::Confetti(bool Set, int FromID, bool Silent)
{
	m_Confetti = Set;
	GameServer()->SendExtraMessage(CONFETTI, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::Sparkle(bool Set, int FromID, bool Silent)
{
	m_pPlayer->m_Sparkle = Set;
	GameServer()->SendExtraMessage(SPARKLE, m_pPlayer->GetCID(), Set, FromID, Silent);
}

void CCharacter::SetJumps(int NewJumps, bool Silent)
{
	if (NewJumps == m_Core.m_Jumps)
		return;

	if (!Silent)
	{
		char aBuf[256];
		if(NewJumps == -1)
			str_copy(aBuf, m_pPlayer->Localize("You only have your ground jump now"), sizeof(aBuf));
		else if (NewJumps == 1)
			str_copy(aBuf, m_pPlayer->Localize("You can jump 1 time"), sizeof(aBuf));
		else
			str_format(aBuf, sizeof(aBuf), m_pPlayer->Localize("You can jump %d times"), NewJumps);
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), aBuf);
	}

	if (NewJumps > m_MaxJumps && m_DDRaceState != DDRACE_CHEAT && !GameServer()->Arenas()->FightStarted(m_pPlayer->GetCID()))
	{
		m_pPlayer->GiveXP(NewJumps * 100, "for upgrading jumps");
		m_MaxJumps = NewJumps;
	}

	m_Core.m_Jumps = NewJumps;
}

void CCharacter::OnRainbowVIP()
{
	if (!GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_VIP)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("You are not VIP"));
		return;
	}

	Rainbow(!(m_Rainbow || m_pPlayer->m_InfRainbow), m_pPlayer->GetCID());
}

void CCharacter::OnBloodyVIP()
{
	if (!GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_VIP)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("You are not VIP"));
		return;
	}

	Bloody(!(m_Bloody || m_StrongBloody), m_pPlayer->GetCID());
	Atom(false, -1, true);
	Trail(false, -1, true);
	RotatingBall(false, -1, true);
	EpicCircle(false, -1, true);
}

void CCharacter::OnAtomVIP()
{
	if (!GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_VIP)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("You are not VIP"));
		return;
	}

	Atom(!m_Atom, m_pPlayer->GetCID());
	Bloody(false, -1, true);
	Trail(false, -1, true);
	RotatingBall(false, -1, true);
	EpicCircle(false, -1, true);
}

void CCharacter::OnTrailVIP()
{
	if (!GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_VIP)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("You are not VIP"));
		return;
	}

	Trail(!m_Trail, m_pPlayer->GetCID());
	Atom(false, -1, true);
	Bloody(false, -1, true);
	EpicCircle(false, -1, true);
}

void CCharacter::OnSpreadGunVIP()
{
	if (!GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_VIP)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("You are not VIP"));
		return;
	}

	SpreadWeapon(WEAPON_GUN, !m_aSpreadWeapon[WEAPON_GUN], m_pPlayer->GetCID());
}

void CCharacter::OnRainbowHookVIP()
{
	if (GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_VIP != VIP_PLUS)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("You are not VIP+"));
		return;
	}

	HookPower(m_HookPower == RAINBOW ? HOOK_NORMAL : RAINBOW, m_pPlayer->GetCID());
}

void CCharacter::OnRotatingBallVIP()
{
	if (GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_VIP != VIP_PLUS)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("You are not VIP+"));
		return;
	}

	RotatingBall(!m_RotatingBall, m_pPlayer->GetCID());
	Atom(false, -1, true);
	Bloody(false, -1, true);
}

void CCharacter::OnEpicCircleVIP()
{
	if (GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_VIP != VIP_PLUS)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("You are not VIP+"));
		return;
	}

	EpicCircle(!m_EpicCircle, m_pPlayer->GetCID());
	Atom(false, -1, true);
	Bloody(false, -1, true);
	Trail(false, -1, true);
}

void CCharacter::OnLovelyVIP()
{
	if (GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_VIP != VIP_PLUS)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("You are not VIP+"));
		return;
	}

	Lovely(!m_Lovely, m_pPlayer->GetCID());
}

void CCharacter::OnRainbowNameVIP()
{
	if (GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_VIP != VIP_PLUS)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("You are not VIP+"));
		return;
	}

	RainbowName(!m_pPlayer->m_RainbowName, m_pPlayer->GetCID());
}

void CCharacter::OnSparkleVIP()
{
	if (GameServer()->m_Accounts[m_pPlayer->GetAccID()].m_VIP != VIP_PLUS)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), m_pPlayer->Localize("You are not VIP+"));
		return;
	}

	Sparkle(!m_pPlayer->m_Sparkle, m_pPlayer->GetCID());
}
