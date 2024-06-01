/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/math.h>
#include <base/system.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/map.h>
#include <engine/masterserver.h>
#include <engine/server.h>
#include <engine/storage.h>

#include <engine/shared/compression.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/demo.h>
#include <engine/shared/econ.h>
#include <engine/shared/filecollection.h>
#include <engine/shared/mapchecker.h>
#include <engine/shared/netban.h>
#include <engine/shared/network.h>
#include <engine/shared/packer.h>
#include <engine/shared/protocol.h>
#include <engine/shared/protocol_ex.h>
#include <engine/shared/snapshot.h>
#include <engine/shared/fifo.h>
#include <engine/shared/json.h>
#include <engine/shared/http.h>

#include "register.h"
#include "server.h"
#include "crc.h"

#include <string.h>
#include <string>
#include <vector>
#include <fstream>
#include <engine/shared/linereader.h>
#include <engine/external/json-parser/json.h>

#if defined(CONF_FAMILY_WINDOWS)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif

/*static const char *StrLtrim(const char *pStr)
{
	while(*pStr && *pStr >= 0 && *pStr <= 32)
		pStr++;
	return pStr;
}

static void StrRtrim(char *pStr)
{
	int i = str_length(pStr);
	while(i >= 0)
	{
		if(pStr[i] < 0 || pStr[i] > 32)
			break;
		pStr[i] = 0;
		i--;
	}
}*/


CSnapIDPool::CSnapIDPool()
{
	Reset();
}

void CSnapIDPool::Reset()
{
	for(int i = 0; i < MAX_IDS; i++)
	{
		m_aIDs[i].m_Next = i+1;
		m_aIDs[i].m_State = 0;
	}

	m_aIDs[MAX_IDS-1].m_Next = -1;
	m_FirstFree = 0;
	m_FirstTimed = -1;
	m_LastTimed = -1;
	m_Usage = 0;
	m_InUsage = 0;
}


void CSnapIDPool::RemoveFirstTimeout()
{
	int NextTimed = m_aIDs[m_FirstTimed].m_Next;

	// add it to the free list
	m_aIDs[m_FirstTimed].m_Next = m_FirstFree;
	m_aIDs[m_FirstTimed].m_State = 0;
	m_FirstFree = m_FirstTimed;

	// remove it from the timed list
	m_FirstTimed = NextTimed;
	if(m_FirstTimed == -1)
		m_LastTimed = -1;

	m_Usage--;
}

int CSnapIDPool::NewID()
{
	int64 Now = time_get();

	// process timed ids
	while(m_FirstTimed != -1 && m_aIDs[m_FirstTimed].m_Timeout < Now)
		RemoveFirstTimeout();

	int ID = m_FirstFree;
	if(ID == -1)
	{
		dbg_msg("server", "invalid id");
		return ID;
	}
	m_FirstFree = m_aIDs[m_FirstFree].m_Next;
	m_aIDs[ID].m_State = 1;
	m_Usage++;
	m_InUsage++;
	return ID;
}

void CSnapIDPool::TimeoutIDs()
{
	// process timed ids
	while(m_FirstTimed != -1)
		RemoveFirstTimeout();
}

void CSnapIDPool::FreeID(int ID)
{
	if(ID < 0)
		return;
	dbg_assert(m_aIDs[ID].m_State == 1, "id is not allocated");

	m_InUsage--;
	m_aIDs[ID].m_State = 2;
	m_aIDs[ID].m_Timeout = time_get()+time_freq()*5;
	m_aIDs[ID].m_Next = -1;

	if(m_LastTimed != -1)
	{
		m_aIDs[m_LastTimed].m_Next = ID;
		m_LastTimed = ID;
	}
	else
	{
		m_FirstTimed = ID;
		m_LastTimed = ID;
	}
}


void CServerBan::InitServerBan(IConsole *pConsole, IStorage *pStorage, CServer* pServer)
{
	CNetBan::Init(pConsole, pStorage, pServer->Config());

	m_pServer = pServer;

	// overwrites base command, todo: improve this
	Console()->Register("ban", "s[id|ip|range] ?i[minutes] r[reason]", CFGFLAG_SERVER|CFGFLAG_STORE, ConBanExt, this, "Ban player with IP/IP range/client id for x minutes for any reason", AUTHED_ADMIN);
}

template<class T>
int CServerBan::BanExt(T *pBanPool, const typename T::CDataType *pData, int Seconds, const char *pReason)
{
	// validate address
	if(Server()->m_RconClientID >= 0 && Server()->m_RconClientID < MAX_CLIENTS &&
		Server()->m_aClients[Server()->m_RconClientID].m_State != CServer::CClient::STATE_EMPTY)
	{
		if(NetMatch(pData, Server()->m_NetServer.ClientAddr(Server()->m_RconClientID)))
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (you can't ban yourself)");
			return -1;
		}

		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(i == Server()->m_RconClientID || Server()->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
				continue;

			if(Server()->m_aClients[i].m_Authed >= Server()->m_RconAuthLevel && NetMatch(pData, Server()->m_NetServer.ClientAddr(i)))
			{
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (command denied)");
				return -1;
			}
		}
	}
	else if(Server()->m_RconClientID == IServer::RCON_CID_VOTE)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(Server()->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
				continue;

			if(Server()->m_aClients[i].m_Authed != AUTHED_NO && NetMatch(pData, Server()->m_NetServer.ClientAddr(i)))
			{
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (command denied)");
				return -1;
			}
		}
	}

	int Result = Ban(pBanPool, pData, Seconds, pReason);
	if(Result != 0)
		return Result;

	// drop banned clients
	typename T::CDataType Data = *pData;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(Server()->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
			continue;

		if(NetMatch(&Data, Server()->m_NetServer.ClientAddr(i)))
		{
			CNetHash NetHash(&Data);
			char aBuf[256];
			MakeBanInfo(pBanPool->Find(&Data, &NetHash), aBuf, sizeof(aBuf), MSGTYPE_PLAYER);
			Server()->m_NetServer.Drop(i, aBuf, true);
		}
	}

	return Result;
}

int CServerBan::BanAddr(const NETADDR *pAddr, int Seconds, const char *pReason)
{
	return BanExt(&m_BanAddrPool, pAddr, Seconds, pReason);
}

int CServerBan::BanRange(const CNetRange *pRange, int Seconds, const char *pReason)
{
	if(pRange->IsValid())
		return BanExt(&m_BanRangePool, pRange, Seconds, pReason);

	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban failed (invalid range)");
	return -1;
}

void CServerBan::ConBanExt(IConsole::IResult *pResult, void *pUser)
{
	CServerBan *pThis = static_cast<CServerBan *>(pUser);

	const char *pStr = pResult->GetString(0);
	int Minutes = pResult->NumArguments()>1 ? clamp(pResult->GetInteger(1), 0, 44640) : 30;
	const char *pReason = pResult->NumArguments()>2 ? pResult->GetString(2) : "No reason given";

	if(!str_is_number(pStr))
	{
		int ClientID = str_toint(pStr);
		if(ClientID < 0 || ClientID >= MAX_CLIENTS || pThis->Server()->m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
			pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (invalid client id)");
		else if (pThis->Server()->m_aClients[ClientID].m_State == CServer::CClient::STATE_DUMMY)
			pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (can't ban dummies)");
		else
		{
			char aName[32];
			str_copy(aName, pThis->Server()->ClientName(ClientID), sizeof(aName));
			if (pThis->BanAddr(pThis->Server()->m_NetServer.ClientAddr(ClientID), Minutes*60, pReason) == 0)
			{
				char aBuf[128];
				if (Minutes == 0)
					str_format(aBuf, sizeof(aBuf), "'%s' has been banned for life (%s)", aName, pReason);
				else
					str_format(aBuf, sizeof(aBuf), "'%s' has been banned for %d minutes (%s)", aName, Minutes, pReason);
				pThis->Server()->GameServer()->SendModLogMessage(pResult->m_ClientID, aBuf);
			}
		}
	}
	else
		ConBan(pResult, pUser);
}


void CServer::CClient::Reset()
{
	// reset input
	for(int i = 0; i < 200; i++)
	{
		m_aInputs[i].m_GameTick = -1;
	}
	m_CurrentInput = 0;
	mem_zero(&m_LatestInput, sizeof(m_LatestInput));

	m_Snapshots.PurgeAll();
	m_LastAckedSnapshot = -1;
	m_LastInputTick = -1;
	m_SnapRate = CClient::SNAPRATE_INIT;
	m_Score = 0;
	m_MapChunk = 0;

	for (int i = 0; i < VANILLA_MAX_CLIENTS; i++)
		m_aIdMap[i] = -1;
	for (int i = 0; i < MAX_CLIENTS; i++)
		m_aReverseIdMap[i] = -1;

	m_CurrentMapDesign = -1;
	m_DesignChange = false;
	m_RedirectDropTime = 0;
}

void CServer::CClient::ResetContent()
{
	m_State = CClient::STATE_EMPTY;
	m_aName[0] = 0;
	m_aClan[0] = 0;
	m_Country = -1;
	m_Authed = AUTHED_NO;
	m_AuthKey = -1;
	m_AuthTries = 0;
	m_pRconCmdToSend = 0;
	m_pMapListEntryToSend = 0;
	m_NoRconNote = false;
	m_Quitting = false;
	m_ShowIps = false;
	m_DDNetVersion = VERSION_NONE;
	m_GotDDNetVersionPacket = false;
	m_DDNetVersionSettled = false;
	m_Traffic = 0;
	m_TrafficSince = 0;
	m_Sevendown = false;
	m_Socket = SOCKET_MAIN;
	m_DnsblState = CClient::DNSBL_STATE_NONE;
	m_PgscState = CClient::PGSC_STATE_NONE;
	m_IdleDummy = false;
	m_DummyHammer = false;
	m_HammerflyMarked = false;
	for (int i = 0; i < 5; i++)
		m_aIdleDummyTrack[i] = 0;
	m_CurrentIdleTrackPos = 0;

	str_copy(m_aLanguage, "none", sizeof(m_aLanguage));
	m_Main = true;

	m_Rejoining = false;
	m_RedirectDropTime = 0;
}

CServer::CServer() : m_DemoRecorder(&m_SnapshotDelta)
{
	m_TickSpeed = SERVER_TICK_SPEED;

	m_pGameServer = 0;

	m_CurrentGameTick = 0;
	m_RunServer = UNINITIALIZED;

	m_pCurrentMapData = 0;
	m_CurrentMapSize = 0;

	m_pFakeMapData = 0;
	m_FakeMapSize = 0;
	m_FakeMapCrc = 0;

	m_NumMapEntries = 0;
	m_pFirstMapEntry = 0;
	m_pLastMapEntry = 0;
	m_pMapListHeap = 0;

	m_MapReload = false;

	m_RconClientID = IServer::RCON_CID_SERV;
	m_RconAuthLevel = AUTHED_ADMIN;

	m_RconRestrict = -1;

	m_RconPasswordSet = 0;

#ifdef CONF_FAMILY_UNIX
	m_ConnLoggingSocketCreated = false;
#endif

	m_pRegister = nullptr;
	m_pRegisterTwo = nullptr;

	m_ServerInfoNeedsUpdate = false;

#if defined (CONF_SQL)
	for (int i = 0; i < MAX_SQLSERVERS; i++)
	{
		m_apSqlReadServers[i] = 0;
		m_apSqlWriteServers[i] = 0;
	}

	CSqlConnector::SetReadServers(m_apSqlReadServers);
	CSqlConnector::SetWriteServers(m_apSqlWriteServers);
#endif

	Init();
}

CServer::~CServer()
{
	delete m_pRegister;
	delete m_pRegisterTwo;
}

bool CServer::IsClientNameAvailable(int ClientId, const char *pNameRequest)
{
	// check for empty names
	if(!pNameRequest[0])
		return false;

	// check for names starting with /, as they can be abused to make people
	// write chat commands
	if(pNameRequest[0] == '/')
		return false;

	// make sure that two clients don't have the same name
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i != ClientId && m_aClients[i].m_State >= CClient::STATE_READY)
		{
			if(str_utf8_comp_confusable(pNameRequest, m_aClients[i].m_aName) == 0)
				return false;
		}
	}

	return true;
}

bool CServer::SetClientNameImpl(int ClientId, const char *pNameRequest, bool Set)
{
	dbg_assert(0 <= ClientId && ClientId < MAX_CLIENTS, "invalid client id");
	if(m_aClients[ClientId].m_State < CClient::STATE_READY)
		return false;

	// trim the name
	char aTrimmedName[MAX_NAME_LENGTH];
	str_copy(aTrimmedName, str_utf8_skip_whitespaces(pNameRequest));
	str_utf8_trim_right(aTrimmedName);

	char aNameTry[MAX_NAME_LENGTH];
	str_copy(aNameTry, aTrimmedName);

	if(!IsClientNameAvailable(ClientId, aNameTry))
	{
		// auto rename
		for(int i = 1;; i++)
		{
			str_format(aNameTry, sizeof(aNameTry), "(%d)%s", i, aTrimmedName);
			if(IsClientNameAvailable(ClientId, aNameTry))
				break;
		}
	}

	bool Changed = str_comp(m_aClients[ClientId].m_aName, aNameTry) != 0;

	if(Set && Changed)
	{
		// set the client name
		str_copy(m_aClients[ClientId].m_aName, aNameTry);
	}

	return Changed;
}

bool CServer::WouldClientNameChange(int ClientId, const char *pNameRequest)
{
	return SetClientNameImpl(ClientId, pNameRequest, false);
}

void CServer::SetClientName(int ClientId, const char *pName)
{
	SetClientNameImpl(ClientId, pName, true);
}

void CServer::SetClientClan(int ClientID, const char *pClan)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY || !pClan)
		return;

	str_copy(m_aClients[ClientID].m_aClan, pClan, sizeof(m_aClients[ClientID].m_aClan));
}

void CServer::SetClientCountry(int ClientID, int Country)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return;

	m_aClients[ClientID].m_Country = Country;
}

void CServer::SetClientScore(int ClientID, int Score)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return;

	if (m_aClients[ClientID].m_Score != Score)
		ExpireServerInfo();

	m_aClients[ClientID].m_Score = Score;
}

int CServer::Kick(int ClientID, const char *pReason)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CClient::STATE_EMPTY)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "invalid client id to kick");
		return 1;
	}
	else if(m_RconClientID == ClientID)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "you can't kick yourself");
 		return 1;
	}
	else if(m_aClients[ClientID].m_Authed > m_RconAuthLevel)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "kick command denied");
 		return 1;
	}
	else if (m_aClients[ClientID].m_State == CClient::STATE_DUMMY)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "you can't kick dummies");
		return 1;
	}

	m_NetServer.Drop(ClientID, pReason);
	return 0;
}

void CServer::Ban(int ClientID, int Seconds, const char *pReason)
{
	m_ServerBan.BanAddr(m_NetServer.ClientAddr(ClientID), Seconds, pReason);
}

void CServer::RedirectClient(int ClientID, int Port, bool Verbose)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return;

	char aBuf[512];
	bool SupportsRedirect = m_aClients[ClientID].m_DDNetVersion >= VERSION_DDNET_REDIRECT;
	if(Verbose)
	{
		str_format(aBuf, sizeof(aBuf), "redirecting '%s' to port %d supported=%d", ClientName(ClientID), Port, SupportsRedirect);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "redirect", aBuf);
	}

	if(!SupportsRedirect)
	{
		bool SamePort = Port == Config()->m_SvPort;
		str_format(aBuf, sizeof(aBuf), "Redirect unsupported: please connect to port %d", Port);
		Kick(ClientID, SamePort ? "Redirect unsupported: please reconnect" : aBuf);
		return;
	}

	CMsgPacker Msg(NETMSG_REDIRECT, true);
	Msg.AddInt(Port);
	SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);

	m_aClients[ClientID].m_RedirectDropTime = time_get() + time_freq() * 10;
	m_aClients[ClientID].m_State = CClient::STATE_REDIRECTED;
}

/*int CServer::Tick()
{
	return m_CurrentGameTick;
}*/

int64 CServer::TickStartTime(int Tick)
{
	return m_GameStartTime + (time_freq()*Tick)/SERVER_TICK_SPEED;
}

/*int CServer::TickSpeed()
{
	return SERVER_TICK_SPEED;
}*/

int CServer::Init()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aClients[i].ResetContent();
		m_aClients[i].m_Snapshots.Init();
	}

	m_AnnouncementLastLine = 0;
	m_BotLookupState = BOTLOOKUP_STATE_DONE;
	m_CurrentGameTick = 0;
	m_aCurrentMap[0] = 0;

	return 0;
}

void CServer::SetRconCID(int ClientID)
{
	m_RconClientID = ClientID;
}

int CServer::GetAuthedState(int ClientID) const
{
	return m_aClients[ClientID].m_Authed;
}

const char *CServer::AuthName(int ClientID) const
{
	switch(m_aClients[ClientID].m_Authed)
	{
	case AUTHED_ADMIN: return "default_admin";
	case AUTHED_MOD: return "default_mod";
	case AUTHED_HELPER: return "default_helper";
	}
	return 0;
}

bool CServer::IsBanned(int ClientID)
{
	return m_ServerBan.IsBanned(m_NetServer.ClientAddr(ClientID), 0, 0, 0);
}

int CServer::GetClientInfo(int ClientID, CClientInfo *pInfo) const
{
	dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "client_id is not valid");
	dbg_assert(pInfo != 0, "info can not be null");

	if(m_aClients[ClientID].m_State == CClient::STATE_INGAME || m_aClients[ClientID].m_State == CClient::STATE_DUMMY)
	{
		pInfo->m_pName = m_aClients[ClientID].m_aName;
		pInfo->m_Latency = m_aClients[ClientID].m_Latency;
		pInfo->m_GotDDNetVersion = m_aClients[ClientID].m_DDNetVersionSettled;
		pInfo->m_DDNetVersion = m_aClients[ClientID].m_DDNetVersion >= 0 ? m_aClients[ClientID].m_DDNetVersion : VERSION_VANILLA;
		if(m_aClients[ClientID].m_GotDDNetVersionPacket)
		{
			pInfo->m_pConnectionID = &m_aClients[ClientID].m_ConnectionID;
			pInfo->m_pDDNetVersionStr = m_aClients[ClientID].m_aDDNetVersionStr;
		}
		else
		{
			pInfo->m_pConnectionID = 0;
			pInfo->m_pDDNetVersionStr = 0;
		}
		return 1;
	}
	return 0;
}

void CServer::SetClientDDNetVersion(int ClientID, int DDNetVersion)
{
	dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "client_id is not valid");

	if(m_aClients[ClientID].m_State == CClient::STATE_INGAME)
	{
		m_aClients[ClientID].m_DDNetVersion = DDNetVersion;
		m_aClients[ClientID].m_DDNetVersionSettled = true;
	}
}

void CServer::GetClientAddr(int ClientID, char *pAddrStr, int Size, bool AddPort) const
{
	if(ClientID >= 0 && ClientID < MAX_CLIENTS && m_aClients[ClientID].m_State == CClient::STATE_INGAME)
		net_addr_str(m_NetServer.ClientAddr(ClientID), pAddrStr, Size, AddPort);
}

int CServer::GetClientVersion(int ClientID) const
{
	if(ClientID >= 0 && ClientID < MAX_CLIENTS && m_aClients[ClientID].m_State == CClient::STATE_INGAME)
		return m_aClients[ClientID].m_Version;
	return 0;
}

const char *CServer::GetClientVersionStr(int ClientID) const
{
	if(ClientID >= 0 && ClientID < MAX_CLIENTS && m_aClients[ClientID].m_State == CClient::STATE_INGAME)
	{
		static char s_aVersion[64];
		if (m_aClients[ClientID].m_Sevendown)
			str_format(s_aVersion, sizeof(s_aVersion), "%d", m_aClients[ClientID].m_DDNetVersion);
		else
			str_format(s_aVersion, sizeof(s_aVersion), "%x", m_aClients[ClientID].m_Version);
		return s_aVersion;
	}
	return "";
}

const char *CServer::ClientName(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
		return "(invalid)";
	if(m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME || m_aClients[ClientID].m_State == CClient::STATE_DUMMY)
		return m_aClients[ClientID].m_aName;
	else
		return "(connecting)";

}

const char *CServer::ClientClan(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
		return "";
	if(m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME || m_aClients[ClientID].m_State == CClient::STATE_DUMMY)
		return m_aClients[ClientID].m_aClan;
	else
		return "";
}

int CServer::ClientCountry(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
		return -1;
	if(m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME || m_aClients[ClientID].m_State == CClient::STATE_DUMMY)
		return m_aClients[ClientID].m_Country;
	else
		return -1;
}

bool CServer::ClientIngame(int ClientID) const
{
	return ClientID >= 0 && ClientID < MAX_CLIENTS && (m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME || m_aClients[ClientID].m_State == CClient::STATE_DUMMY);
}

