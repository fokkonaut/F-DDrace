// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>
#include "drawtile.h"

CDrawTile::CDrawTile(CGameWorld *pGameWorld, vec2 Pos, int Index, int Color, int TuneNumber, bool Collision)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_DRAWTILE, Pos, 14.f, Collision)
{
	m_Index = Index;
	m_Color = Color;
	m_TuneNumber = TuneNumber;

	for (int i = 0; i < NUM_SIDES; i++)
		m_aSides[i].m_ID = Server()->SnapNewID();

	PrepareForCaching();
	ResetCollision();
	GameWorld()->InsertEntity(this);
}

CDrawTile::~CDrawTile()
{
	// Performance saving by handling draw tiles differently while keeping them supported as entity for saving, loading, and the draweditor itself
	// Drawtiles do not need Tick() etc only Snap()
	GameWorld()->m_DrawTiles.MarkForRemoval(this);
	
	ResetCollision(true);
	for (int i = 0; i < NUM_SIDES; i++)
		Server()->SnapFreeID(m_aSides[i].m_ID);
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
		if (m_TuneNumber != -1)
		{
			m_Layer = LAYER_TUNE;
			if (m_Index == TILE_TUNELOCK_RESET)
				m_TuneNumber = 0;
			if (GameServer()->Collision()->IsTune(MapIndex) || GameServer()->Collision()->IsTuneLock(MapIndex))
				m_Layer = -1;
		}
		else
		{
			int TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
			int TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);
			m_Layer = TileIndex == TILE_AIR ? LAYER_GAME : TileFIndex == TILE_AIR ? LAYER_FRONT : -1;
		}

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
	else if (m_Layer == LAYER_TUNE)
		GameServer()->Collision()->SetTuneCollisionAt(m_Pos.x, m_Pos.y, Index, m_TuneNumber);

	// Update other tiles
	CDrawTile *pDrawTile = (CDrawTile *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_DRAWTILE);
	for (; pDrawTile; pDrawTile = (CDrawTile *)pDrawTile->TypeNext())
	{
		bool IsNoTune = m_Layer != LAYER_TUNE && pDrawTile->m_Layer != LAYER_TUNE;
		bool SameTuneTile = m_Layer == LAYER_TUNE && pDrawTile->m_Layer == LAYER_TUNE && pDrawTile->GetTuneNumber() == m_TuneNumber;
		if (pDrawTile->GetIndex() == m_Index && (IsNoTune || SameTuneTile))
			pDrawTile->PrepareForCaching();
	}
}

void CDrawTile::SetPos(vec2 Pos)
{
	CEntity::SetPos(Pos);
	// specifically for draweditor preview position updating
	PrepareForCaching();
}

void CDrawTile::PrepareForCaching()
{
	GameWorld()->m_DrawTiles.Insert(this);
	m_CacheValid = false;
	for (int i = 0; i < NUM_SIDES; i++)
		m_aSides[i].m_Active = false;
}

bool CDrawTile::HasSameIndexNeighborAt(vec2 Pos)
{
	int MapIndex = GameServer()->Collision()->GetPureMapIndex(Pos);

	// only check for real tiles, not when moving or copying, only rely on drawtile entity info for that
	if (m_BrushCID == -1)
	{
		if (m_TuneNumber != -1)
		{
			int Zone = GameServer()->Collision()->IsTune(MapIndex);
			if (Zone && (m_Index != TILE_TUNE || m_TuneNumber != Zone))
				return false;
			int Lock = GameServer()->Collision()->IsTuneLock(MapIndex);
			if (Lock == -1 && m_Index != TILE_TUNELOCK_RESET)
				return false;
			if (Lock > 0 && (m_Index != TILE_TUNELOCK || m_TuneNumber != Lock))
				return false;
		}
		else
		{
			int TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
			int TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);
			if (TileIndex != m_Index && TileFIndex != m_Index)
				return false;
		}
	}

	// dont try to connect to map tiles, only connect to already placed tiles or when moving an area together
	return GameServer()->HasDrawTile(MapIndex, this);
}

static vec2 s_aNeighborOffsets[CDrawTile::NUM_SIDES] = {
	vec2(0, -32),
	vec2(32, 0),
	vec2(0, 32),
	vec2(-32, 0)
};

bool CDrawTile::HasSameIndexNeighbor(int Side)
{
	vec2 NeighborPos = m_Pos + s_aNeighborOffsets[Side];
	return HasSameIndexNeighborAt(NeighborPos);
}

bool CDrawTile::HasEdge(vec2 Pos, int Side)
{
	return !HasSameIndexNeighborAt(Pos + s_aNeighborOffsets[Side]);
}

bool CDrawTile::IsResponsibleForEdge(int Side)
{
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
	CCharacter *pChr = GameServer()->GetPlayerChar(SnappingClient);
	if (pChr && pChr->m_DrawEditor.OnSnapPreview(this))
		return;

	int SnappingClientVersion = GameServer()->GetClientDDNetVersion(SnappingClient);
	CSnapContext Context(SnappingClientVersion, Server()->IsSevendown(SnappingClient), SnappingClient);

	if (!m_CacheValid)
		UpdateSnapCache();

	for (int i = 0; i < NUM_SIDES; i++)
		if (m_aSides[i].m_Active && !NetworkClippedLine(SnappingClient, m_aSides[i].m_To, m_aSides[i].m_From))
			GameServer()->SnapLaserObject(Context, m_aSides[i].m_ID, m_aSides[i].m_To, m_aSides[i].m_From, Server()->Tick(), -1, m_Color, -1, m_Number, LASERFLAG_NO_PREDICT);
}

void CDrawTile::UpdateSnapCache()
{
	const int CornerOffset = 10;
	const int ExtendBy = ((32 / 2) - CornerOffset) * 2;
	vec2 aCorners[4] = {
		vec2(-CornerOffset, -CornerOffset),
		vec2(CornerOffset, -CornerOffset),
		vec2(CornerOffset, CornerOffset),
		vec2(-CornerOffset, CornerOffset),
	};

	vec2 aDirections[4] = {
		vec2(32, 0), // top
		vec2(0, 32), // right
		vec2(-32, 0), // bottom
		vec2(0, -32) // left
	};

	bool MarkForRemoval = true;
	for (int i = 0; i < NUM_SIDES; i++)
	{
		if (HasSameIndexNeighbor(i) || !IsResponsibleForEdge(i))
			continue;

		int StartCorner = i;
		int EndCorner = (i == SIDE_LEFT) ? SIDE_TOP : i + 1;

		vec2 Pos = m_Pos + aCorners[StartCorner];
		vec2 From = m_Pos + aCorners[EndCorner];
		
		// adjust start corner if its an inside corner
		if (HasInsideCornerAt(m_Pos, StartCorner))
			Pos -= normalize(From - Pos) * ExtendBy;

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

		m_aSides[i].m_To = Pos;
		m_aSides[i].m_From = From;
		m_aSides[i].m_Active = true;
		MarkForRemoval = false;
	}

	m_CacheValid = true;
	if (MarkForRemoval)
	{
		GameWorld()->m_DrawTiles.MarkForRemoval(this);
	}
}
