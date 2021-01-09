// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_PORTAL_H
#define GAME_SERVER_ENTITIES_PORTAL_H

#include <game/server/entity.h>
#include <vector>

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
	int m_aID[NUM_IDS];

	std::vector<CEntity*> m_vTeleported;
	void EntitiesEnter();

public:
	CPortal(CGameWorld *pGameWorld, vec2 Pos, int Owner);
	virtual ~CPortal();

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);

	void SetLinkedPortal(CPortal *pPortal);
	void DestroyLinkedPortal();
};

#endif // GAME_SERVER_ENTITIES_PORTAL_H
