// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_PLAYERCOUNTER_H
#define GAME_SERVER_ENTITIES_PLAYERCOUNTER_H

#include <game/server/entity.h>
#include <game/server/entities/misc/lasertext.h>

class CPlayerCounter : public CEntity
{
	int m_Port;
	int m_PlayerCount;
	int64 m_LastUpdate;
	CLaserText *m_pLaserText;
	void Update(int PlayerCount);

public:
	CPlayerCounter(CGameWorld *pGameWorld, vec2 Pos, int Port);
	virtual ~CPlayerCounter();

	virtual void Tick();
	virtual void Snap(int SnappingClient);

	void OnUpdate(int Port, int PlayerCount);
};

#endif // GAME_SERVER_ENTITIES_PLAYERCOUNTER_H
