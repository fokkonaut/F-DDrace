//
// Created by Matq on 11/04/2025.
//

#include "../../gamecontext.h"
#include "bone.h"

void SBone::Snap(int SnappingClient, bool Flipped, int VertexSnapping)
{
	if (!m_Enabled || !m_pEntity || m_ID == -1)
		return;

	auto parentPos = m_pEntity->GetPos();
	int toX = round_to_int(parentPos.x + (Flipped ? -m_To.x : m_To.x));
	int toY = round_to_int(parentPos.y + m_To.y);
	int fromX = round_to_int(parentPos.x + (Flipped ? -m_From.x : m_From.x));
	int fromY = round_to_int(parentPos.y + m_From.y);
	int startTick = Server()->Tick() - 4 + m_Thickness;

	if (VertexSnapping > 1)
	{
		toX = toX / VertexSnapping * VertexSnapping;
		toY = toY / VertexSnapping * VertexSnapping;
		fromX = fromX / VertexSnapping * VertexSnapping;
		fromY = fromY / VertexSnapping * VertexSnapping;
	}

	int SnappingClientVersion = GameServer()->GetClientDDNetVersion(SnappingClient);
	GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion, Server()->IsSevendown(SnappingClient), SnappingClient), m_ID,
		vec2(toX, toY), vec2(fromX, fromY), startTick, -1, m_Color, -1, -1, LASERFLAG_NO_PREDICT);
}

void STrail::Snap(int SnappingClient, bool Flipped, int VertexSnapping)
{
	if (!m_Enabled || !m_pEntity || m_ID == -1)
		return;

	CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID, sizeof(CNetObj_Projectile)));
	if (!pObj)
		return;

	auto parentPos = m_pEntity->GetPos();
	int x = round_to_int(parentPos.x + (Flipped ? -m_pPos->x : m_pPos->x));
	int y = round_to_int(parentPos.y + m_pPos->y);

	if (VertexSnapping > 1)
	{
		x = x / VertexSnapping * VertexSnapping;
		y = y / VertexSnapping * VertexSnapping;
	}

	pObj->m_X = x;
	pObj->m_Y = y;
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
		parentPos + m_Pos, POWERUP_HEALTH, 0, -1, PICKUPFLAG_NO_PREDICT);
}

//

void SBoneModel::InitModel()
{
}

void SBoneModel::ApplyScaleBones(float Scale)
{
	for (int i = 0; i < m_NumBones; i++)
		Bones()[i].Scale(Scale);

	m_BoundTop *= Scale;
	m_BoundBottom *= Scale;
	m_BoundLeft *= Scale;
	m_BoundRight *= Scale;

	m_TotalWidth *= Scale;
	m_TotalHeight *= Scale;
}

void SBoneModel::ApplyScalePropellers(float Scale)
{
	for (int i = 0; i < m_NumPropellers; i++)
		m_aPropellers[i].ApplyScale(Scale);
}

SBoneModel::SBoneModel(CEntity *pEntity, int NumBones, int NumTrails, int NumPropellers)
{
	m_pEntity = pEntity;

	m_aBones = new SBone[NumBones];
	m_NumBones = NumBones;

	m_aTrails = new STrail[NumTrails];
	m_NumTrails = NumTrails;

	m_BoundLeft = 0.0f;
	m_BoundRight = 0.0f;
	m_BoundTop = 0.0f;
	m_BoundBottom = 0.0f;
	m_TotalHeight = 0.0f;
	m_TotalWidth = 0.0f;

	m_aPropellers = new SPropeller[NumPropellers];
	m_NumPropellers = NumPropellers;

	// Call PostConstruction() for overrides
}

SBoneModel::~SBoneModel()
{
	// Clean bone ids
	for (int i = 0; i < m_NumBones; i++)
	{
		if (m_aBones[i].m_ID == -1)
			continue;

		Server()->SnapFreeID(m_aBones[i].m_ID);
		m_aBones[i].m_ID = -1;
	}

	// Clean trail ids
	for (int i = 0; i < m_NumTrails; i++)
	{
		if (m_aTrails[i].m_ID == -1)
			continue;

		Server()->SnapFreeID(m_aTrails[i].m_ID);
		m_aTrails[i].m_ID = -1;
	}

	delete[] m_aBones;
	delete[] m_aTrails;
	delete[] m_aPropellers;
}

void SBoneModel::PostConstruction()
{
	InitModel();
	InitLinkPropellers();
	UpdateBounds();
}

IServer *SBoneModel::Server()
{
	return m_pEntity->Server();
}

void SBoneModel::UpdateBounds()
{
	float Lowest = -std::numeric_limits<float>::infinity();
	float Highest = std::numeric_limits<float>::infinity();
	float Leftest = std::numeric_limits<float>::infinity();
	float Rightest = -std::numeric_limits<float>::infinity();

	for (int i = 0; i < m_NumBones; i++)
	{
		const vec2& from = m_aBones[i].m_From;
		const vec2& to = m_aBones[i].m_To;

		Lowest = std::max({ Lowest, from.y, to.y });
		Highest = std::min({ Highest, from.y, to.y });
		Leftest = std::min({ Leftest, from.x, to.x });
		Rightest = std::max({ Rightest, from.x, to.x });
	}

	m_BoundTop = Highest;
	m_BoundBottom = Lowest;
	m_BoundLeft = Leftest;
	m_BoundRight = Rightest;
	m_TotalHeight = Lowest - Highest;
	m_TotalWidth = Rightest - Leftest;
}

void SBoneModel::ApplyScale(float Scale)
{
	ApplyScaleBones(Scale);
	ApplyScalePropellers(Scale);
}

void SBoneModel::Flip()
{
	for (int i = 0; i < m_NumBones; i++)
		m_aBones[i].Flip();
}

void SBoneModel::SetRotation(float NewRotation)
{
	for (int i = 0; i < m_NumBones; i++)
	{
		m_aBones[i].Reinit();
		m_aBones[i].Rotate(NewRotation);
	}
}

void SBoneModel::InitBuildAnimation()
{
	for (int i = 0; i < m_NumBones; i++)
	{
		Bones()[i].m_Enabled = false;
		Bones()[i].m_Thickness = 0;
		Bones()[i].m_Color = LASERTYPE_FREEZE;
	}

	for (int i = 0; i < m_NumTrails; i++)
		Trails()[i].m_Enabled = false;
}

void SBoneModel::Snap(int SnappingClient, bool SendTrails, bool Flipped, int VertexSnapping)
{
	for (int i = 0; i < m_NumBones; i++)
		Bones()[i].Snap(SnappingClient, Flipped, VertexSnapping);

	if (SendTrails)
		for (int i = 0; i < m_NumTrails; i++)
			Trails()[i].Snap(SnappingClient, Flipped, VertexSnapping);
}

void SBoneModel::UpdateLastPropellerPositions()
{
	for (int i = 0; i < m_NumPropellers; i++)
		m_aPropellers[i].UpdateLastPositions();
}

void SBoneModel::ResetPropellers()
{
	for (int i = 0; i < m_NumPropellers; i++)
		m_aPropellers[i].Reset();
}
