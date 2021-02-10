/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_H
#define GAME_SERVER_PLAYER_H

#include "alloc.h"

#include "entities/pickup_drop.h"
#include "entities/portal.h"
#include <vector>

#include "teeinfo.h"
#include "save.h"

enum NoNameFix
{
	FIX_SET_NAME_ONLY = 0,
	FIX_CHAT_MSG,
	FIX_KILL_MSG
};

enum Gamemode
{
	GAMEMODE_DDRACE = 0,
	GAMEMODE_VANILLA
};

enum Scoreformat
{
	SCORE_TIME = 0,
	SCORE_LEVEL,
	SCORE_BLOCK_POINTS
};

enum Portals
{
	PORTAL_FIRST,
	PORTAL_SECOND,
	NUM_PORTALS
};

enum SpectatorFlagSelection
{
	SPEC_SELECT_FLAG_RED = VANILLA_MAX_CLIENTS - SPEC_FLAGRED,
	SPEC_SELECT_FLAG_BLUE = VANILLA_MAX_CLIENTS - SPEC_FLAGBLUE,
};

enum Transaction
{
	TRANSACTION_BANK,
	TRANSACTION_WALLET,
};

enum
{
	WEAPON_MINIGAME_CHANGE = -5, // killed due to minigame swap
	WEAPON_PLAYER = -4, // killed by a player
	WEAPON_GAME = -3, // team switching etc
	WEAPON_SELF = -2, // console kill command
	WEAPON_WORLD = -1, // death tiles etc
};

// player object
class CPlayer
{
	MACRO_ALLOC_POOL_ID()

public:
	CPlayer(CGameContext *pGameServer, int ClientID, bool DebugDummy, bool AsSpec = false);
	~CPlayer();

	void Reset();

	void TryRespawn();
	void Respawn(bool WeakHook = false); // with WeakHook == true the character will be spawned after all calls of Tick from other Players
	CCharacter* ForceSpawn(vec2 Pos); // required for loading savegames
	void SetTeam(int Team, bool DoChatMsg=true);
	int GetTeam() const { return m_Team; };
	int GetCID() const { return m_ClientID; };
	bool IsDummy() const { return m_DebugDummy; }

	void Tick();
	void PostTick();
	void PostPostTick();
	void Snap(int SnappingClient);
	void FakeSnap();

	void TranslatePlayerFlags(CNetObj_PlayerInput *NewInput);
	void OnDirectInput(CNetObj_PlayerInput *NewInput, bool TeeControlled = false);
	void OnPredictedInput(CNetObj_PlayerInput *NewInput, bool TeeControlled = false);
	void OnDisconnect();

	void ThreadKillCharacter(int Weapon = WEAPON_GAME);
	void KillCharacter(int Weapon = WEAPON_GAME, bool UpdateTeeControl = true);
	CCharacter *GetCharacter();

	void SpectatePlayerName(const char* pName);

	//---------------------------------------------------------
	// this is used for snapping so we know how we can clip the view for the player
	vec2 m_ViewPos;
	int m_TuneZone;
	int m_TuneZoneOld;

	// states if the client is chatting, accessing a menu etc.
	int m_PlayerFlags;

	// used for snapping to just update latency if the scoreboard is active
	int m_aActLatency[MAX_CLIENTS];

	// used for spectator mode
	int GetSpecMode() { return m_SpecMode; };
	int GetSpectatorID() const { return m_SpectatorID; }
	bool SetSpectatorID(int SpecMode, int SpectatorID);

	bool m_IsReadyToEnter;
	bool m_IsReadyToPlay;

	//
	int m_Vote;
	int m_VotePos;
	//
	int m_LastVoteCall;
	int m_LastVoteTry;
	int m_LastChat;
	int m_LastSetTeam;
	int m_LastSetSpectatorMode;
	int m_LastChangeInfo;
	int m_LastEmote;
	int m_LastKill;
	int m_LastCommands[4];
	int m_LastCommandPos;
	int m_LastWhisperTo;
	int m_LastReadyChange;

	int m_LastInvited;

