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

#include <mastersrv/mastersrv.h>

#include "register.h"
#include "server.h"

#include <string.h>
#include <vector>
#include <engine/shared/linereader.h>

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
	dbg_assert(ID != -1, "id error");
	if(ID == -1)
		return ID;
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
	CNetBan::Init(pConsole, pStorage);

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
			Server()->m_NetServer.Drop(i, aBuf);
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
			pThis->BanAddr(pThis->Server()->m_NetServer.ClientAddr(ClientID), Minutes*60, pReason);
	}
	else
		ConBan(pResult, pUser);
}


void CServer::CClient::Reset()
{
	// reset input
	for(int i = 0; i < 200; i++)
		m_aInputs[i].m_GameTick = -1;
	m_CurrentInput = 0;
	mem_zero(&m_LatestInput, sizeof(m_LatestInput));

	m_Snapshots.PurgeAll();
	m_LastAckedSnapshot = -1;
	m_LastInputTick = -1;
	m_SnapRate = CClient::SNAPRATE_INIT;
	m_Score = 0;
	m_MapChunk = 0;

	m_DDNetVersion = VERSION_NONE;
	m_GotDDNetVersionPacket = false;
	m_DDNetVersionSettled = false;
}

CServer::CServer() : m_DemoRecorder(&m_SnapshotDelta), m_Register(false), m_RegisterSevendown(true)
{
	m_TickSpeed = SERVER_TICK_SPEED;

	m_pGameServer = 0;

	m_CurrentGameTick = 0;
	m_RunServer = true;

	m_pCurrentMapData = 0;
	m_CurrentMapSize = 0;

	m_NumMapEntries = 0;
	m_pFirstMapEntry = 0;
	m_pLastMapEntry = 0;
	m_pMapListHeap = 0;

	m_MapReload = false;

	m_RconClientID = IServer::RCON_CID_SERV;
	m_RconAuthLevel = AUTHED_ADMIN;

	m_RconRestrict = -1;

	m_RconPasswordSet = 0;

	m_ServerInfoFirstRequest = 0;
	m_ServerInfoNumRequests = 0;

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

int CServer::TrySetClientName(int ClientID, const char* pName)
{
	char aTrimmedName[64];

	// trim the name
	str_copy(aTrimmedName, str_utf8_skip_whitespaces(pName), sizeof(aTrimmedName));
	str_utf8_trim_right(aTrimmedName);

	// check for empty names
	if (!aTrimmedName[0])
		return -1;

	// check for names starting with /, as they can be abused to make people
	// write chat commands
	if (aTrimmedName[0] == '/')
		return -1;

	// make sure that two clients don't have the same name
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (i != ClientID && m_aClients[i].m_State >= CClient::STATE_READY)
		{
			if (str_utf8_comp_confusable(aTrimmedName, m_aClients[i].m_aName) == 0)
				return -1;
		}
	}

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "'%s' -> '%s'", pName, aTrimmedName);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);
	pName = aTrimmedName;

	// set the client name
	str_utf8_copy_num(m_aClients[ClientID].m_aName, pName, sizeof(m_aClients[ClientID].m_aName), MAX_NAME_LENGTH);
	return 0;
}

void CServer::SetClientName(int ClientID, const char *pName)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY || !pName)
		return;

	char aNameTry[MAX_NAME_ARRAY_SIZE];
	str_utf8_copy_num(aNameTry, pName, sizeof(aNameTry), MAX_NAME_LENGTH);
	if (TrySetClientName(ClientID, aNameTry))
	{
		// auto rename
		for (int i = 1;; i++)
		{
			str_format(aNameTry, sizeof(aNameTry), "(%d)%s", i, pName);
			if (TrySetClientName(ClientID, aNameTry) == 0)
				break;
		}
	}
}

void CServer::SetClientClan(int ClientID, const char *pClan)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY || !pClan)
		return;

	str_utf8_copy_num(m_aClients[ClientID].m_aClan, pClan, sizeof(m_aClients[ClientID].m_aClan), MAX_CLAN_LENGTH);
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
	m_aClients[ClientID].m_Score = Score;
}

