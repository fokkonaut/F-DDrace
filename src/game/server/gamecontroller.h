/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTROLLER_H
#define GAME_SERVER_GAMECONTROLLER_H

#include <base/vmath.h>
#include <base/tl/array.h>

#include <generated/protocol.h>

/*
	Class: Game Controller
		Controls the main game logic. Keeping track of team and player score,
		winning conditions and specific game logic.
*/
class IGameController
{
	class CGameContext *m_pGameServer;
	class CConfig *m_pConfig;
	class IServer *m_pServer;

	// map
	char m_aMapWish[128];

	// spawn
	struct CSpawnEval
	{
		CSpawnEval()
		{
			m_Got = false;
			m_FriendlyTeam = -1;
			m_Pos = vec2(100,100);
		}

		vec2 m_Pos;
		bool m_Got;
		int m_FriendlyTeam;
		float m_Score;
	};
	
	float EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos) const;
	void EvaluateSpawnType(CSpawnEval* pEval, int MapIndex) const;

protected:

	// game
	int m_GameStartTick;
	int m_MatchCount;
	int m_RoundCount;
	int m_SuddenDeath;
	int m_aTeamscore[NUM_TEAMS];

	// info
	int m_GameFlags;
	const char *m_pGameType;
	struct CGameInfo
	{
		int m_MatchCurrent;
		int m_MatchNum;
		int m_ScoreLimit;
		int m_TimeLimit;
	} m_GameInfo;

	typedef void (*COMMAND_CALLBACK)(CGameContext *pGameServer, int ClientID, const char *pArgs);

	static void Com_CmdList(CGameContext* pGameServer, int ClientID, const char* pArgs);

	struct CChatCommand
	{
		char m_aName[32];
		char m_aHelpText[64];
		char m_aArgsFormat[16];
		COMMAND_CALLBACK m_pfnCallback;
		bool m_Used;
	};

	class CChatCommands
	{
		enum
		{
			// 8 is the number of vanilla commands, 14 the number of commands left to fill the chat.
			MAX_COMMANDS = 8 + 14
		};

		CChatCommand m_aCommands[MAX_COMMANDS];
	public:
		CChatCommands();

		// Format: i = int, s = string, p = playername, c = subcommand
		void AddCommand(const char *pName, const char *pArgsFormat, const char *pHelpText, COMMAND_CALLBACK pfnCallback);
		void RemoveCommand(const char *pName);
		void SendRemoveCommand(class IServer *pServer, const char *pName, int ID);
		CChatCommand *GetCommand(const char *pName);

		void OnPlayerConnect(class IServer *pServer, class CPlayer *pPlayer);

		void OnInit();
	};

	CChatCommands m_Commands;

public:
	CGameContext *GameServer() const { return m_pGameServer; }
	CConfig *Config() const { return m_pConfig; }
	IServer *Server() const { return m_pServer; }
	IGameController(class CGameContext *pGameServer);
	virtual ~IGameController() {};

	// event
	/*
		Function: on_CCharacter_death
			Called when a CCharacter in the world dies.

		Arguments:
			victim - The CCharacter that died.
			killer - The player that killed it.
			weapon - What weapon that killed it. Can be -1 for undefined
				weapon when switching team or player suicides.
	*/
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	/*
		Function: on_CCharacter_spawn
			Called when a CCharacter spawns into the game world.

		Arguments:
			chr - The CCharacter that was spawned.
	*/
	virtual void OnCharacterSpawn(class CCharacter *pChr);

	/*
		Function: on_entity
			Called when the map is loaded to process an entity
			in the map.

		Arguments:
			index - Entity index.
			pos - Where the entity is located in the world.

		Returns:
			bool?
	*/
	virtual bool OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number = 0);

	const char* GetTeamName(int Team);

	void OnPlayerCommand(class CPlayer *pPlayer, const char *pCommandName, const char *pCommandArgs);
	CChatCommands* CommandsManager() { return &m_Commands; };

	// game
	enum
	{
		TIMER_INFINITE = -1,
		TIMER_END = 10,
	};

	// general
	virtual void Snap(int SnappingClient);
	virtual void Tick();

	// info
	void UpdateGameInfo(int ClientID);
	const char *GetGameType() const { return m_pGameType; }

	// map
	void ChangeMap(const char *pToMap);

	//spawn
	bool CanSpawn(vec2* pPos, int Index) const;

	// team
	bool CanJoinTeam(int Team, int NotThisID) const;
	int ClampTeam(int Team) const;

	void ResetGame();
	void StartRound();

	// F-DDrace

	float m_CurrentRecord;
};

#endif
