// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include "treasurechest.h"

CTreasureChest::CTreasureChest(CGameWorld *pGameWorld, vec2 Pos, int Owner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_TREASURECHEST, Pos)
{
	m_Pos = Pos;
	m_Owner = Owner;
	m_StartTick = Server()->Tick();
	GameWorld()->InsertEntity(this);
}

CTreasureChest::~CTreasureChest()
{

}

void CTreasureChest::Tick()
{

}

void CTreasureChest::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
	if (!pObj)
		return;

	pObj->m_X = round_to_int(m_Pos.x);
	pObj->m_Y = round_to_int(m_Pos.y);
	pObj->m_FromX = round_to_int(m_Pos.x);
	pObj->m_FromY = round_to_int(m_Pos.y);
	pObj->m_StartTick = Server()->Tick();
}
