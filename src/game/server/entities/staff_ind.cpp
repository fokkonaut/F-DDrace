// original version has been created by ReiTw

#include <game/server/gamecontext.h>
#include "staff_ind.h"

CStaffInd::CStaffInd(CGameWorld *pGameWorld, vec2 Pos, int Owner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_STAFF_IND, Pos)
{
	m_Owner = Owner;

	m_Dist = 0.f;
	m_BallFirst = true;
	m_TeamMask = Mask128();

	for (int i = 0; i < NUM_IDS; i++)
		m_aID[i] = Server()->SnapNewID();
	std::sort(std::begin(m_aID), std::end(m_aID));
	GameWorld()->InsertEntity(this);
}

void CStaffInd::Reset()
{
	for(int i = 0; i < NUM_IDS; i ++) 
   		Server()->SnapFreeID(m_aID[i]);
	GameWorld()->DestroyEntity(this);
}

void CStaffInd::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner || !pOwner->m_StaffInd)
	{
		Reset();
		return;
	}

	m_TeamMask = pOwner->TeamMask();
	m_Pos = pOwner->GetPos();
	m_aPos[ARMOR] = vec2(m_Pos.x, m_Pos.y - 70.f);

	if(m_BallFirst)
	{
		m_Dist += 0.9f;
		if (m_Dist > 25.f)
			m_BallFirst = false;
	}
	else
	{
		m_Dist -= 0.9f;
		if (m_Dist < -25.f)
			m_BallFirst = true;
	}

	m_aPos[BALL] = vec2(m_Pos.x + m_Dist, m_aPos[ARMOR].y);
}

void CStaffInd::Snap(int SnappingClient)
{	
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if (!CmaskIsSet(m_TeamMask, SnappingClient) || (pOwner && pOwner->IsPaused()))
		return;

	int Size = Server()->IsSevendown(SnappingClient) ? 4*4 : sizeof(CNetObj_Pickup);
	CNetObj_Pickup *pArmor = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_aID[ARMOR], Size));
	if (!pArmor)
		return;

	pArmor->m_X = round_to_int(m_aPos[ARMOR].x);
	pArmor->m_Y = round_to_int(m_aPos[ARMOR].y);
	if (Server()->IsSevendown(SnappingClient))
	{
		pArmor->m_Type = POWERUP_ARMOR;
		((int*)pArmor)[3] = 0;
	}
	else
		pArmor->m_Type = POWERUP_ARMOR;

	// m_ID is created before m_aID is created, means that id is lower and we can simply use it to make the ball behind
	CNetObj_Laser *pLaser = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_BallFirst ? m_aID[BALL_FRONT] : m_aID[BALL], sizeof(CNetObj_Laser)));
	if(!pLaser)
		return;

	pLaser->m_X = round_to_int(m_aPos[BALL].x);
	pLaser->m_Y = round_to_int(m_aPos[BALL].y);
	pLaser->m_FromX = round_to_int(m_aPos[BALL].x);
	pLaser->m_FromY = round_to_int(m_aPos[BALL].y);
	pLaser->m_StartTick = Server()->Tick();
}


