//
// Created by Matq on 16/03/2026.
//

#ifndef GAME_SERVER_ENTITIES_BONE_BASE_BONE_H
#define GAME_SERVER_ENTITIES_BONE_BASE_BONE_H

#include "game/server/entity.h"

struct SBounds
{
	float m_Left;
	float m_Top;
	float m_Right;
	float m_Bottom;

	// Getting
	vec2 GetSize() { return { m_Right - m_Left, m_Bottom - m_Top }; }

	// Manipulating
	void Expand(const SBounds& IncludeArea)
	{
		m_Left = minimum(m_Left, IncludeArea.m_Left);
		m_Top = minimum(m_Top, IncludeArea.m_Top);
		m_Right = maximum(m_Right, IncludeArea.m_Right);
		m_Bottom = maximum(m_Bottom, IncludeArea.m_Bottom);
	}
};

struct SBoneModelSnapping
{
	bool m_SendTrails;
	bool m_Flipped;
	float m_VertexSnapping;
	bool m_RainbowMode;
};

class CBone
{
public:
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

	CBone(
		CEntity *pEntity = nullptr,
		int SnapID = -1,
		float FromX = 0,
		float FromY = 0,
		float ToX = 0,
		float ToY = 0,
		int Thickness = 5,
		int Color = LASERTYPE_RIFLE
	);
	CBone(
		CEntity *pEntity,
		int SnapID,
		vec2 From,
		vec2 To,
		int Thickness = 5,
		int Color = LASERTYPE_RIFLE
	);

	// Getting
	IServer *Server() { return m_pEntity->Server(); }
	CGameContext *GameServer() { return m_pEntity->GameServer(); }
	float GetLength() { return distance(m_To, m_From); }
	SBounds GetBounds();

	// Manipulating
	void AssignEntityAndID(CEntity *pEntity, int SnapID);
	void UpdateLine(vec2 From, vec2 To);
	void Flip();
	void Rotate(float Angle);
	void Scale(float factor);
	void Save();
	void SavePositions();
	void LoadPositions();

	// Ticking
	void Snap(int SnappingClient, const SBoneModelSnapping& Options);
};

class CTrailNode
{
public:
	CEntity *m_pEntity;
	int m_ID;
	vec2 *m_pPos;
	bool m_InitEnabled;
	bool m_Enabled;
	bool m_RelativeMode;

public:
	CTrailNode();
	CTrailNode(CEntity *pEntity, int SnapID, vec2 *pPos, bool RelativeMode = true, bool Enabled = true);

	// Getting
	IServer *Server() { return m_pEntity->Server(); }

	// Ticking
	void Snap(int SnappingClient, bool Flipped = false, float VertexSnapping = 0.0f);
};

class CPickupNode // commonly heart or armor
{
public:
	CEntity *m_pEntity;
	vec2 m_Pos;
	int m_ID;
	int m_PickupType;
	bool m_Enabled;

public:
	CPickupNode();
	CPickupNode(CEntity *pEntity, int SnapID, int PickupType, vec2 Pos);

	// Getting
	IServer *Server() { return m_pEntity->Server(); }
	CGameContext *GameServer() { return m_pEntity->GameServer(); }

	// Ticking
	void Snap(int SnappingClient);
};

#endif