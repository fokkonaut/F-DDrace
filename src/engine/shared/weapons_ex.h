// This file is included in engine/shared/protocol.h, so basically every other file should be able to get these enum entries.
// These are weapons, created by fokkonaut.
// NUM_VANILLA_WEAPONS is the old NUM_WEAPONS
// Add another tune for the fire delay when adding a new weapon!

#ifndef ENGINE_SHARED_WEAPONS_EX_H
#define ENGINE_SHARED_WEAPONS_EX_H

#include <generated/protocol.h>

enum
{
	WEAPON_TASER = NUM_VANILLA_WEAPONS,
	WEAPON_PORTAL_RIFLE,
	WEAPON_HEART_GUN,
	WEAPON_PLASMA_RIFLE,
	WEAPON_STRAIGHT_GRENADE,
	WEAPON_TELEKINESIS,
	WEAPON_LIGHTSABER,
	WEAPON_PROJECTILE_RIFLE,
	WEAPON_BALL_GRENADE,
	WEAPON_DRAW_EDITOR,
	NUM_WEAPONS
};

#endif // ENGINE_SHARED_WEAPONS_EX_H
