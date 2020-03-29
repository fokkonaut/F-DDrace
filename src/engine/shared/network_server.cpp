/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/system.h>

#include <engine/console.h>

#include "netban.h"
#include "network.h"
#include "config.h"

#include <engine/shared/protocol.h>
#include <generated/protocol.h>

static unsigned char MsgTypeFromSevendown(unsigned char Byte)
{
	unsigned char Seven = Byte>>1;
	unsigned char Msg;
	if (Byte&1)
	{
		if(Seven == 1)
			Msg = NETMSG_INFO;
		else if(Seven >= 14 && Seven <= 24)
			Msg = NETMSG_READY + Seven - 14;
		else
			return 0;
	}
	else
	{
		if(Seven >= 17 && Seven <= 20)
			Msg = NETMSGTYPE_CL_SAY + Seven - 17;
		else if(Seven == 22)
			Msg = NETMSGTYPE_CL_KILL;
		else if(Seven >= 23 && Seven <= 25)
			Msg = NETMSGTYPE_CL_EMOTICON + Seven - 23;
		else
			return 0;
	}
	return (Msg<<1) | (Byte&1);
}

static unsigned char MsgTypeToSevendown(unsigned char Byte)
{
	unsigned char Msg = Byte>>1;
	unsigned char Seven;
	if (Byte&1)
	{
		if(Msg >= NETMSG_MAP_CHANGE && Msg <= NETMSG_MAP_DATA)
			Seven = Msg;
		else if(Msg >= NETMSG_CON_READY && Msg <= NETMSG_INPUTTIMING)
			Seven = Msg - 1;
		else if(Msg >= NETMSG_AUTH_CHALLANGE && Msg <= NETMSG_AUTH_RESULT)
			Seven = Msg - 4;
		else if(Msg >= NETMSG_PING && Msg <= NETMSG_ERROR)
			Seven = Msg - 4;
		else if(Msg > 24)
			Seven = Msg - 24;
		else
			return 0;
	}
	else
	{
		if(Msg >= NETMSGTYPE_SV_MOTD && Msg <= NETMSGTYPE_SV_CHAT)
			Seven = Msg;
		else if(Msg == NETMSGTYPE_SV_KILLMSG)
			Seven = Msg - 1;
		else if(Msg >= NETMSGTYPE_SV_TUNEPARAMS && Msg <= NETMSGTYPE_SV_VOTESTATUS)
			Seven = Msg;
		else if(Msg > 24)
			Seven = Msg - 24;
		else
			return 0;
	}
	return (Seven<<1) | (Byte&1);
}

bool CNetServer::Open(NETADDR BindAddr, CConfig *pConfig, IConsole *pConsole, IEngine *pEngine, CNetBan *pNetBan,
	int MaxClients, int MaxClientsPerIP, NETFUNC_NEWCLIENT pfnNewClient, NETFUNC_DELCLIENT pfnDelClient, void *pUser)
{
	// zero out the whole structure
	mem_zero(this, sizeof(*this));

	// open socket
	NETSOCKET Socket = net_udp_create(BindAddr, 0);
	if(!Socket.type)
		return false;
	
	// init
	m_pNetBan = pNetBan;
	Init(Socket, pConfig, pConsole, pEngine);

	m_TokenManager.Init(this);
	m_TokenCache.Init(this, &m_TokenManager);

	m_NumClients = 0;
	SetMaxClients(MaxClients);
	SetMaxClientsPerIP(MaxClientsPerIP);

	for(int i = 0; i < NET_MAX_CLIENTS; i++)
		m_aSlots[i].m_Connection.Init(this, true);

	m_pfnNewClient = pfnNewClient;
	m_pfnDelClient = pfnDelClient;
	m_UserPtr = pUser;

	// F-DDrace
	m_ShutdownMessage[0] = '\0';

	return true;
}

void CNetServer::Close()
{
	for(int i = 0; i < NET_MAX_CLIENTS; i++)
		Drop(i, m_ShutdownMessage[0] != '\0' ? m_ShutdownMessage : "Server shutdown");

	Shutdown();
}

void CNetServer::Drop(int ClientID, const char *pReason)
{
	if(ClientID < 0 || ClientID >= NET_MAX_CLIENTS || m_aSlots[ClientID].m_Connection.State() == NET_CONNSTATE_OFFLINE)
		return;

	if(m_pfnDelClient)
		m_pfnDelClient(ClientID, pReason, m_UserPtr);

	m_aSlots[ClientID].m_Connection.Disconnect(pReason);
	m_NumClients--;
}

