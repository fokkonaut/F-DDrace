/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTEXT_H
#define GAME_SERVER_GAMECONTEXT_H

#include <engine/console.h>
#include <engine/server.h>

#include <game/layers.h>
#include <game/voting.h>

#include <vector>
#include <game/server/entities/pickup_drop.h>

#include "eventhandler.h"
#include "gameworld.h"

#include "teehistorian.h"

#include "score.h"
#ifdef _MSC_VER
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif
/*
	Tick
		Game Context (CGameContext::tick)
			Game World (GAMEWORLD::tick)
				Reset world if requested (GAMEWORLD::reset)
				All entities in the world (ENTITY::tick)
				All entities in the world (ENTITY::tick_defered)
				Remove entities marked for deletion (GAMEWORLD::remove_entities)
			Game Controller (GAMECONTROLLER::tick)
			All players (CPlayer::tick)


	Snap
		Game Context (CGameContext::snap)
			Game World (GAMEWORLD::snap)
				All entities in the world (ENTITY::snap)
			Game Controller (GAMECONTROLLER::snap)
			Events handler (EVENT_HANDLER::snap)
			All players (CPlayer::snap)

*/

enum Shop
{
	SHOP_PAGE_NONE = -1,
	SHOP_PAGE_MAIN,

	SHOP_STATE_NONE = 0,
	SHOP_STATE_OPENED_WINDOW,
	SHOP_STATE_CONFIRM,
};

enum Item
{
	SPOOKY_GHOST,
	POLICE,
	NUM_ITEMS
};

enum Minigames
{
	MINIGAME_NONE = 0,
	MINIGAME_BLOCK,
	MINIGAME_SURVIVAL,
	MINIGAME_INSTAGIB_BOOMFNG,
	MINIGAME_INSTAGIB_FNG,
	NUM_MINIGAMES
};

enum Survival
{
	SURVIVAL_OFFLINE = 0,
	SURVIVAL_LOBBY,
	SURVIVAL_PLAYING,
	SURVIVAL_DEATHMATCH,

	BACKGROUND_IDLE = -1,
	BACKGROUND_LOBBY_WAITING,
	BACKGROUND_LOBBY_COUNTDOWN,
	BACKGROUND_DEATHMATCH_COUNTDOWN,
};

enum Top5
{
	TOP_LEVEL,
	TOP_POINTS,
	TOP_MONEY,
	TOP_SPREE,
};

#define ACC_START 1 // account ids start with 1, 0 means not logged in
#define MAX_LEVEL 100

enum
{
	NUM_TUNEZONES = 256
};

class CRandomMapResult;
class CMapVoteResult;

class CGameContext : public IGameServer
{
	IServer *m_pServer;
	class IConsole *m_pConsole;
	IStorage* m_pStorage;
	CLayers m_Layers;
	CCollision m_Collision;
	CNetObjHandler m_NetObjHandler;
	CTuningParams m_Tuning;
	CTuningParams m_aTuningList[NUM_TUNEZONES];

	bool m_TeeHistorianActive;
	CTeeHistorian m_TeeHistorian;
	IOHANDLE m_TeeHistorianFile;
	CUuid m_GameUuid;

	std::shared_ptr<CRandomMapResult> m_pRandomMapResult;
	std::shared_ptr<CMapVoteResult> m_pMapVoteResult;

	static void CommandCallback(int ClientID, int FlagMask, const char *pCmd, IConsole::IResult *pResult, void *pUser);
	static void TeeHistorianWrite(const void *pData, int DataSize, void *pUser);

