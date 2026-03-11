// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_DRAWTILE_H
#define GAME_SERVER_ENTITIES_DRAWTILE_H

#include <game/server/entity.h>

class CDrawTile : public CEntity
{
	enum
	{
		POINT_TOP,
		POINT_RIGHT,
		POINT_BOTTOM,
		POINT_LEFT,
		NUM_SIDES
	};

	int m_aID[NUM_SIDES];
	int m_Index;
	int m_Color;

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
