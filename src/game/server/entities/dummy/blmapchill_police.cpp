// made by fokkonaut and ChillerDragon

#include "blmapchill_police.h"
#include "../character.h"
#include <game/server/player.h>
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include "macros.h"

CDummyBlmapChillPolice::CDummyBlmapChillPolice(CCharacter *pChr)
: CDummyBase(pChr, DUMMYMODE_BLMAPCHILL_POLICE)
{
	m_LovedX = 0;
	m_LovedY = 0;
	m_LowerPanic = 0;
	m_Speed = 70;
	m_HelpMode = 0;
	m_GrenadeJump = 0;
	m_SpawnTeleporter = 0;
	m_FailedAttempts = 0;

	m_IsHelpHook = false;
	m_IsClosestPolice = false;
	m_DidRocketjump = false;
	m_HasTouchedGround = false;
	m_HasAlreadyBeenHere = false;
	m_HasStartGrenade = false;
	m_IsDJUsed = false;
	m_HasReachedCinemaEntrance = false;
}

void CDummyBlmapChillPolice::OnTick()
{
	AvoidFreeze();
	return;

	if (X > 451 && X < 472 && Y > 74 && Y < 85) // new spawn area, walk into the left SPAWN teleporter
	{
		LEFT;
		// jump into tele on spawn or jump onto edge after getting 5 jumps
		if (X > 454 && X < 462) // left side of new spawn area
		{
			JUMP;
			if (TICKS_PASSED(10))
				RELEASE_JUMP;
		}
	}
	else if (X < 240 && Y < 36) // the complete zone in the map intselfs. its for resetting the dummy when he is back in spawn using tp
	{
		if (m_pCharacter->m_IsFrozen && X > 32)
		{
			if (TICKS_PASSED(60))
				GameServer()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_DROP); // tear emote before killing
			if (TICKS_PASSED(500) && IsGrounded()) //kill when freeze
			{
				Die();
				return;
			}
		}
		if (X < 24 && Y < 14 && X > 23) // looking for tp and setting different aims for the swing
			m_SpawnTeleporter = 1;
		if (X < 25 && Y < 14 && X > 24) // looking for tp and setting different aims for the swing
			m_SpawnTeleporter = 2;
		if (X < 26 && Y < 14 && X > 25) // looking for tp and setting different aims for the swing
			m_SpawnTeleporter = 3;
		if (Y > 25 && X > 43 && Y < 35) // kill
		{
			Die();
			return;
		}
		if (Y > 35 && X < 43) // area bottom right from spawn, if he fall, he will kill
		{
			Die();
			return;
		}
		if (X < 16) // area left of old spawn, he will kill too
		{
			Die();
			return;
		}
		if (Y > 25) // after unfreeze hold hook to the right and walk right.
		{
			TARGET(100, 1);
			RIGHT;
			HOOK;
			if (X > 25 && X < 33 && Y > 30) //set weapon and aim down
			{
				if (TICKS_PASSED(5))
				{
					SetWeapon(3);
				}
				if (m_SpawnTeleporter == 1)
				{
					TARGET(190, 100);
				}
				else if (m_SpawnTeleporter == 2)
				{
					TARGET(205, 100);
				}
				else if (m_SpawnTeleporter == 3)
				{
					TARGET(190, 100);
				}
				if (X > 31)
				{
					JUMP;
					FIRE;
				}
			}
			// fix getting stuck in the unfreeze and hooking the wall
			if (X < 33 && X > 31 && Y < 29)
				RELEASE_HOOK;
		}
		else if (X > 33 && X < 50 && Y > 18)
		{
			RIGHT;
			if (X > 33 && X < 42 && Y > 20 && Y < 25)
			{
				GameServer()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_EYES); //happy emote when successfully did the grenaede jump
			}
			if (X < 50)
			{
				JUMP;
				if (TICKS_PASSED(20))
					RELEASE_JUMP;
				if (Y < 19 && m_pCharacter->m_FreezeTime == 0)
					FIRE;
				TARGET(-200, 200);
			}
			// don't walk into freeze
			if (X > 47 && X < 50 &&Y > 17)
			{
				LEFT;
			}
		}
		else if (Y < 16 && X < 75 && X > 40) // walking right on it
		{
			RIGHT;
			RELEASE_JUMP;
			if (Y > 10 && X > 55 && X < 56) //jumping over gap
				JUMP;
		}
		if (Y > 15 && X > 55 && X < 65)
		{
			RIGHT;
			if (X > 63)
				JUMP;
		}
		// fallen into lower right area of the top block area before gores
		if (Y > 20 && Y < 25)
		{
			TARGET(X > 67 ? -20 : 20, 100);
			if (X > 69)
				LEFT;
			if (IsGrounded())
				JUMP;
			if (RAW_Y < _(23) + 20 && VEL_Y < -1.1f)
				FIRE;
		}
		else if (X > 75 && X < 135) //gores stuff (the thign with freeze spikes from top and bottom)
		{
			RELEASE_JUMP;
			RIGHT;
			// start jump into gores
			if (X > 77 && Y > 13 && X < 80)
				JUMP;
			// nade boost on roof
			if ((X > 80 && X < 90) || (X > 104 && X < 117))
			{
				if (Y < 10)
					FIRE;
				TARGET(VEL_X > 12.0f ? -10 : -100, -180);
			}
			if (X > 92 && Y > 12.5f) // hooking stuff from now on
			{
				TARGET(100, -100);
				HOOK;
				if (Y < 14 && X > 100 && X < 110)
					RELEASE_HOOK;
			}
			// Don't swing into roof when boost worked
			if (X > 127 && X < 138 && VEL_X > 12.f && VEL_Y < -1.5f)
				RELEASE_HOOK;
			if (X > 120 && Y < 13)
				RELEASE_HOOK;
		}
		else if (X > 135)
		{
			RIGHT;
			// gores (the long way to 5 jumps)
			if (X < 220)
			{
				if ((RAW_Y > _(12) + 10 && VEL_Y > 4.1f) || (RAW_Y > _(12) + 30 && VEL_Y > -1.1f))
				{
					if (HookState() == HOOK_FLYING || HookState() == HOOK_GRABBED)
					{
						TARGET(-100, 100);
						FIRE;
					}
					else
					{
						TARGET(100, -200);
					}
					HOOK;
					if (Y < 12)
					{
						RELEASE_HOOK;
					}
					if (X > 212)
					{
						HOOK;
						TARGET(100, -75);
						JUMP;
					}
				}
			}
			// 5 jumps area
			if (X > 222)
			{
				RELEASE_JUMP;
				if (Jumps() < 5)
				{
					if (X > 227)
						LEFT;
					else
						RIGHT;
				}
				else // got 5 jumps go into tele
				{
					TARGET(-200, 150);
					if (X < 229)
					{
						if (m_pCharacter->m_FreezeTime == 0)
							FIRE;
					}
					else // on right side of platform (do rj here)
					{
						if (IsGrounded() && !m_pCharacter->GetReloadTimer())
						{
							if (X > 231)
							{
								m_DidRocketjump = true;
								FIRE;
							}
						}
						else
						{
							if (RAW_X > _(229) + 10 && !m_DidRocketjump)
								LEFT;
						}
					}
				}
			}
		}
	}
	if (m_pCharacter->m_IsFrozen && Y < 410) // kills when in freeze and not in policebase
	{
		if (TICKS_PASSED(60))
			GameServer()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_DROP); // tear emote before killing
		if (TICKS_PASSED(500) && IsGrounded()) // kill when freeze
		{
			Die();
			return;
		}
	}
	if (m_pCharacter->m_IsFrozen && X < 41 && X > 33 && Y < 10) // kills when on speedup right next to the newtee spawn to prevent infinite flappy blocking
	{
		if (TICKS_PASSED(500)) // kill when freeze
		{
			Die();
			return;
		}
	}
	// new spawn going left and hopping over the gap under the CFRM.
	// (the jump over the freeze gap before falling down is not here, its in line 38 (search for comment 'jump onto edge after getting 5 jumps'))
	if (m_GrenadeJump == 4 || (X > 368 && Y < 340))
	{
		// change to gun
		if (TICKS_PASSED(3) && X > 497)
			SetWeapon(1);
		LEFT;
		if (X > 509 && Y > 62) // if bot gets under the table he will go right and jump out of the gap under the table
		{
			RIGHT;
			if (X > 511.5)
			{
				if (TICKS_PASSED(10))
					JUMP;
			}
		}
		// jump over chairs
		else if (TICKS_PASSED(10) && X > 505)
			RIGHT;
		// jump out of the chair room
		if (X < 497 && X > 496)
			JUMP;
		// fallen too low backup jump
		if (X > 469 && Y > 74)
		{
			JUMP;
			if (TICKS_PASSED(15))
				RELEASE_JUMP;
		}

		// above new spawn about to jump through freeze
		if (Y < 75)
		{
			// Slow down to hit ground before jumping through freeze
			if (X > 465 && X < 469)
			{
				if (!IsGrounded())
					RIGHT;
			}
			// Too slow to jump through freeze -> go back get speed
			if (X > 455)
			{
				if (X < 461 && VEL_X > -9 && !m_GetSpeed)
				{
					m_GetSpeed = true;
					m_FailedAttempts++;
				}
				if (X > 465 || VEL_X < -10)
					m_GetSpeed = false;
				if (m_GetSpeed)
				{
					RIGHT;
					TARGET(200, 100);
					HOOK;
					if (TICKS_PASSED(15))
						RELEASE_HOOK;
				}
			}
			// count not succeding for a long time as fail
			if (TICKS_PASSED(200))
				m_FailedAttempts++;
		}
		// somebody is blocking flappy intentionally
		if (m_FailedAttempts > 4 && X > 452)
		{
			// don't aim for edge and rather go full speed to bypass the blocker
			LEFT;
			RELEASE_HOOK;
			TARGET(100, 100);
			if (TICKS_PASSED(15))
				SetWeapon(3);
			if (VEL_Y < -0.2f)
				FIRE;
			if (IsGrounded())
				JUMP;
			if (X < 456)
				JUMP;
			if (TICKS_PASSED(15))
				RELEASE_JUMP;
		}

		// rocket jump from new spawn edge to old map entry
		if (X < 453 && Y < 80)
		{
			RELEASE_MOVE;
			if (TICKS_PASSED(10)) // change to grenade
				SetWeapon(WEAPON_GRENADE);

			if (!m_pCharacter->m_FreezeTime && IsGrounded() && m_GrenadeJump == 0) // shoot up
			{
				JUMP;
				TARGET(1, -100);
				FIRE;
				m_GrenadeJump = 1;
			}
			else if (VEL_Y > -7.6f && m_GrenadeJump == 1) // jump in air // basically a timer for when the grenade comes down
			{
				JUMP;
				m_GrenadeJump = 2;
			}
			if (m_GrenadeJump == 2 || m_GrenadeJump == 3) // double grenade
			{
				if (IsGrounded())
					LEFT;
				if (VEL_Y < 0.09f && VEL_X < -0.1f)
				{
					JUMP;
					TARGET(100, 150);
					FIRE;
					m_GrenadeJump = 4;
				}
			}
			if (m_GrenadeJump == 4)
			{
				LEFT;
				if (VEL_Y > 4.4f)
					JUMP;
				if (Y > 85)
					m_GrenadeJump = 0; // something went wrong abort and try fallback grenade jump
			}
		}
		else // Reset rj vars for fallback grenade jump and other reuse
		{
			m_GrenadeJump = 0;
		}
	}
	if (X > 370 && Y < 340 && Y > 310) // bottom going left to the grenade jump
	{
		LEFT;
		// bottom jump over the hole to police station
		if (X < 422 && X > 421)
			JUMP;
		// using 5jump from now on
		if (X < 406 && X > 405)
			JUMP;
		if (X < 397 && X > 396)
			JUMP;
		if (X < 387 && X > 386)
			JUMP;
		// last jump from the 5 jump
		if (X < 377 && X > 376)
			JUMP;
		// recover from uncontrolled long fall
		if (X > 435 && Y > 327 && VEL_Y > 20.0f)
			JUMP;
		if (Y > 339) // if he falls into the hole to police station he will kill
		{
			Die();
			return;
		}
	}
	else if (Y > 296 && X < 370 && X > 350 && Y < 418) // getting up to the grenade jump part
	{
		if (IsGrounded())
		{
			m_HasReachedCinemaEntrance = true;
			JUMP;
		}
		else if (Y < 313 && Y > 312 && X < 367)
			RIGHT;
		else if (X > 367)
			LEFT;
		if (!m_HasReachedCinemaEntrance && X < 370)
			RELEASE_MOVE;
		if (VEL_Y > 0.0000001f && Y < 310)
			JUMP;
	}
	else if (VEL_Y > 0.001f && Y < 293 && X > 366.4f && X < 370)
	{
		LEFT;
		if (TICKS_PASSED(1))
			SetWeapon(WEAPON_GRENADE);
	}
	else if (X > 325 && X < 366 && Y < 295 && Y > 59) // insane grenade jump
	{
		if (IsGrounded() && m_GrenadeJump == 0) // shoot up
		{
			JUMP;
			TARGET(0, -100);
			FIRE;
			m_GrenadeJump = 1;
		}
		else if (VEL_Y > -7.6f && m_GrenadeJump == 1) // jump in air // basically a timer for when the grenade comes down
		{
			JUMP;
			m_GrenadeJump = 2;
		}
		if (m_GrenadeJump == 2 || m_GrenadeJump == 3) // double grenade
		{
			if (Y > 58)
			{
				if (IsGrounded())
					m_HasTouchedGround = true;
				if (m_HasTouchedGround == true)
					LEFT;
				if (VEL_Y > 0.1f && IsGrounded())
				{
					JUMP;
					TARGET(100, 150);
					FIRE;
					m_GrenadeJump = 3;
				}
				if (m_GrenadeJump == 3)
				{
					if (X < 344 && X > 343 && Y > 250) // air grenade for double wall grnade
					{
						TARGET(-100, -100);
						FIRE;
					}
				}
			}
		}
		if (X < 330 && VEL_X == 0.0f && Y > 59) // if on wall jump and shoot
		{
			if (Y > 250 && VEL_Y > 6.0f)
			{
				JUMP;
				m_HasStartGrenade = true;
			}
			if (m_HasStartGrenade == true)
			{
				TARGET(-100, 170);
				FIRE;
			}
			if (Y < 130 && Y > 131)
				JUMP;
			if (VEL_Y > 0.001f && Y < 150)
				m_HasStartGrenade = false;
			if (VEL_Y > 2.0f && Y < 150)
			{
				JUMP;
				m_HasStartGrenade = true;
			}
		}
	}
	if (Y < 60 && X < 337 && X > 325 && Y > 53) // top of the grenade jump // shoot left to get to the right 
	{
		RIGHT;
		TARGET(-100, 0);
		FIRE;
		RELEASE_JUMP;
		if (X > 333 && Y < 335) // hook up and get into the tunnel thingy
		{
			JUMP;
			if (Y < 55)
				RIGHT;
		}
	}
	if (X > 337 && X < 400 && Y < 60 && Y > 40) // hook thru the hookthru
	{
		TARGET(0, -1);
		HOOK;
	}
	if (X > 339.5f && X < 345 && Y < 51)
		LEFT;
	if (X < 339 && X > 315 && Y > 40 && Y < 53) // top of grenade jump the thing to go through the wall
	{
		RELEASE_HOOK;
		TARGET(100, 50);
		if (m_HasAlreadyBeenHere == false)
		{
			if (X < 339)
			{
				RIGHT;
				if (X > 338 && X < 339 && Y > 51)
					m_HasAlreadyBeenHere = true;
			}
		}
		if (m_HasAlreadyBeenHere == true) //using grenade to get throug the freeze in this tunnel thingy
		{
			LEFT;
			if (X < 338)
				FIRE;
		}
		if (X < 328 && Y < 60)
			JUMP;
	}
	// Stuck on the outside of the clu spike thing
	else if (Y > 120 && Y < 185 && X > 233 && X < 300)
	{
		if (X < 272)
			RIGHT;
		/*
		##    <- deep and spikes and clu skip
		#
	######
	######
	##      <-- stuck here
	######
	######
		#
		#  <-- or stuck here
		##
		 #    police entrance
	######      |
	#           v
		*/
	}
	// jumping over the big freeze to get into the tunnel
	else if (Y > 260 && X < 325 && Y < 328 && X > 275)
	{
		LEFT;
		RELEASE_JUMP;
		if (Y > 280 && Y < 285)
			JUMP;
		if (TICKS_PASSED(5))
			SetWeapon(WEAPON_GUN);
	}
	// after grenade jump and being down going into the tunnel to police staion
	else if (Y > 328 && Y < 345 && X > 236 && X < 365)
	{
		RIGHT;
		if (X > 265 && X < 267)
			JUMP;
		if (X > 282 && X < 284)
			JUMP;
	}
	if (X > 294 && X < 297 && Y > 343 && Y < 345) // fix someone blocking flappy, he would just keep moving left into the wall and do nothing there
		RIGHT;
	else if (Y > 337.4f && Y < 345 && X > 295 && X < 365) // walkking left in air to get on the little block
		LEFT;
	if (Y < 361 && Y > 346)
	{
		if (TICKS_PASSED(10))
			SetWeapon(WEAPON_GRENADE);
		RIGHT;
		// slow down and go back to enter the 2 tiles wide hole
		if (X > 321)
			LEFT;
		if (X > 317 && VEL_X > 5.5f)
			RELEASE_MOVE;
		if (X > 316 && VEL_X > 9.8f)
			LEFT;
		// Get enough speed before the rj
		if (X < 297 && X > 296)
			if (VEL_X < 9.9f)
				m_GetSpeed = true;
		if (m_GetSpeed)
		{
			LEFT;
			if ((X < 294 && IsGrounded()) || X < 280)
				m_GetSpeed = false;
		}
		TARGET(-50, 100);
		if (X < 303)
		{
			if (X > 296)
				FIRE;
		}
		else
		{
			TARGET_X(50);
			if (X > 310 && X < 312)
				FIRE;
		}
		RELEASE_JUMP;
		if (VEL_Y > 0.0000001f && Y > 352.6f && X < 315) // jump in air to get to the right
			JUMP;
	}
	if (X > 180 && X < 450 && Y < 450 && Y > 358) // wider police area with left entrance
	{
		if (Y < 408)
			if (TICKS_PASSED(10))
				SetWeapon(WEAPON_GUN);
		// walking right again to get into the tunnel at the bottom
		if (X < 363)
		{
			RIGHT;
		}
		// do not enter in pvp area or bank
		if (X > 323 && Y < 408)
			LEFT;
		// police area entrance tunnel (left side)
		if (X > 316 && X < 366 && Y > 416)
		{
			// jump through freeze if one is close or go back if no vel
			for (int i = 10; i < 160; i+=20)
			{
				if (GameServer()->Collision()->GetTileRaw(RAW_X + i, RAW_Y) == TILE_FREEZE)
				{
					if (VEL_Y > 1.1f)
						LEFT;

					if (IsGrounded() && VEL_X > 8.8f)
						JUMP;
					break;
				}
			}
		}
		/* * * * * * * *
		 * police area *
		 * * * * * * * */
		if (X > 380 && X < 450 && Y < 450 && Y > 380)
		{
			if (X < 397 && Y > 436 && X > 388) // on the money tile jump loop, to prevent blocking flappy there
			{
				RELEASE_JUMP;
				if (TICKS_PASSED(20))
					JUMP;
			}
			//detect lower panic (accedentally fall into the lower police base 
			if (!m_LowerPanic && Y > 437 && RAW_Y > m_LovedY)
			{
				m_LowerPanic = 1;
				GameServer()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_SPLATTEE); //angry emote
			}

			if (m_LowerPanic)
			{
				//Check for end panic
				if (Y < 434)
				{
					if (IsGrounded())
						m_LowerPanic = 0; //made it up yay
				}

				if (m_LowerPanic == 1)//position to jump on stairs
				{
					if (X < 400)
						RIGHT;
					else if (X > 401)
						LEFT;
					else
						m_LowerPanic = 2;
				}
				else if (m_LowerPanic == 2) //jump on the left starblock element
				{
					if (IsGrounded())
					{
						JUMP;
						if (TICKS_PASSED(20))
							RELEASE_JUMP;
					}

					//navigate to platform
					if (RAW_Y < _(435) - 10)
					{
						LEFT;
						if (Y < 433)
						{
							if (VEL_Y > 0.01f && m_IsDJUsed == false)
							{
								JUMP; //double jump
								if (!IsGrounded()) // this dummyuseddj is for only using default 2 jumps even if 5 jump is on
									m_IsDJUsed = true;
							}
						}
						if (m_IsDJUsed == true && IsGrounded())
							m_IsDJUsed = false;
					}

					else if (Y < 438) //only if high enough focus on the first lower platform
					{
						if (X < 403)
							RIGHT;
						else if (RAW_X > _(404) + 20)
							LEFT;
					}

					if ((RAW_Y > _(441) + 10 && (X > 402 || RAW_X < _(399) + 10)) || m_pCharacter->m_IsFrozen) //check for fail position
						m_LowerPanic = 1; //lower panic mode to reposition
				}
			}
			else //no dummy lower panic
			{
				m_HelpMode = 0;
				//check if officer needs help
				CCharacter *pChr = GameWorld()->ClosestCharacter(POS, m_pCharacter, m_pPlayer->GetCID(), 1);
				if (pChr && pChr->IsAlive())
				{
					if (Y > 435) // setting the destination of dummy to top left police entry bcs otherwise bot fails when trying to help --> walks into jail wall xd
					{
						m_LovedX = _(392 + rand() % 2);
						m_LovedY = _(430);
					}
					//aimbot on heuzeueu
					TARGET_X(pChr->Core()->m_Pos.x - RAW_X);
					TARGET_Y(pChr->Core()->m_Pos.y - RAW_Y);

					m_IsClosestPolice = false;

					if (pChr->m_PoliceHelper || GameServer()->m_Accounts[pChr->GetPlayer()->GetAccID()].m_PoliceLevel)
						m_IsClosestPolice = true;

					if (pChr->GetCore().m_Pos.x > _(444) - 10) //police dude failed too far --> to be reached by hook (set too help mode extream to leave save area)
					{
						m_HelpMode = 2;
						if (Jumped() > 1 && X > 431) //double jumped and above the freeze
							LEFT;
						else
							RIGHT;
						//doublejump before falling in freeze
						if ((X > 432 && Y > 432) || X > 437) //over the freeze and too low
						{
							JUMP;
							m_IsHelpHook = true;
						}
						if (IsGrounded() && X > 430 && TICKS_PASSED(60))
							JUMP;
					}
					else
						m_HelpMode = 1;

					if (m_HelpMode == 1 && RAW_X > _(431) + 10)
						LEFT;
					else if (m_HelpMode == 1 && X < 430)
						RIGHT;
					else
					{
						if (!m_IsHelpHook && m_IsClosestPolice)
						{
							if (m_HelpMode == 2) //police dude failed too far --> to be reached by hook
							{
								//if (X > 435) //moved too double jump
								//{
								//	m_IsHelpHook = true;
								//}
							}
							else if (pChr->GetCore().m_Pos.x > _(439)) //police dude in the middle
							{
								if (IsGrounded())
								{
									m_IsHelpHook = true;
									JUMP;
									HOOK;
								}
							}
							else //police dude failed too near to hook from ground
							{
								if (VEL_Y < -4.20f && Y < 431)
								{
									m_IsHelpHook = true;
									JUMP;
									HOOK;
								}
							}
						}
						if (TICKS_PASSED(8))
							RIGHT;
					}

					if (m_IsHelpHook)
					{
						HOOK;
						if (TICKS_PASSED(200))
						{
							m_IsHelpHook = false; //timeout hook maybe failed
							RELEASE_HOOK;
							RIGHT;
						}
					}

					//dont wait on ground
					if (IsGrounded() && TICKS_PASSED(900))
						JUMP;
					//backup reset jump
					if (TICKS_PASSED(1337))
						RELEASE_JUMP;
				}

				if (!m_HelpMode)
				{
					//==============
					//NOTHING TO DO
					//==============
					//basic walk to destination
					if (RAW_X < m_LovedX - 32)
						RIGHT;
					else if (RAW_X > m_LovedX + 32 && X > 384)
						LEFT;

					//change changing speed
					if (TICKS_PASSED(m_Speed))
					{
						if (rand() % 2 == 0)
							m_Speed = rand() % 10000 + 420;
					}

					//choose beloved destination
					if (TICKS_PASSED(m_Speed))
					{
						if ((rand() % 2) == 0)
						{
							if ((rand() % 3) == 0)
							{
								m_LovedX = _(420) + rand() % 69;
								m_LovedY = _(430);
								GameServer()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_GHOST);
							}
							else
							{
								m_LovedX = _(392 + rand() % 2);
								m_LovedY = _(430);
							}
							if ((rand() % 2) == 0)
							{
								m_LovedX = _(384) + rand() % 128;
								m_LovedY = _(430);
								GameServer()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_MUSIC);
							}
							else
							{
								if (rand() % 3 == 0)
								{
									m_LovedX = _(420) + rand() % 128;
									m_LovedY = _(430);
									GameServer()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_SUSHI);
								}
								else if (rand() % 4 == 0)
								{
									m_LovedX = _(429) + rand() % 64;
									m_LovedY = _(430);
									GameServer()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_SUSHI);
								}
							}
							if (rand() % 5 == 0) //lower middel base
							{
								m_LovedX = _(410) + rand() % 64;
								m_LovedY = _(443);
							}
						}
						else
							GameServer()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_EXCLAMATION);
					}
				}
			}

			//dont walk into the lower police base entry freeze
			if (X > 425 && X < 429) //right side
			{
				if (VEL_X < -0.02f && IsGrounded())
					JUMP;
			}
			else if (X > 389 && X < 391) //left side
			{
				if (VEL_X > 0.02f && IsGrounded())
					JUMP;
			}

			//jump over the police underground from entry to enty
			if (RAW_Y > m_LovedY) //only if beloved place is an upper one
			{
				if (X > 415 && X < 418) //right side
				{
					if (VEL_X < -0.02f && IsGrounded())
					{
						JUMP;
						if (TICKS_PASSED(5))
							RELEASE_JUMP;
					}
				}
				else if (X > 398 && X < 401) //left side
				{
					if (VEL_X > 0.02f && IsGrounded())
					{
						JUMP;
						if (TICKS_PASSED(5))
							RELEASE_JUMP;
					}
				}

				//do the doublejump
				if (VEL_Y > 6.9f && Y > 430 && X < 433  && m_IsDJUsed == false) //falling and not too high to hit roof with head
				{
					JUMP;
					if (!IsGrounded()) // this dummyuseddj is for only using default 2 jumps even if 5 jump is on
						m_IsDJUsed = true;
				}
				if (m_IsDJUsed == true && IsGrounded())
					m_IsDJUsed = false;
			}
		}
		// left side of police the freeze pit
		if (Y > 380 && X < 381 && X > 363)
		{
			RIGHT;
			if (X > 367 && X < 368 && IsGrounded())
				JUMP;
			if (Y > 433.7f)
				JUMP;
		}
		if (X > 290 && X < 450 && Y > 415 && Y < 450)
		{
			if (m_pCharacter->m_IsFrozen) // kills when in freeze in policebase or left of it (takes longer that he kills bcs the way is so long he wait a bit longer for help)
			{
				if (TICKS_PASSED(60))
					GameServer()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_DROP); // tear emote before killing
				if (TICKS_PASSED(3000) && (IsGrounded() || X > 430)) // kill when freeze
				{
					Die();
					return;
				}
			}
		}
	}
}