	int m_SendVoteIndex;

	CTeeInfo m_TeeInfos;

	int m_PreviousDieTick;
	int m_DieTick;
	int m_Score;
	int m_ScoreStartTick;
	int m_LastActionTick;
	int m_TeamChangeTick;

	struct
	{
		int m_TargetX;
		int m_TargetY;
	} m_LatestActivity;

	// network latency calculations
	struct
	{
		int m_Accum;
		int m_AccumMin;
		int m_AccumMax;
		int m_Avg;
		int m_Min;
		int m_Max;
	} m_Latency;

private:
	CCharacter *m_pCharacter;
	CGameContext *m_pGameServer;

	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;

	//
	bool m_Spawning;
	bool m_WeakHookSpawn;
	int m_ClientID;
	int m_Team;
	bool m_DebugDummy;

	// used for spectator mode
	int m_SpecMode;
	int m_SpectatorID;
	class CFlag *m_pSpecFlag;
	bool m_ActiveSpecSwitch;

	int m_Paused;
	int64 m_ForcePauseTime;
	int64 m_LastPause;

	// F-DDrace
	int m_DummyMode;

public:
	enum
	{
		PAUSE_NONE = 0,
		PAUSE_PAUSED,
		PAUSE_SPEC
	};

	int64 m_FirstVoteTick;
	char m_TimeoutCode[64];

	void ProcessPause();
	int Pause(int State, bool Force);
	int ForcePause(int Time);
	int IsPaused();

	int64 m_Last_Team;
	bool IsPlaying();
	bool m_ShowOthers;
	bool m_ShowAll;
	vec2 m_ShowDistance;
	bool m_SpecTeam;
	bool m_NinjaJetpack;
	int m_KillMe;
	bool m_HasFinishScore;

	int m_ChatScore;

	bool AfkTimer(int new_target_x, int new_target_y); //returns true if kicked
	void UpdatePlaytime();
	void AfkVoteTimer(CNetObj_PlayerInput* NewTarget);
	int64 m_LastPlaytime;
	int64 m_LastEyeEmote;
	int64 m_LastBroadcast;
	bool m_LastBroadcastImportance;
	int m_LastTarget_x;
	int m_LastTarget_y;
	CNetObj_PlayerInput *m_pLastTarget;
	int m_Sent1stAfkWarning; // afk timer's 1st warning after 50% of sv_max_afk_time
	int m_Sent2ndAfkWarning; // afk timer's 2nd warning after 90% of sv_max_afk_time
	char m_pAfkMsg[160];
	bool m_EyeEmote;
	int m_DefEmote;
	int m_DefEmoteReset;
	bool m_Halloween;
	bool m_FirstPacket;
#if defined(CONF_SQL)
	int64 m_LastSQLQuery;
#endif
	bool m_NotEligibleForFinish;
	int64 m_EligibleForFinishCheck;
	bool m_VotedForPractice;

	bool m_Afk;

	// F-DDrace

	//weapon drops
	std::vector< std::vector<CPickupDrop*> > m_vWeaponLimit;

	//dummy
	int GetDummyMode() { return m_DummyMode; }
	void SetDummyMode(int DummyMode);
	bool m_IsDummy;
	int m_FakePing;
	vec2 m_ForceSpawnPos;

	//hide players
	int m_HidePlayerTeam[MAX_CLIENTS];
	int GetHidePlayerTeam(int Asker);

	//gamemodes
	int m_Gamemode;

	//spooky ghost
	bool m_SpookyGhost;

	//no name fix
	bool m_RemovedName;
	void FixForNoName(int ID);
	int m_FixNameID;
	bool m_ShowName;

	bool m_SetRealName;
	int64 m_SetRealNameTick;

	struct ChatFix
	{
		int m_Mode;
		int m_Target;
		char m_Message[256];
	} m_ChatFix;

	struct KillMsgFix
	{
		int m_Killer;
		int m_Victim;
		int m_Weapon;
		int m_ModeSpecial;
	} m_KillMsgFix;

