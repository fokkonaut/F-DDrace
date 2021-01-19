// made by ChillerDragon

#include "chillblock5_police.h"
#include <game/server/gamecontext.h>

#include "macros.h"

CDummyChillBlock5Police::CDummyChillBlock5Police(CCharacter *pChr)
: CDummyBase(pChr, DUMMYMODE_CHILLBLOCK5_POLICE)
{
	m_GetSpeed = false;
	m_GotStuck = false;
	m_PoliceMode = 0;
	m_AttackMode = 0;
}

void CDummyChillBlock5Police::OnTick()
{
	//selfkill
	//dyn
	if (GetVel().y == 0.000000f && GetVel().x < 0.01f && GetVel().x > -0.01f && m_pCharacter->m_IsFrozen)
	{
		if (TicksPassed(20))
			GameServer()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_DROP);

		if (TicksPassed(200))
		{
			Die();
			return;
		}
	}

	CCharacter *pChr = GameWorld()->ClosestCharacter(GetPos(), m_pCharacter, m_pPlayer->GetCID());
	if (pChr && pChr->IsAlive())
	{
		if (pChr->m_PoliceHelper || GameServer()->m_Accounts[pChr->GetPlayer()->GetAccID()].m_PoliceLevel) //police
		{
			if (pChr->m_FreezeTime > 0 && X < 477)
				m_PoliceMode = 2; // LOCAL: POLICE HELP
			else
				m_PoliceMode = 0; // LOCAL: NOTHING IS GOING ON
		}
		else //not police
		{
			if (pChr->m_FreezeTime == 0)
			{
				if (pChr->GetPos().x < RAW(481))
					m_PoliceMode = 1; //LOCAL: ENEMY ATTACK
			}
			if (pChr->m_IsFrozen)
				m_PoliceMode = 0; //maybe add here a mode where the bot moves the nonPolices away to find failed polices
		}

		if (m_PoliceMode == 0) //nothing is going on
		{
			AimPos(pChr->GetPos());
			if (TicksPassed(90))
				SetWeapon(WEAPON_GUN);
		}
		else if (m_PoliceMode == 1) //Attack enemys
		{
			AimPos(pChr->GetPos());

			if (TicksPassed(30))
				SetWeapon(WEAPON_HAMMER);

			if (m_pCharacter->m_FreezeTime == 0 && pChr->m_FreezeTime == 0 && pChr->Core()->m_Vel.y < -0.5 && pChr->GetPos().x > RAW_X - RAW(3) && pChr->GetPos().x < RAW_X + RAW(3))
				Fire();

			m_AttackMode = 0;
			if (RAW_X < RAW(466) + 20 && pChr->GetPos().x > RAW(469) + 20) //hook enemy in air (rightside)
				m_AttackMode = 1;

			if (m_AttackMode == 0) //default mode
			{
				if (RAW_X < RAW(466) - 5) //only get bored on lovley place 
				{
					(rand() % 2) ? Right() : StopMoving();
					if (IsGrounded())
						Jump(rand() % 2);
					if (pChr->GetPos().y > RAW_Y)
						Hook();
				}
			}
			else if (m_AttackMode == 1) //hook enemy escaping (rightside)
			{
				if (pChr->Core()->m_Vel.x > 1.3f)
					Hook();
			}

			//Dont Hook enemys back in safety
			if ((pChr->GetPos().x < RAW(460) && pChr->GetPos().x > RAW(457)) || (pChr->GetPos().x < RAW(469) && pChr->GetPos().x > RAW(466)))
				Hook(0);
		}
		else if (m_PoliceMode == 2) //help police dudes
		{
			AimPos(pChr->GetPos());

			if (pChr->GetPos().y > RAW_Y)
				Hook();
			if (TicksPassed(40))
			{
				Hook(0);
				Jump(0);
			}
			if (IsGrounded() && pChr->m_IsFrozen)
				Jump();

			if (pChr->m_IsFrozen)
			{
				if (pChr->GetPos().x > RAW_X)
					Right();
				else if (pChr->GetPos().x < RAW_X)
					Left();
			}
			else
			{
				if (pChr->GetPos().x - 110 > RAW_X)
					Right();
				else if (pChr->GetPos().x + 110 < RAW_X)
					Left();
				else
				{
					if (TicksPassed(10))
						SetWeapon(WEAPON_HAMMER);
					if (m_pCharacter->m_FreezeTime == 0 && pChr->m_FreezeTime > 0)
						Fire();
				}
			}

			//invert direction if hooked the player to add speed :)
			if (HookState() == HOOK_GRABBED)
			{
				if (pChr->GetPos().x > RAW_X)
					Left();
				else if (pChr->GetPos().x < RAW_X)
					Right();
			}

			//schleuderprotection   stop hook if mate is safe to prevemt blocking him to the other side
			if (pChr->GetPos().x > RAW(460) + 10 && pChr->GetPos().x < RAW(466))
				Hook(0);
		}
		else if (m_PoliceMode == 3) //EXTERNAL: Enemy attack (rigt side /jail side)
		{
			if (X < 461)
				Right();
			else
			{
				if (X < 484)
					Right();

				if (X > 477 && !IsGrounded())
					Hook();
			}

			//jump all the time xD
			if (IsGrounded() && X > 480)
				Jump();

			//Important jump protection
			if (X > 466 && RAW_Y > RAW(240) + 8 && X < 483)
				Jump();
		}
		else //unknown dummymode
		{
			AimPos(pChr->GetPos());
		}
	}

	if (m_PoliceMode < 3)
	{
		if (RAW_X > RAW(482) + 20 && Y < 236)
		{
			if (GetVel().x > -8.2f && RAW_X < RAW(484) - 20)
				m_GetSpeed = true;

			if (X > 483 && !IsGrounded())
				m_GetSpeed = true;

			if (GetVel().y > 5.3f)
				m_GetSpeed = true;

			if (IsGrounded() && X > 485)
				m_GetSpeed = false;

			if (m_GotStuck)
			{
				Left();

				if (TicksPassed(33))
					Jump();

				if (TicksPassed(20))
					SetWeapon(WEAPON_HAMMER); //hammer

				if (GetTargetX() < -20)
				{
					if (m_pCharacter->m_FreezeTime == 0)
						Fire();
				}
				else if (GetTargetX() > 20)
				{
					Hook();
					if (TicksPassed(25))
						Hook(0);
				}
			}
			else
			{
				if (m_GetSpeed)
				{
					Right();
					if (TicksPassed(90))
						m_GotStuck = true;
				}
				else
				{
					Left();
					if (GetVel().x > -4.4f)
					{
						if (TicksPassed(90))
							m_GotStuck = true;
					}
				}
			}
		}
		else //not Jail spawn
		{
			m_GotStuck = false;

			if (X > 464)
				Left();
			else if (X < 461)
				Right();

			if (X > 466 && RAW_Y > RAW(240) + 8)
				Jump();
		}
	}
}