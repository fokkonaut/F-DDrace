// made by ChillerDragon, improved by fokkonaut

#include "v3_blocker.h"
#include <game/server/gamecontext.h>

#include "macros.h"

CDummyV3Blocker::CDummyV3Blocker(CCharacter *pChr)
: CDummyBase(pChr, DUMMYMODE_V3_BLOCKER)
{
}

void CDummyV3Blocker::OnTick()
{
	int V3_OFFSET_X = GameServer()->Config()->m_SvV3OffsetX;
	int V3_OFFSET_Y = GameServer()->Config()->m_SvV3OffsetY;

	if (!str_comp(GameServer()->Config()->m_SvMap, "blmapV3RoyalX"))
	{
		if (Y > 50 || X > 150 || m_pCharacter->IsSolo())
		{
			Die();
			return;
		}

		if (X < V3_OFFSET_X)
		{
			Right();
			if (
				(IsGrounded() && ((X < 36 && GetVel().x < 5.0f) || (X > 53 && X < 55)))
				|| (((IsGrounded() && GetVel().x > 5.0f) || (GetVel().x < 5.0f && !IsGrounded())) && X > 72 && X < V3_OFFSET_X)
				|| (X > 69 && X < 71)
				)
				Jump();
			if (TicksPassed(20))
				Jump(0);
			return;
		}
	}

	if (m_pCharacter->m_FreezeTime && Server()->Tick() == m_pCharacter->m_FirstFreezeTick + 300)
	{
		Die();
		return;
	}

	CCharacter *pChr = GameWorld()->ClosestCharacter(GetPos(), 30*32.f, m_pCharacter, m_pPlayer->GetCID());
	if (pChr && pChr->IsAlive())
	{
		AimPos(pChr->GetPos());

		/*************************************************
		*                                                *
		*                A T T A C K                     *
		*                                                *
		**************************************************/

		//swing enemy up
		if (RAW_Y < pChr->GetPos().y - 20 && !IsGrounded() && !pChr->m_IsFrozen)
		{
			Hook();
			float dist = distance(pChr->GetPos(), GetPos());
			if (dist < 250.f)
			{
				if (RAW_X < pChr->GetPos().x)
					Left();
				else
					Right();
				if (dist < 80.f) // hammer dist
				{
					if (absolute(pChr->Core()->m_Vel.x) > 2.6f)
					{
						if (m_pCharacter->m_FreezeTime == 0)
							Fire();
					}
				}
			}
		}

		//attack in mid
		if (pChr->GetPos().x > RAW(V3_OFFSET_X + 19) - 7 && pChr->GetPos().x < RAW(V3_OFFSET_X + 22) + 7)
		{
			if (pChr->GetPos().x < RAW_X) // bot on the left
			{
				if (pChr->Core()->m_Vel.x < 0.0f)
					Hook();
				else
					Hook(0);
			}
			else // bot on the right
			{
				if (pChr->Core()->m_Vel.x < 0.0f)
					Hook(0);
				else
					Hook();
			}
			if (pChr->m_IsFrozen)
				Hook(0);
		}

		//attack bot in the middle and enemy in the air -> try to hook down
		if (Y < V3_OFFSET_Y + 19 && Y > V3_OFFSET_Y + 11 && IsGrounded()) // if bot is in position
		{
			if (pChr->GetPos().x < RAW(V3_OFFSET_X + 15) || pChr->GetPos().x > RAW(V3_OFFSET_X + 26)) //enemy on the left side
			{
				if (pChr->GetPos().y < RAW(V3_OFFSET_Y + 17) && pChr->Core()->m_Vel.y > 4.2f)
					Hook();
			}

			if (HookState() == HOOK_FLYING)
				Hook();
			else if (HookState() == HOOK_GRABBED)
			{
				Hook();
				//stay strong and walk agianst hook pull
				if (X < V3_OFFSET_X + 18) //left side
					Right();
				else if (X > V3_OFFSET_X + 23) //right side
					Left();
			}
		}

		// attack throw into left freeze wall
		if (X < V3_OFFSET_X + 9)
		{
			if (pChr->GetPos().y > RAW_Y + 190)
				Hook();
			else if (pChr->GetPos().y < RAW_Y - 190)
				Hook();
			else
			{
				if (pChr->Core()->m_Vel.x < -1.6f)
				{
					if (pChr->GetPos().x < RAW_X - 7 && pChr->GetPos().x > RAW_X - 90) //enemy on the left side
					{
						if (pChr->GetPos().y < RAW_Y + 90 && pChr->GetPos().y > RAW_Y - 90)
						{
							if (m_pCharacter->m_FreezeTime == 0)
								Fire();
						}
					}
				}
			}
		}

		// attack throw into right freeze wall
		if (X > V3_OFFSET_X + 30)
		{
			if (pChr->GetPos().y > RAW_Y + 190)
				Hook();
			else if (pChr->GetPos().y < RAW_Y - 190)
				Hook();
			else
			{
				if (pChr->Core()->m_Vel.x > 1.6f)
				{
					if (pChr->GetPos().x > RAW_X + 7 && pChr->GetPos().x < RAW_X + 90) //enemy on the right side
					{
						if (pChr->GetPos().y > RAW_Y - 90 && pChr->GetPos().y < RAW_Y + 90)
						{
							if (m_pCharacter->m_FreezeTime == 0)
								Fire();
						}
					}
				}
			}
		}

		/*************************************************
		*                                                *
		*                D E F E N D (move)              *
		*                                                *
		**************************************************/

		//########################################
		//Worst hammer switch code eu west rofl! #
		//########################################
		//switch to hammer if enemy is near enough
		if ((pChr->GetPos().x > RAW_X + 130) || (pChr->GetPos().x < RAW_X - 130)) //default is gun
			SetWeapon(WEAPON_GUN);
		else
		{
			//switch to hammer if enemy is near enough
			if ((pChr->GetPos().y > RAW_Y + 130) || (pChr->GetPos().y < RAW_Y - 130)) //default is gun
				SetWeapon(WEAPON_GUN);
			else //near --> hammer
				SetWeapon(WEAPON_HAMMER);
		}

		//Starty movement
		if (X < V3_OFFSET_X + 15 && Y > V3_OFFSET_Y + 20 && pChr->GetPos().y > RAW(V3_OFFSET_Y + 20) && pChr->GetPos().x > RAW(V3_OFFSET_X + 24) && IsGrounded() && pChr->IsGrounded())
			Jump();
		if (X < V3_OFFSET_X + 15 && pChr->GetPos().x > /*307 * 32 +*/ RAW(V3_OFFSET_X) && pChr->GetPos().x > RAW(V3_OFFSET_X + 24))
			Right();

		//important freeze doges leave them last!:

		//freeze prevention mainpart down right
		if (X > V3_OFFSET_X + 23 && X < V3_OFFSET_X + 27 && Y > V3_OFFSET_Y + 19)
		{
			Jump();
			Hook();
			if (TicksPassed(20))
			{
				Hook(0);
				Jump(0);
			}
			Right();
			Aim(200, 80);
		}

		//freeze prevention mainpart down left
		if (X > V3_OFFSET_X + 13 && X < V3_OFFSET_X + 17 && Y > V3_OFFSET_Y + 19)
		{
			Jump();
			Hook();
			if (TicksPassed(20))
			{
				Hook(0);
				Jump(0);
			}
			Left();
			Aim(-200, 80);
		}

		//Freeze prevention top left
		if (X < V3_OFFSET_X + 17 && Y < V3_OFFSET_Y + 12 && RAW_X > RAW(V3_OFFSET_X + 13) - 10)
		{
			Left();
			Hook();
			if (TicksPassed(20))
				Hook(0);
			Aim(-200, -87);
			if (RAW_Y > RAW(19) + 20)
			{
				Aim(-200, -210);
			}
		}

		//Freeze prevention top right
		if (RAW_X < RAW(V3_OFFSET_X + 28) + 10 && Y < V3_OFFSET_Y + 12 && X > V3_OFFSET_X + 23)
		{
			Right();
			Hook();
			if (TicksPassed(20))
				Hook(0);
			Aim(200, -87);
			if (RAW_Y > RAW(V3_OFFSET_Y + 8) + 20)
			{
				Aim(200, -210);
			}
		}

		//Freeze prevention mid
		if (RAW_X > RAW(V3_OFFSET_X + 19) - 7 && RAW_X < RAW(V3_OFFSET_X + 22) + 7)
		{
			if (GetVel().x < 0.0f)
				Left();
			else
				Right();

			if (RAW_Y > RAW(V3_OFFSET_Y + 18) - 1 && !IsGrounded())
			{
				Jump();
				if (Jumped() > 2) //no jumps == rip   --> panic hook
				{
					Hook();
					if (TicksPassed(15))
						Hook(0);
				}
			}
		}

		//Freeze prevention left 
		if (X < V3_OFFSET_X + 6 || (X < V3_OFFSET_X + 8 && GetVel().x < -8.4f))
			Right();
		//Freeze prevention right
		if (X > V3_OFFSET_X + 34 || (X > V3_OFFSET_X + 32 && GetVel().x > 8.4f))
			Left();
		//Dont allow to hook blocks
		if (m_pCharacter->Core()->m_TriggeredEvents&COREEVENTFLAG_HOOK_ATTACH_GROUND)
			Hook(0);
	}
}