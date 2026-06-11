/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTEXT_H
#define GAME_SERVER_GAMECONTEXT_H

#include <engine/antibot.h>
#include <engine/console.h>
#include <engine/server.h>

#include <game/commands.h>
#include <game/layers.h>
#include <game/voting.h>

#include <vector>
#include <string>
#include "entities/interactive/pickup_drop.h"
#include "entities/interactive/money.h"
#include "entities/map/draweditor/drawtile.h"
#include "houses/house.h"
#include "minigames/minigame.h"
#include "minigames/arenas.h"
#include "minigames/durak.h"
#include "minigames/survival.h"

#include "eventhandler.h"
#include "gameworld.h"
#include "misc/whois.h"
#include "misc/rainbowname.h"
#include "misc/votingmenu.h"
#include "misc/plots.h"
#include "misc/accounts.h"
#include "misc/playermapping.h"
#include "misc/savedtees.h"

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

#include "misc/mask128.h"

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

enum
{
	// motd
	MOTD_MAX_LINES = 24,
};


class CRandomMapResult;
class CMapVoteResult;
struct CAntibotData;

struct CSnapContext
{
	CSnapContext(int Version, bool Sevendown, int ClientId) :
		m_ClientVersion(Version), m_Sevendown(Sevendown), m_ClientId(ClientId)
	{
	}

	int GetClientVersion() const { return m_ClientVersion; }
	bool IsSevendown() const { return m_Sevendown; }
	bool ClientId() const { return m_ClientId; }

private:
	int m_ClientVersion;
	bool m_Sevendown;
	int m_ClientId;
};

