/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_SERVER_H
#define ENGINE_SERVER_SERVER_H

#include <base/hash.h>
#include <engine/server.h>
#include <engine/shared/memheap.h>

#include <engine/masterserver.h>
#include <engine/shared/demo.h>
#include <engine/shared/econ.h>
#include <engine/shared/mapchecker.h>
#include <engine/shared/netban.h>
#include "register.h"
#include <engine/shared/fifo.h>
#include "authmanager.h"

#if defined (CONF_SQL)
	#include "sql_connector.h"
	#include "sql_server.h"
#endif

class CSnapIDPool
{
	enum
	{
		MAX_IDS = 32*1024,
	};

	class CID
	{
	public:
		short m_Next;
		short m_State; // 0 = free, 1 = allocated, 2 = timed
		int m_Timeout;
	};

	CID m_aIDs[MAX_IDS];

	int m_FirstFree;
	int m_FirstTimed;
	int m_LastTimed;
	int m_Usage;
	int m_InUsage;

public:

	CSnapIDPool();

	void Reset();
	void RemoveFirstTimeout();
	int NewID();
	void TimeoutIDs();
	void FreeID(int ID);
};


class CServerBan : public CNetBan
{
	class CServer *m_pServer;

	template<class T> int BanExt(T *pBanPool, const typename T::CDataType *pData, int Seconds, const char *pReason);

public:
	class CServer *Server() const { return m_pServer; }

	void InitServerBan(class IConsole *pConsole, class IStorage *pStorage, class CServer* pServer);

	virtual int BanAddr(const NETADDR *pAddr, int Seconds, const char *pReason);
	virtual int BanRange(const CNetRange *pRange, int Seconds, const char *pReason);

	static void ConBanExt(class IConsole::IResult *pResult, void *pUser);
};


class CServer : public IServer
{
	class IGameServer *m_pGameServer;
	class CConfig *m_pConfig;
	class IConsole *m_pConsole;
	class IStorage *m_pStorage;

#if defined(CONF_SQL)
	lock m_GlobalSqlLock;

	CSqlServer *m_apSqlReadServers[MAX_SQLSERVERS];
	CSqlServer *m_apSqlWriteServers[MAX_SQLSERVERS];
#endif
public:
	class IGameServer *GameServer() { return m_pGameServer; }
	class CConfig *Config() { return m_pConfig; }
	class IConsole *Console() { return m_pConsole; }
	class IStorage *Storage() { return m_pStorage; }

	enum
	{
		MAX_RCONCMD_SEND=16,
		MAX_MAPLISTENTRY_SEND = 32,
		MIN_MAPLIST_CLIENTVERSION=0x0703,	// todo 0.8: remove me
		MAX_RCONCMD_RATIO=8,
	};

	struct CMapListEntry;

	class CClient
	{
	public:

		enum
		{
			STATE_EMPTY = 0,
			STATE_PREAUTH,
			STATE_AUTH,
			STATE_CONNECTING,
			STATE_CONNECTING_AS_SPEC,
			STATE_READY,
			STATE_INGAME,

			// F-DDrace
			STATE_DUMMY,

			SNAPRATE_INIT=0,
			SNAPRATE_FULL,
			SNAPRATE_RECOVER
		};

		class CInput
		{
		public:
			int m_aData[MAX_INPUT_SIZE];
			int m_GameTick; // the tick that was chosen for the input
		};

		// connection state info
		int m_State;
		int m_Latency;
		int m_SnapRate;

		int m_LastAckedSnapshot;
		int m_LastInputTick;
		CSnapshotStorage m_Snapshots;

		CInput m_LatestInput;
		CInput m_aInputs[200]; // TODO: handle input better
		int m_CurrentInput;

		char m_aName[MAX_NAME_ARRAY_SIZE];
		char m_aClan[MAX_CLAN_ARRAY_SIZE];
		int m_Version;
		int m_Country;
		int m_Score;
		int m_Authed;
		int m_AuthKey;
		int m_AuthTries;

		int m_MapChunk;
		bool m_NoRconNote;
		bool m_Quitting;
		const IConsole::CCommandInfo *m_pRconCmdToSend;
		const CMapListEntry *m_pMapListEntryToSend;

		void Reset();

		// DDrace
		bool m_ShowIps;

		float m_Traffic;
		int64 m_TrafficSince;

		bool m_Sevendown;

