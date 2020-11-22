// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_MONEY_H
#define GAME_SERVER_ENTITIES_MONEY_H

#include "advanced_entity.h"

enum
{
	NUM_DOTS = 12,
	NUM_MONEY_IDS = NUM_DOTS + 1, // one extra for the merge laser

	MONEY_DROP_RADIUS = 10,
	MERGE_RADIUS = 32*24,
};

class CMoney : public CAdvancedEntity
{
private:
	int64 m_Amount;
	int m_aID[NUM_MONEY_IDS];

	int64 m_MergeTick;
	vec2 m_MergePos;

public:
	CMoney(CGameWorld *pGameWorld, vec2 Pos, int64 Amount, float Direction = 0);
	virtual ~CMoney();

	int GetAmount() { return m_Amount; }
	virtual void Tick();
	virtual void Snap(int SnappingClient);
};

#endif // GAME_SERVER_ENTITIES_MONEY_H