	//flag name fix, because i dont send the gamemsgs for flag drop and capture, that means the names in the caption are wrong when owner changes
	void ForceSetSpectatorID(int SpectatorID) { m_SpectatorID = SpectatorID; }

	//extras
	int m_RainbowSpeed;
	int m_RainbowColor;
	void RainbowTick();

	bool m_InfRainbow;
	int m_InfMeteors;
	bool m_HasSpookyGhost;

	CPortal *m_pPortal[NUM_PORTALS];

	//teecontrol
	void SetTeeControl(CPlayer *pVictim);
	void UnsetTeeControl();
	void ResumeFromTeeControl();
	CPlayer *m_pControlledTee;
	int m_TeeControllerID;
	bool m_HasTeeControl;
	bool m_TeeControlMode;
	int m_TeeControlForcedID;

	//account
	int GetAccID();
	void GiveXP(int64 Amount, const char *pMessage = "");
	void GiveBlockPoints(int Amount);
	bool GiveTaserBattery(int Amount);
	void OnLogin();
	void OnLogout();
	void SetExpireDate(int Item);
	bool IsExpiredItem(int Item);
	void ExpireItems();

	void BankTransaction(int64 Amount, const char *pDescription = "", bool IsEuro = false);
	void WalletTransaction(int64 Amount, const char *pDescription = "");
	void ApplyMoneyHistoryMsg(int Type, int Amount, const char *pDescription);
	int64 GetWalletMoney() { return m_WalletMoney; }
	void SetWalletMoney(int64 Amount) { m_WalletMoney = Amount; }

	char m_aSecurityPin[5];

	// plot
	void CancelPlotAuction();
	void CancelPlotSwap();
	void StopPlotEditing();
	int m_PlotAuctionPrice;
	char m_aPlotSwapUsername[32];
	bool m_PlotSpawn;

	//room key
	bool m_HasRoomKey;

	//score
	int m_ScoreMode;
	int m_InstagibScore;

	//weapon indicator
	bool m_WeaponIndicator;

	// police
	int64 m_EscapeTime;
	int64 m_JailTime;
	int m_SpawnBlockScore;

	//others
	bool IsHooked(int Power = -1);
	void SetPlaying();
	bool m_ResumeMoved;

	bool m_SilentFarm;

	int64 m_LastVote;
	int64 m_LastMoneyXPBomb;

	bool JoinChat(bool Local);
	bool m_LocalChat;
	int GetAuthedHighlighted();

	// automatic actions when player enters afk mode
	void OnSetAfk();

	// clan protection
	bool m_ClanProtectionPunished;
	bool CheckClanProtection();

	//fake information
	void UpdateInformation(int ClientID = -1);
	void SetName(const char *pName) { str_utf8_copy_num(m_CurrentInfo.m_aName, pName, sizeof(m_CurrentInfo.m_aName), MAX_NAME_LENGTH); };
	void SetClan(const char *pClan) { str_utf8_copy_num(m_CurrentInfo.m_aClan, pClan, sizeof(m_CurrentInfo.m_aClan), MAX_CLAN_LENGTH); };

	void ResetSkin(bool Unforce = false);
	void SetSkin(int Skin, bool Force = false);

	struct
	{
		char m_aName[MAX_NAME_ARRAY_SIZE];
		char m_aClan[MAX_CLAN_ARRAY_SIZE];
		CTeeInfo m_TeeInfos;
	} m_CurrentInfo;

	int m_ForcedSkin;

	//minigames
	CSaveTee m_MinigameTee;
	bool m_SavedMinigameTee;
	void SaveMinigameTee();
	bool LoadMinigameTee();
	bool CanLoadMinigameTee();

	bool IsMinigame();
	int m_Minigame;
	int m_SurvivalState;
	int m_SurvivalDieTick;

	// 128p
	void SendConnect(int ClientID, int FakeID);
	void SendDisconnect(int ClientID, int FakeID);
	void SetFakeID();
	int m_FakeID;
	bool m_aSameIP[MAX_CLIENTS];

	// shutdown tee
	bool m_CheckedShutdownSaved;

private:
	int64 m_WalletMoney;
};

#endif
