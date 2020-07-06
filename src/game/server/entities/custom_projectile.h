// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_CUSTOM_PROJECTILE_H
#define GAME_SERVER_ENTITIES_CUSTOM_PROJECTILE_H

#include <game/server/entity.h>

enum
{
	NOT_COLLIDED = 0,
	COLLIDED_ONCE,
	COLLIDED_TWICE
};

class CCustomProjectile : public CEntity
{
public:

	CCustomProjectile(CGameWorld *pGameWorld, int Owner, vec2 Pos, vec2 Dir, bool Freeze,
		bool Explosive, bool Unfreeze, bool Bloody, bool Ghost, bool Spooky, int Type, float Lifetime = 6.0f, float Accel = 1.0f, float Speed = 10.0f);

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);

private:
	vec2 m_Core;
	vec2 m_PrevPos;
	vec2 m_Direction;

	int m_EvalTick;
	int m_LifeTime;

	Mask128 m_TeamMask;
	CCharacter* m_pOwner;
	int m_Owner;

	int m_Freeze;
	int m_Unfreeze;
	int m_Bloody;
	int m_Ghost;
	int m_Spooky;
	bool m_Explosive;
	int m_Type;

	float m_Accel;

	int m_CollisionState;

	void HitCharacter();
	void Move();

};

#endif