void CServer::Kick(int ClientID, const char *pReason)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CClient::STATE_EMPTY)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "invalid client id to kick");
		return;
	}
	else if(m_RconClientID == ClientID)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "you can't kick yourself");
 		return;
	}
	else if(m_aClients[ClientID].m_Authed > m_RconAuthLevel)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "kick command denied");
 		return;
	}
	else if (m_aClients[ClientID].m_State == CClient::STATE_DUMMY)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "you can't kick dummies");
		return;
	}

	m_NetServer.Drop(ClientID, pReason);
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
		m_aClients[i].m_State = CClient::STATE_EMPTY;
		m_aClients[i].m_aName[0] = 0;
		m_aClients[i].m_aClan[0] = 0;
		m_aClients[i].m_Country = -1;
		m_aClients[i].m_Snapshots.Init();
		m_aClients[i].m_ShowIps = false;
		m_aClients[i].m_Traffic = 0;
		m_aClients[i].m_TrafficSince = 0;
		m_aClients[i].m_Sevendown = false;
	}

	m_AnnouncementLastLine = 0;
	m_CurrentGameTick = 0;

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

	if(m_aClients[ClientID].m_State == CClient::STATE_INGAME)
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
			else if (MsgId == NETMSG_CAPABILITIES)
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
			else if (MsgId == NETMSGTYPE_SV_TEAMSSTATE)
				MsgId = 30;
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

		// write message to demo recorder
		if(!(Flags&MSGFLAG_NORECORD))
			m_DemoRecorder.RecordMessage(Pack.Data(), Pack.Size());

		if(!(Flags&MSGFLAG_NOSEND))
			m_NetServer.Send(&Packet);
	}

	return 0;
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
		if(m_aClients[i].m_State != CClient::STATE_INGAME)
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

int CServer::NewClientCallback(int ClientID, bool Sevendown, void *pUser)
{
	CServer *pThis = (CServer *)pUser;

	// Remove non human player on same slot
	if(pThis->GameServer()->IsClientBot(ClientID))
	{
		pThis->GameServer()->OnClientDrop(ClientID, "removing dummy");
	}

	pThis->m_aClients[ClientID].m_State = CClient::STATE_PREAUTH;
	pThis->m_aClients[ClientID].m_aName[0] = 0;
	pThis->m_aClients[ClientID].m_aClan[0] = 0;
	pThis->m_aClients[ClientID].m_Country = -1;
	pThis->m_aClients[ClientID].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientID].m_AuthTries = 0;
	pThis->m_aClients[ClientID].m_pRconCmdToSend = 0;
	pThis->m_aClients[ClientID].m_pMapListEntryToSend = 0;
	pThis->m_aClients[ClientID].m_NoRconNote = false;
	pThis->m_aClients[ClientID].m_Quitting = false;
	pThis->m_aClients[ClientID].m_ShowIps = false;
	pThis->m_aClients[ClientID].m_Traffic = 0;
	pThis->m_aClients[ClientID].m_TrafficSince = 0;
	pThis->m_aClients[ClientID].m_Sevendown = Sevendown;
	pThis->m_aClients[ClientID].Reset();
	pThis->GameServer()->OnClientEngineJoin(ClientID);

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

	pThis->m_aClients[ClientID].m_State = CClient::STATE_EMPTY;
	pThis->m_aClients[ClientID].m_aName[0] = 0;
	pThis->m_aClients[ClientID].m_aClan[0] = 0;
	pThis->m_aClients[ClientID].m_Country = -1;
	pThis->m_aClients[ClientID].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientID].m_AuthTries = 0;
	pThis->m_aClients[ClientID].m_pRconCmdToSend = 0;
	pThis->m_aClients[ClientID].m_pMapListEntryToSend = 0;
	pThis->m_aClients[ClientID].m_NoRconNote = false;
	pThis->m_aClients[ClientID].m_Quitting = false;
	pThis->m_aClients[ClientID].m_ShowIps = false;
	pThis->m_aClients[ClientID].m_Traffic = 0;
	pThis->m_aClients[ClientID].m_TrafficSince = 0;
	pThis->m_aClients[ClientID].m_Snapshots.PurgeAll();
	pThis->m_aClients[ClientID].m_Sevendown = false;
	pThis->GameServer()->OnClientEngineDrop(ClientID, pReason);

	return 0;
}

void CServer::GetMapInfo(char *pMapName, int MapNameSize, int *pMapSize, SHA256_DIGEST *pMapSha256, int *pMapCrc)
{
	str_copy(pMapName, GetMapName(), MapNameSize);
	*pMapSize = m_CurrentMapSize;
	*pMapSha256 = m_CurrentMapSha256;
	*pMapCrc = m_CurrentMapCrc;
}

