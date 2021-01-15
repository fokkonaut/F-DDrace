// made by fokkonaut and ChillerDragon

#include "dummybase.h"
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include "macros.h"

CGameContext *CDummyBase::GameServer() const { return m_pCharacter->GameServer(); }
CGameWorld *CDummyBase::GameWorld() const { return m_pCharacter->GameWorld(); }
IServer *CDummyBase::Server() const { return GameServer()->Server(); }

int CDummyBase::HookState() { return m_pCharacter->Core()->m_HookState; }
int CDummyBase::Jumped() { return m_pCharacter->Core()->m_Jumped; }
int CDummyBase::Jumps() { return m_pCharacter->Core()->m_Jumps; }
bool CDummyBase::IsGrounded() { return m_pCharacter->IsGrounded(); }
void CDummyBase::SetWeapon(int Weapon) { m_pCharacter->SetWeapon(Weapon); }
void CDummyBase::Die() { m_pCharacter->Die(); }

CDummyBase::CDummyBase(CCharacter *pChr, int Mode)
{
	m_pCharacter = pChr;
	m_pPlayer = pChr->GetPlayer();
	m_Mode = Mode;
}

void CDummyBase::Tick()
{
	if (!m_pPlayer->m_IsDummy || m_pPlayer->m_TeeControllerID != -1)
		return;

	// Prepare input
	m_pCharacter->ResetInput();
	RELEASE_HOOK;

	// Then start controlling
	OnTick();
}

void CDummyBase::AvoidFreeze()
{
	#define FREEZE(x, y) (GameServer()->Collision()->GetTileRaw(_(x), _(y)) == TILE_FREEZE)
	#define AIR(x, y) (GameServer()->Collision()->GetTileRaw(_(x), _(y)) == TILE_AIR)
	#define SOLID(x, y) GameServer()->Collision()->IsSolid(_(x), _(y))

	// sides
	if (FREEZE(X+1, Y))
		LEFT;
	if (FREEZE(X-1, Y))
		RIGHT;

	// corners
	if (FREEZE(X+1, Y-1) && !FREEZE(X-1, Y))
		LEFT;
	if (FREEZE(X-1, Y-1) && !FREEZE(X+1, Y))
		RIGHT;

	// small edges
	if (AIR(X-1, Y) && FREEZE(X-1, Y+1))
		RIGHT;

	if (AIR(X+1, Y) && FREEZE(X+1, Y+1))
		LEFT;
		
	// big edges
	if (AIR(X-1, Y) && AIR(X-2, Y) && AIR(X-2, Y+1) && FREEZE(X-2, Y+1))
		RIGHT;
	if (AIR(X+1, Y) && AIR(X+2, Y) && AIR(X+2, Y+1) && FREEZE(X+2, Y+1))
		LEFT;
		
	// while falling
	if (FREEZE(X, Y+VEL_Y))
	{
		if(SOLID(X-VEL_Y, Y+VEL_Y))
			RIGHT;
		if(SOLID(X+VEL_Y, Y+VEL_Y))
			LEFT;
	}

	#undef FREEZE
	#undef AIR
	#undef SOLID
}