static inline bool RepackMsg(const CMsgPacker *pMsg, CPacker &Packer, bool Sevendown)
{
	int MsgId = pMsg->m_MsgID;
	Packer.Reset();
	if(Sevendown)
	{
		if(pMsg->m_System)
		{
			if(MsgId >= NETMSG_MAP_CHANGE && MsgId <= NETMSG_MAP_DATA)
				;
			else if(MsgId >= NETMSG_CON_READY && MsgId <= NETMSG_INPUTTIMING)
				MsgId -= 1;
			else if(MsgId == NETMSG_RCON_AUTH_ON || MsgId == NETMSG_RCON_AUTH_OFF)
				MsgId = 10;
			else if (MsgId == NETMSG_RCON_LINE)
				MsgId = 11;
			else if (MsgId == NETMSG_RCON_CMD_ADD || MsgId == NETMSG_RCON_CMD_REM)
				MsgId += 11;
			else if(MsgId >= NETMSG_AUTH_CHALLANGE && MsgId <= NETMSG_AUTH_RESULT)
				MsgId -= 4;
			else if(MsgId >= NETMSG_PING && MsgId <= NETMSG_ERROR)
				MsgId -= 4;
			else if(MsgId >= OFFSET_UUID)
				;
			else if(MsgId >= NUM_NETMSGTYPES)
				MsgId -= NUM_NETMSGTYPES;
			else
				return true;
		}
		else
		{
			if(MsgId >= NETMSGTYPE_SV_MOTD && MsgId <= NETMSGTYPE_SV_CHAT)
				;
			else if(MsgId == NETMSGTYPE_SV_KILLMSG)
				MsgId -= 1;
			else if(MsgId >= NETMSGTYPE_SV_TUNEPARAMS && MsgId <= NETMSGTYPE_SV_VOTESTATUS)
				;
			else if(MsgId >= OFFSET_UUID)
				;
			else if(MsgId >= NUM_NETMSGTYPES)
				MsgId -= NUM_NETMSGTYPES;
			else
				return true;
		}
	}

	if(MsgId < OFFSET_UUID)
	{
		Packer.AddInt((MsgId<<1)|(pMsg->m_System?1:0));
	}
	else
	{
		Packer.AddInt((0<<1)|(pMsg->m_System?1:0)); // NETMSG_EX, NETMSGTYPE_EX
		g_UuidManager.PackUuid(MsgId, &Packer);
	}
	Packer.AddRaw(pMsg->Data(), pMsg->Size());

	return false;
}

int CServer::SendMsg(CMsgPacker *pMsg, int Flags, int ClientID)
{
	CNetChunk Packet;
	if(!pMsg)
		return -1;

	// drop invalid packet
	if(ClientID != -1 && (ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CClient::STATE_EMPTY || m_aClients[ClientID].m_Quitting))
		return 0;

	mem_zero(&Packet, sizeof(CNetChunk));
	if(Flags&MSGFLAG_VITAL)
		Packet.m_Flags |= NETSENDFLAG_VITAL;
	if(Flags&MSGFLAG_FLUSH)
		Packet.m_Flags |= NETSENDFLAG_FLUSH;

	if(ClientID < 0)
	{
		CPacker Pack6, Pack7;
		if(RepackMsg(pMsg, Pack6, true))
			return -1;
		if(RepackMsg(pMsg, Pack7, false))
			return -1;

		// write message to demo recorder
		if(!(Flags&MSGFLAG_NORECORD))
			m_DemoRecorder.RecordMessage(Pack7.Data(), Pack7.Size());

		if(!(Flags&MSGFLAG_NOSEND))
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(m_aClients[i].m_State == CClient::STATE_INGAME && !m_aClients[i].m_Quitting)
				{
					CPacker *Pack = m_aClients[i].m_Sevendown ? &Pack6 : &Pack7;
					Packet.m_pData = Pack->Data();
					Packet.m_DataSize = Pack->Size();
					Packet.m_ClientID = i;
					if(Antibot()->OnEngineServerMessage(i, Packet.m_pData, Packet.m_DataSize, Flags))
					{
						continue;
					}
					m_NetServer.Send(&Packet);
				}
			}
		}
	}
	else
	{
		CPacker Pack;
		if(RepackMsg(pMsg, Pack, m_aClients[ClientID].m_Sevendown))
			return -1;

		Packet.m_ClientID = ClientID;
		Packet.m_pData = Pack.Data();
		Packet.m_DataSize = Pack.Size();

		if(Antibot()->OnEngineServerMessage(ClientID, Packet.m_pData, Packet.m_DataSize, Flags))
		{
			return 0;
		}

		// write message to demo recorder
		if(!(Flags&MSGFLAG_NORECORD))
			m_DemoRecorder.RecordMessage(Pack.Data(), Pack.Size());

		if(!(Flags&MSGFLAG_NOSEND))
			m_NetServer.Send(&Packet);
	}

	return 0;
}

void CServer::SendMsgRaw(int ClientID, const void *pData, int Size, int Flags)
{
	CNetChunk Packet;
	mem_zero(&Packet, sizeof(CNetChunk));
	Packet.m_ClientID = ClientID;
	Packet.m_pData = pData;
	Packet.m_DataSize = Size;
	Packet.m_Flags = 0;
	if(Flags & MSGFLAG_VITAL)
	{
		Packet.m_Flags |= NETSENDFLAG_VITAL;
	}
	if(Flags & MSGFLAG_FLUSH)
	{
		Packet.m_Flags |= NETSENDFLAG_FLUSH;
	}
	m_NetServer.Send(&Packet);
}

void CServer::DoSnapshot()
{
	GameServer()->OnPreSnap();

	// create snapshot for demo recording
	if(m_DemoRecorder.IsRecording())
	{
		char aData[CSnapshot::MAX_SIZE];
		int SnapshotSize;

		// build snap and possibly add some messages
		m_SnapshotBuilder.Init();
		GameServer()->OnSnap(-1);
		SnapshotSize = m_SnapshotBuilder.Finish(aData);

		// write snapshot
		m_DemoRecorder.RecordSnapshot(Tick(), aData, SnapshotSize);
	}

	// create snapshots for all clients
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// client must be ingame to receive snapshots
		if(m_aClients[i].m_State != CClient::STATE_INGAME || m_aClients[i].m_DesignChange || m_aClients[i].m_Rejoining)
			continue;

		// this client is trying to recover, don't spam snapshots
		if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_RECOVER && (Tick()%50) != 0)
			continue;

		// this client is trying to recover, don't spam snapshots
		if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_INIT && (Tick()%10) != 0)
			continue;

		{
			char aData[CSnapshot::MAX_SIZE];
			CSnapshot *pData = (CSnapshot*)aData;	// Fix compiler warning for strict-aliasing
			char aDeltaData[CSnapshot::MAX_SIZE];
			char aCompData[CSnapshot::MAX_SIZE];
			int SnapshotSize;
			int Crc;
			static CSnapshot EmptySnap;
			CSnapshot *pDeltashot = &EmptySnap;
			int DeltashotSize;
			int DeltaTick = -1;
			int DeltaSize;

			m_SnapshotBuilder.Init(m_aClients[i].m_Sevendown);

			GameServer()->OnSnap(i);

			// finish snapshot
			SnapshotSize = m_SnapshotBuilder.Finish(pData);
			Crc = pData->Crc();

			// remove old snapshos
			// keep 3 seconds worth of snapshots
			m_aClients[i].m_Snapshots.PurgeUntil(m_CurrentGameTick-SERVER_TICK_SPEED*3);

			// save it the snapshot
			m_aClients[i].m_Snapshots.Add(m_CurrentGameTick, time_get(), SnapshotSize, pData, 0);

			// find snapshot that we can perform delta against
			EmptySnap.Clear();

			{
				DeltashotSize = m_aClients[i].m_Snapshots.Get(m_aClients[i].m_LastAckedSnapshot, 0, &pDeltashot, 0);
				if(DeltashotSize >= 0)
					DeltaTick = m_aClients[i].m_LastAckedSnapshot;
				else
				{
					// no acked package found, force client to recover rate
					if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_FULL)
						m_aClients[i].m_SnapRate = CClient::SNAPRATE_RECOVER;
				}
			}

			// create delta
			DeltaSize = m_SnapshotDelta.CreateDelta(pDeltashot, pData, aDeltaData);

			if(DeltaSize)
			{
				// compress it
				int SnapshotSize;
				const int MaxSize = MAX_SNAPSHOT_PACKSIZE;
				int NumPackets;

				SnapshotSize = CVariableInt::Compress(aDeltaData, DeltaSize, aCompData, sizeof(aCompData));
				NumPackets = (SnapshotSize+MaxSize-1)/MaxSize;

				for(int n = 0, Left = SnapshotSize; Left > 0; n++)
				{
					int Chunk = Left < MaxSize ? Left : MaxSize;
					Left -= Chunk;

					if(NumPackets == 1)
					{
						CMsgPacker Msg(NETMSG_SNAPSINGLE, true);
						Msg.AddInt(m_CurrentGameTick);
						Msg.AddInt(m_CurrentGameTick-DeltaTick);
						Msg.AddInt(Crc);
						Msg.AddInt(Chunk);
						Msg.AddRaw(&aCompData[n*MaxSize], Chunk);
						SendMsg(&Msg, MSGFLAG_FLUSH, i);
					}
					else
					{
						CMsgPacker Msg(NETMSG_SNAP, true);
						Msg.AddInt(m_CurrentGameTick);
						Msg.AddInt(m_CurrentGameTick-DeltaTick);
						Msg.AddInt(NumPackets);
						Msg.AddInt(n);
						Msg.AddInt(Crc);
						Msg.AddInt(Chunk);
						Msg.AddRaw(&aCompData[n*MaxSize], Chunk);
						SendMsg(&Msg, MSGFLAG_FLUSH, i);
					}
				}
			}
			else
			{
				CMsgPacker Msg(NETMSG_SNAPEMPTY, true);
				Msg.AddInt(m_CurrentGameTick);
				Msg.AddInt(m_CurrentGameTick-DeltaTick);
				SendMsg(&Msg, MSGFLAG_FLUSH, i);
			}
		}
	}

	GameServer()->OnPostSnap();
}

int CServer::ClientRejoinCallback(int ClientID, bool Sevendown, int Socket, void *pUser)
{
	CServer *pThis = (CServer *)pUser;

	pThis->m_aClients[ClientID].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientID].m_AuthKey = -1;
	pThis->m_aClients[ClientID].m_pRconCmdToSend = 0;
	pThis->m_aClients[ClientID].m_DDNetVersion = VERSION_NONE;
	pThis->m_aClients[ClientID].m_GotDDNetVersionPacket = false;
	pThis->m_aClients[ClientID].m_DDNetVersionSettled = false;

	int PrevDesign = pThis->m_aClients[ClientID].m_CurrentMapDesign;
	pThis->m_aClients[ClientID].Reset();
	pThis->m_aClients[ClientID].m_Rejoining = true;
	pThis->m_aClients[ClientID].m_Sevendown = Sevendown;
	pThis->m_aClients[ClientID].m_Socket = Socket;

	if (pThis->m_aClients[ClientID].m_Main)
	{
		pThis->m_aClients[ClientID].m_DesignChange = false;
		pThis->m_aClients[ClientID].m_CurrentMapDesign = PrevDesign;
	}
	else
	{
		int Dummy = pThis->GetDummy(ClientID);
		if (Dummy != -1)
			pThis->m_aClients[ClientID].m_CurrentMapDesign = pThis->m_aClients[Dummy].m_CurrentMapDesign;
	}

	pThis->SendMap(ClientID);

	return 0;
}

int CServer::NewClientCallback(int ClientID, bool Sevendown, int Socket, void *pUser)
{
	CServer *pThis = (CServer *)pUser;

	// Remove non human player on same slot
	if(pThis->GameServer()->IsClientBot(ClientID))
	{
		pThis->GameServer()->OnClientDrop(ClientID, "removing dummy");
	}

	pThis->m_aClients[ClientID].ResetContent();
	pThis->m_aClients[ClientID].m_State = CClient::STATE_PREAUTH;
	pThis->m_aClients[ClientID].m_Sevendown = Sevendown;
	pThis->m_aClients[ClientID].m_Socket = Socket;

	pThis->m_aClients[ClientID].Reset();
	pThis->GameServer()->OnClientEngineJoin(ClientID);
	pThis->Antibot()->OnEngineClientJoin(ClientID, Sevendown);
#if defined(CONF_FAMILY_UNIX)
	pThis->SendConnLoggingCommand(OPEN_SESSION, pThis->m_NetServer.ClientAddr(ClientID));
#endif
	return 0;
}

int CServer::DelClientCallback(int ClientID, const char *pReason, void *pUser)
{
	CServer *pThis = (CServer *)pUser;

	char aAddrStr[NETADDR_MAXSTRSIZE];
	net_addr_str(pThis->m_NetServer.ClientAddr(ClientID), aAddrStr, sizeof(aAddrStr), true);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "client dropped. cid=%d addr=<{%s}> reason='%s'", ClientID, aAddrStr, pReason);
	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	// notify the mod about the drop
	if(pThis->m_aClients[ClientID].m_State >= CClient::STATE_READY)
	{
		pThis->m_aClients[ClientID].m_Quitting = true;
		pThis->GameServer()->OnClientDrop(ClientID, pReason);
	}

	pThis->m_aClients[ClientID].m_Snapshots.PurgeAll();
	pThis->m_aClients[ClientID].ResetContent();
	pThis->GameServer()->OnClientEngineDrop(ClientID, pReason);
	pThis->Antibot()->OnEngineClientDrop(ClientID, pReason);
#if defined(CONF_FAMILY_UNIX)
	pThis->SendConnLoggingCommand(CLOSE_SESSION, pThis->m_NetServer.ClientAddr(ClientID));
#endif
	return 0;
}

bool CServer::ClientCanCloseCallback(int ClientID, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	return !pThis->m_aClients[ClientID].m_DesignChange;
}

void CServer::GetMapInfo(char *pMapName, int MapNameSize, int *pMapSize, SHA256_DIGEST *pMapSha256, int *pMapCrc)
{
	str_copy(pMapName, GetMapName(), MapNameSize);
	*pMapSize = m_CurrentMapSize;
	*pMapSha256 = m_CurrentMapSha256;
	*pMapCrc = m_CurrentMapCrc;
}

void CServer::SendRconType(int ClientID, bool UsernameReq)
{
	CMsgPacker Msg(NETMSG_RCONTYPE, true);
	Msg.AddInt(UsernameReq);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CServer::SendCapabilities(int ClientID)
{
	CMsgPacker Msg(NETMSG_CAPABILITIES, true);
	Msg.AddInt(SERVERCAP_CURVERSION); // version
	int Flags = SERVERCAPFLAG_DDNET | SERVERCAPFLAG_CHATTIMEOUTCODE | SERVERCAPFLAG_ANYPLAYERFLAG | SERVERCAPFLAG_PINGEX | SERVERCAPFLAG_SYNCWEAPONINPUT;
	if (Config()->m_SvAllowDummy)
		Flags |= SERVERCAPFLAG_ALLOWDUMMY;
	Msg.AddInt(Flags); // flags
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CServer::LoadUpdateFakeMap()
{
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "%s.map", Config()->m_FakeMapFile);
	modify_file_crc32(aBuf, m_FakeMapSize-4, m_FakeMapCrc, false);

	// re-read file
	IOHANDLE File = Storage()->OpenFile(aBuf, IOFLAG_READ, IStorage::TYPE_ALL);
	if (File)
	{
		m_FakeMapSize = (unsigned int)io_length(File);
		if(m_pFakeMapData)
			mem_free(m_pFakeMapData);
		m_pFakeMapData = (unsigned char *)mem_alloc(m_FakeMapSize, 1);
		io_read(File, m_pFakeMapData, m_FakeMapSize);
		io_close(File);
	}
}

void CServer::SendFakeMap(int ClientID)
{
	CMsgPacker Msg(NETMSG_MAP_CHANGE, true);
	Msg.AddString(Config()->m_FakeMapName, 0);
	Msg.AddInt(m_FakeMapCrc);
	Msg.AddInt(m_FakeMapSize);
	SendMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID);

	m_aClients[ClientID].m_MapChunk = 0;
}

void CServer::SendMapData(int ClientID, int Chunk, bool FakeMap)
{
	unsigned int ChunkSize = 1024-128;
	unsigned int Offset = Chunk * ChunkSize;
	int Last = 0;

	unsigned int MapSize = m_CurrentMapSize;
	unsigned char *pMapData = m_pCurrentMapData;
	unsigned int MapCrc = m_CurrentMapCrc;

	int Design = m_aClients[ClientID].m_CurrentMapDesign;
	if (FakeMap)
	{
		MapSize = m_FakeMapSize;
		pMapData = m_pFakeMapData;
		MapCrc = m_FakeMapCrc;
	}
	else if (m_aClients[ClientID].m_DesignChange && Design != -1)
	{
		MapSize = m_aMapDesign[Design].m_Size;
		pMapData = m_aMapDesign[Design].m_pData;
		MapCrc = m_aMapDesign[Design].m_Crc;
	}

	// drop faulty map data requests
	if(Chunk < 0 || Offset > MapSize)
		return;

	if(Offset+ChunkSize >= MapSize)
	{
		ChunkSize = MapSize-Offset;
		Last = 1;
	}

	CMsgPacker Msg(NETMSG_MAP_DATA, true);
	Msg.AddInt(Last);
	Msg.AddInt(MapCrc);
	Msg.AddInt(Chunk);
	Msg.AddInt(ChunkSize);
	Msg.AddRaw(&pMapData[Offset], ChunkSize);
	SendMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID);
}

void CServer::SendMap(int ClientID)
{
	{
		CMsgPacker Msg(NETMSG_MAP_DETAILS, true);
		Msg.AddString(GetMapName(), 0);
		Msg.AddRaw(&m_CurrentMapSha256.data, sizeof(m_CurrentMapSha256.data));
		Msg.AddInt(m_CurrentMapCrc);
		Msg.AddInt(m_CurrentMapSize);
		Msg.AddString(GetHttpsMapURL(), 0);
		SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
	}
	{
		CMsgPacker Msg(NETMSG_MAP_CHANGE, true);
		Msg.AddString(GetMapName(), 0);
		Msg.AddInt(m_CurrentMapCrc);
		Msg.AddInt(m_CurrentMapSize);
		if (!m_aClients[ClientID].m_Sevendown)
		{
			Msg.AddInt(m_MapChunksPerRequest);
			Msg.AddInt(MAP_CHUNK_SIZE);
			Msg.AddRaw(&m_CurrentMapSha256, sizeof(m_CurrentMapSha256));
		}
		SendMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID);
	}

	m_aClients[ClientID].m_MapChunk = 0;
}

void CServer::SendConnectionReady(int ClientID)
{
	CMsgPacker Msg(NETMSG_CON_READY, true);
	SendMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID);
}

