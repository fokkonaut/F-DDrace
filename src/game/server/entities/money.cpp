// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include "money.h"

CMoney::CMoney(CGameWorld *pGameWorld, vec2 Pos, int Amount)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_MONEY, Pos, MONEY_DROP_RADIUS*2)
{
	m_Pos = Pos;
	m_Amount = Amount;

	GameWorld()->InsertEntity(this);

	for (int i = 0; i < NUM_DOTS; i++)
		m_aID[i] = Server()->SnapNewID();
}

CMoney::~CMoney()
{
	for (int i = 0; i < NUM_DOTS; i++)
		Server()->SnapFreeID(m_aID[i]);
}

void CMoney::Tick()
{
	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_Pos);
	m_TuneZone = GameServer()->Collision()->IsTune(CurrentIndex);

	if (!m_TuneZone)
		m_Vel.y += GameServer()->Tuning()->m_Gravity;
	else
		m_Vel.y += GameServer()->TuningList()[m_TuneZone].m_Gravity;

	CCharacter *pClosest = GameWorld()->ClosestCharacter(m_Pos, GetProximityRadius(), 0);
	if (pClosest)
	{
		//pClosest->GiveMoneyToPocket(m_Amount);
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Collected %d money", m_Amount);
		GameServer()->SendChatTarget(pClosest->GetPlayer()->GetCID(), aBuf);
		str_format(aBuf, sizeof(aBuf), "+%d", m_Amount);
		GameServer()->CreateLaserText(pClosest->GetPos(), pClosest->GetPlayer()->GetCID(), aBuf);
		GameWorld()->DestroyEntity(this);
		return;
	}

	GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(GetProximityRadius(), GetProximityRadius()), 0.5f);
}

void CMoney::Snap(int SnappingClient)
{
	CNetObj_Projectile *pBullet = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
	if(!pBullet)
		return;

	pBullet->m_X = (int)m_Pos.x;
	pBullet->m_Y = (int)m_Pos.y;
	pBullet->m_VelX = 0;
	pBullet->m_VelY = 0;
	pBullet->m_StartTick = 0;
	pBullet->m_Type = WEAPON_SHOTGUN;

	float AngleStep = 2.0f * pi / NUM_DOTS;
	for(int i = 0; i < NUM_DOTS; i++)
	{
		vec2 Pos = m_Pos + vec2(MONEY_DROP_RADIUS * cos(AngleStep*i), MONEY_DROP_RADIUS * sin(AngleStep*i));
		
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
