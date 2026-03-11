// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_DRAWTILE_H
#define GAME_SERVER_ENTITIES_DRAWTILE_H

#include <game/server/entity.h>

class CDrawTile : public CEntity
{
public:
	enum
	{
		SIDE_TOP,
		SIDE_RIGHT,
		SIDE_BOTTOM,
		SIDE_LEFT,
		NUM_SIDES
	};

private:

	int m_aID[NUM_SIDES];
	int m_Index;
	int m_Color;

	bool HasSameIndexNeighbor(int Side);
	bool HasEdge(vec2 Pos, int Side);
	bool HasSameIndexNeighborAt(vec2 Pos);
	bool IsResponsibleForEdge(int Side);
	vec2 ExtendEdgeEnd(int Side, vec2 From);
	bool HasInsideCornerAt(vec2 TilePos, int CornerIndex);

public:
	CDrawTile(CGameWorld *pGameWorld, vec2 Pos, int Index, int Color, bool Collision = true);
	virtual ~CDrawTile();
	virtual void ResetCollision(bool Remove = false);
	virtual void Snap(int SnappingClient);
	int GetIndex() { return m_Index; }
	void SetColor(int Lasertype) { m_Color = Lasertype; }
	int GetColor() { return m_Color; }
};

#endif // GAME_SERVER_ENTITIES_DRAWTILE_H
