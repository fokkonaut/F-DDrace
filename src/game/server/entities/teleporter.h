// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_TELEPORTER_H
#define GAME_SERVER_ENTITIES_TELEPORTER_H

#include <game/server/entity.h>

class CTeleporter : public CEntity
{
	enum
	{
		TELE_RADIUS = 16,

		NUM_CIRCLE = 5,
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
	void ResetCollision(bool Remove = false);

public:
	CTeleporter(CGameWorld *pGameWorld, vec2 Pos, int Type, int Number, bool Collision = true);
	virtual ~CTeleporter();
	virtual void Snap(int SnappingClient);
	int GetType() { return m_Type; }
};

#endif // GAME_SERVER_ENTITIES_TELEPORTER_H