void CServer::SendRconLine(int ClientID, const char *pLine)
{
	CMsgPacker Msg(NETMSG_RCON_LINE, true);
	Msg.AddString(pLine, 512);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CServer::SendRconLineAuthed(const char *pLine, void *pUser, bool Highlighted)
{
	CServer *pThis = (CServer *)pUser;
	static volatile int ReentryGuard = 0;
	int i;

	if(ReentryGuard) return;
	ReentryGuard++;

	const char *pStart = str_find(pLine, "<{");
	const char *pEnd = pStart == NULL ? NULL : str_find(pStart + 2, "}>");
	const char *pLineWithoutIps;
	char aLine[512];
	char aLineWithoutIps[512];
	aLine[0] = '\0';
	aLineWithoutIps[0] = '\0';

	if(pStart == NULL || pEnd == NULL)
	{
		pLineWithoutIps = pLine;
	}
	else
	{
		str_append(aLine, pLine, pStart - pLine + 1);
		str_append(aLine, pStart + 2, pStart - pLine + pEnd - pStart - 1);
		str_append(aLine, pEnd + 2, sizeof(aLine));

		str_append(aLineWithoutIps, pLine, pStart - pLine + 1);
		str_append(aLineWithoutIps, "XXX", sizeof(aLineWithoutIps));
		str_append(aLineWithoutIps, pEnd + 2, sizeof(aLineWithoutIps));

		pLine = aLine;
		pLineWithoutIps = aLineWithoutIps;
	}

	for(i = 0; i < MAX_CLIENTS; i++)
	{
		if(pThis->m_aClients[i].m_State != CClient::STATE_EMPTY && pThis->m_aClients[i].m_Authed >= pThis->m_RconAuthLevel && (pThis->m_RconRestrict == -1 || pThis->m_RconRestrict == i))
			pThis->SendRconLine(i, pThis->m_aClients[i].m_ShowIps ? pLine : pLineWithoutIps);
	}

	ReentryGuard--;
}

void CServer::SendRconCmdAdd(const IConsole::CCommandInfo *pCommandInfo, int ClientID)
{
	CMsgPacker Msg(NETMSG_RCON_CMD_ADD, true);
	Msg.AddString(pCommandInfo->m_pName, IConsole::TEMPCMD_NAME_LENGTH);
	Msg.AddString(pCommandInfo->m_pHelp, IConsole::TEMPCMD_HELP_LENGTH);
	Msg.AddString(pCommandInfo->m_pParams, IConsole::TEMPCMD_PARAMS_LENGTH);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CServer::SendRconCmdRem(const IConsole::CCommandInfo *pCommandInfo, int ClientID)
{
	CMsgPacker Msg(NETMSG_RCON_CMD_REM, true);
	Msg.AddString(pCommandInfo->m_pName, 256);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CServer::UpdateClientRconCommands()
{
	for(int ClientID = Tick() % MAX_RCONCMD_RATIO; ClientID < MAX_CLIENTS; ClientID += MAX_RCONCMD_RATIO)
	{
		if(m_aClients[ClientID].m_State != CClient::STATE_EMPTY && m_aClients[ClientID].m_Authed)
		{
			int ConsoleAccessLevel = m_aClients[ClientID].m_Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : m_aClients[ClientID].m_Authed == AUTHED_MOD ? IConsole::ACCESS_LEVEL_MOD : IConsole::ACCESS_LEVEL_HELPER;
			for(int i = 0; i < MAX_RCONCMD_SEND && m_aClients[ClientID].m_pRconCmdToSend; ++i)
			{
				SendRconCmdAdd(m_aClients[ClientID].m_pRconCmdToSend, ClientID);
				m_aClients[ClientID].m_pRconCmdToSend = m_aClients[ClientID].m_pRconCmdToSend->NextCommandInfo(ConsoleAccessLevel, CFGFLAG_SERVER);
			}
		}
	}
}

void CServer::SendMapListEntryAdd(const CMapListEntry *pMapListEntry, int ClientID)
{
	CMsgPacker Msg(NETMSG_MAPLIST_ENTRY_ADD, true);
	Msg.AddString(pMapListEntry->m_aName, 256);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CServer::SendMapListEntryRem(const CMapListEntry *pMapListEntry, int ClientID)
{
	CMsgPacker Msg(NETMSG_MAPLIST_ENTRY_REM, true);
	Msg.AddString(pMapListEntry->m_aName, 256);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}


void CServer::UpdateClientMapListEntries()
{
	for(int ClientID = Tick() % MAX_RCONCMD_RATIO; ClientID < MAX_CLIENTS; ClientID += MAX_RCONCMD_RATIO)
	{
		if(m_aClients[ClientID].m_State != CClient::STATE_EMPTY && m_aClients[ClientID].m_Authed)
		{
			for(int i = 0; i < MAX_MAPLISTENTRY_SEND && m_aClients[ClientID].m_pMapListEntryToSend; ++i)
			{
				SendMapListEntryAdd(m_aClients[ClientID].m_pMapListEntryToSend, ClientID);
				m_aClients[ClientID].m_pMapListEntryToSend = m_aClients[ClientID].m_pMapListEntryToSend->m_pNext;
			}
		}
	}
}

static inline int MsgFromSevendown(int Msg, bool System)
{
	if(System)
	{
		if(Msg == NETMSG_INFO)
			;
		else if(Msg >= 14 && Msg <= 24)
			Msg = NETMSG_READY + Msg - 14;
		else if(Msg < OFFSET_UUID)
			return -1;
	}
	else
	{
		if(Msg >= 17 && Msg <= 20)
			Msg = NETMSGTYPE_CL_SAY + Msg - 17;
		else if (Msg == 21)
			Msg = NETMSGTYPE_CL_SKINCHANGE;
		else if(Msg == 22)
			Msg = NETMSGTYPE_CL_KILL;
		else if(Msg >= 23 && Msg <= 25)
			Msg = NETMSGTYPE_CL_EMOTICON + Msg - 23;
		else if(Msg < OFFSET_UUID)
			return -1;
	}

	return Msg;
}

void CServer::ProcessClientPacket(CNetChunk *pPacket)
{
	int ClientID = pPacket->m_ClientID;
	CUnpacker Unpacker;
	Unpacker.Reset(pPacket->m_pData, pPacket->m_DataSize);
	CMsgPacker Packer(NETMSG_EX);

	// unpack msgid and system flag
	int Msg;
	bool Sys;
	CUuid Uuid;

	int Result = UnpackMessageID(&Msg, &Sys, &Uuid, &Unpacker, &Packer, m_pConfig->m_Debug);
	if(Result == UNPACKMESSAGE_ERROR)
	{
		return;
	}

	if(m_aClients[ClientID].m_Sevendown && (Msg = MsgFromSevendown(Msg, Sys)) < 0)
	{
		return;
	}

	if(Config()->m_SvNetlimit && Msg != NETMSG_REQUEST_MAP_DATA)
	{
		int64 Now = time_get();
		int64 Diff = Now - m_aClients[ClientID].m_TrafficSince;
		float Alpha = Config()->m_SvNetlimitAlpha / 100.0f;
		float Limit = (float)Config()->m_SvNetlimit * 1024 / time_freq();

		if (m_aClients[ClientID].m_Traffic > Limit)
		{
			m_NetServer.NetBan()->BanAddr(&pPacket->m_Address, 600, "Stressing network");
			return;
		}
		if (Diff > 100)
		{
			m_aClients[ClientID].m_Traffic = (Alpha * ((float)pPacket->m_DataSize / Diff)) + (1.0f - Alpha) * m_aClients[ClientID].m_Traffic;
			m_aClients[ClientID].m_TrafficSince = Now;
		}
	}

	if(Result == UNPACKMESSAGE_ANSWER)
	{
		SendMsg(&Packer, MSGFLAG_VITAL, ClientID);
	}

	if(Sys)
	{
		// system message
		if(Msg == NETMSG_CLIENTVER)
		{
			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && (m_aClients[ClientID].m_State == CClient::STATE_PREAUTH || m_aClients[ClientID].m_Rejoining))
			{
				CUuid *pConnectionID = (CUuid *)Unpacker.GetRaw(sizeof(*pConnectionID));
				int DDNetVersion = Unpacker.GetInt();
				const char *pDDNetVersionStr = Unpacker.GetString(CUnpacker::SANITIZE_CC);
				if(Unpacker.Error() || !str_utf8_check(pDDNetVersionStr) || DDNetVersion < 0)
				{
					return;
				}
				m_aClients[ClientID].m_ConnectionID = *pConnectionID;
				m_aClients[ClientID].m_DDNetVersion = DDNetVersion;
				str_copy(m_aClients[ClientID].m_aDDNetVersionStr, pDDNetVersionStr, sizeof(m_aClients[ClientID].m_aDDNetVersionStr));
				m_aClients[ClientID].m_DDNetVersionSettled = true;
				m_aClients[ClientID].m_GotDDNetVersionPacket = true;

				if (!m_aClients[ClientID].m_Rejoining)
				{
					m_aClients[ClientID].m_State = CClient::STATE_AUTH;

					int Dummy = GetDummy(ClientID);
					if (Dummy != -1)
					{
						if (!Config()->m_SvAllowDummy)
						{
							m_NetServer.Drop(ClientID, "Dummy is not allowed");
							return;
						}

						m_aClients[ClientID].m_CurrentMapDesign = m_aClients[Dummy].m_CurrentMapDesign;
						SetLanguage(ClientID, GetLanguage(Dummy));
						m_aClients[ClientID].m_Main = false;
					}
				}
			}
		}
		else if(Msg == NETMSG_INFO)
		{
			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && (m_aClients[ClientID].m_State == CClient::STATE_PREAUTH || m_aClients[ClientID].m_State == CClient::STATE_AUTH))
			{
				const char *pVersion = Unpacker.GetString(CUnpacker::SANITIZE_CC);
				if((!m_aClients[ClientID].m_Sevendown && str_comp(pVersion, GameServer()->NetVersion()) != 0) || (m_aClients[ClientID].m_Sevendown && str_comp(pVersion, GameServer()->NetVersionSevendown()) != 0))
				{
					// wrong version
					char aReason[256];
					str_format(aReason, sizeof(aReason), "Wrong version. Server is running '%s' and client '%s'", GameServer()->NetVersion(), pVersion);
					m_NetServer.Drop(ClientID, aReason);
					return;
				}

				const char *pPassword = Unpacker.GetString(CUnpacker::SANITIZE_CC);
				if(Config()->m_Password[0] != 0 && str_comp(Config()->m_Password, pPassword) != 0)
				{
					// wrong password
					m_NetServer.Drop(ClientID, "Wrong password");
					return;
				}

				if (m_aClients[ClientID].m_Sevendown)
				{
					if (Config()->m_SvDropOldClients && !m_aClients[ClientID].m_GotDDNetVersionPacket)
					{
						m_NetServer.Drop(ClientID, "Version too old, please update");
						return;
					}
				}
				else
				{
					m_aClients[ClientID].m_Version = Unpacker.GetInt();
				}

				SendRconType(ClientID, m_AuthManager.NumNonDefaultKeys() > 0);
				SendCapabilities(ClientID);
				if (m_aClients[ClientID].m_Sevendown && m_FakeMapSize && Config()->m_FakeMapName[0] && Config()->m_FakeMapCrc[0] && GetDummy(ClientID) == -1)
				{
					m_aClients[ClientID].m_State = CClient::STATE_FAKE_MAP;
					SendFakeMap(ClientID);
				}
				else
				{
					m_aClients[ClientID].m_State = CClient::STATE_CONNECTING;
					if (Config()->m_SvDefaultMapDesign[0] && GetDummy(ClientID) == -1)
						ChangeMapDesign(ClientID, Config()->m_SvDefaultMapDesign);
					else
						SendMap(ClientID);
				}
			}
		}
		else if(Msg == NETMSG_REQUEST_MAP_DATA)
		{
			bool SendingFakeMap = m_aClients[ClientID].m_State == CClient::STATE_FAKE_MAP;
			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && (m_aClients[ClientID].m_DesignChange || SendingFakeMap || m_aClients[ClientID].m_State == CClient::STATE_CONNECTING || m_aClients[ClientID].m_State == CClient::STATE_CONNECTING_AS_SPEC))
			{
				if (m_aClients[ClientID].m_Sevendown)
				{
					int Chunk = Unpacker.GetInt();
					if (Chunk != m_aClients[ClientID].m_MapChunk)
					{
						SendMapData(ClientID, Chunk, SendingFakeMap);
						return;
					}

					if (Chunk == 0)
					{
						for (int i = 0; i < Config()->m_SvMapWindow; i++)
							SendMapData(ClientID, i, SendingFakeMap);
					}
					SendMapData(ClientID, Config()->m_SvMapWindow + m_aClients[ClientID].m_MapChunk, SendingFakeMap);
					m_aClients[ClientID].m_MapChunk++;
					return;
				}

				int ChunkSize = MAP_CHUNK_SIZE;

				// send map chunks
				for(int i = 0; i < m_MapChunksPerRequest && m_aClients[ClientID].m_MapChunk >= 0; ++i)
				{
					int Chunk = m_aClients[ClientID].m_MapChunk;
					unsigned int Offset = Chunk * ChunkSize;

					unsigned char *pMapData = m_pCurrentMapData;
					unsigned int Size = m_CurrentMapSize;

					int Design = m_aClients[ClientID].m_CurrentMapDesign;
					if (m_aClients[ClientID].m_DesignChange && Design != -1)
					{
						pMapData = m_aMapDesign[Design].m_pData;
						Size = m_aMapDesign[Design].m_Size;
					}

					// check for last part
					if(Offset+ChunkSize >= Size)
					{
						ChunkSize = Size-Offset;
						m_aClients[ClientID].m_MapChunk = -1;
					}
					else
						m_aClients[ClientID].m_MapChunk++;

					CMsgPacker Msg(NETMSG_MAP_DATA, true);
					Msg.AddRaw(&pMapData[Offset], ChunkSize);
					SendMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID);

					if(Config()->m_Debug)
					{
						char aBuf[64];
						str_format(aBuf, sizeof(aBuf), "sending chunk %d with size %d", Chunk, ChunkSize);
						Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
					}
				}
			}
		}
		else if(Msg == NETMSG_READY)
		{
			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0)
			{
				SendConnectionReady(ClientID);

				if (m_aClients[ClientID].m_State == CClient::STATE_CONNECTING || m_aClients[ClientID].m_State == CClient::STATE_CONNECTING_AS_SPEC)
				{
					char aAddrStr[NETADDR_MAXSTRSIZE];
					net_addr_str(m_NetServer.ClientAddr(ClientID), aAddrStr, sizeof(aAddrStr), true);

					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "player is ready. ClientID=%d addr=<{%s}>", ClientID, aAddrStr);
					Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);

					bool ConnectAsSpec = m_aClients[ClientID].m_State == CClient::STATE_CONNECTING_AS_SPEC;
					m_aClients[ClientID].m_State = CClient::STATE_READY;
					GameServer()->OnClientConnected(ClientID, ConnectAsSpec);
				}

				if (m_aClients[ClientID].m_DesignChange)
				{
					m_aClients[ClientID].m_DesignChange = false;

					// When default map design is specified and we just joined, don't call this function
					if (m_aClients[ClientID].m_State == CClient::STATE_INGAME)
						GameServer()->MapDesignChangeDone(ClientID);

					int Dummy = GetDummy(ClientID);
					if (Dummy != -1)
						m_aClients[Dummy].m_DesignChange = false;
				}
				else if (m_aClients[ClientID].m_Rejoining)
				{
					m_aClients[ClientID].m_Rejoining = false;
					GameServer()->OnClientRejoin(ClientID);
				}
				else if (m_aClients[ClientID].m_State == CClient::STATE_FAKE_MAP)
				{
					m_aClients[ClientID].m_State = CClient::STATE_CONNECTING;
					SendMap(ClientID);
				}
			}
		}
		else if(Msg == NETMSG_ENTERGAME)
		{
			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientID].m_State == CClient::STATE_READY && GameServer()->IsClientReady(ClientID))
			{
				char aAddrStr[NETADDR_MAXSTRSIZE];
				net_addr_str(m_NetServer.ClientAddr(ClientID), aAddrStr, sizeof(aAddrStr), true);

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "player has entered the game. ClientID=%d addr=<{%s}>", ClientID, aAddrStr);
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
				m_aClients[ClientID].m_State = CClient::STATE_INGAME;
				SendServerInfo(ClientID);
				GameServer()->OnClientEnter(ClientID);
			}
		}
		else if(Msg == NETMSG_INPUT)
		{
			CClient::CInput *pInput;
			int64 TagTime;
			int64 Now = time_get();

			int LastAckedSnapshot = Unpacker.GetInt();
			int IntendedTick = Unpacker.GetInt();
			int Size = Unpacker.GetInt();

			// check for errors
			if(Unpacker.Error() || Size/4 > MAX_INPUT_SIZE)
				return;

			if (m_aClients[ClientID].m_DDNetVersion <= VERSION_DDNET_INTENDED_TICK)
			{
				// This does also apply when dummy hammer or dummy copy moves is activated, the "idle" dummy will always send the intended tick of before the swap
				m_aClients[ClientID].m_IdleDummy = (m_aClients[ClientID].m_LastIntendedTick == IntendedTick);
				// During dummy hammerfly inputs are not sent except on the hammer thus leading to big gaps inbetween the last lastackedsnapshots
				m_aClients[ClientID].m_DummyHammer = (m_aClients[ClientID].m_IdleDummy && LastAckedSnapshot > m_aClients[ClientID].m_LastAckedSnapshot + 20);
				// dummy copy moves could be detected aswell by checking whether its the idle dummy and then counting inputs a bit, bcs they get sent twice as often with it acitavted

				m_aClients[ClientID].m_LastIntendedTick = IntendedTick;
			}
			else
			{
				m_aClients[ClientID].m_CurrentIdleTrackPos++;
				m_aClients[ClientID].m_CurrentIdleTrackPos %= 5;

				// Idle dummy sends input just half as often as the active player
				m_aClients[ClientID].m_aIdleDummyTrack[m_aClients[ClientID].m_CurrentIdleTrackPos] = (LastAckedSnapshot - m_aClients[ClientID].m_LastAckedSnapshot > 2);

				int Count = 0;
				for (int i = 0; i < 5; i++)
					if (m_aClients[ClientID].m_aIdleDummyTrack[i])
						Count++;

				// 4/5 inputs should be fine to prove this one is idle, cuz also the idle dummy can have a gap of > 2 sometimes
				// NOTE: this is not 100% reliable all the time + can be dodged by using dummy control on the client side. But in most cases it should be okay-ish
				m_aClients[ClientID].m_IdleDummy = Count >= 4;
			}

			m_aClients[ClientID].m_LastAckedSnapshot = LastAckedSnapshot;
			if(m_aClients[ClientID].m_LastAckedSnapshot > 0)
				m_aClients[ClientID].m_SnapRate = CClient::SNAPRATE_FULL;

			// add message to report the input timing
			// skip packets that are old
			if(IntendedTick > m_aClients[ClientID].m_LastInputTick)
			{
				int TimeLeft = ((TickStartTime(IntendedTick)-Now)*1000) / time_freq();

				CMsgPacker Msg(NETMSG_INPUTTIMING, true);
				Msg.AddInt(IntendedTick);
				Msg.AddInt(TimeLeft);
				SendMsg(&Msg, 0, ClientID);
			}

			m_aClients[ClientID].m_LastInputTick = IntendedTick;

			pInput = &m_aClients[ClientID].m_aInputs[m_aClients[ClientID].m_CurrentInput];

			if(IntendedTick <= Tick())
				IntendedTick = Tick()+1;

			pInput->m_GameTick = IntendedTick;

			for(int i = 0; i < Size/4; i++)
				pInput->m_aData[i] = Unpacker.GetInt();

			int PingCorrection = clamp(Unpacker.GetInt(), 0, 50);
			if(m_aClients[ClientID].m_Snapshots.Get(m_aClients[ClientID].m_LastAckedSnapshot, &TagTime, 0, 0) >= 0)
			{
				m_aClients[ClientID].m_Latency = (int)(((Now-TagTime)*1000)/time_freq());
				m_aClients[ClientID].m_Latency = max(0, m_aClients[ClientID].m_Latency - PingCorrection);
			}

			mem_copy(m_aClients[ClientID].m_LatestInput.m_aData, pInput->m_aData, MAX_INPUT_SIZE*sizeof(int));

			m_aClients[ClientID].m_CurrentInput++;
			m_aClients[ClientID].m_CurrentInput %= 200;

			// new way of checking for hammerfly since dummy intended tick got fixed
			if (m_aClients[ClientID].m_DDNetVersion > VERSION_DDNET_INTENDED_TICK)
			{
				CNetObj_PlayerInput *pPlayerInput = (CNetObj_PlayerInput *)m_aClients[ClientID].m_LatestInput.m_aData;
				if (m_aClients[ClientID].m_HammerflyMarked)
				{
					m_aClients[ClientID].m_DummyHammer = (pPlayerInput->m_Fire == m_aClients[ClientID].m_LastFire + 2);
					if (!m_aClients[ClientID].m_DummyHammer)
						m_aClients[ClientID].m_HammerflyMarked = false;
				}
				else if (pPlayerInput->m_WantedWeapon == WEAPON_HAMMER + 1 && (pPlayerInput->m_Fire&1) != 0 && pPlayerInput->m_Fire != m_aClients[ClientID].m_LastFire)
				{
					m_aClients[ClientID].m_HammerflyMarked = true;
				}

				m_aClients[ClientID].m_LastFire = pPlayerInput->m_Fire;
			}

			// call the mod with the fresh input data
			if(m_aClients[ClientID].m_State == CClient::STATE_INGAME)
				GameServer()->OnClientDirectInput(ClientID, m_aClients[ClientID].m_LatestInput.m_aData);
		}
		else if(Msg == NETMSG_RCON_CMD)
		{
			const char *pCmd = Unpacker.GetString();
			if(!str_utf8_check(pCmd))
			{
				return;
			}
			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Error() == 0 && m_aClients[ClientID].m_Authed)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "ClientID=%d rcon='%s'", ClientID, pCmd);
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);
				m_RconClientID = ClientID;
				m_RconAuthLevel = m_aClients[ClientID].m_Authed;
				Console()->SetAccessLevel(m_aClients[ClientID].m_Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : m_aClients[ClientID].m_Authed == AUTHED_MOD ? IConsole::ACCESS_LEVEL_MOD : m_aClients[ClientID].m_Authed == AUTHED_HELPER ? IConsole::ACCESS_LEVEL_HELPER : IConsole::ACCESS_LEVEL_USER);
				Console()->ExecuteLineFlag(pCmd, CFGFLAG_SERVER, ClientID);
				Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
				m_RconClientID = IServer::RCON_CID_SERV;
				m_RconAuthLevel = AUTHED_ADMIN;
			}
		}
		else if(Msg == NETMSG_RCON_AUTH)
		{
			const char *pName = "";
			if (m_aClients[ClientID].m_Sevendown)
				pName = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			const char *pAuth = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			if(!str_utf8_check(pName) || !str_utf8_check(pAuth))
				return;

			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Error() == 0)
			{
				int AuthLevel = -1;
				int KeySlot = -1;

				// oy removed the usernames...
				// use "name:password" format until we establish new extended netmsg
				char aName[64];
				str_copy(aName, pName, sizeof(aName));
				const char *pPw = pAuth;

				if (!m_aClients[ClientID].m_Sevendown)
				{
					const char *pDelim = str_find(pAuth, ":");
					if(!pDelim)
						pPw = pAuth;
					else
					{
						str_copy(aName, pAuth, min((unsigned long)sizeof(aName), (unsigned long)(pDelim - pAuth + 1)));
						pPw = pDelim + 1;
					}
				}

				if(!aName[0])
				{
					if(m_AuthManager.CheckKey((KeySlot = m_AuthManager.DefaultKey(AUTHED_ADMIN)), pPw))
						AuthLevel = AUTHED_ADMIN;
					else if(m_AuthManager.CheckKey((KeySlot = m_AuthManager.DefaultKey(AUTHED_MOD)), pPw))
						AuthLevel = AUTHED_MOD;
					else if(m_AuthManager.CheckKey((KeySlot = m_AuthManager.DefaultKey(AUTHED_HELPER)), pPw))
						AuthLevel = AUTHED_HELPER;
				}
				else
				{
					KeySlot = m_AuthManager.FindKey(aName);
					if(m_AuthManager.CheckKey(KeySlot, pPw))
						AuthLevel = m_AuthManager.KeyLevel(KeySlot);
				}

				if(AuthLevel != -1)
				{
					if(m_aClients[ClientID].m_Authed != AuthLevel)
					{
						CMsgPacker Msg(NETMSG_RCON_AUTH_ON, true);
						if (m_aClients[ClientID].m_Sevendown)
						{
							Msg.AddInt(1);
							Msg.AddInt(1);
						}
						SendMsg(&Msg, MSGFLAG_VITAL, ClientID);

						m_aClients[ClientID].m_Authed = AuthLevel; // Keeping m_Authed around is unwise...
						m_aClients[ClientID].m_AuthKey = KeySlot;
						// AUTHED_ADMIN - AuthLevel gets the proper IConsole::ACCESS_LEVEL_<x>
						// TODO: Check if it still does
						m_aClients[ClientID].m_pRconCmdToSend = Console()->FirstCommandInfo(AUTHED_ADMIN - AuthLevel, CFGFLAG_SERVER);

						// TODO: Check if we want to send all maps to all rcon clients
						if(m_aClients[ClientID].m_Version >= MIN_MAPLIST_CLIENTVERSION && !m_aClients[ClientID].m_Sevendown)
							m_aClients[ClientID].m_pMapListEntryToSend = m_pFirstMapEntry;

						GameServer()->OnClientAuth(ClientID, AuthLevel);

						char aBuf[256];
						const char *pIdent = m_AuthManager.KeyIdent(KeySlot);
						char aAddrStr[NETADDR_MAXSTRSIZE];
						net_addr_str(m_NetServer.ClientAddr(ClientID), aAddrStr, sizeof(aAddrStr), true);
						switch (AuthLevel)
						{
							case AUTHED_ADMIN:
							{
								SendRconLine(ClientID, "Admin authentication successful. Full remote console access granted.");
								str_format(aBuf, sizeof(aBuf), "ClientID=%d addr=<{%s}> authed with key=%s (admin)", ClientID, aAddrStr, pIdent);
								break;
							}
							case AUTHED_MOD:
							{
								SendRconLine(ClientID, "Moderator authentication successful. Limited remote console access granted.");
								str_format(aBuf, sizeof(aBuf), "ClientID=%d addr=<{%s}> authed with key=%s (moderator)", ClientID, aAddrStr, pIdent);
								break;
							}
							case AUTHED_HELPER:
							{
								SendRconLine(ClientID, "Helper authentication successful. Limited remote console access granted.");
								str_format(aBuf, sizeof(aBuf), "ClientID=%d addr=<{%s}> authed with key=%s (helper)", ClientID, aAddrStr, pIdent);
								break;
							}
						}
						Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
					}
				}
				else if(Config()->m_SvRconMaxTries && m_ServerBan.IsBannable(m_NetServer.ClientAddr(ClientID)))
				{
					m_aClients[ClientID].m_AuthTries++;
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), "Wrong password %d/%d.", m_aClients[ClientID].m_AuthTries, Config()->m_SvRconMaxTries);
					SendRconLine(ClientID, aBuf);
					if(m_aClients[ClientID].m_AuthTries >= Config()->m_SvRconMaxTries)
					{
						if(!Config()->m_SvRconBantime)
							m_NetServer.Drop(ClientID, "Too many remote console authentication tries");
						else
							m_ServerBan.BanAddr(m_NetServer.ClientAddr(ClientID), Config()->m_SvRconBantime*60, "Too many remote console authentication tries");
					}
				}
				else
				{
					SendRconLine(ClientID, "Wrong password.");
				}
			}
		}
		else if(Msg == NETMSG_PING)
		{
			CMsgPacker Msg(NETMSG_PING_REPLY, true);
			SendMsg(&Msg, 0, ClientID);
		}
		else if(Msg == NETMSG_PINGEX)
		{
			CUuid *pID = (CUuid *)Unpacker.GetRaw(sizeof(*pID));
			if(Unpacker.Error())
			{
				return;
			}
			CMsgPacker Msg(NETMSG_PONGEX, true);
			Msg.AddRaw(pID, sizeof(*pID));
			SendMsg(&Msg, MSGFLAG_FLUSH, ClientID);
		}
		else
		{
			if(Config()->m_Debug)
			{
				char aHex[] = "0123456789ABCDEF";
				char aBuf[512];

				for(int b = 0; b < pPacket->m_DataSize && b < 32; b++)
				{
					aBuf[b*3] = aHex[((const unsigned char *)pPacket->m_pData)[b]>>4];
					aBuf[b*3+1] = aHex[((const unsigned char *)pPacket->m_pData)[b]&0xf];
					aBuf[b*3+2] = ' ';
					aBuf[b*3+3] = 0;
				}

				char aBufMsg[256];
				str_format(aBufMsg, sizeof(aBufMsg), "strange message ClientID=%d msg=%d data_size=%d", ClientID, Msg, pPacket->m_DataSize);
				Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBufMsg);
				Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
			}
		}
	}
	else
	{
		// game message
		if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientID].m_State >= CClient::STATE_READY)
			GameServer()->OnMessage(Msg, &Unpacker, ClientID);
	}
}

