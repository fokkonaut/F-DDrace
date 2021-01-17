// made by fokkonaut

#include "house.h"
#include <game/server/gamecontext.h>

#include "macros.h"

CDummyHouse::CDummyHouse(CCharacter *pChr, int Mode)
: CDummyBase(pChr, Mode)
{
}

void CDummyHouse::OnTick()
{
	int Type;
	switch (Mode())
	{
	case DUMMYMODE_SHOP_DUMMY: Type = HOUSE_SHOP; break;
	case DUMMYMODE_PLOT_SHOP_DUMMY: Type = HOUSE_PLOT_SHOP; break;
	case DUMMYMODE_BANK_DUMMY: Type = HOUSE_BANK; break;
	default: return;
	}

	CCharacter *pChr = GameWorld()->ClosestCharacter(GetPos(), m_pCharacter, m_pPlayer->GetCID(), 9);
	if (pChr && GameServer()->m_pHouses[Type]->IsInside(pChr->GetPlayer()->GetCID()))
	{
		AimPos(pChr->GetPos());
	}

	if (!GameServer()->m_pHouses[Type]->IsInside(m_pPlayer->GetCID()) && m_pPlayer->m_ForceSpawnPos == vec2(-1, -1) && TicksPassed(400))
	{
		Die();
		return;
	}
}