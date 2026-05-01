// made by fokkonaut

#include "accounts.h"
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include <engine/shared/config.h>
#include <game/server/gamemodes/DDRace.h>
#include <fstream>
#include <unordered_map>

CGameContext *CAccounts::GameServer() const { return m_pGameServer; }
IServer *CAccounts::Server() const { return GameServer()->Server(); }
CConfig *CAccounts::Config() const { return GameServer()->Config(); }
IConsole *CAccounts::Console() const { return GameServer()->Console(); }

void CAccounts::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;

	AddAccount(); // account id 0 means not logged in, so we add an unused account with id 0
	LogoutAllAccountsPort(Config()->m_SvPort);
	LazySaveTopAccounts();

	m_LastDataSaveTick = Server()->Tick();
	ReadMoneyListFile();

	int64 aNeededXP[] = { 5000, 15000, 25000, 35000, 50000, 65000, 80000, 100000, 120000, 130000, 160000, 200000, 240000, 280000, 325000, 370000, 420000, 470000, 520000, 600000,
	680000, 760000, 850000, 950000, 1200000, 1400000, 1600000, 1800000, 2000000, 2210000, 2430000, 2660000, 2900000, 3150000, 3500000, 3950000, 4500000, 5250000, 6100000, 7000000,
	8000000, 9000000, 10000000, 11000000, 12000000, 13000000, 14000000, 15000000, 16000000, 17000000, 18000000, 19000000, 20000000, 21000000, 22000000, 23000000, 24000000, 25000000,
	26000000, 27000000, 28000000, 29000000, 30000000, 31000000, 32000000, 33000000, 34000000, 35000000, 36000000, 37000000, 38000000, 39000000, 40000000, 41010000, 42020000, 43030000,
	44040000, 45050000, 46060000, 47070000, 48080000, 49090000, 50100000, 51110000, 52120000, 53130000, 54140000, 55150000, 56160000, 57170000, 58180000, 59190000, 60200000, 61300000,
	62400000, 63500000, 64600000, 65700000, 66800000, 68000000 };

	for (int i = 0; i < DIFFERENCE_XP_END; i++)
		m_aNeededXP[i] = aNeededXP[i];

	int aTaserPrice[] = { 50000, 75000, 100000, 150000, 200000, 200000, 200000, 300000, 300000, 400000 };
	for (int i = 0; i < NUM_TASER_LEVELS; i++)
		m_aTaserPrice[i] = aTaserPrice[i];

	int aPoliceLevel[] = { 18, 25, 30, 40, 50 };
	for (int i = 0; i < NUM_POLICE_LEVELS; i++)
		m_aPoliceLevel[i] = aPoliceLevel[i];
}

void CAccounts::WriteData()
{
	SaveCurrentTopAccounts();
	LogoutAllAccounts();
	WriteMoneyListFile();
}

void CAccounts::Tick()
{
	for (int i = 0; i < m_NumAccountSystemBans; i++)
	{
		// either reset if expired or if ip did not get banned reset it after ACC_SYS_BAN_DELAY seconds
		if ((m_aAccountSystemBans[i].m_Expire > 0 && m_aAccountSystemBans[i].m_Expire <= Server()->Tick())
			|| (Server()->Tick() > m_aAccountSystemBans[i].m_LastAttempt + ACC_SYS_BAN_DELAY * Server()->TickSpeed()))
		{
			m_NumAccountSystemBans--;
			m_aAccountSystemBans[i] = m_aAccountSystemBans[m_NumAccountSystemBans];
		}
	}

	if (Server()->Tick() > m_LastDataSaveTick + Server()->TickSpeed() * Config()->m_SvDataSaveInterval * 60)
	{
		// save all accounts
		dbg_msg("acc", "automatic account saving...");
		for (unsigned int i = ACC_START; i < m_Accounts.size(); i++)
			WriteAccountStats(i);
		GameServer()->m_Plots.WriteData();
		WriteMoneyListFile();
		SaveCurrentTopAccounts();
		m_LastDataSaveTick = Server()->Tick();
	}
}

int CAccounts::GetAccIdByClientId(int ClientId)
{
	for (unsigned int i = ACC_START; i < m_Accounts.size(); i++)
		if (m_Accounts[i].m_ClientID == ClientId)
			return i;
	return 0;
}

bool CAccounts::TryAccountSystemBan(const NETADDR *pAddr, int Type, int Secs)
{
	// find a matching register ban for this ip, update expiration time if found
	for(int i = 0; i < m_NumAccountSystemBans; i++)
	{
		if(net_addr_comp(&m_aAccountSystemBans[i].m_Addr, pAddr, false) == 0)
		{
			m_aAccountSystemBans[i].m_LastAttempt = Server()->Tick();

			bool Ban = false;
			switch (Type)
			{
			case ACC_SYS_REGISTER:
			{
				m_aAccountSystemBans[i].m_NumRegistrations++;
				Ban = m_aAccountSystemBans[i].m_NumRegistrations > Config()->m_SvAccSysBanRegistrations;
				break;
			}
			case ACC_SYS_LOGIN:
			{
				m_aAccountSystemBans[i].m_NumFailedLogins++;
				Ban = m_aAccountSystemBans[i].m_NumFailedLogins > Config()->m_SvAccSysBanPwFails;
				break;
			}
			case ACC_SYS_PIN:
			{
				m_aAccountSystemBans[i].m_NumFailedPins++;
				Ban = m_aAccountSystemBans[i].m_NumFailedPins > Config()->m_SvAccSysBanPinFails;
				break;
			}
			}

			if (Ban)
			{
				m_aAccountSystemBans[i].m_Expire = Server()->Tick() + Secs * Server()->TickSpeed();
				return true;
			}
			return false;
		}
	}

	// nothing to update create new one
	if(m_NumAccountSystemBans < MAX_ACC_SYS_BANS)
	{
		m_aAccountSystemBans[m_NumAccountSystemBans].m_Addr = *pAddr;
		m_aAccountSystemBans[m_NumAccountSystemBans].m_Expire = 0;
		m_aAccountSystemBans[m_NumAccountSystemBans].m_LastAttempt = Server()->Tick();

		switch (Type)
		{
		case ACC_SYS_REGISTER: m_aAccountSystemBans[m_NumAccountSystemBans].m_NumRegistrations = 1; break;
		case ACC_SYS_LOGIN: m_aAccountSystemBans[m_NumAccountSystemBans].m_NumFailedLogins = 1; break;
		case ACC_SYS_PIN: m_aAccountSystemBans[m_NumAccountSystemBans].m_NumFailedPins = 1; break;
		}

		m_NumAccountSystemBans++;
		return false;
	}
	// no free slot found
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "acc", "account system ban array is full");
	return false;
}

