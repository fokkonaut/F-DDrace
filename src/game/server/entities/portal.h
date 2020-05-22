// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_PORTAL_H
#define GAME_SERVER_ENTITIES_PORTAL_H

#include <game/server/entity.h>

enum
{
	NUM_SIDE = 12,
	NUM_PARTICLES = 12,
	NUM_IDS = NUM_SIDE + NUM_PARTICLES,
};

class CPortal : public CEntity
{
	int m_StartTick;
	int m_LinkedTick;
	CPortal *m_pLinkedPortal;

	int m_Owner;
	bool m_aTeleported[MAX_CLIENTS];
	int m_aID[NUM_IDS];

	void PlayerEnter();

public:
	CPortal(CGameWorld *pGameWorld, vec2 Pos, int Owner);
	virtual ~CPortal();

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);

	void SetLinkedPortal(CPortal *pPortal);
};

#endif // GAME_SERVER_ENTITIES_PORTAL_H
