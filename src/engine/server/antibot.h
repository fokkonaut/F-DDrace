#ifndef ENGINE_SERVER_ANTIBOT_H
#define ENGINE_SERVER_ANTIBOT_H

#include <antibot/antibot_data.h>
#include <engine/antibot.h>
#include <engine/shared/protocol.h>

class CAntibot : public IEngineAntibot
{
	class IServer *m_pServer;
	class IConsole *m_pConsole;
	class IGameServer *m_pGameServer;
	class CConfig *m_pConfig;

public: // made public for F-DDrace
	class IServer *Server() const { return m_pServer; }
	class IGameServer *GameServer() const { return m_pGameServer; }
private:
	class IConsole *Console() const { return m_pConsole; }
	class CConfig *Config() const { return m_pConfig; }

	CAntibotData m_Data;
	CAntibotRoundData m_RoundData;
	bool m_Initialized;

	void Update();
	static void Log(const char *pMessage, void *pUser);
	static void Send(int ClientID, const void *pData, int Size, int Flags, void *pUser);
	static void Teehistorian(const void *pData, int Size, void *pUser);
	static void Report(int ClientID, const char *pMessage, /*int Count,*/ void *pUser);

	char m_aKind[MAX_CLIENTS][16];
	int m_DumpFilterID;
	int m_FetchKindID;

public:
	CAntibot();
	virtual ~CAntibot();

	// Engine
	virtual void Init();

	virtual void OnEngineTick();
	virtual void OnEngineClientJoin(int ClientID, bool Sevendown);
	virtual void OnEngineClientDrop(int ClientID, const char *pReason);
	virtual bool OnEngineClientMessage(int ClientID, const void *pData, int Size, int Flags);
	virtual bool OnEngineServerMessage(int ClientID, const void *pData, int Size, int Flags);
	virtual bool OnEngineSimulateClientMessage(int *pClientID, void *pBuffer, int BufferSize, int *pOutSize, int *pFlags);

	// Game
	virtual void RoundStart(class IGameServer *pGameServer);
	virtual void RoundEnd();

	virtual void OnPlayerInit(int ClientID);
	virtual void OnPlayerDestroy(int ClientID);
	virtual void OnSpawn(int ClientID);
	virtual void OnHammerFireReloading(int ClientID);
	virtual void OnHammerFire(int ClientID);
	virtual void OnHammerHit(int ClientID, int TargetID);
	virtual void OnDirectInput(int ClientID);
	virtual void OnCharacterTick(int ClientID);
	virtual void OnHookAttach(int ClientID, bool Player);

	virtual void Dump(int ClientID = -1);
};

extern IEngineAntibot *CreateEngineAntibot();

#endif // ENGINE_SERVER_ANTIBOT_H