void CServer::FillAntibot(CAntibotRoundData *pData)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CAntibotPlayerData *pPlayer = &pData->m_aPlayers[i];
		net_addr_str(m_NetServer.ClientAddr(i), pPlayer->m_aAddress, sizeof(pPlayer->m_aAddress), true);
	}
}

void CServer::GenerateServerInfo(CPacker *pPacker, int Token, int Socket)
{
	// count the players
	int PlayerCount = 0, ClientCount = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			if(GameServer()->IsClientPlayer(i))
				PlayerCount++;

			ClientCount++;
		}
	}

	bool DoubleInfo = IsDoubleInfo() && (Socket == SOCKET_MAIN || Socket == SOCKET_TWO);

	if(Token != -1)
	{
		pPacker->Reset();
		pPacker->AddRaw(SERVERBROWSE_INFO, sizeof(SERVERBROWSE_INFO));
		pPacker->AddInt(Token);
	}

	pPacker->AddString(GameServer()->Version(), 32);
	
	if(Config()->m_SvMaxClients <= VANILLA_MAX_CLIENTS)
	{
		pPacker->AddString(Config()->m_SvName, 64);
	}
	else
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s [%d/%d]", Config()->m_SvName, ClientCount, Config()->m_SvMaxClients);
		if (DoubleInfo)
		{
			int Count = Socket == SOCKET_MAIN ? 1 : Socket == SOCKET_TWO ? 2 : 0;
			if (Count)
			{
				char aDouble[8];
				str_format(aDouble, sizeof(aDouble), " [%d/2]", Count);
				str_append(aBuf, aDouble, sizeof(aBuf));
			}
		}
		pPacker->AddString(aBuf, 64);
	}

	pPacker->AddString(Config()->m_SvHostname, 128);
	pPacker->AddString(GetMapName(), 32);

	// gametype
	pPacker->AddString(GameServer()->GameType(), 16);

	// flags
	int Flags = 0;
	if(Config()->m_Password[0])  // password set
		Flags |= SERVERINFO_FLAG_PASSWORD;
	if(Config()->m_SvDefaultScoreMode == 0) // F-DDrace // CPlayer::SCORE_TIME means 0
		Flags |= SERVERINFO_FLAG_TIMESCORE;
	pPacker->AddInt(Flags);

	int MaxClients = Config()->m_SvMaxClients;
	bool FillFirst = ClientCount >= MaxClients;
	if (DoubleInfo)
	{
		int Diff = FillFirst ? VANILLA_MAX_CLIENTS : VANILLA_MAX_CLIENTS-1;

		if (Socket == SOCKET_MAIN)
		{
			ClientCount = Diff;
		}
		else if (Socket == SOCKET_TWO)
		{
			ClientCount -= Diff;
			MaxClients -= VANILLA_MAX_CLIENTS;
		}
	}

	ClientCount = min(ClientCount, (int)VANILLA_MAX_CLIENTS);
	PlayerCount = min(PlayerCount, ClientCount);
	MaxClients = min(MaxClients, (int)VANILLA_MAX_CLIENTS);
	int PlayerSlots = min(Config()->m_SvPlayerSlots, MaxClients);
	PlayerSlots = max(PlayerCount, PlayerSlots);

	pPacker->AddInt(Config()->m_SvSkillLevel);	// server skill level
	pPacker->AddInt(PlayerCount); // num players
	pPacker->AddInt(PlayerSlots); // max players
	pPacker->AddInt(ClientCount); // num clients
	pPacker->AddInt(max(ClientCount, MaxClients)); // max clients

	if(Token != -1)
	{
		#define SENDPLAYER(i) do \
		{ \
			if (Sent >= ClientCount) \
				break; \
			if(m_aClients[i].m_State != CClient::STATE_EMPTY) \
			{ \
				pPacker->AddString(ClientName(i), 0); /*client name*/ \
				pPacker->AddString(ClientClan(i), 0); /*client clan*/ \
				pPacker->AddInt(m_aClients[i].m_Country); /*client country*/ \
				pPacker->AddInt(m_aClients[i].m_Score); /*client score*/ \
				pPacker->AddInt(m_aClients[i].m_State == CClient::STATE_DUMMY ? 2 : GameServer()->IsClientPlayer(i)?0:1); /*flag spectator=1, bot=2 (player=0)*/ \
				Sent++; \
			} \
		} while(0)

		int Sent = 0;
		if (Socket == SOCKET_TWO)
		{
			for (int i = MAX_CLIENTS-1; i >= 0; i--)
				SENDPLAYER(i);
		}
		else
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
				SENDPLAYER(i);
		}
	}
}

void CServer::SendServerInfoSevendown(const NETADDR *pAddr, int Token, int Socket)
{
	CPacker p;
	char aBuf[128];

	// count the players
	int PlayerCount = 0, ClientCount = 0, DummyCount = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aClients[i].m_State == CClient::STATE_DUMMY)
			DummyCount++;
		else if(m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			if(GameServer()->IsClientPlayer(i))
				PlayerCount++;

			ClientCount++;
		}
	}

	#define ADD_RAW(p, x) (p).AddRaw(x, sizeof(x))
	#define ADD_INT(p, x) do { str_format(aBuf, sizeof(aBuf), "%d", x); (p).AddString(aBuf, 0); } while(0)

	p.Reset();
	ADD_RAW(p, SERVERBROWSE_INFO_EXTENDED);
	ADD_INT(p, Token);
 
	p.AddString(GameServer()->VersionSevendown(), 32);
	p.AddString(Config()->m_SvName, 64);
	p.AddString(GetMapName(), 32);
 
	ADD_INT(p, m_CurrentMapCrc);
	ADD_INT(p, m_CurrentMapSize);
	p.AddString(GetGameTypeServerInfo(), 16);
 
	ADD_INT(p, Config()->m_Password[0] ? SERVERINFO_FLAG_PASSWORD : 0);

	ADD_INT(p, min(PlayerCount, ClientCount));
	ADD_INT(p, max(PlayerCount, Config()->m_SvPlayerSlots-DummyCount));
	ADD_INT(p, ClientCount);
	ADD_INT(p, max(ClientCount, Config()->m_SvMaxClients-DummyCount));

	p.AddString("", 0);

	const void *pPrefix = p.Data();
	int PrefixSize = p.Size();

	CPacker pp;
	CNetChunk Packet;
	int PacketsSent = 0;
	Packet.m_ClientID = -1;
	Packet.m_Address = *pAddr;
	Packet.m_Flags = NETSENDFLAG_CONNLESS;

	#define SEND(size) \
		do \
		{ \
			Packet.m_pData = pp.Data(); \
			Packet.m_DataSize = size; \
			m_NetServer.Send(&Packet, NET_TOKEN_NONE, true, Socket); \
			PacketsSent++; \
		} while(0)

	#define RESET() \
		do \
		{ \
			pp.Reset(); \
			pp.AddRaw(pPrefix, PrefixSize); \
		} while(0)

	RESET();

	pPrefix = SERVERBROWSE_INFO_EXTENDED_MORE;
	PrefixSize = sizeof(SERVERBROWSE_INFO_EXTENDED_MORE);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY && m_aClients[i].m_State != CClient::STATE_DUMMY)
		{
			int PreviousSize = pp.Size();

			pp.AddString(ClientName(i), MAX_NAME_LENGTH);
			pp.AddString(ClientClan(i), MAX_CLAN_LENGTH);

			ADD_INT(pp, m_aClients[i].m_Country);
			// 0 means CPlayer::SCORE_TIME, so the other score modes use scoreformat instead of time format
			// thats why we just send -9999, because it will be displayed as nothing
			// browserscorefix is not required anymore since we have client_score_kind, but we keep it in this, in case it's not fetched via http master and doesnt know about score kind
			int Score = -9999;
			if (Config()->m_SvDefaultScoreMode == 0 && m_aClients[i].m_Score != -1)
				Score = abs(m_aClients[i].m_Score) * -1;
			else if (IsBrowserScoreFix())
				Score = m_aClients[i].m_Score;
			ADD_INT(pp, Score);
			ADD_INT(pp, GameServer()->IsClientPlayer(i) ? 1 : 0);
			pp.AddString("", 0);

			if(pp.Size() >= NET_MAX_PAYLOAD)
			{
				// Retry current player.
				i--;
				SEND(PreviousSize);
				RESET();
				ADD_INT(pp, Token);
				ADD_INT(pp, PacketsSent);
				pp.AddString("", 0);
				continue;
			}
		}
	}

	SEND(pp.Size());
	#undef SEND
	#undef RESET
	#undef ADD_RAW
	#undef ADD_INT
}

const char *CServer::GetGameTypeServerInfo()
{
	static char aBuf[128];
	str_copy(aBuf, GameServer()->GameType(), sizeof(aBuf));
	if (IsBrowserScoreFix())
	{
		// if we want to display normal score we have to get rid of the string "race", thats why we replace it here with a fake letter 'c'
		const char *pGameType = GameServer()->GameType();
		const char *pStart = str_find_nocase(pGameType, "race");
		if (pStart)
		{
			unsigned char aSymbol[] = { 0xD0, 0xB5, 0x00, 0x00 }; // https://de.wiktionary.org/wiki/%D0%B5 // fake 'e'
			char aFakeRace[16];
			str_format(aFakeRace, sizeof(aFakeRace), "rac%s", aSymbol);
			str_append(aBuf, pGameType, pStart - pGameType + 1);
			str_append(aBuf, aFakeRace, sizeof(aBuf));
			str_append(aBuf, pStart + 4, sizeof(aBuf));
		}

		if (Config()->m_SvBrowserScoreFix == 2)
		{
			// this will make the client think gametype is idm leading to the client displaying the gametype in red color, otherwise gametype would be white
			if (str_length(aBuf) + 4 < 16) // only if we have enough space to actually "set" the color. if it gets cut off we can leave it out entirely
				str_append(aBuf, " idm", sizeof(aBuf));
		}
	}
	return aBuf;
}

void CServer::UpdateRegisterServerInfo()
{
	// count the players
	int PlayerCount = 0, ClientCount = 0, DummyCount = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aClients[i].m_State == CClient::STATE_DUMMY)
			DummyCount++;
		else if(m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			if(GameServer()->IsClientPlayer(i))
				PlayerCount++;

			ClientCount++;
		}
	}

	int MaxPlayers = max(PlayerCount, Config()->m_SvPlayerSlots-DummyCount);
	int MaxClients = max(ClientCount, Config()->m_SvMaxClients-DummyCount);
	char aName[256];
	char aGameType[32];
	char aMapName[64];
	char aVersion[64];
	char aScoreKind[32];
	char aMapSha256[SHA256_MAXSTRSIZE];

	sha256_str(m_CurrentMapSha256, aMapSha256, sizeof(aMapSha256));

	char aInfo[16384];
	str_format(aInfo, sizeof(aInfo),
		"{"
		"\"max_clients\":%d,"
		"\"max_players\":%d,"
		"\"passworded\":%s,"
		"\"game_type\":\"%s\","
		"\"name\":\"%s\","
		"\"map\":{"
		"\"name\":\"%s\","
		"\"sha256\":\"%s\","
		"\"size\":%d"
		"},"
		"\"version\":\"%s\","
		"\"client_score_kind\":\"%s\","
		"\"clients\":[",
		MaxClients,
		MaxPlayers,
		JsonBool(Config()->m_Password[0]),
		EscapeJson(aGameType, sizeof(aGameType), GetGameTypeServerInfo()),
		EscapeJson(aName, sizeof(aName), Config()->m_SvName),
		EscapeJson(aMapName, sizeof(aMapName), GetMapName()),
		aMapSha256,
		m_CurrentMapSize,
		EscapeJson(aVersion, sizeof(aVersion), GameServer()->VersionSevendown()),
		EscapeJson(aScoreKind, sizeof(aScoreKind), Config()->m_SvDefaultScoreMode == 0 ? "time" : "points"));

	bool FirstPlayer = true;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY && m_aClients[i].m_State != CClient::STATE_DUMMY)
		{
			// 0 means CPlayer::SCORE_TIME, so the other score modes use scoreformat instead of time format
			int Score = m_aClients[i].m_Score;
			if (Config()->m_SvDefaultScoreMode == 0 && m_aClients[i].m_Score == -1)
				Score = -9999;

			char aCName[32];
			char aCClan[32];

			char aExtraPlayerInfo[512];
			GameServer()->OnUpdatePlayerServerInfo(aExtraPlayerInfo, sizeof(aExtraPlayerInfo), i);

			char aClientInfo[1024];
			str_format(aClientInfo, sizeof(aClientInfo),
				"%s{"
				"\"name\":\"%s\","
				"\"clan\":\"%s\","
				"\"country\":%d,"
				"\"score\":%d,"
				"\"is_player\":%s"
				"%s"
				"}",
				!FirstPlayer ? "," : "",
				EscapeJson(aCName, sizeof(aCName), ClientName(i)),
				EscapeJson(aCClan, sizeof(aCClan), ClientClan(i)),
				m_aClients[i].m_Country,
				Score,
				JsonBool(GameServer()->IsClientPlayer(i)),
				aExtraPlayerInfo);
			str_append(aInfo, aClientInfo, sizeof(aInfo));
			FirstPlayer = false;
		}
	}

	str_append(aInfo, "]}", sizeof(aInfo));

	m_pRegister->OnNewInfo(aInfo);
	if (IsDoubleInfo())
		m_pRegisterTwo->OnNewInfo("{\"type\":\"0.7-placeholder\"}");
}

void CServer::UpdateServerInfo(bool Resend)
{
	if (m_RunServer == UNINITIALIZED)
		return;

	UpdateRegisterServerInfo();
	if (Resend)
		SendServerInfo(-1);

	m_ServerInfoNeedsUpdate = false;
}

void CServer::ExpireServerInfo()
{
	m_ServerInfoNeedsUpdate = true;
}

void CServer::SendServerInfo(int ClientID)
{
	CMsgPacker MsgMain(NETMSG_SERVERINFO, true);
	GenerateServerInfo(&MsgMain, -1, SOCKET_MAIN);
	CMsgPacker MsgTwo(NETMSG_SERVERINFO, true);
	GenerateServerInfo(&MsgTwo, -1, SOCKET_TWO);

	if(ClientID == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_aClients[i].m_State != CClient::STATE_EMPTY)
			{
				if (m_aClients[i].m_Sevendown)
				{
					SendServerInfoSevendown(m_NetServer.ClientAddr(i), -1, m_aClients[i].m_Socket);
				}
				else
				{
					CMsgPacker *pMsg = m_aClients[i].m_Socket == SOCKET_MAIN ? &MsgMain : &MsgTwo;
					SendMsg(pMsg, MSGFLAG_VITAL|MSGFLAG_FLUSH, i);
				}
			}
		}
	}
	else if(ClientID >= 0 && ClientID < MAX_CLIENTS && m_aClients[ClientID].m_State != CClient::STATE_EMPTY)
	{
		if (m_aClients[ClientID].m_Sevendown)
		{
			SendServerInfoSevendown(m_NetServer.ClientAddr(ClientID), -1, m_aClients[ClientID].m_Socket);
		}
		else
		{
			CMsgPacker *pMsg = m_aClients[ClientID].m_Socket == SOCKET_MAIN ? &MsgMain : &MsgTwo;
			SendMsg(pMsg, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID);
		}
	}
}

void CServer::SendRedirectSaveTeeAdd(int Port, const char *pHash)
{
	SendRedirectSaveTeeImpl(true, Port, pHash);
}

void CServer::SendRedirectSaveTeeRemove(int Port, const char *pHash)
{
	SendRedirectSaveTeeImpl(false, Port, pHash);
}

void CServer::SendRedirectSaveTeeImpl(bool Add, int Port, const char *pHash)
{
	if (!pHash[0])
		return;

	CPacker Packer;
	Packer.Reset();
	if (Add)
		Packer.AddRaw(REDIRECT_SAVE_TEE_ADD, sizeof(REDIRECT_SAVE_TEE_ADD));
	else
		Packer.AddRaw(REDIRECT_SAVE_TEE_REMOVE, sizeof(REDIRECT_SAVE_TEE_REMOVE));
	Packer.AddInt(Port);
	Packer.AddString(pHash, SHA256_MAXSTRSIZE);

	CNetChunk Packet;
	Packet.m_ClientID = -1;
	mem_zero(&Packet.m_Address, sizeof(Packet.m_Address));
	Packet.m_Address.type = m_NetServer.NetType(SOCKET_MAIN) | NETTYPE_LINK_BROADCAST;
	Packet.m_Address.port = Port;
	Packet.m_Flags = NETSENDFLAG_CONNLESS;
	Packet.m_DataSize = Packer.Size();
	Packet.m_pData = Packer.Data();
	m_NetServer.Send(&Packet, NET_TOKEN_NONE, true);
}

