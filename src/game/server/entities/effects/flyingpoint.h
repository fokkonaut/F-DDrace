#ifndef GAME_SERVER_ENTITIES_FLYINGPOINT_H
#define GAME_SERVER_ENTITIES_FLYINGPOINT_H

#include <game/server/entity.h>

class CFlyingPoint : public CEntity
{
private:
	vec2 m_InitialVel;
	float m_InitialAmount;
	int m_Owner;
	Mask128 m_TeamMask;

	bool m_PortalBlocker;
	vec2 m_PrevPos;

	// Either a to clientid is set or a to position
	int m_To;
	vec2 m_ToPos;
	
public:
	CFlyingPoint(CGameWorld *pGameWorld, vec2 Pos, int To, int Owner, vec2 InitialVel, vec2 ToPos = vec2(-1, -1), bool PortalBlocker = false);
	
	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
};

#endif