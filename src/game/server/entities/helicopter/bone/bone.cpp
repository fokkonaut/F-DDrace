//
// Created by Matq on 16/03/2026.
//

#include "bone.h"
#include "../../../gamecontext.h"

CBone::CBone(
	CEntity *pEntity,
	int SnapID,
	float FromX,
	float FromY,
	float ToX,
	float ToY,
	int Thickness,
	int Color
)
	: CBone(pEntity, SnapID, vec2(FromX, FromY), vec2(ToX, ToY), Thickness, Color)
{
}

CBone::CBone(CEntity *pEntity, int SnapID, vec2 From, vec2 To, int Thickness, int Color)
	: m_pEntity(pEntity),
	  m_ID(SnapID),
	  m_InitFrom(From),
	  m_InitTo(To),
	  m_InitColor(Color),
	  m_InitThickness(Thickness),
	  m_Enabled(true),
	  m_From(From),
	  m_To(To),
	  m_Color(Color),
	  m_Thickness(Thickness)
{
}

SBounds CBone::GetBounds()
{
	return {
		std::min(m_From.x, m_To.x),
		std::min(m_From.y, m_To.y),
		std::max(m_From.x, m_To.x),
		std::max(m_From.y, m_To.y)
	};
}

void CBone::AssignEntityAndID(CEntity *pEntity, int SnapID)
{
	m_pEntity = pEntity;
	m_ID = SnapID;
}

void CBone::UpdateLine(vec2 From, vec2 To)
{
	m_From = From;
	m_To = To;
}

void CBone::Flip()
{
	m_From.x = -m_From.x;
	m_To.x = -m_To.x;
}

void CBone::Rotate(float Angle)
{
	m_From = rotate(m_From, Angle);
	m_To = rotate(m_To, Angle);
}

void CBone::Scale(float factor)
{
	// Only use case where init positions matter after scaling
	m_InitFrom *= factor;
	m_InitTo *= factor;
	m_From *= factor;
	m_To *= factor;
}

void CBone::Save()
{
	m_InitFrom = m_From;
	m_InitTo = m_To;
	m_InitColor = m_Color;
	m_InitThickness = m_Thickness;
}

void CBone::SavePositions()
{
	m_InitFrom = m_From;
	m_InitTo = m_To;
}

void CBone::LoadPositions()
{
	m_From = m_InitFrom;
	m_To = m_InitTo;
}

void CBone::Snap(int SnappingClient, bool Flipped, float VertexSnapping, bool RainbowMode)
{
	if (!m_Enabled || !m_pEntity || m_ID == -1)
		return;

	CSnapContext ctx(
		GameServer()->GetClientDDNetVersion(SnappingClient),
		Server()->IsSevendown(SnappingClient),
		SnappingClient
	);

	auto parentPos = m_pEntity->GetPos();
	float toX = parentPos.x + (Flipped ? -m_To.x : m_To.x);
	float toY = parentPos.y + m_To.y;
	float fromX = parentPos.x + (Flipped ? -m_From.x : m_From.x);
	float fromY = parentPos.y + m_From.y;
	int startTick = Server()->Tick() - 4 + m_Thickness;

	if (VertexSnapping > 1.0f)
	{
		toX = floor(toX / VertexSnapping) * VertexSnapping;
		toY = floor(toY / VertexSnapping) * VertexSnapping;
		fromX = floor(fromX / VertexSnapping) * VertexSnapping;
		fromY = floor(fromY / VertexSnapping) * VertexSnapping;
	}

	vec2 To = vec2(toX, toY);
	vec2 From = vec2(fromX, fromY);
	int Color = RainbowMode ? (m_Color + Server()->Tick()) / 4 % 4 : m_Color;

	GameServer()->SnapLaserObject(ctx, m_ID, To, From, startTick, -1, Color, -1, -1, LASERFLAG_NO_PREDICT);
}

//

CTrailNode::CTrailNode()
	: m_pEntity(nullptr), m_ID(-1), m_pPos(nullptr)
{
}

CTrailNode::CTrailNode(CEntity *pEntity, int SnapID, vec2 *pPos)
	: m_pEntity(pEntity), m_ID(SnapID), m_pPos(pPos), m_Enabled(true)
{
}

void CTrailNode::Snap(int SnappingClient, bool Flipped, float VertexSnapping)
{
	if (!m_Enabled || !m_pEntity || m_ID == -1)
		return;

	CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID, sizeof(CNetObj_Projectile)));
	if (!pObj)
		return;

	auto parentPos = m_pEntity->GetPos();
	float x = parentPos.x + (Flipped ? -m_pPos->x : m_pPos->x);
	float y = parentPos.y + m_pPos->y;

	if (VertexSnapping > 1.0f)
	{
		x = floor(x / VertexSnapping) * VertexSnapping;
		y = floor(y / VertexSnapping) * VertexSnapping;
	}

	pObj->m_X = round_to_int(x);
	pObj->m_Y = round_to_int(y);
	pObj->m_VelX = 0;
	pObj->m_VelY = 0;
	pObj->m_StartTick = Server()->Tick();
	pObj->m_Type = WEAPON_HAMMER;
}

//

CPickupNode::CPickupNode()
	: CPickupNode(nullptr, -1, POWERUP_HEALTH, vec2(0.f, 0.f))
{
}

CPickupNode::CPickupNode(CEntity *pEntity, int SnapID, int PickupType, vec2 Pos)
	: m_pEntity(pEntity), m_Pos(Pos), m_ID(SnapID), m_PickupType(PickupType), m_Enabled(true)
{
}

void CPickupNode::Snap(int SnappingClient)
{
	if (!m_Enabled || !m_pEntity || m_ID == -1)
		return;

	CSnapContext ctx(
		GameServer()->GetClientDDNetVersion(SnappingClient),
		Server()->IsSevendown(SnappingClient),
		SnappingClient
	);

	auto parentPos = m_pEntity->GetPos();
	GameServer()->SnapPickupObject(ctx, m_ID, parentPos + m_Pos, m_PickupType, 0, -1, PICKUPFLAG_NO_PREDICT);
}
