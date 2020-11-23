// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_MONEY_H
#define GAME_SERVER_ENTITIES_MONEY_H

#include "advanced_entity.h"

enum
{
	NUM_DOTS_SMALL = 6,
	NUM_DOTS_BIG = 12,
	NUM_MONEY_IDS = NUM_DOTS_BIG + 1, // one extra for the merge laser

	SMALL_MONEY_AMOUNT = 1000,
	MONEY_RADIUS_BIG = 14,
	MONEY_RADIUS_SMALL = 10,

	RADIUS_FIND_MONEY = 32*24,
	RADIUS_FIND_PLAYERS = 32*12,
	MAX_SPEED = 2,
};

class CMoney : public CAdvancedEntity
{
private:
	int64 m_Amount;
	int64 m_StartTick;
	bool TimePassed(float Seconds);
	int m_aID[NUM_MONEY_IDS];

	void MoveTo(vec2 Pos, int Radius);
	int GetRadius() { return GetRadius(m_Amount); }
	int GetRadius(int64 Amount) { return (Amount < SMALL_MONEY_AMOUNT) ? MONEY_RADIUS_SMALL : MONEY_RADIUS_BIG; } // To be called in the constructor
	int GetNumDots() { return (m_Amount < SMALL_MONEY_AMOUNT) ? NUM_DOTS_SMALL : NUM_DOTS_BIG; }

public:
	CMoney(CGameWorld *pGameWorld, vec2 Pos, int64 Amount, int Owner = -1, float Direction = 0);
	virtual ~CMoney();

	int GetAmount() { return m_Amount; }
	virtual void Tick();
	virtual void Snap(int SnappingClient);
};

#endif // GAME_SERVER_ENTITIES_MONEY_H