void CServer::SendCapabilities(int ClientID)
{
	CMsgPacker Msg(NETMSG_CAPABILITIES, true);
	Msg.AddInt(SERVERCAP_CURVERSION); // version
	Msg.AddInt(SERVERCAPFLAG_DDNET | SERVERCAPFLAG_CHATTIMEOUTCODE | SERVERCAPFLAG_ANYPLAYERFLAG); // flags
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CServer::SendMap(int ClientID)
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
		else if(Msg == 26) // CL_ISDDNET
			Msg += NUM_NETMSGTYPES;
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
			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientID].m_State == CClient::STATE_PREAUTH)
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
				m_aClients[ClientID].m_State = CClient::STATE_AUTH;
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

				m_aClients[ClientID].m_Version = Unpacker.GetInt();

				m_aClients[ClientID].m_State = CClient::STATE_CONNECTING;
				SendCapabilities(ClientID);
				SendMap(ClientID);
			}
		}
		else if(Msg == NETMSG_REQUEST_MAP_DATA)
		{
			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && (m_aClients[ClientID].m_State == CClient::STATE_CONNECTING || m_aClients[ClientID].m_State == CClient::STATE_CONNECTING_AS_SPEC))
			{
				int ChunkSize = m_aClients[ClientID].m_Sevendown ? 1024-128 : MAP_CHUNK_SIZE;

				// send map chunks
				for(int i = 0; i < m_MapChunksPerRequest && m_aClients[ClientID].m_MapChunk >= 0; ++i)
				{
					int Chunk = m_aClients[ClientID].m_Sevendown ? Unpacker.GetInt() : m_aClients[ClientID].m_MapChunk;
					unsigned int Offset = Chunk * ChunkSize;
					int Last = 0;

					if (m_aClients[ClientID].m_Sevendown)
					{
						// drop faulty map data requests
						if(Chunk < 0 || (int)Offset > m_CurrentMapSize)
							return;
					}

					// check for last part
					if((int)Offset+ChunkSize >= m_CurrentMapSize)
					{
						ChunkSize = m_CurrentMapSize-Offset;
						Last = 1;
						m_aClients[ClientID].m_MapChunk = -1;
					}
					else
						m_aClients[ClientID].m_MapChunk++;

					CMsgPacker Msg(NETMSG_MAP_DATA, true);
					if (m_aClients[ClientID].m_Sevendown)
					{
						Msg.AddInt(Last);
						Msg.AddInt(m_CurrentMapCrc);
						Msg.AddInt(Chunk);
						Msg.AddInt(ChunkSize);
					}
					Msg.AddRaw(&m_pCurrentMapData[Offset], ChunkSize);
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
			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && (m_aClients[ClientID].m_State == CClient::STATE_CONNECTING || m_aClients[ClientID].m_State == CClient::STATE_CONNECTING_AS_SPEC))
			{
				char aAddrStr[NETADDR_MAXSTRSIZE];
				net_addr_str(m_NetServer.ClientAddr(ClientID), aAddrStr, sizeof(aAddrStr), true);

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "player is ready. ClientID=%d addr=<{%s}>", ClientID, aAddrStr);
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);

				bool ConnectAsSpec = m_aClients[ClientID].m_State == CClient::STATE_CONNECTING_AS_SPEC;
				m_aClients[ClientID].m_State = CClient::STATE_READY;
				GameServer()->OnClientConnected(ClientID, ConnectAsSpec);
				SendConnectionReady(ClientID);
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

			m_aClients[ClientID].m_LastAckedSnapshot = Unpacker.GetInt();
			int IntendedTick = Unpacker.GetInt();
			int Size = Unpacker.GetInt();

			// check for errors
			if(Unpacker.Error() || Size/4 > MAX_INPUT_SIZE)
				return;

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
			// we dont use the username thing yet, so unpack the empty username first
			if (m_aClients[ClientID].m_Sevendown)
				Unpacker.GetString(CUnpacker::SANITIZE_CC);
			const char *pAuth = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			if(!str_utf8_check(pAuth))
				return;

			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Error() == 0)
			{
				int AuthLevel = -1;
				int KeySlot = -1;

				// oy removed the usernames...
				// use "name:password" format until we establish new extended netmsg
				char aName[64] = {};
				const char *pDelim = str_find(pAuth, ":");
				const char *pPw = nullptr;
				if(!pDelim)
					pPw = pAuth;
				else
				{
					str_copy(aName, pAuth, min(sizeof(aName), (unsigned long)(pDelim - pAuth + 1)));
					pPw = pDelim + 1;
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

void CServer::GenerateServerInfo(CPacker *pPacker, int Token)
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
	int PlayerSlots = Config()->m_SvPlayerSlots;
	if(ClientCount >= VANILLA_MAX_CLIENTS)
	{
		if(ClientCount < MaxClients)
			ClientCount = VANILLA_MAX_CLIENTS - 1;
		else
			ClientCount = VANILLA_MAX_CLIENTS;
	}
	if(MaxClients > VANILLA_MAX_CLIENTS)
		MaxClients = VANILLA_MAX_CLIENTS;
	if(PlayerCount > ClientCount)
		PlayerCount = ClientCount;
	if (PlayerSlots > VANILLA_MAX_CLIENTS)
		PlayerSlots = VANILLA_MAX_CLIENTS;

	pPacker->AddInt(Config()->m_SvSkillLevel);	// server skill level
	pPacker->AddInt(PlayerCount); // num players
	pPacker->AddInt(PlayerSlots); // max players
	pPacker->AddInt(ClientCount); // num clients
	pPacker->AddInt(max(ClientCount, MaxClients)); // max clients

	if(Token != -1)
	{
		int Sent = 0;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if (Sent >= VANILLA_MAX_CLIENTS)
				break;

			if(m_aClients[i].m_State != CClient::STATE_EMPTY)
			{
				pPacker->AddString(ClientName(i), 0); // client name
				pPacker->AddString(ClientClan(i), 0); // client clan
				pPacker->AddInt(m_aClients[i].m_Country); // client country
				pPacker->AddInt(m_aClients[i].m_Score); // client score
				pPacker->AddInt(m_aClients[i].m_State == CClient::STATE_DUMMY ? 2 : GameServer()->IsClientPlayer(i)?0:1); // flag spectator=1, bot=2 (player=0)
				Sent++;
			}
		}
	}
}

