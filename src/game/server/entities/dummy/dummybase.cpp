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
int CDummyBase::GetTargetX() { return m_pCharacter->Input()->m_TargetX; }
int CDummyBase::GetTargetY() { return m_pCharacter->Input()->m_TargetY; }
int CDummyBase::GetDirection() { return m_pCharacter->Input()->m_Direction; }

void CDummyBase::SetWeapon(int Weapon) { m_pCharacter->SetWeapon(Weapon); }
void CDummyBase::Die() { m_pCharacter->Die(); }
void CDummyBase::Left() { m_pCharacter->Input()->m_Direction = DIRECTION_LEFT; }
void CDummyBase::Right() { m_pCharacter->Input()->m_Direction = DIRECTION_RIGHT; }
void CDummyBase::StopMoving() { m_pCharacter->Input()->m_Direction = DIRECTION_NONE; }
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
	m_DebugColor = -1;
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

bool CDummyBase::IsFreezeTile(int _X, int _Y)
{
	return GameServer()->Collision()->GetTileRaw(_X, _Y) == TILE_FREEZE ||
		GameServer()->Collision()->GetFTileRaw(_X, _Y) == TILE_FREEZE ||
		GameServer()->Collision()->GetTileRaw(_X, _Y) == TILE_DFREEZE ||
		GameServer()->Collision()->GetFTileRaw(_X, _Y) == TILE_DFREEZE;
}

void CDummyBase::AvoidTile(int Tile)
{
	#define IS_TILE(x, y) (GameServer()->Collision()->GetTileRaw(RAW(x), RAW(y)) == Tile || GameServer()->Collision()->GetFTileRaw(RAW(x), RAW(y)) == Tile)
	#define AIR(x, y) !IS_TILE(x, y)
	#define SOLID(x, y) GameServer()->Collision()->IsSolid(RAW(x), RAW(y))

	// sides
	if (IS_TILE(X+1, Y))
		Left();
	if (IS_TILE(X-1, Y))
		Right();

	// corners
	if (AIR(X-1, Y) && IS_TILE(X+1, Y-1))
		Left();
	if (AIR(X+1, Y) && IS_TILE(X-1, Y-1))
		Right();

	// small edges
	if (AIR(X-1, Y) && IS_TILE(X-1, Y+1))
		Right();

	if (AIR(X+1, Y) && IS_TILE(X+1, Y+1))
		Left();
		
	// big edges
	if (AIR(X-1, Y) && AIR(X-2, Y) && AIR(X-2, Y+1) && IS_TILE(X-2, Y+1))
		Right();
	if (AIR(X+1, Y) && AIR(X+2, Y) && AIR(X+2, Y+1) && IS_TILE(X+2, Y+1))
		Left();
		
	// while falling
	if (IS_TILE(X, Y+GetVel().y))
	{
		if(SOLID(X-GetVel().y, Y+GetVel().y))
			Right();
		if(SOLID(X+GetVel().y, Y+GetVel().y))
			Left();
	}

	#undef IS_TILE
	#undef AIR
	#undef SOLID
}

void CDummyBase::AvoidFreeze()
{
	AvoidTile(TILE_FREEZE);
	AvoidTile(TILE_DFREEZE);
}

void CDummyBase::AvoidDeath()
{
	AvoidTile(TILE_DEATH);
	if (X+5 >= GameServer()->Collision()->GetWidth() + 200)
		Left();
	else if (X-5 <= -200)
		Right();
}

void CDummyBase::AvoidFreezeWeapons()
{
	SetWeapon(WEAPON_GRENADE);
	// avoid hitting freeze roof
	if (GetVel().y < -0.05)
	{
		int distY = RAW_Y + GetVel().y * 4 - 40;
		if (!GameServer()->Collision()->IntersectLine(GetPos(), vec2(GetPos().x, distY), 0, 0) && 
			IsFreezeTile(RAW_X, distY))
		{
			Aim(GetVel().x, -200);
			Fire();
		}
	}
	// avoid hitting freeze floor
	else if (GetVel().y > 0.05)
	{
		int distY = RAW_Y + GetVel().y * 3 + 20;
		if (!GameServer()->Collision()->IntersectLine(GetPos(), vec2(GetPos().x, distY), 0 , 0) && 
			IsFreezeTile(RAW_X, distY))
		{
			Aim(GetVel().x, 200);
			Fire();
			Jump();
		}
	}
	// jump over freeze when flying into it from the right side
	if (GetVel().x < -2.2f && (IsGrounded() || GetVel().y > 2.2f))
	{
		if (IsFreezeTile(RAW_X - 32, RAW_Y) ||
			(GetVel().y > 2.2f && IsFreezeTile(RAW_X - 32, RAW_Y + 30)))
			Jump();
	}
	// jump over freeze when flying into it from the left side
	if (GetVel().x > 2.2f && (IsGrounded() || GetVel().y > 2.2f))
	{
		if (IsFreezeTile(RAW_X + 32, RAW_Y) ||
			(GetVel().y < -2.2f && IsFreezeTile(RAW_X + 32, RAW_Y + 30)))
			Jump();
	}
}

void CDummyBase::RightAntiStuck()
{
	Right();
	if (GameServer()->Collision()->IntersectLine(GetPos(), vec2(RAW_X + 60, RAW_Y), 0, 0))
	{
		Jump(random(5));
		if(GameServer()->Collision()->IntersectLine(GetPos(), vec2(RAW_X + 10, RAW_Y + 60), 0, 0))
			Left();
	}
}