int CAccounts::ProcessAccountSystemBan(int ClientID, int Type)
{
	if(!GameServer()->m_apPlayers[ClientID])
		return 0;

	if (IsAccountSystemBanned(ClientID, true))
		return 1;

	NETADDR Addr;
	Server()->GetClientAddr(ClientID, &Addr);
	if (TryAccountSystemBan(&Addr, Type, ACC_SYS_BAN_DELAY))
	{
		const char *pReason = "";
		switch (Type)
		{
		case ACC_SYS_REGISTER: pReason = GameServer()->m_apPlayers[ClientID]->Localize("Registration spam"); break;
		case ACC_SYS_LOGIN: pReason = GameServer()->m_apPlayers[ClientID]->Localize("Too many login fails"); break;
		case ACC_SYS_PIN: pReason = GameServer()->m_apPlayers[ClientID]->Localize("Too many pin fails"); break;
		}

		char aBuf[128];
		str_format(aBuf, sizeof aBuf, GameServer()->m_apPlayers[ClientID]->Localize("You have been banned from the account system for %d seconds (%s)"), ACC_SYS_BAN_DELAY, pReason);
		GameServer()->SendChatTarget(ClientID, aBuf);
		return 1;
	}

	return 0;
}

bool CAccounts::IsAccountSystemBanned(int ClientID, bool ChatMsg)
{
	if(!GameServer()->m_apPlayers[ClientID])
		return false;

	NETADDR Addr;
	Server()->GetClientAddr(ClientID, &Addr);
	int AccountSystemBanned = 0;

	for(int i = 0; i < m_NumAccountSystemBans; i++)
	{
		if(net_addr_comp(&Addr, &m_aAccountSystemBans[i].m_Addr, false) == 0)
		{
			AccountSystemBanned = (m_aAccountSystemBans[i].m_Expire - Server()->Tick()) / Server()->TickSpeed();
			break;
		}
	}

	if (AccountSystemBanned > 0)
	{
		char aBuf[128];
		str_format(aBuf, sizeof aBuf, GameServer()->m_apPlayers[ClientID]->Localize("You are banned from the account system for the next %d seconds."), AccountSystemBanned);
		GameServer()->SendChatTarget(ClientID, aBuf);
		return true;
	}

	return false;
}

float CAccounts::MonthsPassedSinceRegister(int AccID)
{
	if (AccID < ACC_START)
		return 0;

	time_t Date = m_Accounts[AccID].m_RegisterDate;
	if (!Date)
	{
		// Register date unknown, registered before records started, set register date to 9th april 2021 when it got added for comparison
		Date = (time_t)1617919200;
	}

	time_t Now;
	time(&Now);
	double Seconds = difftime(Now, Date);
	int Days = Seconds / 60 / 60 / 24;
	return Days / 30.f;
}

void CAccounts::LazySaveTopAccounts()
{
	char aFile[256];
	str_format(aFile, sizeof(aFile), "%s/topaccounts.txt", Config()->m_SvTopAccountsFilePath);
	std::ofstream TopAccsFile(aFile);
	if (TopAccsFile.is_open())
	{
		char aEntry[256];
		for (unsigned int i = 0; i < m_TopAccounts.size(); i++)
		{
			str_format(aEntry, sizeof(aEntry), "%s\t%s\t%d\t%d\t%lld\t%d\t%d\t%d\t%d\t%d",
				m_TopAccounts[i].m_aAccountName,
				m_TopAccounts[i].m_aUsername,
				m_TopAccounts[i].m_Level,
				m_TopAccounts[i].m_Points,
				m_TopAccounts[i].m_Money,
				m_TopAccounts[i].m_KillStreak,
				m_TopAccounts[i].m_PortalBattery,
				m_TopAccounts[i].m_PortalBlocker,
				m_TopAccounts[i].m_DurakWins,
				m_TopAccounts[i].m_DurakProfit
			);
			TopAccsFile << aEntry << "\n";
		}
		TopAccsFile << "\n";
	}
	m_TopAccounts.clear();
}

bool CAccounts::LazyLoadTopAccounts(int Type)
{
	char aFile[256];
	str_format(aFile, sizeof(aFile), "%s/topaccounts.txt", Config()->m_SvTopAccountsFilePath);
	std::fstream TopAccsFile(aFile);
	if (!TopAccsFile.is_open())
		return false;

	std::unordered_map<std::string, AccountInfo> AccountMap;
	for (unsigned int AccID = ACC_START; AccID < m_Accounts.size(); AccID++)
	{
		AccountMap[m_Accounts[AccID].m_Username] = m_Accounts[AccID];
	}

	std::string data;
	while (getline(TopAccsFile, data))
	{
		TopAccounts Account;
		const char *pData = data.c_str();
		int Num = sscanf(pData, "%[^\t]\t%[^\t]\t%d\t%d\t%lld\t%d\t%d\t%d\t%d\t%d",
			Account.m_aAccountName,
			Account.m_aUsername,
			&Account.m_Level,
			&Account.m_Points,
			&Account.m_Money,
			&Account.m_KillStreak,
			&Account.m_PortalBattery,
			&Account.m_PortalBlocker,
			&Account.m_DurakWins,
			&Account.m_DurakProfit
		);

		if (Num == 10)
		{
			// update top accounts with all currently online accs so we get correct and up-to-date information
			auto it = AccountMap.find(Account.m_aAccountName);
			if (it != AccountMap.end())
			{
				str_copy(Account.m_aUsername, it->second.m_aLastPlayerName, sizeof(Account.m_aUsername));
				Account.m_Level = it->second.m_Level;
				Account.m_Points = it->second.m_BlockPoints;
				Account.m_Money = it->second.m_Money;
				Account.m_KillStreak = it->second.m_KillingSpreeRecord;
				Account.m_PortalBattery = it->second.m_PortalBattery;
				Account.m_PortalBlocker = it->second.m_PortalBlocker;
				Account.m_DurakWins = it->second.m_DurakWins;
				Account.m_DurakProfit = it->second.m_DurakProfit;
			}
			m_TopAccounts.push_back(Account);
		}
	}

	std::sort(m_TopAccounts.begin(), m_TopAccounts.end(), [Type](const TopAccounts &a, const TopAccounts &b) -> bool {
		switch (Type)
		{
			case TOP_LEVEL: return a.m_Level > b.m_Level;
			case TOP_POINTS: return a.m_Points > b.m_Points;
			case TOP_MONEY: return a.m_Money > b.m_Money;
			case TOP_SPREE: return a.m_KillStreak > b.m_KillStreak;
			case TOP_PORTAL_BATTERY: return a.m_PortalBattery > b.m_PortalBattery;
			case TOP_PORTAL_BLOCKER: return a.m_PortalBlocker > b.m_PortalBlocker;
			case TOP_DURAK_WINS: return a.m_DurakWins > b.m_DurakWins;
			case TOP_DURAK_PROFIT: return a.m_DurakProfit > b.m_DurakProfit;
			default: return false;
		}
	});
	return true;
}

