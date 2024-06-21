// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_PORTAL_H
#define GAME_SERVER_ENTITIES_PORTAL_H

#include <game/server/entity.h>
#include <vector>

class CPortal : public CEntity
{
	enum
	{
		NUM_SIDE = 12,
		NUM_PARTICLES = 12,
		NUM_PORTAL_IDS = NUM_SIDE + NUM_PARTICLES,
	};

	int m_StartTick;
	int m_LinkedTick;
	CPortal *m_pLinkedPortal;

	int m_Owner;
	int m_ThroughPlotDoor; // for flags
	bool m_InNoBonusArea;
	int m_aID[NUM_PORTAL_IDS];

	std::vector<CEntity*> m_vTeleported;
	void EntitiesEnter();

public:
	CPortal(CGameWorld *pGameWorld, vec2 Pos, int Owner, int ThroughPlotDoor = 0, bool InNoBonusArea = false);
	virtual ~CPortal();

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);

	void SetThroughPlotDoor(int PlotID) { m_ThroughPlotDoor = PlotID; }
	int GetThroughPlotDoor() { return m_ThroughPlotDoor; }
	void SetNoBonusArea() { m_InNoBonusArea = true; }

	void SetLinkedPortal(CPortal *pPortal);
	void DestroyLinkedPortal();
};

#endif // GAME_SERVER_ENTITIES_PORTAL_H
