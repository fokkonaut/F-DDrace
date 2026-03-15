//
// Created by Matq on 11/04/2025.
//

#ifndef GAME_SERVER_ENTITIES_HELICOPTER_BONE_H
#define GAME_SERVER_ENTITIES_HELICOPTER_BONE_H

#include "../../entity.h"

struct SBone
{
	CEntity *m_pEntity;
	int m_ID;
	vec2 m_InitFrom;
	vec2 m_InitTo;
	int m_InitColor;
	int m_InitThickness;
	bool m_Enabled;

	vec2 m_From;
	vec2 m_To;
	int m_Color;
	int m_Thickness;

	//	SBone() : SBone(nullptr, -1, 0, 0, 0, 0) { }
	SBone(CEntity *pEntity = nullptr,
	      int SnapID = -1,
	      float FromX = 0,
	      float FromY = 0,
	      float ToX = 0,
	      float ToY = 0,
	      int Thickness = 5)
		: SBone(pEntity, SnapID, vec2(FromX, FromY), vec2(ToX, ToY), Thickness)
	{
	}
	SBone(CEntity *pEntity, int SnapID, vec2 From, vec2 To, int Thickness = 5)
		: m_pEntity(pEntity),
		  m_ID(SnapID),
		  m_InitFrom(From),
		  m_InitTo(To),
		  m_InitColor(LASERTYPE_RIFLE),
		  m_InitThickness(Thickness),
		  m_Enabled(true),
		  m_From(From),
		  m_To(To),
		  m_Color(LASERTYPE_RIFLE),
		  m_Thickness(Thickness)
	{
	}

	// Sense
	IServer *Server() { return m_pEntity->Server(); }
	CGameContext *GameServer() { return m_pEntity->GameServer(); }
	float GetLength() { return distance(m_To, m_From); }

	// Manipulating
	void AssignEntityAndID(CEntity *pEntity, int SnapID)
	{
		m_pEntity = pEntity;
		m_ID = SnapID;
	}
	void UpdateLine(vec2 From, vec2 To)
	{
		m_From = From;
		m_To = To;
	}
	void Flip()
	{
		m_From.x *= -1;
		m_To.x *= -1;
	}
	void Rotate(float Angle)
	{
		m_From = rotate(m_From, Angle);
		m_To = rotate(m_To, Angle);
	}
	void Scale(float factor)
	{
		// Only use case where init positions matter after scaling
		m_InitFrom *= factor;
		m_InitTo *= factor;
		m_From *= factor;
		m_To *= factor;
	}
	void Reinit()
	{
		m_From = m_InitFrom;
		m_To = m_InitTo;
	}

	// Ticking
	void Snap(int SnappingClient, bool Flipped = false, int VertexSnapping = 0);
};

struct STrail
{
	CEntity *m_pEntity;
	int m_ID;
	vec2 *m_pPos;
	bool m_Enabled;

public:
	STrail() : m_pEntity(nullptr), m_ID(-1), m_pPos(nullptr)
	{
	};
	STrail(CEntity *pEntity, int SnapID, vec2 *pPos)
		: m_pEntity(pEntity), m_ID(SnapID), m_pPos(pPos), m_Enabled(true)
	{
	};

	// Sense
	IServer *Server() { return m_pEntity->Server(); }

	// Ticking
	void Snap(int SnappingClient, bool Flipped = false, int VertexSnapping = 0);
};

struct SHeart
{
	CEntity *m_pEntity;
	vec2 m_Pos;
	int m_ID;
	bool m_Enabled;

public:
	SHeart() : SHeart(nullptr, -1, vec2(0.f, 0.f))
	{
	};
	SHeart(CEntity *pEntity, int SnapID, vec2 Pos) : m_pEntity(pEntity), m_Pos(Pos), m_ID(SnapID), m_Enabled(true)
	{
	};

	// Sense
	IServer *Server() { return m_pEntity->Server(); }
	CGameContext *GameServer() { return m_pEntity->GameServer(); }

	// Ticking
	void Snap(int SnappingClient);
};

//

// Thop
struct SPropeller // (horizontal)
{
	SBone *m_pBoneA;
	SBone *m_pBoneB;
	float m_Radius;

	vec2 m_LastA;
	vec2 m_LastB;

public:
	SPropeller() : SPropeller(nullptr, nullptr, 0.0f)
	{
	}
	SPropeller(SBone *pBoneA, SBone *pBoneB, float Radius)
	{
		m_pBoneA = pBoneA;
		m_pBoneB = pBoneB;
		m_Radius = Radius;

		m_LastA = vec2(0.0f, 0.0f);
		m_LastB = vec2(0.0f, 0.0f);
	}

	// Getting
	vec2 GetCenter() { return m_pBoneA->m_To; }

	// Generating
	void GetFullPropellerPositions(vec2& outPosA, vec2& outPosB)
	{
		vec2 bladeSpan = normalize(m_pBoneA->m_To - m_pBoneA->m_From) * m_Radius;
		outPosA = m_pBoneA->m_To + bladeSpan;
		outPosB = m_pBoneA->m_To - bladeSpan;
	}

	// Manipulating
	void ApplyScale(float Scale)
	{
		m_Radius *= Scale;
	}
	void Reset()
	{
		vec2 Direction = normalize(m_pBoneA->m_From - m_pBoneA->m_To);
		m_pBoneA->m_From = m_pBoneA->m_To + Direction * m_Radius;
		m_pBoneB->m_From = m_pBoneB->m_To + Direction * -m_Radius;
	}

	// Ticking
	void UpdateLastPositions()
	{
		GetFullPropellerPositions(m_LastA, m_LastB);
	}
};

struct SBoneModel // Currently assumed as a helicopter model only
{
	CEntity *m_pEntity;

	SBone *m_aBones;
	int m_NumBones;

	STrail *m_aTrails;
	int m_NumTrails;

	float m_BoundTop;
	float m_BoundBottom;
	float m_BoundLeft;
	float m_BoundRight;
	float m_TotalHeight;
	float m_TotalWidth;

	virtual void InitModel();
	void ApplyScaleBones(float Scale);
	void ApplyScalePropellers(float Scale); // Thop

	// Temporary helicopter only part (Thop)
	SPropeller *m_aPropellers;
	int m_NumPropellers;
	virtual void InitLinkPropellers()
	{
	}
	// Thop end

public:
	SBoneModel(CEntity *pEntity, int NumBones, int NumTrails, int NumPropellers);
	virtual ~SBoneModel();
	void PostConstruction();

	// Sense
	CEntity *Entity() { return m_pEntity; }
	SBone *Bones() { return m_aBones; } // size: m_NumBones
	STrail *Trails() { return m_aTrails; } // size: m_NumTrails
	SPropeller *Propellers() { return m_aPropellers; } // size: m_NumPropellers
	IServer *Server();

	// Manipulating
	void UpdateBounds();

	virtual void ApplyScale(float Scale);
	void Flip();
	void SetRotation(float NewRotation);

	void InitBuildAnimation();

	// Ticking
	void Snap(int SnappingClient, bool SendTrails, bool Flipped = false, int VertexSnapping = 0);

	// Temporary helicopter only part (Thop)
	void UpdateLastPropellerPositions();
	void ResetPropellers();
	// Thop end
};

#endif // GAME_SERVER_ENTITIES_HELICOPTER_BONE_H
