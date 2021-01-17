// made by fokkonaut and ChillerDragon

#include "dummybase.h"
#include <game/server/gamecontext.h>

#include "macros.h"

CGameContext *CDummyBase::GameServer() const { return m_pCharacter->GameServer(); }
CGameWorld *CDummyBase::GameWorld() const { return m_pCharacter->GameWorld(); }
IServer *CDummyBase::Server() const { return GameServer()->Server(); }

bool CDummyBase::TicksPassed(int Ticks) { return Server()->Tick() % Ticks == 0; }
vec2 CDummyBase::GetPos() { return m_pCharacter->Core()->m_Pos; }
vec2 CDummyBase::GetVel() { return m_pCharacter->Core()->m_Vel; }
int CDummyBase::HookState() { return m_pCharacter->Core()->m_HookState; }
int CDummyBase::Jumped() { return m_pCharacter->Core()->m_Jumped; }
int CDummyBase::Jumps() { return m_pCharacter->Core()->m_Jumps; }
bool CDummyBase::IsGrounded() { return m_pCharacter->IsGrounded(); }
void CDummyBase::SetWeapon(int Weapon) { m_pCharacter->SetWeapon(Weapon); }
void CDummyBase::Die() { m_pCharacter->Die(); }

void CDummyBase::Left() { m_pCharacter->Input()->m_Direction = -1; }
void CDummyBase::Right() { m_pCharacter->Input()->m_Direction = 1; }
void CDummyBase::StopMoving() { m_pCharacter->Input()->m_Direction = 0; }
void CDummyBase::Hook(bool Stroke) { m_pCharacter->Input()->m_Hook = Stroke; }
void CDummyBase::Jump(bool Stroke) { m_pCharacter->Input()->m_Jump = Stroke; }
void CDummyBase::Aim(int TargetX, int TargetY) { AimX(TargetX); AimY(TargetY); }
void CDummyBase::AimX(int TargetX) { m_pCharacter->LatestInput()->m_TargetX = TargetX; m_pCharacter->Input()->m_TargetX = TargetX; }
void CDummyBase::AimY(int TargetY) { m_pCharacter->LatestInput()->m_TargetY = TargetY; m_pCharacter->Input()->m_TargetY = TargetY; }
void CDummyBase::AimPos(vec2 Pos) { Aim(Pos.x - RAW_X, Pos.y - RAW_Y); }
void CDummyBase::Fire(bool Stroke)
{
	if (Stroke)
	{
		m_pCharacter->LatestInput()->m_Fire++;
		m_pCharacter->Input()->m_Fire++;
	}
	else
	{
		m_pCharacter->LatestInput()->m_Fire = 0;
		m_pCharacter->Input()->m_Fire = 0;
	}
}

CDummyBase::CDummyBase(CCharacter *pChr, int Mode)
{
	m_pCharacter = pChr;
	m_pPlayer = pChr->GetPlayer();
	m_Mode = Mode;
}

void CDummyBase::Tick()
{
	if (!m_pCharacter->IsAlive() || !m_pPlayer->m_IsDummy || m_pPlayer->m_TeeControllerID != -1)
		return;

	// Prepare input
	m_pCharacter->ResetInput();
	Hook(0);

	// Then start controlling
	OnTick();
}

void CDummyBase::AvoidFreeze()
{
	#define TILE(x, y) GameServer()->Collision()->GetTileRaw(RAW(x), RAW(y))
	#define FTILE(x, y) GameServer()->Collision()->GetFTileRaw(RAW(x), RAW(y))
	#define FREEZE(x, y) (TILE(x, y) == TILE_FREEZE || FTILE(x, y) == TILE_FREEZE || TILE(x, y) == TILE_DFREEZE || FTILE(x, y) == TILE_DFREEZE)
	#define AIR(x, y) !FREEZE(x, y)
	#define SOLID(x, y) GameServer()->Collision()->IsSolid(RAW(x), RAW(y))

	// sides
	if (FREEZE(X+1, Y))
		Left();
	if (FREEZE(X-1, Y))
		Right();

	// corners
	if (AIR(X-1, Y) && FREEZE(X+1, Y-1))
		Left();
	if (AIR(X+1, Y) && FREEZE(X-1, Y-1))
		Right();

	// small edges
	if (AIR(X-1, Y) && FREEZE(X-1, Y+1))
		Right();

	if (AIR(X+1, Y) && FREEZE(X+1, Y+1))
		Left();
		
	// big edges
	if (AIR(X-1, Y) && AIR(X-2, Y) && AIR(X-2, Y+1) && FREEZE(X-2, Y+1))
		Right();
	if (AIR(X+1, Y) && AIR(X+2, Y) && AIR(X+2, Y+1) && FREEZE(X+2, Y+1))
		Left();
		
	// while falling
	if (FREEZE(X, Y+GetVel().y))
	{
		if(SOLID(X-GetVel().y, Y+GetVel().y))
			Right();
		if(SOLID(X+GetVel().y, Y+GetVel().y))
			Left();
	}

	#undef TILE
	#undef FTILE
	#undef FREEZE
	#undef AIR
	#undef SOLID
}