void CServer::SendServerInfoSevendown(const NETADDR *pAddr, int Token, bool SendClients)
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
	p.AddString(GameServer()->GameType(), 16);
 
	ADD_INT(p, Config()->m_Password[0] ? SERVERINFO_FLAG_PASSWORD : 0);

	if(PlayerCount > ClientCount)
		PlayerCount = ClientCount;
	ADD_INT(p, PlayerCount);
	ADD_INT(p, max(Config()->m_SvPlayerSlots-DummyCount, PlayerCount));
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
			m_NetServer.Send(&Packet, NET_TOKEN_NONE, true); \
			PacketsSent++; \
		} while(0)

	#define RESET() \
		do \
		{ \
			pp.Reset(); \
			pp.AddRaw(pPrefix, PrefixSize); \
		} while(0)

	RESET();

	if (!SendClients)
	{
		SEND(pp.Size());
		return;
	}

	pPrefix = SERVERBROWSE_INFO_EXTENDED_MORE;
	PrefixSize = sizeof(SERVERBROWSE_INFO_EXTENDED_MORE);

	int Sent = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// DDNet clients can show x/128 as playercount, but the client info is still limited to 64
		if (Sent >= VANILLA_MAX_CLIENTS)
			break;

		if(m_aClients[i].m_State != CClient::STATE_EMPTY && m_aClients[i].m_State != CClient::STATE_DUMMY)
		{
			int PreviousSize = pp.Size();

			pp.AddString(ClientName(i), MAX_NAME_LENGTH);
			pp.AddString(ClientClan(i), MAX_CLAN_LENGTH);

			ADD_INT(pp, m_aClients[i].m_Country);
			// 0 means CPlayer::SCORE_TIME, so the other score modes use scoreformat instead of time format
			// thats why we just send -9999, because it will be displayed as nothing
			int Score = -9999;
			if (Config()->m_SvDefaultScoreMode == 0 && m_aClients[i].m_Score != -1)
				Score = abs(m_aClients[i].m_Score) * -1;
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
			Sent++;
		}
	}

	SEND(pp.Size());
	#undef SEND
	#undef RESET
	#undef ADD_RAW
	#undef ADD_INT
}

