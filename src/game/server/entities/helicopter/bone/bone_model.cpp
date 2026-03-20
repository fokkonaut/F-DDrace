//
// Created by Matq on 11/04/2025.
//

#include "../../../gamecontext.h"
#include "bone_model.h"

void IBoneModel::SetBonesRotation(float NewRotation)
{
	for (int i = 0; i < m_NumBones; i++)
	{
		m_aBones[i].LoadPositions();
		m_aBones[i].Rotate(NewRotation);
	}
}

void IBoneModel::ApplyScaleBones(float Scale)
{
	for (int i = 0; i < m_NumBones; i++)
		Bones()[i].Scale(Scale);

	m_Bounds.m_Top *= Scale;
	m_Bounds.m_Bottom *= Scale;
	m_Bounds.m_Left *= Scale;
	m_Bounds.m_Right *= Scale;

	m_TotalSize *= Scale;
}

IBoneModel::IBoneModel(CEntity *pEntity, int NumBones, int NumTrails)
{
	m_pEntity = pEntity;

	m_aBones = NumBones ? new CBone[NumBones] : nullptr;
	m_NumBones = NumBones;

	m_aTrails = NumTrails ? new CTrailNode[NumTrails] : nullptr;
	m_NumTrails = NumTrails;

	m_Bounds = { 0, 0, 0, 0 };
	m_TotalSize = vec2(0, 0);

	// Call PostConstructor() for overrides
}

IBoneModel::~IBoneModel()
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
}

void IBoneModel::PostConstructor()
{
	InitModel();
	UpdateAndCacheBounds();
}

IServer *IBoneModel::Server()
{
	return m_pEntity->Server();
}

void IBoneModel::InitBuildAnimation()
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

void IBoneModel::SetRotation(float NewRotation)
{
	SetBonesRotation(NewRotation);
}

void IBoneModel::ApplyScale(float Scale)
{
	ApplyScaleBones(Scale);
}

void IBoneModel::UpdateAndCacheBounds()
{
	m_Bounds = { 0, 0, 0, 0 };
	for (int i = 0; i < m_NumBones; i++)
	{
		SBounds BoneBounds = m_aBones[i].GetBounds();
		m_Bounds.Expand(BoneBounds);
	}

	m_TotalSize = m_Bounds.GetSize();
}

void IBoneModel::Snap(int SnappingClient, bool SendTrails, bool Flipped, float VertexSnapping, bool RainbowMode)
{
	for (int i = 0; i < m_NumBones; i++)
		Bones()[i].Snap(SnappingClient, Flipped, VertexSnapping, RainbowMode);

	if (SendTrails)
		for (int i = 0; i < m_NumTrails; i++)
			Trails()[i].Snap(SnappingClient, Flipped, VertexSnapping);
}
