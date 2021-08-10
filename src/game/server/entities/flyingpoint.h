#ifndef GAME_SERVER_ENTITIES_FLYINGPOINT_H
#define GAME_SERVER_ENTITIES_FLYINGPOINT_H

#include <game/server/entity.h>

class CFlyingPoint : public CEntity
{
private:
	vec2 m_InitialVel;
	float m_InitialAmount;
	int m_To;
	int m_Owner;
	
public:
	CFlyingPoint(CGameWorld *pGameWorld, vec2 Pos, int To, int Owner, vec2 InitialVel);
	
	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
};

#endif