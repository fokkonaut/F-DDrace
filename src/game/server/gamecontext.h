/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTEXT_H
#define GAME_SERVER_GAMECONTEXT_H

#include <engine/console.h>
#include <engine/server.h>

#include <game/commands.h>
#include <game/layers.h>
#include <game/voting.h>

#include <vector>
#include "entities/pickup_drop.h"
#include "entities/money.h"
#include "houses/house.h"

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

#include "mask128.h"

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

enum
{
	// start for account ids and plot ids
	ACC_START = 1, // account ids start with 1, 0 means not logged in
	PLOT_START = 1,

	// needed xp after the hardcoded requirements
	DIFFERENCE_XP_END = 100,
	OVER_LVL_100_XP = 2000000,

	// item maximums
	NUM_TASER_LEVELS = 7,
	NUM_POLICE_LEVELS = 5,
	MAX_TASER_BATTERY = 100,
	MAX_PASSWORD_LENGTH = 128,

	// update this one with every acc change you do
	ACC_CURRENT_VERSION = 5,

	// motd
	MOTD_MAX_LINES = 24,
};

enum
{
	TYPE_DONATION,
	TYPE_PURCHASE
};


enum
{
	NUM_TUNEZONES = 256
};

class CRandomMapResult;
class CMapVoteResult;

class CGameContext : public IGameServer
{
	IServer *m_pServer;
	class CConfig *m_pConfig;
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
	static void ConTunes(IConsole::IResult *pResult, void *pUserData);
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
	static void ConchainUpdateLocalChat(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData);

	static void NewCommandHook(const CCommandManager::CCommand *pCommand, void *pContext);
	static void RemoveCommandHook(const CCommandManager::CCommand *pCommand, void *pContext);

	static void LegacyCommandCallback(IConsole::IResult *pResult, void *pContext);
	void RegisterLegacyDDRaceCommands();

	// DDRace

	static void ConRandomMap(IConsole::IResult *pResult, void *pUserData);
	static void ConRandomUnfinishedMap(IConsole::IResult *pResult, void *pUserData);

	CGameContext(int Resetting);
	void Construct(int Resetting);

	bool m_Resetting;
public:
	IServer *Server() const { return m_pServer; }
	class CConfig *Config() { return m_pConfig; }
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
	CCommandManager m_CommandManager;

