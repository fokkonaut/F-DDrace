//
// Created by Matq on 11/04/2025.
//

#include "../../gamecontext.h"
#include "bone.h"

void SBone::Snap(int SnappingClient)
{
	if (!m_Enabled || !m_pEntity || m_ID == -1)
		return;

	auto parentPos = m_pEntity->GetPos();
	if (GameServer()->GetClientDDNetVersion(SnappingClient) >= VERSION_DDNET_MULTI_LASER)
	{
		CNetObj_DDNetLaser *pObj = static_cast<CNetObj_DDNetLaser *>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, m_ID, sizeof(CNetObj_DDNetLaser)));
		if (!pObj)
			return;

		pObj->m_ToX = round_to_int(parentPos.x + m_To.x);
		pObj->m_ToY = round_to_int(parentPos.y + m_To.y);
		pObj->m_FromX = round_to_int(parentPos.x + m_From.x);
		pObj->m_FromY = round_to_int(parentPos.y + m_From.y);
//		pObj->m_StartTick = Server()->Tick() - 1;
		pObj->m_StartTick = Server()->Tick() - 4 + m_Thickness;
		pObj->m_Owner = -1;
		pObj->m_Type = m_Color;
	}
	else
	{
		CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ID, sizeof(CNetObj_Laser)));
		if (!pObj)
			return;

		pObj->m_X = round_to_int(parentPos.x + m_To.x);
		pObj->m_Y = round_to_int(parentPos.y + m_To.y);
		pObj->m_FromX = round_to_int(parentPos.x + m_From.x);
		pObj->m_FromY = round_to_int(parentPos.y + m_From.y);
		pObj->m_StartTick = Server()->Tick() - 4 + m_Thickness;
	}
}

void STrail::Snap(int SnappingClient)
{
	if (!m_Enabled || !m_pEntity || m_ID == -1)
		return;

	CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID, sizeof(CNetObj_Projectile)));
	if (!pObj)
		return;

	auto parentPos = m_pEntity->GetPos();
	pObj->m_X = round_to_int(parentPos.x + m_pPos->x);
	pObj->m_Y = round_to_int(parentPos.y + m_pPos->y);
	pObj->m_VelX = 0;
	pObj->m_VelY = 0;
	pObj->m_StartTick = Server()->Tick();
	pObj->m_Type = WEAPON_HAMMER;
}

void SHeart::Snap(int SnappingClient)
{
	if (!m_Enabled || !m_pEntity || m_ID == -1)
		return;

	int Size = Server()->IsSevendown(SnappingClient) ? 4*4 : sizeof(CNetObj_Pickup);
	CNetObj_Pickup *pPickup = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_ID, Size));
	if (!pPickup)
		return;

	auto parentPos = m_pEntity->GetPos();
	pPickup->m_X = round_to_int(parentPos.x + m_Pos.x);
	pPickup->m_Y = round_to_int(parentPos.y + m_Pos.y);
	pPickup->m_Type = POWERUP_HEALTH;
	if (Server()->IsSevendown(SnappingClient))
		((int*)pPickup)[3] = 0;
}