	static void ConTuneParam(IConsole::IResult *pResult, void *pUserData);
	static void ConToggleTuneParam(IConsole::IResult* pResult, void* pUserData);
	static void ConTuneReset(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneDump(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneZone(IConsole::IResult* pResult, void* pUserData);
	static void ConTuneDumpZone(IConsole::IResult* pResult, void* pUserData);
	static void ConTuneResetZone(IConsole::IResult* pResult, void* pUserData);
	static void ConTuneSetZoneMsgEnter(IConsole::IResult* pResult, void* pUserData);
	static void ConTuneSetZoneMsgLeave(IConsole::IResult* pResult, void* pUserData);
	static void ConSwitchOpen(IConsole::IResult* pResult, void* pUserData);
	static void ConPause(IConsole::IResult* pResult, void* pUserData);	static void ConChangeMap(IConsole::IResult *pResult, void *pUserData);
	static void ConRestart(IConsole::IResult *pResult, void *pUserData);
	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConBroadcast(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTeamAll(IConsole::IResult *pResult, void *pUserData);
	static void ConAddVote(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveVote(IConsole::IResult *pResult, void *pUserData);
	static void ConClearVotes(IConsole::IResult *pResult, void *pUserData);
	static void ConVote(IConsole::IResult *pResult, void *pUserData);
	static void ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainSettingUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainGameinfoUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	static void ConchainUpdateHidePlayers(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData);

	// DDRace

	static void ConRandomMap(IConsole::IResult *pResult, void *pUserData);
	static void ConRandomUnfinishedMap(IConsole::IResult *pResult, void *pUserData);

	CGameContext(int Resetting);
	void Construct(int Resetting);

	bool m_Resetting;
public:
	IServer *Server() const { return m_pServer; }
	class IConsole *Console() { return m_pConsole; }
	IStorage* Storage() { return m_pStorage; }
	CCollision *Collision() { return &m_Collision; }
	CTuningParams *Tuning() { return &m_Tuning; }
	CTuningParams* TuningList() { return &m_aTuningList[0]; }

	CGameContext();
	~CGameContext();

	void Clear();

	CEventHandler m_Events;
	class CPlayer *m_apPlayers[MAX_CLIENTS];

	class IGameController *m_pController;
	CGameWorld m_World;

	// helper functions
	class CCharacter *GetPlayerChar(int ClientID);

	int m_LockTeams;

	// voting
	void StartVote(const char *pDesc, const char *pCommand, const char *pReason);
	void EndVote(int Type, bool Force);
	void ForceVote(int Type, const char *pDescription, const char *pReason);
	void SendVoteSet(int Type, int ToClientID);
	void SendVoteStatus(int ClientID, int Total, int Yes, int No);
	void AbortVoteOnDisconnect(int ClientID);
	void AbortVoteOnTeamChange(int ClientID);

	int m_VoteCreator;
	int m_VoteType;
	int64 m_VoteCloseTime;
	int64 m_VoteCancelTime;
	bool m_VoteUpdate;
	int m_VotePos;
	char m_aVoteDescription[VOTE_DESC_LENGTH];
	char m_aVoteCommand[VOTE_CMD_LENGTH];
	char m_aVoteReason[VOTE_REASON_LENGTH];
	int m_VoteClientID;
	int m_NumVoteOptions;
	int m_VoteEnforce;
	char m_aaZoneEnterMsg[NUM_TUNEZONES][256]; // 0 is used for switching from or to area without tunings
	char m_aaZoneLeaveMsg[NUM_TUNEZONES][256];

	char m_aDeleteTempfile[128];
	void DeleteTempfile();

	enum
	{
		VOTE_ENFORCE_UNKNOWN=0,
		VOTE_ENFORCE_NO,
		VOTE_ENFORCE_YES,

		VOTE_TIME=25,
		VOTE_CANCEL_TIME = 10,

		MIN_SKINCHANGE_CLIENTVERSION = 0x0703,
	};
	class CHeap *m_pVoteOptionHeap;
	CVoteOptionServer *m_pVoteOptionFirst;
	CVoteOptionServer *m_pVoteOptionLast;

	// helper functions
	void CreateDamage(vec2 Pos, int Id, vec2 Source, int HealthAmount, int ArmorAmount, bool Self, int64_t Mask = -1LL);
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, int64_t Mask = -1LL);
	void CreateHammerHit(vec2 Pos, int64_t Mask = -1LL);
	void CreatePlayerSpawn(vec2 Pos, int64_t Mask = -1LL);
	void CreateDeath(vec2 Pos, int Who, int64_t Mask = -1LL);
	void CreateSound(vec2 Pos, int Sound, int64_t Mask = -1LL);

	void CreateSoundGlobal(int Sound);
	void CreateSound(int Sound, int ClientID);

	// network
	void SendChatTarget(int To, const char* pText);
	void SendChatTeam(int Team, const char* pText);
	void SendChat(int ChatterClientID, int Mode, int To, const char *pText, int SpamProtectionClientID = -1);
	void SendBroadcast(const char* pText, int ClientID, bool IsImportant = true);
	void SendEmoticon(int ClientID, int Emoticon);
	void SendWeaponPickup(int ClientID, int Weapon);
	void SendSettings(int ClientID);
	void SendSkinChange(int ClientID, int TargetID);

	// DDRace
	void CallVote(int ClientID, const char *aDesc, const char *aCmd, const char *pReason, const char *aChatmsg);

	void List(int ClientID, const char* filter);

	void SendGameMsg(int GameMsgID, int ClientID);
	void SendGameMsg(int GameMsgID, int ParaI1, int ClientID);
	void SendGameMsg(int GameMsgID, int ParaI1, int ParaI2, int ParaI3, int ClientID);

	//
	void SendTuningParams(int ClientID, int Zone = 0);

	void LoadMapSettings();

	// engine events
	virtual void OnInit();
	virtual void OnConsoleInit();
	virtual void OnMapChange(char* pNewMapName, int MapNameSize);
	virtual void OnShutdown(bool FullShutdown = false);

	virtual void OnTick();
	virtual void OnPreSnap();
	virtual void OnSnap(int ClientID);
	virtual void OnPostSnap();

	virtual void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID);

	virtual void OnClientConnected(int ClientID, bool AsSpec) { OnClientConnected(ClientID, false, AsSpec); }
	void OnClientConnected(int ClientID, bool Dummy, bool AsSpec);
	void OnClientTeamChange(int ClientID);
	virtual void OnClientEnter(int ClientID);
	virtual void OnClientDrop(int ClientID, const char *pReason);
	virtual void OnClientAuth(int ClientID, int Level);
	virtual void OnClientDirectInput(int ClientID, void *pInput);
	virtual void OnClientPredictedInput(int ClientID, void *pInput);

	virtual void OnClientEngineJoin(int ClientID);
	virtual void OnClientEngineDrop(int ClientID, const char *pReason);

	virtual bool IsClientReady(int ClientID) const;
	virtual bool IsClientPlayer(int ClientID) const;
	virtual bool IsClientSpectator(int ClientID) const;

	virtual const CUuid GameUuid() const;
	virtual const char *GameType() const;
	virtual const char *Version() const;
	virtual const char *NetVersion() const;

	int ProcessSpamProtection(int ClientID);
	int GetDDRaceTeam(int ClientID);
	int64 m_NonEmptySince;
	int64 m_LastMapVote;
	void ForceVote(int EnforcerID, bool Success);

	// F-DDrace
	
	//dummy
	void ConnectDummy(int Dummymode = 0, vec2 Pos = vec2(-1, -1));
	void ConnectDefaultDummies();
	void SetV3Offset(int X = -1, int Y = -1);

	bool IsShopDummy(int ClientID);
	int GetShopDummy();

	int GetNextClientID(bool Inverted = false);

	void UpdateHidePlayers(int ClientID = -1);

	//account
	int GetAccount(const char* pUsername);
	void FreeAccount(int ID, bool Silent = false);

	struct TopAccounts
	{
		int m_Level;
		int m_Points;
		int64 m_Money;
		int m_KillStreak;
		char m_aUsername[32];
	};
	std::vector<TopAccounts> m_TempTopAccounts;
	static int TopAccountsCallback(const char* pName, int IsDir, int StorageType, void* pUser);
	void UpdateTopAccounts(int Type);

	static int LogoutAccountsCallback(const char* pName, int IsDir, int StorageType, void* pUser);
	int AddAccount();
	void ReadAccountStats(int ID, const char* pName);
	void WriteAccountStats(int ID);
	void Logout(int ID);

	int m_aTaserPrice[7];
	int m_aNeededXP[MAX_LEVEL + 1];
	int m_LastAccSaveTick;

	struct AccountInfo
	{
		int m_Port;
		bool m_LoggedIn;
		bool m_Disabled;
		char m_Password[32];
		char m_Username[32];
		int m_ClientID;
		int m_Level;
		int m_XP;
		int64 m_Money;
		int m_Kills;
		int m_Deaths;
		int m_PoliceLevel;
		int m_SurvivalKills;
		int m_SurvivalWins;
		bool m_aHasItem[NUM_ITEMS];
		char m_aLastMoneyTransaction[5][256];
		bool m_VIP;
		int m_BlockPoints;
		int m_InstagibKills;
		int m_InstagibWins;
		int m_SpawnWeapon[3];
		bool m_Ninjajetpack;
		char m_aLastPlayerName[32];
		int m_SurvivalDeaths;
		int m_InstagibDeaths;
		int m_TaserLevel;
		int m_KillingSpreeRecord;
	};
	std::vector<AccountInfo> m_Accounts;

	// make sure these are in the same order as the variables above
	enum AccountVariables
	{
		PORT,
		LOGGED_IN,
		DISABLED,
		PASSWORD,
		USERNAME,
		CLIENT_ID,
		LEVEL,
		XP,
		MONEY,
		KILLS,
		DEATHS,
		POLICE_LEVEL,
		SURVIVAL_KILLS,
		SURVIVAL_WINS,
		ITEM_SPOOKY_GHOST,
		ITEM_POLICE,
		LAST_MONEY_TRANSACTION_0,
		LAST_MONEY_TRANSACTION_1,
		LAST_MONEY_TRANSACTION_2,
		LAST_MONEY_TRANSACTION_3,
		LAST_MONEY_TRANSACTION_4,
		VIP,
		BLOCK_POINTS,
		INSTAGIB_KILLS,
		INSTAGIB_WINS,
		SPAWN_WEAPON_0,
		SPAWN_WEAPON_1,
		SPAWN_WEAPON_2,
		NINJAJETPACK,
		LAST_PLAYER_NAME,
		SURVIVAL_DEATHS,
		INSTAGIB_DEATHS,
		TASER_LEVEL,
		KILLING_SPREE_RECORD,
		NUM_ACCOUNT_VARIABLES
	};

	//motd
	const char* FixMotd(const char* pMsg);

	//extras
	void SendExtraMessage(int Extra, int ToID, bool Set, int FromID, bool Silent, int Special = 0);
	const char* CreateExtraMessage(int Extra, bool Set, int FromID, int ToID, int Special);
	const char* GetExtraName(int Extra, int Special = 0);

	bool IsValidHookPower(int HookPower);

	//others
	int GetCIDByName(const char* pName);
	void SendMotd(const char* pMsg, int ClientID);

	const char* GetWeaponName(int Weapon);
	int GetRealWeapon(int Weapon);
	int GetRealPickupType(int Type, int Subtype);

	const char* GetMinigameName(int Minigame);
	const char* GetMinigameCommand(int Minigame);

	int CountConnectedPlayers(bool CountSpectators = true, bool ExcludeBots = false);

	void CreateLaserText(vec2 Pos, int Owner, const char* pText);

	//pickup drops
	std::vector<CPickupDrop*> m_vPickupDropLimit;

	//minigames disabled
	bool m_aMinigameDisabled[NUM_MINIGAMES];

	//survival
	void SurvivalTick();
	void SetPlayerSurvivalState(int State);
	void SendSurvivalBroadcast(const char* pMsg, bool Sound = false, bool IsImportant = true);
	int CountSurvivalPlayers(int State);
	int GetRandomSurvivalPlayer(int State, int NotThis = -1);
	int m_SurvivalBackgroundState;
	int m_SurvivalGameState;
	int64 m_SurvivalTick;
	int m_SurvivalWinner;

	//instagib
	void InstagibTick(int Type);

private:

	bool m_VoteWillPass;
	class IScore* m_pScore;

	//DDRace Console Commands

	static void ConKillPlayer(IConsole::IResult* pResult, void* pUserData);

	static void ConNinja(IConsole::IResult* pResult, void* pUserData);
	static void ConUnSolo(IConsole::IResult* pResult, void* pUserData);
	static void ConUnDeep(IConsole::IResult* pResult, void* pUserData);
	static void ConUnSuper(IConsole::IResult* pResult, void* pUserData);
	static void ConSuper(IConsole::IResult* pResult, void* pUserData);
	static void ConShotgun(IConsole::IResult* pResult, void* pUserData);
	static void ConGrenade(IConsole::IResult* pResult, void* pUserData);
	static void ConRifle(IConsole::IResult* pResult, void* pUserData);
	static void ConWeapons(IConsole::IResult* pResult, void* pUserData);
	static void ConUnShotgun(IConsole::IResult* pResult, void* pUserData);
	static void ConUnGrenade(IConsole::IResult* pResult, void* pUserData);
	static void ConUnRifle(IConsole::IResult* pResult, void* pUserData);
	static void ConUnWeapons(IConsole::IResult* pResult, void* pUserData);
	static void ConAddWeapon(IConsole::IResult* pResult, void* pUserData);
	static void ConRemoveWeapon(IConsole::IResult* pResult, void* pUserData);

	void ModifyWeapons(IConsole::IResult* pResult, void* pUserData, int Weapon, bool Remove, bool AddRemoveCommand = false);
	void MoveCharacter(int ClientID, int X, int Y, bool Raw = false);
	static void ConGoLeft(IConsole::IResult* pResult, void* pUserData);
	static void ConGoRight(IConsole::IResult* pResult, void* pUserData);
	static void ConGoUp(IConsole::IResult* pResult, void* pUserData);
	static void ConGoDown(IConsole::IResult* pResult, void* pUserData);
	static void ConMove(IConsole::IResult* pResult, void* pUserData);
	static void ConMoveRaw(IConsole::IResult* pResult, void* pUserData);

	static void ConToTeleporter(IConsole::IResult* pResult, void* pUserData);
	static void ConToCheckTeleporter(IConsole::IResult* pResult, void* pUserData);
	static void ConTeleport(IConsole::IResult* pResult, void* pUserData);

	static void ConCredits(IConsole::IResult* pResult, void* pUserData);
	static void ConInfo(IConsole::IResult* pResult, void* pUserData);
	static void ConHelp(IConsole::IResult* pResult, void* pUserData);
	static void ConSettings(IConsole::IResult* pResult, void* pUserData);
	static void ConKill(IConsole::IResult* pResult, void* pUserData);
	static void ConTogglePause(IConsole::IResult* pResult, void* pUserData);
	static void ConTogglePauseVoted(IConsole::IResult* pResult, void* pUserData);
	static void ConToggleSpec(IConsole::IResult* pResult, void* pUserData);
	static void ConToggleSpecVoted(IConsole::IResult* pResult, void* pUserData);
	static void ConForcePause(IConsole::IResult* pResult, void* pUserData);
	static void ConTeamTop5(IConsole::IResult *pResult, void *pUserData);
	static void ConTop5(IConsole::IResult* pResult, void* pUserData);

	static void ConMapInfo(IConsole::IResult* pResult, void* pUserData);
	static void ConMap(IConsole::IResult *pResult, void *pUserData);
	static void ConTeamRank(IConsole::IResult* pResult, void* pUserData);
	static void ConRank(IConsole::IResult* pResult, void* pUserData);
	static void ConJoinTeam(IConsole::IResult* pResult, void* pUserData);
	static void ConLockTeam(IConsole::IResult* pResult, void* pUserData);
	static void ConInviteTeam(IConsole::IResult* pResult, void* pUserData);
	static void ConMe(IConsole::IResult* pResult, void* pUserData);
	static void ConWhisper(IConsole::IResult* pResult, void* pUserData);
	static void ConConverse(IConsole::IResult* pResult, void* pUserData);
	static void ConSetEyeEmote(IConsole::IResult* pResult, void* pUserData);
	static void ConEyeEmote(IConsole::IResult* pResult, void* pUserData);
	static void ConShowOthers(IConsole::IResult* pResult, void* pUserData);
	static void ConShowAll(IConsole::IResult* pResult, void* pUserData);
	static void ConSpecTeam(IConsole::IResult* pResult, void* pUserData);
	static void ConNinjaJetpack(IConsole::IResult* pResult, void* pUserData);
	static void ConSayTime(IConsole::IResult* pResult, void* pUserData);
	static void ConSayTimeAll(IConsole::IResult* pResult, void* pUserData);
	static void ConTime(IConsole::IResult* pResult, void* pUserData);
	static void ConRescue(IConsole::IResult* pResult, void* pUserData);
	static void ConProtectedKill(IConsole::IResult* pResult, void* pUserData);

	static void ConVoteMute(IConsole::IResult* pResult, void* pUserData);
	static void ConVoteUnmute(IConsole::IResult* pResult, void* pUserData);
	static void ConVoteMutes(IConsole::IResult* pResult, void* pUserData);
	static void ConMute(IConsole::IResult* pResult, void* pUserData);
	static void ConMuteID(IConsole::IResult* pResult, void* pUserData);
	static void ConMuteIP(IConsole::IResult* pResult, void* pUserData);
	static void ConUnmute(IConsole::IResult* pResult, void* pUserData);
	static void ConMutes(IConsole::IResult* pResult, void* pUserData);

	static void ConList(IConsole::IResult* pResult, void* pUserData);
	static void ConSetDDRTeam(IConsole::IResult* pResult, void* pUserData);
	static void ConUninvite(IConsole::IResult* pResult, void* pUserData);

	//chat
	static void ConScore(IConsole::IResult* pResult, void* pUserData);
	static void ConWeaponIndicator(IConsole::IResult* pResult, void* pUserData);

	static void ConSpookyGhostInfo(IConsole::IResult* pResult, void* pUserData);
	static void ConVIPInfo(IConsole::IResult* pResult, void* pUserData);
	static void ConPoliceInfo(IConsole::IResult* pResult, void* pUserData);
	static void ConSpawnWeaponsInfo(IConsole::IResult* pResult, void* pUserData);
	static void ConAccountInfo(IConsole::IResult* pResult, void* pUserData);
	static void ConTaserInfo(IConsole::IResult* pResult, void* pUserData);

	static void ConLogin(IConsole::IResult* pResult, void* pUserData);
	static void ConLogout(IConsole::IResult* pResult, void* pUserData);
	static void ConRegister(IConsole::IResult* pResult, void* pUserData);
	static void ConChangePassword(IConsole::IResult* pResult, void* pUserData);

	static void ConPayMoney(IConsole::IResult* pResult, void* pUserData);
	static void ConMoney(IConsole::IResult* pResult, void* pUserData);

	static void ConRoom(IConsole::IResult* pResult, void* pUserData);

	static void ConSmoothFreeze(IConsole::IResult* pResult, void* pUserData);

	void SetMinigame(IConsole::IResult* pResult, void* pUserData, int Minigame);
	static void ConMinigames(IConsole::IResult* pResult, void* pUserData);
	static void ConLeaveMinigame(IConsole::IResult* pResult, void* pUserData);
	static void ConJoinBlock(IConsole::IResult* pResult, void* pUserData);
	static void ConJoinSurvival(IConsole::IResult* pResult, void* pUserData);
	static void ConJoinBoomFNG(IConsole::IResult* pResult, void* pUserData);
	static void ConJoinFNG(IConsole::IResult* pResult, void* pUserData);

	static void ConResumeMoved(IConsole::IResult* pResult, void* pUserData);

	static void ConStats(IConsole::IResult* pResult, void* pUserData);

	void SendTop5AccMessage(IConsole::IResult* pResult, void* pUserData, int Type);
	static void ConTop5Level(IConsole::IResult* pResult, void* pUserData);
	static void ConTop5Points(IConsole::IResult* pResult, void* pUserData);
	static void ConTop5Money(IConsole::IResult* pResult, void* pUserData);
	static void ConTop5Spree(IConsole::IResult* pResult, void* pUserData);

	static void ConPoliceHelper(IConsole::IResult* pResult, void* pUserData);

	//rcon
	static void ConFreezeHammer(IConsole::IResult* pResult, void* pUserData);

	static void ConAllWeapons(IConsole::IResult* pResult, void* pUserData);
	static void ConUnAllWeapons(IConsole::IResult* pResult, void* pUserData);

	static void ConExtraWeapons(IConsole::IResult* pResult, void* pUserData);
	static void ConUnExtraWeapons(IConsole::IResult* pResult, void* pUserData);

	static void ConPlasmaRifle(IConsole::IResult* pResult, void* pUserData);
	static void ConUnPlasmaRifle(IConsole::IResult* pResult, void* pUserData);
	static void ConHeartGun(IConsole::IResult* pResult, void* pUserData);
	static void ConUnHeartGun(IConsole::IResult* pResult, void* pUserData);
	static void ConStraightGrenade(IConsole::IResult* pResult, void* pUserData);
	static void ConUnStraightGrenade(IConsole::IResult* pResult, void* pUserData);
	static void ConTelekinesis(IConsole::IResult* pResult, void* pUserData);
	static void ConUnTelekinesis(IConsole::IResult* pResult, void* pUserData);
	static void ConLightsaber(IConsole::IResult* pResult, void* pUserData);
	static void ConUnLightsaber(IConsole::IResult* pResult, void* pUserData);
	static void ConTeleRifle(IConsole::IResult* pResult, void* pUserData);
	static void ConUnTeleRifle(IConsole::IResult* pResult, void* pUserData);
	static void ConProjectileRifle(IConsole::IResult* pResult, void* pUserData);
	static void ConUnProjectileRifle(IConsole::IResult* pResult, void* pUserData);
	static void ConBallGrenade(IConsole::IResult* pResult, void* pUserData);
	static void ConUnBallGrenade(IConsole::IResult* pResult, void* pUserData);

	static void ConHammer(IConsole::IResult* pResult, void* pUserData);
	static void ConGun(IConsole::IResult* pResult, void* pUserData);
	static void ConUnHammer(IConsole::IResult* pResult, void* pUserData);
	static void ConUnGun(IConsole::IResult* pResult, void* pUserData);

	static void ConScrollNinja(IConsole::IResult* pResult, void* pUserData);

	static void ConSound(IConsole::IResult* pResult, void* pUserData);

	static void ConConnectDummy(IConsole::IResult* pResult, void* pUserData);
	static void ConDisconnectDummy(IConsole::IResult* pResult, void* pUserData);
	static void ConDummymode(IConsole::IResult* pResult, void* pUserData);
	static void ConConnectDefaultDummies(IConsole::IResult* pResult, void* pUserData);

	static void ConForceFlagOwner(IConsole::IResult* pResult, void* pUserData);

	static void ConPlayerName(IConsole::IResult* pResult, void* pUserData);
	static void ConPlayerClan(IConsole::IResult* pResult, void* pUserData);

	static void ConAccInfo(IConsole::IResult* pResult, void* pUserData);
	static void ConPlayerInfo(IConsole::IResult* pResult, void* pUserData);
	static void ConLaserText(IConsole::IResult* pResult, void* pUserData);

	static void ConItem(IConsole::IResult* pResult, void* pUserData);
	static void ConInvisible(IConsole::IResult* pResult, void* pUserData);
	static void ConHookPower(IConsole::IResult* pResult, void* pUserData);

	static void ConInfiniteJumps(IConsole::IResult* pResult, void* pUserData);
	static void ConEndlessHook(IConsole::IResult* pResult, void* pUserData);
	static void ConJetpack(IConsole::IResult* pResult, void* pUserData);
	static void ConSpookyGhost(IConsole::IResult* pResult, void* pUserData);
	static void ConSpooky(IConsole::IResult* pResult, void* pUserData);
	static void ConRainbowSpeed(IConsole::IResult* pResult, void* pUserData);
	static void ConRainbow(IConsole::IResult* pResult, void* pUserData);
	static void ConInfRainbow(IConsole::IResult* pResult, void* pUserData);
	static void ConAtom(IConsole::IResult* pResult, void* pUserData);
	static void ConTrail(IConsole::IResult* pResult, void* pUserData);
	static void ConAddMeteor(IConsole::IResult* pResult, void* pUserData);
	static void ConAddInfMeteor(IConsole::IResult* pResult, void* pUserData);
	static void ConRemoveMeteors(IConsole::IResult* pResult, void* pUserData);
	static void ConPassive(IConsole::IResult* pResult, void* pUserData);
	static void ConVanillaMode(IConsole::IResult* pResult, void* pUserData);
	static void ConDDraceMode(IConsole::IResult* pResult, void* pUserData);
	static void ConBloody(IConsole::IResult* pResult, void* pUserData);
	static void ConStrongBloody(IConsole::IResult* pResult, void* pUserData);

	static void ConAlwaysTeleWeapon(IConsole::IResult* pResult, void* pUserData);
	static void ConTeleGun(IConsole::IResult* pResult, void* pUserData);
	static void ConTeleGrenade(IConsole::IResult* pResult, void* pUserData);
	static void ConTeleLaser(IConsole::IResult* pResult, void* pUserData);

	static void ConDoorHammer(IConsole::IResult* pResult, void* pUserData);

	static void ConSpinBot(IConsole::IResult* pResult, void* pUserData);
	static void ConSpinBotSpeed(IConsole::IResult* pResult, void* pUserData);
	static void ConAimClosest(IConsole::IResult* pResult, void* pUserData);

	static void ConAccLogout(IConsole::IResult* pResult, void* pUserData);
	static void ConAccDisable(IConsole::IResult* pResult, void* pUserData);
	static void ConAccVIP(IConsole::IResult* pResult, void* pUserData);

	static void ConSayBy(IConsole::IResult* pResult, void* pUserData);

	enum
	{
		MAX_MUTES = 32,
		MAX_VOTE_MUTES = 32,
	};
	struct CMute
	{
		NETADDR m_Addr;
		int m_Expire;
	};
	struct CVoteMute
	{
		NETADDR m_Addr;
		int m_Expire;
	};

	CMute m_aMutes[MAX_MUTES];
	int m_NumMutes;
	CVoteMute m_aVoteMutes[MAX_VOTE_MUTES];
	int m_NumVoteMutes;
	bool TryMute(const NETADDR *pAddr, int Secs);
	void Mute(const NETADDR *pAddr, int Secs, const char *pDisplayName);
	bool TryVoteMute(const NETADDR* pAddr, int Secs);
	bool VoteMute(const NETADDR* pAddr, int Secs, const char* pDisplayName, int AuthedID);
	bool VoteUnmute(const NETADDR* pAddr, const char* pDisplayName, int AuthedID);

public:
	CLayers* Layers() { return &m_Layers; }
	class IScore* Score() { return m_pScore; }
	bool m_VoteKick;
	bool m_VoteSpec;
	enum
	{
		VOTE_ENFORCE_NO_ADMIN = VOTE_ENFORCE_YES + 1,
		VOTE_ENFORCE_YES_ADMIN
	};
	int m_VoteEnforcer;
	static void SendChatResponse(const char* pLine, void* pUser, bool Highlighted = false);
	static void SendChatResponseAll(const char* pLine, void* pUser);
	virtual bool PlayerCollision();
	virtual bool PlayerHooking();
	virtual float PlayerJetpack();

	void ResetTuning();

	int m_ChatResponseTargetID;
	int m_ChatPrintCBIndex;
};

inline int64_t CmaskAll() { return -1LL; }
inline int64_t CmaskOne(int ClientID) { return 1LL<<ClientID; }
inline int64_t CmaskUnset(int64_t Mask, int ClientID) { return Mask^CmaskOne(ClientID); }
inline int64_t CmaskAllExceptOne(int ClientID) { return CmaskUnset(CmaskAll(), ClientID); }
inline bool CmaskIsSet(int64_t Mask, int ClientID) { return (Mask&CmaskOne(ClientID)) != 0; }
#endif
