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

	struct SLaserEdge
	{
		int m_ID;
		bool m_Active;
		vec2 m_To;
		vec2 m_From;
	} m_aSides[NUM_SIDES];

	int m_Index;
	int m_Color;
	int m_TuneNumber;

	bool HasSameIndexNeighbor(int Side);
	bool HasEdge(vec2 Pos, int Side);
	bool HasSameIndexNeighborAt(vec2 Pos);
	bool IsResponsibleForEdge(int Side);
	vec2 ExtendEdgeEnd(int Side, vec2 From);
	bool HasInsideCornerAt(vec2 TilePos, int CornerIndex);

	bool m_CacheValid;
	void PrepareForCaching();
	void UpdateSnapCache();

public:
	CDrawTile(CGameWorld *pGameWorld, vec2 Pos, int Index, int Color, int TuneNumber = -1, bool Collision = true);
	virtual ~CDrawTile();
	virtual void ResetCollision(bool Remove = false);
	virtual void Snap(int SnappingClient);
	void SetIndex(int Index) { m_Index = Index; }
	int GetIndex() { return m_Index; }
	void SetColor(int Lasertype) { m_Color = Lasertype; }
	int GetColor() { return m_Color; }
	void SetTuneNumber(int Number) { m_TuneNumber = Number; }
	int GetTuneNumber() { return m_TuneNumber; }

	void SetPos(vec2 Pos) override;
};

#endif // GAME_SERVER_ENTITIES_DRAWTILE_H
