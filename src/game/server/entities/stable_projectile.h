// made by timakro originally

#ifndef GAME_SERVER_ENTITIES_STABLE_PROJECTILE_H
#define GAME_SERVER_ENTITIES_STABLE_PROJECTILE_H

#include <game/server/entity.h>

class CStableProjectile : public CEntity
{
	int m_Type;
	bool m_CalculatedVel;
	vec2 m_PrevPos;

	enum
	{
		SNAPINFO_LOWBANDWIDTH,
		SNAPINFO_HIGHBANDWIDTH,
		NUM_SNAPINFO
	};

	struct SSnapInfo
	{
		vec2 m_LastResetPos;
		int m_LastResetTick;
		ivec2 m_Vel;
	} m_aSnap[NUM_SNAPINFO];

	Mask128 m_TeamMask;
	int m_Owner;
	int m_Flags;

	void CalculateVel();

	void SnapProjectile(int SnappingClient, int InfoId);
	void SnapDDNetProjectile(int SnappingClient, int InfoId);

public:
	enum EFlags
	{
		HIDE_ON_SPEC = 1<<0,
		ONLY_SHOW_OWNER = 1<<1,
		DDNETPROJ_ANTIPING = 1<<2,
	};

	CStableProjectile(CGameWorld *pGameWorld, int Type, int Owner, vec2 Pos, int Flags = 0);

	virtual void Reset();
	virtual void TickDeferred();
	virtual void Snap(int SnappingClient);

	void SetPos(vec2 Pos) { m_Pos = Pos; };
};

#endif
