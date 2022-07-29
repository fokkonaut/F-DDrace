// made by fokkonaut

#ifndef GAME_SERVER_SNAKE_H
#define GAME_SERVER_SNAKE_H

#include <generated/protocol.h>
#include <game/server/entity.h>

class CCharacter;

class CSnake
{
	CGameContext *GameServer() const;
	IServer *Server() const;

	CCharacter *m_pCharacter;
	bool m_Active;
	int m_MoveLifespan;
	vec2 m_Dir;

	struct SSnakeInput
	{
		int m_Jump;
		int m_Hook;
		int m_Direction;
	};
	SSnakeInput m_Input;

	struct SSnakeData
	{
		CCharacter *m_pChr;
		vec2 m_Pos;
	};
	std::vector<SSnakeData> m_vSnake;

	bool HandleInput();
	bool IsInSnake(CCharacter *pChr);

	void InvalidateTees();
	void AddNewTees();
	void UpdateTees();

public:
	void Init(CCharacter *pChr);
	void Tick();

	bool Active();
	bool SetActive(bool Active);
	bool IsInAnySnake(CCharacter *pCheck);

	void OnPlayerDeath();
	void OnInput(CNetObj_PlayerInput *pNewInput);
};
#endif //GAME_SERVER_SNAKE_H