void CServer::PumpNetwork()
{
	CNetChunk Packet;
	TOKEN ResponseToken;
	bool Sevendown;

	m_NetServer.Update();

	// process packets
	for (int Socket = 0; Socket < NUM_SOCKETS; Socket++)
	{
		while(m_NetServer.Recv(&Packet, &ResponseToken, &Sevendown, Socket))
		{
			if(Packet.m_Flags&NETSENDFLAG_CONNLESS)
			{
				IRegister *pRegister = m_pRegister;
				if (Socket == SOCKET_TWO)
				{
					if (Sevendown || !IsDoubleInfo())
						continue;
					pRegister = m_pRegisterTwo;
				}

				if (ResponseToken == NET_TOKEN_NONE && pRegister->OnPacket(&Packet))
					continue;

				if(Packet.m_DataSize >= int(sizeof(SERVERBROWSE_GETINFO)) && mem_comp(Packet.m_pData, SERVERBROWSE_GETINFO, sizeof(SERVERBROWSE_GETINFO)) == 0)
				{
					CUnpacker Unpacker;
					Unpacker.Reset((unsigned char*)Packet.m_pData+sizeof(SERVERBROWSE_GETINFO), Packet.m_DataSize-sizeof(SERVERBROWSE_GETINFO));

					int SrvBrwsToken;
					if (Sevendown)
					{
						if (!Config()->m_SvAllowSevendown)
							continue;

						int ExtraToken = (Packet.m_aExtraData[0] << 8) | Packet.m_aExtraData[1];
						SrvBrwsToken = ((unsigned char*)Packet.m_pData)[sizeof(SERVERBROWSE_GETINFO)];
						SrvBrwsToken |= ExtraToken << 8;
						SendServerInfoSevendown(&Packet.m_Address, SrvBrwsToken, Socket);
					}
					else
					{
						SrvBrwsToken = Unpacker.GetInt();
						if (Unpacker.Error())
							continue;

						CPacker Packer;
						CNetChunk Response;

						GenerateServerInfo(&Packer, SrvBrwsToken, Socket);

						Response.m_ClientID = -1;
						Response.m_Address = Packet.m_Address;
						Response.m_Flags = NETSENDFLAG_CONNLESS;
						Response.m_pData = Packer.Data();
						Response.m_DataSize = Packer.Size();
						m_NetServer.Send(&Response, ResponseToken, false, Socket);
					}
				}
				else if (Packet.m_DataSize >= int(sizeof(REDIRECT_SAVE_TEE_ADD)) && mem_comp(Packet.m_pData, REDIRECT_SAVE_TEE_ADD, sizeof(REDIRECT_SAVE_TEE_ADD)) == 0)
				{
					CUnpacker Unpacker;
					Unpacker.Reset((unsigned char*)Packet.m_pData + sizeof(REDIRECT_SAVE_TEE_ADD), Packet.m_DataSize - sizeof(REDIRECT_SAVE_TEE_ADD));

					int Port = Unpacker.GetInt();
					const char *pHash = Unpacker.GetString(CUnpacker::SANITIZE_CC);

					if (Unpacker.Error() || Port != Config()->m_SvPort)
						continue;

					GameServer()->OnRedirectSaveTeeAdd(pHash);
				}
				else if (Packet.m_DataSize >= int(sizeof(REDIRECT_SAVE_TEE_REMOVE)) && mem_comp(Packet.m_pData, REDIRECT_SAVE_TEE_REMOVE, sizeof(REDIRECT_SAVE_TEE_REMOVE)) == 0)
				{
					CUnpacker Unpacker;
					Unpacker.Reset((unsigned char*)Packet.m_pData + sizeof(REDIRECT_SAVE_TEE_REMOVE), Packet.m_DataSize - sizeof(REDIRECT_SAVE_TEE_REMOVE));

					int Port = Unpacker.GetInt();
					const char *pHash = Unpacker.GetString(CUnpacker::SANITIZE_CC);

					if (Unpacker.Error() || Port != Config()->m_SvPort)
						continue;

					GameServer()->OnRedirectSaveTeeRemove(pHash);
				}
			}
			else
			{
				if(m_aClients[Packet.m_ClientID].m_State == CClient::STATE_REDIRECTED)
					continue;

				int GameFlags = 0;
				if(Packet.m_Flags & NET_CHUNKFLAG_VITAL)
				{
					GameFlags |= MSGFLAG_VITAL;
				}
				if(Antibot()->OnEngineClientMessage(Packet.m_ClientID, Packet.m_pData, Packet.m_DataSize, GameFlags))
				{
					continue;
				}

				ProcessClientPacket(&Packet);
			}
		}
	}

	{
		unsigned char aBuffer[NET_MAX_PAYLOAD];
		int Flags;
		CNetChunk Packet;
		mem_zero(&Packet, sizeof(Packet));
		Packet.m_pData = aBuffer;
		while(Antibot()->OnEngineSimulateClientMessage(&Packet.m_ClientID, aBuffer, sizeof(aBuffer), &Packet.m_DataSize, &Flags))
		{
			Packet.m_Flags = 0;
			if(Flags & MSGFLAG_VITAL)
			{
				Packet.m_Flags |= NET_CHUNKFLAG_VITAL;
			}
			ProcessClientPacket(&Packet);
		}
	}

	m_ServerBan.Update();
	m_Econ.Update();
}

const char *CServer::GetFileName(char *pPath)
{
	// get the name of the file without his path
	char *pShortName = &pPath[0];
	for(int i = 0; i < str_length(pPath)-1; i++)
	{
		if(pPath[i] == '/' || pPath[i] == '\\')
			pShortName = &pPath[i+1];
	}
	return pShortName;
}

const char *CServer::GetMapName()
{
	// get the name of the map without his path
	char *pMapShortName = &Config()->m_SvMap[0];
	for(int i = 0; i < str_length(Config()->m_SvMap)-1; i++)
	{
		if(Config()->m_SvMap[i] == '/' || Config()->m_SvMap[i] == '\\')
			pMapShortName = &Config()->m_SvMap[i+1];
	}
	return pMapShortName;
}

const char *CServer::GetCurrentMapName()
{
	// Config()->m_SvMap gets updated immediately, so this will return the map name before a map change for example
	return GetFileName(m_aCurrentMap);
}

void CServer::ChangeMap(const char *pMap)
{
	str_copy(Config()->m_SvMap, pMap, sizeof(Config()->m_SvMap));
	m_MapReload = str_comp(Config()->m_SvMap, m_aCurrentMap) != 0;
}

int CServer::LoadMap(const char *pMapName)
{
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "maps/%s.map", pMapName);
	GameServer()->OnMapChange(aBuf, sizeof(aBuf));

	// check for valid standard map
	if(!m_MapChecker.ReadAndValidateMap(Storage(), aBuf, IStorage::TYPE_ALL))
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mapchecker", "invalid standard map");
		return 0;
	}

	if(!m_pMap->Load(aBuf))
		return 0;

	// stop recording when we change map
	m_DemoRecorder.Stop();

	// reinit snapshot ids
	m_IDPool.TimeoutIDs();

	// get the sha256 and crc of the map
	m_CurrentMapSha256 = m_pMap->Sha256();
	m_CurrentMapCrc = m_pMap->Crc();
	char aSha256[SHA256_MAXSTRSIZE];
	sha256_str(m_CurrentMapSha256, aSha256, sizeof(aSha256));
	char aBufMsg[256];
	str_format(aBufMsg, sizeof(aBufMsg), "%s sha256 is %s", aBuf, aSha256);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBufMsg);
	str_format(aBufMsg, sizeof(aBufMsg), "%s crc is %08x", aBuf, m_CurrentMapCrc);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBufMsg);

	// call pre shutdown here so the mapname still is the correct one
	if (m_aCurrentMap[0])
		GameServer()->OnPreShutdown();
	str_copy(m_aCurrentMap, pMapName, sizeof(m_aCurrentMap));

	// load complete map into memory for download
	{
		IOHANDLE File = Storage()->OpenFile(aBuf, IOFLAG_READ, IStorage::TYPE_ALL);
		m_CurrentMapSize = (unsigned int)io_length(File);
		if(m_pCurrentMapData)
			mem_free(m_pCurrentMapData);
		m_pCurrentMapData = (unsigned char *)mem_alloc(m_CurrentMapSize, 1);
		io_read(File, m_pCurrentMapData, m_CurrentMapSize);
		io_close(File);
	}

	LoadUpdateFakeMap();
	LoadMapDesigns();

	return 1;
}

void CServer::InitInterfaces(CConfig *pConfig, IConsole *pConsole, IGameServer *pGameServer, IEngineMap *pMap, IStorage *pStorage, IEngineAntibot *pAntibot)
{
	m_pConfig = pConfig;
	m_pConsole = pConsole;
	m_pGameServer = pGameServer;
	m_pMap = pMap;
	m_pStorage = pStorage;
	m_pAntibot = pAntibot;
	HttpInit(m_pStorage);
}

int CServer::Run()
{
	if (m_RunServer == UNINITIALIZED)
		m_RunServer = RUNNING;

	m_AuthManager.Init(m_pConfig);

	if(Config()->m_Debug)
	{
		g_UuidManager.DebugDump();
	}

	//
	m_PrintCBIndex = Console()->RegisterPrintCallback(Config()->m_ConsoleOutputLevel, SendRconLineAuthed, this);

	// list maps
	m_pMapListHeap = new CHeap();
	CSubdirCallbackUserdata Userdata;
	Userdata.m_pServer = this;
	str_copy(Userdata.m_aName, "", sizeof(Userdata.m_aName));
	m_pStorage->ListDirectory(IStorage::TYPE_ALL, "maps/", MapListEntryCallback, &Userdata);

	// load map
	if(!LoadMap(Config()->m_SvMap))
	{
		dbg_msg("server", "failed to load map. mapname='%s'", Config()->m_SvMap);
		return -1;
	}
	m_MapChunksPerRequest = Config()->m_SvMapDownloadSpeed;

	// start server
	NETADDR BindAddr;
	if(Config()->m_Bindaddr[0] && net_host_lookup(Config()->m_Bindaddr, &BindAddr, NETTYPE_ALL) == 0)
	{
		// sweet!
		BindAddr.type = NETTYPE_ALL;
		BindAddr.port = Config()->m_SvPort;
	}
	else
	{
		mem_zero(&BindAddr, sizeof(BindAddr));
		BindAddr.type = NETTYPE_ALL;
		BindAddr.port = Config()->m_SvPort;
	}

	if(!m_NetServer.Open(BindAddr, Config(), Console(), Kernel()->RequestInterface<IEngine>(), &m_ServerBan,
		Config()->m_SvMaxClients, Config()->m_SvMaxClientsPerIP, NewClientCallback, DelClientCallback, ClientRejoinCallback, ClientCanCloseCallback, this))
	{
		dbg_msg("server", "couldn't open socket. port %d might already be in use", Config()->m_SvPort);
		return -1;
	}

	IEngine *pEngine = Kernel()->RequestInterface<IEngine>();
	m_pRegister = CreateRegister(m_pConfig, m_pConsole, pEngine, Config()->m_SvPort, m_NetServer.GetGlobalToken());
	m_pRegisterTwo = CreateRegister(m_pConfig, m_pConsole, pEngine, Config()->m_SvPortTwo, m_NetServer.GetGlobalToken());

	m_Econ.Init(Config(), Console(), &m_ServerBan);

#if defined(CONF_FAMILY_UNIX)
	m_Fifo.Init(Console(), Config()->m_SvInputFifo, CFGFLAG_SERVER);
#endif

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "server name is '%s'", Config()->m_SvName);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	Antibot()->Init();
	GameServer()->OnInit();
	str_format(aBuf, sizeof(aBuf), "netversion %s", GameServer()->NetVersion());
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
	str_format(aBuf, sizeof(aBuf), "netversion %s", GameServer()->NetVersionSevendown());
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
	str_format(aBuf, sizeof(aBuf), "game version %s", GameServer()->Version());
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	// process pending commands
	m_pConsole->StoreCommands(false);
	m_pRegister->OnConfigChange();
	m_pRegisterTwo->OnConfigChange();

	if(m_AuthManager.IsGenerated())
	{
		dbg_msg("server", "+-------------------------+");
		dbg_msg("server", "| rcon password: '%s' |", Config()->m_SvRconPassword);
		dbg_msg("server", "+-------------------------+");
	}

	// start game
	{
		m_GameStartTime = time_get();

		UpdateServerInfo();
		while(m_RunServer < STOPPING)
		{
			// load new map
			if(m_MapReload || m_CurrentGameTick >= 0x6FFFFFFF) //	force reload to make sure the ticks stay within a valid range
			{
				m_MapReload = false;

				// load map
				if(LoadMap(Config()->m_SvMap))
				{
					// new map loaded
					bool aSpecs[MAX_CLIENTS];
					for(int c = 0; c < MAX_CLIENTS; c++)
						aSpecs[c] = GameServer()->IsClientSpectator(c);

					GameServer()->OnShutdown();

					for(int c = 0; c < MAX_CLIENTS; c++)
					{
						if(m_aClients[c].m_State <= CClient::STATE_AUTH)
							continue;

						SendMap(c);
						m_aClients[c].Reset();
						m_aClients[c].m_State = aSpecs[c] ? CClient::STATE_CONNECTING_AS_SPEC : CClient::STATE_CONNECTING;
					}

					m_GameStartTime = time_get();
					m_CurrentGameTick = 0;
					Kernel()->ReregisterInterface(GameServer());
					GameServer()->OnInit();
					UpdateServerInfo(true);
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), "failed to load map. mapname='%s'", Config()->m_SvMap);
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
					str_copy(Config()->m_SvMap, m_aCurrentMap, sizeof(Config()->m_SvMap));
				}
			}

			int64 Now = time_get();
			bool NewTicks = false;
			bool ShouldSnap = false;
			while(Now > TickStartTime(m_CurrentGameTick+1))
			{
				for(int c = 0; c < MAX_CLIENTS; c++)
					if(m_aClients[c].m_State == CClient::STATE_INGAME)
						for(int i = 0; i < 200; i++)
							if(m_aClients[c].m_aInputs[i].m_GameTick == Tick() + 1)
								GameServer()->OnClientPredictedEarlyInput(c, m_aClients[c].m_aInputs[i].m_aData);

				m_CurrentGameTick++;
				NewTicks = true;
				if((m_CurrentGameTick%2) == 0)
					ShouldSnap = true;

				// apply new input
				for(int c = 0; c < MAX_CLIENTS; c++)
				{
					if(m_aClients[c].m_State != CClient::STATE_INGAME)
						continue;
					for(int i = 0; i < 200; i++)
					{
						if(m_aClients[c].m_aInputs[i].m_GameTick == Tick())
						{
							GameServer()->OnClientPredictedInput(c, m_aClients[c].m_aInputs[i].m_aData);
							break;
						}
					}
				}

				GameServer()->OnTick();

				// remove after 24 hours because iphub.info has 1000 free requests within 24 hours
				// actually lets use 48 hours just to be safe
				if (Tick() % (TickSpeed() * 60 * 60 * 48))
				{
					m_DnsblCache.m_vBlacklist.clear();
					m_DnsblCache.m_vWhitelist.clear();
				}

				for (int i = 0; i < MAX_CLIENTS; i++)
				{
					if (m_aClients[i].m_State != CClient::STATE_INGAME)
						continue;

					// vpn/proxy detection
					if (Config()->m_SvIPHubXKey[0])
					{
						if(m_aClients[i].m_DnsblState == CClient::DNSBL_STATE_NONE)
						{
							// initiate dnsbl lookup
							InitDnsbl(i);
						}
						else if(m_aClients[i].m_DnsblState == CClient::DNSBL_STATE_PENDING && m_aClients[i].m_pDnsblLookup->Status() == IJob::STATE_DONE)
						{
							if(m_aClients[i].m_pDnsblLookup->m_Result == 1) // only return on 1, not on 2 as that might be a false positive
							{
								// bad ip -> blacklisted
								m_aClients[i].m_DnsblState = CClient::DNSBL_STATE_BLACKLISTED;
								m_DnsblCache.m_vBlacklist.push_back(*m_NetServer.ClientAddr(i));

								// console output
								char aAddrStr[NETADDR_MAXSTRSIZE];
								net_addr_str(m_NetServer.ClientAddr(i), aAddrStr, sizeof(aAddrStr), true);

								char aBuf[256];
								str_format(aBuf, sizeof(aBuf), "ClientID=%d addr=<{%s}> blacklisted", i, aAddrStr);
								Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "dnsbl", aBuf);
							}
							else
							{
								// good ip -> whitelisted
								m_aClients[i].m_DnsblState = CClient::DNSBL_STATE_WHITELISTED;
								m_DnsblCache.m_vWhitelist.push_back(*m_NetServer.ClientAddr(i));
							}
						}

						if (m_aClients[i].m_DnsblState == CClient::DNSBL_STATE_BLACKLISTED)
							m_NetServer.NetBan()->BanAddr(m_NetServer.ClientAddr(i), 60 * 10, "VPN detected, try connecting without. Contact admin if mistaken");
					}

					// proxy game server detection
					if (Config()->m_SvPgsc)
					{
						if(m_aClients[i].m_PgscState == CClient::PGSC_STATE_NONE)
						{
							// initiate proxy game server check lookup
							InitProxyGameServerCheck(i);
						}
						else if(m_aClients[i].m_PgscState == CClient::PGSC_STATE_PENDING && m_aClients[i].m_pPgscLookup->Status() == IJob::STATE_DONE)
						{
							m_aClients[i].m_PgscState = CClient::PGSC_STATE_DONE;

							if(m_aClients[i].m_pPgscLookup->m_Result == 1)
							{
								// console output
								char aAddrStr[NETADDR_MAXSTRSIZE];
								net_addr_str(m_NetServer.ClientAddr(i), aAddrStr, sizeof(aAddrStr), true);

								char aBuf[256];
								str_format(aBuf, sizeof(aBuf), "ClientID=%d addr=<{%s}> broadcasts a proxy game server", i, aAddrStr);
								Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "proxy", aBuf);

								m_NetServer.NetBan()->BanAddr(m_NetServer.ClientAddr(i), 60*60*6, "Proxy server, try connecting to the real server. Contact admin if mistaken");
							}
						}
					}
				}
			}

			// snap game
			if(NewTicks)
			{
				if(Config()->m_SvHighBandwidth || ShouldSnap)
					DoSnapshot();

				UpdateClientRconCommands();
				UpdateClientMapListEntries();

#if defined(CONF_FAMILY_UNIX)
				m_Fifo.Update();
#endif
			}

			// master server stuff
			m_pRegister->Update();
			if (IsDoubleInfo())
				m_pRegisterTwo->Update();

			if (m_ServerInfoNeedsUpdate)
				UpdateServerInfo();

			Antibot()->OnEngineTick();

			PumpNetwork();

			{
				bool ServerEmpty = true;

				for(int c = 0; c < MAX_CLIENTS; c++)
				{
					if(m_aClients[c].m_State == CClient::STATE_REDIRECTED)
						if(time_get() > m_aClients[c].m_RedirectDropTime)
							m_NetServer.Drop(c, "redirected");

					if(m_aClients[c].m_State != CClient::STATE_EMPTY && m_aClients[c].m_State != CClient::STATE_DUMMY)
						ServerEmpty = false;
				}

				if(Config()->m_SvShutdownWhenEmpty && ServerEmpty)
					m_RunServer = STOPPING;
			}

			// wait for incoming data
			m_NetServer.Wait(clamp(int((TickStartTime(m_CurrentGameTick+1)-time_get())*1000/time_freq()), 1, 1000/SERVER_TICK_SPEED/2));
		}
	}

	// F-DDrace // to handle everything while the players are still available
	GameServer()->OnPreShutdown();

	// disconnect all clients on shutdown
	m_NetServer.Close();
	m_Econ.Shutdown();
	m_pRegister->OnShutdown();
	m_pRegisterTwo->OnShutdown();

#if defined(CONF_FAMILY_UNIX)
	m_Fifo.Shutdown();
#endif

	GameServer()->OnShutdown(true);
	m_pMap->Unload();

	if(m_pCurrentMapData)
	{
		mem_free(m_pCurrentMapData);
		m_pCurrentMapData = 0;
	}
	if(m_pFakeMapData)
	{
		mem_free(m_pFakeMapData);
		m_pFakeMapData = 0;
	}
	if(m_pMapListHeap)
	{
		delete m_pMapListHeap;
		m_pMapListHeap = 0;
	}
	for (int i = 0; i < NUM_MAP_DESIGNS; i++)
		if (m_aMapDesign[i].m_pData)
		{
			delete m_aMapDesign[i].m_pData;
			m_aMapDesign[i].m_pData = 0;
		}
	return 0;
}

int CServer::MapListEntryCallback(const char *pFilename, int IsDir, int DirType, void *pUser)
{
	CSubdirCallbackUserdata *pUserdata = (CSubdirCallbackUserdata *)pUser;
	CServer *pThis = pUserdata->m_pServer;

	if(pFilename[0] == '.') // hidden files
		return 0;

	char aFilename[IO_MAX_PATH_LENGTH];
	if(pUserdata->m_aName[0])
		str_format(aFilename, sizeof(aFilename), "%s/%s", pUserdata->m_aName, pFilename);
	else
		str_format(aFilename, sizeof(aFilename), "%s", pFilename);

	if(IsDir)
	{
		CSubdirCallbackUserdata Userdata;
		Userdata.m_pServer = pThis;
		str_copy(Userdata.m_aName, aFilename, sizeof(Userdata.m_aName));
		char FindPath[IO_MAX_PATH_LENGTH];
		str_format(FindPath, sizeof(FindPath), "maps/%s/", aFilename);
		pThis->m_pStorage->ListDirectory(IStorage::TYPE_ALL, FindPath, MapListEntryCallback, &Userdata);
		return 0;
	}

	const char *pSuffix = str_endswith(aFilename, ".map");
	if(!pSuffix) // not ending with .map
	{
			return 0;
	}

	CMapListEntry *pEntry = (CMapListEntry *)pThis->m_pMapListHeap->Allocate(sizeof(CMapListEntry));
	pThis->m_NumMapEntries++;
	pEntry->m_pNext = 0;
	pEntry->m_pPrev = pThis->m_pLastMapEntry;
	if(pEntry->m_pPrev)
		pEntry->m_pPrev->m_pNext = pEntry;
	pThis->m_pLastMapEntry = pEntry;
	if(!pThis->m_pFirstMapEntry)
		pThis->m_pFirstMapEntry = pEntry;

	str_truncate(pEntry->m_aName, sizeof(pEntry->m_aName), aFilename, pSuffix-aFilename);

	return 0;
}

void CServer::ConEuroMode(IConsole::IResult *pResult, void *pUser)
{
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Value: %d", ((CServer *)pUser)->Config()->m_SvEuroMode);
	((CServer *)pUser)->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
}