void CAccounts::SetTopAccStats(int FromID)
{
	for (unsigned int i = 0; i < m_TopAccounts.size(); i++)
	{
		// update if we have it in already
		if (!str_comp(m_Accounts[FromID].m_Username, m_TopAccounts[i].m_aAccountName))
		{
			m_TopAccounts[i].m_Level = m_Accounts[FromID].m_Level;
			m_TopAccounts[i].m_Points = m_Accounts[FromID].m_BlockPoints;
			m_TopAccounts[i].m_Money = m_Accounts[FromID].m_Money;
			m_TopAccounts[i].m_KillStreak = m_Accounts[FromID].m_KillingSpreeRecord;
			m_TopAccounts[i].m_PortalBattery = m_Accounts[FromID].m_PortalBattery;
			m_TopAccounts[i].m_PortalBlocker = m_Accounts[FromID].m_PortalBlocker;
			m_TopAccounts[i].m_DurakWins = m_Accounts[FromID].m_DurakWins;
			m_TopAccounts[i].m_DurakProfit = m_Accounts[FromID].m_DurakProfit;
			str_copy(m_TopAccounts[i].m_aUsername, m_Accounts[FromID].m_aLastPlayerName, sizeof(m_TopAccounts[i].m_aUsername));
			return;
		}
	}

	// if not existing in m_TopAccounts yet, add it
	TopAccounts Account;
	Account.m_Level = m_Accounts[FromID].m_Level;
	Account.m_Points = m_Accounts[FromID].m_BlockPoints;
	Account.m_Money = m_Accounts[FromID].m_Money;
	Account.m_KillStreak = m_Accounts[FromID].m_KillingSpreeRecord;
	Account.m_PortalBattery = m_Accounts[FromID].m_PortalBattery;
	Account.m_PortalBlocker = m_Accounts[FromID].m_PortalBlocker;
	Account.m_DurakWins = m_Accounts[FromID].m_DurakWins;
	Account.m_DurakProfit = m_Accounts[FromID].m_DurakProfit;
	str_copy(Account.m_aUsername, m_Accounts[FromID].m_aLastPlayerName, sizeof(Account.m_aUsername));
	str_copy(Account.m_aAccountName, m_Accounts[FromID].m_Username, sizeof(Account.m_aAccountName));
	m_TopAccounts.push_back(Account);
}

