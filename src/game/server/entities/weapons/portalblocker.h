// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_PORTAL_BLOCKER_H
#define GAME_SERVER_ENTITIES_PORTAL_BLOCKER_H

#include <game/server/entity.h>

class CPortalBlocker : public CEntity
{
	Mask128 m_TeamMask;
	int m_Owner;
	int m_Lifetime;
	int m_aID[2];

	vec2 m_StartPos;
	bool m_HasStartPos;
	bool m_HasEndPos;

	bool CanPlace();

public:
	CPortalBlocker(CGameWorld *pGameWorld, vec2 Pos, int Owner);
	virtual ~CPortalBlocker();
	virtual void Tick();
	virtual void Snap(int SnappingClient);

	bool OnPlace();
	bool IsPlaced() { return m_HasEndPos; }
	vec2 GetStartPos() { return m_StartPos; }
};

#endif // GAME_SERVER_ENTITIES_PORTAL_BLOCKER_H
