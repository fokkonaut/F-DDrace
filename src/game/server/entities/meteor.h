#ifndef GAME_SERVER_ENTITIES_METEOR_H
#define GAME_SERVER_ENTITIES_METEOR_H

#include <game/server/entities/stable_projectile.h>

class CMeteor : public CStableProjectile
{
	vec2 m_Vel;
	int m_Owner;
	bool m_Infinite;
	int m_TuneZone;

public:
	CMeteor(CGameWorld *pGameWorld, vec2 Pos, int Owner, bool Infinite);
	
	virtual void Reset();
	virtual void Tick();
};

#endif
