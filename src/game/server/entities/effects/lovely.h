// original version has been created by ReiTw

#ifndef GAME_SERVER_ENTITIES_LOVELY_H
#define GAME_SERVER_ENTITIES_LOVELY_H

#include <game/server/entity.h>

class CLovely : public CEntity
{
	enum
	{
		MAX_HEARTS = 4
	};

	Mask128 m_TeamMask;
	int m_Owner;
	float m_SpawnDelay;

	struct SLovelyData
	{
		int m_ID;
		vec2 m_Pos;
		float m_Lifespan;
	};
	SLovelyData m_aLovelyData[MAX_HEARTS];
	void SpawnNewHeart();

public:
	CLovely(CGameWorld *pGameWorld, vec2 Pos, int Owner);

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
};

#endif