void CServer::SendServerInfo(int ClientID)
{
	CMsgPacker Msg(NETMSG_SERVERINFO, true);
	GenerateServerInfo(&Msg, -1);
	if(ClientID == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_aClients[i].m_State != CClient::STATE_EMPTY)
			{
				if (m_aClients[i].m_Sevendown)
					SendServerInfoSevendown(m_NetServer.ClientAddr(i), -1, true);
				else
					SendMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH, i);
			}
		}
	}
	else if(ClientID >= 0 && ClientID < MAX_CLIENTS && m_aClients[ClientID].m_State != CClient::STATE_EMPTY)
	{
		if (m_aClients[ClientID].m_Sevendown)
			SendServerInfoSevendown(m_NetServer.ClientAddr(ClientID), -1, true);
		else
			SendMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID);
	}
}


void CServer::PumpNetwork()
{
	CNetChunk Packet;
	TOKEN ResponseToken;
	bool Sevendown;

	m_NetServer.Update();

	// process packets
	while(m_NetServer.Recv(&Packet, &ResponseToken, &Sevendown))
	{
		if(Packet.m_Flags&NETSENDFLAG_CONNLESS)
		{
			if (Sevendown)
			{
				if(m_RegisterSevendown.RegisterProcessPacket(&Packet, ResponseToken))
					continue;
			}
			else
			{
				if(m_Register.RegisterProcessPacket(&Packet, ResponseToken))
					continue;
			}

			if(Packet.m_DataSize >= int(sizeof(SERVERBROWSE_GETINFO)) &&
				mem_comp(Packet.m_pData, SERVERBROWSE_GETINFO, sizeof(SERVERBROWSE_GETINFO)) == 0)
			{
				CUnpacker Unpacker;
				Unpacker.Reset((unsigned char*)Packet.m_pData+sizeof(SERVERBROWSE_GETINFO), Packet.m_DataSize-sizeof(SERVERBROWSE_GETINFO));

				int SrvBrwsToken;
				if (Sevendown)
				{
					if (!Config()->m_SvAllowSevendown)
						continue;

					bool SendClients = true;
					if(Config()->m_SvServerInfoPerSecond)
					{
						SendClients = m_ServerInfoNumRequests <= Config()->m_SvServerInfoPerSecond;
						const int64 Now = Tick();

						if (Now <= m_ServerInfoFirstRequest + TickSpeed())
						{
							m_ServerInfoNumRequests++;
						}
						else
						{
							m_ServerInfoNumRequests = 1;
							m_ServerInfoFirstRequest = Now;
						}
					}

					int ExtraToken = (Packet.m_aExtraData[0] << 8) | Packet.m_aExtraData[1];
					SrvBrwsToken = ((unsigned char*)Packet.m_pData)[sizeof(SERVERBROWSE_GETINFO)];
					SrvBrwsToken |= ExtraToken << 8;
					SendServerInfoSevendown(&Packet.m_Address, SrvBrwsToken, SendClients);
				}
				else
				{
					SrvBrwsToken = Unpacker.GetInt();
					if (Unpacker.Error())
						continue;

					CPacker Packer;
					CNetChunk Response;

					GenerateServerInfo(&Packer, SrvBrwsToken);

					Response.m_ClientID = -1;
					Response.m_Address = Packet.m_Address;
					Response.m_Flags = NETSENDFLAG_CONNLESS;
					Response.m_pData = Packer.Data();
					Response.m_DataSize = Packer.Size();
					m_NetServer.Send(&Response, ResponseToken);
				}
			}
		}
		else
			ProcessClientPacket(&Packet);
	}

	m_ServerBan.Update();
	m_Econ.Update();
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

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_aClients[i].m_State == CClient::STATE_DUMMY)
			DummyLeave(i);
	}

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

	str_copy(m_aCurrentMap, pMapName, sizeof(m_aCurrentMap));

	// load complete map into memory for download
	{
		IOHANDLE File = Storage()->OpenFile(aBuf, IOFLAG_READ, IStorage::TYPE_ALL);
		m_CurrentMapSize = (int)io_length(File);
		if(m_pCurrentMapData)
			mem_free(m_pCurrentMapData);
		m_pCurrentMapData = (unsigned char *)mem_alloc(m_CurrentMapSize, 1);
		io_read(File, m_pCurrentMapData, m_CurrentMapSize);
		io_close(File);
	}
	return 1;
}

void CServer::InitRegister(CNetServer *pNetServer, IEngineMasterServer *pMasterServer, CConfig *pConfig, IConsole *pConsole)
{
	m_Register.Init(pNetServer, pMasterServer, pConfig, pConsole);
	m_RegisterSevendown.Init(pNetServer, pMasterServer, pConfig, pConsole);
}

