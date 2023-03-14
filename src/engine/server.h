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

struct CAntibotRoundData;

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
	virtual void SetRconAuthLevel(int AuthLevel) = 0;

	// F-DDrace

	virtual void GetClientAddr(int ClientID, NETADDR* pAddr) = 0;
	virtual const char* GetAnnouncementLine(char const* FileName) = 0;

	virtual void SendWebhookMessage(const char *pURL, const char *pMessage, const char *pUsername = "", const char *pAvatarURL = "") = 0;

	virtual int *GetIdMap(int ClientID) = 0;
	virtual int *GetReverseIdMap(int ClientID) = 0;

	virtual void DummyJoin(int DummyID) = 0;
	virtual void DummyLeave(int DummyID) = 0;

	virtual bool IsSevendown(int ClientID) = 0;
	virtual bool IsIdleDummy(int ClientID) = 0;
	virtual bool IsDummyHammer(int ClientID) = 0;
	virtual bool DesignChanging(int ClientID) = 0;
	virtual bool IsMain(int ClientID) = 0;
	virtual const char *GetLanguage(int ClientID) = 0;
	virtual void SetLanguage(int ClientID, const char *pLanguage) = 0;
	virtual void ChangeMapDesign(int ClientID, const char *pName) = 0;
	virtual const char *GetMapDesign(int ClientID) = 0;

	virtual void PrintBotLookup() = 0;
	virtual void TranslateChat(int ClientID, const char *pMsg, int Mode) = 0;

	virtual void SaveWhitelist() = 0;
	virtual void AddWhitelist(const NETADDR *pAddr, const char *pReason) = 0;
	virtual void RemoveWhitelist(const NETADDR *pAddr) = 0;
	virtual void RemoveWhitelistByIndex(unsigned int Index) = 0;
	virtual void PrintWhitelist() = 0;

	virtual bool IsUniqueAddress(int ClientID) = 0;
	virtual int GetDummy(int ClientID) = 0;
	virtual bool IsDummy(int ClientID1, int ClientID2) = 0;

	virtual void ExpireServerInfo() = 0;

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

	char aBuf[512];

	int SendPackMsgTranslate(CNetMsg_Sv_Chat *pMsg, int Flags, int ClientID)
	{
		// 128 player translation
		int *pID = pMsg->m_Mode == CHAT_WHISPER_SEND ? &pMsg->m_TargetID : &pMsg->m_ClientID;
		if (*pID >= 0 && ((Flags&MSGFLAG_NONAME) || (pMsg->m_Mode == CHAT_TEAM && *pID != ClientID) || pMsg->m_Mode == CHAT_WHISPER_RECV || !Translate(*pID, ClientID)))
		{
			str_format(aBuf, sizeof(aBuf), "%s: %s", ClientName(*pID), pMsg->m_pMessage);
			pMsg->m_pMessage = aBuf;

			// with noname and sending a whisper to ourselves show our own client id as targetid because otherwise it would be two times id 63 with same text which gets shown twice the same msg
			if (pMsg->m_Mode == CHAT_WHISPER_SEND && *pID == ClientID)
				Translate(*pID, ClientID);
			else
				*pID = VANILLA_MAX_CLIENTS - 1;
		}

		if (!IsSevendown(ClientID))
		{
			if (pMsg->m_Mode == CHAT_WHISPER_SEND)
			{
				Translate(pMsg->m_ClientID, ClientID);
				pMsg->m_Mode = CHAT_WHISPER;
			}
			else if (pMsg->m_Mode == CHAT_WHISPER_RECV)
			{
				Translate(pMsg->m_TargetID, ClientID);
				pMsg->m_Mode = CHAT_WHISPER;
			}
			return SendPackMsgOne(pMsg, Flags, ClientID);
		}

		// 0.6 chat message translation
		CMsgPacker Packer(pMsg->MsgID(), false);
		Packer.AddInt(pMsg->m_Mode == CHAT_WHISPER_SEND ? 2 : pMsg->m_Mode == CHAT_WHISPER_RECV ? 3 : (int)(pMsg->m_Mode == CHAT_TEAM));
		Packer.AddInt(*pID);
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
		return (Flags&MSGFLAG_NOTRANSLATE || Translate(pMsg->m_ClientID, ClientID)) && SendPackMsgOne(pMsg, Flags, ClientID);
	}

	int SendPackMsgTranslate(CNetMsg_Sv_ClientDrop *pMsg, int Flags, int ClientID)
	{
		return (Flags&MSGFLAG_NOTRANSLATE || Translate(pMsg->m_ClientID, ClientID)) && SendPackMsgOne(pMsg, Flags, ClientID);
	}

	int SendPackMsgTranslate(CNetMsg_Sv_VoteSet *pMsg, int Flags, int ClientID)
	{
		return Translate(pMsg->m_ClientID, ClientID) && SendPackMsgOne(pMsg, Flags, ClientID);
	}

	int SendPackMsgTranslate(CNetMsg_Sv_RaceFinish *pMsg, int Flags, int ClientID)
	{
		return Translate(pMsg->m_ClientID, ClientID) && SendPackMsgOne(pMsg, Flags, ClientID);
	}

	bool Translate(int& Target, int Client)
	{
		if (Target < 0 || Target >= MAX_CLIENTS)
			return false;
		int *pMap = GetReverseIdMap(Client);
		if (pMap[Target] == -1)
			return false;
		Target = pMap[Target];
		return true;
	}

	bool ReverseTranslate(int& Target, int Client)
	{
		if (Target < 0 || Target >= VANILLA_MAX_CLIENTS)
			return false;
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
	virtual void Ban(int ClientID, int Seconds, const char *pReason) = 0;
	virtual void ChangeMap(const char *pMap) = 0;
	virtual const char *GetMapName() = 0;
	virtual const char *GetCurrentMapName() = 0;
	virtual const char *GetFileName(char *pPath) = 0;

	virtual void SendMsgRaw(int ClientID, const void *pData, int Size, int Flags) = 0;

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
	virtual void MapDesignChangeDone(int ClientID) = 0;
	virtual void OnClientDrop(int ClientID, const char *pReason) = 0;
	virtual void OnClientAuth(int ClientID, int Level) = 0;
	virtual void OnClientDirectInput(int ClientID, void *pInput) = 0;
	virtual void OnClientPredictedInput(int ClientID, void *pInput) = 0;
	virtual void OnClientPredictedEarlyInput(int ClientID, void *pInput) = 0;
	virtual void OnClientRejoin(int ClientID) = 0;

	virtual bool IsClientBot(int ClientID) const = 0;
	virtual bool IsClientReady(int ClientID) const = 0;
	virtual bool IsClientPlayer(int ClientID) const = 0;
	virtual bool IsClientSpectator(int ClientID) const = 0;

	virtual void SendChatMessage(int ChatterClientID, int Mode, int To, const char *pText) = 0;

	virtual const CUuid GameUuid() const = 0;
	virtual const char *GameType() const = 0;
	virtual const char *Version() const = 0;
	virtual const char *VersionSevendown() const = 0;
	virtual const char *NetVersion() const = 0;
	virtual const char *NetVersionSevendown() const = 0;

	virtual void OnClientEngineJoin(int ClientID) = 0;
	virtual void OnClientEngineDrop(int ClientID, const char *pReason) = 0;

	virtual void FillAntibot(CAntibotRoundData *pData) = 0;
	virtual void SetBotDetected(int ClientID) = 0;

	/**
	 * Used to report custom player info to master servers.
	 * 
	 * @param aBuf Should be the json key values to add, starting with a ',' beforehand, like: ',"skin": "default", "team": 1'
	 * @param i The client id.
	 */
	virtual void OnUpdatePlayerServerInfo(char *aBuf, int BufSize, int ID) = 0;
};

extern IGameServer *CreateGameServer();
#endif