void CAccounts::SendTop5AccMessage(IConsole::IResult* pResult, void* pUserData, int Type)
{
	CAccounts *pSelf = (CAccounts*)pUserData;
	CPlayer *pPlayer = pSelf->GameServer()->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	pSelf->LazyLoadTopAccounts(Type);

	char aBuf[512];
	int Debut = pResult->NumArguments() >= 1 && pResult->GetInteger(0) != 0 ? pResult->GetInteger(0) : 1;
	Debut = maximum(1, Debut < 0 ? (int)pSelf->m_TopAccounts.size() + Debut - 3 : Debut);

	// Header
	const char *pType = "";
	switch (Type)
	{
	case TOP_LEVEL: pType = pPlayer->Localize("Level"); break;
	case TOP_POINTS: pType = pPlayer->Localize("Points"); break;
	case TOP_MONEY: pType = pPlayer->Localize("Money"); break;
	case TOP_SPREE: pType = pPlayer->Localize("Spree"); break;
	case TOP_PORTAL_BATTERY: pType = pPlayer->Localize("Portal Battery"); break;
	case TOP_PORTAL_BLOCKER: pType = pPlayer->Localize("Portal Blocker"); break;
	case TOP_DURAK_WINS: pType = pPlayer->Localize("Durák Wins"); break;
	case TOP_DURAK_PROFIT: pType = pPlayer->Localize("Durák Profit"); break;
	}

	str_format(aBuf, sizeof(aBuf), "----------- Top 5 %s -----------", pType);
	pSelf->GameServer()->SendChatTarget(pResult->m_ClientID, aBuf);

	// Short name for entries
	switch (Type)
	{
	case TOP_PORTAL_BATTERY: pType = "Batteries"; break;
	case TOP_PORTAL_BLOCKER: pType = "Blockers"; break;
	case TOP_DURAK_WINS: pType = "Wins"; break;
	case TOP_DURAK_PROFIT: pType = "Profit"; break;
	}

	for (int i = 0; i < 5; i++)
	{
		if (i + Debut > (int)pSelf->m_TopAccounts.size())
			break;
		TopAccounts* r = &pSelf->m_TopAccounts[i + Debut - 1];

		if (Type == TOP_MONEY)
		{
			str_format(aBuf, sizeof(aBuf), "%d. %s %s: %lld", i + Debut, r->m_aUsername, pPlayer->Localize("Money"), r->m_Money);
		}
		else
		{
			int Value = -1;
			switch (Type)
			{
			case TOP_LEVEL: Value = r->m_Level; break;
			case TOP_POINTS: Value = r->m_Points; break;
			case TOP_SPREE: Value = r->m_KillStreak; break;
			case TOP_PORTAL_BATTERY: Value = r->m_PortalBattery; break;
			case TOP_PORTAL_BLOCKER: Value = r->m_PortalBlocker; break;
			case TOP_DURAK_WINS: Value = r->m_DurakWins; break;
			case TOP_DURAK_PROFIT: Value = r->m_DurakProfit; break;
			}
			char aValue[64];
			str_format(aValue, sizeof(aValue), "%s%d%s", Type == TOP_DURAK_PROFIT && Value > 0 ? "+" : "", Value, Type == TOP_DURAK_PROFIT ? "$" : "");
			str_format(aBuf, sizeof(aBuf), "%d. %s %s: %s", i + Debut, r->m_aUsername, pType, aValue);
		}

		pSelf->GameServer()->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	pSelf->GameServer()->SendChatTarget(pResult->m_ClientID, "----------------------------------------");

	// Unload top accounts again after lazy loading
	pSelf->m_TopAccounts.clear();
}

void CAccounts::SaveCurrentTopAccounts()
{
	LazyLoadTopAccounts(TOP_LEVEL);
	for (unsigned int i = 0; i < m_Accounts.size(); i++)
		SetTopAccStats(i);
	LazySaveTopAccounts();
}

int CAccounts::InitAccounts(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CAccounts *pSelf = (CAccounts *)pUser;

	if (!IsDir && str_endswith(pName, ".acc"))
	{
		char aUsername[64];
		str_copy(aUsername, pName, str_length(pName) - 3); // remove the .acc

		int ID = pSelf->GetAccount(aUsername);
		if (ID < ACC_START)
			return 0;

		// load all accounts into the top account list too
		pSelf->SetTopAccStats(ID);

		// logout account if needed
		if (pSelf->m_Accounts[ID].m_LoggedIn && pSelf->m_Accounts[ID].m_Port == pSelf->m_LogoutAccountsPort)
			pSelf->Logout(ID, true);
		else
			pSelf->FreeAccount(ID);
	}

	return 0;
}

int CAccounts::AddAccount()
{
	AccountInfo Account;
	Account.m_Port = Config()->m_SvPort;
	Account.m_LoggedIn = false;
	Account.m_Disabled = false;
	Account.m_Password.data[0] = 0;
	Account.m_Username[0] = '\0';
	Account.m_ClientID = -1;
	Account.m_Level = 0;
	Account.m_XP = 0;
	Account.m_Money = 0;
	Account.m_Kills = 0;
	Account.m_Deaths = 0;
	Account.m_PoliceLevel = 0;
	Account.m_SurvivalKills = 0;
	Account.m_SurvivalWins = 0;
	Account.m_aLastMoneyTransaction[0][0] = '\0';
	Account.m_aLastMoneyTransaction[1][0] = '\0';
	Account.m_aLastMoneyTransaction[2][0] = '\0';
	Account.m_aLastMoneyTransaction[3][0] = '\0';
	Account.m_aLastMoneyTransaction[4][0] = '\0';
	Account.m_SpookyGhost = false;
	Account.m_VIP = 0;
	Account.m_BlockPoints = 0;
	Account.m_InstagibKills = 0;
	Account.m_InstagibWins = 0;
	Account.m_SpawnWeapon[0] = 0;
	Account.m_SpawnWeapon[1] = 0;
	Account.m_SpawnWeapon[2] = 0;
	Account.m_Ninjajetpack = false;
	Account.m_aLastPlayerName[0] = '\0';
	Account.m_SurvivalDeaths = 0;
	Account.m_InstagibDeaths = 0;
	Account.m_TaserLevel = 0;
	Account.m_KillingSpreeRecord = 0;
	Account.m_Euros = 0;
	Account.m_ExpireDateVIP = 0;
	Account.m_PortalRifle = 0;
	Account.m_ExpireDatePortalRifle = 0;
	Account.m_Version = ACC_CURRENT_VERSION;
	Account.m_Addr.type = -1;
	Account.m_LastAddr.type = -1;
	Account.m_TaserBattery = 0;
	Account.m_aContact[0] = '\0';
	Account.m_aTimeoutCode[0] = '\0';
	Account.m_aSecurityPin[0] = '\0';
	Account.m_RegisterDate = 0;
	Account.m_LastLoginDate = 0;
	Account.m_Flags = 0;
	Account.m_aEmail[0] = '\0';
	Account.m_aDesign[0] = '\0';
	Account.m_PortalBattery = 0;
	Account.m_PortalBlocker = 0;
	Account.m_VoteMenuFlags = 0;
	Account.m_DurakWins = 0;
	Account.m_DurakProfit = 0;
	Account.m_aLanguage[0] = '\0';

	m_Accounts.push_back(Account);
	return m_Accounts.size()-1;
}

void CAccounts::ReadAccountStats(int ID, const char *pName)
{
	std::string data;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s/%s.acc", Config()->m_SvAccFilePath, pName);
	std::fstream AccFile(aBuf);

	for (int i = 0; i < NUM_ACCOUNT_VARIABLES; i++)
	{
		getline(AccFile, data);
		const char *pData = data.c_str();
		SetAccVar(ID, i, pData);
	}
}

void CAccounts::WriteAccountStats(int ID)
{
	std::string data;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s/%s.acc", Config()->m_SvAccFilePath, m_Accounts[ID].m_Username);
	std::ofstream AccFile(aBuf);

	if (AccFile.is_open())
	{
		for (int i = 0; i < NUM_ACCOUNT_VARIABLES; i++)
		{
			AccFile << GetAccVarValue(ID, i) << "\n";
		}
		dbg_msg("acc", "saved acc '%s'", m_Accounts[ID].m_Username);
	}
	AccFile.close();
}

void CAccounts::SetAccVar(int ID, int VariableID, const char *pData)
{
	switch (VariableID)
	{
	case ACC_PORT:						m_Accounts[ID].m_Port = atoi(pData); break;
	case ACC_LOGGED_IN:					m_Accounts[ID].m_LoggedIn = atoi(pData); break;
	case ACC_DISABLED:					m_Accounts[ID].m_Disabled = atoi(pData); break;
	case ACC_PASSWORD:					sha256_from_str(&m_Accounts[ID].m_Password, pData); break;
	case ACC_USERNAME:					str_copy(m_Accounts[ID].m_Username, pData, sizeof(m_Accounts[ID].m_Username)); break;
	case ACC_CLIENT_ID:					m_Accounts[ID].m_ClientID = atoi(pData); break;
	case ACC_LEVEL:						m_Accounts[ID].m_Level = atoi(pData); break;
	case ACC_XP:						m_Accounts[ID].m_XP = atoll(pData); break;
	case ACC_MONEY:						m_Accounts[ID].m_Money = atoll(pData); break;
	case ACC_KILLS:						m_Accounts[ID].m_Kills = atoi(pData); break;
	case ACC_DEATHS:					m_Accounts[ID].m_Deaths = atoi(pData); break;
	case ACC_POLICE_LEVEL:				m_Accounts[ID].m_PoliceLevel = atoi(pData); break;
	case ACC_SURVIVAL_KILLS:			m_Accounts[ID].m_SurvivalKills = atoi(pData); break;
	case ACC_SURVIVAL_WINS:				m_Accounts[ID].m_SurvivalWins = atoi(pData); break;
	case ACC_SPOOKY_GHOST:				m_Accounts[ID].m_SpookyGhost = atoi(pData); break;
	case ACC_LAST_MONEY_TRANSACTION_0:	str_copy(m_Accounts[ID].m_aLastMoneyTransaction[0], pData, sizeof(m_Accounts[ID].m_aLastMoneyTransaction[0])); break;
	case ACC_LAST_MONEY_TRANSACTION_1:	str_copy(m_Accounts[ID].m_aLastMoneyTransaction[1], pData, sizeof(m_Accounts[ID].m_aLastMoneyTransaction[1])); break;
	case ACC_LAST_MONEY_TRANSACTION_2:	str_copy(m_Accounts[ID].m_aLastMoneyTransaction[2], pData, sizeof(m_Accounts[ID].m_aLastMoneyTransaction[2])); break;
	case ACC_LAST_MONEY_TRANSACTION_3:	str_copy(m_Accounts[ID].m_aLastMoneyTransaction[3], pData, sizeof(m_Accounts[ID].m_aLastMoneyTransaction[3])); break;
	case ACC_LAST_MONEY_TRANSACTION_4:	str_copy(m_Accounts[ID].m_aLastMoneyTransaction[4], pData, sizeof(m_Accounts[ID].m_aLastMoneyTransaction[4])); break;
	case ACC_VIP:						m_Accounts[ID].m_VIP = atoi(pData); break;
	case ACC_BLOCK_POINTS:				m_Accounts[ID].m_BlockPoints = atoi(pData); break;
	case ACC_INSTAGIB_KILLS:			m_Accounts[ID].m_InstagibKills = atoi(pData); break;
	case ACC_INSTAGIB_WINS:				m_Accounts[ID].m_InstagibWins = atoi(pData); break;
	case ACC_SPAWN_WEAPON_0:			m_Accounts[ID].m_SpawnWeapon[0] = atoi(pData); break;
	case ACC_SPAWN_WEAPON_1:			m_Accounts[ID].m_SpawnWeapon[1] = atoi(pData); break;
	case ACC_SPAWN_WEAPON_2:			m_Accounts[ID].m_SpawnWeapon[2] = atoi(pData); break;
	case ACC_NINJAJETPACK:				m_Accounts[ID].m_Ninjajetpack = atoi(pData); break;
	case ACC_LAST_PLAYER_NAME:			str_copy(m_Accounts[ID].m_aLastPlayerName, pData, sizeof(m_Accounts[ID].m_aLastPlayerName)); break;
	case ACC_SURVIVAL_DEATHS:			m_Accounts[ID].m_SurvivalDeaths = atoi(pData); break;
	case ACC_INSTAGIB_DEATHS:			m_Accounts[ID].m_InstagibDeaths = atoi(pData); break;
	case ACC_TASER_LEVEL:				m_Accounts[ID].m_TaserLevel = atoi(pData); break;
	case ACC_KILLING_SPREE_RECORD:		m_Accounts[ID].m_KillingSpreeRecord = atoi(pData); break;
	case ACC_EUROS:						m_Accounts[ID].m_Euros = atof(pData); break;
	case ACC_EXPIRE_DATE_VIP:			m_Accounts[ID].m_ExpireDateVIP = atoll(pData); break;
	case ACC_PORTAL_RIFLE:				m_Accounts[ID].m_PortalRifle = atoi(pData); break;
	case ACC_EXPIRE_DATE_PORTAL_RIFLE:	m_Accounts[ID].m_ExpireDatePortalRifle = atoll(pData); break;
	case ACC_VERSION:					m_Accounts[ID].m_Version = atoi(pData); break;
	case ACC_ADDR:						net_addr_from_str(&m_Accounts[ID].m_Addr, pData); break;
	case ACC_LAST_ADDR:					net_addr_from_str(&m_Accounts[ID].m_LastAddr, pData); break;
	case ACC_TASER_BATTERY:				m_Accounts[ID].m_TaserBattery = atoi(pData); break;
	case ACC_CONTACT:					str_copy(m_Accounts[ID].m_aContact, pData, sizeof(m_Accounts[ID].m_aContact)); break;
	case ACC_TIMEOUT_CODE:				str_copy(m_Accounts[ID].m_aTimeoutCode, pData, sizeof(m_Accounts[ID].m_aTimeoutCode)); break;
	case ACC_SECURITY_PIN:				str_copy(m_Accounts[ID].m_aSecurityPin, pData, sizeof(m_Accounts[ID].m_aSecurityPin)); break;
	case ACC_REGISTER_DATE:				m_Accounts[ID].m_RegisterDate = atoll(pData); break;
	case ACC_LAST_LOGIN_DATE:			m_Accounts[ID].m_LastLoginDate = atoll(pData); break;
	case ACC_FLAGS:						m_Accounts[ID].m_Flags = atoi(pData); break;
	case ACC_EMAIL:						str_copy(m_Accounts[ID].m_aEmail, pData, sizeof(m_Accounts[ID].m_aEmail)); break;
	case ACC_DESIGN:					str_copy(m_Accounts[ID].m_aDesign, pData, sizeof(m_Accounts[ID].m_aDesign)); break;
	case ACC_PORTAL_BATTERY:			m_Accounts[ID].m_PortalBattery = atoi(pData); break;
	case ACC_PORTAL_BLOCKER:			m_Accounts[ID].m_PortalBlocker = atoi(pData); break;
	case ACC_VOTE_MENU_FLAGS:			m_Accounts[ID].m_VoteMenuFlags = atoi(pData); break;
	case ACC_DURAK_WINS:				m_Accounts[ID].m_DurakWins = atoi(pData); break;
	case ACC_DURAK_PROFIT:				m_Accounts[ID].m_DurakProfit = atoll(pData); break;
	case ACC_LANGUAGE:					str_copy(m_Accounts[ID].m_aLanguage, pData, sizeof(m_Accounts[ID].m_aLanguage)); break;
	}
}

const char *CAccounts::GetAccVarName(int VariableID)
{
	switch (VariableID)
	{
	case ACC_PORT:						return "port";
	case ACC_LOGGED_IN:					return "logged_in";
	case ACC_DISABLED:					return "disabled";
	case ACC_PASSWORD:					return "password";
	case ACC_USERNAME:					return "username";
	case ACC_CLIENT_ID:					return "client_id";
	case ACC_LEVEL:						return "level";
	case ACC_XP:						return "xp";
	case ACC_MONEY:						return "money";
	case ACC_KILLS:						return "kills";
	case ACC_DEATHS:					return "deaths";
	case ACC_POLICE_LEVEL:				return "police_level";
	case ACC_SURVIVAL_KILLS:			return "survival_kills";
	case ACC_SURVIVAL_WINS:				return "survival_wins";
	case ACC_SPOOKY_GHOST:				return "spooky_ghost";
	case ACC_LAST_MONEY_TRANSACTION_0:	return "last_money_transaction_0";
	case ACC_LAST_MONEY_TRANSACTION_1:	return "last_money_transaction_1";
	case ACC_LAST_MONEY_TRANSACTION_2:	return "last_money_transaction_2";
	case ACC_LAST_MONEY_TRANSACTION_3:	return "last_money_transaction_3";
	case ACC_LAST_MONEY_TRANSACTION_4:	return "last_money_transaction_4";
	case ACC_VIP:						return "vip";
	case ACC_BLOCK_POINTS:				return "block_points";
	case ACC_INSTAGIB_KILLS:			return "instagib_kills";
	case ACC_INSTAGIB_WINS:				return "instagib_wins";
	case ACC_SPAWN_WEAPON_0:			return "spawn_weapon_shotgun";
	case ACC_SPAWN_WEAPON_1:			return "spawn_weapon_grenade";
	case ACC_SPAWN_WEAPON_2:			return "spawn_weapon_rifle";
	case ACC_NINJAJETPACK:				return "ninjajetpack";
	case ACC_LAST_PLAYER_NAME:			return "last_player_name";
	case ACC_SURVIVAL_DEATHS:			return "survival_deaths";
	case ACC_INSTAGIB_DEATHS:			return "instagib_deaths";
	case ACC_TASER_LEVEL:				return "taser_level";
	case ACC_KILLING_SPREE_RECORD:		return "killing_spree_record";
	case ACC_EUROS:						return "euros";
	case ACC_EXPIRE_DATE_VIP:			return "expire_date_vip";
	case ACC_PORTAL_RIFLE:				return "portal_rifle";
	case ACC_EXPIRE_DATE_PORTAL_RIFLE:	return "expire_date_portal_rifle";
	case ACC_VERSION:					return "version";
	case ACC_ADDR:						return "addr";
	case ACC_LAST_ADDR:					return "last_addr";
	case ACC_TASER_BATTERY:				return "taser_battery";
	case ACC_CONTACT:					return "contact";
	case ACC_TIMEOUT_CODE:				return "timeout_code";
	case ACC_SECURITY_PIN:				return "security_pin";
	case ACC_REGISTER_DATE:				return "register_date";
	case ACC_LAST_LOGIN_DATE:			return "last_login_date";
	case ACC_FLAGS:						return "flags";
	case ACC_EMAIL:						return "email";
	case ACC_DESIGN:					return "design";
	case ACC_PORTAL_BATTERY:			return "portal_battery";
	case ACC_PORTAL_BLOCKER:			return "portal_blocker";
	case ACC_VOTE_MENU_FLAGS:			return "vote_menu_flags";
	case ACC_DURAK_WINS:				return "durak_wins";
	case ACC_DURAK_PROFIT:				return "durak_profit";
	case ACC_LANGUAGE:					return "language";
	}
	return "Unknown";
}

const char *CAccounts::GetAccVarValue(int ID, int VariableID)
{
	static char aBuf[128];
	str_copy(aBuf, "Unknown", sizeof(aBuf));

	switch (VariableID)
	{
	case ACC_PORT:						str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_Port); break;
	case ACC_LOGGED_IN:					str_format(aBuf, sizeof(aBuf), "%d", (int)m_Accounts[ID].m_LoggedIn); break;
	case ACC_DISABLED:					str_format(aBuf, sizeof(aBuf), "%d", (int)m_Accounts[ID].m_Disabled); break;
	case ACC_PASSWORD:					sha256_str(m_Accounts[ID].m_Password, aBuf, sizeof(aBuf)); break;
	case ACC_USERNAME:					str_copy(aBuf, m_Accounts[ID].m_Username, sizeof(aBuf)); break;
	case ACC_CLIENT_ID:					str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_ClientID); break;
	case ACC_LEVEL:						str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_Level); break;
	case ACC_XP:						str_format(aBuf, sizeof(aBuf), "%lld", m_Accounts[ID].m_XP); break;
	case ACC_MONEY:						str_format(aBuf, sizeof(aBuf), "%lld", m_Accounts[ID].m_Money); break;
	case ACC_KILLS:						str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_Kills); break;
	case ACC_DEATHS:					str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_Deaths); break;
	case ACC_POLICE_LEVEL:				str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_PoliceLevel); break;
	case ACC_SURVIVAL_KILLS:			str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_SurvivalKills); break;
	case ACC_SURVIVAL_WINS:				str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_SurvivalWins); break;
	case ACC_SPOOKY_GHOST:				str_format(aBuf, sizeof(aBuf), "%d", (int)m_Accounts[ID].m_SpookyGhost); break;
	case ACC_LAST_MONEY_TRANSACTION_0:	str_copy(aBuf, m_Accounts[ID].m_aLastMoneyTransaction[0], sizeof(aBuf)); break;
	case ACC_LAST_MONEY_TRANSACTION_1:	str_copy(aBuf, m_Accounts[ID].m_aLastMoneyTransaction[1], sizeof(aBuf)); break;
	case ACC_LAST_MONEY_TRANSACTION_2:	str_copy(aBuf, m_Accounts[ID].m_aLastMoneyTransaction[2], sizeof(aBuf)); break;
	case ACC_LAST_MONEY_TRANSACTION_3:	str_copy(aBuf, m_Accounts[ID].m_aLastMoneyTransaction[3], sizeof(aBuf)); break;
	case ACC_LAST_MONEY_TRANSACTION_4:	str_copy(aBuf, m_Accounts[ID].m_aLastMoneyTransaction[4], sizeof(aBuf)); break;
	case ACC_VIP:						str_format(aBuf, sizeof(aBuf), "%d", (int)m_Accounts[ID].m_VIP); break;
	case ACC_BLOCK_POINTS:				str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_BlockPoints); break;
	case ACC_INSTAGIB_KILLS:			str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_InstagibKills); break;
	case ACC_INSTAGIB_WINS:				str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_InstagibWins); break;
	case ACC_SPAWN_WEAPON_0:			str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_SpawnWeapon[0]); break;
	case ACC_SPAWN_WEAPON_1:			str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_SpawnWeapon[1]); break;
	case ACC_SPAWN_WEAPON_2:			str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_SpawnWeapon[2]); break;
	case ACC_NINJAJETPACK:				str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_Ninjajetpack); break;
	case ACC_LAST_PLAYER_NAME:			str_copy(aBuf, m_Accounts[ID].m_aLastPlayerName, sizeof(aBuf)); break;
	case ACC_SURVIVAL_DEATHS:			str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_SurvivalDeaths); break;
	case ACC_INSTAGIB_DEATHS:			str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_InstagibDeaths); break;
	case ACC_TASER_LEVEL:				str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_TaserLevel); break;
	case ACC_KILLING_SPREE_RECORD:		str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_KillingSpreeRecord); break;
	case ACC_EUROS:						str_format(aBuf, sizeof(aBuf), "%.2f", m_Accounts[ID].m_Euros); break;
	case ACC_EXPIRE_DATE_VIP:			str_format(aBuf, sizeof(aBuf), "%lld", (int64)m_Accounts[ID].m_ExpireDateVIP); break;
	case ACC_PORTAL_RIFLE:				str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_PortalRifle); break;
	case ACC_EXPIRE_DATE_PORTAL_RIFLE:	str_format(aBuf, sizeof(aBuf), "%lld", (int64)m_Accounts[ID].m_ExpireDatePortalRifle); break;
	case ACC_VERSION:					str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_Version); break;
	case ACC_ADDR:						net_addr_str(&m_Accounts[ID].m_Addr, aBuf, sizeof(aBuf), true); break;
	case ACC_LAST_ADDR:					net_addr_str(&m_Accounts[ID].m_LastAddr, aBuf, sizeof(aBuf), true); break;
	case ACC_TASER_BATTERY:				str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_TaserBattery); break;
	case ACC_CONTACT:					str_copy(aBuf, m_Accounts[ID].m_aContact, sizeof(aBuf)); break;
	case ACC_TIMEOUT_CODE:				str_copy(aBuf, m_Accounts[ID].m_aTimeoutCode, sizeof(aBuf)); break;
	case ACC_SECURITY_PIN:				str_copy(aBuf, m_Accounts[ID].m_aSecurityPin, sizeof(aBuf)); break;
	case ACC_REGISTER_DATE:				str_format(aBuf, sizeof(aBuf), "%lld", (int64)m_Accounts[ID].m_RegisterDate); break;
	case ACC_LAST_LOGIN_DATE:			str_format(aBuf, sizeof(aBuf), "%lld", (int64)m_Accounts[ID].m_LastLoginDate); break;
	case ACC_FLAGS:						str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_Flags); break;
	case ACC_EMAIL:						str_copy(aBuf, m_Accounts[ID].m_aEmail, sizeof(aBuf)); break;
	case ACC_DESIGN:					str_copy(aBuf, m_Accounts[ID].m_aDesign, sizeof(aBuf)); break;
	case ACC_PORTAL_BATTERY:			str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_PortalBattery); break;
	case ACC_PORTAL_BLOCKER:			str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_PortalBlocker); break;
	case ACC_VOTE_MENU_FLAGS:			str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_VoteMenuFlags); break;
	case ACC_DURAK_WINS:				str_format(aBuf, sizeof(aBuf), "%d", m_Accounts[ID].m_DurakWins); break;
	case ACC_DURAK_PROFIT:				str_format(aBuf, sizeof(aBuf), "%lld", m_Accounts[ID].m_DurakProfit); break;
	case ACC_LANGUAGE:					str_copy(aBuf, m_Accounts[ID].m_aLanguage, sizeof(aBuf)); break;
	}
	return aBuf;
}

