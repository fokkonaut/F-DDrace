// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_MONEY_H
#define GAME_SERVER_ENTITIES_MONEY_H

#include <game/server/entity.h>

enum
{
	NUM_DOTS = 12,
	MONEY_DROP_RADIUS = 10,
};

class CMoney : public CEntity
{
private:
	vec2 m_Vel;
	int m_TuneZone;

	int m_Amount;
	int m_aID[NUM_DOTS];

public:
	CMoney(CGameWorld *pGameWorld, vec2 Pos, int Amount);
	virtual ~CMoney();

	int GetAmount() { return m_Amount; }
	virtual void Tick();
	virtual void Snap(int SnappingClient);
};

#endif // GAME_SERVER_ENTITIES_MONEY_H
