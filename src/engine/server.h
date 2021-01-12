/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_H
#define ENGINE_SERVER_H

#include <base/hash.h>

#include "kernel.h"
#include "message.h"

#include <generated/protocol.h>
#include <engine/shared/protocol.h>
#include <base/math.h>

class IServer : public IInterface
{
	MACRO_INTERFACE("server", 0)
protected:
	int m_CurrentGameTick;
	int m_TickSpeed;

public:
	/*
		Structure: CClientInfo
	*/
	struct CClientInfo
	{
		const char *m_pName;
		int m_Latency;
		bool m_GotDDNetVersion;
		int m_DDNetVersion;
		const char *m_pDDNetVersionStr;
		const CUuid *m_pConnectionID;
	};

	int Tick() const { return m_CurrentGameTick; }
	int TickSpeed() const { return m_TickSpeed; }

	virtual const char *ClientName(int ClientID) const = 0;
	virtual const char *ClientClan(int ClientID) const = 0;
	virtual int ClientCountry(int ClientID) const = 0;
	virtual bool ClientIngame(int ClientID) const = 0;
	virtual int GetClientInfo(int ClientID, CClientInfo *pInfo) const = 0;
	virtual void GetClientAddr(int ClientID, char *pAddrStr, int Size, bool AddPort = false) const = 0;
	virtual int GetClientVersion(int ClientID) const = 0;
	virtual void SetClientDDNetVersion(int ClientID, int DDNetVersion) = 0;
	virtual void RestrictRconOutput(int ClientID) = 0;

	// F-DDrace

	virtual void GetClientAddr(int ClientID, NETADDR* pAddr) = 0;
	virtual const char* GetAnnouncementLine(char const* FileName) = 0;

	virtual int *GetIdMap(int ClientID) = 0;

	virtual void DummyJoin(int DummyID) = 0;
	virtual void DummyLeave(int DummyID) = 0;

	virtual bool IsSevendown(int ClientID) = 0;

	virtual int SendMsg(CMsgPacker *pMsg, int Flags, int ClientID) = 0;

