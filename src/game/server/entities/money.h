// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_MONEY_H
#define GAME_SERVER_ENTITIES_MONEY_H

#include <game/server/entity.h>

class CMoney : public CEntity
{
public:
	CMoney(CGameWorld *pGameWorld, vec2 Pos);
	virtual ~CMoney();

	virtual void Tick();
	virtual void Snap(int SnappingClient);
};

#endif // GAME_SERVER_ENTITIES_MONEY_H
