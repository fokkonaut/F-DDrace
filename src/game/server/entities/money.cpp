// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include "money.h"

CMoney::CMoney(CGameWorld *pGameWorld, vec2 Pos, int64 Amount, int Owner, float Direction)
: CAdvancedEntity(pGameWorld, CGameWorld::ENTTYPE_MONEY, Pos, GetRadius(Amount)*2, Owner, false)
{
	m_Pos = Pos;
	m_Amount = Amount;
	m_Vel = vec2(5*Direction, Direction == 0 ? 0 : -5);
	m_StartTick = Server()->Tick();

	for (int i = 0; i < NUM_DOTS_BIG; i++)
		m_aID[i] = Server()->SnapNewID();

	GameWorld()->InsertEntity(this);
}

CMoney::~CMoney()
{
	for (int i = 0; i < NUM_DOTS_BIG; i++)
		Server()->SnapFreeID(m_aID[i]);
}

bool CMoney::SecondsPassed(float Seconds)
{
	return m_StartTick < (Server()->Tick() - Server()->TickSpeed() * Seconds);
}

void CMoney::Tick()
{
	if (IsMarkedForDestroy())
		return;

	CAdvancedEntity::Tick();
	HandleDropped();

	// Remove small money drops after 10 minutes
	if (m_Amount < SMALL_MONEY_AMOUNT && SecondsPassed(60 * 10))
	{
		Reset();
		return;
	}

	CCharacter *pClosest = GameWorld()->ClosestCharacter(m_Pos, RADIUS_FIND_PLAYERS, SecondsPassed(2) ? 0 : GetOwner(), m_Owner, true, true);
	if (pClosest)
	{
		if (distance(m_Pos, pClosest->GetPos()) < GetRadius() + pClosest->GetProximityRadius())
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "Collected %lld money", m_Amount);
			GameServer()->SendChatTarget(pClosest->GetPlayer()->GetCID(), aBuf);
			pClosest->GetPlayer()->WalletTransaction(m_Amount, "collected");

			str_format(aBuf, sizeof(aBuf), "+%lld", m_Amount);
			GameServer()->CreateLaserText(m_Pos, pClosest->GetPlayer()->GetCID(), aBuf, GameServer()->MoneyLaserTextTime(m_Amount));
			GameServer()->CreateSound(m_Pos, SOUND_HOOK_LOOP, pClosest->Teams()->TeamMask(pClosest->Team(), -1, pClosest->GetPlayer()->GetCID()));

			Reset();
			return;
		}
		else
			MoveTo(pClosest->GetPos(), RADIUS_FIND_PLAYERS);
	}

	CMoney *pMoney = (CMoney *)GameWorld()->ClosestEntity(m_Pos, RADIUS_FIND_MONEY, CGameWorld::ENTTYPE_MONEY, this, true);
	if (pMoney)
	{
		if (distance(m_Pos, pMoney->GetPos()) < GetRadius() + pMoney->GetRadius())
		{
			m_Amount += pMoney->m_Amount;
			pMoney->Reset();

			Mask128 TeamMask = Mask128();
			if (GetOwner())
				TeamMask = GetOwner()->Teams()->TeamMask(GetOwner()->Team(), -1, GetOwner()->GetPlayer()->GetCID());
			GameServer()->CreateDeath(m_Pos, m_Owner, TeamMask);
		}
		else if (!pClosest)
			MoveTo(pMoney->GetPos(), RADIUS_FIND_MONEY);
	}

	m_PrevPos = m_Pos;
}

void CMoney::MoveTo(vec2 Pos, int Radius)
{
	float MaxFlySpeed = m_TuneZone ? GameServer()->TuningList()[m_TuneZone].m_MoneyMaxFlySpeed : GameServer()->Tuning()->m_MoneyMaxFlySpeed;
	if (MaxFlySpeed <= 0.f)
		return;

	vec2 Diff = vec2(Pos.x - m_Pos.x, Pos.y - m_Pos.y);
	float AddVelX = (Diff.x/Radius*5);
	m_Vel.x = clamp(m_Vel.x+AddVelX, min(-MaxFlySpeed, m_Vel.x-AddVelX), max(MaxFlySpeed, m_Vel.x-AddVelX));

	// Calculate out the gravity while we move to a position, we cant just not call HandleDropped() because we still want teleporter, stopper, etc...
	float Gravity = m_TuneZone ? GameServer()->TuningList()[m_TuneZone].m_Gravity : GameServer()->Tuning()->m_Gravity;
	m_Vel.y -= Gravity;
	float AddVelY = (Diff.y/Radius*5);
	m_Vel.y = clamp(m_Vel.y+AddVelY, min(-MaxFlySpeed, m_Vel.y-AddVelY), max(MaxFlySpeed, m_Vel.y-AddVelY));
}

void CMoney::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	if (GameServer()->GetPlayerChar(SnappingClient) && GetOwner())
	{
		if (GetOwner()->IsPaused())
			return;

		if (!CmaskIsSet(GetOwner()->Teams()->TeamMask(GetOwner()->Team(), -1, GetOwner()->GetPlayer()->GetCID()), SnappingClient))
			return;
	}

	CNetObj_Projectile *pBullet = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
	if(!pBullet)
		return;

	pBullet->m_X = (int)m_Pos.x;
	pBullet->m_Y = (int)m_Pos.y;
	pBullet->m_VelX = 0;
	pBullet->m_VelY = 0;
	pBullet->m_StartTick = 0;
	pBullet->m_Type = WEAPON_SHOTGUN;

	float AngleStep = 2.0f * pi / GetNumDots();
	for(int i = 0; i < GetNumDots(); i++)
	{
		vec2 Pos = m_Pos + vec2(GetRadius() * cos(AngleStep*i), GetRadius() * sin(AngleStep*i));
		
		CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_aID[i], sizeof(CNetObj_Projectile)));
		if(!pObj)
			return;

		pObj->m_X = (int)Pos.x;
		pObj->m_Y = (int)Pos.y;
		pObj->m_VelX = 0;
		pObj->m_VelY = 0;
		pObj->m_StartTick = 0;
		pObj->m_Type = WEAPON_HAMMER;
	}
}