void CAccounts::Logout(int ID, bool Silent)
{
	if (ID < ACC_START)
		return;

	if (m_Accounts[ID].m_ClientID >= 0 && GameServer()->m_apPlayers[m_Accounts[ID].m_ClientID])
		GameServer()->m_apPlayers[m_Accounts[ID].m_ClientID]->OnLogout();

	m_Accounts[ID].m_LoggedIn = false;
	m_Accounts[ID].m_ClientID = -1;
	WriteAccountStats(ID);

	if (!Silent)
		dbg_msg("acc", "logged out account '%s'", m_Accounts[ID].m_Username);
	FreeAccount(ID);
}

void CAccounts::LogoutAllAccounts()
{
	unsigned int Amount = m_Accounts.size();
	for (unsigned int i = ACC_START; i < Amount; i++)
		Logout(ACC_START);
	dbg_msg("acc", "logged out all accounts");
}

void CAccounts::LogoutAllAccountsPort(int Port)
{
	m_LogoutAccountsPort = Port; // set before calling InitAccounts
	GameServer()->Storage()->ListDirectory(IStorage::TYPE_ALL, Config()->m_SvAccFilePath, InitAccounts, this);
}

bool CAccounts::Login(int ClientID, const char *pUsername, const char *pPassword, bool PasswordRequired, bool ForceDesignLoad)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if (!pPlayer)
		return false;

	if (IsAccountSystemBanned(ClientID))
		return false;

	if (pPlayer->GetAccID() >= ACC_START)
	{
		GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You are already logged in"));
		return false;
	}

	int ID = AddAccount();
	ReadAccountStats(ID, pUsername);

	if (m_Accounts[ID].m_Username[0] == '\0')
	{
		GameServer()->SendChatTarget(ClientID, pPlayer->Localize("That account doesn't exist, please register first"));
		FreeAccount(ID);
		return false;
	}

	if (m_Accounts[ID].m_LoggedIn)
	{
		if (m_Accounts[ID].m_Port == Config()->m_SvPort)
			GameServer()->SendChatTarget(ClientID, pPlayer->Localize("This account is already logged in"));
		else
			GameServer()->SendChatTarget(ClientID, pPlayer->Localize("This account is already logged in on another server"));
		FreeAccount(ID);
		return false;
	}

	if (m_Accounts[ID].m_Disabled)
	{
		GameServer()->SendChatTarget(ClientID, pPlayer->Localize("This account is disabled"));
		FreeAccount(ID);
		return false;
	}

	if (PasswordRequired && pPassword[0] && m_Accounts[ID].m_Version < 7)
	{
		char aPassword[SHA256_MAXSTRSIZE];
		str_copy(aPassword, pPassword, sizeof(aPassword));
		SHA256_CTX Sha256Ctx;
		sha256_init(&Sha256Ctx);
		sha256_update(&Sha256Ctx, &aPassword, sizeof(aPassword));
		SHA256_DIGEST Sha256 = sha256_finish(&Sha256Ctx);
		if (sha256_comp(m_Accounts[ID].m_Password, Sha256) == 0)
			SetPassword(ID, pPassword);
	}

	if (PasswordRequired && CheckPassword(ID, pPassword))
	{
		GameServer()->SendChatTarget(ClientID, pPlayer->Localize("Wrong password"));
		FreeAccount(ID);

		ProcessAccountSystemBan(ClientID, ACC_SYS_LOGIN);
		return false;
	}

	// set some variables and save the account with some new values
	{
		m_Accounts[ID].m_Port = Config()->m_SvPort;
		m_Accounts[ID].m_LoggedIn = true;
		m_Accounts[ID].m_ClientID = ClientID;
		m_Accounts[ID].m_Version = ACC_CURRENT_VERSION;
		str_copy(m_Accounts[ID].m_aLastPlayerName, Server()->ClientName(ClientID), sizeof(m_Accounts[ID].m_aLastPlayerName));
		if (pPlayer->m_TimeoutCode[0] != '\0')
			str_copy(m_Accounts[ID].m_aTimeoutCode, pPlayer->m_TimeoutCode, sizeof(m_Accounts[ID].m_aTimeoutCode));
		time_t Now;
		time(&Now);
		m_Accounts[ID].m_LastLoginDate = Now;

		NETADDR Addr;
		Server()->GetClientAddr(ClientID, &Addr);
		if (net_addr_comp(&Addr, &m_Accounts[ID].m_Addr, false) != 0)
		{
			// addresses are not equal, update last address and set new current address
			m_Accounts[ID].m_LastAddr = m_Accounts[ID].m_Addr;
			Server()->GetClientAddr(ClientID, &m_Accounts[ID].m_Addr);
		}
		else
		{
			// addresses are equal, just update the current address to get the possible changed port
			Server()->GetClientAddr(ClientID, &m_Accounts[ID].m_Addr);
		}

		WriteAccountStats(ID);
	}

	pPlayer->OnLogin(ForceDesignLoad);
	return true;
}

