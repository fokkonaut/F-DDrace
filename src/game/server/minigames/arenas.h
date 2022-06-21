// made by fokkonaut

#ifndef GAME_SERVER_MINIGAMES_ARENAS_H
#define GAME_SERVER_MINIGAMES_ARENAS_H

#include "minigame.h"
#include <base/vmath.h>
#include <vector>

class CPlayer;
class CCharacter;

class CFight
{
public:
	bool m_Active;

	void Reset()
	{
		m_Active = false;
		m_ScoreLimit = 10;
		m_KillBorder = false;
		m_LongFreezeStart = false;

		m_Weapons.m_Hammer = true;
		m_Weapons.m_Shotgun = false;
		m_Weapons.m_Grenade = false;
		m_Weapons.m_Laser = false;

		for (int i = 0; i < 2; i++)
		{
			m_aParticipants[i].m_ClientID = -1;
			m_aParticipants[i].m_Score = 0;
			m_aParticipants[i].m_Status = -1;
		}
	}

	vec2 m_aCorners[4];
	vec2 m_aSpawns[2];
	vec2 m_MiddlePos;
	int m_ScoreLimit;
	bool m_KillBorder;
	bool m_LongFreezeStart;

	struct
	{
		bool m_Hammer;
		bool m_Shotgun;
		bool m_Grenade;
		bool m_Laser;
	} m_Weapons;

	struct
	{
		int m_ClientID;
		int m_Status;
		int m_Score;
	} m_aParticipants[2];
};

class CArenas : public CMinigame
{
	enum
	{
		MAX_FIGHTS = VANILLA_MAX_CLIENTS-1, // team 1-63

		POINT_TOP_LEFT = 0,
		POINT_TOP_RIGHT,
		POINT_BOTTOM_RIGHT,
		POINT_BOTTOM_LEFT,

		// swapping state order can cause bugs
		STATE_1VS1_NONE = -1,
		STATE_1VS1_PLACE_ARENA,
		STATE_1VS1_PLACE_FIRST_SPAWN,
		STATE_1VS1_PLACE_SECOND_SPAWN,
		STATE_1VS1_SELECT_WEAPONS,
		STATE_1VS1_DONE,

		PARTICIPANT_OWNER = 0,
		PARTICIPANT_WAITING,
		PARTICIPANT_INVITED,
		PARTICIPANT_ACCEPTED,
	};

	CFight m_GlobalArena;
	CFight m_aFights[MAX_FIGHTS];
	void SetArenaCollision(int Fight, bool Remove);
	void PlaceArena(int ClientID);
	void PlaceSpawn(int ClientID);
	void SelectWeapon(int ClientID);
	void FinishConfiguration(int Fight, int ClientID);
	int GetFreeArena();
	void KillParticipants(int Fight, int Killer = -1);
	void IncreaseScore(int Fight, int Index);
	vec2 GetShowDistance(int ClientID);
	bool IsGrounded(CCharacter *pChr);
	bool ValidSpawnPos(vec2 Pos);
	void StartFight(int Fight);

	int m_aState[MAX_CLIENTS];
	int m_aLastDirection[MAX_CLIENTS];
	int m_aLastJump[MAX_CLIENTS];
	int m_aSelectedWeapon[MAX_CLIENTS];
	int m_aInFight[MAX_CLIENTS];
	int64 m_aFirstGroundedFreezeTick[MAX_CLIENTS];

	void UpdateSnapPositions(int ClientID);
	struct
	{
		int m_aBorder[4];
		int m_aSpawn[2];
		int m_aWeaponBox[4];
		int m_SelectedWeapon;
		int m_WeaponActivated;
	} m_IDs;

public:
	enum
	{
		PARTICIPANT_GLOBAL = -2,
	};

	CArenas(CGameContext *pGameServer, int Type);
	virtual ~CArenas();

	virtual void Tick();
	virtual void Snap(int SnappingClient);

	void Reset(int ClientID);
	int GetClientFight(int ClientID, bool HasToBeJoined = true);
	int GetClientScore(int ClientID);
	int GetScoreLimit(int ClientID);
	vec2 GetSpawnPos(int ClientID);
	bool FightStarted(int ClientID) { return m_aInFight[ClientID]; }
	bool IsInArena(int Fight, vec2 Pos);
	bool IsKillBorder(int Fight) { return m_aFights[Fight].m_KillBorder; }

	bool IsConfiguring(int ClientID) { return m_aState[ClientID] != STATE_1VS1_NONE && m_aState[ClientID] != STATE_1VS1_DONE; }
	bool HasJoined(int Fight, int Index) { return m_aFights[Fight].m_aParticipants[Index].m_Status == PARTICIPANT_OWNER || m_aFights[Fight].m_aParticipants[Index].m_Status == PARTICIPANT_ACCEPTED; }

	bool OnCharacterSpawn(int ClientID);
	void OnPlayerLeave(int ClientID, bool Disconnect = false);
	void OnPlayerDie(int ClientID);
	void OnInput(int ClientID, CNetObj_PlayerInput *pNewInput);
	bool ClampViewPos(int ClientID);

	void StartConfiguration(int ClientID, int Participant, int ScoreLimit, bool KillBorder);
	bool AcceptFight(int Creator, int ClientID);
	void EndFight(int Fight);

	bool GlobalArenaExists() { return m_GlobalArena.m_Active; }
	bool LongFreezeStart(int ClientID);
	const char *StartGlobalArenaFight(int ClientID1, int ClientID2);
};

#endif // GAME_SERVER_MINIGAMES_ARENAS_H