void CServer::ConTestingCommands(IConsole::IResult *pResult, void *pUser)
{
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Value: %d", ((CServer *)pUser)->Config()->m_SvTestingCommands);
	((CServer *)pUser)->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
}

void CServer::ConRescue(IConsole::IResult *pResult, void *pUser)
{
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Value: %d", ((CServer *)pUser)->Config()->m_SvRescue);
	((CServer *)pUser)->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
}

void CServer::ConKick(IConsole::IResult *pResult, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	int KickedID = pResult->GetInteger(0);
	const char *pReason = pResult->GetString(1);

	int Ret = -1;
	char aName[32];
	str_copy(aName, pThis->ClientName(KickedID), sizeof(aName));

	if(pResult->NumArguments() > 1)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Kicked (%s)", pReason);
		Ret = pThis->Kick(KickedID, aBuf);
	}
	else
		Ret = pThis->Kick(pResult->GetInteger(0), "Kicked by console");

	if (Ret == 0)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "'%s' has been kicked", aName);
		if (pReason[0])
		{
			char aReason[128];
			str_format(aReason, sizeof(aReason), " (%s)", pReason);
			str_append(aBuf, aReason, sizeof(aBuf));
		}
		pThis->GameServer()->SendModLogMessage(pResult->m_ClientID, aBuf);
	}
}

void CServer::ConStatus(IConsole::IResult *pResult, void *pUser)
{
	char aBuf[1024];
	char aAddrStr[NETADDR_MAXSTRSIZE];
	CServer* pThis = static_cast<CServer *>(pUser);
	const char *pName = pResult->NumArguments() == 1 ? pResult->GetString(0) : "";

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!str_utf8_find_nocase(pThis->m_aClients[i].m_aName, pName))
			continue;

		if (pThis->m_aClients[i].m_State == CClient::STATE_DUMMY)
		{
			str_format(aBuf, sizeof(aBuf), "id=%d name='%s' score=%d dummy=yes", i, pThis->m_aClients[i].m_aName, pThis->m_aClients[i].m_Score);
			if (!pThis->Config()->m_SvHideBotsStatus || pName[0] != 0)
				pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		}
		else if(pThis->m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			net_addr_str(pThis->m_NetServer.ClientAddr(i), aAddrStr, sizeof(aAddrStr), true);
			if(pThis->m_aClients[i].m_State == CClient::STATE_INGAME)
			{
				char aAuthStr[128];
				aAuthStr[0] = '\0';
				if(pThis->m_aClients[i].m_AuthKey >= 0)
				{
					const char *pAuthStr = pThis->m_aClients[i].m_Authed == AUTHED_ADMIN ? "(Admin)" :
											pThis->m_aClients[i].m_Authed == AUTHED_MOD ? "(Mod)" :
											pThis->m_aClients[i].m_Authed == AUTHED_HELPER ? "(Helper)" : "";

					str_format(aAuthStr, sizeof(aAuthStr), " key=%s %s", pThis->m_AuthManager.KeyIdent(pThis->m_aClients[i].m_AuthKey), pAuthStr);
				}

				char aDummy[64];
				aDummy[0] = '\0';
				int Dummy = pThis->GetDummy(i);
				if (Dummy != -1)
					str_format(aDummy, sizeof(aDummy), " %s=%d:'%s'", pThis->m_aClients[i].m_Main ? "dummy" : "main", Dummy, pThis->ClientName(Dummy));

				str_format(aBuf, sizeof(aBuf), "id=%d addr=<{%s}> client=%s sevendown=%d socket=%d name='%s' score=%d%s%s", i, aAddrStr,
						pThis->GetClientVersionStr(i), (int)pThis->m_aClients[i].m_Sevendown, pThis->m_aClients[i].m_Socket, pThis->m_aClients[i].m_aName, pThis->m_aClients[i].m_Score, aDummy, aAuthStr);
			}
			else
				str_format(aBuf, sizeof(aBuf), "id=%d addr=<{%s}> connecting", i, aAddrStr);
			pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		}
	}
}

void CServer::ConShutdown(IConsole::IResult *pResult, void *pUser)
{
	str_copy(((CServer*)pUser)->m_NetServer.m_ShutdownMessage, pResult->GetString(0), sizeof(((CServer*)pUser)->m_NetServer.m_ShutdownMessage));
	((CServer *)pUser)->m_RunServer = STOPPING;
}

void CServer::LogoutClient(int ClientID, const char *pReason)
{
	CMsgPacker Msg(NETMSG_RCON_AUTH_OFF, true);
	if (m_aClients[ClientID].m_Sevendown)
	{
		Msg.AddInt(0);
		Msg.AddInt(0);
	}
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);

	m_aClients[ClientID].m_AuthTries = 0;
	m_aClients[ClientID].m_pRconCmdToSend = 0;
	m_aClients[ClientID].m_pMapListEntryToSend = 0;

	char aBuf[64];
	if(*pReason)
	{
		str_format(aBuf, sizeof(aBuf), "Logged out by %s.", pReason);
		SendRconLine(ClientID, aBuf);
		str_format(aBuf, sizeof(aBuf), "ClientID=%d with key=%s logged out by %s", ClientID, m_AuthManager.KeyIdent(m_aClients[ClientID].m_AuthKey), pReason);
	}
	else
	{
		SendRconLine(ClientID, "Logout successful.");
		str_format(aBuf, sizeof(aBuf), "ClientID=%d with key=%s logged out", ClientID, m_AuthManager.KeyIdent(m_aClients[ClientID].m_AuthKey));
	}

	m_aClients[ClientID].m_Authed = AUTHED_NO;
	m_aClients[ClientID].m_AuthKey = -1;

	GameServer()->OnClientAuth(ClientID, AUTHED_NO);

	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CServer::LogoutKey(int Key, const char *pReason)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(m_aClients[i].m_AuthKey == Key)
			LogoutClient(i, pReason);
}

static int GetAuthLevel(const char *pLevel)
{
	int Level = -1;
	if(!str_comp_nocase(pLevel, "admin"))
		Level = AUTHED_ADMIN;
	else if(!str_comp_nocase_num(pLevel, "mod", 3))
		Level = AUTHED_MOD;
	else if(!str_comp_nocase(pLevel, "helper"))
		Level = AUTHED_HELPER;

	return Level;
}

void CServer::AuthRemoveKey(int KeySlot)
{
	int NewKeySlot = KeySlot;
	int OldKeySlot = m_AuthManager.RemoveKey(KeySlot);
	LogoutKey(KeySlot, "key removal");

	// Update indices.
	if(OldKeySlot != NewKeySlot)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(m_aClients[i].m_AuthKey == OldKeySlot)
				m_aClients[i].m_AuthKey = NewKeySlot;
	}
}

void CServer::ConAuthAdd(IConsole::IResult *pResult, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	CAuthManager *pManager = &pThis->m_AuthManager;

	const char *pIdent = pResult->GetString(0);
	const char *pLevel = pResult->GetString(1);
	const char *pPw = pResult->GetString(2);

	int Level = GetAuthLevel(pLevel);
	if(Level == -1)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "level can be one of {\"admin\", \"mod(erator)\", \"helper\"}");
		return;
	}

	bool NeedUpdate = !pManager->NumNonDefaultKeys();
	if(pManager->AddKey(pIdent, pPw, Level) < 0)
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ident already exists");
	else
	{
		if(NeedUpdate)
			pThis->SendRconType(-1, true);
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "key added");
	}
}

void CServer::ConAuthAddHashed(IConsole::IResult *pResult, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	CAuthManager *pManager = &pThis->m_AuthManager;

	const char *pIdent = pResult->GetString(0);
	const char *pLevel = pResult->GetString(1);
	const char *pPw = pResult->GetString(2);
	const char *pSalt = pResult->GetString(3);

	int Level = GetAuthLevel(pLevel);
	if(Level == -1)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "level can be one of {\"admin\", \"mod(erator)\", \"helper\"}");
		return;
	}

	MD5_DIGEST Hash;
	unsigned char aSalt[SALT_BYTES];

	if(md5_from_str(&Hash, pPw))
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "Malformed password hash");
		return;
	}
	if(str_hex_decode(aSalt, sizeof(aSalt), pSalt))
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "Malformed salt hash");
		return;
	}

	bool NeedUpdate = !pManager->NumNonDefaultKeys();

	if(pManager->AddKeyHash(pIdent, Hash, aSalt, Level) < 0)
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ident already exists");
	else
	{
		if(NeedUpdate)
			pThis->SendRconType(-1, true);
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "key added");
	}
}

void CServer::ConAuthUpdate(IConsole::IResult *pResult, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	CAuthManager *pManager = &pThis->m_AuthManager;

	const char *pIdent = pResult->GetString(0);
	const char *pLevel = pResult->GetString(1);
	const char *pPw = pResult->GetString(2);

	int KeySlot = pManager->FindKey(pIdent);
	if(KeySlot == -1)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ident couldn't be found");
		return;
	}

	int Level = GetAuthLevel(pLevel);
	if(Level == -1)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "level can be one of {\"admin\", \"mod(erator)\", \"helper\"}");
		return;
	}

	pManager->UpdateKey(KeySlot, pPw, Level);
	pThis->LogoutKey(KeySlot, "key update");

	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "key updated");
}

void CServer::ConAuthUpdateHashed(IConsole::IResult *pResult, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	CAuthManager *pManager = &pThis->m_AuthManager;

	const char *pIdent = pResult->GetString(0);
	const char *pLevel = pResult->GetString(1);
	const char *pPw = pResult->GetString(2);
	const char *pSalt = pResult->GetString(3);

	int KeySlot = pManager->FindKey(pIdent);
	if(KeySlot == -1)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ident couldn't be found");
		return;
	}

	int Level = GetAuthLevel(pLevel);
	if(Level == -1)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "level can be one of {\"admin\", \"mod(erator)\", \"helper\"}");
		return;
	}

	MD5_DIGEST Hash;
	unsigned char aSalt[SALT_BYTES];

	if(md5_from_str(&Hash, pPw))
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "Malformed password hash");
		return;
	}
	if(str_hex_decode(aSalt, sizeof(aSalt), pSalt))
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "Malformed salt hash");
		return;
	}

	pManager->UpdateKeyHash(KeySlot, Hash, aSalt, Level);
	pThis->LogoutKey(KeySlot, "key update");

	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "key updated");
}

void CServer::ConAuthRemove(IConsole::IResult *pResult, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	CAuthManager *pManager = &pThis->m_AuthManager;

	const char *pIdent = pResult->GetString(0);

	int KeySlot = pManager->FindKey(pIdent);
	if(KeySlot == -1)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ident couldn't be found");
		return;
	}

	pThis->AuthRemoveKey(KeySlot);

	if(!pManager->NumNonDefaultKeys())
		pThis->SendRconType(-1, false);

	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "key removed, all users logged out");
}

static void ListKeysCallback(const char *pIdent, int Level, void *pUser)
{
	static const char LSTRING[][10] = {"helper", "moderator", "admin"};

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s %s", pIdent, LSTRING[Level - 1]);
	((CServer *)pUser)->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", aBuf);
}

void CServer::ConAuthList(IConsole::IResult *pResult, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	CAuthManager *pManager = &pThis->m_AuthManager;

	pManager->ListKeys(ListKeysCallback, pThis);
}

void CServer::DemoRecorder_HandleAutoStart()
{
	if(Config()->m_SvAutoDemoRecord)
	{
		m_DemoRecorder.Stop();
		char aFilename[128];
		char aDate[20];
		str_timestamp(aDate, sizeof(aDate));
		str_format(aFilename, sizeof(aFilename), "demos/%s_%s.demo", "auto/autorecord", aDate);
		m_DemoRecorder.Start(Storage(), m_pConsole, aFilename, GameServer()->NetVersion(), m_aCurrentMap, m_CurrentMapSha256, m_CurrentMapCrc, "server");
		if(Config()->m_SvAutoDemoMax)
		{
			// clean up auto recorded demos
			CFileCollection AutoDemos;
			AutoDemos.Init(Storage(), "demos/server", "autorecord", ".demo", Config()->m_SvAutoDemoMax);
		}
	}
}

bool CServer::DemoRecorder_IsRecording()
{
	return m_DemoRecorder.IsRecording();
}

void CServer::ConRecord(IConsole::IResult *pResult, void *pUser)
{
	// Crashes, cuz snap functions dont handle clientid -1 properly :) xd
	/*CServer* pServer = (CServer *)pUser;
	char aFilename[128];
	if(pResult->NumArguments())
		str_format(aFilename, sizeof(aFilename), "demos/%s.demo", pResult->GetString(0));
	else
	{
		char aDate[20];
		str_timestamp(aDate, sizeof(aDate));
		str_format(aFilename, sizeof(aFilename), "demos/demo_%s.demo", aDate);
	}
	pServer->m_DemoRecorder.Start(pServer->Storage(), pServer->Console(), aFilename, pServer->GameServer()->NetVersion(), pServer->m_aCurrentMap, pServer->m_CurrentMapSha256, pServer->m_CurrentMapCrc, "server");*/
}

void CServer::ConStopRecord(IConsole::IResult *pResult, void *pUser)
{
	((CServer *)pUser)->m_DemoRecorder.Stop();
}

void CServer::ConMapReload(IConsole::IResult *pResult, void *pUser)
{
	((CServer *)pUser)->m_MapReload = true;
}

void CServer::ConLogout(IConsole::IResult *pResult, void *pUser)
{
	CServer *pServer = (CServer *)pUser;

	if(pServer->m_RconClientID >= 0 && pServer->m_RconClientID < MAX_CLIENTS &&
		pServer->m_aClients[pServer->m_RconClientID].m_State != CServer::CClient::STATE_EMPTY)
	{
		pServer->LogoutClient(pServer->m_RconClientID, "");
	}
}

void CServer::ConShowIps(IConsole::IResult *pResult, void *pUser)
{
	CServer *pServer = (CServer *)pUser;

	if(pServer->m_RconClientID >= 0 && pServer->m_RconClientID < MAX_CLIENTS &&
		pServer->m_aClients[pServer->m_RconClientID].m_State != CServer::CClient::STATE_EMPTY)
	{
		if(pResult->NumArguments())
		{
			pServer->m_aClients[pServer->m_RconClientID].m_ShowIps = pResult->GetInteger(0);
		}
		else
		{
			char aStr[9];
			str_format(aStr, sizeof(aStr), "Value: %d", pServer->m_aClients[pServer->m_RconClientID].m_ShowIps);
			char aBuf[32];
			pServer->SendRconLine(pServer->m_RconClientID, pServer->Console()->Format(aBuf, sizeof(aBuf), "server", aStr));
		}
	}
}

void CServer::ConchainSpecialInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CServer *pSelf = (CServer *)pUserData;
	if(pResult->NumArguments())
	{
		str_clean_whitespaces(pSelf->Config()->m_SvName);
		pSelf->UpdateServerInfo(true);
	}
}

void CServer::ConchainPlayerSlotsUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CServer *pSelf = (CServer *)pUserData;
	if(pResult->NumArguments())
	{
		if(pSelf->Config()->m_SvMaxClients < pSelf->Config()->m_SvPlayerSlots)
			pSelf->Config()->m_SvPlayerSlots = pSelf->Config()->m_SvMaxClients;
	}
}

void CServer::ConchainMaxclientsUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CServer *pSelf = (CServer *)pUserData;
	if(pResult->NumArguments())
	{
		if(pSelf->Config()->m_SvMaxClients < pSelf->Config()->m_SvPlayerSlots)
			pSelf->Config()->m_SvPlayerSlots = pSelf->Config()->m_SvMaxClients;
		pSelf->m_NetServer.SetMaxClients(pResult->GetInteger(0));
	}
}

void CServer::ConchainMaxclientsperipUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		((CServer *)pUserData)->m_NetServer.SetMaxClientsPerIP(pResult->GetInteger(0));
}

void CServer::ConchainCommandAccessUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	if(pResult->NumArguments() == 2)
	{
		CServer *pThis = static_cast<CServer *>(pUserData);
		const IConsole::CCommandInfo *pInfo = pThis->Console()->GetCommandInfo(pResult->GetString(0), CFGFLAG_SERVER, false);
		int OldAccessLevel = 0;
		if(pInfo)
			OldAccessLevel = pInfo->GetAccessLevel();
		pfnCallback(pResult, pCallbackUserData);
		if(pInfo && OldAccessLevel != pInfo->GetAccessLevel())
		{
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				if(pThis->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY ||
				(pInfo->GetAccessLevel() > AUTHED_ADMIN - pThis->m_aClients[i].m_Authed && AUTHED_ADMIN - pThis->m_aClients[i].m_Authed < OldAccessLevel) ||
				(pInfo->GetAccessLevel() < AUTHED_ADMIN - pThis->m_aClients[i].m_Authed && AUTHED_ADMIN - pThis->m_aClients[i].m_Authed > OldAccessLevel) ||
				(pThis->m_aClients[i].m_pRconCmdToSend && str_comp(pResult->GetString(0), pThis->m_aClients[i].m_pRconCmdToSend->m_pName) >= 0))
					continue;

				if(OldAccessLevel < pInfo->GetAccessLevel())
					pThis->SendRconCmdAdd(pInfo, i);
				else
					pThis->SendRconCmdRem(pInfo, i);
			}
		}
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

void CServer::ConchainConsoleOutputLevelUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() == 1)
	{
		CServer *pThis = static_cast<CServer *>(pUserData);
		pThis->Console()->SetPrintOutputLevel(pThis->m_PrintCBIndex, pResult->GetInteger(0));
	}
}

void CServer::ConchainRconPasswordChangeGeneric(int Level, const char *pCurrent, IConsole::IResult *pResult)
{
	if(pResult->NumArguments() == 1)
	{
		int KeySlot = m_AuthManager.DefaultKey(Level);
		const char *pNew = pResult->GetString(0);
		if(str_comp(pCurrent, pNew) == 0)
		{
			return;
		}
		if(KeySlot == -1 && pNew[0])
		{
			m_AuthManager.AddDefaultKey(Level, pNew);
		}
		else if(KeySlot >= 0)
		{
			if(!pNew[0])
			{
				AuthRemoveKey(KeySlot);
				// Already logs users out.
			}
			else
			{
				m_AuthManager.UpdateKey(KeySlot, pNew, Level);
				LogoutKey(KeySlot, "key update");
			}
		}
	}
}

void CServer::ConchainMapUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() >= 1)
	{
		CServer *pThis = static_cast<CServer *>(pUserData);
		pThis->m_MapReload = str_comp(pThis->Config()->m_SvMap, pThis->m_aCurrentMap) != 0;
	}
}

void CServer::ConchainRconPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	((CServer *)pUserData)->ConchainRconPasswordChangeGeneric(AUTHED_ADMIN, ((CServer*)pUserData)->Config()->m_SvRconPassword, pResult);
	pfnCallback(pResult, pCallbackUserData);
}

void CServer::ConchainRconModPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	((CServer *)pUserData)->ConchainRconPasswordChangeGeneric(AUTHED_MOD, ((CServer*)pUserData)->Config()->m_SvRconModPassword, pResult);
	pfnCallback(pResult, pCallbackUserData);
}

void CServer::ConchainRconHelperPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	((CServer *)pUserData)->ConchainRconPasswordChangeGeneric(AUTHED_HELPER, ((CServer*)pUserData)->Config()->m_SvRconHelperPassword, pResult);
	pfnCallback(pResult, pCallbackUserData);
}

void CServer::ConchainFakeMapCrc(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CServer *pThis = (CServer *)pUserData;
	pThis->m_FakeMapCrc = strtoul(pThis->Config()->m_FakeMapCrc, 0, 16);
	pThis->LoadUpdateFakeMap();
}

#if defined(CONF_FAMILY_UNIX)
void CServer::ConchainConnLoggingServerChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() == 1)
	{
		CServer *pServer = (CServer *)pUserData;

		// open socket to send new connections
		if(!pServer->m_ConnLoggingSocketCreated)
		{
			pServer->m_ConnLoggingSocket = net_unix_create_unnamed();
			if(pServer->m_ConnLoggingSocket == -1)
			{
				pServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Failed to created socket for communication with the connection logging server.");
			}
			else
			{
				pServer->m_ConnLoggingSocketCreated = true;
			}
		}

		// set the destination address for the connection logging
		net_unix_set_addr(&pServer->m_ConnLoggingDestAddr, pResult->GetString(0));
	}
}
#endif

