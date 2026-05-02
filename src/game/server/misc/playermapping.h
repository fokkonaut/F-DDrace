// made by fokkonaut

#ifndef GAME_SERVER_MISC_PLAYERMAPPING_H
#define GAME_SERVER_MISC_PLAYERMAPPING_H

#include <engine/shared/protocol.h>
#include <generated/protocol.h>
#include <game/teamscore.h>

class CGameContext;
class CPlayer;
class CConfig;
class IServer;

class CPlayerMapping
{
	CGameContext *m_pGameServer;
	CConfig *m_pConfig;
	IServer *m_pServer;

	struct PlayerMap
	{
		enum SSeeOthers {
			STATE_NONE = -1,
			STATE_PAGE_FIRST,
			STATE_PAGE_SECOND,

			MAX_NUM_SEE_OTHERS = 34,
		};

		void Init(int ClientID, CPlayerMapping *pPlayerMapping);
		void InitPlayer(bool Rejoin, bool Timeout);
		CPlayerMapping *m_pPlayerMapping;
		CPlayer *GetPlayer();
		int m_ClientID;
		int m_NumReserved;
		bool m_UpdateTeamsState;
		bool m_aReserved[MAX_CLIENTS];
		bool m_ResortReserved;
		int *m_pMap;
		int *m_pReverseMap;
		void Update();
		void Add(int MapID, int ClientID);
		int Remove(int MapID);
		void InsertNextEmpty(int ClientID);
		int GetMapSize();
		// See others
		int m_SeeOthersState;
		int m_TotalOverhang;
		int m_NumPages;
		int m_NumSeeOthers;
		bool m_aWasSeeOthers[MAX_CLIENTS];
		bool m_DoSeeOthersByVote;
		void DoSeeOthers();
		void CycleSeeOthers();
		void UpdateSeeOthers();
		void ResetSeeOthers();
		int GetSpecSelectFlag(int SpecFlag);
		void AddToNumReserved(int Summand);
	} m_aMap[MAX_CLIENTS];
	void UpdatePlayerMap(int ClientID);
	//int m_aTeamSizes[MAX_CLIENTS];

public:
	CGameContext *GameServer() { return m_pGameServer; }
	CConfig *Config() { return m_pConfig; }
	IServer *Server() { return m_pServer; }

	void Init(CGameContext *pGameServer);
	void Tick();

	void InitPlayerMap(int ClientID, bool Rejoin = false, bool Timeout = false) { m_aMap[ClientID].InitPlayer(Rejoin, Timeout); }
	void UpdateTeamsState(int ClientID) { m_aMap[ClientID].m_UpdateTeamsState = true; }
	void ForceInsertPlayer(int Insert, int ClientID) { m_aMap[ClientID].InsertNextEmpty(Insert); }
	void AddToNumReserved(int ClientID, int Summand) { m_aMap[ClientID].AddToNumReserved(Summand); }
	bool ReserveTeamSlots(int DDTeam, int AskerID);

	enum
	{
		SEE_OTHERS_IND_NONE = -1,
		SEE_OTHERS_IND_PLAYER,
		SEE_OTHERS_IND_BUTTON,
	};

	// We start after the seeother indicator/button, and can take all the ids up to m_NumReserved, we may not interfere with GetMapSize()
	int GetFirstDurakID(int ClientID) { return GetSeeOthersID(ClientID) - 1; }
	int GetSeeOthersID(int ClientID);
	bool DoSeeOthers(int ClientId, int SelectedId, bool DoByVote = false);
	void ResetSeeOthers(int ClientID);
	int GetTotalOverhang(int ClientID);
	int GetSeeOthersInd(int ClientID, int MapID);
	const char *GetSeeOthersName(int ClientID);

	int GetSpecSelectFlag(int ClientID, int SpecFlag) { return m_aMap[ClientID].GetSpecSelectFlag(SpecFlag); }
};

#endif // GAME_SERVER_MISC_PLAYERMAPPING_H
