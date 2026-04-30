// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_TASER_SHIELD_H
#define GAME_SERVER_ENTITIES_TASER_SHIELD_H

#include <game/server/entity.h>

class CTaserShield : public CEntity
{
	enum
	{
		MAX_SHIELDS = 3
	};

	Mask128 m_TeamMask;
	int m_Owner;
	float m_SpawnDelay;

	struct SShieldData
	{
		int m_ID;
		vec2 m_Pos;
		float m_Lifespan;
		bool m_Used;
	};
	SShieldData m_aShieldData[MAX_SHIELDS];
	void SpawnNewShield();

public:
	CTaserShield(CGameWorld *pGameWorld, vec2 Pos, int Owner);

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
};

#endif // GAME_SERVER_ENTITIES_TASER_SHIELD_H