void CServer::RegisterCommands()
{
	// register console commands
	Console()->Register("kick", "i[id] ?r[reason]", CFGFLAG_SERVER, ConKick, this, "Kick player with specified id for any reason", AUTHED_ADMIN);
	Console()->Register("status", "?r[name]", CFGFLAG_SERVER, ConStatus, this, "List players containing name or all players", AUTHED_MOD);
	Console()->Register("shutdown", "?r[message]", CFGFLAG_SERVER, ConShutdown, this, "Shut down", AUTHED_ADMIN);
	Console()->Register("logout", "", CFGFLAG_SERVER, ConLogout, this, "Logout of rcon", AUTHED_HELPER);
	Console()->Register("show_ips", "?i[show]", CFGFLAG_SERVER, ConShowIps, this, "Show IP addresses in rcon commands (1 = on, 0 = off)", AUTHED_ADMIN);

	Console()->Register("record", "?s[file]", CFGFLAG_SERVER|CFGFLAG_STORE, ConRecord, this, "Record to a file", AUTHED_ADMIN);
	Console()->Register("stoprecord", "", CFGFLAG_SERVER, ConStopRecord, this, "Stop recording", AUTHED_ADMIN);

	Console()->Register("reload", "", CFGFLAG_SERVER, ConMapReload, this, "Reload the map", AUTHED_ADMIN);

	// Auth Manager
	// TODO: Maybe move these into CAuthManager?
	Console()->Register("auth_add", "s[ident] s[level] s[pw]", CFGFLAG_SERVER|CFGFLAG_NONTEEHISTORIC, ConAuthAdd, this, "Add a rcon key", AUTHED_ADMIN);
	Console()->Register("auth_add_p", "s[ident] s[level] s[hash] s[salt]", CFGFLAG_SERVER|CFGFLAG_NONTEEHISTORIC, ConAuthAddHashed, this, "Add a prehashed rcon key", AUTHED_ADMIN);
	Console()->Register("auth_change", "s[ident] s[level] s[pw]", CFGFLAG_SERVER|CFGFLAG_NONTEEHISTORIC, ConAuthUpdate, this, "Update a rcon key", AUTHED_ADMIN);
	Console()->Register("auth_change_p", "s[ident] s[level] s[hash] s[salt]", CFGFLAG_SERVER|CFGFLAG_NONTEEHISTORIC, ConAuthUpdateHashed, this, "Update a rcon key with prehashed data", AUTHED_ADMIN);
	Console()->Register("auth_remove", "s[ident]", CFGFLAG_SERVER|CFGFLAG_NONTEEHISTORIC, ConAuthRemove, this, "Remove a rcon key", AUTHED_ADMIN);
	Console()->Register("auth_list", "", CFGFLAG_SERVER, ConAuthList, this, "List all rcon keys", AUTHED_ADMIN);

	Console()->Chain("sv_name", ConchainSpecialInfoupdate, this);
	Console()->Chain("password", ConchainSpecialInfoupdate, this);

	Console()->Chain("sv_player_slots", ConchainPlayerSlotsUpdate, this);
	Console()->Chain("sv_max_clients", ConchainMaxclientsUpdate, this);
	Console()->Chain("sv_max_clients", ConchainSpecialInfoupdate, this);
	Console()->Chain("sv_max_clients_per_ip", ConchainMaxclientsperipUpdate, this);
	Console()->Chain("access_level", ConchainCommandAccessUpdate, this);
	Console()->Chain("console_output_level", ConchainConsoleOutputLevelUpdate, this);

	Console()->Chain("sv_map", ConchainMapUpdate, this);
	Console()->Chain("sv_rcon_password", ConchainRconPasswordChange, this);
	Console()->Chain("sv_rcon_mod_password", ConchainRconModPasswordChange, this);
	Console()->Chain("sv_rcon_helper_password", ConchainRconHelperPasswordChange, this);

	Console()->Chain("fake_map_crc", ConchainFakeMapCrc, this);

#if defined(CONF_FAMILY_UNIX)
	Console()->Chain("sv_conn_logging_server", ConchainConnLoggingServerChange, this);
#endif

	// DDRace

#if defined(CONF_SQL)
	Console()->Register("add_sqlserver", "s['r'|'w'] s[Database] s[Prefix] s[User] s[Password] s[IP] i[Port] ?i[SetUpDatabase ?]", CFGFLAG_SERVER|CFGFLAG_NONTEEHISTORIC, ConAddSqlServer, this, "add a sqlserver", AUTHED_ADMIN);
	Console()->Register("dump_sqlservers", "s['r'|'w']", CFGFLAG_SERVER, ConDumpSqlServers, this, "dumps all sqlservers readservers = r, writeservers = w", AUTHED_ADMIN);
#endif

	// register console commands in sub parts
	m_ServerBan.InitServerBan(Console(), Storage(), this);
	m_pGameServer->OnConsoleInit();
}


int CServer::SnapNewID()
{
	return m_IDPool.NewID();
}

void CServer::SnapFreeID(int ID)
{
	m_IDPool.FreeID(ID);
}


void *CServer::SnapNewItem(int Type, int ID, int Size)
{
	if(!(Type >= 0 && Type <= 0xffff))
	{
		g_UuidManager.GetUuid(Type);
	}
	dbg_assert(ID >= 0 && ID <= 0xffff, "incorrect id");
	return ID < 0 ? 0 : m_SnapshotBuilder.NewItem(Type, ID, Size);
}

void CServer::SnapSetStaticsize(int ItemType, int Size)
{
	m_SnapshotDelta.SetStaticsize(ItemType, Size);
}

static CServer *CreateServer() { return new CServer(); }

int main(int argc, const char **argv) // ignore_convention
{
#if defined(CONF_FAMILY_WINDOWS)
	for(int i = 1; i < argc; i++) // ignore_convention
	{
		if(str_comp("-s", argv[i]) == 0 || str_comp("--silent", argv[i]) == 0) // ignore_convention
		{
			ShowWindow(GetConsoleWindow(), SW_HIDE);
			break;
		}
	}
#endif

	bool UseDefaultConfig = false;
	for(int i = 1; i < argc; i++) // ignore_convention
	{
		if(str_comp("-d", argv[i]) == 0 || str_comp("--default", argv[i]) == 0) // ignore_convention
		{
			UseDefaultConfig = true;
			break;
		}
	}

	if(secure_random_init() != 0)
	{
		dbg_msg("secure", "could not initialize secure RNG");
		return -1;
	}

	CServer *pServer = CreateServer();
	IKernel *pKernel = IKernel::Create();

	// create the components
	int FlagMask = CFGFLAG_SERVER|CFGFLAG_ECON;
	IEngine *pEngine = CreateEngine("Teeworlds_Server");
	IEngineMap *pEngineMap = CreateEngineMap();
	IGameServer *pGameServer = CreateGameServer();
	IConsole *pConsole = CreateConsole(CFGFLAG_SERVER|CFGFLAG_ECON);
	IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_SERVER, argc, argv); // ignore_convention
	IConfigManager *pConfigManager = CreateConfigManager();
	IEngineAntibot *pEngineAntibot = CreateEngineAntibot();

	{
		bool RegisterFail = false;

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pServer); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngine);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineMap*>(pEngineMap)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IMap*>(pEngineMap));
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pGameServer);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConsole);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pStorage);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConfigManager);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngineAntibot);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IAntibot*>(pEngineAntibot));

		if(RegisterFail)
			return -1;
	}

	pEngine->Init();
	pConfigManager->Init(FlagMask);
	pConsole->Init();

	pServer->InitInterfaces(pConfigManager->Values(), pConsole, pGameServer, pEngineMap, pStorage, pEngineAntibot);
	if(!UseDefaultConfig)
	{
		// register all console commands
		pServer->RegisterCommands();

		// execute autoexec file
		pConsole->ExecuteFile("autoexec.cfg");

		// parse the command line arguments
		if(argc > 1) // ignore_convention
			pConsole->ParseArguments(argc-1, &argv[1]); // ignore_convention
	}

	// restore empty config strings to their defaults
	pConfigManager->RestoreStrings();

	// these variables cant be changed ingame
	pConsole->Register("sv_test_cmds", "", CFGFLAG_SERVER, CServer::ConTestingCommands, pServer, "Turns testing commands aka cheats on/off", AUTHED_ADMIN);
	pConsole->Register("sv_rescue", "", CFGFLAG_SERVER, CServer::ConRescue, pServer, "Allow /rescue command so players can teleport themselves out of freeze", AUTHED_ADMIN);
	pConsole->Register("sv_euro_mode", "", CFGFLAG_SERVER, CServer::ConEuroMode, pServer, "Whether euro mode is enabled", AUTHED_ADMIN);

	pEngine->InitLogfile();

	// run the server
	dbg_msg("server", "starting...");
	int Ret = pServer->Run();

	// free
	delete pServer;
	delete pKernel;
	delete pEngine;
	delete pEngineMap;
	delete pGameServer;
	delete pConsole;
	delete pStorage;
	delete pConfigManager;

	return Ret;
}

// F-DDrace

void CServer::GetClientAddr(int ClientID, NETADDR* pAddr)
{
	if (ClientID >= 0 && ClientID < MAX_CLIENTS && (m_aClients[ClientID].m_State == CClient::STATE_INGAME || m_aClients[ClientID].m_State == CClient::STATE_DUMMY))
	{
		*pAddr = *m_NetServer.ClientAddr(ClientID);
	}
}

const char* CServer::GetAnnouncementLine(char const* pFileName)
{
	IOHANDLE File = m_pStorage->OpenFile(pFileName, IOFLAG_READ, IStorage::TYPE_ALL);
	if (!File)
		return 0;

	std::vector<char*> v;
	char* pLine;
	CLineReader* lr = new CLineReader();
	lr->Init(File);
	while ((pLine = lr->Get()))
		if (str_length(pLine))
			if (pLine[0] != '#')
				v.push_back(pLine);
	if (v.empty())
	{
		return 0;
	}
	else if (v.size() == 1)
	{
		m_AnnouncementLastLine = 0;
	}
	else if (!Config()->m_SvAnnouncementRandom)
	{
		if (++m_AnnouncementLastLine >= v.size())
			m_AnnouncementLastLine %= v.size();
	}
	else
	{
		unsigned Rand;
		do
		{
			Rand = rand() % v.size();
		} while (Rand == m_AnnouncementLastLine);

		m_AnnouncementLastLine = Rand;
	}

	io_close(File);

	return v[m_AnnouncementLastLine];
}

bool CServer::SetTimedOut(int ClientID, int OrigID)
{
	if (!m_NetServer.SetTimedOut(ClientID, OrigID))
	{
		return false;
	}

	m_aClients[ClientID].m_Sevendown = m_aClients[OrigID].m_Sevendown;
	m_aClients[ClientID].m_Socket = m_aClients[OrigID].m_Socket;

	if(m_aClients[OrigID].m_Authed != AUTHED_NO)
	{
		LogoutClient(ClientID, "Timeout Protection");
	}

	m_aClients[ClientID].m_CurrentMapDesign = m_aClients[OrigID].m_CurrentMapDesign;
	m_aClients[ClientID].m_Authed = AUTHED_NO;
	m_aClients[ClientID].m_DDNetVersion = m_aClients[OrigID].m_DDNetVersion;
	m_aClients[ClientID].m_GotDDNetVersionPacket = m_aClients[OrigID].m_GotDDNetVersionPacket;
	m_aClients[ClientID].m_DDNetVersionSettled = m_aClients[OrigID].m_DDNetVersionSettled;

	// important ot call OnSetTimedOut before we remove the original client but after we swapped already
	GameServer()->OnSetTimedOut(ClientID, OrigID);

	DelClientCallback(OrigID, "Timeout Protection used", this);
	return true;
}

int CServer::NumClients()
{
	return m_NetServer.NumClients();
}

bool CServer::IsDoubleInfo()
{
	return NumClients() >= VANILLA_MAX_CLIENTS && Config()->m_SvMaxClients > VANILLA_MAX_CLIENTS;
}

bool CServer::IsBrowserScoreFix()
{
	return (Config()->m_SvBrowserScoreFix || !str_find_nocase(GameServer()->GameType(), "race")) && Config()->m_SvDefaultScoreMode != 0;
}

bool CServer::IsUniqueAddress(int ClientID)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (i == ClientID || m_aClients[i].m_State == CClient::STATE_EMPTY)
			continue;
		if (net_addr_comp(m_NetServer.ClientAddr(ClientID), m_NetServer.ClientAddr(i), false) == 0)
			return false;
	}
	return true;
}

int CServer::GetDummy(int ClientID)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (IsDummy(ClientID, i))
			return i;
	return -1;
}

bool CServer::IsDummy(int ClientID1, int ClientID2)
{
	if (ClientID1 == ClientID2 || m_aClients[ClientID1].m_State == CClient::STATE_EMPTY || m_aClients[ClientID2].m_State == CClient::STATE_EMPTY
		|| m_aClients[ClientID1].m_State == CClient::STATE_DUMMY || m_aClients[ClientID2].m_State == CClient::STATE_DUMMY)
		return false;
	return net_addr_comp(m_NetServer.ClientAddr(ClientID1), m_NetServer.ClientAddr(ClientID2), false) == 0
		&& m_aClients[ClientID1].m_ConnectionID == m_aClients[ClientID2].m_ConnectionID;
}

bool CServer::DummyControlOrCopyMoves(int ClientID)
{
	int Dummy = GetDummy(ClientID);
	if (Dummy == -1)
		return false;
	// if both are not marked as idle, then we can assume copy moves is activated or dummy control is being used (to control dummy independently)
	return !m_aClients[ClientID].m_IdleDummy && !m_aClients[Dummy].m_IdleDummy;
}

void CServer::CWebhook::Run()
{
	int ret = system(m_aCommand);
	if (ret)
	{
		dbg_msg("webhook", "Sending webhook message failed, returned %d", ret);
		dbg_msg("webhook", "%s", m_aCommand);
	}
}

const char *CServer::GetAuthIdent(int ClientID)
{
	if (ClientID < 0 || ClientID >= MAX_CLIENTS)
		return "";
	return m_AuthManager.KeyIdent(m_aClients[ClientID].m_AuthKey);
}

void CServer::SendWebhookMessage(const char *pURL, const char *pMessage, const char *pUsername, const char *pAvatarURL)
{
	if (!pURL[0] || !pMessage[0])
		return;

	char *pMsg = strdup(pMessage);
	char *pName = strdup(pUsername);
	for (int i = 0; i < 2; i++)
	{
		char *ptr = i == 0 ? pMsg : pName;
		for (; *ptr; ptr++)
			if (*ptr == '\"' || *ptr == '\'' || *ptr == '\\' || *ptr == '\n' || *ptr == '|' || *ptr == '`' || *ptr == '@')
				*ptr = ' ';
	}

	const char *pBase = "curl -i -H \"Accept: application/json\" -H \"Content-Type:application/json\" -X POST --data";

	const char *pRedirect = "/dev/null";
	const char *pSingleQuote = "'";
	const char *pQuote = "\"";
#if defined(CONF_FAMILY_WINDOWS)
	pRedirect = "nul";
	pSingleQuote = "\"";
	pQuote = "\\\"";
#endif

	char aData[512];
	str_format(aData, sizeof(aData), "%s{%susername%s: %s%s%s, %scontent%s: %s%s%s, %savatar_url%s: %s%s%s}%s", pSingleQuote,
		pQuote, pQuote, pQuote, pName, pQuote,
		pQuote, pQuote, pQuote, pMsg, pQuote,
		pQuote, pQuote, pQuote, pAvatarURL, pQuote,
		pSingleQuote);

	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf), "%s %s %s >%s 2>&1", pBase, aData, pURL, pRedirect);
	free(pMsg);
	free(pName);

	IEngine *pEngine = Kernel()->RequestInterface<IEngine>();
	pEngine->AddJob(std::make_shared<CWebhook>(aBuf));
}

void CServer::CBotLookup::Run()
{
	if (!m_pServer->Config()->m_SvBotLookupURL[0])
	{
		m_pServer->m_BotLookupState = CServer::BOTLOOKUP_STATE_DONE;
		return;
	}

	char *pURL = strdup(m_pServer->Config()->m_SvBotLookupURL);
	char *ptr = pURL;
	for (; *ptr; ptr++)
		if (*ptr == '\'')
			*ptr = ' ';

	char aCmd[256];
	str_format(aCmd, sizeof(aCmd), "curl -s '%s'", pURL);
	free(pURL);

	FILE *pStream = pipe_open(aCmd, "r");
	if (!pStream)
	{
		m_pServer->m_BotLookupState = CServer::BOTLOOKUP_STATE_DONE;
		return;
	}

	char *pResult = (char *)calloc(1, 1);
	char aPiece[1024];
	int NewSize = 0;

	while (fgets(aPiece, sizeof(aPiece), pStream))
	{
		// +1 below to allow room for null terminator
		NewSize = str_length(pResult) + str_length(aPiece) + 1;
		pResult = (char *)realloc(pResult, NewSize);
		str_append(pResult, aPiece, NewSize);
	}
	pipe_close(pStream);

	bool Found = false;
	bool aDummy[MAX_CLIENTS] = { 0 };
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_pServer->m_aClients[i].m_State != CServer::CClient::STATE_INGAME || aDummy[i])
			continue;

		char aAddrStr[NETADDR_MAXSTRSIZE];
		net_addr_str(m_pServer->m_NetServer.ClientAddr(i), aAddrStr, sizeof(aAddrStr), false);
		if (str_find(pResult, aAddrStr))
		{
			char aBuf[256];
			int Dummy = m_pServer->GetDummy(i);
			if (Dummy != -1)
			{
				str_format(aBuf, sizeof(aBuf), "%d: %s, %d: %s", i, m_pServer->ClientName(i), Dummy, m_pServer->ClientName(Dummy));
				aDummy[Dummy] = true;
			}
			else
			{
				str_format(aBuf, sizeof(aBuf), "%d: %s", i, m_pServer->ClientName(i));
			}
			m_pServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "botlookup", aBuf);
			Found = true;
		}
	}

	if (!Found)
		m_pServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "botlookup", "No results found");

	m_pServer->m_BotLookupState = CServer::BOTLOOKUP_STATE_DONE;
	free(pResult);
}

void CServer::PrintBotLookup()
{
	if (m_BotLookupState == BOTLOOKUP_STATE_PENDING)
		return;

	IEngine *pEngine = Kernel()->RequestInterface<IEngine>();
	pEngine->AddJob(std::make_shared<CBotLookup>(this));
	m_BotLookupState = BOTLOOKUP_STATE_PENDING;
}

void CServer::CClient::CPgscLookup::Run()
{
	char aCmd[256];
	str_copy(aCmd, "curl -s https://master1.ddnet.tw/ddnet/15/servers.json", sizeof(aCmd));

	FILE *pStream = pipe_open(aCmd, "r");
	if (!pStream)
		return;

	char *pResult = (char *)calloc(1, 1);
	char aPiece[1024];
	int NewSize = 0;

	while (fgets(aPiece, sizeof(aPiece), pStream))
	{
		// +1 below to allow room for null terminator
		NewSize = str_length(pResult) + str_length(aPiece) + 1;
		pResult = (char *)realloc(pResult, NewSize);
		str_append(pResult, aPiece, NewSize);
	}
	pipe_close(pStream);

	const char *ptr = pResult;
	while ((ptr = str_find(ptr, m_aAddress)))
	{
		const char *pFind = "\"name\":";
		if ((ptr = str_find_nocase(ptr, pFind)))
		{
			ptr += str_length(pFind) + 1; // skip the starting " from the name
			const char *ptr2 = str_find(ptr, "\""); // end "
			if (ptr2)
			{
				int NameLength = str_length(ptr) - str_length(ptr2) + 1; // for null terminator
				char aServerName[128];
				str_copy(aServerName, ptr, min(NameLength, (int)sizeof(aServerName)));
				if (str_utf8_find_confusable(aServerName, m_aFindString)) // can be empty, then just ban ip if there is a game server broadcasted with this ip
				{
					free(pResult);
					m_Result = 1;
					return;
				}
			}
		}
	}
	free(pResult);
}

void CServer::InitProxyGameServerCheck(int ClientID)
{
	for (unsigned int i = 0; i < m_vWhitelist.size(); i++)
	{
		if (net_addr_comp(m_NetServer.ClientAddr(ClientID), &m_vWhitelist[i].m_Addr, false) == 0)
		{
			m_aClients[ClientID].m_PgscState = CClient::PGSC_STATE_DONE;
			return;
		}
	}

	IEngine *pEngine = Kernel()->RequestInterface<IEngine>();
	pEngine->AddJob(m_aClients[ClientID].m_pPgscLookup = std::make_shared<CClient::CPgscLookup>(m_NetServer.ClientAddr(ClientID), Config()->m_SvPgscString));
	m_aClients[ClientID].m_PgscState = CClient::PGSC_STATE_PENDING;
}

void CServer::CClient::CDnsblLookup::Run()
{
	FILE *pStream = pipe_open(m_aCommand, "r");
	if (!pStream)
		return;

	char aResult[512] = "";
	if (fgets(aResult, sizeof(aResult), pStream))
		dbg_msg("dnsbl", "%s", aResult);
	pipe_close(pStream);

	// parse json data
	json_settings JsonSettings;
	mem_zero(&JsonSettings, sizeof(JsonSettings));
	char aError[128];
	const json_value *pJsonData = json_parse_ex(&JsonSettings, aResult, sizeof(aResult), aError);
	if (pJsonData == 0)
	{
		dbg_msg("dnsbl", "Failed to parse json: %s", aError);
		return;
	}

	const json_value &rBlocked = (*pJsonData)["block"];
	if (rBlocked.type == json_integer)
		m_Result = (int)rBlocked.u.integer;
}

void CServer::InitDnsbl(int ClientID)
{
	for (int i = 0; i < 3; i++)
	{
		std::vector<NETADDR> List;
		switch (i)
		{
		case 0:
		{
			for (unsigned int k = 0; k < m_vWhitelist.size(); k++)
				List.push_back(m_vWhitelist[k].m_Addr);
			break;
		} 
		case 1: List = m_DnsblCache.m_vBlacklist; break;
		case 2: List = m_DnsblCache.m_vWhitelist; break;
		default: return;
		}

		for (unsigned int j = 0; j < List.size(); j++)
		{
			if (net_addr_comp(m_NetServer.ClientAddr(ClientID), &List[j], false) == 0)
			{
				m_aClients[ClientID].m_DnsblState = i == 1 ? CClient::DNSBL_STATE_BLACKLISTED : CClient::DNSBL_STATE_WHITELISTED;
				return;
			}
		}
	}

	char aAddrStr[NETADDR_MAXSTRSIZE];
	net_addr_str(m_NetServer.ClientAddr(ClientID), aAddrStr, sizeof(aAddrStr), false);

	char aBuf[512];
	str_format(aBuf, 512, "curl -s http://v2.api.iphub.info/ip/%s -H \"X-Key: %s\"", aAddrStr, Config()->m_SvIPHubXKey);

	IEngine *pEngine = Kernel()->RequestInterface<IEngine>();
	pEngine->AddJob(m_aClients[ClientID].m_pDnsblLookup = std::make_shared<CClient::CDnsblLookup>(aBuf));
	m_aClients[ClientID].m_DnsblState = CClient::DNSBL_STATE_PENDING;
}

