// made by fokkonaut

#ifndef GAME_SERVER_MISC_ACCOUNTS_H
#define GAME_SERVER_MISC_ACCOUNTS_H

#include <base/hash_ctxt.h>
#include <generated/protocol.h>
#include <engine/shared/protocol.h>
#include <engine/console.h>
#include <vector>

enum
{
	// start for account ids and plot ids
	ACC_START = 1, // account ids start with 1, 0 means not logged in
	PLOT_START = 1,

	// needed xp after the hardcoded requirements
	DIFFERENCE_XP_END = 100,
	OVER_LVL_100_XP = 2000000,

	// item maximums
	NUM_TASER_LEVELS = 10,
	NUM_POLICE_LEVELS = 5,
	MAX_TASER_BATTERY = 100,
	MAX_PASSWORD_LENGTH = 128,
	MIN_USERNAME_LENGTH = 3,
	MAX_USERNAME_LENGTH = 20,

	// update this one with every acc change you do
	ACC_CURRENT_VERSION = 19,

	// vip
	VIP_CLASSIC = 1,
	VIP_PLUS,
};

// make sure these are in the same order as the variables in AccountInfo struct
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
	ACC_REGISTER_DATE,
	ACC_LAST_LOGIN_DATE,
	ACC_FLAGS,
	ACC_EMAIL,
	ACC_DESIGN,
	ACC_PORTAL_BATTERY,
	ACC_PORTAL_BLOCKER,
	ACC_VOTE_MENU_FLAGS,
	ACC_DURAK_WINS,
	ACC_DURAK_PROFIT,
	ACC_LANGUAGE,
	NUM_ACCOUNT_VARIABLES
};

class CGameContext;
class IServer;
class CConfig;

class CAccounts
{
public:
	struct SSavedDesignEntry
	{
		char m_aMapName[128];
		char m_aDesign[64];
	};

	struct AccountInfo
	{
		int m_Port;
		bool m_LoggedIn;
		bool m_Disabled;
		SHA256_DIGEST m_Password;
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
		int m_VIP;
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
		float m_Euros;
		time_t m_ExpireDateVIP;
		int m_PortalRifle;
		time_t m_ExpireDatePortalRifle;
		int m_Version;
		NETADDR m_Addr;
		NETADDR m_LastAddr;
		int m_TaserBattery;
		char m_aContact[128];
		char m_aTimeoutCode[64];
		char m_aSecurityPin[5];
		time_t m_RegisterDate;
		time_t m_LastLoginDate;
		int m_Flags;
		char m_aEmail[128];
		char m_aDesign[256];
		int m_PortalBattery;
		int m_PortalBlocker;
		int m_VoteMenuFlags;
		int m_DurakWins;
		int64 m_DurakProfit;
		char m_aLanguage[32];
	};

private:
	CGameContext *m_pGameServer;
	CGameContext *GameServer() const;
	IServer *Server() const;
	CConfig *Config() const;
	IConsole *Console() const;

	struct TopAccounts
	{
		char m_aUsername[32];
		char m_aAccountName[32];
		int m_Level;
		int m_Points;
		int64 m_Money;
		int m_KillStreak;
		int m_PortalBattery;
		int m_PortalBlocker;
		int m_DurakWins;
		int m_DurakProfit;
	};
	std::vector<TopAccounts> m_TopAccounts;
	void SetTopAccStats(int FromID);
	void LazySaveTopAccounts();
	bool LazyLoadTopAccounts(int Type);
	void SaveCurrentTopAccounts();

	int m_LogoutAccountsPort;
	static int InitAccounts(const char* pName, int IsDir, int StorageType, void* pUser);

	int64 m_aNeededXP[DIFFERENCE_XP_END];
	int m_LastDataSaveTick;
	std::vector<AccountInfo> m_Accounts;

	std::vector<SSavedDesignEntry> GetDesignList(int ID);
	bool TryAccountSystemBan(const NETADDR *pAddr, int Type, int Secs);

	// money drops
	void WriteMoneyListFile();
	void ReadMoneyListFile();

public:
	void Init(CGameContext *pGameServer);
	void Tick();
	void WriteData();

