// made by fokkonaut and ChillerDragon

#ifndef GAME_SERVER_ENTITIES_DUMMYBASE_H
#define GAME_SERVER_ENTITIES_DUMMYBASE_H

#include <game/server/entity.h>
#include <engine/shared/config.h>

class CCharacter;
class CPlayer;

enum Dummymode
{
	DUMMYMODE_IDLE = 0,
	DUMMYMODE_V3_BLOCKER = -6,
	DUMMYMODE_CHILLBLOCK5_RACER = 23,
	DUMMYMODE_CHILLBLOCK5_BLOCKER = 29,
	DUMMYMODE_CHILLBOCK5_POLICE = 31,
	DUMMYMODE_BLMAPCHILL_POLICE = 32,
	DUMMYMODE_SHOP_DUMMY = 99,
	DUMMYMODE_PLOT_SHOP_DUMMY = 98,
	DUMMYMODE_BANK_DUMMY = 97,
};

class CDummyBase
{
public:
	CDummyBase(CCharacter *pChr, int Mode);
	virtual ~CDummyBase() {}
	void Tick();
	int Mode() { return m_Mode; }

private:
	virtual void OnTick() {}
	int m_Mode;

protected:
	CGameContext *GameServer() const;
	CGameWorld *GameWorld() const;
	IServer *Server() const;

	CCharacter *m_pCharacter;
	CPlayer *m_pPlayer;

	bool TicksPassed(int Ticks);

	void Left();
	void Right();
	void StopMoving();
	void Hook(bool Stroke = true);
	void Jump(bool Stroke = true);
	void Fire(bool Stroke = true);
	void Aim(int TargetX, int TargetY);
	void AimPos(vec2 Pos);
	void AimX(int TargetX);
	void AimY(int TargetY);

	vec2 GetPos();
	vec2 GetVel();

	int HookState();
	int Jumped();
	int Jumps();
	bool IsGrounded();

	void SetWeapon(int Weapon);
	void Die();
	void AvoidTile(int Tile);
	void AvoidFreeze();
	void AvoidDeath();
};

#endif