SHA256_DIGEST CAccounts::HashPassword(const char *pPassword)
{
	SHA256_CTX Sha256Ctx;
	sha256_init(&Sha256Ctx);
	sha256_update(&Sha256Ctx, pPassword, str_length(pPassword));
	return sha256_finish(&Sha256Ctx);
}

void CAccounts::SetPassword(int ID, const char *pPassword)
{
	if (ID < ACC_START)
		return;
	m_Accounts[ID].m_Password = HashPassword(pPassword);
}

bool CAccounts::CheckPassword(int ID, const char *pPassword)
{
	if (ID < ACC_START)
		return true;
	return sha256_comp(m_Accounts[ID].m_Password, HashPassword(pPassword)) != 0;
}

int64 CAccounts::GetNeededXP(int Level)
{
	if (Level < 0)
		return 0;
	if (Level < DIFFERENCE_XP_END)
		return m_aNeededXP[Level];
	return m_aNeededXP[DIFFERENCE_XP_END-1] + (OVER_LVL_100_XP * (Level+1 - DIFFERENCE_XP_END));
}

int CAccounts::GetAccIDByUsername(const char *pUsername)
{
	for (unsigned int i = ACC_START; i < m_Accounts.size(); i++)
		if (!str_comp(pUsername, m_Accounts[i].m_Username))
			return i;
	return 0;
}

