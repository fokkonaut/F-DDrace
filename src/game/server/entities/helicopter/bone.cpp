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

	int SnappingClientVersion = GameServer()->GetClientDDNetVersion(SnappingClient);
	GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion, Server()->IsSevendown(SnappingClient), SnappingClient), m_ID,
		parentPos+m_To, parentPos+m_From, Server()->Tick()-4+m_Thickness, -1, m_Color, -1, -1, LASERFLAG_NO_PREDICT);
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

	int SnappingClientVersion = GameServer()->GetClientDDNetVersion(SnappingClient);
	auto parentPos = m_pEntity->GetPos();
	GameServer()->SnapPickupObject(CSnapContext(SnappingClientVersion, Server()->IsSevendown(SnappingClient), SnappingClient), m_ID,
		parentPos + m_Pos, POWERUP_HEALTH, -1, -1, PICKUPFLAG_NO_PREDICT);
}
