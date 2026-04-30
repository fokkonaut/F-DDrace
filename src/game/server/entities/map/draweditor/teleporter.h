// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_TELEPORTER_H
#define GAME_SERVER_ENTITIES_TELEPORTER_H

#include <game/server/entity.h>

class CTeleporter : public CEntity
{
	enum
	{
		TELE_RADIUS = 16,

		NUM_CIRCLE = 5, // has to be at least 2 for the light speedup to work, sine they share the same id pool
		NUM_PARTICLES = 1,
		NUM_TELEPORTER_IDS = NUM_CIRCLE + NUM_PARTICLES,
	};

	struct
	{
		vec2 m_Pos;
		float m_Time;
		float m_LastTime;
	} m_Snap;

	int m_aID[NUM_TELEPORTER_IDS];
	int m_Type;

public:
	CTeleporter(CGameWorld *pGameWorld, vec2 Pos, int Type, int Number, bool Collision = true);
	virtual ~CTeleporter();
	virtual void ResetCollision(bool Remove = false);
	virtual void Snap(int SnappingClient);
	virtual void Tick();
	int GetType() { return m_Type; }
};

#endif // GAME_SERVER_ENTITIES_TELEPORTER_H