int CAccounts::GetAccount(const char *pUsername)
{
	int ID = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if (pPlayer && pPlayer->GetAccID() >= ACC_START && !str_comp(m_Accounts[pPlayer->GetAccID()].m_Username, pUsername))
		{
			ID = pPlayer->GetAccID();
			break;
		}
	}

	if (ID < ACC_START)
	{
		ID = AddAccount();
		ReadAccountStats(ID, pUsername);
	}

	if (m_Accounts[ID].m_Username[0] == '\0')
	{
		FreeAccount(ID);
		return 0;
	}

	return ID;
}

void CAccounts::FreeAccount(int ID)
{
	m_Accounts.erase(m_Accounts.begin() + ID);
}

bool CAccounts::IsAccLoggedInThisPort(int ID)
{
	return m_Accounts[ID].m_LoggedIn && m_Accounts[ID].m_Port == Config()->m_SvPort;
}

void CAccounts::UpdateDesignList(int ID, const char *pMapDesign)
{
	std::vector<SSavedDesignEntry> vDesigns = GetDesignList(ID);

	// Update the list
	bool Found = false;
	for (unsigned int i = 0; i < vDesigns.size(); i++)
	{
		if (str_comp(vDesigns[i].m_aMapName, Server()->GetCurrentMapName()) == 0)
		{
			// Update
			str_copy(vDesigns[i].m_aDesign, pMapDesign, sizeof(vDesigns[i].m_aDesign));
			Found = true;
			break;
		}
	}

	// Add if not found
	if (!Found)
	{
		SSavedDesignEntry Entry;
		str_copy(Entry.m_aDesign, pMapDesign, sizeof(Entry.m_aDesign));
		str_copy(Entry.m_aMapName, Server()->GetCurrentMapName(), sizeof(Entry.m_aDesign));
		vDesigns.push_back(Entry);
	}

	// Write list
	m_Accounts[ID].m_aDesign[0] = '\0';
	for (unsigned int i = 0; i < vDesigns.size(); i++)
	{
		// don't add default's to the list, waste
		if (str_comp(vDesigns[i].m_aDesign, "default") == 0)
			continue;

		char aEntry[196];
		str_format(aEntry, sizeof(aEntry), "%s:%s,", vDesigns[i].m_aMapName, vDesigns[i].m_aDesign);
		str_append(m_Accounts[ID].m_aDesign, aEntry, sizeof(m_Accounts[ID].m_aDesign));
	}
}

