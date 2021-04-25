// made by fokkonaut

#ifndef GAME_SERVER_MINIGAMES_MINIGAME_H
#define GAME_SERVER_MINIGAMES_MINIGAME_H

#include <engine/shared/protocol.h>

class CGameContext;
class IServer;

enum Minigames
{
	MINIGAME_NONE = -1,
	MINIGAME_BLOCK,
	MINIGAME_SURVIVAL,
	MINIGAME_1VS1,
	MINIGAME_INSTAGIB_BOOMFNG,
	MINIGAME_INSTAGIB_FNG,
	NUM_MINIGAMES
};

class CMinigame
{
protected:
	CGameContext *GameServer() const { return m_pGameServer; }
	CGameContext *m_pGameServer;
	IServer *Server() const;

	int m_Type;

public:
	CMinigame(CGameContext *pGameServer, int Type);
	virtual ~CMinigame() {}

	virtual void Tick() {}
	virtual void Snap(int SnappingClient) {}

	int GetType() { return m_Type; }
	bool IsType(int Type) { return m_Type == Type; }
};
#endif // GAME_SERVER_MINIGAMES_MINIGAME_H
