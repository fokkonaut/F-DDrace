#ifndef GAME_SERVER_ENTITIES_STABLE_PROJECTILE_H
#define GAME_SERVER_ENTITIES_STABLE_PROJECTILE_H

#include <game/server/entity.h>

class CStableProjectile : public CEntity
{
	int m_Type;
	vec2 m_LastResetPos;
	int m_LastResetTick;
	bool m_CalculatedVel;
	int m_VelX;
	int m_VelY;

	int m_Owner;
	bool m_HideOnSpec;
	bool m_OnlyShowOwner;

	void CalculateVel();

public:
	CStableProjectile(CGameWorld *pGameWorld, int Type, int Owner, vec2 Pos=vec2(), bool HideOnSpec = false, bool OnlyShowOwner = false);

	virtual void Reset();
	virtual void TickDefered();
	virtual void Snap(int SnappingClient);

	void SetPos(vec2 Pos) { m_Pos = Pos; };
};

#endif
