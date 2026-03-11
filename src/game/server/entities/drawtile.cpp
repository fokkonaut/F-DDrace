// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>
#include "drawtile.h"

CDrawTile::CDrawTile(CGameWorld *pGameWorld, vec2 Pos, int Index, int Color, bool Collision)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_DRAWTILE, Pos, 14.f, Collision)
{
	m_Index = Index;
	m_Color = Color;

	for (int i = 0; i < NUM_SIDES; i++)
		m_aID[i] = Server()->SnapNewID();

	ResetCollision();
	GameWorld()->InsertEntity(this);
}

CDrawTile::~CDrawTile()
{
	ResetCollision(true);
	for (int i = 0; i < NUM_SIDES; i++)
		Server()->SnapFreeID(m_aID[i]);
}

void CDrawTile::ResetCollision(bool Remove)
{
	// For preview, we cant use m_BrushCID here yet because when the entity is created its not set yet
	if (!m_Collision)
		return;

	// never allow overriding map tiles or other tiles.
	if (!Remove)
	{
		int MapIndex = GameServer()->Collision()->GetPureMapIndex(m_Pos);
		int TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
		int TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);

		m_Layer = TileIndex == TILE_AIR ? LAYER_GAME : TileFIndex == TILE_AIR ? LAYER_FRONT : -1;
		if (m_Layer == -1)
			return; // shouldnt happen
	}

	int Index = m_Index;
	if (Remove)
	{
		Index = TILE_AIR;
		m_Collision = false;
	}

	if (m_Layer == LAYER_GAME)
		GameServer()->Collision()->SetCollisionAt(m_Pos.x, m_Pos.y, Index);
	else if (m_Layer == LAYER_FRONT)
		GameServer()->Collision()->SetFCollisionAt(m_Pos.x, m_Pos.y, Index);
}

bool CDrawTile::HasSameIndexNeighborAt(vec2 Pos)
{
	int MapIndex = GameServer()->Collision()->GetPureMapIndex(Pos);
	int TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	int TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);
	return TileIndex == m_Index || TileFIndex == m_Index;
}

static vec2 s_aNeighborOffsets[CDrawTile::NUM_SIDES] = {
	vec2(0, -32),
	vec2(32, 0),
	vec2(0, 32),
	vec2(-32, 0)
};

bool CDrawTile::HasSameIndexNeighbor(int Side)
{
	if (m_BrushCID != -1)
		return false;
	vec2 NeighborPos = m_Pos + s_aNeighborOffsets[Side];
	return HasSameIndexNeighborAt(NeighborPos);
}

bool CDrawTile::HasEdge(vec2 Pos, int Side)
{
	return !HasSameIndexNeighborAt(Pos + s_aNeighborOffsets[Side]);
}

bool CDrawTile::IsResponsibleForEdge(int Side)
{
	if (m_BrushCID != -1)
		return true;

	vec2 aScanDirs[4] = {
		vec2(-32, 0), // left
		vec2(0, -32), // top
		vec2(32, 0), // right
		vec2(0, 32) // down
	};

	vec2 NeighborPos = m_Pos + aScanDirs[Side];
	// if theres no tile, we are responsible or if the tile has no edge on this side
	return !HasSameIndexNeighborAt(NeighborPos) || !HasEdge(NeighborPos, Side);
}

bool CDrawTile::HasInsideCornerAt(vec2 TilePos, int CornerIndex)
{
	vec2 aDiagonals[4] = {
		vec2(-32, -32), // top left
		vec2(32, -32), // top right
		vec2(32, 32), // bottom right
		vec2(-32, 32) // bottom left
	};
	
	vec2 aNeighborChecks[4][2] = {
		{ vec2(0, -32), vec2(-32, 0) }, // top left
		{ vec2(0, -32), vec2(32, 0) }, // top right
		{ vec2(0, 32), vec2(32, 0) }, // bottom right
		{ vec2(0, 32), vec2(-32, 0) } // bottom left
	};
	
	bool HasDiagonal = HasSameIndexNeighborAt(TilePos + aDiagonals[CornerIndex]);
	bool FirstMissing = !HasSameIndexNeighborAt(TilePos + aNeighborChecks[CornerIndex][0]);
	bool SecondMissing = !HasSameIndexNeighborAt(TilePos + aNeighborChecks[CornerIndex][1]);
	
	return HasDiagonal && (FirstMissing || SecondMissing);
}

void CDrawTile::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	CCharacter *pChr = GameServer()->GetPlayerChar(SnappingClient);
	if (pChr && pChr->m_DrawEditor.OnSnapPreview(this))
		return;

	int SnappingClientVersion = GameServer()->GetClientDDNetVersion(SnappingClient);
	CSnapContext Context(SnappingClientVersion, Server()->IsSevendown(SnappingClient), SnappingClient);

	const int CornerOffset = 12;
	const int ExtendBy = ((32 / 2) - CornerOffset) * 2;
	vec2 aCorners[4] = {
		vec2(-CornerOffset, -CornerOffset),
		vec2(CornerOffset, -CornerOffset),
		vec2(CornerOffset, CornerOffset),
		vec2(-CornerOffset, CornerOffset),
	};

	for (int i = 0; i < NUM_SIDES; i++)
	{
		if (HasSameIndexNeighbor(i) || !IsResponsibleForEdge(i))
			continue;

		int StartCorner = i;
		int EndCorner = (i == SIDE_LEFT) ? SIDE_TOP : i + 1;

		vec2 Pos = m_Pos + aCorners[StartCorner];
		vec2 From = m_Pos + aCorners[EndCorner];

		// only if placed and not a brush
		if (m_BrushCID == -1)
		{
			// adjust start corner if its an inside corner
			if (HasInsideCornerAt(m_Pos, StartCorner))
				Pos -= normalize(From - Pos) * ExtendBy;
				
			vec2 aDirections[4] = {
				vec2(32, 0), // top
				vec2(0, 32), // right
				vec2(-32, 0), // bottom
				vec2(0, -32) // left
			};

			vec2 CheckPos = m_Pos;
			while (true)
			{
				vec2 NextPos = CheckPos + aDirections[i];
				if (!HasSameIndexNeighborAt(NextPos) || !HasEdge(NextPos, i))
					break;

				From += aDirections[i];
				CheckPos = NextPos;
			}
			
			// adjust end corner if the last extended tile has an inside corner
			if (HasInsideCornerAt(CheckPos, EndCorner))
				From += normalize(aDirections[i]) * ExtendBy;
		}

		GameServer()->SnapLaserObject(Context, m_aID[i], Pos, From, Server()->Tick(), -1, m_Color, -1, m_Number, LASERFLAG_NO_PREDICT);
	}
}