	template<class T>
	int SendPackMsg(T *pMsg, int Flags, int ClientID)
	{
		int Result = 0;
		T Tmp;
		if (ClientID == -1)
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
				if(ClientIngame(i))
				{
					mem_copy(&Tmp, pMsg, sizeof(T));
					Result = SendPackMsgTranslate(&Tmp, Flags, i);
				}
		}
		else
		{
			mem_copy(&Tmp, pMsg, sizeof(T));
			Result = SendPackMsgTranslate(&Tmp, Flags, ClientID);
		}
		return Result;
	}

	template<class T>
	int SendPackMsgTranslate(T *pMsg, int Flags, int ClientID)
	{
		return SendPackMsgOne(pMsg, Flags, ClientID);
	}

	template<class T>
	int SendPackMsgOne(T *pMsg, int Flags, int ClientID)
	{
		CMsgPacker Packer(pMsg->MsgID(), false);
		if(pMsg->Pack(&Packer))
			return -1;
		return SendMsg(&Packer, Flags, ClientID);
	}

	char msgbuf[1000];

	int SendPackMsgTranslate(CNetMsg_Sv_Chat *pMsg, int Flags, int ClientID)
	{
		// 128 player translation
		if (pMsg->m_Mode != CHAT_WHISPER && pMsg->m_ClientID >= 0 && ((pMsg->m_Mode == CHAT_TEAM && pMsg->m_ClientID != ClientID) || !Translate(pMsg->m_ClientID, ClientID)))
		{
			str_format(msgbuf, sizeof(msgbuf), "%s: %s", ClientName(pMsg->m_ClientID), pMsg->m_pMessage);
			pMsg->m_pMessage = msgbuf;
			pMsg->m_ClientID = VANILLA_MAX_CLIENTS - 1;
		}

		if (!IsSevendown(ClientID))
			return SendPackMsgOne(pMsg, Flags, ClientID);

		// 0.6 chat message translation
		CMsgPacker Packer(pMsg->MsgID(), false);
		Packer.AddInt((int)(pMsg->m_Mode == CHAT_TEAM));
		Packer.AddInt(pMsg->m_ClientID);
		Packer.AddString(pMsg->m_pMessage, -1);
		if (Packer.Error() != 0)
			return -1;
		return SendMsg(&Packer, Flags, ClientID);
	}

	int SendPackMsgTranslate(CNetMsg_Sv_KillMsg *pMsg, int Flags, int ClientID)
	{
		if (!Translate(pMsg->m_Victim, ClientID)) return 0;
		if (!Translate(pMsg->m_Killer, ClientID)) pMsg->m_Killer = pMsg->m_Victim;
		return SendPackMsgOne(pMsg, Flags, ClientID);
	}

	int SendPackMsgTranslate(CNetMsg_Sv_Emoticon *pMsg, int Flags, int ClientID)
	{
		return Translate(pMsg->m_ClientID, ClientID) && SendPackMsgOne(pMsg, Flags, ClientID);
	}

	int SendPackMsgTranslate(CNetMsg_Sv_Team *pMsg, int Flags, int ClientID)
	{
		return Translate(pMsg->m_ClientID, ClientID) && SendPackMsgOne(pMsg, Flags, ClientID);
	}

	int SendPackMsgTranslate(CNetMsg_Sv_SkinChange *pMsg, int Flags, int ClientID)
	{
		return Translate(pMsg->m_ClientID, ClientID) && SendPackMsgOne(pMsg, Flags, ClientID);
	}

	int SendPackMsgTranslate(CNetMsg_Sv_ClientInfo *pMsg, int Flags, int ClientID)
	{
		return (Flags&MSGFLAG_NO_TRANSLATE || Translate(pMsg->m_ClientID, ClientID)) && SendPackMsgOne(pMsg, Flags, ClientID);
	}

	int SendPackMsgTranslate(CNetMsg_Sv_ClientDrop *pMsg, int Flags, int ClientID)
	{
		return (Flags&MSGFLAG_NO_TRANSLATE || Translate(pMsg->m_ClientID, ClientID)) && SendPackMsgOne(pMsg, Flags, ClientID);
	}

	// TODO: Messages VOTESET and RACEFINISH are not translated

	bool Translate(int& Target, int Client)
	{
		int *pMap = GetIdMap(Client);
		bool Found = false;
		for (int i = 0; i < VANILLA_MAX_CLIENTS; i++)
		{
			if (Target == pMap[i])
			{
				Target = i;
				Found = true;
				break;
			}
		}
		return Found;
	}

	bool ReverseTranslate(int& Target, int Client)
	{
		Target = clamp(Target, 0, VANILLA_MAX_CLIENTS-1);
		int *pMap = GetIdMap(Client);
		if (pMap[Target] == -1)
			return false;
		Target = pMap[Target];
		return true;
	}

	virtual void GetMapInfo(char *pMapName, int MapNameSize, int *pMapSize, SHA256_DIGEST *pSha256, int *pMapCrc) = 0;

	virtual void SetClientName(int ClientID, char const *pName) = 0;
	virtual void SetClientClan(int ClientID, char const *pClan) = 0;
	virtual void SetClientCountry(int ClientID, int Country) = 0;
	virtual void SetClientScore(int ClientID, int Score) = 0;

	virtual int SnapNewID() = 0;
	virtual void SnapFreeID(int ID) = 0;
	virtual void *SnapNewItem(int Type, int ID, int Size) = 0;

	virtual void SnapSetStaticsize(int ItemType, int Size) = 0;

	enum
	{
		RCON_CID_SERV=-1,
		RCON_CID_VOTE=-2,
	};
	virtual void SetRconCID(int ClientID) = 0;
	virtual int GetAuthedState(int ClientID) const = 0;
	virtual const char *AuthName(int ClientID) const = 0;
	virtual bool IsBanned(int ClientID) = 0;
	virtual void Kick(int ClientID, const char *pReason) = 0;
	virtual void ChangeMap(const char *pMap) = 0;

	virtual void DemoRecorder_HandleAutoStart() = 0;
	virtual bool DemoRecorder_IsRecording() = 0;

	virtual const char *GetNetErrorString(int ClientID) = 0;
	virtual void ResetNetErrorString(int ClientID) = 0;
	virtual bool SetTimedOut(int ClientID, int OrigID) = 0;
	virtual void SetTimeoutProtected(int ClientID) = 0;
};

class IGameServer : public IInterface
{
	MACRO_INTERFACE("gameserver", 0)
protected:
public:
	virtual void OnInit() = 0;
	virtual void OnMapChange(char* pNewMapName, int MapNameSize) = 0;
	virtual void OnConsoleInit() = 0;
	virtual void OnShutdown(bool FullShutdown = false) = 0;
	virtual void OnPreShutdown() = 0;
	virtual void OnSetTimedOut(int ClientID, int OrigID) = 0;

	virtual void OnTick() = 0;
	virtual void OnPreSnap() = 0;
	virtual void OnSnap(int ClientID) = 0;
	virtual void OnPostSnap() = 0;

	virtual void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID) = 0;

	virtual void OnClientConnected(int ClientID, bool AsSpec) = 0;
	virtual void OnClientEnter(int ClientID) = 0;
	virtual void OnClientDrop(int ClientID, const char *pReason) = 0;
	virtual void OnClientAuth(int ClientID, int Level) = 0;
	virtual void OnClientDirectInput(int ClientID, void *pInput) = 0;
	virtual void OnClientPredictedInput(int ClientID, void *pInput) = 0;

	virtual bool IsClientBot(int ClientID) const = 0;
	virtual bool IsClientReady(int ClientID) const = 0;
	virtual bool IsClientPlayer(int ClientID) const = 0;
	virtual bool IsClientSpectator(int ClientID) const = 0;

	virtual const CUuid GameUuid() const = 0;
	virtual const char *GameType() const = 0;
	virtual const char *Version() const = 0;
	virtual const char *VersionSevendown() const = 0;
	virtual const char *NetVersion() const = 0;
	virtual const char *NetVersionSevendown() const = 0;

	virtual void OnClientEngineJoin(int ClientID) = 0;
	virtual void OnClientEngineDrop(int ClientID, const char *pReason) = 0;
};

extern IGameServer *CreateGameServer();
#endif