	CCommandManager *CommandManager() { return &m_CommandManager; }

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
		MIN_RACE_CLIENTVERSION = 0x0704,
	};
	class CHeap *m_pVoteOptionHeap;
	CVoteOptionServer *m_pVoteOptionFirst;
	CVoteOptionServer *m_pVoteOptionLast;

	// helper functions
	void CreateDamage(vec2 Pos, int Id, vec2 Source, int HealthAmount, int ArmorAmount, bool Self, Mask128 Mask = Mask128(), int SevendownAmount = 0);
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, Mask128 Mask = Mask128());
	void CreateHammerHit(vec2 Pos, Mask128 Mask = Mask128());
	void CreatePlayerSpawn(vec2 Pos, Mask128 Mask = Mask128());
	void CreateDeath(vec2 Pos, int Who, Mask128 Mask = Mask128());
	void CreateSound(vec2 Pos, int Sound, Mask128 Mask = Mask128());

	// network
	void SendChatMsg(CNetMsg_Sv_Chat *pMsg, int Flags, int To);
	void SendChatTarget(int To, const char* pText);
	void SendChatTeam(int Team, const char* pText);
	void SendChat(int ChatterClientID, int Mode, int To, const char *pText, int SpamProtectionClientID = -1);
	void SendBroadcast(const char* pText, int ClientID, bool IsImportant = true);
	void SendEmoticon(int ClientID, int Emoticon);
	void SendWeaponPickup(int ClientID, int Weapon);
	void SendSettings(int ClientID);
	void SendSkinChange(CTeeInfo TeeInfos, int ClientID, int TargetID);

	// DDRace
	void SendTeamChange(int ClientID, int Team, bool Silent, int CooldownTick, int ToClientID);

	void CallVote(int ClientID, const char *aDesc, const char *aCmd, const char *pReason, const char *aChatmsg);

	void List(int ClientID, const char* filter); // ClientID -1 prints to console, otherwise to chat

	void SendGameMsg(int GameMsgID, int ClientID);
	void SendGameMsg(int GameMsgID, int ParaI1, int ClientID);
	void SendGameMsg(int GameMsgID, int ParaI1, int ParaI2, int ParaI3, int ClientID);

	void SendChatCommand(const CCommandManager::CCommand *pCommand, int ClientID);
	void SendChatCommands(int ClientID);
	void SendRemoveChatCommand(const CCommandManager::CCommand *pCommand, int ClientID);
	void SendRemoveChatCommand(const char *pName, int ClientID);

	//
	void SendTuningParams(int ClientID, int Zone = 0);

	struct CVoteOptionServer *GetVoteOption(int Index);
	void ProgressVoteOptions(int ClientID);

	void LoadMapSettings();

	// engine events
	virtual void OnInit();
	virtual void OnConsoleInit();
	virtual void OnMapChange(char* pNewMapName, int MapNameSize);
	virtual void OnShutdown(bool FullShutdown = false);
	virtual void OnPreShutdown();

	virtual void OnTick();
	virtual void OnPreSnap();
	virtual void OnSnap(int ClientID);
	virtual void OnPostSnap();

	void *PreProcessMsg(int MsgID, CUnpacker *pUnpacker, int ClientID);
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

	virtual bool IsClientBot(int ClientID) const;
	virtual bool IsClientReady(int ClientID) const;
	virtual bool IsClientPlayer(int ClientID) const;
	virtual bool IsClientSpectator(int ClientID) const;

	virtual const CUuid GameUuid() const;
	virtual const char *GameType() const;
	virtual const char *Version() const;
	virtual const char *VersionSevendown() const;
	virtual const char *NetVersion() const;
	virtual const char *NetVersionSevendown() const;

	void OnClientDDNetVersionKnown(int ClientID);
	int GetClientDDNetVersion(int ClientID);
	int ProcessSpamProtection(int ClientID);
	int GetDDRaceTeam(int ClientID);
	int64 m_NonEmptySince;
	int64 m_LastMapVote;
	void ForceVote(int EnforcerID, bool Success);

	// F-DDrace

	//dummy
	void ConnectDummy(int DummyMode = 0, vec2 Pos = vec2(-1, -1));
	void ConnectDefaultDummies();

	bool IsHouseDummy(int ClientID, int Type = -1);
	int GetHouseDummy(int Type = -1);
	void ConnectHouseDummy(int Type);

	int GetNextClientID();

	void UpdateHidePlayers(int UpdateID = -1);

	// plots
	void InitPlots();
	void ReadPlotStats(int ID);
	void WritePlotStats(int ID);
	void SetPlotInfo(int PlotID, int AccID);
	void SetPlotExpire(int PlotID);

	unsigned int GetMaxPlotObjects(int PlotID);
	const char *GetPlotSizeString(int PlotID);

	void SetExpireDate(time_t *pDate, float Days);
	bool IsExpired(time_t Date);

	struct SPlot
	{
		char m_aOwner[32];
		char m_aDisplayName[32];
		time_t m_ExpireDate;

		int m_Size;
		vec2 m_ToTele;
		std::vector<CEntity *> m_vObjects;
	} m_aPlots[MAX_PLOTS];

	enum PlotVariables
	{
		PLOT_OWNER_ACC_USERNAME,
		PLOT_DISPLAY_NAME,
		PLOT_EXPIRE_DATE,
		NUM_PLOT_VARIABLES
	};

	void SetPlotDoorStatus(int PlotID, bool Close);
	void ClearPlot(int PlotID);
	int GetPlotID(int AccID);
	void ExpirePlots();
	int GetTilePlotID(vec2 Pos);

	int m_FullHourOffsetTicks;
	bool IsFullHour() { return Server()->Tick() % (Server()->TickSpeed() * 60 * 60) == m_FullHourOffsetTicks; }
	bool HasPlotByIP(int ClientID);

	bool IntersectedLineDoor(vec2 Pos0, vec2 Pos1, int Team, bool PlotDoorOnly);
	void RemovePortalsFromPlot(int PlotID);

	//account
	int GetAccIDByUsername(const char *pUsername);
	int GetAccount(const char* pUsername);
	void FreeAccount(int ID);

	const char *GetAccVarName(int VariableID);
	const char *GetAccVarValue(int ID, int VariableID);
	void SetAccVar(int ID, int VariableID, const char *pData);

	struct TopAccounts
	{
		int m_Level;
		int m_Points;
		int64 m_Money;
		int m_KillStreak;
		char m_aUsername[32];
		char m_aAccountName[32];
	};
	std::vector<TopAccounts> m_TopAccounts;
	void UpdateTopAccounts(int Type);
	void SetTopAccStats(int FromID);

	int m_LogoutAccountsPort;
	static int InitAccounts(const char* pName, int IsDir, int StorageType, void* pUser);
	int AddAccount();
	void ReadAccountStats(int ID, const char* pName);
	void WriteAccountStats(int ID);
	void Logout(int ID, bool Silent = false);
	void LogoutAllAccounts();
	bool Login(int ClientID, const char *pUsername, const char *pPassword, bool PasswordRequired = true);

	int m_aTaserPrice[NUM_TASER_LEVELS];
	int m_aPoliceLevel[NUM_POLICE_LEVELS];
	int64 m_aNeededXP[DIFFERENCE_XP_END];
	int64 GetNeededXP(int Level);
	int m_LastAccSaveTick;

	const char *GetDate(time_t Time, bool ShowTime = true);
	void WriteDonationFile(int Type, int Amount, int ID, const char *pDescription);

	struct AccountInfo
	{
		int m_Port;
		bool m_LoggedIn;
		bool m_Disabled;
		char m_Password[MAX_PASSWORD_LENGTH];
		char m_Username[32];
		int m_ClientID;
		int m_Level;
		int64 m_XP;
		int64 m_Money;
		int m_Kills;
		int m_Deaths;
		int m_PoliceLevel;
		int m_SurvivalKills;
		int m_SurvivalWins;
		bool m_SpookyGhost;
		char m_aLastMoneyTransaction[5][128];
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
		int m_Euros;
		time_t m_ExpireDateVIP;
		bool m_PortalRifle;
		time_t m_ExpireDatePortalRifle;
		int m_Version;
		NETADDR m_Addr;
		NETADDR m_LastAddr;
		int m_TaserBattery;
		char m_aContact[128];
		char m_aTimeoutCode[64];
		char m_aSecurityPin[5];
	};
	std::vector<AccountInfo> m_Accounts;

	// make sure these are in the same order as the variables above
	// if you add another variable make sure to change the ACC_CURRENT_VERSION in this file
	enum AccountVariables
	{
		ACC_PORT,
		ACC_LOGGED_IN,
		ACC_DISABLED,
		ACC_PASSWORD,
		ACC_USERNAME,
		ACC_CLIENT_ID,
		ACC_LEVEL,
		ACC_XP,
		ACC_MONEY,
		ACC_KILLS,
		ACC_DEATHS,
		ACC_POLICE_LEVEL,
		ACC_SURVIVAL_KILLS,
		ACC_SURVIVAL_WINS,
		ACC_SPOOKY_GHOST,
		ACC_LAST_MONEY_TRANSACTION_0,
		ACC_LAST_MONEY_TRANSACTION_1,
		ACC_LAST_MONEY_TRANSACTION_2,
		ACC_LAST_MONEY_TRANSACTION_3,
		ACC_LAST_MONEY_TRANSACTION_4,
		ACC_VIP,
		ACC_BLOCK_POINTS,
		ACC_INSTAGIB_KILLS,
		ACC_INSTAGIB_WINS,
		ACC_SPAWN_WEAPON_0,
		ACC_SPAWN_WEAPON_1,
		ACC_SPAWN_WEAPON_2,
		ACC_NINJAJETPACK,
		ACC_LAST_PLAYER_NAME,
		ACC_SURVIVAL_DEATHS,
		ACC_INSTAGIB_DEATHS,
		ACC_TASER_LEVEL,
		ACC_KILLING_SPREE_RECORD,
		ACC_EUROS,
		ACC_EXPIRE_DATE_VIP,
		ACC_PORTAL_RIFLE,
		ACC_EXPIRE_DATE_PORTAL_RIFLE,
		ACC_VERSION,
		ACC_ADDR,
		ACC_LAST_ADDR,
		ACC_TASER_BATTERY,
		ACC_CONTACT,
		ACC_TIMEOUT_CODE,
		ACC_SECURITY_PIN,
		NUM_ACCOUNT_VARIABLES
	};

	// money drops
	void WriteMoneyListFile();
	void ReadMoneyListFile();
	void SaveOrDropWallet();

	//motd
	const char *FormatMotd(const char *pMsg);
	const char *AppendMotdFooter(const char *pMsg, const char *pFooter);

	//acc broadcast
	const char* FormatExperienceBroadcast(const char* pMsg, int ClientID);

	//extras
	void SendExtraMessage(int Extra, int ToID, bool Set, int FromID, bool Silent, int Special = 0);
	const char* CreateExtraMessage(int Extra, bool Set, int FromID, int ToID, int Special);
	const char* GetExtraName(int Extra, int Special = 0);

	bool IsValidHookPower(int HookPower);

	//others
	int GetCIDByName(const char* pName);
	void SendMotd(const char* pMsg, int ClientID);

	const char* GetWeaponName(int Weapon);
	int GetWeaponType(int Weapon);
	int GetProjectileType(int Weapon);
	int GetPickupType(int Type, int Subtype);

	const char* GetMinigameName(int Minigame);
	const char* GetMinigameCommand(int Minigame);

	int CountConnectedPlayers(bool CountSpectators = true, bool ExcludeBots = false);

	void CreateLaserText(vec2 Pos, int Owner, const char* pText, int Seconds = 3);
	int MoneyLaserTextTime(int64 Amount) { return Amount < SMALL_MONEY_AMOUNT ? 1 : 3; }

	class CHouse *m_pHouses[NUM_HOUSES];

	void CreateSoundGlobal(int Sound);
	void CreateSoundPlayer(int Sound, int ClientID);
	void CreateSoundPlayerAt(vec2 Pos, int Sound, int ClientID);

	void UnsetTelekinesis(CEntity *pEntity);
	void UnsetKiller(int ClientID);

	bool SameIP(int ClientID1, int ClientID2);
	bool SameIP(int AccID, const NETADDR *pAddr);

	bool FlagsUsed();
	void CreateFolders();

	bool IsLocal(int CientID1, int ClientID2);
	bool CanReceiveMessage(int Sender, int Receiver);

	//pickup drops
	std::vector<CPickupDrop*> m_vPickupDropLimit;

	//minigames disabled
	bool m_aMinigameDisabled[NUM_MINIGAMES];

	void SetMinigame(int ClientID, int Minigame, bool Force = false);

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

	//isdummy callback for console victim dummy
	static void ConsoleIsDummyCallback(int ClientID, bool *pIsDummy, void *pUser);

	// shutdown tee
	struct
	{
		int m_ClientID;
		bool m_Got;
		char m_aUsername[32];
	} m_ShutdownSave;

	void CheckShutdownSaved(int ClientID);
	static int CheckShutdownSavedCallback(const char *pName, int IsDir, int StorageType, void *pUser);
	void SwapAddrSeparator(char *pAddrStr);
	static int RemoveShutdownSaves(const char *pName, int IsDir, int StorageType, void *pUser);

	// this function is used for shutdown tee but also for save drop
	void SaveCharacter(int ClientID);

	//
	virtual void OnSetTimedOut(int ClientID, int OrigID);

	// map specific
	void SetMapSpecificOptions();

	// police
	void SendChatPolice(const char *pMessage);
	void JailPlayer(int ClientID, int Seconds);

	// gangster
	void ProcessSpawnBlockProtection(int ClientID);
	bool IsSpawnArea(vec2 Pos);

	// saved
	void SavePlayer(int ClientID);
	void CheckLoadPlayer(int ClientID);

	struct SavedPlayer
	{
		char m_aUsername[32];
		NETADDR m_Addr;
		char m_aTimeoutCode[64];

		char m_aName[MAX_NAME_LENGTH];
		CTeeInfo m_TeeInfo;

		// value that can get saved and restored
		int64 m_JailTime;
		int64 m_EscapeTime;
	};
	std::vector<SavedPlayer> m_vSavedPlayers;

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
	static void ConPractice(IConsole::IResult *pResult, void *pUserData);

	static void ConTimeout(IConsole::IResult *pResult, void *pUserData);
	static void ConSave(IConsole::IResult* pResult, void* pUserData);
	static void ConLoad(IConsole::IResult* pResult, void* pUserData);

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
	static void ConContact(IConsole::IResult* pResult, void* pUserData);
	static void ConPin(IConsole::IResult* pResult, void* pUserData);

	static void ConPayMoney(IConsole::IResult* pResult, void* pUserData);
	static void ConMoney(IConsole::IResult* pResult, void* pUserData);

	static void ConRoom(IConsole::IResult* pResult, void* pUserData);
	static void ConSpawn(IConsole::IResult* pResult, void* pUserData);
	static void ConSilentFarm(IConsole::IResult* pResult, void* pUserData);

	void PreSetMinigame(IConsole::IResult *pResult, void *pUserData, int Minigame);
	static void ConMinigames(IConsole::IResult* pResult, void* pUserData);
	static void ConLeaveMinigame(IConsole::IResult* pResult, void* pUserData);
	static void ConJoinBlock(IConsole::IResult* pResult, void* pUserData);
	static void ConJoinSurvival(IConsole::IResult* pResult, void* pUserData);
	static void ConJoinBoomFNG(IConsole::IResult* pResult, void* pUserData);
	static void ConJoinFNG(IConsole::IResult* pResult, void* pUserData);

	static void ConResumeMoved(IConsole::IResult* pResult, void* pUserData);
	static void ConWeapon(IConsole::IResult* pResult, void* pUserData);

	static void ConStats(IConsole::IResult* pResult, void* pUserData);
	static void ConAccount(IConsole::IResult* pResult, void* pUserData);

	void SendTop5AccMessage(IConsole::IResult* pResult, void* pUserData, int Type);
	static void ConTop5Level(IConsole::IResult* pResult, void* pUserData);
	static void ConTop5Points(IConsole::IResult* pResult, void* pUserData);
	static void ConTop5Money(IConsole::IResult* pResult, void* pUserData);
	static void ConTop5Spree(IConsole::IResult* pResult, void* pUserData);

	static void ConPoliceHelper(IConsole::IResult* pResult, void* pUserData);

	static void ConRainbowVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConBloodyVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConAtomVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConTrailVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConSpreadGunVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConSpinBotVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConAimClosestVIP(IConsole::IResult* pResult, void* pUserData);

	static void ConPlot(IConsole::IResult* pResult, void* pUserData);

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
	static void ConPortalRifle(IConsole::IResult* pResult, void* pUserData);
	static void ConUnPortalRifle(IConsole::IResult* pResult, void* pUserData);
	static void ConProjectileRifle(IConsole::IResult* pResult, void* pUserData);
	static void ConUnProjectileRifle(IConsole::IResult* pResult, void* pUserData);
	static void ConBallGrenade(IConsole::IResult* pResult, void* pUserData);
	static void ConUnBallGrenade(IConsole::IResult* pResult, void* pUserData);

	static void ConHammer(IConsole::IResult* pResult, void* pUserData);
	static void ConUnHammer(IConsole::IResult* pResult, void* pUserData);
	static void ConGun(IConsole::IResult* pResult, void* pUserData);
	static void ConUnGun(IConsole::IResult* pResult, void* pUserData);

	static void ConDrawEditor(IConsole::IResult* pResult, void* pUserData);
	static void ConUnDrawEditor(IConsole::IResult* pResult, void* pUserData);

	static void ConScrollNinja(IConsole::IResult* pResult, void* pUserData);

	static void ConSound(IConsole::IResult* pResult, void* pUserData);
	static void ConLaserText(IConsole::IResult* pResult, void* pUserData);
	static void ConSendMotd(IConsole::IResult* pResult, void* pUserData);

	static void ConConnectDummy(IConsole::IResult* pResult, void* pUserData);
	static void ConDisconnectDummy(IConsole::IResult* pResult, void* pUserData);
	static void ConDummymode(IConsole::IResult* pResult, void* pUserData);
	static void ConConnectDefaultDummies(IConsole::IResult* pResult, void* pUserData);

	static void ConForceFlagOwner(IConsole::IResult* pResult, void* pUserData);

	static void ConPlayerName(IConsole::IResult* pResult, void* pUserData);
	static void ConPlayerClan(IConsole::IResult* pResult, void* pUserData);
	static void ConPlayerSkin(IConsole::IResult* pResult, void* pUserData);

	static void ConPlayerInfo(IConsole::IResult* pResult, void* pUserData);
	static void ConListRcon(IConsole::IResult* pResult, void* pUserData);

	static void ConItem(IConsole::IResult* pResult, void* pUserData);
	static void ConInvisible(IConsole::IResult* pResult, void* pUserData);
	static void ConHookPower(IConsole::IResult* pResult, void* pUserData);

	static void ConSetJumps(IConsole::IResult* pResult, void* pUserData);
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

	static void ConAccLogoutPort(IConsole::IResult* pResult, void* pUserData);
	static void ConAccLogout(IConsole::IResult* pResult, void* pUserData);
	static void ConAccDisable(IConsole::IResult* pResult, void* pUserData);
	static void ConAccInfo(IConsole::IResult* pResult, void* pUserData);
	static void ConAccAddEuros(IConsole::IResult* pResult, void* pUserData);
	static void ConAccEdit(IConsole::IResult* pResult, void* pUserData);

	static void ConSayBy(IConsole::IResult* pResult, void* pUserData);
	static void ConTeeControl(IConsole::IResult* pResult, void* pUserData);
	static void ConSetMinigame(IConsole::IResult* pResult, void* pUserData);

	static void ConToTelePlot(IConsole::IResult* pResult, void* pUserData);
	static void ConClearPlot(IConsole::IResult* pResult, void* pUserData);
	static void ConPlotOwner(IConsole::IResult* pResult, void* pUserData);
	static void ConPlotInfo(IConsole::IResult* pResult, void* pUserData);

	enum
	{
		MAX_MUTES = 32,
		MAX_VOTE_MUTES = 32,

		MAX_REGISTER_BANS = 256,
		REGISTER_BAN_DELAY = 60 * 60 * 12, // 12 hours
	};
	struct CMute
	{
		NETADDR m_Addr;
		int m_Expire;
		char m_aReason[128];
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
	bool TryMute(const NETADDR *pAddr, int Secs, const char *pReason);
	void Mute(const NETADDR *pAddr, int Secs, const char *pDisplayName, const char *pReason = "");
	bool TryVoteMute(const NETADDR* pAddr, int Secs);
	bool VoteMute(const NETADDR* pAddr, int Secs, const char* pDisplayName, int AuthedID);
	bool VoteUnmute(const NETADDR* pAddr, const char* pDisplayName, int AuthedID);

	const char *GetWhisper(char *pStr, int *pTarget);
	bool IsVersionBanned(int Version);

	struct CRegisterBan
	{
		NETADDR m_Addr;
		int m_Expire;
		int m_NumRegistrations;
		int64 m_LastAttempt;
	};

	CRegisterBan m_aRegisterBans[MAX_REGISTER_BANS];
	int m_NumRegisterBans;
	int ProcessRegisterBan(int ClientID);
	bool TryRegisterBan(const NETADDR *pAddr, int Secs);

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

inline Mask128 CmaskAll() { return Mask128(); }
inline Mask128 CmaskNone() { return Mask128(-1); }
inline Mask128 CmaskOne(int ClientID) { return Mask128(ClientID); }
inline Mask128 CmaskUnset(Mask128 Mask, int ClientID) { return Mask^CmaskOne(ClientID); }
inline Mask128 CmaskAllExceptOne(int ClientID) { return CmaskUnset(CmaskAll(), ClientID); }
inline bool CmaskIsSet(Mask128 Mask, int ClientID) { return (Mask&CmaskOne(ClientID)) != 0; }
#endif
