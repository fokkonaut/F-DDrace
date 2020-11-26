// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_PICKUP_DROP_H
#define GAME_SERVER_ENTITIES_PICKUP_DROP_H

#include "advanced_entity.h"
#include <game/server/mask128.h>

class CPickupDrop : public CAdvancedEntity
{
public:
	CPickupDrop(CGameWorld *pGameWorld, vec2 Pos, int Type, int Owner, float Direction, int Lifetime = 300, int Weapon = WEAPON_GUN, int Bullets = -1, int Special = 0);
	virtual ~CPickupDrop();

	void Reset(bool Picked);
	virtual void Reset() { Reset(false); }
	virtual void Tick();
	virtual void Snap(int SnappingClient);

private:
	static int const ms_PhysSize = 14;

	int IsCharacterNear();
	void IsShieldNear();
	void Pickup();

	Mask128 m_TeamMask;
	bool m_DDraceMode;
	int m_Type;
	int m_Weapon;
	int m_Lifetime;
	int m_Bullets;
	int m_PickupDelay;
	int m_Special;
	vec2 m_SnapPos;

	// have to define a new ID variable for the bullet
	int m_aID[4];
};

#endif