int CNetServer::Update()
{
	for(int i = 0; i < NET_MAX_CLIENTS; i++)
	{
		if(m_aSlots[i].m_Connection.State() == NET_CONNSTATE_OFFLINE)
			continue;

		m_aSlots[i].m_Connection.Update();
		if(m_aSlots[i].m_Connection.State() == NET_CONNSTATE_ERROR)
		{
			Drop(i, m_aSlots[i].m_Connection.ErrorString());
		}
	}

	m_TokenManager.Update();
	m_TokenCache.Update();

	return 0;
}

bool CNetServer::GetSevendown(const NETADDR *pAddr, CNetPacketConstruct *pPacket)
{
	if (pPacket->m_Flags&NET_PACKETFLAG_CONNLESS)
		return false;

	for(int i = 0; i < NET_MAX_CLIENTS; i++)
	{
		if(m_aSlots[i].m_Connection.State() == NET_CONNSTATE_OFFLINE)
			continue;

		if(net_addr_comp(m_aSlots[i].m_Connection.PeerAddress(), pAddr) == 0)
			return m_aSlots[i].m_Connection.m_Sevendown;
	}

	return !(pPacket->m_Flags&1);
}

/*
	TODO: chopp up this function into smaller working parts
*/
int CNetServer::Recv(CNetChunk *pChunk, TOKEN *pResponseToken)
{
	while(1)
	{
		// check for a chunk
		if(m_RecvUnpacker.FetchChunk(pChunk))
		{
			if(m_aSlots[pChunk->m_ClientID].m_Connection.m_Sevendown)
				*(unsigned char*)pChunk->m_pData = MsgTypeFromSevendown(*(unsigned char*)pChunk->m_pData);
			return 1;
		}

		// TODO: empty the recvinfo
		NETADDR Addr;
		bool Sevendown;
		int Result = UnpackPacket(&Addr, m_RecvUnpacker.m_aBuffer, &m_RecvUnpacker.m_Data, &Sevendown, this);
		// no more packets for now
		if(Result > 0)
			break;

		if(!Result)
		{
			// check for bans
			char aBuf[128];
			int LastInfoQuery;
			if(NetBan() && NetBan()->IsBanned(&Addr, aBuf, sizeof(aBuf), &LastInfoQuery))
			{
				// banned, reply with a message (5 second cooldown)
				int Time = time_timestamp();
				if(LastInfoQuery + 5 < Time)
				{
					SendControlMsg(&Addr, m_RecvUnpacker.m_Data.m_ResponseToken, 0, NET_CTRLMSG_CLOSE, aBuf, str_length(aBuf) + 1, Sevendown, NET_SECURITY_TOKEN_UNSUPPORTED);
				}
				continue;
			}

			bool Found = false;
			// try to find matching slot
			for(int i = 0; i < NET_MAX_CLIENTS; i++)
			{
				if(m_aSlots[i].m_Connection.State() == NET_CONNSTATE_OFFLINE)
					continue;

				if(net_addr_comp(m_aSlots[i].m_Connection.PeerAddress(), &Addr) == 0)
				{
					if(m_aSlots[i].m_Connection.Feed(&m_RecvUnpacker.m_Data, &Addr, Sevendown))
					{
						if(m_RecvUnpacker.m_Data.m_DataSize)
						{
							if(!(m_RecvUnpacker.m_Data.m_Flags&NET_PACKETFLAG_CONNLESS))
								m_RecvUnpacker.Start(&Addr, &m_aSlots[i].m_Connection, i);
							else
							{
								pChunk->m_Flags = NETSENDFLAG_CONNLESS;
								pChunk->m_Address = *m_aSlots[i].m_Connection.PeerAddress();
								pChunk->m_ClientID = i;
								pChunk->m_DataSize = m_RecvUnpacker.m_Data.m_DataSize;
								pChunk->m_pData = m_RecvUnpacker.m_Data.m_aChunkData;
								if(pResponseToken)
									*pResponseToken = NET_TOKEN_NONE;
								return 1;
							}
						}
					}
					Found = true;
				}
			}

			if(Found)
				continue;

			if (!Sevendown)
			{
				int Accept = m_TokenManager.ProcessMessage(&Addr, &m_RecvUnpacker.m_Data);
				if(Accept <= 0)
					continue;
			}

			if(m_RecvUnpacker.m_Data.m_Flags&NET_PACKETFLAG_CONTROL)
			{
				if(m_RecvUnpacker.m_Data.m_aChunkData[0] == NET_CTRLMSG_CONNECT)
				{
					if (Connlimit(Addr))
					{
						const char LimitMsg[] = "Too many connections in a short time";
						SendControlMsg(&Addr, m_RecvUnpacker.m_Data.m_ResponseToken, 0, NET_CTRLMSG_CLOSE, LimitMsg, str_length(LimitMsg) + 1, Sevendown, NET_SECURITY_TOKEN_UNSUPPORTED);
						continue; // failed to add client
					}

					// check if there are free slots
					if(m_NumClients >= m_MaxClients)
					{
						const char FullMsg[] = "This server is full";
						SendControlMsg(&Addr, m_RecvUnpacker.m_Data.m_ResponseToken, 0, NET_CTRLMSG_CLOSE, FullMsg, sizeof(FullMsg), Sevendown, NET_SECURITY_TOKEN_UNSUPPORTED);
						continue;
					}

					// only allow a specific number of players with the same ip
					NETADDR ThisAddr = Addr, OtherAddr;
					int FoundAddr = 1;
					ThisAddr.port = 0;
					for(int i = 0; i < NET_MAX_CLIENTS; i++)
					{
						if(m_aSlots[i].m_Connection.State() == NET_CONNSTATE_OFFLINE)
							continue;

						OtherAddr = *m_aSlots[i].m_Connection.PeerAddress();
						OtherAddr.port = 0;
						if(!net_addr_comp(&ThisAddr, &OtherAddr))
						{
							if(FoundAddr++ >= m_MaxClientsPerIP)
							{
								char aBuf[128];
								str_format(aBuf, sizeof(aBuf), "Only %d players with the same IP are allowed", m_MaxClientsPerIP);
								SendControlMsg(&Addr, m_RecvUnpacker.m_Data.m_ResponseToken, 0, NET_CTRLMSG_CLOSE, aBuf, str_length(aBuf) + 1, Sevendown, NET_SECURITY_TOKEN_UNSUPPORTED);
								return 0;
							}
						}
					}

					for(int i = 0; i < NET_MAX_CLIENTS; i++)
					{
						if(m_aSlots[i].m_Connection.State() == NET_CONNSTATE_OFFLINE)
						{
							m_NumClients++;
							m_aSlots[i].m_Connection.SetToken(m_RecvUnpacker.m_Data.m_Token);
							SECURITY_TOKEN SecurityToken = Sevendown ? m_TokenManager.GenerateToken(&Addr) : NET_SECURITY_TOKEN_UNSUPPORTED;
							m_aSlots[i].m_Connection.Feed(&m_RecvUnpacker.m_Data, &Addr, Sevendown, SecurityToken);
							if(m_pfnNewClient)
								m_pfnNewClient(i, Sevendown, m_UserPtr);
							break;
						}
					}					
				}
				else if(m_RecvUnpacker.m_Data.m_aChunkData[0] == NET_CTRLMSG_TOKEN)
					m_TokenCache.AddToken(&Addr, m_RecvUnpacker.m_Data.m_ResponseToken, NET_TOKENFLAG_RESPONSEONLY);
			}
			else if(m_RecvUnpacker.m_Data.m_Flags&NET_PACKETFLAG_CONNLESS)
			{
				pChunk->m_Flags = NETSENDFLAG_CONNLESS;
				pChunk->m_ClientID = -1;
				pChunk->m_Address = Addr;
				pChunk->m_DataSize = m_RecvUnpacker.m_Data.m_DataSize;
				pChunk->m_pData = m_RecvUnpacker.m_Data.m_aChunkData;
				if(pResponseToken)
					*pResponseToken = m_RecvUnpacker.m_Data.m_ResponseToken;
				return 1;
			}
		}
	}
	return 0;
}