		bool m_GotDDNetVersionPacket;
		bool m_DDNetVersionSettled;
		int m_DDNetVersion;
		char m_aDDNetVersionStr[64];
		CUuid m_ConnectionID;
	};

	CClient m_aClients[MAX_CLIENTS];
	int IdMap[MAX_CLIENTS * VANILLA_MAX_CLIENTS];

	CSnapshotDelta m_SnapshotDelta;
	CSnapshotBuilder m_SnapshotBuilder;
	CSnapIDPool m_IDPool;
	CNetServer m_NetServer;
	CEcon m_Econ;
#if defined(CONF_FAMILY_UNIX)
	CFifo m_Fifo;
#endif
	CServerBan m_ServerBan;

	IEngineMap *m_pMap;

	int64 m_GameStartTime;
	bool m_RunServer;
	bool m_MapReload;
	int m_RconClientID;
	int m_RconAuthLevel;
	int m_PrintCBIndex;

	int m_RconRestrict;

	// map
	enum
	{
		MAP_CHUNK_SIZE=NET_MAX_PAYLOAD-NET_MAX_CHUNKHEADERSIZE-4, // msg type
	};
	char m_aCurrentMap[64];
	SHA256_DIGEST m_CurrentMapSha256;
	unsigned m_CurrentMapCrc;
	unsigned char *m_pCurrentMapData;
	int m_CurrentMapSize;
	int m_MapChunksPerRequest;

	//maplist
	struct CMapListEntry
	{
		CMapListEntry *m_pPrev;
		CMapListEntry *m_pNext;
		char m_aName[IConsole::TEMPMAP_NAME_LENGTH];
	};

	struct CSubdirCallbackUserdata
	{
		CServer *m_pServer;
		char m_aName[IConsole::TEMPMAP_NAME_LENGTH];
	};

	CHeap *m_pMapListHeap;
	CMapListEntry *m_pLastMapEntry;
	CMapListEntry *m_pFirstMapEntry;
	int m_NumMapEntries;

	int m_RconPasswordSet;
	int m_GeneratedRconPassword;

	CDemoRecorder m_DemoRecorder;
	CRegister m_Register;
	CRegister m_RegisterSevendown;
	CMapChecker m_MapChecker;
	CAuthManager m_AuthManager;

	int64 m_ServerInfoFirstRequest;
	int m_ServerInfoNumRequests;

	CServer();

	int TrySetClientName(int ClientID, const char* pName);

	virtual void SetClientName(int ClientID, const char *pName);
	virtual void SetClientClan(int ClientID, char const *pClan);
	virtual void SetClientCountry(int ClientID, int Country);
	virtual void SetClientScore(int ClientID, int Score);

	void Kick(int ClientID, const char *pReason);

	void DemoRecorder_HandleAutoStart();
	bool DemoRecorder_IsRecording();

	int64 TickStartTime(int Tick);

	int Init();

	void InitRconPasswordIfUnset();

	void SetRconCID(int ClientID);
	int GetAuthedState(int ClientID) const;
	const char *AuthName(int ClientID) const;
	bool IsBanned(int ClientID);
	void GetMapInfo(char *pMapName, int MapNameSize, int *pMapSize, SHA256_DIGEST *pMapSha256, int *pMapCrc);
	int GetClientInfo(int ClientID, CClientInfo *pInfo) const;
	void GetClientAddr(int ClientID, char *pAddrStr, int Size, bool AddPort = false) const;
	void SetClientDDNetVersion(int ClientID, int DDNetVersion);
	int GetClientVersion(int ClientID) const;
	const char *ClientName(int ClientID) const;
	const char *ClientClan(int ClientID) const;
	int ClientCountry(int ClientID) const;
	bool ClientIngame(int ClientID) const;

	virtual int SendMsg(CMsgPacker *pMsg, int Flags, int ClientID);

	void DoSnapshot();

	static int NewClientCallback(int ClientID, bool Sevendown, void *pUser);
	static int DelClientCallback(int ClientID, const char *pReason, void *pUser);

	bool IsSevendown(int ClientID) { return m_aClients[ClientID].m_Sevendown; }

	void SendCapabilities(int ClientID);
	void SendMap(int ClientID);
	void SendConnectionReady(int ClientID);
	void SendRconLine(int ClientID, const char *pLine);
	static void SendRconLineAuthed(const char *pLine, void *pUser, bool Highlighted);