void CDummyBase::LeftAntiStuck()
{
	Left();
	if (GameServer()->Collision()->IntersectLine(GetPos(), vec2(RAW_X - 60, RAW_Y), 0, 0))
	{
		Jump(random(5));
		if(GameServer()->Collision()->IntersectLine(GetPos(), vec2(RAW_X - 10, RAW_Y - 60), 0, 0))
			Right();
	}
}

void CDummyBase::RightThroughFreeze()
{
	if(m_pCharacter->m_FreezeTime)
	{
		m_RtfGetSpeed = 0;
		return;
	}
	Right();
	if (m_RtfGetSpeed)
	{
		Left();
		if (GameServer()->Collision()->GetTileRaw(RAW_X + m_RtfGetSpeed, RAW_Y) == TILE_FREEZE ||
			GameServer()->Collision()->GetTileRaw(RAW_X + m_RtfGetSpeed, RAW_Y + 16) == TILE_FREEZE)
			return;
		m_RtfGetSpeed = 0;
	}
	// jump through freeze if one is close or go back if no vel
	for (int i = 5; i < 160; i+=5)
	{
		int TileRight = GameServer()->Collision()->GetTileRaw(RAW_X + i, RAW_Y);
		// ignore freeze behind collision
		if (TileRight == TILE_SOLID || TileRight == TILE_NOHOOK)
			break;
		else if (TileRight == TILE_FREEZE ||
			GameServer()->Collision()->GetTileRaw(RAW_X + i, RAW_Y + 16) == TILE_FREEZE)
		{
			if (GetVel().y > 1.1f)
			{
				if (GameServer()->Collision()->GetTileRaw(RAW_X - 32, RAW_Y) != TILE_FREEZE &&
					GameServer()->Collision()->GetTileRaw(RAW_X - 32, RAW_Y + 16) != TILE_FREEZE)
					Left();
			}
			if (IsGrounded() && GetVel().x > 8.8f)
				Jump(TicksPassed(2));
			if (i < 22 && GetVel().x < 5.5f)
			{
				int k;
				for (k = 5; k < 160; k+=5)
				{
					if (GameServer()->Collision()->GetTileRaw(RAW_X - k, RAW_Y) == TILE_FREEZE ||
						GameServer()->Collision()->GetTileRaw(RAW_X - k, RAW_Y + 16) == TILE_FREEZE)
					{
						break;
					}
				}
				m_RtfGetSpeed = k < 80 ? 20 : 40;
				Left();
			}
			break;
		}
	}
}

void CDummyBase::LeftThroughFreeze()
{
	if(m_pCharacter->m_FreezeTime)
	{
		m_LtfGetSpeed = 0;
		return;
	}
	Left();
	if (m_LtfGetSpeed)
	{
		Right();
		if (GameServer()->Collision()->GetTileRaw(RAW_X - m_LtfGetSpeed, RAW_Y) == TILE_FREEZE ||
			GameServer()->Collision()->GetTileRaw(RAW_X - m_LtfGetSpeed, RAW_Y + 16) == TILE_FREEZE)
			return;
		m_LtfGetSpeed = 0;
	}
	// jump through freeze if one is close or go back if no vel
	for (int i = 5; i < 160; i+=5)
	{
		int TileLeft = GameServer()->Collision()->GetTileRaw(RAW_X - i, RAW_Y);
		// ignore freeze behind collision
		if (TileLeft == TILE_SOLID || TileLeft == TILE_NOHOOK)
			break;
		else if (TileLeft == TILE_FREEZE ||
			GameServer()->Collision()->GetTileRaw(RAW_X - i, RAW_Y + 16) == TILE_FREEZE)
		{
			if (GetVel().y > 1.1f)
			{
				if (GameServer()->Collision()->GetTileRaw(RAW_X + 32, RAW_Y) != TILE_FREEZE &&
					GameServer()->Collision()->GetTileRaw(RAW_X + 32, RAW_Y + 16) != TILE_FREEZE)
					Right();
			}
			if (IsGrounded() && GetVel().x < -8.8f)
				Jump(TicksPassed(2));
			if (i < 22 && GetVel().x > -5.5f)
			{
				int k;
				for (k = 5; k < 160; k+=5)
				{
					if (GameServer()->Collision()->GetTileRaw(RAW_X + k, RAW_Y) == TILE_FREEZE ||
						GameServer()->Collision()->GetTileRaw(RAW_X + k, RAW_Y + 16) == TILE_FREEZE)
					{
						break;
					}
				}
				m_LtfGetSpeed = k < 80 ? 20 : 40;
				Right();
			}
			break;
		}
	}
}

void CDummyBase::DebugColor(int DebugColor)
{
	if (DebugColor == m_DebugColor)
		return;
	m_DebugColor = DebugColor;

	if (DebugColor == -1)
	{
		m_pPlayer->ResetSkin();
		return;
	}

	CTeeInfo Info = m_pPlayer->m_CurrentInfo.m_TeeInfos;
	for (int p = 0; p < NUM_SKINPARTS; p++)
	{
		int BaseColor = (DebugColor*30) * 0x010000;
		int Color = 0xff32;
		if (DebugColor == COLOR_BLACK)
			Color = BaseColor = 0;
		else if (DebugColor == COLOR_WHITE)
			Color = BaseColor = 255;
		if (p == SKINPART_MARKING)
			Color *= -256;

		Info.m_aUseCustomColors[p] = 1;
		Info.m_aSkinPartColors[p] = BaseColor + Color;
		Info.m_Sevendown.m_UseCustomColor = 1;
		Info.m_Sevendown.m_ColorBody = Info.m_Sevendown.m_ColorFeet = BaseColor + Color;
	}

	GameServer()->SendSkinChange(Info, m_pPlayer->GetCID(), -1);
}