class CGameContext : public IGameServer
{
	IServer *m_pServer;
	class CConfig *m_pConfig;
	class IConsole *m_pConsole;
	IStorage* m_pStorage;
	IAntibot *m_pAntibot;
	CLayers m_Layers;
	CCollision m_Collision;
	CNetObjHandler m_NetObjHandler;
	CTuningParams m_Tuning;
	CTuningParams m_aTuningList[TuneZone::NUM];
	LOCKED_TUNES m_vLockedTuning[TuneZone::NUM];

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
	static void ConTuneLock(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneLockDump(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneLockReset(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneLockSetMsgEnter(IConsole::IResult *pResult, void *pUserData);
	static void ConSwitchOpen(IConsole::IResult* pResult, void* pUserData);
	static void ConPause(IConsole::IResult* pResult, void* pUserData);	static void ConChangeMap(IConsole::IResult *pResult, void *pUserData);
	static void ConRestart(IConsole::IResult *pResult, void *pUserData);
	static void ConServerAlert(IConsole::IResult *pResult, void *pUserData);
	static void ConModAlert(IConsole::IResult *pResult, void *pUserData);
	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConBroadcast(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTeamAll(IConsole::IResult *pResult, void *pUserData);
	static void ConForceVote(IConsole::IResult *pResult, void *pUserData);
	static void ConAddVote(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveVote(IConsole::IResult *pResult, void *pUserData);
	static void ConClearVotes(IConsole::IResult *pResult, void *pUserData);
	static void ConVote(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpAntibot(IConsole::IResult *pResult, void *pUserData);
	static void ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainSettingUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainGameinfoUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	static void ConchainUpdateHidePlayers(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData);
	static void ConchainUpdateLocalChat(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData);
	static void ConchainUpdateBankMode(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData);

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
	CTuningParams *TuningFromChrOrZone(int ClientID, int Zone = -1);
	CTuningParams* TuningList() { return &m_aTuningList[0]; }
	IAntibot *Antibot() { return m_pAntibot; }

	LOCKED_TUNES *LockedTuning() { return &m_vLockedTuning[0]; }
	bool IsTuneInList(LOCKED_TUNES *pLockedTunings, const char *pParam) { return std::any_of(pLockedTunings->begin(), pLockedTunings->end(),
			[pParam](const CLockedTune &Tune) {
				return str_comp(Tune.m_aParam, pParam) == 0;
			}); }
	int SetLockedTune(LOCKED_TUNES *pLockedTunings, CLockedTune &Tune, bool AllowGlobalValues = false);
	void ApplyTuneLock(LOCKED_TUNES *pLockedTunings, int TuneLock);
	CTuningParams *ApplyLockedTunings(CTuningParams *pTuning, LOCKED_TUNES &LockedTunings);

	CGameContext();
	~CGameContext();

	void Clear();

	CEventHandler m_Events;
	class CPlayer *m_apPlayers[MAX_CLIENTS];

	// keep last input to always apply when none is sent
	CNetObj_PlayerInput m_aLastPlayerInput[MAX_CLIENTS];
	bool m_aPlayerHasInput[MAX_CLIENTS];

	class IGameController *m_pController;
	CGameWorld m_World;
	CCommandManager m_CommandManager;

	CCommandManager *CommandManager() { return &m_CommandManager; }

	// helper functions
	class CCharacter *GetPlayerChar(int ClientID);

	int m_LockTeams;

	// voting
	void StartVote(const char *pDesc, const char *pCommand, const char *pReason, const char *pSevendownDesc);
	void EndVote(int Type, bool Force);
	void ForceVote(int Type, const char *pDescription, const char *pReason);
	void SendVoteSet(int Type, int ClientID);
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
	char m_aSevendownVoteDescription[VOTE_DESC_LENGTH];
	char m_aVoteCommand[VOTE_CMD_LENGTH];
	char m_aVoteReason[VOTE_REASON_LENGTH];
	int m_VoteClientID;
	int m_NumVoteOptions;
	int m_VoteEnforce;
	char m_aaZoneEnterMsg[TuneZone::NUM][256]; // 0 is used for switching from or to area without tunings
	char m_aaZoneLeaveMsg[TuneZone::NUM][256];
	char m_aaTuneLockMsg[TuneZone::NUM][256];

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
	void AddVote(const char *pDescription, const char *pCommand);

	// helper functions
	void CreateDamage(vec2 Pos, int Id, vec2 Source, int HealthAmount, int ArmorAmount, bool Self, Mask128 Mask = Mask128(), int SevendownAmount = 0);
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, Mask128 Mask = Mask128());
	void CreateHammerHit(vec2 Pos, Mask128 Mask = Mask128());
	void CreatePlayerSpawn(vec2 Pos, Mask128 Mask = Mask128());
	void CreateDeath(vec2 Pos, int Who, Mask128 Mask = Mask128());
	void CreateFinishConfetti(vec2 Pos, Mask128 Mask = Mask128());
	void CreateSound(vec2 Pos, int Sound, Mask128 Mask = Mask128());

	bool SnapLaserObject(const CSnapContext &Context, int SnapId, const vec2 &To, const vec2 &From, int StartTick, int Owner = -1, int LaserType = -1, int Subtype = -1, int SwitchNumber = -1, int Flags = 0) const;
	bool SnapPickup(const CSnapContext &Context, int SnapId, const vec2 &Pos, int Type, int SubType = 0, int SwitchNumber = -1, int Flags = 0, int Special = 0, int aExtraIds[4] = { 0 }) const;
	bool SnapPickupObject(const CSnapContext &Context, int SnapId, const vec2 &Pos, int Type, int SubType = 0, int SwitchNumber = -1, int Flags = 0) const;

	enum
	{
		CHAT_SEVEN = 1<<0,
		CHAT_SEVENDOWN = 1<<1,
		CHAT_NO_WEBHOOK = 1<<2,

		CHATFLAG_ALL = CHAT_SEVEN|CHAT_SEVENDOWN,
	};

	// network
	void SendChatMsg(CNetMsg_Sv_Chat *pMsg, int Flags, int To);
	void SendChatTarget(int To, const char *pText, int Flags = CHATFLAG_ALL);
	void SendChatTeam(int Team, const char *pText, CFormatArg *pArgs = 0, int NumArgs = 0);
	void SendChatMessage(int ChatterClientID, int Mode, int To, const char *pText) override { SendChat(ChatterClientID, Mode, To, pText); }
	bool SendChat(int ChatterClientID, int Mode, int To, const char *pText, int SpamProtectionClientID = -1, int Flags = CHATFLAG_ALL, CFormatArg *pArgs = 0, int NumArgs = 0);

	template<typename... Args>
	void SendChatTeamFormat(int Team, const char *pFormat, Args&&... args)
	{
		CFormatArg aArgs[] = { CFormatArg(std::forward<Args>(args))... };
		SendChatTeam(Team, pFormat, aArgs, std::size(aArgs));
	}

	template<typename... Args>
	void SendChatFormat(int ChatterClientID, int Mode, int To, int Flags, const char* pFormat, Args&&... args)
	{
		CFormatArg aArgs[] = { CFormatArg(std::forward<Args>(args))... };
		SendChat(ChatterClientID, Mode, To, pFormat, -1, Flags, aArgs, std::size(aArgs));
	}

	void SendBroadcast(const char* pText, int ClientID, bool IsImportant = true, CFormatArg *pArgs = 0, int NumArgs = 0);
	template<typename... Args>
	void SendBroadcastFormat(int ClientID, bool IsImportant, const char *pFormat, Args&&... args)
	{
		CFormatArg aArgs[] = { CFormatArg(std::forward<Args>(args))... };
		SendBroadcast(pFormat, ClientID, IsImportant, aArgs, std::size(aArgs));
	}
	void SendEmoticon(int ClientID, int Emoticon);
	void SendWeaponPickup(int ClientID, int Weapon);
	void SendSettings(int ClientID);
	void SendSkinChange(CTeeInfo TeeInfos, int ClientID, int TargetID);

	void SendServerAlert(const char *pMessage);
	void SendModeratorAlert(const char *pMessage, int ToClientId);

	// DDRace
	void SendTeamChange(int ClientID, int Team, bool Silent, int CooldownTick, int ToClientID);

	void CallVote(int ClientID, const char *aDesc, const char *aCmd, const char *pReason, const char *aChatmsg, const char *pSevendownDesc, CFormatArg *pArgs = 0, int NumArgs = 0);

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
	void StartResendingVotes(int ClientID, bool ResendVotesPage = true);

	void LoadMapSettings();

	// engine events
	void OnInit() override;
	void OnConsoleInit() override;
	void OnShutdown(bool FullShutdown = false) override;
	void OnPreShutdown() override;

	void OnTick() override;
	void OnPreSnap() override;
	void OnSnap(int ClientID) override;
	void OnPostSnap() override;

	void *PreProcessMsg(int MsgID, CUnpacker *pUnpacker, int ClientID);
	void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID) override;

	void OnClientConnected(int ClientID, bool AsSpec) override { OnClientConnected(ClientID, false, AsSpec); }
	void OnClientConnected(int ClientID, bool Dummy, bool AsSpec);
	void OnClientTeamChange(int ClientID);
	void OnClientEnter(int ClientID) override;
	void OnClientDrop(int ClientID, const char *pReason) override;
	void OnClientAuth(int ClientID, int Level) override;
	void OnClientDirectInput(int ClientID, void *pInput) override;
	void OnClientPredictedInput(int ClientID, void *pInput) override;
	void OnClientPredictedEarlyInput(int ClientID, void *pInput) override;
	void OnClientRejoin(int ClientID) override;

	void PreInputClients(int ClientId, bool *pClients) override;

	void OnClientEngineJoin(int ClientID) override;
	void OnClientEngineDrop(int ClientID, const char *pReason) override;

	bool IsClientBot(int ClientID) const override;
	bool IsClientReady(int ClientID) const override;
	bool IsClientPlayer(int ClientID) const override;
	bool IsClientSpectator(int ClientID) const override;

	const CUuid GameUuid() const override;
	const char *GameType() const override;
	const char *Version() const override;
	const char *VersionSevendown() const override;
	const char *NetVersion() const override;
	const char *NetVersionSevendown() const override;

	void OnCountryCodeLookup(int ClientID) override;
	void SetBotDetected(int ClientID) override;
	void FillAntibot(CAntibotRoundData *pData) override;
	bool OnClientDDNetVersionKnown(int ClientID);
	int GetClientDDNetVersion(int ClientID);
	Mask128 ClientsMaskExcludeClientVersionAndHigher(int Version);
	int ProcessSpamProtection(int ClientID);
	int GetDDRaceTeam(int ClientID);
	int64 m_NonEmptySince;
	int64 m_LastMapVote;

	// Checks if player can vote and notify them about the reason
	bool RateLimitPlayerVote(int ClientID);
	bool RateLimitPlayerMapVote(int ClientID);

	void OnUpdatePlayerServerInfo(char *aBuf, int BufSize, int ID) override;

	// F-DDrace

	//dummy
	void ConnectDummy(int DummyMode = 0, vec2 Pos = vec2(-1, -1));
	void ConnectDefaultDummies();

	bool IsHouseDummy(int ClientID, int Type = -1);
	int GetHouseDummy(int Type = -1);
	void ConnectHouseDummy(int Type, bool SpawnTileOnly = false);

	int GetNextClientID();

	void UpdateHidePlayers(int UpdateID = -1);

	CPlayerMapping m_PlayerMapping;
	CPlots m_Plots;
	CAccounts m_Accounts;
	CSavedTees m_SavedTees;

	const char *GetDate(time_t Time, bool ShowTime = true);
	void SetExpireDateDays(time_t *pDate, float Days);
	void SetExpireDate(time_t *pDate, float Hours, bool SetMinutesZero = false);
	bool IsExpired(time_t Date);

	int m_FullHourOffsetTicks;
	bool IsFullHour() { return Server()->Tick() % (Server()->TickSpeed() * 60 * 60) == m_FullHourOffsetTicks; }

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
	int GetWeaponType(int Weapon) const;
	int GetProjectileType(int Weapon) const;
	int GetPickupType(int Type, int Subtype) const;
	bool IsValidSpreadWeapon(int Type);

	const char *GetScoreModeName(int ScoreMode);
	const char *GetScoreModeCommand(int ScoreMode);

	const char* GetMinigameName(int Minigame);
	const char* GetMinigameCommand(int Minigame);

	int CountConnectedPlayers(bool CountSpectators = true, bool ExcludeBots = false);

	class CHouse *m_pHouses[NUM_HOUSES];
	class CMinigame *m_pMinigames[NUM_MINIGAMES];
	CArenas *Arenas() { return ((CArenas *)m_pMinigames[MINIGAME_1VS1]); }
	CDurak *Durak() { return ((CDurak *)m_pMinigames[MINIGAME_DURAK]); }
	CSurvival *Survival() { return ((CSurvival *)m_pMinigames[MINIGAME_SURVIVAL]); }
	CMinigame *BlockMg() { return m_pMinigames[MINIGAME_BLOCK]; }
	CWhoIs m_WhoIs;
	CRainbowName m_RainbowName;
	CVotingMenu m_VotingMenu;

	void CreateSoundGlobal(int Sound);
	void CreateSoundPlayer(int Sound, int ClientID);
	void CreateSoundPlayerAt(vec2 Pos, int Sound, int ClientID);

	void CreateFolders();

	bool IsLocal(int CientID1, int ClientID2);
	bool CanReceiveMessage(int Sender, int Receiver);
	bool IsMuted(int Sender, int Receiver);
	bool LineShouldHighlight(const char *pLine, const char *pName);
	
	void CalcScreenParams(float Aspect, float Zoom, float *w, float *h);

	void MapDesignChangeDone(int ClientID) override;
	void SendStartMessages(int ClientID);

	const char *FormatURL(const char *pURL);
	const char *GetAvatarURL(int ClientID);

	void SnapSelectedArea(CSelectedArea *pSelectedArea, const CSnapContext &Context);

	enum
	{
		MODLOG_ID_SERVER = -2,
	};
	void SendModLogMessage(int ClientID, const char *pMsg, bool IsAuth = false) override;

	//pickup drops
	std::vector<CPickupDrop*> m_vPickupDropLimit;

	//minigames disabled
	bool m_aMinigameDisabled[NUM_MINIGAMES];

	void SetMinigame(int ClientID, int Minigame, bool Force = false, bool DoChatMsg = true);

	//instagib
	void InstagibTick(int Type);

	//isdummy callback for console victim dummy
	static void ConsoleIsDummyCallback(int ClientID, bool *pIsDummy, void *pUser);
	static void ConsoleIsInViewCallback(int ClientID, int CallerID, bool *pIsInView, void *pUser);

	//
	void OnSetTimedOut(int ClientID, int OrigID) override;

	// map specific
	void SetMapSpecificOptions();

	// police
	template<typename... Args>
	void SendChatPoliceFormat(const char *pFormat, Args&&... args)
	{
		CFormatArg aArgs[] = { CFormatArg(std::forward<Args>(args))... };
		SendChat(-1, CHAT_POLICE_CHANNEL, -1, pFormat, -1, CHATFLAG_ALL, aArgs, std::size(aArgs));
	}
	void SendChatPolice(const char *pMessage);
	bool JailPlayer(int ClientID, int Seconds, int ModLogID = -1);
	bool ForceJailRelease(int ClientID);

	// gangster
	void ProcessSpawnBlockProtection(int ClientID);
	bool IsSpawnArea(vec2 Pos);
	
	// Redirect tile
	void OnRedirectSaveTeeAdd(const char *pHash) override;
	void OnRedirectSaveTeeRemove(const char *pHash) override;
	int GetRedirectListPort(int WantedSwitchNumber);
	int GetRedirectListSwitch(int WantedPort);

	void OnPlayerCountUpdate(int Port, int PlayerCount) override;
	void SendPlayerCountUpdate(bool Shutdown = false);
	int64 m_LastPlayerCountUpdate;

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
	static void ConZoomCursor(IConsole::IResult* pResult, void* pUserData);

	static void ConHelpToggle(IConsole::IResult* pResult, void* pUserData);
	static void ConVIPInfo(IConsole::IResult* pResult, void* pUserData);
	static void ConPoliceInfo(IConsole::IResult* pResult, void* pUserData);
	static void ConSpawnWeaponsInfo(IConsole::IResult* pResult, void* pUserData);
	static void ConTaserInfo(IConsole::IResult* pResult, void* pUserData);

	static void ConLogin(IConsole::IResult* pResult, void* pUserData);
	static void ConLogout(IConsole::IResult* pResult, void* pUserData);
	static void ConRegister(IConsole::IResult* pResult, void* pUserData);
	static void ConChangePassword(IConsole::IResult* pResult, void* pUserData);
	static void ConContact(IConsole::IResult* pResult, void* pUserData);
	static void ConPin(IConsole::IResult* pResult, void* pUserData);

	static void ConPayMoney(IConsole::IResult* pResult, void* pUserData);
	static void ConMoney(IConsole::IResult* pResult, void* pUserData);
	static void ConPortal(IConsole::IResult* pResult, void* pUserData);

	static void ConRoom(IConsole::IResult* pResult, void* pUserData);
	static void ConSpawn(IConsole::IResult* pResult, void* pUserData);
	static void ConSilentFarm(IConsole::IResult* pResult, void* pUserData);

	static void ConMinigames(IConsole::IResult* pResult, void* pUserData);
	static void ConLeaveMinigame(IConsole::IResult* pResult, void* pUserData);
	static void ConJoinBlock(IConsole::IResult* pResult, void* pUserData);
	static void ConJoinSurvival(IConsole::IResult* pResult, void* pUserData);
	static void ConJoinBoomFNG(IConsole::IResult* pResult, void* pUserData);
	static void ConJoinFNG(IConsole::IResult* pResult, void* pUserData);
	static void Con1VS1(IConsole::IResult* pResult, void* pUserData);
	static void ConJoinDurak(IConsole::IResult* pResult, void* pUserData);

	static void ConResumeMoved(IConsole::IResult* pResult, void* pUserData);
	static void ConMutePlayer(IConsole::IResult* pResult, void* pUserData);
	static void ConDesign(IConsole::IResult* pResult, void* pUserData);
	static void ConChatLanguage(IConsole::IResult* pResult, void* pUserData);
	static void ConLanguage(IConsole::IResult* pResult, void* pUserData);
	static void ConDiscord(IConsole::IResult* pResult, void* pUserData);

	static void ConStats(IConsole::IResult* pResult, void* pUserData);
	static void ConAccount(IConsole::IResult* pResult, void* pUserData);

	static void ConTop5Level(IConsole::IResult* pResult, void* pUserData);
	static void ConTop5Points(IConsole::IResult* pResult, void* pUserData);
	static void ConTop5Money(IConsole::IResult* pResult, void* pUserData);
	static void ConTop5Spree(IConsole::IResult* pResult, void* pUserData);
	static void ConTop5PortalBattery(IConsole::IResult* pResult, void* pUserData);
	static void ConTop5PortalBlocker(IConsole::IResult* pResult, void* pUserData);
	static void ConTop5DurakWins(IConsole::IResult* pResult, void* pUserData);
	static void ConTop5DurakProfit(IConsole::IResult* pResult, void* pUserData);

	static void ConPoliceHelper(IConsole::IResult* pResult, void* pUserData);
	static void ConWanted(IConsole::IResult* pResult, void* pUserData);

	static void ConRainbowVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConBloodyVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConAtomVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConTrailVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConSpreadGunVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConRotatingBallVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConEpicCircleVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConLovelyVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConRainbowHookVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConRainbowNameVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConRainbowSpeedVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConSparkleVIP(IConsole::IResult* pResult, void* pUserData);
	static void ConShrug(IConsole::IResult* pResult, void* pUserData);

	static void ConPlot(IConsole::IResult* pResult, void* pUserData);
	static void ConHideDrawings(IConsole::IResult* pResult, void* pUserData);
	static void ConHideBroadcasts(IConsole::IResult* pResult, void* pUserData);
	static void ConAntiPing(IConsole::IResult* pResult, void* pUserData);
	static void ConHighBandwidth(IConsole::IResult* pResult, void* pUserData);
	static void ConSaveSession(IConsole::IResult* pResult, void* pUserData);
	static void ConNoWeaponFix(IConsole::IResult* pResult, void* pUserData);

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
	static void ConTeleRifle(IConsole::IResult* pResult, void* pUserData);
	static void ConUnTeleRifle(IConsole::IResult* pResult, void* pUserData);
	static void ConTaser(IConsole::IResult* pResult, void* pUserData);
	static void ConUnTaser(IConsole::IResult* pResult, void* pUserData);
	static void ConLightningLaser(IConsole::IResult* pResult, void* pUserData);
	static void ConUnLightningLaser(IConsole::IResult* pResult, void* pUserData);

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
	static void ConSpider(IConsole::IResult* pResult, void* pUserData);
	static void ConRemoveSpiders(IConsole::IResult* pResult, void* pUserData);
	static void ConHelicopter(IConsole::IResult* pResult, void* pUserData);
	static void ConRemoveHelicopters(IConsole::IResult* pResult, void* pUserData);
	static void ConSnake(IConsole::IResult* pResult, void* pUserData);

	static void ConForceTransformZombie(IConsole::IResult* pResult, void* pUserData);
	static void ConForceTransformHuman(IConsole::IResult* pResult, void* pUserData);
	static void ConSetDoubleXpLifes(IConsole::IResult* pResult, void* pUserData);
	static void ConSetTaserShield(IConsole::IResult* pResult, void* pUserData);
	static void ConSetSafeArea(IConsole::IResult* pResult, void* pUserData);
	static void ConUnsetSafeArea(IConsole::IResult* pResult, void* pUserData);

	static void ConConnectDummy(IConsole::IResult* pResult, void* pUserData);
	static void ConDisconnectDummy(IConsole::IResult* pResult, void* pUserData);
	static void ConDummymode(IConsole::IResult* pResult, void* pUserData);
	static void ConConnectDefaultDummies(IConsole::IResult* pResult, void* pUserData);

	static void ConTuneLockPlayer(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneLockPlayerReset(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneLockPlayerDump(IConsole::IResult *pResult, void *pUserData);

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

	static void ConProjectileHammer(IConsole::IResult* pResult, void* pUserData);
	static void ConDoorHammer(IConsole::IResult* pResult, void* pUserData);
	static void ConLovely(IConsole::IResult* pResult, void* pUserData);
	static void ConRotatingBall(IConsole::IResult* pResult, void* pUserData);
	static void ConEpicCircle(IConsole::IResult* pResult, void* pUserData);
	static void ConStaffInd(IConsole::IResult* pResult, void* pUserData);
	static void ConRainbowName(IConsole::IResult* pResult, void* pUserData);
	static void ConConfetti(IConsole::IResult* pResult, void* pUserData);
	static void ConSparkle(IConsole::IResult* pResult, void* pUserData);
	static void ConHideFromSpecCount(IConsole::IResult* pResult, void* pUserData);

	static void ConAccLogoutPort(IConsole::IResult* pResult, void* pUserData);
	static void ConAccLogout(IConsole::IResult* pResult, void* pUserData);
	static void ConAccDisable(IConsole::IResult* pResult, void* pUserData);
	static void ConAccInfo(IConsole::IResult* pResult, void* pUserData);
	static void ConAccAddEuros(IConsole::IResult* pResult, void* pUserData);
	static void ConAccEdit(IConsole::IResult* pResult, void* pUserData);
	static void ConAccLevelNeededXP(IConsole::IResult* pResult, void* pUserData);

	static void ConSayBy(IConsole::IResult* pResult, void* pUserData);
	static void ConTeeControl(IConsole::IResult* pResult, void* pUserData);
	static void ConSetMinigame(IConsole::IResult* pResult, void* pUserData);
	static void ConSetNoBonusArea(IConsole::IResult* pResult, void* pUserData);
	static void ConUnsetNoBonusArea(IConsole::IResult* pResult, void* pUserData);
	static void ConRedirectPort(IConsole::IResult* pResult, void* pUserData);
	static void ConJailArrest(IConsole::IResult* pResult, void* pUserData);
	static void ConJailRelease(IConsole::IResult* pResult, void* pUserData);

	static void ConSaveDrop(IConsole::IResult* pResult, void* pUserData);
	static void ConListSavedTees(IConsole::IResult* pResult, void* pUserData);

	static void Con1VS1GlobalCreate(IConsole::IResult* pResult, void* pUserData);
	static void Con1VS1GlobalStart(IConsole::IResult* pResult, void* pUserData);

	void SetViewCursor(IConsole::IResult* pResult, void* pUserData, bool Zoomed);
	static void ConViewCursor(IConsole::IResult* pResult, void* pUserData);
	static void ConViewCursorZoomed(IConsole::IResult* pResult, void* pUserData);

	static void ConWhoIs(IConsole::IResult* pResult, void* pUserData);
	static void ConWhoIsID(IConsole::IResult* pResult, void* pUserData);

	static void ConWhitelistAdd(IConsole::IResult* pResult, void* pUserData);
	static void ConWhitelistRemove(IConsole::IResult* pResult, void* pUserData);
	static void ConWhitelist(IConsole::IResult* pResult, void* pUserData);
	static void ConWhitelistSave(IConsole::IResult* pResult, void* pUserData);
	static void ConWhitelistUpdateServers(IConsole::IResult* pResult, void* pUserData);
	static void ConBansUpdateServers(IConsole::IResult* pResult, void* pUserData);

	static void ConBotLookup(IConsole::IResult* pResult, void* pUserData);

	static void ConAccSysBans(IConsole::IResult* pResult, void* pUserData);
	static void ConAccSysUnban(IConsole::IResult* pResult, void* pUserData);

	static void ConToTelePlot(IConsole::IResult* pResult, void* pUserData);
	static void ConClearPlot(IConsole::IResult* pResult, void* pUserData);
	static void ConPlotOwner(IConsole::IResult* pResult, void* pUserData);
	static void ConPlotInfo(IConsole::IResult* pResult, void* pUserData);
	static void ConPresetList(IConsole::IResult* pResult, void* pUserData);

	static void ConListLoadedLanguages(IConsole::IResult* pResult, void* pUserData);
	static void ConReloadLanguages(IConsole::IResult* pResult, void* pUserData);
	static void ConReloadDesigns(IConsole::IResult* pResult, void* pUserData);
	static void ConAddGrog(IConsole::IResult* pResult, void* pUserData);
	static void ConSetPermille(IConsole::IResult* pResult, void* pUserData);

	enum
	{
		MAX_MUTES = 256,
		MAX_VOTE_MUTES = 64,
	};
	struct CMute
	{
		NETADDR m_Addr;
		int m_Expire;
		char m_aReason[128];
		bool m_InitialChatDelay;
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
	bool TryMute(const NETADDR *pAddr, int Secs, const char *pReason, bool InitialChatDelay);
	void Mute(const NETADDR *pAddr, int Secs, const char *pDisplayName, const char *pReason = "", int ExecutorID = -1, bool InitialChatDelay = false);
	bool TryVoteMute(const NETADDR* pAddr, int Secs);
	bool VoteMute(const NETADDR* pAddr, int Secs, const char* pDisplayName, int AuthedID);
	bool VoteUnmute(const NETADDR* pAddr, const char* pDisplayName, int AuthedID);

	const char *GetWhisper(char *pStr, int *pTarget);
	bool IsVersionBanned(int Version);

	void FDDraceInitPreMapInit();
	void FDDraceInit();

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
	bool PlayerCollision();
	bool PlayerHooking();
	float PlayerJetpack();

	void ResetTuning();

	int m_ChatResponseTargetID;
	int m_ChatPrintCBIndex;
};

inline Mask128 CmaskAll() { return Mask128(); }
inline Mask128 CmaskNone() { return Mask128(-1); }
inline Mask128 CmaskOne(int ClientID) { return Mask128(ClientID); }
inline Mask128 CmaskUnset(Mask128 Mask, int ClientID) { return Mask^CmaskOne(ClientID); }
inline Mask128 CmaskAllExceptOne(int ClientID) { return CmaskUnset(CmaskAll(), ClientID); }
inline bool CmaskIsSet(Mask128 Mask, int ClientID) { return (Mask&CmaskOne(ClientID)) != CmaskNone(); }
#endif
