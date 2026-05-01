// made by fokkonaut

#include "survival.h"
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include <engine/shared/config.h>
#include <game/server/gamemodes/DDRace.h>

CSurvival::CSurvival(CGameContext *pGameServer) : CMinigame(pGameServer, MINIGAME_SURVIVAL)
{
	m_GameState = SURVIVAL_OFFLINE;
	m_BackgroundState = SURVIVAL_OFFLINE;
	m_TimerTick = 0;
	m_Winner = -1;

	std::fill(std::begin(m_aState), std::end(m_aState), SURVIVAL_OFFLINE);
	std::fill(std::begin(m_aDieTick), std::end(m_aDieTick), 0);
}

int CSurvival::SpawnIndex(int ClientID) const
{
	if (m_aState[ClientID] == SURVIVAL_LOBBY)
		return TILE_SURVIVAL_LOBBY;
	else if (m_aState[ClientID] == SURVIVAL_PLAYING)
		return TILE_SURVIVAL_SPAWN;
	else if (m_aState[ClientID] == SURVIVAL_DEATHMATCH)
		return TILE_SURVIVAL_DEATHMATCH;
	return CMinigame::SpawnIndex(ClientID);
}

void CSurvival::OnPlayerJoin(int ClientID)
{
	m_aState[ClientID] = SURVIVAL_LOBBY;
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	pPlayer->m_Gamemode = pPlayer->m_SavedGamemode = GAMEMODE_VANILLA;
}

void CSurvival::OnPlayerLeave(int ClientID, bool Disconnect, bool Shutdown)
{
	m_aState[ClientID] = SURVIVAL_OFFLINE;
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	pPlayer->m_Gamemode = pPlayer->m_SavedGamemode = GAMEMODE_DDRACE;
	pPlayer->m_ShowName = true;
}

void CSurvival::OnCharacterDie(CCharacter *pChr, int Killer)
{
	// character doesnt exist, print some messages and set states
	// if the player is in deathmatch mode, or simply playing
	CPlayer *pPlayer = pChr->GetPlayer();
	int ClientID = pPlayer->GetCID();
	CPlayer *pKiller = Killer >= 0 && Killer != pPlayer->GetCID() ? GameServer()->m_apPlayers[Killer] : 0;
	if (m_GameState > SURVIVAL_LOBBY && m_aState[ClientID] > SURVIVAL_LOBBY && Killer != WEAPON_GAME)
	{
		// check for players in the current game state
		if (pPlayer->GetCID() != m_Winner)
			GameServer()->SendChatTarget(pPlayer->GetCID(), pPlayer->Localize("You lost, you can wait for another round or leave the lobby using '/leave'"));
		if (CountPlayers(m_GameState) > 2)
		{
			// if there are more than just two players left, you will watch your killer or a random player
			pPlayer->Pause(CPlayer::PAUSE_PAUSED, true);
			pPlayer->SetSpectatorID(SPEC_PLAYER, pKiller ? Killer : GetRandomPlayer(m_GameState, pPlayer->GetCID()));

			// update the ones that are watching you
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				CPlayer *pCheck = GameServer()->m_apPlayers[i];
				if (i == pPlayer->GetCID() || !pCheck || pCheck->m_Minigame != MINIGAME_SURVIVAL || pCheck->GetSpectatorID() != pPlayer->GetCID())
					continue;
				pCheck->SetSpectatorID(SPEC_PLAYER, pPlayer->GetSpectatorID());
			}

			// printing a message that you died and informing about remaining players
			char aKillMsg[128];
			str_format(aKillMsg, sizeof(aKillMsg), "'%s' died\nAlive players: %d", Server()->ClientName(pPlayer->GetCID()), CountPlayers(m_GameState) -1 /* -1 because we have to exclude the currently dying*/);
			SendBroadcast(aKillMsg, true);
		}
		// sending you back to lobby
		m_aState[ClientID] = SURVIVAL_LOBBY;
		m_aDieTick[ClientID] = Server()->Tick();
		pPlayer->m_ShowName = true;
	}
}

