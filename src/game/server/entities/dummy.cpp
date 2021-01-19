#include "character.h"
#include <game/server/player.h>
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

void CCharacter::Fire(bool Stroke)
{
	if (Stroke)
	{
		m_LatestInput.m_Fire++;
		m_Input.m_Fire++;
	}
	else
	{
		m_LatestInput.m_Fire = 0;
		m_Input.m_Fire = 0;
	}
}

void CCharacter::DummyTick()
{
	if (!IsAlive() || !m_pPlayer->m_IsDummy || m_pPlayer->m_TeeControllerID != -1)
		return;

	ResetInput();
	m_Input.m_Hook = 0;

	if (m_pPlayer->GetDummyMode() == DUMMYMODE_CHILLBLOCK5_RACER) //Race mode ChillBlock5 //by chillerdragon cleanup by fokkonaut
	{
		if (m_Core.m_Pos.x > 241 * 32 && m_Core.m_Pos.x < 418 * 32 && m_Core.m_Pos.y > 121 * 32 && m_Core.m_Pos.y < 192 * 32) //new spawn ChillBlock5 (tourna edition (the on with the gores stuff))
		{
			//dieser code wird also nur ausgeführt wenn der bot gerade im neuen bereich ist
			if (m_Core.m_Pos.x > 319 * 32 && m_Core.m_Pos.y < 161 * 32) //top right spawn
			{
				//look up left
				if (m_Core.m_Pos.x < 372 * 32 && m_Core.m_Vel.y > 3.1f)
				{
					m_Input.m_TargetX = -30;
					m_Input.m_TargetY = -80;
				}
				else
				{
					m_Input.m_TargetX = -100;
					m_Input.m_TargetY = -80;
				}

				if (m_Core.m_Pos.x > 331 * 32 && m_IsFrozen)
				{
					Die();
					return;
				}

				if (m_Core.m_Pos.x < 327 * 32) //dont klatsch in ze wand
					m_Input.m_Direction = 1; //nach rechts laufen
				else
					m_Input.m_Direction = -1;

				if (IsGrounded() && m_Core.m_Pos.x < 408 * 32) //initial jump from spawnplatform
					m_Input.m_Jump = 1;

				if (m_Core.m_Pos.x > 330 * 32) //only hook in tunnel and let fall at the end
				{
					if (m_Core.m_Pos.y > 147 * 32 || (m_Core.m_Pos.y > 143 * 32 && m_Core.m_Vel.y > 3.3f)) //gores pro hook up
						m_Input.m_Hook = 1;
					else if (m_Core.m_Pos.y < 143 * 32 && m_Core.m_Pos.x < 372 * 32) //hook down (if too high and in tunnel)
					{
						m_Input.m_TargetX = -42;
						m_Input.m_TargetY = 100;
						m_Input.m_Hook = 1;
					}
				}
			}
			else if (m_Core.m_Pos.x < 317 * 32) //top left spawn
			{
				if (m_Core.m_Pos.y < 158 * 32) //spawn area find down
				{
					//selfkill
					if (m_IsFrozen)
					{
						Die();
						return;
					}

					if (m_Core.m_Pos.x < 276 * 32 + 20) //is die mitte von beiden linken spawns also da wo es runter geht
					{
						m_Input.m_TargetX = 57;
						m_Input.m_TargetY = -100;
						m_Input.m_Direction = 1;
					}
					else
					{
						m_Input.m_TargetX = -57;
						m_Input.m_TargetY = -100;
						m_Input.m_Direction = -1;
					}

					if (m_Core.m_Pos.y > 147 * 32)
					{
						m_Input.m_Hook = 1;
						if (m_Core.m_Pos.x > 272 * 32 && m_Core.m_Pos.x < 279 * 32) //let fall in the hole
						{
							m_Input.m_Hook = 0;
							m_Input.m_TargetX = 2;
							m_Input.m_TargetY = 100;
						}
					}
				}
				else if (m_Core.m_Pos.y > 162 * 32) //managed it to go down --> go left
				{
					//selfkill
					if (m_IsFrozen)
					{
						Die();
						return;
					}

					if (m_Core.m_Pos.x < 283 * 32)
					{
						m_Input.m_TargetX = 200;
						m_Input.m_TargetY = -136;
						if (m_Core.m_Pos.y > 164 * 32 + 10)
							m_Input.m_Hook = 1;
					}
					else
					{
						m_Input.m_TargetX = 80;
						m_Input.m_TargetY = -100;
						if (m_Core.m_Pos.y > 171 * 32 - 10)
							m_Input.m_Hook = 1;
					}

					m_Input.m_Direction = 1;
				}
			}
			else //lower end area of new spawn --> entering old spawn by walling and walking right
			{
				m_Input.m_Direction = 1;
				m_Input.m_TargetX = 200;
				m_Input.m_TargetY = -84;

				//Selfkills
				if (m_IsFrozen && IsGrounded()) //should never lie in freeze at the ground
				{
					Die();
					return;
				}

				if (m_Core.m_Pos.y < 166 * 32 - 20)
					m_Input.m_Hook = 1;

				if (m_Core.m_Pos.x > 332 * 32 && m_Core.m_Pos.x < 337 * 32 && m_Core.m_Pos.y > 182 * 32) //wont hit the wall --> jump
					m_Input.m_Jump = 1;

				if (m_Core.m_Pos.x > 336 * 32 + 20 && m_Core.m_Pos.y > 180 * 32) //stop moving if walled
					m_Input.m_Direction = 0;
			}
		}
		else if (false && m_Core.m_Pos.y < 193 * 32 && m_Core.m_Pos.x < 450 * 32) //new spawn
		{
			m_Input.m_TargetX = 200;
			m_Input.m_TargetY = -80;

			//not falling in freeze is bad
			if (m_Core.m_Vel.y < 0.01f && m_FreezeTime > 0)
			{
				if (Server()->Tick() % 40 == 0)
				{
					Die();
					return;
				}
			}
			if (m_Core.m_Pos.y > 116 * 32 && m_Core.m_Pos.x > 394 * 32)
			{
				Die();
				return;
			}

			if (m_Core.m_Pos.x > 364 * 32 && m_Core.m_Pos.y < 126 * 32 && m_Core.m_Pos.y > 122 * 32 + 10)
			{
				if (m_Core.m_Vel.y > -1.0f)
					m_Input.m_Hook = 1;
			}

			if (m_Core.m_Pos.y < 121 * 32 && m_Core.m_Pos.x > 369 * 32)
				m_Input.m_Direction = -1;
			else
				m_Input.m_Direction = 1;
			if (m_Core.m_Pos.y < 109 * 32 && m_Core.m_Pos.x > 377 * 32 && m_Core.m_Pos.x < 386 * 32)
				m_Input.m_Direction = 1;

			if (m_Core.m_Pos.y > 128 * 32)
				m_Input.m_Jump = 1;

			//speeddown at end to avoid selfkill cuz to slow falling in freeze
			if (m_Core.m_Pos.x > 384 * 32 && m_Core.m_Pos.y > 121 * 32)
			{
				m_Input.m_TargetX = 200;
				m_Input.m_TargetY = 300;
				m_Input.m_Hook = 1;
			}
		}
		else //under 193 (above 193 is new spawn)
		{
			//Selfkill
			//Checken ob der bot far im race ist
			if (m_DummyCollectedWeapons && m_Core.m_Pos.x > 470 * 32 && m_Core.m_Pos.y < 200 * 32)
			{
				CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID(), 7);
				if (pChr && pChr->IsAlive())
				{
					//
				}
				else
				{
					if (!m_IsFrozen || m_Core.m_Vel.x < -0.5f || m_Core.m_Vel.x > 0.5f || m_Core.m_Vel.y != 0.000000f)
					{
						//mach nichts
					}
					else
					{
						if (Server()->Tick() % 370 == 0)
						{
							Die();
							return;
						}
					}
				}
			}
			else //sonst normal relativ schnell killen
			{
				CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID());
				if (pChr && pChr->IsAlive())
				{
					if (!m_IsFrozen || m_Core.m_Vel.x < -0.5f || m_Core.m_Vel.x > 0.5f || m_Core.m_Vel.y != 0.000000f)
					{
						//mach nichts
					}
					else
					{
						if (Server()->Tick() % 270 == 0)
						{
							Die();
							return;
						}
					}
				}
				else
				{
					if (m_IsFrozen && m_Core.m_Vel.y == 0.000000f && m_Core.m_Vel.x < 0.1f && m_Core.m_Vel.x > -0.1f)
					{
						Die();
						return;
					}
				}
			}

			//instant self kills
			if (m_Core.m_Pos.x < 390 * 32 && m_Core.m_Pos.x > 325 * 32 && m_Core.m_Pos.y > 215 * 32)  //Links am spawn runter
			{
				Die();
				return;
			}

			else if (m_Core.m_Pos.y < 215 * 32 && m_Core.m_Pos.y > 213 * 32 && m_Core.m_Pos.x > 415 * 32 && m_Core.m_Pos.x < 428 * 32) //freeze decke im tunnel
			{
				Die();
				return;
			}

			if ((m_Core.m_Pos.y < 220 * 32 && m_Core.m_Pos.x < 415 * 32 && m_FreezeTime > 1) && (m_Core.m_Pos.x > 350 * 32)) //always suicide on freeze if not reached teh block area yet             (new) AND not coming from the new spawn and falling through the freeze
			{
				Die();
				return;
			}

			m_DummyMovementMode = 0;

			if (m_Core.m_Pos.x < 388 * 32 && m_Core.m_Pos.y > 213 * 32) //jump to old spawn
			{
				m_Input.m_Jump = 1;
				Fire();
				m_Input.m_Hook = 1;
				m_Input.m_TargetX = -200;
				m_Input.m_TargetY = 0;
			}

			if (m_DummyMovementMode == 0)
			{
				if (m_Core.m_Pos.x < 415 * 32) //bis zum tunnel laufen
					m_Input.m_Direction = 1;
				else if (m_Core.m_Pos.x < 440 * 32 && m_Core.m_Pos.y > 213 * 32) //im tunnel laufen
				{
					m_Input.m_Direction = 1;
					if (m_Core.m_Vel.x < 5.5f)
					{
						m_Input.m_TargetY = -3;
						m_Input.m_TargetX = 200;
						m_LatestInput.m_TargetY = -3;
						m_LatestInput.m_TargetX = 200;

						if (Server()->Tick() % 30 == 0)
							SetWeapon(0);
						if (Server()->Tick() % 55 == 0)
						{
							if (m_FreezeTime == 0)
								Fire();
						}
						if (Server()->Tick() % 200 == 0)
							m_Input.m_Jump = 1;
					}
				}

				//externe if abfrage weil laufen während sprinegn xD
				if (m_Core.m_Pos.x > 413 * 32 && m_Core.m_Pos.x < 415 * 32) // in den tunnel springen
					m_Input.m_Jump = 1;
				else if (m_Core.m_Pos.x > 428 * 32 - 20 && m_Core.m_Pos.y > 213 * 32) // im tunnel springen
					m_Input.m_Jump = 1;

				// externen springen aufhören für dj
				if (m_Core.m_Pos.x > 428 * 32 && m_Core.m_Pos.y > 213 * 32) // im tunnel springen nicht mehr springen
					m_Input.m_Jump = 0;

				//nochmal extern weil springen während springen
				if (m_Core.m_Pos.x > 430 * 32 && m_Core.m_Pos.y > 213 * 32) // im tunnel springen springen
					m_Input.m_Jump = 1;

				if (m_Core.m_Pos.x > 431 * 32 && m_Core.m_Pos.y > 213 * 32) //jump refillen für wayblock spot
					m_Input.m_Jump = 0;
			}
			else if (m_DummyMovementMode == 1) //enter ruler area with a left jump 
			{
				if (m_Core.m_Pos.x < 415 * 32) //bis zum tunnel laufen
					m_Input.m_Direction = 1;
				else if (m_Core.m_Pos.x < 440 * 32 && m_Core.m_Pos.y > 213 * 32) //im tunnel laufen
				{
					m_Input.m_Direction = 1;
				}

				//springen
				if (m_Core.m_Pos.x > 413 * 32 && m_Core.m_Pos.x < 415 * 32 && m_Core.m_Pos.y > 213 * 32) // in den tunnel springen
					m_Input.m_Jump = 1;
				else if (m_Core.m_Pos.x > 428 * 32 - 3 && m_Core.m_Pos.y > 217 * 32 && m_Core.m_Pos.y > 213 * 32) // im tunnel springen
					m_Input.m_Jump = 1;

				if (m_Core.m_Pos.x > 429 * 32 - 18)
					m_Input.m_Direction = -1;

				//nochmal extern weil springen während springen
				if (m_Core.m_Pos.y < 217 * 32 && m_Core.m_Pos.x > 420 * 32 && m_Core.m_Pos.y > 213 * 32 + 20)
				{
					//m_Input.m_Direction = -1;
					if (m_Core.m_Pos.y < 216 * 32) // im tunnel springen springen
						m_Input.m_Jump = 1;
				}
			}

			if (m_Core.m_Pos.y < 213 * 32 && m_Core.m_Pos.x > 428 * 32 && m_Core.m_Pos.x < 429 * 32)
				m_Input.m_Jump = 1;

			if (m_Core.m_Pos.x > 417 * 32 && m_Core.m_Pos.y < 213 * 32 && m_Core.m_Pos.x < 450 * 32) //vom ruler nach rechts nachm unfreeze werden
				m_Input.m_Direction = 1;

			if (m_Core.m_Pos.x > 439 * 32 && m_Core.m_Pos.y < 213 * 32 && m_Core.m_Pos.x < 441 * 32) //über das freeze zum hf start springen
				m_Input.m_Jump = 1;

			if (m_Core.m_Pos.y > 200 * 32 && m_Core.m_Pos.x > 457 * 32)
				m_Input.m_Direction = -1;

			if (m_DummyCollectedWeapons)
			{
				if (m_Core.m_Pos.x < 466 * 32)
					SetWeapon(3);

				//prepare for rocktjump
				if (m_Core.m_Pos.x < 451 * 32 + 1 && m_Core.m_Pos.y > 209 * 32) //wenn zu weit links für rj
					m_Input.m_Direction = 1;
				else if (m_Core.m_Pos.x > 451 * 32 + 3 && m_Core.m_Pos.y > 209 * 32) //wenn zu weit links für rj
					m_Input.m_Direction = -1;
				else
				{
					if (m_Core.m_Vel.x < 0.01f && m_Core.m_Vel.x > -0.01f) //nahezu stillstand
					{
						//ROCKETJUMP
						if (m_Core.m_Pos.x > 450 * 32 && m_Core.m_Pos.y > 209 * 32)
						{
							m_Input.m_TargetX = 0;
							m_Input.m_TargetY = 37;
							m_LatestInput.m_TargetX = 0;
							m_LatestInput.m_TargetY = 37;
						}

						if (m_Core.m_Pos.y > 210 * 32 + 30 && !m_IsFrozen) //wenn der dummy auf dem boden steht und unfreeze is
						{
							if (m_Core.m_Vel.y == 0.000000f)
								m_Input.m_Jump = 1;
						}

						if (m_Core.m_Pos.y > 210 * 32 + 10 && m_Core.m_Vel.y < -0.9f && !m_IsFrozen) //dann schiessen
							Fire();
					}
				}
			}

			if (m_Core.m_Pos.x > 448 * 32 && m_Core.m_Pos.x < 458 * 32 && m_Core.m_Pos.y > 209 * 32) //wenn der bot auf der platform is
			{
				if (Server()->Tick() % 3 == 0)
					m_Input.m_Direction = 0;
			}

			//Rocketjump2 an der freeze wand
			//prepare aim!
			if (m_Core.m_Pos.y < 196 * 32)
			{
				m_Input.m_TargetX = -55;
				m_Input.m_TargetY = 32;
				m_LatestInput.m_TargetX = -55;
				m_LatestInput.m_TargetY = 32;
			}

			if (m_Core.m_Pos.x < 452 * 32 && m_Core.m_Pos.y > 188 * 32 && m_Core.m_Pos.y < 192 * 32 && m_Core.m_Vel.y < 0.1f && m_DummyCollectedWeapons)
			{
				m_DummyRocketJumped = true;
				Fire();
			}

			//Fliegen nach rocketjump
			if (m_DummyRocketJumped)
			{
				m_Input.m_Direction = 1;

				if (m_Core.m_Pos.x > 461 * 32 && m_Core.m_Pos.y > 192 * 32 + 20)
					m_Input.m_Jump = 1;

				if (m_Core.m_Pos.x > 478 * 32 || m_Core.m_Pos.y > 196 * 32)
					m_DummyRocketJumped = false;
			}

			//Check ob der dummy schon waffen hat
			if (m_aWeapons[3].m_Got && m_aWeapons[2].m_Got)
				m_DummyCollectedWeapons = true;
			else
				m_DummyCollectedWeapons = false;

			if (1 == 0.5 + 0.5)
			{
				CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID());
				if (pChr && pChr->IsAlive())
				{
					if (pChr->m_Pos.y < 165 * 32 && pChr->m_Pos.x > 451 * 32 - 10 && pChr->m_Pos.x < 454 * 32 + 10)
						m_DummyMateCollectedWeapons = true;
				}
			}

			//Hammerfly
			if (m_Core.m_Pos.x > 447 * 32)
			{
				CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID());
				if (pChr && pChr->IsAlive())
				{
					//unfreezemates on platform
					//Get closer to the mate
					if (pChr->m_Pos.y == m_Core.m_Pos.y && m_Core.m_Pos.x > 450 * 32 && m_Core.m_Pos.x < 457 * 32 && pChr->m_FreezeTime > 0)
					{
						if (pChr->m_Pos.x > m_Core.m_Pos.x + 70) //if friend is on the right of the bot
							m_Input.m_Direction = 1;
						else if (pChr->m_Pos.x < m_Core.m_Pos.x - 70) //if firend is on the left of the bot
							m_Input.m_Direction = -1;
					}

					//Hammer mate if near enough
					if (m_Core.m_Pos.x < 456 * 32 + 20 && pChr->m_FreezeTime > 0 && m_Core.m_Pos.y > 209 * 32 && pChr->m_Pos.y > 209 * 32 && pChr->m_Pos.x > 449 * 32 && pChr->m_Pos.x < 457 * 32)
					{
						if (pChr->m_Pos.x > m_Core.m_Pos.x - 60 && pChr->m_Pos.x < m_Core.m_Pos.x + 60)
						{
							if (GetActiveWeapon() == WEAPON_HAMMER && m_FreezeTime == 0)
							{
								Fire();
								m_DummyHookAfterHammer = true;
							}
						}
					}
					if (m_DummyHookAfterHammer)
					{
						if (pChr->m_Core.m_Vel.x < -0.3f || pChr->m_Core.m_Vel.x > 0.3f)
							m_Input.m_Hook = 1;
						else
							m_DummyHookAfterHammer = false;

						//stop this hook after some time to prevent nonstop hooking if something went wrong
						if (Server()->Tick() % 100 == 0)
							m_DummyHookAfterHammer = false;
					}

					if (pChr->m_IsFrozen)
						m_DummyHelpBeforeFly = true;
					if (pChr->m_FreezeTime == 0)
						m_DummyHelpBeforeFly = false;
				}

				if (m_DummyHelpBeforeFly)
				{
					if (!m_DummyCollectedWeapons)
					{
						if (Server()->Tick() % 20 == 0)
							SetWeapon(0);

						CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID(), 8); //only search freezed tees --> so even if others get closer he still has his mission 
						if (pChr && pChr->IsAlive())
						{
							m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
							m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;
							m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
							m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;

							//Check where help is needed
							if (pChr->m_Pos.x > 457 * 32 + 10 && pChr->m_Pos.x < 468 * 32 && pChr->m_Pos.y < 213 * 32 + 5) //right freeze becken
							{
								//Get in help position:
								if (m_Core.m_Pos.x < 457 * 32 - 1)
									m_Input.m_Direction = 1;
								else if (m_Core.m_Pos.x > 457 * 32 + 8)
									m_Input.m_Direction = -1;

								//jump 
								if (m_Core.m_Vel.y == 0.000000f && m_FreezeTime == 0 && m_Core.m_Pos.y > 209 * 32)
								{
									if (Server()->Tick() % 16 == 0)
										m_Input.m_Jump = 1;
								}

								//hook
								if (m_Core.m_Pos.y < pChr->m_Pos.y - 60 && pChr->m_FreezeTime > 0)
								{
									m_Input.m_Hook = 1;
									if (m_Core.m_Pos.x > 454 * 32)
										m_Input.m_Direction = -1;
								}

								//unfreezehammer
								if (pChr->m_Pos.x < m_Core.m_Pos.x + 60 && pChr->m_Pos.x > m_Core.m_Pos.x - 60 && pChr->m_Pos.y < m_Core.m_Pos.y + 60 && pChr->m_Pos.y > m_Core.m_Pos.y - 60)
								{
									if (m_FreezeTime == 0)
										Fire();
								}
							}
							else if (pChr->m_Pos.x > 469 * 32 + 20 && pChr->m_Pos.x < 480 * 32 && pChr->m_Pos.y < 213 * 32 + 5 && pChr->m_Pos.y > 202 * 32)
							{
								//Get in help position:
								if (m_Core.m_Pos.x < 467 * 32)
								{
									if (m_Core.m_Pos.x < 458 * 32)
									{
										if (m_Core.m_Vel.y == 0.000000f)
											m_Input.m_Direction = 1;
									}
									else
										m_Input.m_Direction = 1;

									if (m_Core.m_Vel.y > 0.2f || m_Core.m_Pos.y > 212 * 32)
										m_Input.m_Jump = 1;
								}
								if (m_Core.m_Pos.x > 469 * 32)
								{
									m_Input.m_Direction = -1;
									if (m_Core.m_Vel.y > 0.2f || m_Core.m_Pos.y > 212 * 32)
										m_Input.m_Jump = 1;
								}

								//jump 
								if (m_Core.m_Vel.y == 0.000000f && m_FreezeTime == 0 && m_Core.m_Pos.y > 209 * 32 && m_Core.m_Pos.x > 466 * 32)
								{
									if (Server()->Tick() % 16 == 0)
										m_Input.m_Jump = 1;
								}

								//hook
								if (m_Core.m_Pos.y < pChr->m_Pos.y - 60 && pChr->m_FreezeTime > 0)
								{
									m_Input.m_Hook = 1;
									if (m_Core.m_Pos.x > 468 * 32)
										m_Input.m_Direction = -1;
								}

								//unfreezehammer
								if (pChr->m_Pos.x < m_Core.m_Pos.x + 60 && pChr->m_Pos.x > m_Core.m_Pos.x - 60 && pChr->m_Pos.y < m_Core.m_Pos.y + 60 && pChr->m_Pos.y > m_Core.m_Pos.y - 60)
								{
									if (m_FreezeTime == 0)
										Fire();
								}
							}
							else if (pChr->m_Pos.x > 437 * 32 && pChr->m_Pos.x < 456 * 32 && pChr->m_Pos.y < 219 * 32 && pChr->m_Pos.y > 203 * 32) //left freeze becken
							{
								if (m_aWeapons[2].m_Got && Server()->Tick() % 40 == 0)
									SetWeapon(2);

								if (m_Core.m_Jumped == 0)//has dj --> go left over the freeze and hook ze mate
									m_Input.m_Direction = -1;
								else
									m_Input.m_Direction = 1;

								if (m_Core.m_Pos.y > 211 * 32 + 21)
								{
									m_Input.m_Jump = 1;
									m_DummyHelpBeforeHammerfly = true;
									if (m_aWeapons[2].m_Got && m_FreezeTime == 0)
										Fire();
								}

								if (m_DummyHelpBeforeHammerfly)
								{
									m_Input.m_Hook = 1;
									m_DummyHelpBeforeHammerfly++;
									if (m_DummyHelpBeforeHammerfly > 60 && m_Core.m_HookState != HOOK_GRABBED)
										m_DummyHelpBeforeHammerfly = 0;
								}
							}
							else
								m_DummyHelpBeforeFly = false;
						}
						else
							m_DummyHelpBeforeFly = false;
					}
				}
				if (!m_DummyHelpBeforeFly)
				{
					if (!m_DummyCollectedWeapons)
					{
						if (Server()->Tick() % 20 == 0)
							SetWeapon(0);

						CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID());
						if (pChr && pChr->IsAlive())
						{
							m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
							m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;
							m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
							m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;

							//Hammerfly normal way
							if (m_DummyRaceMode == 0)
							{
								//shot schiessen schissen
								//im freeze nicht schiessen
								if (m_DummyFreezed == false)
								{
									m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
									m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;

									//schiess delay
									if (Server()->Tick() >= m_DummyEmoteTickNext && pChr->m_Pos.y < 212 * 32 - 5)
									{
										m_pPlayer->m_LastEmote = Server()->Tick();
										GameServer()->SendEmoticon(m_pPlayer->GetCID(), 7);

										Fire();

										m_DummyEmoteTickNext = Server()->Tick() + Server()->TickSpeed() / 2;
									}

									//wenn schon nah an der nade
									if (m_Core.m_Pos.y < 167 * 32)
									{
										m_Input.m_Jump = 1;

										if (m_Core.m_Pos.x < 453 * 32 - 8)
											m_Input.m_Direction = 1;
										else if (m_Core.m_Pos.x > 454 * 32 + 8)
											m_Input.m_Direction = -1;
									}

								}
								else if (m_DummyFreezed == true) //if (m_DummyFreezed == false)
								{
									Fire(false);
									m_DummyFreezed = false;
								}
							}
						}
					}
				}
			}

			if (m_Core.m_Pos.y < 200 * 32)
			{
				//check ob der mate fail ist
				CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID());
				if (pChr && pChr->IsAlive())
				{
					if ((pChr->m_Pos.y > 198 * 32 + 10 && pChr->IsGrounded()) ||
						(pChr->m_Pos.y < 198 * 32 + 10 && pChr->m_Pos.x < 472 * 32 && pChr->IsGrounded()) || // recognize mates freeze in the freeze tunnel on the left
						(m_DummyMateHelpMode == 3)) // yolo hook swing mode handles mate as failed until he is unfreeze
					{
						if (pChr->m_IsFrozen)
							m_DummyMateFailed = true;
					}
					if (pChr->m_FreezeTime == 0)
					{
						m_DummyMateFailed = false;
						m_DummyMateHelpMode = 2; // set it to something not 3 because help mode 3 cant do the part (cant play if not failed)
					}
				}

				//schau ob der bot den part geschafft hat und auf state -1 gehen soll
				if (m_Core.m_Pos.x > 485 * 32)
					m_DummyRaceState = -1; //part geschafft --> mach aus

				if (m_Core.m_Pos.x > 466 * 32)
				{
					CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID());
					if (pChr && pChr->IsAlive())
					{
						m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
						m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;
						m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
						m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;
						
						if (m_DummyCollectedWeapons && m_FreezeTime == 0 && m_Core.m_Pos.x > 478 * 32 && m_Core.m_Pos.x < 492 * 32 + 10 && pChr->m_Pos.x > 476 * 32) //new testy
						{
							if (pChr->m_Pos.y > 198 * 32) //wenn pChr iwiw runter gefallen is dann mach den hook weg
								m_DummyRaceHook = false;

							if (m_Core.m_Pos.x < 477 * 32 || m_DummyMateFailed) //TODO: add if pChr wants to make the part
								m_DummyRaceState = -1;

							if (m_Core.m_Pos.x > 477 * 32 && m_Core.m_Pos.x < 485 * 32 && m_Core.m_Pos.y < 195 * 32  /*|| pChr->m_Pos.x < 476 * 32 - 11 || pChr->m_Pos.y < 191 * 32*/) //alle states die mit anfangen zutuen haben nur wenn der bot auch in position steht den part zu machen
							{
								if (pChr->m_FreezeTime == 0 && m_FreezeTime == 0) //wenn beide unfreeze sind zeih auf
									m_DummyRaceState = 0;

								if ((m_Core.m_Pos.x > pChr->m_Pos.x && pChr->m_Pos.y == m_Core.m_Pos.y && m_Core.m_Pos.x > 481 * 32)
									|| (pChr->m_Pos.x > 476 * 32 - 10 && m_Core.m_Pos.x > pChr->m_Pos.x && pChr->m_Pos.y > 191 * 32 - 10 && m_Core.m_Pos.x < 482 * 32 + 10))
								{
									m_DummyRaceState = 1; //starting to do the part->walking left and hammerin'
									if (Server()->Tick() % 30 == 0 && m_DummyNothingHappensCounter == 0)
										SetWeapon(0);
								}

								if ((m_DummyRaceState == 1 && pChr->m_Core.m_Vel.y > 0.5f && pChr->m_Pos.x < 479 * 32) || m_DummyRaceHook)
								{
									m_DummyRaceState = 2; //keep going doing the part->hookin' and walking to the right
									m_DummyRaceHook = true;
								}

								if (m_DummyRaceState == 2 && pChr->m_Pos.x > 485 * 32 + 8)
									m_DummyRaceState = 3; //final stage of doing the part->jumpin' and unfreeze pChr with hammerhit

								if (pChr->m_Pos.x > 489 * 32 || (pChr->m_Pos.x > 486 * 32 && pChr->m_Pos.y < 186 * 32)) //Wenn grad gehammert und der tee weit genugn is spring rein
									m_DummyRaceState = 4;

								if (pChr->m_Pos.y < 191 * 32 && pChr->m_Pos.x < 486 * 32)
									m_DummyRaceHook = false;

								if (m_DummyMateFailed)
									m_DummyRaceState = -1;
							}

							//state=? 5 //extern weil der bot woanders is
							if (m_FreezeTime == 0 && m_Core.m_Pos.x > 485 * 32 && pChr->m_Pos.x < 485 * 32) //wenn der bot rechts und unfreeze is und der mate noch links
								m_DummyRaceState = 5;

							if (m_FreezeTime == 0 && m_Core.m_Pos.x > 490 * 32 && pChr->m_FreezeTime > 0)
								m_DummyRaceState = 6;

							if (m_DummyRaceState == 0) //prepare doing the part (gettin right pos)
								m_Input.m_Direction = 1;
							else if (m_DummyRaceState == 1) //starting to do the part -> walking left and hammerin'
							{
								if (m_Core.m_Pos.x > 480 * 32 - 15) //lauf nach links bis zur hammer pos
									m_Input.m_Direction = -1;

								if (pChr->m_Pos.x < 480 * 32) //wenn pChr weit gwenung zum hammern is
									Fire();

								//testy stop mate if hammer was too hard and mate fly to far
								if (pChr->m_Pos.x < 478 * 32)
									m_Input.m_Hook = 1;
							}
							else if (m_DummyRaceState == 2) //keep going doing the part->hookin' and walking to the right
							{
								if (pChr->m_Pos.y > 194 * 32 + 10)
									m_Input.m_Hook = 1;

								if (pChr->m_Pos.y < 197 * 32)
									m_Input.m_Direction = 1;
							}
							else if (m_DummyRaceState == 3) //final stage of doing the part->jumpin' and unfreeze pChr with hammerhit
							{
								if (Server()->Tick() % 30 == 0)
									SetWeapon(0); //hammer

								if (pChr->m_FreezeTime > 0) //keep holding hook untill pChr is unfreeze
									m_Input.m_Hook = 1;

								//GameServer()->SendChat(m_pPlayer->GetCID(), CGameContext::CHAT_ALL, "debug [3]");
								m_Input.m_Direction = 1;
								m_Input.m_Jump = 1;

								//Now tricky part the unfreeze hammer
								if (pChr->m_Pos.y - m_Core.m_Pos.y < 7 && m_FreezeTime == 0) //wenn der abstand der beiden tees nach oben weniger is als 7 ^^
									Fire();
							}
						}
						
						if (!m_DummyMateFailed && m_DummyRaceState == 4) //PART geschafft! spring ins freeze
						{
							SetWeapon(2);
							m_Input.m_TargetX = 1;
							m_Input.m_TargetY = 1;
							m_LatestInput.m_TargetX = 1;
							m_LatestInput.m_TargetY = 1;

							if (m_Core.m_Pos.y < 195 * 32 && m_Core.m_Pos.x > 478 * 32 - 15) //wenn der bot noch auf der plattform is
							{
								if (m_Core.m_Pos.x < 480 * 32)  //wenn er schon knapp an der kante is
								{
									//nicht zu schnell laufen
									if (Server()->Tick() % 5 == 0)
										m_Input.m_Direction = -1; //geh links bisse füllst
								}
								else
									m_Input.m_Direction = -1;
							}
							else
								m_Input.m_Direction = 1;

							//DJ ins freeze
							if (m_Core.m_Pos.y > 195 * 32 + 10)
								m_Input.m_Jump = 1;

							if (m_Input.m_Direction == 1 && m_FreezeTime == 0)
								Fire();
						}

						if (!m_DummyMateFailed && m_DummyRaceState == 5) //made the part --> help mate
						{

							if (pChr->m_FreezeTime == 0 && pChr->m_Pos.x > 485 * 32)
								m_DummyRaceState = -1;

							if (m_Core.m_Jumped > 1) //double jumped
							{
								if (m_Core.m_Pos.x < 492 * 32 - 30) //zu weit links
								{
									m_Input.m_Direction = 1;

									if (m_Core.m_Pos.x > 488 * 32) //wenn schon knapp dran
									{
										if (m_Core.m_Vel.x < 2.3f)
											m_Input.m_Direction = 0;
									}
								}
							}

							//hold left wall till jump to always do same move with same distance and speed
							if (m_Core.m_Jumped < 2 && !IsGrounded())
								m_Input.m_Direction = -1;

							if (m_Core.m_Pos.y > 195 * 32)
								m_Input.m_Jump = 1;

							if (m_Core.m_Pos.x > 492 * 32)
								m_Input.m_Direction = -1;
						}

						if (m_DummyMateFailed)
						{
							m_DummyHelpNoEmergency = false;

							if (Server()->Tick() % 20 == 0)
								GameServer()->SendEmoticon(m_pPlayer->GetCID(), 7);

							//Go on left edge to help:
							if (m_Core.m_Pos.x > 479 * 32 + 4) //to much right
							{
								if (m_Core.m_Pos.x < 480 * 32 - 25)
								{
									if (Server()->Tick() % 9 == 0)
										m_Input.m_Direction = -1;
								}
								else
								{
									m_Input.m_Direction = -1;
									m_Input.m_TargetX = 300;
									m_Input.m_TargetY = -10;
									m_LatestInput.m_TargetX = 300;
									m_LatestInput.m_TargetY = -10;
								}

								if (m_Core.m_Vel.x < -1.5f && m_Core.m_Pos.x < 480 * 32)
									m_Input.m_Direction = 0;
							}
							else if (m_Core.m_Pos.x < 479 * 32 - 1)
								m_Input.m_Direction = 1;

							if (pChr->m_Pos.x < 478 * 32 && (m_DummyMateHelpMode != 3)) // if currently in yolo fly save mode -> goto else branch to keep yolo flying
							{
								if (Server()->Tick() % 30 == 0)
									SetWeapon(2); //switch to sg

								if (m_FreezeTime == 0 && pChr->m_Core.m_Vel.y == 0.000000f && pChr->m_Core.m_Vel.x < 0.007f && pChr->m_Core.m_Vel.x > -0.007f && m_Core.m_Pos.x < 480 * 32)
									Fire();
							}
							else //if right enough to stop sg
							{
								if (pChr->m_Pos.x < 479 * 32 && (m_DummyMateHelpMode != 3))
								{
									if (pChr->m_Pos.y > 194 * 32)
									{
										if (pChr->m_Core.m_Pos.y > 197 * 32)
											m_Input.m_Hook = 1;

										//reset hook if something went wrong
										if (Server()->Tick() % 90 == 0 && pChr->m_Core.m_Vel.y == 0.000000f) //if the bot shoudl hook but the mate lays on the ground --> resett hook
										{
											m_Input.m_Hook = 0;
											m_DummyNothingHappensCounter++;
											if (m_DummyNothingHappensCounter > 2)
											{
												if (m_Core.m_Pos.x > 478 * 32 - 1 && m_Core.m_Jumped == 0)
													m_Input.m_Direction = -1;

												m_Input.m_TargetX = m_Input.m_TargetX - 5;
												m_LatestInput.m_TargetX = m_LatestInput.m_TargetX - 5;
											}
											if (m_DummyNothingHappensCounter > 4) //warning long time nothing happend! do crazy stuff
											{
												if (m_FreezeTime == 0)
													Fire();
											}
											if (m_DummyNothingHappensCounter > 5) //high warning mate coudl get bored --> swtich through all weapons and move angel back
											{
												SetWeapon(m_DummyPanicWeapon);
												m_DummyPanicWeapon++;
												m_Input.m_TargetX++;
												m_LatestInput.m_TargetX++;
											}
										}
										if (pChr->m_Core.m_Vel.y != 0.000000f)
											m_DummyNothingHappensCounter = 0;
									}
								}
								else
								{
									if (Server()->Tick() % 50 == 0)
										SetWeapon(2);

									if (GetActiveWeapon() == WEAPON_SHOTGUN && m_FreezeTime == 0)
									{
										if (pChr->m_Pos.y < 198 * 32 && (m_DummyMateHelpMode != 3)) //if mate is high enough and not in yolo hook help mode
										{
											m_Input.m_TargetX = -200;
											m_Input.m_TargetY = 30;
											m_LatestInput.m_TargetX = -200;
											m_LatestInput.m_TargetY = 30;
											Fire();
										}
										else //if mate is too low --> change angel or move depnding on the x position
										{
											if (pChr->m_Pos.x < 481 * 32 - 4) //left enough to get him with other shotgun angels from the edge
											{
												//first go on the leftest possible pos on the edge
												if (m_Core.m_Vel.x > -0.0004f && m_Core.m_Pos.x > 478 * 32 - 2 && m_Core.m_Jumped == 0)
													m_Input.m_Direction = -1;

												if (m_DummyMateHelpMode == 0) // start with good mode and increase chance of using it in the rand 0-3 range
													m_DummyMateHelpMode = 3;
												else if (Server()->Tick() % 400 == 0)
													m_DummyMateHelpMode = rand() % 4;

												if (m_DummyMateHelpMode == 3) // 2018 new yolo move with jumping in the air
												{
													if (m_Core.m_Jumped < 2)
													{
														if (m_Core.m_Pos.x > 476 * 32)
														{
															m_Input.m_Direction = -1;
															if (IsGrounded() && m_Core.m_Pos.x < 481 * 32)
																m_Input.m_Jump = 1;

															if (m_Core.m_Pos.x < 477 * 32)
																m_Input.m_Hook = 1; // start hook
															if (m_Core.m_HookState == HOOK_GRABBED)
															{
																m_Input.m_Hook = 1; // hold hook
																m_Input.m_Direction = 1;
																m_Input.m_TargetX = 200;
																m_Input.m_TargetY = 7;
																if (!m_FreezeTime)
																	Fire();
															}
														}

														// anti stuck on edge
														if (Server()->Tick() % 80 == 0)
														{
															if (m_Core.m_Vel.x < 0.0f && m_Core.m_Vel.y < 0.0f)
															{
																m_Input.m_Direction = -1;
																if (m_Core.m_Pos.y > 193 * 32 + 18) // don't jump in the freeze roof
																	m_Input.m_Jump = 1;
																m_Input.m_Hook = 1;
																if (!m_FreezeTime)
																	Fire();
															}
														}
													}
												}
												else if (m_DummyMateHelpMode == 2) //new (jump and wallshot the left wall)
												{
													if (m_Core.m_Pos.y > 193 * 32 && m_Core.m_Vel.y == 0.000000f)
													{
														if (Server()->Tick() % 30 == 0)
															m_Input.m_Jump = 1;
													}

													if (m_Core.m_Pos.y < 191 * 32) //prepare aim
													{
														m_Input.m_TargetX = -300;
														m_Input.m_TargetY = 200;
														m_LatestInput.m_TargetX = -300;
														m_LatestInput.m_TargetY = 200;

														if (m_Core.m_Pos.y < 192 * 32 - 30) //shoot
														{
															if (m_FreezeTime == 0 && GetActiveWeapon() == WEAPON_SHOTGUN && m_Core.m_Vel.y < -0.5f)
																Fire();
														}
													}


													//Panic if fall of platform
													if (m_Core.m_Pos.y > 195 * 32 + 5)
													{
														m_Input.m_Jump = 1;
														m_Input.m_Direction = 1;
														m_Input.m_TargetX = 300;
														m_Input.m_TargetY = -2;
														m_LatestInput.m_TargetX = 300;
														m_LatestInput.m_TargetY = -2;
														m_DummyPanicWhileHelping = true;
													}
													if ((m_Core.m_Pos.x > 480 * 32 && m_FreezeTime == 0) || m_FreezeTime > 0) //stop this mode if the bot made it back to the platform or failed
														m_DummyPanicWhileHelping = false;
													if (m_DummyPanicWhileHelping)
													{
														m_Input.m_Direction = 1;
														m_Input.m_TargetX = 300;
														m_Input.m_TargetY = -2;
														m_LatestInput.m_TargetX = 300;
														m_LatestInput.m_TargetY = -2;
													}

												}
												else if (m_DummyMateHelpMode == 1) //old (shooting straight down from edge and try to wallshot)
												{
													m_Input.m_TargetX = 15;
													m_Input.m_TargetY = 300;
													m_LatestInput.m_TargetX = 15;
													m_LatestInput.m_TargetY = 300;
													if (m_Core.m_Vel.x > -0.1f && m_FreezeTime == 0)
														Fire();

													if (m_Core.m_Pos.y > 195 * 32 + 5)
													{
														m_Input.m_Jump = 1;
														m_Input.m_Direction = 0; //old 1
														m_Input.m_TargetX = 300;
														m_Input.m_TargetY = -2;
														m_LatestInput.m_TargetX = 300;
														m_LatestInput.m_TargetY = -2;
														m_DummyPanicWhileHelping = true;
													}
													if ((m_Core.m_Pos.x > 480 * 32 && m_FreezeTime == 0) || m_FreezeTime > 0) //stop this mode if the bot made it back to the platform or failed
													{
														m_DummyPanicWhileHelping = false;
													}
													if (m_DummyPanicWhileHelping)
													{
														if (m_Core.m_Pos.y < 196 * 32 - 8)
															m_Input.m_Direction = 1;
														else
															m_Input.m_Direction = 0;

														m_Input.m_TargetX = 300;
														m_Input.m_TargetY = -2;
														m_LatestInput.m_TargetX = 300;
														m_LatestInput.m_TargetY = -2;
													}
												}
											}
											else //if mate is far and dummy has to jump of the platform and shotgun him
											{
												m_DummyHelpNoEmergency = true;

												if (Server()->Tick() % 30 == 0)
													SetWeapon(2);

												//go down and jump
												if (m_Core.m_Jumped >= 2) //if bot has no jump
													m_Input.m_Direction = 1;
												else
												{
													m_Input.m_Direction = -1;

													if (m_Core.m_Pos.x < 477 * 32 && m_Core.m_Vel.x < -3.4f) //dont rush too hard intro nowehre 
														m_Input.m_Direction = 0;

													if (m_Core.m_Pos.y > 195 * 32) //prepare aim
													{
														m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
														m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;
														m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
														m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;

														if (m_Core.m_Pos.y > 196 * 32 + 25 || m_Core.m_Pos.x < 475 * 32 + 15)
														{
															Fire();
															m_Input.m_Jump = 1;
														}

														if ((pChr->m_Pos.x < 486 * 32 && m_Core.m_Pos.y > 195 * 32 + 20) || (pChr->m_Pos.x < 486 * 32 && m_Core.m_Pos.x < 477 * 32)) //if mate is in range add a hook
															m_DummyRaceHelpHook = true;
														if (m_Core.m_Pos.x > 479 * 32)
															m_DummyRaceHelpHook = false;

														if (m_DummyRaceHelpHook)
															m_Input.m_Hook = 1;
													}
												}
											}
										}
									}
								}
							}

							if (pChr->m_Pos.x < 475 * 32) // mate failed in left tunnel
							{
								int dist = distance(pChr->m_Pos, m_Core.m_Pos);
								if (dist < 11 * 32)
								{
									m_Input.m_Hook = 1;
									m_DummyMateHelpMode = 3;
									if (Server()->Tick() % 100 == 0) // reset hook to not get stuck
									{
										m_Input.m_Hook = 0;
										if (IsGrounded())
											m_Input.m_Jump = 1;
									}
								}
							}

							if (m_Core.m_Pos.y < pChr->m_Pos.y + 40 && pChr->m_Pos.x < 479 * 32 + 10 && m_FreezeTime == 0) //if the mate is near enough to hammer
								Fire();

							//do something if nothing happens cuz the bot is stuck somehow
							if (Server()->Tick() % 100 == 0 && pChr->m_Core.m_Vel.y == 0.000000f && m_DummyNothingHappensCounter == 0) //if the mate stands still after 90secs the m_DummyNothingHappensCounter should get triggerd. but if not this if function turns true
								m_Input.m_Direction = -1;

							if ((m_Core.m_Pos.y > 195 * 32 && !m_DummyHelpNoEmergency)) //if the bot left the platform
								m_DummyHelpEmergency = true;

							if ((m_Core.m_Pos.x > 479 * 32 && m_Core.m_Jumped == 0) || m_IsFrozen)
								m_DummyHelpEmergency = false;

							if (m_DummyHelpEmergency)
							{
								m_Input.m_Hook = 0;
								m_Input.m_Jump = 0;
								m_Input.m_Direction = 0;
								Fire(false);

								if (Server()->Tick() % 20 == 0)
									GameServer()->SendEmoticon(m_pPlayer->GetCID(), 1);

								m_Input.m_TargetX = 0;
								m_Input.m_TargetY = -200;
								m_LatestInput.m_TargetX = 0;
								m_LatestInput.m_TargetY = -200;

								if (m_Core.m_Pos.y > 194 * 32 + 18)
									m_Input.m_Jump = 1;
								if (m_Core.m_Jumped >= 2)
									m_Input.m_Direction = 1;
							}

							if (m_Core.m_Pos.x < 475 * 32 && m_Core.m_Vel.x < -2.2f)
							{
								if (m_Core.m_Vel.y > 1.1f) // falling -> sg roof
								{
									m_Input.m_TargetX = 10;
									m_Input.m_TargetY = -120;
									if (!m_FreezeTime)
										Fire();
								}
								if (m_Core.m_Pos.y > 193 * 32 + 18) // don't jump in the freeze roof
									m_Input.m_Jump = 1;

								m_Input.m_Direction = 1;
								m_DummyHelpEmergency = true;
							}
						}

						if (m_DummyRaceState == 6)
						{
							m_Input.m_Direction = 0;
							m_Input.m_Jump = 0;
							m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
							m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;
							m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
							m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;

							if (Server()->Tick() % 40 == 0 && m_aWeapons[2].m_Got)
								SetWeapon(2);

							if (m_FreezeTime == 0)
								Fire();
						}
					}
				}

				//Hammerhit with race mate till finish
				if (m_DummyRaceMode == 0 || m_DummyRaceMode == 2) //normal hammerhit
				{
					if (m_Core.m_Pos.x > 491 * 32)
					{
						if (m_Core.m_Pos.x <= 514 * 32 - 5 && pChr->m_Pos.y < 198 * 32)
							SetWeapon(0);

						CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID(), 8);
						if (pChr && pChr->IsAlive())
						{
							if (pChr->m_Pos.x > 490 * 32 + 2) //newly added this to improve the m_DummyRaceState = 5 skills (go on edge if mate made the part)
							{
								m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
								m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;
								m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
								m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;

								if ((m_FreezeTime == 0 && pChr->m_Pos.x < 514 * 32 - 2) || (m_FreezeTime == 0 && pChr->m_Pos.x > 521 * 32 + 2))
								{
									//get right hammer pos [rechte seite]
									if (pChr->m_Pos.x < 515 * 32) //wenn der mate links des ziels ist
									{
										if (m_Core.m_Pos.x > pChr->m_Pos.x + 45) //zu weit rechts von hammer position
											m_Input.m_Direction = -1;
										else if (m_Core.m_Pos.x < pChr->m_Pos.x + 39) // zu weit links von hammer position
											m_Input.m_Direction = 1;
									}
									else //get right hammer pos [rechte seite] (wenn der mate rechts des ziels is)
									{
										if (m_Core.m_Pos.x > pChr->m_Pos.x - 45) //zu weit links von hammer position
											m_Input.m_Direction = -1;
										else if (m_Core.m_Pos.x < pChr->m_Pos.x - 39) // zu weit rechts von hammer position
											m_Input.m_Direction = 1;
									}

									//deactivate bool for hook if mate is high enough or bot is freezed (but freezed is checked somewerhe else)                                                                                                                                                                              oder wenn der mate unter dem bot ist und unfreeze
									if ((pChr->m_FreezeTime == 0 && pChr->m_Core.m_Vel.y > -1.5f && m_Core.m_Pos.y > pChr->m_Pos.y - 15) || pChr->m_Core.m_Vel.y > 3.4f || (pChr->m_FreezeTime == 0 && pChr->m_Pos.y + 38 > m_Core.m_Pos.y) || m_IsFrozen)
										m_DummyHook = false;

									//activate bool for hook if mate stands still
									if (pChr->m_Core.m_Vel.y == 0.000000f)
										m_DummyHook = true;

									if (m_DummyHook)
										m_Input.m_Hook = 1;

									//jump if too low && if mate is freeze otherwise it woudl be annoying af
									if (m_Core.m_Pos.y > 191 * 32 && pChr->m_FreezeTime > 0)
										m_Input.m_Jump = 1;

									//Hammer
									if (pChr->m_FreezeTime > 0 && pChr->m_Pos.y - m_Core.m_Pos.y < 18) //wenn der abstand kleiner als 10 is nach oben
										Fire();
								}
								else
									m_DummyHook = false; //reset hook if bot is freeze

								//ReHook if mate flys to high
								if ((pChr->m_Pos.y < m_Core.m_Pos.y - 40 && pChr->m_Core.m_Vel.y < -4.4f) || pChr->m_Pos.y < 183 * 32)
									m_Input.m_Hook = 1;

								if (pChr->m_FreezeTime == 0 || m_IsFrozen || pChr->m_Pos.x > 512 * 32 + 5) //if mate gets unfreezed or dummy freezed stop balance
									m_DummyPanicBalance = false;

								if (m_DummyPanicBalance)
								{
									if (m_Core.m_Pos.x < pChr->m_Pos.x - 2) //Bot is too far left
										m_Input.m_Direction = 1;
									else if (m_Core.m_Pos.x > pChr->m_Pos.x) //Bot is too far right
										m_Input.m_Direction = -1;

									if (m_Core.m_Pos.x > pChr->m_Pos.x - 2 && m_Core.m_Pos.x < pChr->m_Pos.x && m_Core.m_Vel.x > -0.3f && m_FreezeTime == 0)
									{
										m_Input.m_Direction = 1;
										Fire();
									}
								}

								//Go in finish if near enough
								if ((m_Core.m_Vel.y < 4.4f && m_Core.m_Pos.x > 511 * 32) || (m_Core.m_Vel.y < 8.4f && m_Core.m_Pos.x > 512 * 32))
								{
									if (m_Core.m_Pos.x < 514 * 32 && !m_DummyPanicBalance)
										m_Input.m_Direction = 1;
								}

								//If dummy made it till finish but mate is still freeze on the left side
								//he automaiclly help. BUT if he fails the hook resett it!
								//left side                                                                                      right side
								if ((m_Core.m_Pos.x > 514 * 32 - 5 && m_FreezeTime == 0 && pChr->m_IsFrozen && pChr->m_Pos.x < 515 * 32) || (m_Core.m_Pos.x > 519 * 32 - 5 && m_FreezeTime == 0 && pChr->m_IsFrozen && pChr->m_Pos.x < 523 * 32))
								{
									if (Server()->Tick() % 70 == 0)
										m_Input.m_Hook = 0;
								}
								//if mate is too far for hook --> shotgun him
								if (m_Core.m_Pos.x > 514 * 32 - 5 && m_Core.m_Pos.x > pChr->m_Pos.x && m_Core.m_Pos.x - pChr->m_Pos.x > 8 * 32)
								{
									SetWeapon(2); //shotgun
									if (pChr->m_FreezeTime > 0 && m_FreezeTime == 0 && pChr->m_Core.m_Vel.y == 0.000000f)
										Fire();
								}
								//another hook if normal hook doesnt work
								//to save mate if bot is finish
								if (m_Input.m_Hook == 0)
								{
									if (pChr->m_FreezeTime > 0 && m_FreezeTime == 0 && m_Core.m_Pos.y < pChr->m_Pos.y - 60 && m_Core.m_Pos.x > 514 * 32 - 5)
									{
										m_Input.m_Hook = 1;
										Fire();
										if (Server()->Tick() % 10 == 0)
											GameServer()->SendEmoticon(m_pPlayer->GetCID(), 1);
									}
								}

								//Important dont walk of finish plattform check
								//if (m_Core.m_Vel.y < 6.4f) //Check if not falling to fast
								if (!m_DummyPanicBalance)
								{
									if ((m_Core.m_Vel.y < 6.4f && m_Core.m_Pos.x > 512 * 32 && m_Core.m_Pos.x < 515 * 32)
										|| (m_Core.m_Pos.x > 512 * 32 + 30 && m_Core.m_Pos.x < 515 * 32)) //left side
									{
										m_Input.m_Direction = 1;
									}

									if (m_Core.m_Vel.y < 6.4f && m_Core.m_Pos.x > 520 * 32 && m_Core.m_Pos.x < 524 * 32 /* || too lazy rarly needed*/) //right side
										m_Input.m_Direction = -1;
								}
							}
						}
					}
				}
				else if (m_DummyRaceMode == 1) //tricky hammerhit (harder)
				{
					if (m_Core.m_Pos.x > 491 * 32)
					{
						SetWeapon(0);
						CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID());
						if (pChr && pChr->IsAlive())
						{
							m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
							m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;
							m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
							m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;

							//just do things if unffr
							if (m_FreezeTime == 0 && pChr->m_FreezeTime > 0) //und der mate auch freeze is (damit der nich von edges oder aus dem ziel gehookt wird)
							{
								//get right hammer pos [rechte seite]
								if (m_Core.m_Pos.x > pChr->m_Pos.x + 45)
									m_Input.m_Direction = -1;
								else if (m_Core.m_Pos.x < pChr->m_Pos.x + 39)
									m_Input.m_Direction = 1;

								if ((pChr->m_FreezeTime == 0 && pChr->m_Core.m_Vel.y < -2.5f && pChr->m_Pos.y < m_Core.m_Pos.y) || pChr->m_Core.m_Vel.y > 3.4f)
									m_DummyHook = false;

								//activate bool for hook if mate stands still
								if (pChr->m_Core.m_Vel.y == 0.000000f)
									m_DummyHook = true;

								//jump if too low && if mate is freeze otherwise it woudl be annoying af
								if (m_Core.m_Pos.y > 191 * 32 && pChr->m_FreezeTime > 0)
									m_Input.m_Jump = 1;

								if (pChr->m_FreezeTime > 0 && pChr->m_Pos.y - m_Core.m_Pos.y < 18)
									Fire();
							}
							else
								m_DummyHook = false;
						}
					}

					if (m_DummyHook)
						m_Input.m_Hook = 1;
				}
			}

			if (m_FreezeTime > 0)
				m_Input.m_Hook = 0;
		}
	}
	else if (m_pPlayer->GetDummyMode() == DUMMYMODE_CHILLBLOCK5_BLOCKER) //chillblock5 blocker //made by chillerdragon, cleaned up as much as possible by fokkonaut
	{
		if (m_DummyBoredCounter > 2)
		{
			CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID(), 2);
			if (pChr && pChr->IsAlive())
			{
				//
			}
			else //no ruler alive
				m_DummyLockBored = true;
		}
		else
			m_DummyLockBored = false;

		if (m_DummyLockBored)
		{
			if (m_Core.m_Pos.x < 429 * 32 && IsGrounded())
				m_DummyBored = true;
		}

		if (m_Core.m_Pos.y > 214 * 32 && m_Core.m_Pos.x > 424 * 32)
		{
			CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID(), 3);
			if (pChr && pChr->IsAlive())
				m_DummyBlockMode = 1;
		}
		else if (m_DummyBored)
			m_DummyBlockMode = 2;
		else if (m_DummySpecialDefend) //Check mode 3 [Attack from tunnel wayblocker]
			m_DummyBlockMode = 3;
		else
			m_DummyBlockMode = 0; //change to main mode

		//Modes:
		if (m_DummyBlockMode == 3) //special defend mode
		{
			//testy wenn der dummy in den special defend mode gesetzt wird pusht das sein adrenalin und ihm is nicht mehr lw
			m_DummyBoredCounter = 0;

			CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID(), 2);
			if (pChr && pChr->IsAlive())
			{
				m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
				m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;
				m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
				m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;

				//rest on tick
				m_Input.m_Hook = 0;
				m_Input.m_Jump = 0;
				m_Input.m_Direction = 0;
				Fire(false);
				SetWeapon(1); //gun verwenden damit auch erkannt wird wann der mode getriggert wird

				if (pChr->m_FreezeTime == 0)
				{
					//wenn der gegner doch irgendwie unfreeze wird übergib an den main mode und lass den notstand das regeln
					m_DummySpecialDefend = false;
					m_DummySpecialDefendAttack = false;
				}

				if (pChr->m_Core.m_Vel.y > -0.9f && pChr->m_Pos.y > 211 * 32)
				{
					//wenn der gegner am boden liegt starte angriff
					m_DummySpecialDefendAttack = true;

					//start jump
					m_Input.m_Jump = 1;
				}

				if (m_DummySpecialDefendAttack)
				{
					if (m_Core.m_Pos.x - pChr->m_Pos.x < 50) //wenn der gegner nah genung is mach dj
						m_Input.m_Jump = 1;

					if (pChr->m_Pos.x < m_Core.m_Pos.x)
						m_Input.m_Hook = 1;
					else //wenn der gegner weiter rechts als der bot is lass los und übergib an main deine arbeit ist hier getahen
					{    //main mode wird evenetuell noch korrigieren mit schieben
						m_DummySpecialDefend = false;
						m_DummySpecialDefendAttack = false;
					}

					//Der bot sollte möglichst weit nach rechts gehen aber natürlich nicht ins freeze
					if (m_Core.m_Pos.x < 427 * 32 + 15)
						m_Input.m_Direction = 1;
					else
						m_Input.m_Direction = -1;
				}
			}
			else //wenn kein gegner mehr im Ruler bereich is
			{
				m_DummySpecialDefend = false;
				m_DummySpecialDefendAttack = false;
			}
		}
		else if (m_DummyBlockMode == 2) //different wayblock mode
		{
			//rest on tick
			m_Input.m_Hook = 0;
			m_Input.m_Jump = 0;
			m_Input.m_Direction = 0;
			Fire(false);
			if (Server()->Tick() % 30 == 0)
				SetWeapon(0);

			//Selfkills (bit random but they work)
			if (m_IsFrozen)
			{
				//wenn der bot freeze is warte erstmal n paar sekunden und dann kill dich
				if (Server()->Tick() % 300 == 0)
				{
					Die();
					return;
				}
			}

			CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID(), 4);
			if (pChr && pChr->IsAlive())
			{
				//Check ob an notstand mode18 = 0 übergeben
				if (pChr->m_FreezeTime == 0)
				{
					m_DummyBored = false;
					m_DummyBoredCounter = 0;
					m_DummyBlockMode = 0;
				}

				m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
				m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;
				m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
				m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;

				m_Input.m_Jump = 1;

				if (pChr->m_Pos.y > m_Core.m_Pos.y && pChr->m_Pos.x > m_Core.m_Pos.x + 20) //solange der bot über dem gegner ist (damit er wenn er ihn weg hammert nicht weiter hookt)
					m_Input.m_Hook = 1;

				if (m_Core.m_Pos.x > 420 * 32)

					m_Input.m_Direction = -1;
				if (pChr->m_Pos.y < m_Core.m_Pos.y + 15)
					Fire();
			}
			else //lieblings position finden wenn nichts abgeht
			{
				if (m_Core.m_Pos.x < 423 * 32)
					m_Input.m_Direction = 1;
				else if (m_Core.m_Pos.x > 424 * 32 + 30)
					m_Input.m_Direction = -1;
			}
		}
		else if (m_DummyBlockMode == 1) //attack in tunnel
		{
			//Selfkills (bit random but they work)
			if (m_IsFrozen)
			{
				//wenn der bot freeze is warte erstmal n paar sekunden und dann kill dich
				if (Server()->Tick() % 300 == 0)
				{
					Die();
					return;
				}
			}

			//stay on position
			if (m_Core.m_Pos.x < 426 * 32 + 10) // zu weit links
				m_Input.m_Direction = 1; //geh rechts
			else if (m_Core.m_Pos.x > 428 * 32 - 10) //zu weit rechts
				m_Input.m_Direction = -1; // geh links
			else if (m_Core.m_Pos.x > 428 * 32 + 10) // viel zu weit rechts
			{
				m_Input.m_Direction = -1; // geh links
				m_Input.m_Jump = 1;
			}
			else
			{
				CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID(), 3);
				if (pChr && pChr->IsAlive())
				{
					if (pChr->m_Pos.x < 436 * 32) //wenn er ganz weit über dem freeze auf der kante ist (hooke direkt)
					{
						m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
						m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;
						m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
						m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;
					}
					else //wenn der Gegner weiter hinter dem unhook ist (hook über den Gegner um ihn trozdem zu treffen und das unhook zu umgehen)
					{
						m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x - 50;
						m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;
						m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x - 50;
						m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;
					}

					CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID(), 5);
					if (pChr && pChr->IsAlive())
					{
						//wenn jemand im tunnel is check ob du nicht ausversehen den hookst anstatt des ziels in der WB area
						if (pChr->m_Pos.x < m_Core.m_Pos.x) //hooke nur wenn kein Gegner rechts von dem bot im tunnel is (da er sonst ziemlich wahrscheinlich den hooken würde)
							m_Input.m_Hook = 1;
					}
					else
						m_Input.m_Hook = 1;

					//schau ob sich der gegner bewegt und der bot grad nicht mehr am angreifen iss dann resette falls er davor halt misshookt hat
					//geht nich -.-
					if (m_Core.m_HookState != HOOK_FLYING && m_Core.m_HookState != HOOK_GRABBED)
					{
						if (Server()->Tick() % 10 == 0)
							m_Input.m_Hook = 0;
					}
					if (m_Core.m_Vel.x > 3.0f)
						m_Input.m_Direction = -1;
					else
						m_Input.m_Direction = 0;
				}
				else
					m_DummyBlockMode = 0;
			}
		}
		else if (m_DummyBlockMode == 0) //main mode
		{
			m_Input.m_Jump = 0;
			m_Input.m_Direction = 0;
			Fire(false);

			//Check ob jemand in der linken freeze wand is
			CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID(), 6);
			if (pChr && pChr->IsAlive()) // wenn ein spieler rechts im freeze lebt //----> versuche im notstand nicht den gegner auch da rein zu hauen da ist ja jetzt voll
				m_DummyLeftFreezeFull = true;
			else // wenn da keiner is fülle diesen spot (linke freeze wand im ruler spot)
				m_DummyLeftFreezeFull = false;

			if (m_Core.m_Pos.x > 241 * 32 && m_Core.m_Pos.x < 418 * 32 && m_Core.m_Pos.y > 121 * 32 && m_Core.m_Pos.y < 192 * 32) //new spawn ChillBlock5 (tourna edition (the on with the gores stuff))
			{
				//dieser code wird also nur ausgeführt wenn der bot gerade im neuen bereich ist
				if (m_Core.m_Pos.x > 319 * 32 && m_Core.m_Pos.y < 161 * 32) //top right spawn
				{
					//look up left
					if (m_Core.m_Pos.x < 372 * 32 && m_Core.m_Vel.y > 3.1f)
					{
						m_Input.m_TargetX = -30;
						m_Input.m_TargetY = -80;
					}
					else
					{
						m_Input.m_TargetX = -100;
						m_Input.m_TargetY = -80;
					}

					if (m_Core.m_Pos.x > 331 * 32 && m_IsFrozen)
					{
						Die();
						return;
					}

					if (m_Core.m_Pos.x < 327 * 32) //dont klatsch in ze wand
						m_Input.m_Direction = 1; //nach rechts laufen
					else
						m_Input.m_Direction = -1;

					if (IsGrounded() && m_Core.m_Pos.x < 408 * 32) //initial jump from spawnplatform
						m_Input.m_Jump = 1;

					if (m_Core.m_Pos.x > 330 * 32) //only hook in tunnel and let fall at the end
					{
						if (m_Core.m_Pos.y > 147 * 32 || (m_Core.m_Pos.y > 143 * 32 && m_Core.m_Vel.y > 3.3f)) //gores pro hook up
							m_Input.m_Hook = 1;
						else if (m_Core.m_Pos.y < 143 * 32 && m_Core.m_Pos.x < 372 * 32) //hook down (if too high and in tunnel)
						{
							m_Input.m_TargetX = -42;
							m_Input.m_TargetY = 100;
							m_Input.m_Hook = 1;
						}
					}
				}
				else if (m_Core.m_Pos.x < 317 * 32) //top left spawn
				{
					if (m_Core.m_Pos.y < 158 * 32) //spawn area find down
					{
						//selfkill
						if (m_IsFrozen)
						{
							Die();
							return;
						}

						if (m_Core.m_Pos.x < 276 * 32 + 20) //is die mitte von beiden linken spawns also da wo es runter geht
						{
							m_Input.m_TargetX = 57;
							m_Input.m_TargetY = -100;
							m_Input.m_Direction = 1;
						}
						else
						{
							m_Input.m_TargetX = -57;
							m_Input.m_TargetY = -100;
							m_Input.m_Direction = -1;
						}

						if (m_Core.m_Pos.y > 147 * 32)
						{
							m_Input.m_Hook = 1;
							if (m_Core.m_Pos.x > 272 * 32 && m_Core.m_Pos.x < 279 * 32) //let fall in the hole
							{
								m_Input.m_Hook = 0;
								m_Input.m_TargetX = 2;
								m_Input.m_TargetY = 100;
							}
						}
					}
					else if (m_Core.m_Pos.y > 162 * 32) //managed it to go down --> go left
					{
						//selfkill
						if (m_IsFrozen)
						{
							Die();
							return;
						}

						if (m_Core.m_Pos.x < 283 * 32)
						{
							m_Input.m_TargetX = 200;
							m_Input.m_TargetY = -136;
							if (m_Core.m_Pos.y > 164 * 32 + 10)
								m_Input.m_Hook = 1;
						}
						else
						{
							m_Input.m_TargetX = 80;
							m_Input.m_TargetY = -100;
							if (m_Core.m_Pos.y > 171 * 32 - 10)
								m_Input.m_Hook = 1;
						}

						m_Input.m_Direction = 1;
					}
				}
				else //lower end area of new spawn --> entering old spawn by walling and walking right
				{
					m_Input.m_Direction = 1;
					m_Input.m_TargetX = 200;
					m_Input.m_TargetY = -84;

					//Selfkills
					if (m_IsFrozen && IsGrounded()) //should never lie in freeze at the ground
					{
						Die();
						return;
					}
					if (m_Core.m_Pos.y < 166 * 32 - 20)
						m_Input.m_Hook = 1;

					if (m_Core.m_Pos.x > 332 * 32 && m_Core.m_Pos.x < 337 * 32 && m_Core.m_Pos.y > 182 * 32) //wont hit the wall --> jump
						m_Input.m_Jump = 1;

					if (m_Core.m_Pos.x > 336 * 32 + 20 && m_Core.m_Pos.y > 180 * 32) //stop moving if walled
						m_Input.m_Direction = 0;
				}
			}
			else
			{
				if (m_Core.m_Pos.x < 390 * 32 && m_Core.m_Pos.x > 325 * 32 && m_Core.m_Pos.y > 215 * 32)  //Links am spawn runter
				{
					Die();
					return;
				}
				else if (m_Core.m_Pos.y < 215 * 32 && m_Core.m_Pos.y > 213 * 32 && m_Core.m_Pos.x > 415 * 32 && m_Core.m_Pos.x < 428 * 32) //freeze decke im tunnel
				{
					Die();
					return;
				}
				else if (m_Core.m_Pos.y > 222 * 32) //freeze becken unter area
				{
					Die();
					return;
				}
				if (m_Core.m_Pos.y < 220 * 32 && m_Core.m_Pos.x < 415 * 32 && m_FreezeTime > 1 && m_Core.m_Pos.x > 352 * 32) //always suicide on freeze if not reached teh block area yet AND dont suicide in spawn area because new spawn sys can get pretty freezy
				{
					Die();
					return;
				}
				//new spawn do something agianst hookers 
				if (m_Core.m_Pos.x < 380 * 32 && m_Core.m_Pos.x > 322 * 32 && m_Core.m_Vel.x < -0.001f)
				{
					m_Input.m_Hook = 1;
					if ((m_Core.m_Pos.x < 362 * 32 && IsGrounded()) || m_Core.m_Pos.x < 350 * 32)
					{
						if (Server()->Tick() % 10 == 0)
						{
							m_Input.m_Jump = 1;
							SetWeapon(0);
						}
					}
				}
				if (m_Core.m_Pos.x < 415 * 32)
				{
					CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID());
					if (pChr && pChr->IsAlive())
					{
						if (pChr->m_Core.m_Pos.x > m_Core.m_Pos.x - 100 && pChr->m_Core.m_Pos.x < m_Core.m_Pos.x + 100 && pChr->m_Core.m_Pos.y > m_Core.m_Pos.y - 100 && pChr->m_Core.m_Pos.y < m_Core.m_Pos.y + 100)
						{
							if (pChr->m_Core.m_Vel.y < -1.5f) //only boost and use existing up speed
								Fire();
							if (Server()->Tick() % 3 == 0)
								SetWeapon(0);
						}
						//old spawn do something agianst way blockers (roof protection)
						if (m_Core.m_Pos.y > 206 * 32 + 4 && m_Core.m_Pos.y < 208 * 32 && m_Core.m_Vel.y < -4.4f)
							m_Input.m_Jump = 1;

						//old spawn roof protection / attack hook
						if (pChr->m_Core.m_Pos.y > m_Core.m_Pos.y + 50)
							m_Input.m_Hook = 1;
					}
				}

				if (m_Core.m_Pos.x < 388 * 32 && m_Core.m_Pos.y > 213 * 32) //jump to old spawn
				{
					m_Input.m_Jump = 1;
					Fire();
					m_Input.m_Hook = 1;
					m_Input.m_TargetX = -200;
					m_Input.m_TargetY = 0;
				}

				if (!m_DummyPlannedMovement)
				{
					CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID(), 5);
					if (pChr && pChr->IsAlive())
					{
						if (pChr->m_Core.m_Vel.x < 3.3f) //found a slow bob in tunnel
							m_DummyMovementToBlockArea = true;
					}

					m_DummyPlannedMovement = true;
				}

				if (m_DummyMovementToBlockArea)
				{
					if (m_Core.m_Pos.x < 415 * 32)
					{
						m_Input.m_Direction = 1;

						if (m_Core.m_Pos.x > 404 * 32 && IsGrounded())
							m_Input.m_Jump = 1;

						if (m_Core.m_Pos.y < 208 * 32)
							m_Input.m_Jump = 1;

						if (m_Core.m_Pos.x > 410 * 32)
						{
							m_Input.m_TargetX = 200;
							m_Input.m_TargetY = 70;
							if (m_Core.m_Pos.x > 413 * 32)
								m_Input.m_Hook = 1;
						}
					}
					else //not needed but safty xD when the bot managed it to get into the ruler area change to old movement
						m_DummyMovementToBlockArea = false;

					//something went wrong:
					if (m_Core.m_Pos.y > 214 * 32)
					{
						m_Input.m_Jump = 1;
						m_DummyMovementToBlockArea = false;
					}
				}
				else //down way
				{
					//CheckFatsOnSpawn
					if (m_Core.m_Pos.x < 406 * 32)
					{
						CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID());
						if (pChr && pChr->IsAlive())
						{
							m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
							m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;
							m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
							m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;


							if (pChr->m_Pos.x < 407 * 32 && pChr->m_Pos.y > 212 * 32 && pChr->m_Pos.y < 215 * 32 && pChr->m_Pos.x > m_Core.m_Pos.x) //wenn ein im weg stehender tee auf der spawn plattform gefunden wurde
							{
								SetWeapon(0); //hol den hammer raus!
								if (pChr->m_Pos.x - m_Core.m_Pos.x < 30) //wenn der typ nahe bei dem bot ist
								{
									if (m_FreezeTick == 0) //nicht rum schrein
										Fire();

									if (Server()->Tick() % 10 == 0)
										GameServer()->SendEmoticon(m_pPlayer->GetCID(), 9); //angry
								}
							}
							else
							{
								if (Server()->Tick() % 20 == 0)
									SetWeapon(1); //pack den hammer weg
							}
						}
					}

					//Check attacked on spawn
					if (m_Core.m_Pos.x < 412 * 32 && m_Core.m_Pos.y > 217 * 32 && m_Core.m_Vel.x < -0.5f)
					{
						m_Input.m_Jump = 1;
						m_DummyAttackedOnSpawn = true;
					}
					if (IsGrounded())
						m_DummyAttackedOnSpawn = false;

					if (m_DummyAttackedOnSpawn)
					{
						if (Server()->Tick() % 100 == 0) //this shitty stuff can set it right after activation to false but i dont care
							m_DummyAttackedOnSpawn = false;
					}

					if (m_DummyAttackedOnSpawn)
					{
						int r = rand() % 88;

						if (r > 44)
							m_Input.m_Fire++;

						int rr = rand() % 1337;
						if (rr > 420)
							SetWeapon(0);

						CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID());
						if (pChr && pChr->IsAlive())
						{
							int r = rand() % 10 - 10;

							m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
							m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y - r;

							if (Server()->Tick() % 13 == 0)
								GameServer()->SendEmoticon(m_pPlayer->GetCID(), 9);

							if (m_Core.m_HookState == HOOK_GRABBED || (m_Core.m_Pos.y < 216 * 32 && pChr->m_Pos.x > 404 * 32) || (pChr->m_Pos.x > 405 * 32 && m_Core.m_Pos.x > 404 * 32 + 20))
							{
								m_Input.m_Hook = 1;
								if (Server()->Tick() % 10 == 0)
								{
									int x = rand() % 20;
									int y = rand() % 20 - 10;
									m_Input.m_TargetX = x;
									m_Input.m_TargetY = y;
								}
							}
						}
					}

					//CheckSlowDudesInTunnel
					if (m_Core.m_Pos.x > 415 * 32 && m_Core.m_Pos.y > 214 * 32) //wenn bot im tunnel ist
					{
						CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID(), 5);
						if (pChr && pChr->IsAlive())
						{
							if (pChr->m_Core.m_Vel.x < 7.8f) //wenn der nächste spieler im tunnel ein slowdude is 
							{
								//HauDenBau
								SetWeapon(0); //hol den hammer raus!

								m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
								m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;
								m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
								m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;

								if (m_FreezeTick == 0) //nicht rum schrein
									Fire();

								if (Server()->Tick() % 10 == 0)  //angry emotes machen
									GameServer()->SendEmoticon(m_pPlayer->GetCID(), 9);
							}
						}
					}

					//CheckSpeedInTunnel
					if (m_Core.m_Pos.x > 425 * 32 && m_Core.m_Pos.y > 214 * 32 && m_Core.m_Vel.x < 9.4f) //wenn nich genung speed zum wb spot springen
						m_DummyGetSpeed = true;

					if (m_DummyGetSpeed) //wenn schwung holen == true (tunnel)
					{
						if (m_Core.m_Pos.x > 422 * 32) //zu weit rechts
						{
							//---> hol schwung für den jump
							m_Input.m_Direction = -1;

							//new hammer agressive in the walkdirection to free the way
							if (m_FreezeTime == 0)
							{
								m_Input.m_TargetX = -200;
								m_Input.m_TargetY = -2;
								if (Server()->Tick() % 20 == 0)
									SetWeapon(0);
								if (Server()->Tick() % 25 == 0)
									Fire();
							}
						}
						else //wenn weit genung links
							m_DummyGetSpeed = false;
					}
					else
					{
						if (m_Core.m_Pos.x < 415 * 32) //bis zum tunnel laufen
							m_Input.m_Direction = 1;
						else if (m_Core.m_Pos.x < 440 * 32 && m_Core.m_Pos.y > 213 * 32) //im tunnel laufen
							m_Input.m_Direction = 1;

						//externe if abfrage weil laufen während sprinegn xD
						if (m_Core.m_Pos.x > 413 * 32 && m_Core.m_Pos.x < 415 * 32) // in den tunnel springen
							m_Input.m_Jump = 1;
						else if (m_Core.m_Pos.x > 428 * 32 - 20 && m_Core.m_Pos.y > 213 * 32) // im tunnel springen
							m_Input.m_Jump = 1;

						if (m_Core.m_Pos.x > 428 * 32 && m_Core.m_Pos.y > 213 * 32) // im tunnel springen nicht mehr springen
							m_Input.m_Jump = 0;

						//nochmal extern weil springen während springen
						if (m_Core.m_Pos.x > 430 * 32 && m_Core.m_Pos.y > 213 * 32) // im tunnel springen springen
							m_Input.m_Jump = 1;

						if (m_Core.m_Pos.x > 431 * 32 && m_Core.m_Pos.y > 213 * 32) //jump refillen für wayblock spot
							m_Input.m_Jump = 0;
					}
				}

				if (!m_IsFrozen || m_Core.m_Vel.x < -0.5f || m_Core.m_Vel.x > 0.5f || m_Core.m_Vel.y != 0.000000f)
				{
					//mach nichts
				}
				else
				{
					if (Server()->Tick() % 150 == 0)
					{
						Die();
						return;
					}
				}

				m_DummyEmergency = false;

				//normaler internen wb spot stuff
				//if (m_Core.m_Pos.y < 213 * 32) //old new added a x check idk why the was no
				if (m_Core.m_Pos.y < 213 * 32 && m_Core.m_Pos.x > 415 * 32)
				{
					CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID(), 2);
					if (pChr && pChr->IsAlive())
					{
						//sometimes walk to enemys.   to push them in freeze or super hammer them away
						if (pChr->m_Pos.y < m_Core.m_Pos.y + 2 && pChr->m_Pos.y > m_Core.m_Pos.y - 9)
						{
							if (pChr->m_Pos.x > m_Core.m_Pos.x)
								m_Input.m_Direction = 1;
							else
								m_Input.m_Direction = -1;
						}

						if (pChr->m_FreezeTime == 0) //if enemy in ruler spot is unfreeze -->notstand panic
						{
							if (Server()->Tick() % 30 == 0)  //angry emotes machen
								GameServer()->SendEmoticon(m_pPlayer->GetCID(), 9);

							if (Server()->Tick() % 20 == 0)
								SetWeapon(0);

							m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
							m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;
							m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
							m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;

							if (m_FreezeTime == 0)
								Fire();

							m_DummyEmergency = true;

							if (!m_DummyLeftFreezeFull)
							{
								if (!m_DummyJumped)
								{
									m_Input.m_Jump = 1;
									m_DummyJumped = true;
								}
								else
									m_Input.m_Jump = 0;

								if (!m_DummyHooked)
								{
									if (Server()->Tick() % 30 == 0)
										m_DummyHookDelay = true;

									if (m_DummyHookDelay)
									{
										if (Server()->Tick() % 200 == 0)
										{
											m_DummyHooked = true;
											m_Input.m_Hook = 0;
										}
									}
								}

								if (!m_DummyMovedLeft)
								{
									if (m_Core.m_Pos.x > 419 * 32 + 20)
										m_Input.m_Direction = -1;
									else
										m_Input.m_Direction = 1;

									if (Server()->Tick() % 200 == 0)
									{
										m_DummyMovedLeft = true;
										m_Input.m_Direction = 0;
									}
								}
							}

							//Blocke gefreezte gegner für immer 
							if (pChr->m_FreezeTime > 0 && pChr->m_Pos.y > 204 * 32 && pChr->m_Pos.x > 422 * 32) //wenn ein gegner weit genung rechts freeze am boden liegt
							{
								if (m_Core.m_Pos.x + (5 * 32 + 40) < pChr->m_Pos.x) // er versucht 5 tiles links des gefreezten gegner zu kommen
								{
									m_Input.m_Direction = -1;

									if (m_Core.m_Pos.x > pChr->m_Pos.x && m_Core.m_Pos.x < pChr->m_Pos.x + (4 * 32)) // wenn er 4 tiles rechts des gefreezten gegners is
										m_Input.m_Jump = 1;
								}
								else //wenn der bot links des gefreezten spielers is
								{
									m_Input.m_Jump = 1;
									m_Input.m_Direction = -1;

									if (m_Core.m_Pos.x < pChr->m_Pos.x) //solange der bot weiter links is
										m_Input.m_Hook = 1;
									else
										m_Input.m_Hook = 0;
								}
							}

							if ((pChr->m_Pos.x + 10 < m_Core.m_Pos.x && pChr->m_Pos.y > 211 * 32 && pChr->m_Pos.x < 418 * 32)
								|| (pChr->m_FreezeTime > 0 && pChr->m_Pos.y > 210 * 32 && pChr->m_Pos.x < m_Core.m_Pos.x && pChr->m_Pos.x > 417 * 32 - 60)) // wenn der spieler neben der linken wand linken freeze wand liegt schiebt ihn der bot rein
							{
								if (!m_DummyLeftFreezeFull) //wenn da niemand is schieb den rein
								{
									// HIER TESTY TESTY CHANGES  211 * 32 + 40 stand hier
									if (pChr->m_Pos.y > 211 * 32 + 40) // wenn der gegner wirklich ganz tief genung is
									{
										if (m_Core.m_Pos.x > 418 * 32) // aber nicht selber ins freeze laufen
										{
											m_Input.m_Direction = -1;

											//Check ob der gegener freeze is
											if (pChr->m_FreezeTime > 0)
												Fire(false);

											//letzten stupser geben (sonst gibs bugs kb zu fixen)
											if (pChr->m_IsFrozen) //wenn er schon im freeze is
												Fire();
										}
										else
										{
											m_Input.m_Direction = 1;
											if (pChr->m_FreezeTime > 0)
												Fire(false);
										}
									}
									else //wenn der gegner nicht tief genung ist
									{
										m_Input.m_Direction = 1;

										if (pChr->m_FreezeTime > 0)
											Fire(false);
									}
								}
							}
							else if (m_Core.m_Pos.x < 419 * 32 + 10) //sonst mehr abstand halten
								m_Input.m_Direction = 1;

							if (m_Core.m_Vel.y < 20.0f && m_Core.m_Pos.y < 207 * 32)  // wenn der tee nach oben gehammert wird
							{
								if (m_Core.m_Pos.y > 206 * 32) //ab 206 würd er so oder so ins freeze springen
									m_Input.m_Jump = 1;

								if (m_Core.m_Pos.y < pChr->m_Pos.y) //wenn der bot über dem spieler is soll er hooken
									m_Input.m_Hook = 1;
								else
									m_Input.m_Hook = 0;
							}

							if (m_Core.m_Pos.y < 207 * 32 && m_Core.m_Pos.y < pChr->m_Pos.y)
							{
								//in hammer position bewegen
								if (m_Core.m_Pos.x > 418 * 32 + 20) //nicht ins freeze laufen
								{
									if (m_Core.m_Pos.x > pChr->m_Pos.x - 45) //zu weit rechts von hammer position
										m_Input.m_Direction = -1; //gehe links
									else if (m_Core.m_Pos.x < pChr->m_Pos.x - 39) // zu weit links von hammer position
										m_Input.m_Direction = 1;
									else  //im hammer bereich
										m_Input.m_Direction = 0; //bleib da
								}
							}

							//Check ob der gegener freeze is
							if (pChr->m_FreezeTime > 0 && pChr->m_Pos.y > 208 * 32 && !pChr->m_IsFrozen) //wenn der Gegner tief und freeze is macht es wenig sinn den frei zu hammern
								Fire(false);

							//Hau den weg (wie dummymode 21)
							if (pChr->m_Pos.x > 418 * 32 && pChr->m_Pos.y > 209 * 32)  //das ganze findet nur im bereich statt wo sonst eh nichts passiert
							{
								//wenn der bot den gegner nicht boosten würde hammer den auch nich weg
								Fire(false);

								if (pChr->m_Core.m_Vel.y < -0.5f && m_Core.m_Pos.y + 15 > pChr->m_Pos.y) //wenn der dude speed nach oben hat
								{
									m_Input.m_Jump = 1;
									if (m_FreezeTime == 0)
										Fire();
								}
							}

							if (pChr->m_Pos.x > 421 * 32 && pChr->m_FreezeTick > 0 && pChr->m_Pos.x < m_Core.m_Pos.x)
							{
								m_Input.m_Jump = 1;
								m_Input.m_Hook = 1;
							}

							//during the fight dodge the freezefloor on the left with brain
							if (m_Core.m_Pos.x > 428 * 32 + 20 && m_Core.m_Pos.x < 438 * 32 - 20)
							{
								//very nobrain directions decision
								if (m_Core.m_Pos.x < 432 * 32) //left --> go left
									m_Input.m_Direction = -1;
								else //right --> go right
									m_Input.m_Direction = 1;

								//high speed left goto speed
								if (m_Core.m_Vel.x < -6.4f && m_Core.m_Pos.x < 434 * 32)
									m_Input.m_Direction = -1;

								//low speed to the right
								if (m_Core.m_Pos.x > 431 * 32 + 20 && m_Core.m_Vel.x > 3.3f)
									m_Input.m_Direction = 1;
							}
						}
						else //sonst normal wayblocken und 427 aufsuchen
						{
							SetWeapon(1);
							m_Input.m_Jump = 0;

							if (m_Core.m_HookState == HOOK_FLYING)
								m_Input.m_Hook = 1;
							else if (m_Core.m_HookState == HOOK_GRABBED)
								m_Input.m_Hook = 1;
							else
								m_Input.m_Hook = 0;

							m_DummyJumped = false;
							m_DummyHooked = false;
							m_DummyMovedLeft = false;

							if (m_Core.m_Pos.x > 428 * 32 + 15 && m_DummyRuled) //wenn viel zu weit ausserhalb der ruler area wo der bot nur hingehookt werden kann
							{
								m_Input.m_Jump = 1;
								m_Input.m_Hook = 1;
							}

							//GameServer()->SendChat(m_pPlayer->GetCID(), CGameContext::CHAT_ALL, "Prüfe ob zu weit rechts");
							if (m_Core.m_Pos.x < (418 * 32) - 10) // zu weit links -> geh rechts
								m_Input.m_Direction = 1;
							else if (m_Core.m_Pos.x >(428 * 32) + 10) // zu weit rechts -> geh links
								m_Input.m_Direction = -1;
							else // im toleranz bereich -> stehen bleiben
							{
								m_DummyRuled = true;
								m_Input.m_Direction = 0;

								// normal wayblock
								CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID(), 2);
								if (pChr && pChr->IsAlive())
								{
									//Trick[4] clears the left freeze
									if (pChr->m_Pos.x < 418 * 32 - 10 && pChr->m_Pos.y > 210 * 32 && pChr->m_Pos.y < 213 * 32 && pChr->m_IsFrozen && pChr->m_Core.m_Vel.y == 0.00f)
										m_DummyFreezeBlockTrick = 4;

									if (pChr->m_Pos.y < 213 * 32 + 10 && pChr->m_Pos.x < 430 * 32 && pChr->m_Pos.y > 210 * 32 && pChr->m_Pos.x > 417 * 32) // wenn ein spieler auf der linken seite in der ruler area is 
									{
										//wenn ein gegner rechts des bots is prepare für trick[1]
										if (m_Core.m_Pos.x < pChr->m_Pos.x && pChr->m_Pos.x < 429 * 32 + 6)
										{
											m_Input.m_Direction = -1;
											m_Input.m_Jump = 0;

											if (m_Core.m_Pos.x < 422 * 32)
											{
												m_Input.m_Jump = 1;
												m_DummyFreezeBlockTrick = 1;
											}
										}
										//wenn ein gegner links des bots is prepare für tick[3]
										if (m_Core.m_Pos.x > pChr->m_Pos.x && pChr->m_Pos.x < 429 * 32)
										{
											m_Input.m_Direction = 1;
											m_Input.m_Jump = 0;

											if (m_Core.m_Pos.x > 427 * 32 || m_Core.m_Pos.x > pChr->m_Pos.x + (5 * 32))
											{
												m_Input.m_Jump = 1;
												m_DummyFreezeBlockTrick = 3;
											}
										}
									}
									else // wenn spieler irgendwo anders is
									{
										//wenn ein spieler rechts des freezes is und freeze is Tric[2]
										if (pChr->m_Pos.x > 433 * 32 && pChr->m_FreezeTime > 0 && IsGrounded())
											m_DummyFreezeBlockTrick = 2;
									}
								}
							}
						}
					}
					else // wenn kein lebender spieler im ruler spot ist
					{
						//Suche trozdem 427 auf
						if (m_Core.m_Pos.x < (427 * 32) - 20) // zu weit links -> geh rechts
							m_Input.m_Direction = 1;
						else if (m_Core.m_Pos.x >(427 * 32) + 40) // zu weit rechts -> geh links
							m_Input.m_Direction = -1;
					}

					// über das freeze springen wenn rechts der bevorzugenten camp stelle
					if (m_Core.m_Pos.x > 434 * 32)
						m_Input.m_Jump = 1;
					else if (m_Core.m_Pos.x > (434 * 32) - 20 && m_Core.m_Pos.x < (434 * 32) + 20) // bei flug über freeze jump wd holen
						m_Input.m_Jump = 0;

					//new testy moved tricks into Wayblocker area (y < 213 && x > 215) (was external)
					//TRICKS
					if (1 == 1)
					{
						CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID(), 2);
						if (pChr && pChr->IsAlive())
						{
							if (!m_DummyEmergency && m_Core.m_Pos.x > 415 && m_Core.m_Pos.y < 213 * 32 && m_DummyFreezeBlockTrick != 0) //as long as no enemy is unfreeze in base --->  do some trickzz
							{
								//off tricks when not gud to make tricks#
								if (pChr->m_FreezeTime == 0)
								{
									m_DummyFreezeBlockTrick = 0;
									m_DummyPanicDelay = 0;
									m_DummyTrick3Check = false;
									m_DummyTrick3StartCount = false;
									m_DummyTrick3Panic = false;
									m_DummyTrick4HasStartPos = false;
								}

								if (m_DummyFreezeBlockTrick == 1) //Tick[1] enemy on the right
								{
									if (pChr->m_IsFrozen)
										m_DummyFreezeBlockTrick = 0; //stop trick if enemy is in freeze

									m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
									m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;
									m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
									m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;

									if (Server()->Tick() % 40 == 0)
										SetWeapon(0);

									if (pChr->m_Pos.x < m_Core.m_Pos.x && pChr->m_Pos.x > m_Core.m_Pos.x - 180) //if enemy is on the left in hammer distance
										Fire();

									if (m_Core.m_Pos.y < 210 * 32 + 10)
										m_DummyStartHook = true;

									if (m_DummyStartHook)
									{
										if (Server()->Tick() % 80 == 0 || pChr->m_Pos.x < m_Core.m_Pos.x + 22)
											m_DummyStartHook = false;
									}


									if (m_DummyStartHook)
										m_Input.m_Hook = 1;
									else
										m_Input.m_Hook = 0;
								}
								else if (m_DummyFreezeBlockTrick == 2) //enemy on the right plattform --> swing him away
								{
									m_Input.m_Hook = 0;
									m_Input.m_Jump = 0;
									m_Input.m_Direction = 0;
									Fire(false);

									if (Server()->Tick() % 50 == 0)
									{
										m_DummyBoredCounter++;
										GameServer()->SendEmoticon(m_pPlayer->GetCID(), 7);
									}

									if (m_Core.m_Pos.x < 438 * 32) //first go right
										m_Input.m_Direction = 1;

									if (m_Core.m_Pos.x > 428 * 32 && m_Core.m_Pos.x < 430 * 32) //first jump
										m_Input.m_Jump = 1;

									if (m_Core.m_Pos.x > 427 * 32) //aim and swing
									{
										m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
										m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;
										m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
										m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;

										if (m_Core.m_Pos.x > 427 * 32 + 30 && pChr->m_Pos.y < 214 * 32)
										{
											m_Input.m_Hook = 1;
											if (Server()->Tick() % 10 == 0)
											{
												int x = rand() % 100 - 50;
												int y = rand() % 100 - 50;

												m_Input.m_TargetX = x;
												m_Input.m_TargetY = y;
											}
											//random shooting xD
											int r = rand() % 200 + 10;
											if (Server()->Tick() % r == 0 && m_FreezeTime == 0)
												Fire();
										}
									}

									//also this trick needs some freeze dogining because sometime huge speed fucks the bot
									//and NOW THIS CODE is here to fuck the high speed
									if (m_Core.m_Pos.x > 440 * 32)
										m_Input.m_Direction = -1;
									if (m_Core.m_Pos.x > 439 * 32 + 20 && m_Core.m_Pos.x < 440 * 32)
										m_Input.m_Direction = 0;
								}
								else if (m_DummyFreezeBlockTrick == 3) //enemy on the left swing him to the right
								{
									m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
									m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;
									m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
									m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;

									if (m_Core.m_Pos.y < 210 * 32 + 10)
									{
										m_DummyStartHook = true;
										m_DummyTrick3StartCount = true;
									}

									if (m_DummyStartHook)
									{
										if (Server()->Tick() % 80 == 0 || pChr->m_Pos.x > m_Core.m_Pos.x - 22)
											m_DummyStartHook = false;
									}

									if (m_DummyStartHook)
										m_Input.m_Hook = 1;
									else
										m_Input.m_Hook = 0;
									if (m_Core.m_Pos.x < 429 * 32)
										m_Input.m_Direction = 1;
									else
										m_Input.m_Direction = -1;

									//STOPPER hook:
									//hook the tee if he flys to much to the right
									if (pChr->m_Pos.x > 433 * 32 + 20)
										m_Input.m_Hook = 1;

									//Hook the tee agian and go to the left -> drag him under block area
									//-->Trick 5
									if (pChr->m_Core.m_Vel.y > 8.1f && pChr->m_Pos.x > 429 * 32 + 1 && pChr->m_Pos.y > 209 * 32)
									{
										m_DummyFreezeBlockTrick = 5;
										m_Input.m_Hook = 1;
									}

									//if he lands on the right plattform switch trick xD
									//doesnt work anysways (now fixed by the stopper hook)
									if (pChr->m_Pos.x > 433 * 32 && pChr->m_Core.m_Vel.y == 0.0f)
										m_DummyFreezeBlockTrick = 2;

									//Check for trick went wrong --> trick3 panic activation
									if (m_DummyTrick3StartCount)
										m_DummyPanicDelay++;
									if (m_DummyPanicDelay > 52)
										m_DummyTrick3Check = true;
									if (m_DummyTrick3Check)
									{
										if (pChr->m_Pos.x < 430 * 32 && pChr->m_Pos.x > 426 * 32 + 10 && pChr->IsGrounded())
										{
											m_DummyTrick3Panic = true;
											m_DummyTrick3PanicLeft = true;
										}
									}
									if (m_DummyTrick3Panic)
									{
										//stuck --> go left and swing him down
										m_Input.m_Direction = 1;
										if (m_Core.m_Pos.x < 425 * 32)
											m_DummyTrick3PanicLeft = false;

										if (m_DummyTrick3PanicLeft)
											m_Input.m_Direction = -1;
										else
										{
											if (m_Core.m_Pos.y < 212 * 32 + 10)
												m_Input.m_Hook = 1;
										}
									}
								}
								else if (m_DummyFreezeBlockTrick == 4) //clear left freeze
								{
									m_Input.m_Hook = 0;
									m_Input.m_Jump = 0;
									m_Input.m_Direction = 0;
									Fire(false);

									if (!m_DummyTrick4HasStartPos)
									{
										if (m_Core.m_Pos.x < 423 * 32 - 10)
											m_Input.m_Direction = 1;
										else if (m_Core.m_Pos.x > 424 * 32 - 20)
											m_Input.m_Direction = -1;
										else
											m_DummyTrick4HasStartPos = true;
									}
									else //has start pos
									{
										m_Input.m_TargetX = -200;
										m_Input.m_TargetY = -2;
										if (pChr->m_IsFrozen)
											m_Input.m_Hook = 1;
										else
										{
											m_Input.m_Hook = 0;
											m_DummyFreezeBlockTrick = 0; //fuck it too lazy normal stuff shoudl do the rest xD
										}
										if (Server()->Tick() % 7 == 0)
											m_Input.m_Hook = 0;
									}
								}
								else if (m_DummyFreezeBlockTrick == 5) //Hook under blockarea to the left (mostly the end of a trick)
								{
									m_Input.m_Hook = 1;

									if (m_Core.m_HookState == HOOK_GRABBED)
										m_Input.m_Direction = -1;
									else
									{
										if (m_Core.m_Pos.x < 428 * 32 + 20)
											m_Input.m_Direction = 1;
									}
								}
							}
						}
						else //nobody alive in ruler area --> stop tricks
						{
							m_DummyTrick4HasStartPos = false;
							m_DummyTrick3Panic = false;
							m_DummyTrick3StartCount = false;
							m_DummyTrick3Check = false;
							m_DummyPanicDelay = 0;
							m_DummyFreezeBlockTrick = 0;
						}
					}
				}

				if (m_Core.m_Pos.x > 429 * 32 && m_Core.m_Pos.x < 436 * 32 && m_Core.m_Pos.y < 214 * 32) //dangerous area over the freeze
				{
					//first check! too low?
					if (m_Core.m_Pos.y > 211 * 32 + 10 && IsGrounded() == false)
					{
						m_Input.m_Jump = 1;
						m_Input.m_Hook = 1;
						if (Server()->Tick() % 4 == 0)
							m_Input.m_Jump = 0;
					}

					//second check! too speedy?
					if (m_Core.m_Pos.y > 209 * 32 && m_Core.m_Vel.y > 5.6f)
					{
						m_Input.m_Jump = 1;
						if (Server()->Tick() % 4 == 0)
							m_Input.m_Jump = 0;
					}
				}

				if (m_Core.m_Pos.x > 439 * 32 && m_Core.m_Pos.x < 449 * 32)
				{
					//low left lowspeed --> go left
					if (m_Core.m_Pos.x > 439 * 32 && m_Core.m_Pos.y > 209 * 32 && m_Core.m_Vel.x < 3.0f)
						m_Input.m_Direction = -1;
					//low left highrightspeed --> go right with the speed and activate some random modes to keep the speed xD
					if (m_Core.m_Pos.x > 439 * 32 && m_Core.m_Pos.y > 209 * 32 && m_Core.m_Vel.x > 6.0f && m_Core.m_Jumped < 2)
					{
						m_Input.m_Direction = 1;
						m_Input.m_Jump = 1;
						if (Server()->Tick() % 5 == 0)
							m_Input.m_Jump = 0;
						m_DummySpeedRight = true;
					}

					if (m_IsFrozen || m_Core.m_Vel.x < 4.3f)
						m_DummySpeedRight = false;

					if (m_DummySpeedRight)
					{
						m_Input.m_Direction = 1;
						m_Input.m_TargetX = 200;
						int r = rand() % 200 - 100;
						m_Input.m_TargetY = r;
						m_Input.m_Hook = 1;
						if (Server()->Tick() % 30 == 0 && m_Core.m_HookState != HOOK_GRABBED)
							m_Input.m_Hook = 0;
					}
				}
				else //out of the freeze area resett bools
					m_DummySpeedRight = false;

				if (m_Core.m_Pos.x > 433 * 32 + 20 && m_Core.m_Pos.x < 437 * 32 && m_Core.m_Jumped > 2)
					m_Input.m_Direction = 1;

				if (m_Core.m_Pos.x > 448 * 32)
				{
					m_Input.m_Hook = 0;
					m_Input.m_Jump = 0;
					m_Input.m_Direction = 0;
					Fire(false);

					if (m_Core.m_Pos.x < 451 * 32 + 20 && IsGrounded() == false && m_Core.m_Jumped > 2)
						m_Input.m_Direction = 1;
					if (m_Core.m_Vel.x < -0.8f && m_Core.m_Pos.x < 450 * 32 && IsGrounded())
						m_Input.m_Jump = 1;
					CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, this, m_pPlayer->GetCID());
					if (pChr && pChr->IsAlive())
					{
						if (m_Core.m_Pos.x < 451 * 32)
							m_Input.m_Direction = 1;

						if (pChr->m_Pos.x < m_Core.m_Pos.x - 7 * 32 && m_Core.m_Jumped < 2) //if enemy is on the left & bot has jump --> go left too
							m_Input.m_Direction = -1;
						if (m_Core.m_Pos.x > 454 * 32)
						{
							m_Input.m_Direction = -1;
							m_Input.m_TargetX = pChr->m_Pos.x - m_Pos.x;
							m_Input.m_TargetY = pChr->m_Pos.y - m_Pos.y;
							m_LatestInput.m_TargetX = pChr->m_Pos.x - m_Pos.x;
							m_LatestInput.m_TargetY = pChr->m_Pos.y - m_Pos.y;

							if (m_Core.m_Pos.y + 40 > pChr->m_Pos.y)
							{
								m_Input.m_Hook = 1;
								if (Server()->Tick() % 50 == 0)
									m_Input.m_Hook = 0;
							}
						}

						//second freeze from the right --> goto singel palttform
						if (m_Core.m_Pos.x > 464 * 32 && m_Core.m_Jumped > 2 && m_Core.m_Pos.x < 468 * 32)
							m_Input.m_Direction = 1;
						//go back
						if (m_Core.m_Pos.x < 468 * 32 && IsGrounded() && m_Core.m_Pos.x > 464 * 32)
							m_Input.m_Jump = 1;
						//balance
						if (m_Core.m_Pos.x > 460 * 32 && m_Core.m_Pos.x < 464 * 32 && m_Core.m_Pos.y > 210 * 32 + 10)
							m_DummyDoBalance = true;
						if (IsGrounded() && m_IsFrozen)
							m_DummyDoBalance = false;

						if (m_DummyDoBalance)
						{
							if (m_Core.m_Pos.x > 463 * 32) //go right
							{
								if (m_Core.m_Pos.x > pChr->m_Pos.x - 4)
									m_Input.m_Direction = -1;
								else if (m_Core.m_Pos.x > pChr->m_Pos.x)
									m_Input.m_Direction = 1;

								if (m_Core.m_Pos.x < pChr->m_Pos.x)
								{
									m_Input.m_TargetX = 5;
									m_Input.m_TargetY = 200;
									if (m_Core.m_Pos.x - 1 < pChr->m_Pos.x)
										Fire();
								}
								else
								{
									//do the random flick
									int r = rand() % 100 - 50;
									m_Input.m_TargetX = r;
									m_Input.m_TargetY = -200;
								}
								if (pChr->m_Pos.x > 465 * 32 - 10 && pChr->m_Pos.x < 469 * 32) //right enough go out
									m_Input.m_Direction = 1;
							}
							else //go left
							{
								if (m_Core.m_Pos.x > pChr->m_Pos.x + 4)
									m_Input.m_Direction = -1;
								else if (m_Core.m_Pos.x < pChr->m_Pos.x)
									m_Input.m_Direction = 1;

								if (m_Core.m_Pos.x > pChr->m_Pos.x)
								{
									m_Input.m_TargetX = 5;
									m_Input.m_TargetY = 200;
									if (m_Core.m_Pos.x + 1 > pChr->m_Pos.x)
										Fire();
								}
								else
								{
									//do the random flick
									int r = rand() % 100 - 50;
									m_Input.m_TargetX = r;
									m_Input.m_TargetY = -200;
								}
								if (pChr->m_Pos.x < 459 * 32) //left enough go out
									m_Input.m_Direction = -1;
							}
						}

					}
					else //no close enemy alive
					{
						if (m_Core.m_Jumped < 2)
							m_Input.m_Direction = -1;
					}

					if (m_Core.m_Pos.x > 458 * 32 && m_Core.m_Pos.x < 466 * 32)
					{
						if (m_Core.m_Pos.y > 211 * 32 + 26)
							m_Input.m_Jump = 1;
						if (m_Core.m_Pos.y > 210 * 32 && m_Core.m_Vel.y > 5.4f)
							m_Input.m_Jump = 1;
					}

					//go home if its oky, oky?
					if ((m_Core.m_Pos.x < 458 * 32 && IsGrounded() && pChr->m_IsFrozen) || (m_Core.m_Pos.x < 458 * 32 && IsGrounded() && pChr->m_Pos.x > m_Core.m_Pos.x + (10 * 32)))
						m_Input.m_Direction = -1;
					//keep going also in the air xD
					if (m_Core.m_Pos.x < 450 * 32 && m_Core.m_Vel.x < 1.1f && m_Core.m_Jumped < 2)
						m_Input.m_Direction = -1;
					//go back if too far
					if (m_Core.m_Pos.x > 468 * 32 + 20)
						m_Input.m_Direction = -1;

				}
			}
		}
		else
			m_DummyBlockMode = 0;
	}
}