const char *CAccounts::GetCurrentDesignFromList(int ID)
{
	static char aBuf[128];
	str_copy(aBuf, "default", sizeof(aBuf));

	std::vector<SSavedDesignEntry> vDesigns = GetDesignList(ID);
	for (unsigned int i = 0; i < vDesigns.size(); i++)
	{
		if (str_comp(vDesigns[i].m_aMapName, Server()->GetCurrentMapName()) == 0)
		{
			str_copy(aBuf, vDesigns[i].m_aDesign, sizeof(aBuf));
			break;
		}
	}
	return aBuf;
}

std::vector<CAccounts::SSavedDesignEntry> CAccounts::GetDesignList(int ID)
{
	std::vector<SSavedDesignEntry> vDesigns;
	if (ID < ACC_START)
		return vDesigns;

	const char *pList = m_Accounts[ID].m_aDesign;
	while (1)
	{
		if (!pList || !pList[0])
			break;

		SSavedDesignEntry Entry;
		Entry.m_aMapName[0] = '\0';
		Entry.m_aDesign[0] = '\0';
		sscanf(pList, "%[^:]:%[^,]", Entry.m_aMapName, Entry.m_aDesign);
		if (Entry.m_aMapName[0] && Entry.m_aDesign[0])
		{
			vDesigns.push_back(Entry);
		}

		// jump to next comma, if it exists skip it so we can start the next loop run with the next data
		if ((pList = str_find(pList, ",")))
			pList++;
	}

	return vDesigns;
}

bool CAccounts::SameIP(int ClientID1, int ClientID2)
{
	if (ClientID1 < 0 || ClientID1 >= MAX_CLIENTS || ClientID2 < 0 || ClientID2 >= MAX_CLIENTS ||
		!GameServer()->m_apPlayers[ClientID1] || !GameServer()->m_apPlayers[ClientID2])
		return false;

	NETADDR Addr1, Addr2;
	Server()->GetClientAddr(ClientID1, &Addr1);
	Server()->GetClientAddr(ClientID2, &Addr2);

	bool Same = (net_addr_comp(&Addr1, &Addr2, false) == 0);

	int AccID1 = GameServer()->m_apPlayers[ClientID1]->GetAccID();
	if (AccID1 >= ACC_START)
		Same = Same || (net_addr_comp(&Addr2, &m_Accounts[AccID1].m_LastAddr, false) == 0);

	int AccID2 = GameServer()->m_apPlayers[ClientID2]->GetAccID();
	if (AccID2 >= ACC_START)
		Same = Same || (net_addr_comp(&Addr1, &m_Accounts[AccID2].m_LastAddr, false) == 0);

	return Same;
}

bool CAccounts::SameIP(int AccID, const NETADDR *pAddr)
{
	if (AccID < ACC_START)
		return false;

	return (net_addr_comp(pAddr, &m_Accounts[AccID].m_Addr, false) == 0
		|| net_addr_comp(pAddr, &m_Accounts[AccID].m_LastAddr, false) == 0);
}

void CAccounts::WriteDonationFile(int Type, float Amount, int ID, const char *pDescription)
{
	const char* pFrom = Type == TYPE_DONATION ? "donation" : Type == TYPE_PURCHASE ? "purchase" : "";
	char aBuf[256], aMsg[256];
	time_t Now = time(0);
	str_format(aMsg, sizeof(aMsg), "Date: %s, Euros: %.2f, Account: '%s', Description: '%s'", GameServer()->GetDate(Now, false), Amount, m_Accounts[ID].m_Username, pDescription);
	Console()->Format(aBuf, sizeof(aBuf), pFrom, aMsg);

	char aFile[256];
	str_format(aFile, sizeof(aFile), "%s/donations.txt", Config()->m_SvDonationFilePath);
	std::ofstream DonationsFile(aFile, std::ios_base::app | std::ios_base::out);
	DonationsFile << aBuf << "\n";
}

void CAccounts::ReadMoneyListFile()
{
	std::string data;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s/%s/moneydrops.txt", Config()->m_SvMoneyDropsFilePath, Server()->GetCurrentMapName());
	std::fstream MoneyDropsFile(aBuf);
	getline(MoneyDropsFile, data);
	const char *pStr = data.c_str();

	while (1)
	{
		if (!pStr)
			break;

		vec2 Pos = vec2(-1, -1);
		int Amount = 0;

		sscanf(pStr, "%f/%f:%d", &Pos.x, &Pos.y, &Amount);
		if (Amount > 0)
			new CMoney(&GameServer()->m_World, vec2(Pos.x*32.f, Pos.y*32.f), Amount);

		// jump to next comma, if it exists skip it so we can start the next loop run with the next money data
		if ((pStr = str_find(pStr, ",")))
			pStr++;
	}
}

void CAccounts::WriteMoneyListFile()
{
	char aFile[256];
	str_format(aFile, sizeof(aFile), "%s/%s/moneydrops.txt", Config()->m_SvMoneyDropsFilePath, Server()->GetCurrentMapName());
	std::ofstream MoneyDropsFile(aFile);

	CMoney *pMoney = (CMoney *)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_MONEY);
	for (; pMoney; pMoney = (CMoney *)pMoney->TypeNext())
	{
		char aEntry[64];
		str_format(aEntry, sizeof(aEntry), "%.2f/%.2f:%d,", pMoney->GetPos().x/32.f, pMoney->GetPos().y/32.f, pMoney->GetAmount());
		MoneyDropsFile << aEntry;
	}
}
