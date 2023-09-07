// This file is included in engine/shared/protocol.h, so basically every other file should be able to get these enum entries.
// These are weapons, created by fokkonaut.
// NUM_VANILLA_WEAPONS is the old NUM_WEAPONS
// Add another tune for the fire delay when adding a new weapon!
// And also update game/server/save.cpp to save and load the new weapon correctly, aswell as hadweapon and spreadweapon.

#ifndef ENGINE_SHARED_WEAPONS_EX_H
#define ENGINE_SHARED_WEAPONS_EX_H

#include <generated/protocol.h>

enum
{
	// try to keep order from laser to ninja, so the ddrace hud kinda goes backwards after vanilla weapons and also start with important taser and portal
	WEAPON_TASER = NUM_VANILLA_WEAPONS,
	WEAPON_PORTAL_RIFLE,
	WEAPON_PLASMA_RIFLE,
	WEAPON_PROJECTILE_RIFLE,
	WEAPON_TELE_RIFLE,
	WEAPON_LIGHTNING_LASER,
	WEAPON_STRAIGHT_GRENADE,
	WEAPON_BALL_GRENADE,
	WEAPON_HEART_GUN,
	WEAPON_LIGHTSABER,
	WEAPON_TELEKINESIS,
	WEAPON_DRAW_EDITOR,
	NUM_WEAPONS
};

#endif // ENGINE_SHARED_WEAPONS_EX_H