int CNetServer::Send(CNetChunk *pChunk, TOKEN Token)
{
	if(pChunk->m_Flags&NETSENDFLAG_CONNLESS)
	{
		if(pChunk->m_DataSize >= NET_MAX_PAYLOAD)
		{
			dbg_msg("netserver", "packet payload too big. %d. dropping packet", pChunk->m_DataSize);
			return -1;
		}

		if(pChunk->m_ClientID == -1)
		{
			for(int i = 0; i < NET_MAX_CLIENTS; i++)
			{
				if(m_aSlots[i].m_Connection.State() == NET_CONNSTATE_OFFLINE)
					continue;

				if(net_addr_comp(&pChunk->m_Address, m_aSlots[i].m_Connection.PeerAddress()) == 0)
				{
					// upgrade the packet, now that we know its recipent
					pChunk->m_ClientID = i;
					break;
				}
			}
		}

		if(Token != NET_TOKEN_NONE)
		{
			SendPacketConnless(&pChunk->m_Address, Token, m_TokenManager.GenerateToken(&pChunk->m_Address), pChunk->m_pData, pChunk->m_DataSize);
		}
		else
		{
			if(pChunk->m_ClientID == -1)
			{
				m_TokenCache.SendPacketConnless(&pChunk->m_Address, pChunk->m_pData, pChunk->m_DataSize);
			}
			else
			{
				dbg_assert(pChunk->m_ClientID >= 0, "errornous client id");
				dbg_assert(pChunk->m_ClientID < NET_MAX_CLIENTS, "errornous client id");
				dbg_assert(m_aSlots[pChunk->m_ClientID].m_Connection.State() != NET_CONNSTATE_OFFLINE, "errornous client id");

				m_aSlots[pChunk->m_ClientID].m_Connection.SendPacketConnless((const char *)pChunk->m_pData, pChunk->m_DataSize);
			}
		}
	}
	else
	{
		if (m_aSlots[pChunk->m_ClientID].m_Connection.State() == NET_CONNSTATE_DUMMY)
			return -1;

		if(pChunk->m_DataSize+NET_MAX_CHUNKHEADERSIZE >= NET_MAX_PAYLOAD)
		{
			dbg_msg("netclient", "chunk payload too big. %d. dropping chunk", pChunk->m_DataSize);
			return -1;
		}

		int Flags = 0;
		dbg_assert(pChunk->m_ClientID >= 0, "errornous client id");
		dbg_assert(pChunk->m_ClientID < NET_MAX_CLIENTS, "errornous client id");
		dbg_assert(m_aSlots[pChunk->m_ClientID].m_Connection.State() != NET_CONNSTATE_OFFLINE, "errornous client id");

		if(m_aSlots[pChunk->m_ClientID].m_Connection.m_Sevendown)
		{
			unsigned int MsgType = MsgTypeToSevendown(*(unsigned char*)pChunk->m_pData);
			if (MsgType == 0) return 0;
			*(unsigned char*)pChunk->m_pData = MsgType;
		}

		if(pChunk->m_Flags&NETSENDFLAG_VITAL)
			Flags = NET_CHUNKFLAG_VITAL;

		if(m_aSlots[pChunk->m_ClientID].m_Connection.QueueChunk(Flags, pChunk->m_DataSize, pChunk->m_pData) == 0)
		{
			if(pChunk->m_Flags&NETSENDFLAG_FLUSH)
				m_aSlots[pChunk->m_ClientID].m_Connection.Flush();
		}
		else
		{
			Drop(pChunk->m_ClientID, "Error sending data");
		}
	}
	return 0;
}

