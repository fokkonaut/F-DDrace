// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include "money.h"

CMoney::CMoney(CGameWorld *pGameWorld, vec2 Pos, int Amount)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_MONEY, Pos)
{
	m_Pos = Pos;
	m_Amount = Amount;

	GameWorld()->InsertEntity(this);
}

CMoney::~CMoney()
{

}

void CMoney::Tick()
{

}

void CMoney::Snap(int SnappingClient)
{

}