void CServer::InitInterfaces(CConfig *pConfig, IConsole *pConsole, IGameServer *pGameServer, IEngineMap *pMap, IStorage *pStorage)
{
	m_pConfig = pConfig;
	m_pConsole = pConsole;
	m_pGameServer = pGameServer;
	m_pMap = pMap;
	m_pStorage = pStorage;
}

int CServer::Run()
{
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
		Config()->m_SvMaxClients, Config()->m_SvMaxClientsPerIP, NewClientCallback, DelClientCallback, this))
	{
		dbg_msg("server", "couldn't open socket. port %d might already be in use", Config()->m_SvPort);
		return -1;
	}

	m_Econ.Init(Config(), Console(), &m_ServerBan);

#if defined(CONF_FAMILY_UNIX)
	m_Fifo.Init(Console(), Config()->m_SvInputFifo, CFGFLAG_SERVER);
#endif

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "server name is '%s'", Config()->m_SvName);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	GameServer()->OnInit();
	str_format(aBuf, sizeof(aBuf), "netversion %s", GameServer()->NetVersion());
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
	str_format(aBuf, sizeof(aBuf), "game version %s", GameServer()->Version());
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	// process pending commands
	m_pConsole->StoreCommands(false);

	if(m_AuthManager.IsGenerated())
	{
		dbg_msg("server", "+-------------------------+");
		dbg_msg("server", "| rcon password: '%s' |", Config()->m_SvRconPassword);
		dbg_msg("server", "+-------------------------+");
	}

	// start game
	{
		m_GameStartTime = time_get();

		while(m_RunServer)
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
					m_ServerInfoFirstRequest = 0;
					Kernel()->ReregisterInterface(GameServer());
					GameServer()->OnInit();
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
			m_Register.RegisterUpdate(m_NetServer.NetType());
			m_RegisterSevendown.RegisterUpdate(m_NetServer.NetType());

			PumpNetwork();

			if(Config()->m_SvShutdownWhenEmpty)
			{
				bool ServerEmpty = true;

				for(int c = 0; c < MAX_CLIENTS; c++)
					if(m_aClients[c].m_State != CClient::STATE_EMPTY && m_aClients[c].m_State != CClient::STATE_DUMMY)
						ServerEmpty = false;

				if(ServerEmpty)
					m_RunServer = false;
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
	if(m_pMapListHeap)
	{
		delete m_pMapListHeap;
		m_pMapListHeap = 0;
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
	if(pResult->NumArguments() > 1)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Kicked (%s)", pResult->GetString(1));
		((CServer *)pUser)->Kick(pResult->GetInteger(0), aBuf);
	}
	else
		((CServer *)pUser)->Kick(pResult->GetInteger(0), "Kicked by console");
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

				str_format(aBuf, sizeof(aBuf), "id=%d addr=<{%s}> client=%s sevendown=%d name='%s' score=%d %s", i, aAddrStr,
						pThis->GetClientVersionStr(i), (int)pThis->m_aClients[i].m_Sevendown, pThis->m_aClients[i].m_aName, pThis->m_aClients[i].m_Score, aAuthStr);
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
	((CServer *)pUser)->m_RunServer = false;
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

	if(pManager->AddKey(pIdent, pPw, Level) < 0)
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ident already exists");
	else
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "key added");
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

	if(pManager->AddKeyHash(pIdent, Hash, aSalt, Level) < 0)
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ident already exists");
	else
	{
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
	CServer* pServer = (CServer *)pUser;
	char aFilename[128];
	if(pResult->NumArguments())
		str_format(aFilename, sizeof(aFilename), "demos/%s.demo", pResult->GetString(0));
	else
	{
		char aDate[20];
		str_timestamp(aDate, sizeof(aDate));
		str_format(aFilename, sizeof(aFilename), "demos/demo_%s.demo", aDate);
	}
	pServer->m_DemoRecorder.Start(pServer->Storage(), pServer->Console(), aFilename, pServer->GameServer()->NetVersion(), pServer->m_aCurrentMap, pServer->m_CurrentMapSha256, pServer->m_CurrentMapCrc, "server");
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
		pSelf->SendServerInfo(-1);
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

void CServer::RegisterCommands()
{
	// register console commands
	Console()->Register("kick", "i[id] ?r[reason]", CFGFLAG_SERVER, ConKick, this, "Kick player with specified id for any reason", AUTHED_ADMIN);
	Console()->Register("status", "?r[name]", CFGFLAG_SERVER, ConStatus, this, "List players containing name or all players", AUTHED_HELPER);
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
	IEngineMasterServer *pEngineMasterServer = CreateEngineMasterServer();
	IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_SERVER, argc, argv); // ignore_convention
	IConfigManager *pConfigManager = CreateConfigManager();

	pServer->InitRegister(&pServer->m_NetServer, pEngineMasterServer, pConfigManager->Values(), pConsole);

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
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineMasterServer*>(pEngineMasterServer)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IMasterServer*>(pEngineMasterServer));

		if(RegisterFail)
			return -1;
	}

	pEngine->Init();
	pConfigManager->Init(FlagMask);
	pConsole->Init();
	pEngineMasterServer->Init();
	pEngineMasterServer->Load();

	pServer->InitInterfaces(pConfigManager->Values(), pConsole, pGameServer, pEngineMap, pStorage);
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
	delete pEngineMasterServer;
	delete pStorage;
	delete pConfigManager;

	return Ret;
}