void CServer::CTranslateChat::Run()
{
	char *ptr = m_aMessage;
	for (; *ptr; ptr++)
		if (*ptr == '\"' || *ptr == '\'' || *ptr == '\\' || *ptr == '\n' || *ptr == '|' || *ptr == '`')
			*ptr = ' ';

	char aCmd[512];
	char aKey[128];
#ifdef CONF_FAMILY_WINDOWS
	if (m_pServer->Config()->m_SvLibreTranslateKey[0])
		str_format(aKey, sizeof(aKey), ",\\\"api_key\\\":\\\"%s\\\"", m_pServer->Config()->m_SvLibreTranslateKey);
	str_format(aCmd, sizeof(aCmd), "curl -s -H \"Content-Type:application/json\" -X POST --data \"{\\\"q\\\":\\\"%s\\\",\\\"source\\\":\\\"auto\\\",\\\"target\\\":\\\"%s\\\"%s}\" %s", m_aMessage, m_aLanguage, aKey, m_pServer->Config()->m_SvLibreTranslateURL);
#else
	if (m_pServer->Config()->m_SvLibreTranslateKey[0])
		str_format(aKey, sizeof(aKey), ",\"api_key\":\"%s\"", m_pServer->Config()->m_SvLibreTranslateKey);
	str_format(aCmd, sizeof(aCmd), "curl -s -H \"Content-Type:application/json\" -X POST --data '{\"q\":\"%s\",\"source\":\"auto\",\"target\":\"%s\"%s}' %s", m_aMessage, m_aLanguage, aKey, m_pServer->Config()->m_SvLibreTranslateURL);
#endif

	char aResult[512] = "";
	FILE *pStream = pipe_open(aCmd, "r");
	if (pStream)
	{
		if (!fgets(aResult, sizeof(aResult), pStream))
			aResult[0] = '\0'; // should be 0 already but i dont want to ignore output for warning.. xd
		pipe_close(pStream);

		{
			// parse json data
			json_settings JsonSettings;
			mem_zero(&JsonSettings, sizeof(JsonSettings));
			char aError[128];
			const json_value *pJsonData = json_parse_ex(&JsonSettings, aResult, sizeof(aResult), aError);
			aResult[0] = '\0';
			if (pJsonData == 0)
			{
				dbg_msg("translate", "Failed to parse json: %s", aError);
			}
			else
			{
				const json_value &rTranslatedText = (*pJsonData)["translatedText"];
				if (rTranslatedText.type == json_string)
					str_copy(aResult, rTranslatedText, sizeof(aResult));
			}
		}
	}

	if (!aResult[0])
		str_copy(aResult, m_aMessage, sizeof(aResult));

	if (aResult[0])
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (m_pServer->m_aClients[i].m_State != CServer::CClient::STATE_INGAME || str_comp_nocase(m_pServer->GetLanguage(i), m_aLanguage) != 0)
				continue;
			int Mode = (m_Mode == CHAT_TEAM || m_Mode == CHAT_LOCAL) ? CHAT_SINGLE_TEAM : CHAT_SINGLE;
			m_pServer->GameServer()->SendChatMessage(m_ClientID, Mode, i, aResult);
		}
	}
}

void CServer::TranslateChat(int ClientID, const char *pMsg, int Mode)
{
	std::vector<const char *> vLanguages;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_aClients[i].m_State != CServer::CClient::STATE_INGAME || str_comp_nocase(GetLanguage(i), "none") == 0)
			continue;

		bool Continue = false;
		for (unsigned int j = 0; j < vLanguages.size(); j++)
			if (!str_comp(GetLanguage(i), vLanguages[j]))
			{
				Continue = true;
				break;
			}

		if (Continue)
			continue;

		vLanguages.push_back(GetLanguage(i));
	}

	for (unsigned int i = 0; i < vLanguages.size(); i++)
	{
		IEngine *pEngine = Kernel()->RequestInterface<IEngine>();
		pEngine->AddJob(std::make_shared<CTranslateChat>(this, ClientID, Mode, pMsg, vLanguages[i]));
	}
}

void CServer::LoadMapDesigns()
{
	for (int i = 0; i < NUM_MAP_DESIGNS; i++)
	{
		m_aMapDesign[i].m_aName[0] = '\0';
		m_aMapDesign[i].m_Sha256 = SHA256_ZEROED;
		m_aMapDesign[i].m_Crc = 0;
		m_aMapDesign[i].m_pData = 0;
		m_aMapDesign[i].m_Size = 0;
	}

	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "%s/%s", Config()->m_SvMapDesignPath, GetCurrentMapName());
	m_vMapDesignFiles.clear();
	Storage()->ListDirectory(IStorage::TYPE_ALL, aPath, InitMapDesign, this);

	for (unsigned int i = 0; i < m_vMapDesignFiles.size(); i++)
	{
		if (i >= NUM_MAP_DESIGNS)
			break;

		str_format(aPath, sizeof(aPath), "%s/%s/%s.map", Config()->m_SvMapDesignPath, GetCurrentMapName(), m_vMapDesignFiles[i].c_str());

		IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_ALL);
		if (File)
		{
			str_copy(m_aMapDesign[i].m_aName, m_vMapDesignFiles[i].c_str(), sizeof(m_aMapDesign[i].m_aName));

			m_aMapDesign[i].m_Size = (unsigned int)io_length(File);
			if(m_aMapDesign[i].m_pData)
				mem_free(m_aMapDesign[i].m_pData);
			m_aMapDesign[i].m_pData = (unsigned char *)mem_alloc(m_aMapDesign[i].m_Size, 1);
			io_read(File, m_aMapDesign[i].m_pData, m_aMapDesign[i].m_Size);
			io_close(File);
		}

		CDataFileReader Reader;
		Reader.Open(Storage(), aPath, IStorage::TYPE_ALL);
		if (Reader.IsOpen())
		{
			m_aMapDesign[i].m_Sha256 = Reader.Sha256();
			m_aMapDesign[i].m_Crc = Reader.Crc();
		}
		Reader.Close();
	}
}

int CServer::InitMapDesign(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CServer *pThis = (CServer *)pUser;

	if (!IsDir && str_endswith(pName, ".map"))
	{
		std::string Name = pName;
		Name = Name.erase(Name.size() - 4); // remove .map
		for (unsigned int i = 0; i < pThis->m_vMapDesignFiles.size(); i++)
			if (pThis->m_vMapDesignFiles[i] == Name)
				return 0;

		pThis->m_vMapDesignFiles.push_back(Name);
	}

	return 0;
}

void CServer::ChangeMapDesign(int ClientID, const char *pName)
{
	if (!pName[0])
		return;

	int Design = m_aClients[ClientID].m_CurrentMapDesign;
	if (str_comp_nocase(pName, "default") == 0)
		Design = -1;

	for (int i = 0; i < NUM_MAP_DESIGNS; i++)
	{
		if (str_comp_nocase(pName, m_aMapDesign[i].m_aName) == 0)
		{
			Design = i;
			break;
		}
	}

	if (Design != m_aClients[ClientID].m_CurrentMapDesign)
		SendMapDesign(ClientID, Design);
}

void CServer::SendMapDesign(int ClientID, int Design)
{
	if (!m_aClients[ClientID].m_Main && m_aClients[ClientID].m_Sevendown)
	{
		int Main = GetDummy(ClientID);
		if (Main != -1)
		{
			SendMapDesign(Main, Design);
			return;
		}
	}

	m_aClients[ClientID].m_DesignChange = true;
	m_aClients[ClientID].m_CurrentMapDesign = Design;

	// we also set it for the dummy so that ClientCanCloseCallback also works for dummy while design change
	int Dummy = GetDummy(ClientID);
	if (Dummy != -1)
	{
		m_aClients[Dummy].m_DesignChange = true;
		m_aClients[Dummy].m_CurrentMapDesign = Design;
	}

	if (Design == -1)
	{
		SendMap(ClientID);
		return;
	}

	char aName[128];
	str_format(aName, sizeof(aName), "%s_%s", GetMapName(), m_aMapDesign[Design].m_aName);

	{
		CMsgPacker Msg(NETMSG_MAP_DETAILS, true);
		Msg.AddString(aName, 0);
		Msg.AddRaw(&m_aMapDesign[Design].m_Sha256.data, sizeof(m_aMapDesign[Design].m_Sha256.data));
		Msg.AddInt(m_aMapDesign[Design].m_Crc);
		Msg.AddInt(m_aMapDesign[Design].m_Size);
		Msg.AddString(GetHttpsMapURL(Design), 0);
		SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
	}
	{
		CMsgPacker Msg(NETMSG_MAP_CHANGE, true);
		Msg.AddString(aName, 0);
		Msg.AddInt(m_aMapDesign[Design].m_Crc);
		Msg.AddInt(m_aMapDesign[Design].m_Size);
		if (!m_aClients[ClientID].m_Sevendown)
		{
			Msg.AddInt(m_MapChunksPerRequest);
			Msg.AddInt(MAP_CHUNK_SIZE);
			Msg.AddRaw(&m_aMapDesign[Design].m_Sha256, sizeof(m_aMapDesign[Design].m_Sha256));
		}
		SendMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID);
	}

	m_aClients[ClientID].m_MapChunk = 0;
}

const char *CServer::GetMapDesign(int ClientID)
{
	int Design = m_aClients[ClientID].m_CurrentMapDesign;
	if (Design == -1)
		return "default";
	return m_aMapDesign[Design].m_aName;
}

const char *CServer::GetHttpsMapURL(int Design)
{
	if (!Config()->m_SvHttpsMapDownloadURL[0])
		return "";

	char aName[256];
	char aSha256[SHA256_MAXSTRSIZE];
	if (Design != -1)
	{
		sha256_str(m_aMapDesign[Design].m_Sha256, aSha256, sizeof(aSha256));
		str_format(aName, sizeof(aName), "%s_%s_%s", GetMapName(), m_aMapDesign[Design].m_aName, aSha256);
	}
	else
	{
		sha256_str(m_CurrentMapSha256, aSha256, sizeof(aSha256));
		str_format(aName, sizeof(aName), "%s_%s", GetMapName(), aSha256);
	}

	static char aFullPath[512];
	str_format(aFullPath, sizeof(aFullPath), "%s/%s.map", Config()->m_SvHttpsMapDownloadURL, aName);
	return aFullPath;
}

void CServer::AddWhitelist(const NETADDR *pAddr, const char *pReason)
{
	// avoid double entries
	for (unsigned int j = 0; j < m_vWhitelist.size(); j++)
		if (net_addr_comp(pAddr, &m_vWhitelist[j].m_Addr, false) == 0)
			return;

	SWhitelist Entry;
	Entry.m_Addr = *pAddr;
	str_copy(Entry.m_aReason, pReason, sizeof(Entry.m_aReason));
	m_vWhitelist.push_back(Entry);

	char aAddrStr[NETADDR_MAXSTRSIZE];
	net_addr_str(pAddr, aAddrStr, sizeof(aAddrStr), false);

	char aReason[64] = "";
	if (pReason[0])
		str_format(aReason, sizeof(aReason), "(%s)", pReason);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "Added '%s' to whitelist %s", aAddrStr, aReason);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "whitelist", aBuf);
}

void CServer::RemoveWhitelist(const NETADDR *pAddr)
{
	for (unsigned int i = 0; i < m_vWhitelist.size(); i++)
	{
		if (net_addr_comp(pAddr, &m_vWhitelist[i].m_Addr, false) == 0)
		{
			RemoveWhitelistByIndex(i);
			break;
		}
	}
}

void CServer::RemoveWhitelistByIndex(unsigned int Index)
{
	if (Index >= m_vWhitelist.size())
		return;

	char aAddrStr[NETADDR_MAXSTRSIZE];
	net_addr_str(&m_vWhitelist[Index].m_Addr, aAddrStr, sizeof(aAddrStr), false);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "Removed '%s' from whitelist", aAddrStr);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "whitelist", aBuf);

	m_vWhitelist.erase(m_vWhitelist.begin() + Index);
}

void CServer::PrintWhitelist()
{
	for (unsigned int i = 0; i < m_vWhitelist.size(); i++)
	{
		char aAddrStr[NETADDR_MAXSTRSIZE];
		net_addr_str(&m_vWhitelist[i].m_Addr, aAddrStr, sizeof(aAddrStr), false);

		char aReason[64] = "";
		if (m_vWhitelist[i].m_aReason[0])
			str_format(aReason, sizeof(aReason), "(%s)", m_vWhitelist[i].m_aReason);

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "#%d '%s' %s", i, aAddrStr, aReason);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "whitelist", aBuf);
	}
}

void CServer::SaveWhitelist()
{
	std::string data;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s", Config()->m_SvWhitelistFile);
	std::ofstream Whitelist(aBuf);
	if (!Whitelist.is_open())
		return;

	char aAddrStr[NETADDR_MAXSTRSIZE];
	for (unsigned int i = 0; i < m_vWhitelist.size(); i++)
	{
		net_addr_str(&m_vWhitelist[i].m_Addr, aAddrStr, sizeof(aAddrStr), false);
		str_format(aBuf, sizeof(aBuf), "whitelist_add \"%s\" \"%s\"", aAddrStr, m_vWhitelist[i].m_aReason);
		Whitelist << aBuf << "\n";
	}
}

int *CServer::GetIdMap(int ClientID)
{
	return m_aClients[ClientID].m_aIdMap;
}

int *CServer::GetReverseIdMap(int ClientID)
{
	return m_aClients[ClientID].m_aReverseIdMap;
}

void CServer::DummyJoin(int DummyID)
{
	const char* pNames[] = {
		"flappy", /*0*/ "Chillingo", "ZillyDreck", "Fluffy", "MLG_PRO", "Enzym", "ciliDR[HUN]", "fuzzle", "Piko", "chilliger", "fokkonautt", "GubbaFubba", "fuZZle", "<bot>", "<noob>", "<police>", //16th name
		"<train>", "<boat>", "<blocker>", "<racer>", "<hyper>", "$heeP", "b3ep", "chilluminatee", "auftragschiller", "abcJuhee", "BANANA", "POTATO", "<cucumber>", "fokkoNUT", "<_BoT__>", "NotMyName", //32nd name
		"NotChiller", "NotChiIIer", "NotChlIer", "fuckmesoon", "DataNub", "5.196.132.14", "<hacker>", "<cheater>", "<glitcher>", "__ERROR", "404_kein_tier", "ZitrusFRUCHT", "BAUMKIND", "KELLERKIND", "KINDERKIND", "einZug-", //48th name
		"<bob>",  "BezzyHill", "BeckySkill", "Skilli.*", "UltraVa.", "DONATE!", "SUBSCRIBE!", "SHARE!", "#like", "<#name_>", "KRISTIAN-.", ".,-,08/524", "3113pimml34", "NotAB0t", "Hurman", "xxlddnnet64", "flappy2", //64th name
		"steeeve", "naki", "tuba", "higge", "linux_uzer3k", "hubbat.*", "Proviet-", "7h89", "1276", "SchinKKKen", "FOSSIELamKIEL", "apfelFUZ", "cron_tabur", "hinter_c_dur", "equariator", "deckztinator", //80th name
		"intezinatoha", "defquirlibaor", "enmuhinakur", "wooknazitur", "demnatura", "intranuza", "eggspikuza", "finaluba", "denkrikator", "nihilatur", "Goethe[HUN]", "RightIsRight", "Egg_user_xd", "avucadur", "NoeeoN", "wuuuzzZZZa", //96th name
		"JezzicaP", "Jeqqicaqua", "analyticus", "haspiclecane", "nameus", "tahdequz", "rostBEULEH", "regenwurm674", "mc_cm", "blockddrace", "BlockDDrace", "pidgin.,a", "bibubablbl", "randomNAME2", "Mircaduzla", "zer0_brain", //112th name
		"haxxor-420", "fok-me-fok", "fok-fee-san", "denzulat", "epsilat", "destructat", "hinzuckat", "penZilin", "deszilin", "VogelFisch7", "Dont4sk", "i_fokmen_i", "noobScout24", "geneticual", "trollface" //128th name
	};
	const char* pClans[] = {
		"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16",
		"17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31", "32",
		"33", "34", "35", "36", "37", "38", "39", "40", "41", "42", "43", "44", "45", "46", "47", "48",
		"49", "50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "60", "61", "62", "63", "64",
		"65", "66", "67", "68", "69", "70", "71", "72", "73", "74", "75", "76", "77", "78", "79", "80",
		"81", "82", "83", "84", "85", "86", "87", "88", "89", "90", "91", "92", "93", "94", "95", "96",
		"97", "98", "99", "100", "101", "102", "103", "104", "105", "106", "107", "108", "109", "110", "111", "112",
		"113", "114", "115", "116", "117", "118", "119", "120", "121", "122", "123", "124", "125", "126", "127", "128",
	};

	m_NetServer.DummyInit(DummyID);
	m_aClients[DummyID].Reset();
	m_aClients[DummyID].ResetContent();
	m_aClients[DummyID].m_State = CClient::STATE_DUMMY;
	m_aClients[DummyID].m_Authed = AUTHED_NO;
	m_aClients[DummyID].m_Sevendown = false;
	m_aClients[DummyID].m_Socket = SOCKET_MAIN;

	str_copy(m_aClients[DummyID].m_aName, pNames[DummyID], sizeof(m_aClients[DummyID].m_aName));
	str_copy(m_aClients[DummyID].m_aClan, pClans[DummyID], sizeof(m_aClients[DummyID].m_aClan));
}

void CServer::DummyLeave(int DummyID)
{
	GameServer()->OnClientDrop(DummyID, "");
	m_aClients[DummyID].m_Snapshots.PurgeAll();
	m_aClients[DummyID].Reset();
	m_aClients[DummyID].ResetContent();
	m_NetServer.DummyDelete(DummyID);
}

#ifdef CONF_FAMILY_UNIX
void CServer::SendConnLoggingCommand(CONN_LOGGING_CMD Cmd, const NETADDR *pAddr)
{
	if(!Config()->m_SvConnLoggingServer[0] || !m_ConnLoggingSocketCreated)
		return;

	// pack the data and send it
	unsigned char aData[23] = {0};
	aData[0] = Cmd;
	mem_copy(&aData[1], &pAddr->type, 4);
	mem_copy(&aData[5], pAddr->ip, 16);
	mem_copy(&aData[21], &pAddr->port, 2);

	net_unix_send(m_ConnLoggingSocket, &m_ConnLoggingDestAddr, aData, sizeof(aData));
}
#endif

#if defined (CONF_SQL)
void CServer::ConAddSqlServer(IConsole::IResult *pResult, void *pUserData)
{
	CServer *pSelf = (CServer *)pUserData;

	if (pResult->NumArguments() != 7 && pResult->NumArguments() != 8)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "7 or 8 arguments are required");
		return;
	}

	bool ReadOnly;
	if (str_comp_nocase(pResult->GetString(0), "w") == 0)
		ReadOnly = false;
	else if (str_comp_nocase(pResult->GetString(0), "r") == 0)
		ReadOnly = true;
	else
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "choose either 'r' for SqlReadServer or 'w' for SqlWriteServer");
		return;
	}

	bool SetUpDb = pResult->NumArguments() == 8 ? pResult->GetInteger(7) : true;

	CSqlServer** apSqlServers = ReadOnly ? pSelf->m_apSqlReadServers : pSelf->m_apSqlWriteServers;

	for (int i = 0; i < MAX_SQLSERVERS; i++)
	{
		if (!apSqlServers[i])
		{
			apSqlServers[i] = new CSqlServer(pResult->GetString(1), pResult->GetString(2), pResult->GetString(3), pResult->GetString(4), pResult->GetString(5), pResult->GetInteger(6), &pSelf->m_GlobalSqlLock, ReadOnly, SetUpDb);

			if(SetUpDb)
				thread_init(CreateTablesThread, apSqlServers[i], "CreateTables");

			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "Added new Sql%sServer: %d: DB: '%s' Prefix: '%s' User: '%s' IP: <{'%s'}> Port: %d", ReadOnly ? "Read" : "Write", i, apSqlServers[i]->GetDatabase(), apSqlServers[i]->GetPrefix(), apSqlServers[i]->GetUser(), apSqlServers[i]->GetIP(), apSqlServers[i]->GetPort());
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
	}
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "failed to add new sqlserver: limit of sqlservers reached");
}

void CServer::ConDumpSqlServers(IConsole::IResult *pResult, void *pUserData)
{
	CServer *pSelf = (CServer *)pUserData;

	bool ReadOnly;
	if (str_comp_nocase(pResult->GetString(0), "w") == 0)
		ReadOnly = false;
	else if (str_comp_nocase(pResult->GetString(0), "r") == 0)
		ReadOnly = true;
	else
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "choose either 'r' for SqlReadServer or 'w' for SqlWriteServer");
		return;
	}

	CSqlServer** apSqlServers = ReadOnly ? pSelf->m_apSqlReadServers : pSelf->m_apSqlWriteServers;

	for (int i = 0; i < MAX_SQLSERVERS; i++)
		if (apSqlServers[i])
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "SQL-%s %d: DB: '%s' Prefix: '%s' User: '%s' Pass: '%s' IP: <{'%s'}> Port: %d", ReadOnly ? "Read" : "Write", i, apSqlServers[i]->GetDatabase(), apSqlServers[i]->GetPrefix(), apSqlServers[i]->GetUser(), apSqlServers[i]->GetPass(), apSqlServers[i]->GetIP(), apSqlServers[i]->GetPort());
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		}
}

void CServer::CreateTablesThread(void *pData)
{
	((CSqlServer *)pData)->CreateTables();
}

#endif