	// Account getter
	AccountInfo &Get(int AccID) { return (AccID >= ACC_START && AccID < (int)m_Accounts.size()) ? m_Accounts[AccID] : m_Accounts[0]; }
	int GetAccIdByClientId(int ClientId);

	//account
	int GetAccIDByUsername(const char *pUsername);
	int GetAccount(const char *pUsername);
	void FreeAccount(int ID);
	bool IsAccLoggedInThisPort(int ID);

	// acc saved design
	void UpdateDesignList(int ID, const char *pMapDesign);
	const char *GetCurrentDesignFromList(int ID);

	const char *GetAccVarName(int VariableID);
	const char *GetAccVarValue(int ID, int VariableID);
	void SetAccVar(int ID, int VariableID, const char *pData);

	enum Top5
	{
		TOP_LEVEL,
		TOP_POINTS,
		TOP_MONEY,
		TOP_SPREE,
		TOP_PORTAL_BATTERY,
		TOP_PORTAL_BLOCKER,
		TOP_DURAK_WINS,
		TOP_DURAK_PROFIT,
	};
	void SendTop5AccMessage(IConsole::IResult* pResult, void* pUserData, int Type);

	int AddAccount();
	void ReadAccountStats(int ID, const char* pName);
	void WriteAccountStats(int ID);
	void Logout(int ID, bool Silent = false);
	void LogoutAllAccounts();
	void LogoutAllAccountsPort(int Port);
	bool Login(int ClientID, const char *pUsername, const char *pPassword, bool PasswordRequired = true, bool ForceDesignLoad = false);
	SHA256_DIGEST HashPassword(const char *pPassword);
	void SetPassword(int ID, const char *pPassword);
	bool CheckPassword(int ID, const char *pPassword);

	int m_aTaserPrice[NUM_TASER_LEVELS];
	int m_aPoliceLevel[NUM_POLICE_LEVELS];
	int64 GetNeededXP(int Level);

	enum
	{
		TYPE_DONATION,
		TYPE_PURCHASE
	};
	void WriteDonationFile(int Type, float Amount, int ID, const char *pDescription);

	// flags for specific ingame variables that will be saved on logout and loaded on login again
	enum AccountFlags
	{
		ACCFLAG_ZOOMCURSOR = 1<<0,
		ACCFLAG_PLOTSPAWN = 1<<1,
		ACCFLAG_SILENTFARM = 1<<2,
		ACCFLAG_HIDEDRAWINGS = 1<<3,
		ACCFLAG_RESUMEMOVED = 1<<4,
		ACCFLAG_HIDEBROADCASTS = 1<<5,
		ACCFLAG_ANTIPING = 1<<6,
		ACCFLAG_HIGHBANDWIDTH = 1<<7,
		ACCFLAG_SAVEPLAYERDISCONNECT = 1<<8,
		ACCFLAG_NOWEAPONFIX = 1<<9,
	};

	float MonthsPassedSinceRegister(int AccID);
	
	bool SameIP(int ClientID1, int ClientID2);
	bool SameIP(int AccID, const NETADDR *pAddr);

	// account system bans
	enum
	{
		MAX_ACC_SYS_BANS = 512,
		ACC_SYS_BAN_DELAY = 60 * 60 * 6, // 6 hours
	};

	struct CAccountSystemBan
	{
		NETADDR m_Addr;
		int m_Expire;
		int m_NumRegistrations;
		int m_NumFailedLogins;
		int m_NumFailedPins;
		int64 m_LastAttempt;
	};

	enum
	{
		ACC_SYS_REGISTER,
		ACC_SYS_LOGIN,
		ACC_SYS_PIN,
	};

	CAccountSystemBan m_aAccountSystemBans[MAX_ACC_SYS_BANS];
	int m_NumAccountSystemBans;
	int ProcessAccountSystemBan(int ClientID, int Type);
	bool IsAccountSystemBanned(int ClientID, bool ChatMsg = false);

	bool IsTrustWorthy(int AccID);
};

#endif //GAME_SERVER_MISC_ACCOUNTS_H
