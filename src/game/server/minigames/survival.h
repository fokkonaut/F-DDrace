// made by fokkonaut

#ifndef GAME_SERVER_MINIGAMES_SURVIVAL_H
#define GAME_SERVER_MINIGAMES_SURVIVAL_H

#include "minigame.h"

class CSurvival : public CMinigame
{
	enum
	{
		SURVIVAL_OFFLINE = 0,
		SURVIVAL_LOBBY,
		SURVIVAL_PLAYING,
		SURVIVAL_DEATHMATCH,

		BACKGROUND_IDLE = -1,
		BACKGROUND_LOBBY_WAITING,
		BACKGROUND_LOBBY_COUNTDOWN,
		BACKGROUND_DEATHMATCH_COUNTDOWN,
	};

	void SetPlayerState(int State);
	template<typename... Args>
	void SendBroadcastFormat(bool Sound, bool IsImportant, const char *pFormat, Args&&... args)
	{
		CFormatArg aArgs[] = { CFormatArg(std::forward<Args>(args))... };
		SendBroadcast(pFormat, Sound, IsImportant, aArgs, std::size(aArgs));
	}
	void SendBroadcast(const char* pMsg, bool Sound = false, bool IsImportant = true, CFormatArg *pArgs = 0, int NumArgs = 0);
	int CountPlayers(int State);
	int GetRandomPlayer(int State, int NotThis = -1);
	
	int m_BackgroundState;
	int m_GameState;
	int64 m_TimerTick;
	int m_Winner;

	int m_aState[MAX_CLIENTS];
	int m_aDieTick[MAX_CLIENTS];

public:
	CSurvival(CGameContext *pGameServer);
	virtual ~CSurvival() {}
	void Tick() override;

	void OnPlayerJoin(int ClientID) override;
	void OnPlayerLeave(int ClientID, bool Disconnect = false, bool Shutdown = false) override;
	//bool OnCharacterSpawn(CCharacter *pChr) override;
	void OnCharacterDie(CCharacter *pChr, int Killer) override;
	int SpawnIndex(int ClientID) const override;

	bool HideWeaponIndicator() const { return m_BackgroundState < BACKGROUND_DEATHMATCH_COUNTDOWN; }
	bool IsPlaying(int ClientID) const { return m_aState[ClientID] > SURVIVAL_LOBBY; }
	bool AllowClickToSpectate(int ClientID) const;
};

#endif // GAME_SERVER_MINIGAMES_SURVIVAL_H