	void SendRconCmdAdd(const IConsole::CCommandInfo *pCommandInfo, int ClientID);
	void SendRconCmdRem(const IConsole::CCommandInfo *pCommandInfo, int ClientID);
	void UpdateClientRconCommands();
	void SendMapListEntryAdd(const CMapListEntry *pMapListEntry, int ClientID);
	void SendMapListEntryRem(const CMapListEntry *pMapListEntry, int ClientID);
	void UpdateClientMapListEntries();

	void ProcessClientPacket(CNetChunk *pPacket);

	void SendServerInfo(int ClientID);
	void GenerateServerInfo(CPacker *pPacker, int Token);
	void SendServerInfoSevendown(const NETADDR *pAddr, int Token, bool SendClients);

	void PumpNetwork();

	virtual void ChangeMap(const char *pMap);
	const char *GetMapName();
	int LoadMap(const char *pMapName);

	void InitRegister(CNetServer *pNetServer, IEngineMasterServer *pMasterServer, CConfig *pConfig, IConsole *pConsole);
	void InitInterfaces(CConfig *pConfig, IConsole *pConsole, IGameServer *pGameServer, IEngineMap *pMap, IStorage *pStorage);
	int Run();

	static int MapListEntryCallback(const char *pFilename, int IsDir, int DirType, void *pUser);

	static void ConEuroMode(IConsole::IResult *pResult, void *pUser);
	static void ConTestingCommands(IConsole::IResult *pResult, void *pUser);
	static void ConRescue(IConsole::IResult *pResult, void *pUser);
	static void ConKick(IConsole::IResult *pResult, void *pUser);
	static void ConStatus(IConsole::IResult *pResult, void *pUser);
	static void ConShutdown(IConsole::IResult *pResult, void *pUser);
	static void ConRecord(IConsole::IResult *pResult, void *pUser);
	static void ConStopRecord(IConsole::IResult *pResult, void *pUser);
	static void ConMapReload(IConsole::IResult *pResult, void *pUser);
	static void ConSaveConfig(IConsole::IResult *pResult, void *pUser);
	static void ConLogout(IConsole::IResult *pResult, void *pUser);

	static void ConchainSpecialInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainPlayerSlotsUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainMaxclientsUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainMaxclientsperipUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainCommandAccessUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainConsoleOutputLevelUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	static void ConchainMapUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	static void ConShowIps(IConsole::IResult* pResult, void* pUser);
	void ConchainRconPasswordChangeGeneric(int Level, const char *pCurrent, IConsole::IResult *pResult);
	static void ConchainRconPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainRconModPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainRconHelperPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	void LogoutClient(int ClientID, const char *pReason);
	void LogoutKey(int Key, const char *pReason);
	void AuthRemoveKey(int KeySlot);
	static void ConAuthAdd(IConsole::IResult *pResult, void *pUser);
	static void ConAuthAddHashed(IConsole::IResult *pResult, void *pUser);
	static void ConAuthUpdate(IConsole::IResult *pResult, void *pUser);
	static void ConAuthUpdateHashed(IConsole::IResult *pResult, void *pUser);
	static void ConAuthRemove(IConsole::IResult *pResult, void *pUser);
	static void ConAuthList(IConsole::IResult *pResult, void *pUser);

	void RegisterCommands();

	virtual int SnapNewID();
	virtual void SnapFreeID(int ID);
	virtual void *SnapNewItem(int Type, int ID, int Size);
	void SnapSetStaticsize(int ItemType, int Size);

	void RestrictRconOutput(int ClientID) { m_RconRestrict = ClientID; }

	// F-DDrace

	const char *GetNetErrorString(int ClientID) { return m_NetServer.ErrorString(ClientID); };
	void ResetNetErrorString(int ClientID) { m_NetServer.ResetErrorString(ClientID); };
	bool SetTimedOut(int ClientID, int OrigID);
	void SetTimeoutProtected(int ClientID) { m_NetServer.SetTimeoutProtected(ClientID); };

	void GetClientAddr(int ClientID, NETADDR* pAddr);
	const char* GetAnnouncementLine(char const* FileName);
	unsigned m_AnnouncementLastLine;

	virtual int *GetIdMap(int ClientID);

	void DummyJoin(int DummyID);
	void DummyLeave(int DummyID);

	const char *GetClientVersionStr(int ClientID) const;

#if defined (CONF_SQL)
	// console commands for sqlmasters
	static void ConAddSqlServer(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpSqlServers(IConsole::IResult *pResult, void *pUserData);

	static void CreateTablesThread(void *pData);
#endif
};

#endif
