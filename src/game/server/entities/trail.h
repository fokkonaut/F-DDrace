#ifndef GAME_SERVER_ENTITIES_TRAIL_H
#define GAME_SERVER_ENTITIES_TRAIL_H

#include <game/server/entity.h>
#include "stable_projectile.h"
#include <deque>

#define NUM_TRAILS 20
#define TRAIL_DIST 20

class CTrail : public CEntity
{
	std::vector<CStableProjectile *> m_TrailProjs;
	struct HistoryPoint
	{
		vec2 m_Pos;
		float m_Dist;

		HistoryPoint(vec2 Pos, float Dist) : m_Pos(Pos), m_Dist(Dist) {}
	};
	std::deque<HistoryPoint> m_TrailHistory;
	float m_TrailHistoryLength;

	int m_Owner;

public:
	CTrail(CGameWorld *pGameWorld, vec2 Pos, int Owner);
	
	void Clear();
	virtual void Reset();
	virtual void Tick();
};

#endif
