// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_MAP_DRAWEDITOR_TELEPORTER_H
#define GAME_SERVER_ENTITIES_MAP_DRAWEDITOR_TELEPORTER_H

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

	int m_StartTick;
	int m_aID[NUM_TELEPORTER_IDS];
	int m_Type;

public:
	CTeleporter(CGameWorld *pGameWorld, vec2 Pos, int Type, int Number, bool Collision = true);
	virtual ~CTeleporter();
	void ResetCollision(bool Remove = false) override;
	void Snap(int SnappingClient) override;
	void Tick() override;
	int GetType() { return m_Type; }
};

#endif // GAME_SERVER_ENTITIES_MAP_DRAWEDITOR_TELEPORTER_H
