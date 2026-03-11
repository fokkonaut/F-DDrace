// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>
#include "drawtile.h"

CDrawTile::CDrawTile(CGameWorld *pGameWorld, vec2 Pos, int Index, int Color, bool Collision)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_DRAWTILE, Pos, 16.f, Collision)
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
		int MapIndex = GameServer()->Collision()->GetMapIndex(m_Pos);
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

void CDrawTile::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	CCharacter *pChr = GameServer()->GetPlayerChar(SnappingClient);
	if (pChr && pChr->m_DrawEditor.OnSnapPreview(this))
		return;

	int SnappingClientVersion = GameServer()->GetClientDDNetVersion(SnappingClient);
	CSnapContext Context(SnappingClientVersion, Server()->IsSevendown(SnappingClient), SnappingClient);
	vec2 aCorners[4] = {
		vec2(-12, -12),
		vec2(12, -12),
		vec2(12, 12),
		vec2(-12, 12),
	};

	for (int i = 0; i < NUM_SIDES; i++)
	{
		int To = i == POINT_LEFT ? POINT_TOP : i+1;

		vec2 Pos = m_Pos + aCorners[i];
		vec2 From = m_Pos + aCorners[To];
		GameServer()->SnapLaserObject(Context, m_aID[i], Pos, From, Server()->Tick(), -1, m_Color, -1, m_Number, LASERFLAG_NO_PREDICT);
	}
}
