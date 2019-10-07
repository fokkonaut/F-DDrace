// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_PICKUP_DROP_H
#define GAME_SERVER_ENTITIES_PICKUP_DROP_H

#include <game/server/entity.h>

class CPickupDrop : public CEntity
{
public:
	CPickupDrop(CGameWorld *pGameWorld, vec2 Pos, int Type, int Owner, float Direction, int Weapon = WEAPON_GUN,
		int Lifetime = 300, int Bullets = -1, bool SpreadWeapon = false, bool Jetpack = false, bool TeleWeapon = false, bool DoorHammer = false);
	virtual ~CPickupDrop();

	void Reset(bool Erase, bool Picked);
	virtual void Reset() { Reset(true, false); };
	virtual void Tick();
	virtual void Snap(int SnappingClient);

	static int const ms_PhysSize = 14;

private:
	int IsCharacterNear();
	void IsShieldNear();
	void Pickup();
	bool IsGrounded(bool SetVel = false);
	void HandleDropped();

	vec2 m_Vel;

	int64_t m_TeamMask;
	CCharacter* m_pOwner;
	int m_Owner;

	int m_Type;
	int m_Weapon;
	int m_Lifetime;
	int m_Bullets;
	int m_PickupDelay;
	bool m_SpreadWeapon;
	bool m_Jetpack;
	bool m_TeleWeapon;
	bool m_DoorHammer;
	vec2 m_PrevPos;
	vec2 m_SnapPos;

	// have to define a new ID variable for the bullet
	int m_aID[4];

	int m_TeleCheckpoint;
	int m_TuneZone;

	static bool IsSwitchActiveCb(int Number, void* pUser);
	void HandleTiles(int Index);
	int m_TileIndex;
	int m_TileFIndex;
	int m_MoveRestrictions;
};

#endif
