// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_TREASURECHEST_H
#define GAME_SERVER_ENTITIES_TREASURECHEST_H

#include <game/server/entity.h>

class CTreasureChest : public CEntity
{
	int m_Owner;
	int m_StartTick;

public:
	CTreasureChest(CGameWorld *pGameWorld, vec2 Pos, int Owner);
	virtual ~CTreasureChest();

	virtual void Tick();
	virtual void Snap(int SnappingClient);
};

#endif // GAME_SERVER_ENTITIES_TREASURECHEST_H