void CNetServer::SetMaxClients(int MaxClients)
{
	m_MaxClients = clamp(MaxClients, 1, int(NET_MAX_CLIENTS));
}

void CNetServer::SetMaxClientsPerIP(int MaxClientsPerIP)
{
	m_MaxClientsPerIP = clamp(MaxClientsPerIP, 1, int(NET_MAX_CLIENTS));
}

// F-DDrace
void CNetServer::DummyInit(int DummyID)
{
	m_aSlots[DummyID].m_Connection.DummyConnect();
	m_NumClients++;
}

void CNetServer::DummyDelete(int DummyID)
{
	m_aSlots[DummyID].m_Connection.DummyDrop();
	m_NumClients--;
}

bool CNetServer::Connlimit(NETADDR Addr)
{
	int64 Now = time_get();
	int Oldest = 0;

	for(int i = 0; i < NET_CONNLIMIT_IPS; ++i)
	{
		if(!net_addr_comp(&m_aSpamConns[i].m_Addr, &Addr))
		{
			if(m_aSpamConns[i].m_Time > Now - time_freq() * CNetBase::Config()->m_SvConnlimitTime)
			{
				if(m_aSpamConns[i].m_Conns >= CNetBase::Config()->m_SvConnlimit)
					return true;
			}
			else
			{
				m_aSpamConns[i].m_Time = Now;
				m_aSpamConns[i].m_Conns = 0;
			}
			m_aSpamConns[i].m_Conns++;
			return false;
		}

		if(m_aSpamConns[i].m_Time < m_aSpamConns[Oldest].m_Time)
			Oldest = i;
	}

	m_aSpamConns[Oldest].m_Addr = Addr;
	m_aSpamConns[Oldest].m_Time = Now;
	m_aSpamConns[Oldest].m_Conns = 1;
	return false;
}
