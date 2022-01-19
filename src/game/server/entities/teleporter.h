// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_TELEPORTER_H
#define GAME_SERVER_ENTITIES_TELEPORTER_H

#include <game/server/entity.h>

class CTeleporter : public CEntity
{
	int m_Type;
	bool m_Collision;
	void ResetCollision(bool Remove = false);

public:
	CTeleporter(CGameWorld *pGameWorld, vec2 Pos, int Type, int Number, bool Collision = true);
	virtual ~CTeleporter();
	virtual void Snap(int SnappingClient);
	int GetType() { return m_Type; }
};

#endif // GAME_SERVER_ENTITIES_TELEPORTER_H