// F-DDrace

void CServer::GetClientAddr(int ClientID, NETADDR* pAddr)
{
	if (ClientID >= 0 && ClientID < MAX_CLIENTS && m_aClients[ClientID].m_State == CClient::STATE_INGAME)
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
	if (v.size() == 1)
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
			Rand = rand() % v.size();
		while (Rand == m_AnnouncementLastLine);

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

	if(m_aClients[OrigID].m_Authed != AUTHED_NO)
	{
		LogoutClient(ClientID, "Timeout Protection");
	}

	// important ot call OnSetTimedOut before we remove the original client but after we swapped already
	GameServer()->OnSetTimedOut(ClientID, OrigID);

	DelClientCallback(OrigID, "Timeout Protection used", this);
	//m_aClients[ClientID].m_Flags = m_aClients[OrigID].m_Flags;
	return true;
}

int* CServer::GetIdMap(int ClientID)
{
	return (int*)(IdMap + VANILLA_MAX_CLIENTS * ClientID);
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
		"55", "66", "67", "68", "69", "70", "71", "72", "73", "74", "75", "76", "77", "78", "79", "80",
		"81", "82", "83", "84", "85", "86", "87", "88", "89", "90", "91", "92", "93", "94", "95", "96",
		"97", "98", "99", "100", "101", "102", "103", "104", "105", "106", "107", "108", "109", "110", "111", "112",
		"113", "114", "115", "116", "117", "118", "119", "120", "121", "122", "123", "124", "125", "126", "127", "128",
	};

	m_NetServer.DummyInit(DummyID);
	m_aClients[DummyID].m_State = CClient::STATE_DUMMY;
	m_aClients[DummyID].m_Authed = AUTHED_NO;
	m_aClients[DummyID].m_Sevendown = false;

	str_utf8_copy_num(m_aClients[DummyID].m_aName, pNames[DummyID], sizeof(m_aClients[DummyID].m_aName), MAX_NAME_LENGTH);
	str_utf8_copy_num(m_aClients[DummyID].m_aClan, pClans[DummyID], sizeof(m_aClients[DummyID].m_aClan), MAX_CLAN_LENGTH);
}

void CServer::DummyLeave(int DummyID)
{
	GameServer()->OnClientDrop(DummyID, "");

	m_aClients[DummyID].m_State = CClient::STATE_EMPTY;
	m_aClients[DummyID].m_aName[0] = 0;
	m_aClients[DummyID].m_aClan[0] = 0;
	m_aClients[DummyID].m_Country = -1;
	m_aClients[DummyID].m_Authed = AUTHED_NO;
	m_aClients[DummyID].m_AuthTries = 0;
	m_aClients[DummyID].m_pRconCmdToSend = 0;
	m_aClients[DummyID].m_pMapListEntryToSend = 0;
	m_aClients[DummyID].m_NoRconNote = false;
	m_aClients[DummyID].m_Quitting = false;
	m_aClients[DummyID].m_ShowIps = false;
	m_aClients[DummyID].m_Traffic = 0;
	m_aClients[DummyID].m_TrafficSince = 0;
	m_aClients[DummyID].m_Snapshots.PurgeAll();
	m_aClients[DummyID].m_Sevendown = false;

	m_NetServer.DummyDelete(DummyID);
}

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
