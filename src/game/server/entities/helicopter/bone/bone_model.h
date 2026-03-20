//
// Created by Matq on 11/04/2025.
//

#ifndef GAME_SERVER_ENTITIES_HELICOPTER_BONE_H
#define GAME_SERVER_ENTITIES_HELICOPTER_BONE_H

#include "bone.h"

class IBoneModel
{
protected:
	CEntity *m_pEntity;

	CBone *m_aBones;
	int m_NumBones;

	CTrailNode *m_aTrails;
	int m_NumTrails;

	SBounds m_Bounds;
	vec2 m_TotalSize;

	void SetBonesRotation(float NewRotation);
	void ApplyScaleBones(float Scale);
	virtual void InitModel() = 0;

public:
	IBoneModel(CEntity *pEntity, int NumBones, int NumTrails);
	virtual ~IBoneModel();
	virtual void PostConstructor();

	// Getting
	CEntity *Entity() { return m_pEntity; }
	CBone *Bones() { return m_aBones; } // size: m_NumBones
	CTrailNode *Trails() { return m_aTrails; } // size: m_NumTrails
	int NumBones() { return m_NumBones; }
	int NumTrails() { return m_NumTrails; }
	const SBounds& GetCachedBounds() { return m_Bounds; }
	vec2 GetTotalSize() { return m_TotalSize; }
	IServer *Server();

	// Manipulating
	void InitBuildAnimation();
	virtual void SetRotation(float NewRotation);
	virtual void ApplyScale(float Scale);
	void UpdateAndCacheBounds();

	// Ticking
	void Snap(int SnappingClient, bool SendTrails, bool Flipped = false, float VertexSnapping = 0.0f, bool RainbowMode = false);
};

#endif // GAME_SERVER_ENTITIES_HELICOPTER_BONE_H