void CSurvival::Tick()
{
	// if there are no spawn tiles, we cant play the game
	if (!GameServer()->m_aMinigameDisabled[MINIGAME_SURVIVAL] && (!GameServer()->Collision()->TileUsed(TILE_SURVIVAL_LOBBY) || !GameServer()->Collision()->TileUsed(TILE_SURVIVAL_SPAWN) || !GameServer()->Collision()->TileUsed(TILE_SURVIVAL_DEATHMATCH)))
	{
		GameServer()->m_aMinigameDisabled[MINIGAME_SURVIVAL] = true;
		return;
	}

	// set the mode to lobby, if the game is offline and there are now players
	if (m_GameState == SURVIVAL_OFFLINE)
		m_GameState = SURVIVAL_LOBBY;

	// check if we dont have any players in the current state
	if (!CountPlayers(m_GameState))
	{
		m_GameState = SURVIVAL_OFFLINE;
		m_BackgroundState = SURVIVAL_OFFLINE;
		return;
	}

	// decrease the tick at any time if it exists (its a timer)
	if (m_TimerTick)
		m_TimerTick--;

	int Remaining = m_TimerTick / Server()->TickSpeed();

	// main part
	char aBuf[128];

	if (m_GameState > SURVIVAL_LOBBY && CountPlayers(m_GameState) == 1)
	{
		// if there is only one survival player left, before the time is over, we have a winner
		m_Winner = GetRandomPlayer(m_GameState);

		if (GameServer()->m_apPlayers[m_Winner])
		{
			SendBroadcastFormat(true, true, Localizable("The winner is '%s'"), Server()->ClientName(m_Winner));

			// send message to winner
			GameServer()->SendChatTarget(m_Winner, GameServer()->m_apPlayers[m_Winner]->Localize("You are the winner"));

			// add a win to the winners' accounts
			if (GameServer()->m_apPlayers[m_Winner]->GetAccID() >= ACC_START)
				GameServer()->m_Accounts.Get(GameServer()->m_apPlayers[m_Winner]->GetAccID()).m_SurvivalWins++;
			GameServer()->m_apPlayers[m_Winner]->GiveXP(250, "for winning a survival round");
		}

		// sending back to lobby
		m_GameState = SURVIVAL_LOBBY;
		m_BackgroundState = SURVIVAL_OFFLINE;
		SetPlayerState(SURVIVAL_LOBBY);
	}


	// checking for foreground states
	switch (m_GameState)
	{
		case SURVIVAL_LOBBY:
		{
			// check whether we have something running in the background
			if (m_BackgroundState != SURVIVAL_OFFLINE)
				break;

			// count the lobby players, if they are fewer than the minimum amount, set the waiting mode in the background
			if (CountPlayers(SURVIVAL_LOBBY) < GameServer()->Config()->m_SvSurvivalMinPlayers)
			{
				m_BackgroundState = BACKGROUND_LOBBY_WAITING;
			}
			// if we are more than the minimum players waiting, the countdown will start in the background (30 seconds until the game starts)
			else
			{
				m_BackgroundState = BACKGROUND_LOBBY_COUNTDOWN;
				m_TimerTick = Server()->TickSpeed() * (GameServer()->Config()->m_SvSurvivalLobbyCountdown + 1);
			}
			break;
		}

		case SURVIVAL_PLAYING:
		{
			// the game is running
			break;
		}

		case SURVIVAL_DEATHMATCH:
		{
			if (!m_TimerTick)
			{
				// if the deathmatch is over, reset the survival game, sending players back to lobby
				if (CountPlayers(SURVIVAL_DEATHMATCH) > 1)
					SendBroadcast("There is no winner this round!");
				m_GameState = SURVIVAL_OFFLINE;
				m_BackgroundState = BACKGROUND_IDLE;
				SetPlayerState(SURVIVAL_LOBBY);
			}
			else
			{
				// before its over, send some broadcasts until its finally over
				if (Server()->Tick() % 50 == 0)
				{
					if (Remaining % 30 == 0 || Remaining <= 10)
					{
						str_format(aBuf, sizeof(aBuf), "Deathmatch will end in %d seconds", Remaining);
						SendBroadcast(aBuf, true);
					}
				}
			}
			break;
		}
	}

	// checking for background states
	switch (m_BackgroundState)
	{
		case BACKGROUND_LOBBY_WAITING:
		{
			// send the waiting for players broadcast to all survival players
			if (Server()->Tick() % 50 == 0)
			{
				str_format(aBuf, sizeof(aBuf), "[%d/%d] players to start a round", CountPlayers(SURVIVAL_LOBBY), GameServer()->Config()->m_SvSurvivalMinPlayers);
				SendBroadcast(aBuf, false, false);
			}
			break;
		}

		case BACKGROUND_LOBBY_COUNTDOWN:
		{
			if (!m_TimerTick)
			{
				const int Minutes = GameServer()->Config()->m_SvSurvivalRoundTime;

				// timer is over, the round starts
				str_format(aBuf, sizeof(aBuf), "Round started, you have %d minute%s to kill each other", Minutes, Minutes > 1 ? "s" : "");
				SendBroadcast(aBuf);

				// set a new tick, this time for the round to end after its up
				m_TimerTick = Server()->TickSpeed() * 60 * GameServer()->Config()->m_SvSurvivalRoundTime;
				// set the foreground state
				m_GameState = SURVIVAL_PLAYING;
				// change background state
				m_BackgroundState = BACKGROUND_DEATHMATCH_COUNTDOWN;
				// set the player's survival state
				SetPlayerState(SURVIVAL_PLAYING);
			}
			else if (CountPlayers(SURVIVAL_LOBBY) >= GameServer()->Config()->m_SvSurvivalMinPlayers)
			{
				// if we are more than the minimum players, the countdown will start
				if (Server()->Tick() % 50 == 0)
				{
					str_format(aBuf, sizeof(aBuf), "Round will start in %d seconds", Remaining);
					SendBroadcast(aBuf, Remaining <= 10, false);
				}
			}
			// if someone left the lobby, the countdown stops and we return to the lobby state (waiting for players again)
			else
			{
				SendBroadcast("Start failed, too few players");
				m_GameState = SURVIVAL_LOBBY;
				m_BackgroundState = SURVIVAL_OFFLINE;
			}
			break;
		}

		case BACKGROUND_DEATHMATCH_COUNTDOWN:
		{
			if (!m_TimerTick)
			{
				const int Minutes = GameServer()->Config()->m_SvSurvivalDeathmatchTime;

				// deathmatch countdown is over, we will start the deathmatch now
				str_format(aBuf, sizeof(aBuf), "Deathmatch started, you have %d minute%s to kill the last survivors", Minutes, Minutes > 1 ? "s" : "");
				SendBroadcast(aBuf);

				//sending to deathmatch arena
				m_GameState = SURVIVAL_DEATHMATCH;
				SetPlayerState(SURVIVAL_DEATHMATCH);
				m_BackgroundState = BACKGROUND_IDLE;

				// deathmatch will be 2 minutes
				m_TimerTick = Server()->TickSpeed() * 60 * Minutes;
			}
			else
			{
				// printing broadcast until deathmatch starts
				if (Server()->Tick() % 50 == 0)
				{
					if (Remaining % 60 == 0 || Remaining == 30 || Remaining <= 10)
					{
						str_format(aBuf, sizeof(aBuf), "Deathmatch will start in %d %s%s", Remaining > 30 ? Remaining / 60 : Remaining, (Remaining % 60 == 0 && Remaining != 0) ? "minute" : "second", (Remaining == 1 || Remaining == 60) ? "" : "s");
						SendBroadcast(aBuf, true);
					}
				}
			}
			break;
		}
	}
}

int CSurvival::CountPlayers(int State)
{
	int count = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_Minigame == MINIGAME_SURVIVAL && (m_aState[i] == State || State == -1))
			count++;
	return count;
}

void CSurvival::SetPlayerState(int State)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_Minigame == MINIGAME_SURVIVAL)
		{
			// only send playing people to deathmatch
			if (State == SURVIVAL_DEATHMATCH && m_aState[i] != SURVIVAL_PLAYING)
				continue;

			// unset spectator mode and pause
			GameServer()->m_apPlayers[i]->SetPlaying();
			// kill the character
			GameServer()->m_apPlayers[i]->KillCharacter(WEAPON_GAME);
			// hide name in every state except lobby
			GameServer()->m_apPlayers[i]->m_ShowName = State == SURVIVAL_LOBBY;
			// set its new survival state
			m_aState[i] = State;
		}
}

int CSurvival::GetRandomPlayer(int State, int NotThis)
{
	std::vector<int> SurvivalPlayers;
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (i != NotThis && GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_Minigame == MINIGAME_SURVIVAL && (m_aState[i] == State || State == -1))
			SurvivalPlayers.push_back(i);
	if (SurvivalPlayers.size())
	{
		int Rand = rand() % SurvivalPlayers.size();
		return SurvivalPlayers[Rand];
	}
	return -1;
}

void CSurvival::SendBroadcast(const char *pMsg, bool Sound, bool IsImportant, CFormatArg *pArgs, int NumArgs)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_Minigame == MINIGAME_SURVIVAL)
		{
			if (Sound)
				GameServer()->CreateSoundPlayer(SOUND_HOOK_NOATTACH, i);

			// show money broadcast instead of the wanted one if we are on a money tile
			if (GameServer()->m_apPlayers[i]->GetCharacter() && GameServer()->m_apPlayers[i]->GetCharacter()->m_MoneyTile)
				continue;
			GameServer()->SendBroadcast(pMsg, i, IsImportant, pArgs, NumArgs);
		}
	}
}

bool CSurvival::AllowClickToSpectate(int ClientID) const
{
	return m_aState[ClientID] == SURVIVAL_OFFLINE || m_aDieTick[ClientID] < Server()->Tick() - Server()->TickSpeed() * 2;
}
