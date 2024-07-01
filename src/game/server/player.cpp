/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entities/character.h"
#include "entities/flag.h"
#include "entities/flyingpoint.h"
#include "gamecontext.h"
#include "gamecontroller.h"
#include "player.h"
#include <game/server/gamemodes/DDRace.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include "score.h"
#include "houses/shop.h"


MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS)

IServer *CPlayer::Server() const { return m_pGameServer->Server(); }

CPlayer::CPlayer(CGameContext *pGameServer, int ClientID, bool DebugDummy, bool AsSpec, bool Dummy)
{
	m_pGameServer = pGameServer;
	m_ClientID = ClientID;
	m_Team = AsSpec ? TEAM_SPECTATORS : GameServer()->m_pController->GetStartTeam(ClientID);
	m_DebugDummy = DebugDummy;
	m_IsDummy = Dummy;
	Reset();
	GameServer()->Antibot()->OnPlayerInit(m_ClientID);
}

CPlayer::~CPlayer()
{
	GameServer()->Antibot()->OnPlayerDestroy(m_ClientID);
	delete m_pLastTarget;
	delete m_pCharacter;
	delete m_pKillMsgNoName;
	m_pCharacter = 0;
}

void CPlayer::Reset()
{
	m_DieTick = Server()->Tick();
	m_PreviousDieTick = m_DieTick;
	m_ScoreStartTick = Server()->Tick();
	m_pCharacter = 0;
	m_SpecMode = SPEC_FREEVIEW;
	m_SpectatorID = -1;
	m_pSpecFlag = 0;
	m_KillMe = 0;
	m_ActiveSpecSwitch = 0;
	m_LastActionTick = Server()->Tick();
	m_TeamChangeTick = Server()->Tick();
	m_IsReadyToPlay = false;
	m_WeakHookSpawn = false;

	// F-DDrace

	m_LastCommandPos = 0;
	m_LastPlaytime = 0;
	m_Sent1stAfkWarning = 0;
	m_Sent2ndAfkWarning = 0;
	m_ChatScore = 0;
	m_EyeEmote = true;
	m_DefEmote = EMOTE_NORMAL;
	m_Afk = true;
	m_LastWhisperTo = -1;
	m_LastSetSpectatorMode = 0;
	m_TimeoutCode[0] = '\0';

	delete m_pLastTarget;
	m_pLastTarget = nullptr;
	m_LastTargetInit = false;
	m_TuneZone = 0;
	m_TuneZoneOld = m_TuneZone;
	m_Halloween = false;
	m_FirstPacket = true;

	m_SendVoteIndex = -1;

	if (GameServer()->Config()->m_Events)
	{
		time_t rawtime;
		struct tm* timeinfo;
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		if ((timeinfo->tm_mon == 11 && timeinfo->tm_mday == 31) || (timeinfo->tm_mon == 0 && timeinfo->tm_mday == 1))
		{ // New Year
			m_DefEmote = EMOTE_HAPPY;
		}
		else if ((timeinfo->tm_mon == 9 && timeinfo->tm_mday == 31) || (timeinfo->tm_mon == 10 && timeinfo->tm_mday == 1))
		{ // Halloween
			m_DefEmote = EMOTE_ANGRY;
			m_Halloween = true;
		}
		else
		{
			m_DefEmote = EMOTE_NORMAL;
		}
	}
	m_DefEmoteReset = -1;

	GameServer()->Score()->PlayerData(m_ClientID)->Reset();

	m_ShowOthers = GameServer()->Config()->m_SvShowOthersDefault;
	m_ShowAll = GameServer()->Config()->m_SvShowAllDefault;
	m_ShowDistance = vec2(1200, 800);
	m_SpecTeam = false;
	m_NinjaJetpack = false;

	m_Paused = PAUSE_NONE;

	m_LastPause = 0;
	m_Score = -1;
	m_HasFinishScore = false;

	// Variable initialized:
	m_Last_Team = 0;

	int64 Now = Server()->Tick();
	int64 TickSpeed = Server()->TickSpeed();
	// If the player joins within ten seconds of the server becoming
	// non-empty, allow them to vote immediately. This allows players to
	// vote after map changes or when they join an empty server.
	//
	// Otherwise, block voting in the begnning after joining.
	if (Now > GameServer()->m_NonEmptySince + 10 * TickSpeed)
		m_FirstVoteTick = Now + GameServer()->Config()->m_SvJoinVoteDelay * TickSpeed;
	else
		m_FirstVoteTick = Now;

	m_NotEligibleForFinish = false;
	m_EligibleForFinishCheck = 0;
	m_VotedForPractice = false;

	// F-DDrace

	m_DummyMode = DUMMYMODE_IDLE;
	m_FakePing = 0;

	m_vWeaponLimit.resize(NUM_WEAPONS);

	m_Gamemode = GameServer()->Config()->m_SvVanillaModeStart ? GAMEMODE_VANILLA : GAMEMODE_DDRACE;
	m_SavedGamemode = m_Gamemode;

	m_RemovedName = false;
	m_ShowName = true;

	m_ResumeMoved = false;

	m_RainbowSpeed = GameServer()->Config()->m_SvRainbowSpeedDefault;
	m_RainbowColor = 0;
	m_InfRainbow = false;
	m_InfMeteors = 0;

	m_InstagibScore = 0;

	m_ForceSpawnPos = vec2(-1, -1);
	m_WeaponIndicator = GameServer()->Config()->m_SvWeaponIndicatorDefault;

	m_SavedMinigameTee = false;
	m_Minigame = MINIGAME_NONE;
	m_SurvivalState = SURVIVAL_OFFLINE;

	m_SpookyGhost = false;
	m_HasSpookyGhost = false;

	m_ConfettiWinEffectTick = 0;

	m_ScoreMode = GameServer()->Config()->m_SvDefaultScoreMode;
	m_HasRoomKey = false;

	m_ForcedSkin = SKIN_NONE;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		m_HidePlayerTeam[i] = TEAM_RED;
		m_aMuted[i] = false;
	}

	for (int i = 0; i < NUM_HOUSES; i++)
		if (GameServer()->m_pHouses[i]) // if a bot is created by a tile the shops are not yet created
			GameServer()->m_pHouses[i]->Reset(m_ClientID);

	m_pControlledTee = 0;
	m_TeeControllerID = -1;
	m_TeeControlMode = false;
	m_HasTeeControl = false;
	m_TeeControlForcedID = -1;
	m_ClanProtectionPunished = false;

	for (int i = 0; i < NUM_PORTALS; i++)
		m_pPortal[i] = 0;

	m_PlotAuctionPrice = 0;
	m_aPlotSwapUsername[0] = '\0';
	m_PlotSpawn = false;
	m_ToggleSpawn = false;
	m_CheckedSavePlayer = false;
	m_LoadedSavedPlayer = false;
	m_WalletMoney = 0;
	m_LastMoneyXPBomb = 0;
	m_LastVote = 0;
	m_LastPlotAuction = 0;
	m_aSecurityPin[0] = '\0';
	m_LocalChat = false;

	m_SpawnBlockScore = 0;
	m_EscapeTime = 0;
	m_JailTime = 0;
	m_SilentFarm = 0;
	m_SkipSetViewPos = 0;

	// Set this to MINIGAME_NONE so we dont have a timer when we want to leave a minigame, just when we enter
	m_RequestedMinigame = MINIGAME_NONE;
	m_LastMinigameRequest = 0;

	m_ViewCursorID = -2;
	m_ViewCursorZoomed = false;

	m_ZoomCursor = false;
	m_StandardShowDistance = m_ShowDistance;
	m_SentShowDistance = false;

	for (int i = 0; i < VANILLA_MAX_CLIENTS; i++)
		m_aStrongWeakID[i] = 0;

	m_HideDrawings = false;
	m_LastMovementTick = 0;

	m_VoteQuestionRunning = false;
	m_VoteQuestionType = CPlayer::VOTE_QUESTION_NONE;
	m_VoteQuestionEndTick = 0;
	m_LastRedirectTryTick = 0;
}

void CPlayer::Tick()
{
	if(!IsDummy() && !Server()->ClientIngame(m_ClientID))
		return;

	if (m_KillMe != 0)
	{
		KillCharacter(m_KillMe);
		m_KillMe = 0;
		return;
	}

	if (m_ChatScore > 0)
		m_ChatScore--;

	Server()->SetClientScore(m_ClientID, GameServer()->Config()->m_SvDefaultScoreMode == SCORE_TIME ? m_Score : GameServer()->Config()->m_SvDefaultScoreMode == SCORE_LEVEL ? GameServer()->m_Accounts[GetAccID()].m_Level : GameServer()->m_Accounts[GetAccID()].m_BlockPoints);

	// do latency stuff
	if (!m_IsDummy)
	{
		IServer::CClientInfo Info;
		if(Server()->GetClientInfo(m_ClientID, &Info))
		{
			m_Latency.m_Accum += Info.m_Latency;
			m_Latency.m_AccumMax = max(m_Latency.m_AccumMax, Info.m_Latency);
			m_Latency.m_AccumMin = min(m_Latency.m_AccumMin, Info.m_Latency);
		}
		// each second
		if(Server()->Tick()%Server()->TickSpeed() == 0)
		{
			m_Latency.m_Avg = m_Latency.m_Accum/Server()->TickSpeed();
			m_Latency.m_Max = m_Latency.m_AccumMax;
			m_Latency.m_Min = m_Latency.m_AccumMin;
			m_Latency.m_Accum = 0;
			m_Latency.m_AccumMin = 1000;
			m_Latency.m_AccumMax = 0;
		}
	}

	if(Server()->GetNetErrorString(m_ClientID)[0])
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "'%s' would have timed out, but can use timeout protection now", Server()->ClientName(m_ClientID));
		GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
		Server()->ResetNetErrorString(m_ClientID);
	}

	if (!GameServer()->m_World.m_Paused)
	{
		int EarliestRespawnTick = m_PreviousDieTick + Server()->TickSpeed() * 3;
		int RespawnTick = max(m_DieTick, EarliestRespawnTick)+2;
		if (!m_pCharacter && RespawnTick <= Server()->Tick())
			m_Spawning = true;

		if(!m_pCharacter && (m_Team == TEAM_SPECTATORS || m_Paused) && m_pSpecFlag)
		{
			if(m_pSpecFlag->GetCarrier())
				m_SpectatorID = m_pSpecFlag->GetCarrier()->GetPlayer()->GetCID();
			else
				m_SpectatorID = -1;
		}

		if(m_pCharacter)
		{
			if (m_pCharacter->IsAlive())
			{
				ProcessPause();
				if (!m_Paused && !m_pControlledTee && !GameServer()->Arenas()->IsConfiguring(m_ClientID))
					m_ViewPos = m_pCharacter->GetPos();
			}
			else if (!m_pCharacter->IsPaused())
			{
				delete m_pCharacter;
				m_pCharacter = 0;
			}
		}
		else if(m_Spawning && !m_WeakHookSpawn)
			TryRespawn();
	}
	else
	{
		++m_PreviousDieTick;
		++m_DieTick;
		++m_ScoreStartTick;
		++m_LastActionTick;
		++m_TeamChangeTick;
 	}

	m_TuneZoneOld = m_TuneZone; // determine needed tunings with viewpos
	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_ViewPos);
	m_TuneZone = GameServer()->Collision()->IsTune(CurrentIndex);

	if (m_TuneZone != m_TuneZoneOld) // don't send tunigs all the time
	{
		GameServer()->SendTuningParams(m_ClientID, m_TuneZone);
	}

	// checking whether scoreboard is activated or not
	if (m_pCharacter)
	{
		if (m_PlayerFlags&PLAYERFLAG_SCOREBOARD)
		{
			for (int i = 0; i < NUM_HOUSES; i++)
				GameServer()->m_pHouses[i]->ResetLastMotd(m_ClientID);
		}
		else
			m_pCharacter->m_TabDoubleClickCount = 0;
	}

	// name
	if (!m_RemovedName && !m_ShowName && !ShowNameShortRunning())
	{
		SetName(" ");
		SetClan("");
		UpdateInformation();
		m_RemovedName = true;
	}
	if (m_RemovedName && (m_ShowName || ShowNameShortRunning()))
	{
		SetName(Server()->ClientName(m_ClientID));
		SetClan(Server()->ClientClan(m_ClientID));
		UpdateInformation();
		m_RemovedName = false;
	}

	// fixing messages if name is hidden
	if (ShowNameShortRunning() && m_ShowNameShortTick < Server()->Tick())
	{
		m_ShowNameShortTick = 0;

		if (m_pKillMsgNoName)
		{
			for (int i = 0; i < MAX_CLIENTS; i++)
				if (GameServer()->m_apPlayers[i] && (!GameServer()->Config()->m_SvHideMinigamePlayers || (m_Minigame == GameServer()->m_apPlayers[i]->m_Minigame)))
					Server()->SendPackMsg(m_pKillMsgNoName, MSGFLAG_VITAL, i);

			delete m_pKillMsgNoName;
			m_pKillMsgNoName = 0;
		}
	}

	// dummy fake ping
	if (m_IsDummy && GameServer()->Config()->m_SvFakeDummyPing && Server()->Tick() % 200 == 0)
		m_FakePing = 32 + rand() % 11;

	// rainbow
	RainbowTick();

	if (GameServer()->IsFullHour())
		ExpireItems();

	// reduce spawnblocks every 30 seconds
	if (m_SpawnBlockScore > 0 && Server()->Tick() % (Server()->TickSpeed() * 30) == 0)
		m_SpawnBlockScore--;

	if (m_JailTime > 1) // 1 is the indicator to respawn at the jail release tile
	{
		m_JailTime--;
		if (m_JailTime == 1)
		{
			GameServer()->SendChatTarget(m_ClientID, "You were released from jail");
			KillCharacter(WEAPON_GAME);
		}
		else if (Server()->Tick() % 50 == 0 && (!m_pCharacter || !m_pCharacter->m_MoneyTile))
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "You are arrested for %lld seconds", m_JailTime / Server()->TickSpeed());
			GameServer()->SendBroadcast(aBuf, m_ClientID, false);
		}
	}

	if (m_EscapeTime > 0)
	{
		m_EscapeTime--;
		if (m_EscapeTime == 1)
		{
			GameServer()->SendChatTarget(m_ClientID, "Your life as a gangster is over, you are free now");
		}
		else if (Server()->Tick() % Server()->TickSpeed() * 60 == 0)
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Avoid policehammers for the next %lld seconds", m_EscapeTime / Server()->TickSpeed());
			GameServer()->SendBroadcast(aBuf, m_ClientID, false);
		}
	}

	MinigameRequestTick();
	MinigameAfkCheck();

	// Automatic close/stop after 30 seconds
	if (m_VoteQuestionEndTick && Server()->Tick() > m_VoteQuestionEndTick)
		OnEndVoteQuestion();
}

void CPlayer::PostTick()
{
	// update latency value
	if(m_PlayerFlags&PLAYERFLAG_SCOREBOARD || (m_pControlledTee && m_pControlledTee->m_PlayerFlags&PLAYERFLAG_SCOREBOARD))
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				m_aActLatency[i] = GameServer()->m_apPlayers[i]->m_Latency.m_Min;
		}
	}

	// update view pos for spectators and dead players
	if((m_Team == TEAM_SPECTATORS || m_Paused) && m_SpecMode != SPEC_FREEVIEW)
	{
		if(m_pSpecFlag)
			m_ViewPos = m_pSpecFlag->GetPos();
		else if (GameServer()->GetPlayerChar(m_SpectatorID))
			m_ViewPos = GameServer()->GetPlayerChar(m_SpectatorID)->GetPos();
	}
	else if (m_Team != TEAM_SPECTATORS && !m_Paused && m_pControlledTee && m_pControlledTee->GetCharacter())
		m_ViewPos = m_pControlledTee->GetCharacter()->GetPos();
}

void CPlayer::PostPostTick()
{
	if(!IsDummy() && !Server()->ClientIngame(m_ClientID))
		return;

	if(!GameServer()->m_World.m_Paused && !m_pCharacter && m_Spawning && m_WeakHookSpawn)
		TryRespawn();
}

void CPlayer::SendConnect(int FakeID, int ClientID)
{
	if (Server()->IsSevendown(m_ClientID))
		return;

	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if (!pPlayer)
		return;

	CNetMsg_Sv_ClientInfo NewClientInfoMsg;
	NewClientInfoMsg.m_ClientID = FakeID;
	NewClientInfoMsg.m_Local = ClientID == m_ClientID;
	NewClientInfoMsg.m_Team = pPlayer->GetHidePlayerTeam(m_ClientID);
	NewClientInfoMsg.m_pName = pPlayer->m_CurrentInfo.m_aName;
	NewClientInfoMsg.m_pClan = pPlayer->m_CurrentInfo.m_aClan;
	NewClientInfoMsg.m_Country = Server()->ClientCountry(ClientID);
	NewClientInfoMsg.m_Silent = 1;

	for (int p = 0; p < NUM_SKINPARTS; p++)
	{
		NewClientInfoMsg.m_apSkinPartNames[p] = pPlayer->m_CurrentInfo.m_TeeInfos.m_aaSkinPartNames[p];
		NewClientInfoMsg.m_aUseCustomColors[p] = pPlayer->m_CurrentInfo.m_TeeInfos.m_aUseCustomColors[p];
		NewClientInfoMsg.m_aSkinPartColors[p] = pPlayer->m_CurrentInfo.m_TeeInfos.m_aSkinPartColors[p];
	}

	Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD|MSGFLAG_NOTRANSLATE, m_ClientID);
}

void CPlayer::SendDisconnect(int FakeID)
{
	if (Server()->IsSevendown(m_ClientID))
		return;

	CNetMsg_Sv_ClientDrop ClientDropMsg;
	ClientDropMsg.m_ClientID = FakeID;
	ClientDropMsg.m_pReason = "";
	ClientDropMsg.m_Silent = 1;

	Server()->SendPackMsg(&ClientDropMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD|MSGFLAG_NOTRANSLATE, m_ClientID);
}

void CPlayer::Snap(int SnappingClient)
{
	if(!IsDummy() && !Server()->ClientIngame(m_ClientID))
		return;

	int ID = m_ClientID;
	if (SnappingClient > -1 && !Server()->Translate(ID, SnappingClient))
		return;

	int Size = Server()->IsSevendown(SnappingClient) ? 5*4 : sizeof(CNetObj_PlayerInfo);
	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, ID, Size));
	if(!pPlayerInfo)
		return;

	CPlayer *pSnapping = GameServer()->m_apPlayers[SnappingClient];

	int Latency = 0;
	{
		// realistic ping for dummies
		if (m_IsDummy && GameServer()->Config()->m_SvFakeDummyPing)
			Latency = m_FakePing;
		else
			Latency = SnappingClient == -1 ? m_Latency.m_Min : GameServer()->m_apPlayers[SnappingClient]->m_aActLatency[m_ClientID];
	}
	
	int Score = 0;
	{
		bool AccUsed = true;

		// check for minigames first, then normal score modes, as minigames of course overwrite the wanted scoremodes
		if (pSnapping->m_Minigame == MINIGAME_BLOCK)
			Score = GameServer()->m_Accounts[GetAccID()].m_Kills;
		else if (pSnapping->m_Minigame == MINIGAME_SURVIVAL)
			Score = GameServer()->m_Accounts[GetAccID()].m_SurvivalKills;
		else if (pSnapping->m_Minigame == MINIGAME_INSTAGIB_BOOMFNG || pSnapping->m_Minigame == MINIGAME_INSTAGIB_FNG)
		{
			Score = m_InstagibScore;
			AccUsed = false;
		}
		else if (pSnapping->m_Minigame == MINIGAME_1VS1 || (m_Minigame == MINIGAME_1VS1 && GameServer()->Arenas()->FightStarted(m_ClientID) && pSnapping->m_ScoreMode != SCORE_TIME)) // 1vs1 broadcasts 1vs1 score to everyone
		{
			Score = GameServer()->Arenas()->GetClientScore(m_ClientID);
			AccUsed = false;
		}
		else if (pSnapping->m_ScoreMode == SCORE_TIME)
		{
			// send 0 if times of others are not shown
			if (SnappingClient != m_ClientID && GameServer()->Config()->m_SvHideScore)
				Score = -1;
			else
				Score = m_Score == -1 ? -1 : abs(m_Score) * 1000.0f;
			AccUsed = false;

			if (Server()->IsSevendown(SnappingClient))
			{
				if (Score == -1)
					Score = -9999;
				else
					Score = abs(m_Score) * -1;
			}
		}
		else if (pSnapping->m_ScoreMode == SCORE_LEVEL)
			Score = GameServer()->m_Accounts[GetAccID()].m_Level;
		else if (pSnapping->m_ScoreMode == SCORE_BLOCK_POINTS)
			Score = GameServer()->m_Accounts[GetAccID()].m_BlockPoints;

		if (AccUsed && GetAccID() < ACC_START)
			Score = 0;
	}

	if (m_ClientID == SnappingClient && (m_Team == TEAM_SPECTATORS || m_Paused || m_TeeControlMode || GameServer()->Arenas()->IsConfiguring(m_ClientID)))
	{
		int Size = Server()->IsSevendown(SnappingClient) ? 3*4 : sizeof(CNetObj_SpectatorInfo);
		CNetObj_SpectatorInfo* pSpectatorInfo = static_cast<CNetObj_SpectatorInfo*>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, Size));
		if (!pSpectatorInfo)
			return;

		int SpecMode = m_SpecMode;
		int SpectatorID = m_SpectatorID;

		if (m_Team != TEAM_SPECTATORS && !m_Paused)
		{
			if (m_pControlledTee)
			{
				SpecMode = SPEC_PLAYER;
				SpectatorID = m_pControlledTee->GetCID();
			}
			else
			{
				bool ClampViewPos = GameServer()->Arenas()->ClampViewPos(m_ClientID);
				if (ClampViewPos)
					SkipSetViewPos();

				if (m_TeeControlMode || ClampViewPos)
				{
					SpecMode = SPEC_PLAYER;
					SpectatorID = m_ClientID;
				}
				else if (GameServer()->Arenas()->IsConfiguring(m_ClientID))
				{
					SpecMode = SPEC_FREEVIEW;
					SpectatorID = -1;
				}
			}
		}
		else
		{
			// when we are spectating while being affected by rainbowname people we dont wanna focus on one player, so that no tee gets transparent due to IsOtherTeam
			// its now actually only activated when we spec someone who has rainbowname, rest is handled in the update function
			if (SpecMode == SPEC_PLAYER && GameServer()->m_RainbowName.IsAffected(m_ClientID))
			{
				SpectatorID = m_ClientID;
			}
		}

		if (SpecMode != SPEC_FREEVIEW && SpectatorID >= 0)
		{
			if (!Server()->Translate(SpectatorID, m_ClientID))
			{
				SpectatorID = ID;
				SpecMode = SPEC_PLAYER;
			}
		}

		if (Server()->IsSevendown(m_ClientID))
		{
			if (m_pSpecFlag && (SpecMode == SPEC_FLAGRED || SpecMode == SPEC_FLAGBLUE))
			{
				((int*)pSpectatorInfo)[0] = SpectatorID != -1 ? SpectatorID : ID;
				((int*)pSpectatorInfo)[1] = m_pSpecFlag->GetPos().x;
				((int*)pSpectatorInfo)[2] = m_pSpecFlag->GetPos().y;
			}
			else
			{
				((int*)pSpectatorInfo)[0] = SpectatorID;
				((int*)pSpectatorInfo)[1] = m_ViewPos.x;
				((int*)pSpectatorInfo)[2] = m_ViewPos.y;
			}
		}
		else
		{
			pSpectatorInfo->m_SpecMode = SpecMode;
			pSpectatorInfo->m_SpectatorID = SpectatorID;
			if (m_pSpecFlag && (SpecMode == SPEC_FLAGRED || SpecMode == SPEC_FLAGBLUE))
			{
				pSpectatorInfo->m_X = m_pSpecFlag->GetPos().x;
				pSpectatorInfo->m_Y = m_pSpecFlag->GetPos().y;
			}
			else
			{
				pSpectatorInfo->m_X = m_ViewPos.x;
				pSpectatorInfo->m_Y = m_ViewPos.y;
			}
		}
	}

	// demo recording
	if(SnappingClient == -1)
	{
		CNetObj_De_ClientInfo *pClientInfo = static_cast<CNetObj_De_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_DE_CLIENTINFO, m_ClientID, sizeof(CNetObj_De_ClientInfo)));
		if(!pClientInfo)
			return;

		pClientInfo->m_Local = 0;
		pClientInfo->m_Team = m_Team;
		StrToInts(pClientInfo->m_aName, 4, m_CurrentInfo.m_aName);
		StrToInts(pClientInfo->m_aClan, 3, m_CurrentInfo.m_aClan);
		pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);

		for(int p = 0; p < NUM_SKINPARTS; p++)
		{
			StrToInts(pClientInfo->m_aaSkinPartNames[p], 6, m_CurrentInfo.m_TeeInfos.m_aaSkinPartNames[p]);
			pClientInfo->m_aUseCustomColors[p] = m_CurrentInfo.m_TeeInfos.m_aUseCustomColors[p];
			pClientInfo->m_aSkinPartColors[p] = m_CurrentInfo.m_TeeInfos.m_aSkinPartColors[p];
		}
	}

	if(Server()->IsSevendown(SnappingClient))
	{
		int *pClientInfo = (int*)Server()->SnapNewItem(11 + NUM_NETOBJTYPES, ID, 17*4); // NETOBJTYPE_CLIENTINFO
		if(!pClientInfo)
			return;

		StrToInts(&pClientInfo[0], 4, m_CurrentInfo.m_aName);
		StrToInts(&pClientInfo[4], 3, m_CurrentInfo.m_aClan);
		pClientInfo[7] = Server()->ClientCountry(m_ClientID);
		StrToInts(&pClientInfo[8], 6, m_CurrentInfo.m_TeeInfos.m_Sevendown.m_SkinName);
		pClientInfo[14] = m_CurrentInfo.m_TeeInfos.m_Sevendown.m_UseCustomColor;
		pClientInfo[15] = m_CurrentInfo.m_TeeInfos.m_Sevendown.m_ColorBody;
		pClientInfo[16] = m_CurrentInfo.m_TeeInfos.m_Sevendown.m_ColorFeet;

		((int*)pPlayerInfo)[0] = (int)((m_ClientID == SnappingClient && (!m_pControlledTee || m_Paused)) || (m_TeeControllerID == SnappingClient && !pSnapping->m_Paused));
		((int*)pPlayerInfo)[1] = ID;
		((int*)pPlayerInfo)[2] = (!m_TeeControlMode || m_pControlledTee) ? GetHidePlayerTeam(SnappingClient) : TEAM_SPECTATORS;
		if (GameServer()->GetClientDDNetVersion(SnappingClient) < VERSION_DDNET_INDEPENDENT_SPECTATORS_TEAM)
		{
			((int*)pPlayerInfo)[2] = ((m_Paused != PAUSE_PAUSED && (!m_TeeControlMode || m_pControlledTee) && !GameServer()->Arenas()->IsConfiguring(m_ClientID)) || m_ClientID != SnappingClient) && m_Paused < PAUSE_SPEC ? GetHidePlayerTeam(SnappingClient) : TEAM_SPECTATORS;
		}
		((int*)pPlayerInfo)[3] = Score;
		((int*)pPlayerInfo)[4] = Latency;
	}
	else
	{
		pPlayerInfo->m_PlayerFlags = m_PlayerFlags&PLAYERFLAG_CHATTING;
		if(GetAuthedHighlighted())
			pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_ADMIN;
		if(m_IsReadyToPlay)
			pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_READY;
		if(m_Paused)
			pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_DEAD;
		if(SnappingClient != -1 && (m_Team == TEAM_SPECTATORS || m_Paused) && (SnappingClient == m_SpectatorID))
			pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_WATCHING;
		if (m_IsDummy && GameServer()->Config()->m_SvDummyBotSkin)
			pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_BOT;
		if (GameServer()->GetClientDDNetVersion(SnappingClient) >= VERSION_DDNET && (m_PlayerFlags&PLAYERFLAG_AIM))
			pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_AIM;

		pPlayerInfo->m_Latency = Latency;
		pPlayerInfo->m_Score = Score;
	}

	CNetObj_DDNetPlayer *pDDNetPlayer = static_cast<CNetObj_DDNetPlayer *>(Server()->SnapNewItem(NETOBJTYPE_DDNETPLAYER, ID, sizeof(CNetObj_DDNetPlayer)));
	if(!pDDNetPlayer)
		return;

	pDDNetPlayer->m_AuthLevel = GetAuthedHighlighted();
	pDDNetPlayer->m_Flags = 0;
	if(m_Afk || GameServer()->Arenas()->IsConfiguring(m_ClientID) || Server()->DesignChanging(m_ClientID))
		pDDNetPlayer->m_Flags |= EXPLAYERFLAG_AFK;
	if(m_Paused == PAUSE_SPEC)
		pDDNetPlayer->m_Flags |= EXPLAYERFLAG_SPEC;
	if(m_Paused == PAUSE_PAUSED || GameServer()->Arenas()->IsConfiguring(m_ClientID))
		pDDNetPlayer->m_Flags |= EXPLAYERFLAG_PAUSED;

	bool ShowSpec = false;
	vec2 SpecPos;
	if (m_pCharacter)
	{
		ShowSpec = m_pCharacter->IsPaused() && m_pCharacter->CanSnapCharacter(SnappingClient);
		SpecPos = m_pCharacter->Core()->m_Pos;
	}

	if (IsMinigame() && m_SavedMinigameTee)
	{
		ShowSpec = true;
		SpecPos = m_MinigameTee.GetPos();
	}

	if(SnappingClient >= 0)
	{
		bool ShowTeam = !GameServer()->Arenas()->FightStarted(SnappingClient) && (GameServer()->GetDDRaceTeam(m_ClientID) == GameServer()->GetDDRaceTeam(SnappingClient) || GameServer()->Arenas()->FightStarted(m_ClientID));
		ShowSpec = ShowSpec && (ShowTeam || pSnapping->m_ShowOthers == 1 || (pSnapping->GetTeam() == TEAM_SPECTATORS || pSnapping->IsPaused()));
	}

	if(ShowSpec)
	{
		CNetObj_SpecChar *pSpecChar = static_cast<CNetObj_SpecChar *>(Server()->SnapNewItem(NETOBJTYPE_SPECCHAR, ID, sizeof(CNetObj_SpecChar)));
		if (!pSpecChar)
			return;
		pSpecChar->m_X = SpecPos.x;
		pSpecChar->m_Y = SpecPos.y;
	}
}

void CPlayer::FakeSnap()
{
	// see others in spec
	int SeeOthersID = GameServer()->m_World.GetSeeOthersID(m_ClientID);

	if (!Server()->IsSevendown(m_ClientID))
	{
		if (GameServer()->m_World.GetTotalOverhang(m_ClientID))
		{
			CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, SeeOthersID, sizeof(CNetObj_PlayerInfo)));
			if(!pPlayerInfo)
				return;

			pPlayerInfo->m_PlayerFlags = 0;
			pPlayerInfo->m_Score = m_ScoreMode == SCORE_TIME ? -9999 : -1;
			pPlayerInfo->m_Latency = 0;
		}

		// nothing more to process in fake snap for 0.7
		return;
	}

	// see others
	if (GameServer()->m_World.GetTotalOverhang(m_ClientID))
	{
		int *pClientInfo = (int*)Server()->SnapNewItem(11 + NUM_NETOBJTYPES, SeeOthersID, 17*4); // NETOBJTYPE_CLIENTINFO
		if(!pClientInfo)
			return;

		StrToInts(&pClientInfo[0], 4, GameServer()->m_World.GetSeeOthersName(m_ClientID));
		StrToInts(&pClientInfo[4], 3, "");
		StrToInts(&pClientInfo[8], 6, "default");
		pClientInfo[14] = 1;
		pClientInfo[15] = pClientInfo[16] = 6618880;

		CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, SeeOthersID, 5*4));
		if (!pPlayerInfo)
			return;

		((int*)pPlayerInfo)[0] = 0;
		((int*)pPlayerInfo)[1] = SeeOthersID;
		((int*)pPlayerInfo)[2] = TEAM_BLUE;
		((int*)pPlayerInfo)[3] = m_ScoreMode == SCORE_TIME ? -9999 : -1;
		((int*)pPlayerInfo)[4] = 0;
	}

	// empty name to say chat messages
	int FakeID = VANILLA_MAX_CLIENTS - 1;

	int *pClientInfo = (int*)Server()->SnapNewItem(11 + NUM_NETOBJTYPES, FakeID, 17*4); // NETOBJTYPE_CLIENTINFO
	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo[0], 4, " ");
	StrToInts(&pClientInfo[4], 3, "");
	StrToInts(&pClientInfo[8], 6, "default");

	// flags
	if (!GameServer()->FlagsUsed())
		return;

	for (int i = 0; i < 2; i++)
	{
		FakeID--;
		pClientInfo = (int*)Server()->SnapNewItem(11 + NUM_NETOBJTYPES, FakeID, 17*4); // NETOBJTYPE_CLIENTINFO
		if(!pClientInfo)
			return;

		StrToInts(&pClientInfo[0], 4, i == TEAM_RED ? "Red Flag" : "Blue Flag");
		StrToInts(&pClientInfo[4], 3, "");
		StrToInts(&pClientInfo[8], 6, "default");
		pClientInfo[14] = 1;
		pClientInfo[15] = pClientInfo[16] = i == TEAM_RED ? 65387 : 10223467;

		CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, FakeID, 5*4));
		if (!pPlayerInfo)
			return;

		((int*)pPlayerInfo)[0] = 0;
		((int*)pPlayerInfo)[1] = FakeID;
		((int*)pPlayerInfo)[2] = TEAM_BLUE;
		((int*)pPlayerInfo)[3] = m_ScoreMode == SCORE_TIME ? -9999 : -1;
		((int*)pPlayerInfo)[4] = 0;
	}

	// Smooth flag hooking
	int Team = -1;
	if (GetCharacter())
	{
		if (GetCharacter()->Core()->HookedPlayer() == HOOK_FLAG_BLUE)
			Team = TEAM_BLUE;
		else if (GetCharacter()->Core()->HookedPlayer() == HOOK_FLAG_RED)
			Team = TEAM_RED;
	}

	if (Team == -1)
		return;

	CFlag *pFlag = ((CGameControllerDDRace *)GameServer()->m_pController)->m_apFlags[Team];
	if (!pFlag)
		return;

	// We dont send the NETOBJTYPE_PLAYERINFO object, because that would make the client render the tee. We could send it every 2nd snapshot, so the client doesnt
	// have the SNAP_PREV of it and wont render it aswell, but it seems to me that not having the object at all is also fine
	FakeID = VANILLA_MAX_CLIENTS - 1;

	// Send character at the position of the flag we are currently hooking
	CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, FakeID, sizeof(CNetObj_Character)));
	if(!pCharacter)
		return;

	pCharacter->m_X = pFlag->GetPos().x;
	pCharacter->m_Y = pFlag->GetPos().y;

	// If the flag is getting hooked while close to us, the client predicts the invisible fake tee as if it would be colliding with us
	CNetObj_DDNetCharacter *pDDNetCharacter = static_cast<CNetObj_DDNetCharacter *>(Server()->SnapNewItem(NETOBJTYPE_DDNETCHARACTER, FakeID, sizeof(CNetObj_DDNetCharacter)));
	if(!pDDNetCharacter)
		return;
	pDDNetCharacter->m_Flags = CHARACTERFLAG_NO_COLLISION;
}

int CPlayer::GetHidePlayerTeam(int Asker)
{
	CPlayer *pAsker = GameServer()->m_apPlayers[Asker];
	if (m_TeeControllerID != Asker && m_Team != TEAM_SPECTATORS && ((GameServer()->Config()->m_SvHideDummies && m_IsDummy)
		|| (GameServer()->Config()->m_SvHideMinigamePlayers && (m_Minigame != MINIGAME_1VS1 || !GameServer()->Arenas()->FightStarted(m_ClientID)) && pAsker->m_Minigame != m_Minigame)))
		return TEAM_BLUE;
	return m_Team;
}

bool CPlayer::IsMinigame()
{
	return m_Minigame != MINIGAME_NONE;
}

int CPlayer::GetAuthedHighlighted()
{
	if (GameServer()->Config()->m_SvLocalChat)
		return m_LocalChat ? AUTHED_HELPER : AUTHED_NO;
	return GameServer()->Config()->m_SvAuthedHighlighted ? Server()->GetAuthedState(m_ClientID) : AUTHED_NO;
}

bool CPlayer::RestrictZoom()
{
	// allow zoom in block and 1vs1
	return IsMinigame() && m_Minigame != MINIGAME_BLOCK && m_Minigame != MINIGAME_1VS1;
}

float CPlayer::GetZoomLevel()
{
	return m_ShowDistance.x / m_StandardShowDistance.x;
}

bool CPlayer::JoinChat(bool Local)
{
	if (m_LocalChat == Local || !GameServer()->Config()->m_SvLocalChat)
		return false;

	m_LocalChat = Local;
	if (m_LocalChat)
		GameServer()->SendChatTarget(m_ClientID, "Entered local chat");
	else
		GameServer()->SendChatTarget(m_ClientID, "Entered public chat");
	return true;
}

void CPlayer::OnDisconnect()
{
	if (m_JailTime || m_EscapeTime)
		GameServer()->SaveCharacter(m_ClientID, SAVE_JAIL, GameServer()->Config()->m_SvJailSaveTeeExpire);

	// Make sure to call this before the character dies because on disconnect it should drop the money even when frozen
	if (GameServer()->Config()->m_SvDropsOnDeath && m_pCharacter)
		m_pCharacter->DropMoney(GetWalletMoney());
	KillCharacter();
	GameServer()->Arenas()->OnPlayerLeave(m_ClientID, true);
	GameServer()->Logout(GetAccID());

	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
	Controller->m_Teams.SetForceCharacterTeam(m_ClientID, 0);

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_Team != TEAM_SPECTATORS)
		{
			// update spectator modes
			if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_SpecMode == SPEC_PLAYER && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
			{
				GameServer()->m_apPlayers[i]->m_SpecMode = SPEC_FREEVIEW;
				GameServer()->m_apPlayers[i]->m_SpectatorID = -1;
			}
		}

		if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_LastWhisperTo == m_ClientID)
			GameServer()->m_apPlayers[i]->m_LastWhisperTo = -1;

		CCharacter* pChr = GameServer()->GetPlayerChar(i);
		if (pChr && pChr->Core()->m_Killer.m_ClientID == m_ClientID)
		{
			pChr->Core()->m_Killer.m_ClientID = -1;
			pChr->Core()->m_Killer.m_Weapon = -1;
		}

		if (GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_HidePlayerTeam[m_ClientID] = TEAM_RED;
			GameServer()->m_apPlayers[i]->m_aMuted[m_ClientID] = false;

			if (GameServer()->m_apPlayers[i]->m_TeeControlForcedID == m_ClientID)
			{
				GameServer()->m_apPlayers[i]->m_HasTeeControl = false;
				GameServer()->m_apPlayers[i]->m_TeeControlForcedID = -1;
			}
		}
	}
}

void CPlayer::TranslatePlayerFlags(CNetObj_PlayerInput *NewInput)
{
	if (!Server()->IsSevendown(m_ClientID))
		return;

	int PlayerFlags = 0;
	if (NewInput->m_PlayerFlags&4) PlayerFlags |= PLAYERFLAG_CHATTING;
	if (NewInput->m_PlayerFlags&8) PlayerFlags |= PLAYERFLAG_SCOREBOARD;
	if (NewInput->m_PlayerFlags&16) PlayerFlags |= PLAYERFLAG_AIM;
	NewInput->m_PlayerFlags = PlayerFlags;
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *NewInput, bool TeeControlled)
{
	// F-DDrace
	if (m_pControlledTee && !m_Paused && !TeeControlled)
	{
		TranslatePlayerFlags(NewInput);
		m_pControlledTee->OnPredictedInput(NewInput, true);
		return;
	}
	else if (m_TeeControllerID != -1 && !TeeControlled)
		return;
	else
		TranslatePlayerFlags(NewInput);

	// skip the input if chat is active
	if((m_PlayerFlags&PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING))
		return;

	AfkVoteTimer(NewInput);

	if(m_pCharacter && !m_Paused && (!m_TeeControlMode || TeeControlled))
		m_pCharacter->OnPredictedInput(NewInput);
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput *NewInput, bool TeeControlled)
{
	// F-DDrace
	if (m_pControlledTee && !m_Paused && !TeeControlled)
	{
		TranslatePlayerFlags(NewInput);
		m_pControlledTee->OnDirectInput(NewInput, true);
		return;
	}
	else if (m_TeeControllerID != -1 && !TeeControlled)
		return;
	else
		TranslatePlayerFlags(NewInput);

	if (AfkTimer(NewInput->m_TargetX, NewInput->m_TargetY))
		return; // we must return if kicked, as player struct is already deleted
	AfkVoteTimer(NewInput);

	if(GameServer()->m_World.m_Paused)
	{
		m_PlayerFlags = NewInput->m_PlayerFlags;
		return;
	}

	if ((((!m_pCharacter && m_Team == TEAM_SPECTATORS) || m_Paused) && m_SpecMode == SPEC_FREEVIEW) || GameServer()->Arenas()->IsConfiguring(m_ClientID))
	{
		if (m_SkipSetViewPos <= 0)
			m_ViewPos = vec2(NewInput->m_TargetX, NewInput->m_TargetY);
		else
			m_SkipSetViewPos--;
	}
	else
		SkipSetViewPos();

	if(NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING)
	{
		// skip the input if chat is active
		if(m_PlayerFlags&PLAYERFLAG_CHATTING)
			return;

		// reset input
		if(m_pCharacter)
			m_pCharacter->ResetInput();

		m_PlayerFlags = NewInput->m_PlayerFlags;
		return;
	}

	m_PlayerFlags = NewInput->m_PlayerFlags;

	if (m_pCharacter && !ApplyDirectInput(TeeControlled))
	{
		m_pCharacter->ResetNumInputs();

		if (m_pControlledTee && m_pControlledTee->m_pCharacter)
			m_pControlledTee->m_pCharacter->ResetNumInputs();
	}

	if(!m_pCharacter && m_Team != TEAM_SPECTATORS && !GameServer()->Arenas()->FightStarted(m_ClientID) && (NewInput->m_Fire&1))
		m_Spawning = true;

	if(((!m_pCharacter && m_Team == TEAM_SPECTATORS) || m_Paused) && (NewInput->m_Fire&1)
		// to prevent clicking after you got killed and immediately unspectate your killer on accident
		&& (m_SurvivalState == SURVIVAL_OFFLINE || m_SurvivalDieTick < Server()->Tick() - Server()->TickSpeed() * 2))
	{
		if(!m_ActiveSpecSwitch)
		{
			m_ActiveSpecSwitch = true;
			if(m_SpecMode == SPEC_FREEVIEW)
			{
				// F-DDrace
				CCharacter *pNotThis = (m_pCharacter && m_pCharacter->IsAlive()) ? m_pCharacter : 0;
				if (m_pControlledTee && m_pControlledTee->m_pCharacter)
					pNotThis = m_pControlledTee->m_pCharacter;

				CCharacter *pChar = (CCharacter *)GameServer()->m_World.ClosestCharacter(m_ViewPos, 6.0f*32, pNotThis, -1, true, false, true); // also allow minigame spec chars to be chosen
				CFlag *pFlag = (CFlag *)GameServer()->m_World.ClosestEntity(m_ViewPos, 6.0f*32, CGameWorld::ENTTYPE_FLAG, 0);
				if(pChar || pFlag)
				{
					if(!pChar || (pFlag && pChar && distance(m_ViewPos, pFlag->GetPos()) < distance(m_ViewPos, pChar->GetPos())))
					{
						m_SpecMode = pFlag->GetTeam() == TEAM_RED ? SPEC_FLAGRED : SPEC_FLAGBLUE;
						m_pSpecFlag = pFlag;
						m_SpectatorID = -1;
					}
					else
					{
						m_SpecMode = SPEC_PLAYER;
						m_pSpecFlag = 0;
						m_SpectatorID = pChar->GetPlayer()->GetCID();
					}
				}
			}
			else
			{
				m_SpecMode = SPEC_FREEVIEW;
				m_pSpecFlag = 0;
				m_SpectatorID = -1;
			}
		}
	}
	else if(m_ActiveSpecSwitch)
		m_ActiveSpecSwitch = false;

	// check for activity
	if(!m_IsDummy && mem_comp(NewInput, m_pLastTarget, sizeof(CNetObj_PlayerInput)))
	{
		mem_copy(m_pLastTarget, NewInput, sizeof(CNetObj_PlayerInput));
		// Ignore the first direct input and keep the player afk as it is sent automatically
		if(m_LastTargetInit)
			UpdatePlaytime();
		m_LastActionTick = Server()->Tick();
		m_LastTargetInit = true;
	}
}

void CPlayer::OnPredictedEarlyInput(CNetObj_PlayerInput *NewInput, bool TeeControlled)
{
	if (m_pControlledTee && !m_Paused && !TeeControlled)
	{
		TranslatePlayerFlags(NewInput);
		m_pControlledTee->OnPredictedEarlyInput(NewInput, true);
		return;
	}
	else if (m_TeeControllerID != -1 && !TeeControlled)
		return;
	else
		TranslatePlayerFlags(NewInput);

	// skip the input if chat is active
	if((m_PlayerFlags&PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING))
		return;

	if(m_pCharacter && ApplyDirectInput(TeeControlled))
		m_pCharacter->OnDirectInput(NewInput);
}

bool CPlayer::ApplyDirectInput(bool TeeControlled)
{
	return !m_Paused && (!m_TeeControlMode || TeeControlled) && !GameServer()->Arenas()->IsConfiguring(m_ClientID);
}

CCharacter *CPlayer::GetCharacter()
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return m_pCharacter;
	return 0;
}

void CPlayer::ThreadKillCharacter(int Weapon)
{
	m_KillMe = Weapon;
}

void CPlayer::KillCharacter(int Weapon, bool UpdateTeeControl)
{
	if(m_pCharacter)
	{
		m_pCharacter->Die(Weapon, UpdateTeeControl);
		delete m_pCharacter;
		m_pCharacter = 0;
	}
}

void CPlayer::Respawn(bool WeakHook)
{
	if(m_Team != TEAM_SPECTATORS)
	{
		m_WeakHookSpawn = WeakHook;
		m_Spawning = true;
	}
}

CCharacter* CPlayer::ForceSpawn(vec2 Pos)
{
	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_pCharacter->Spawn(this, Pos);
	m_Team = 0;
	return m_pCharacter;
}

bool CPlayer::SetSpectatorID(int SpecMode, int SpectatorID)
{
	if((SpecMode == m_SpecMode && SpecMode != SPEC_PLAYER) ||
		(m_SpecMode == SPEC_PLAYER && SpecMode == SPEC_PLAYER && (SpectatorID == -1 || m_SpectatorID == SpectatorID || m_ClientID == SpectatorID)))
	{
		return false;
	}

	if(m_Team == TEAM_SPECTATORS || m_Paused)
	{
		// check for freeview or if wanted player is playing
		if(SpecMode != SPEC_PLAYER || (SpecMode == SPEC_PLAYER && GameServer()->m_apPlayers[SpectatorID] && GameServer()->m_apPlayers[SpectatorID]->GetTeam() != TEAM_SPECTATORS))
		{
			if(SpecMode == SPEC_FLAGRED || SpecMode == SPEC_FLAGBLUE)
			{
				CFlag *pFlag = (CFlag*)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_FLAG);
				while (pFlag)
				{
					if ((pFlag->GetTeam() == TEAM_RED && SpecMode == SPEC_FLAGRED) || (pFlag->GetTeam() == TEAM_BLUE && SpecMode == SPEC_FLAGBLUE))
					{
						m_pSpecFlag = pFlag;
						if (pFlag->GetCarrier())
							m_SpectatorID = pFlag->GetCarrier()->GetPlayer()->GetCID();
						else
							m_SpectatorID = -1;
						break;
					}
					pFlag = (CFlag*)pFlag->TypeNext();
				}
				if (!m_pSpecFlag)
					return false;
				m_SpecMode = SpecMode;
				GameServer()->m_World.ResetSeeOthers(m_ClientID);
				return true;
			}
			m_pSpecFlag = 0;
			m_SpecMode = SpecMode;
			m_SpectatorID = SpectatorID;
			GameServer()->m_World.ResetSeeOthers(m_ClientID);
			return true;
		}
	}

	return false;
}

void CPlayer::SetTeam(int Team, bool DoChatMsg)
{
	Team = GameServer()->m_pController->ClampTeam(Team);
	if(m_Team == Team)
		return;

	if (Team == TEAM_SPECTATORS)
	{
		GameServer()->Arenas()->OnPlayerLeave(m_ClientID);

		CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
		Controller->m_Teams.SetForceCharacterTeam(m_ClientID, 0);
	}

	KillCharacter();

	m_Team = Team;
	m_LastActionTick = Server()->Tick();
	m_SpecMode = SPEC_FREEVIEW;
	m_SpectatorID = -1;
	m_pSpecFlag = 0;

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", m_ClientID, Server()->ClientName(m_ClientID), m_Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		// update spectator modes
		if (Team == TEAM_SPECTATORS)
		{
			if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
			{
				GameServer()->m_apPlayers[i]->m_SpectatorID = -1;
				GameServer()->m_apPlayers[i]->m_SpecMode = SPEC_FREEVIEW;
			}
		}

		if (GameServer()->m_apPlayers[i])
			GameServer()->m_apPlayers[i]->m_HidePlayerTeam[m_ClientID] = TEAM_RED;
	}

	GameServer()->OnClientTeamChange(m_ClientID);

	// notify clients
	GameServer()->SendTeamChange(m_ClientID, Team, true, m_TeamChangeTick, -1);
	GameServer()->UpdateHidePlayers(m_ClientID);

	if (DoChatMsg)
	{
		str_format(aBuf, sizeof(aBuf), "'%s' joined the %s", Server()->ClientName(m_ClientID), GameServer()->m_pController->GetTeamName(Team));
		GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
	}
}

void CPlayer::TryRespawn()
{
	if (m_Team == TEAM_SPECTATORS)
		return;

	vec2 SpawnPos = vec2(-1, -1);
	int Index = ENTITY_SPAWN;
	bool JailRelease = false;

	if (m_ForceSpawnPos != vec2(-1, -1))
	{
		SpawnPos = m_ForceSpawnPos;
	}
	else if (m_DummyMode == DUMMYMODE_SHOP_DUMMY)
	{
		Index = TILE_SHOP;
	}
	else if (m_DummyMode == DUMMYMODE_PLOT_SHOP_DUMMY)
	{
		Index = TILE_PLOT_SHOP;
	}
	else if (m_DummyMode == DUMMYMODE_BANK_DUMMY)
	{
		Index = TILE_BANK;
	}
	else if (m_Minigame == MINIGAME_BLOCK || m_DummyMode == DUMMYMODE_V3_BLOCKER)
	{
		Index = TILE_MINIGAME_BLOCK;
	}
	else if (m_Minigame == MINIGAME_SURVIVAL)
	{
		if (m_SurvivalState == SURVIVAL_LOBBY)
			Index = TILE_SURVIVAL_LOBBY;
		else if (m_SurvivalState == SURVIVAL_PLAYING)
			Index = TILE_SURVIVAL_SPAWN;
		else if (m_SurvivalState == SURVIVAL_DEATHMATCH)
			Index = TILE_SURVIVAL_DEATHMATCH;
	}
	else if (m_Minigame == MINIGAME_INSTAGIB_BOOMFNG)
	{
		Index = ENTITY_SPAWN_RED;
	}
	else if (m_Minigame == MINIGAME_INSTAGIB_FNG)
	{
		Index = ENTITY_SPAWN_BLUE;
	}
	else if (m_Minigame == MINIGAME_1VS1)
	{
		if (GameServer()->Arenas()->FightStarted(m_ClientID))
			SpawnPos = GameServer()->Arenas()->GetSpawnPos(m_ClientID);
		else
			Index = TILE_1VS1_LOBBY;
	}
	else if (m_JailTime == 1)
	{
		m_JailTime = 0;
		JailRelease = true;
		Index = TILE_JAIL_RELEASE;
	}
	else if (m_JailTime)
	{
		Index = TILE_JAIL;
	}
	else if ((m_PlotSpawn && !m_ToggleSpawn) || (!m_PlotSpawn && m_ToggleSpawn))
	{
		int PlotID = GameServer()->GetPlotID(GetAccID());
		if (PlotID > 0)
			SpawnPos = GameServer()->m_aPlots[PlotID].m_ToTele;
	}

	// its gonna be loaded in CCharacter::Spawn()
	if (CanLoadMinigameTee())
		SpawnPos = m_MinigameTee.GetPos();

	if (SpawnPos == vec2(-1, -1))
	{
		if (!GameServer()->Collision()->TileUsed(Index))
			Index = ENTITY_SPAWN;

		if (m_ForceSpawnPos == vec2(-1, -1) && !GameServer()->m_pController->CanSpawn(&SpawnPos, Index))
			return;
	}

	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;

	m_ToggleSpawn = false;
	m_WeakHookSpawn = false;
	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_ViewPos = SpawnPos;
	m_pCharacter->Spawn(this, SpawnPos);

	if (!m_CheckedSavePlayer)
	{
		m_CheckedSavePlayer = true;
		if (GameServer()->CheckLoadPlayer(m_ClientID))
		{
			if (!GetCharacter())
			{
				m_Spawning = true; // if the character got killed due to jail load for example, make him instantly respawn
				return; // return because the character got killed, probably due to loading jail and sending him there
			}

			// update spawnpos for the playerspawn effect
			SpawnPos = m_pCharacter->GetPos();
		}
	}

	GameServer()->CreatePlayerSpawn(SpawnPos, m_pCharacter->TeamMask());

	// Freeze character on jail release
	if (JailRelease)
		m_pCharacter->Freeze(5);

	if (GameServer()->Config()->m_SvTeam == 3)
	{
		int NewTeam = 1;
		for (; NewTeam < TEAM_SUPER; NewTeam++)
			if (Controller->m_Teams.Count(NewTeam) == 0)
				break;

		if (NewTeam == TEAM_SUPER)
			NewTeam = 0;

		Controller->m_Teams.SetForceCharacterTeam(GetCID(), NewTeam);
		m_pCharacter->SetSolo(true);
	}
}

bool CPlayer::AfkTimer(int NewTargetX, int NewTargetY)
{
	/*
		afk timer (x, y = mouse coordinates)
		Since a player has to move the mouse to play, this is a better method than checking
		the player's position in the game world, because it can easily be bypassed by just locking a key.
		Frozen players could be kicked as well, because they can't move.
		It also works for spectators.
		returns true if kicked
	*/

	if (m_IsDummy)
		return false;
	if (Server()->GetAuthedState(m_ClientID))
		return false; // don't kick admins
	if (GameServer()->Config()->m_SvMaxAfkTime == 0)
		return false; // 0 = disabled

	if (NewTargetX != m_LastTarget_x || NewTargetY != m_LastTarget_y)
	{
		UpdatePlaytime();
		m_LastTarget_x = NewTargetX;
		m_LastTarget_y = NewTargetY;
		m_Sent1stAfkWarning = 0; // afk timer's 1st warning after 50% of sv_max_afk_time
		m_Sent2ndAfkWarning = 0;

	}
	else
	{
		if (!m_Paused)
		{
			// not playing, check how long
			if (m_Sent1stAfkWarning == 0 && m_LastPlaytime < time_get() - time_freq() * (int)(GameServer()->Config()->m_SvMaxAfkTime * 0.5))
			{
				str_format(m_pAfkMsg, sizeof(m_pAfkMsg),
					"You have been afk for %d seconds now. Please note that you get kicked after not playing for %d seconds.",
					(int)(GameServer()->Config()->m_SvMaxAfkTime * 0.5),
					GameServer()->Config()->m_SvMaxAfkTime
				);
				m_pGameServer->SendChatTarget(m_ClientID, m_pAfkMsg);
				m_Sent1stAfkWarning = 1;
			}
			else if (m_Sent2ndAfkWarning == 0 && m_LastPlaytime < time_get() - time_freq() * (int)(GameServer()->Config()->m_SvMaxAfkTime * 0.9))
			{
				str_format(m_pAfkMsg, sizeof(m_pAfkMsg),
					"You have been afk for %d seconds now. Please note that you get kicked after not playing for %d seconds.",
					(int)(GameServer()->Config()->m_SvMaxAfkTime * 0.9),
					GameServer()->Config()->m_SvMaxAfkTime
				);
				m_pGameServer->SendChatTarget(m_ClientID, m_pAfkMsg);
				m_Sent2ndAfkWarning = 1;
			}
			else if (m_LastPlaytime < time_get() - time_freq() * GameServer()->Config()->m_SvMaxAfkTime)
			{
				m_pGameServer->Server()->Kick(m_ClientID, "Away from keyboard");
				return true;
			}
		}
	}
	return false;
}

void CPlayer::UpdatePlaytime()
{
	m_LastPlaytime = time_get();
}

void CPlayer::AfkVoteTimer(CNetObj_PlayerInput* NewTarget)
{
	if (m_IsDummy)
		return;
	if (GameServer()->Config()->m_SvMaxAfkVoteTime == 0)
		return;

	if(!m_pLastTarget)
	{
		m_pLastTarget = new CNetObj_PlayerInput(*NewTarget);
		m_LastPlaytime = 0;
		m_Afk = true;
		return;
	}
	else if(mem_comp(NewTarget, m_pLastTarget, sizeof(CNetObj_PlayerInput)) != 0)
	{
		UpdatePlaytime();
		mem_copy(m_pLastTarget, NewTarget, sizeof(CNetObj_PlayerInput));
	}
	else if (m_LastPlaytime < time_get() - time_freq() * GameServer()->Config()->m_SvMaxAfkVoteTime)
	{
		OnSetAfk();
		m_Afk = true;
		return;
	}

	m_Afk = false;
}

void CPlayer::ProcessPause()
{
	if (m_ForcePauseTime && m_ForcePauseTime < Server()->Tick())
	{
		m_ForcePauseTime = 0;
		Pause(PAUSE_NONE, true);
	}

	if (m_Paused == PAUSE_SPEC && !m_pCharacter->IsPaused() && m_pCharacter->IsGrounded() && m_pCharacter->GetPos() == m_pCharacter->m_PrevPos)
	{
		m_pCharacter->Pause(true);
		GameServer()->CreateDeath(m_pCharacter->GetPos(), m_ClientID, m_pCharacter->TeamMask());
		GameServer()->CreateSound(m_pCharacter->GetPos(), SOUND_PLAYER_DIE, m_pCharacter->TeamMask());
	}
}

int CPlayer::Pause(int State, bool Force)
{
	if (State < PAUSE_NONE || State > PAUSE_SPEC) // Invalid pause state passed
		return 0;

	if (!m_pCharacter)
		return 0;

	if (m_TeeControllerID != -1)
	{
		GameServer()->SendChatTarget(m_ClientID, "You can't pause while you are controlled by someone else");
		return 0;
	}
	if (m_TeeControlMode && !m_pControlledTee)
	{
		GameServer()->SendChatTarget(m_ClientID, "You can't pause while you are selecting a tee to control");
		return 0;
	}
	if (GameServer()->Arenas()->IsConfiguring(m_ClientID))
		return 0;

	char aBuf[128];
	if (State != m_Paused)
	{
		// Get to wanted state
		switch (State) {
		case PAUSE_PAUSED:
		case PAUSE_NONE:
			if (m_pCharacter->IsPaused()) // First condition might be unnecessary
			{
				if (!Force && m_LastPause && m_LastPause + GameServer()->Config()->m_SvSpecFrequency * Server()->TickSpeed() > Server()->Tick())
				{
					GameServer()->SendChatTarget(m_ClientID, "Can't /spec that quickly.");
					return m_Paused; // Do not update state. Do not collect $200
				}
				m_pCharacter->Pause(false);
				m_ViewPos = m_pCharacter->GetPos();
				GameServer()->CreatePlayerSpawn(m_pCharacter->GetPos(), m_pCharacter->TeamMask());
			}
			// fall-thru
		case PAUSE_SPEC:
			if (GameServer()->Config()->m_SvPauseMessages)
			{
				str_format(aBuf, sizeof(aBuf), (State > PAUSE_NONE) ? "'%s' speced" : "'%s' resumed", Server()->ClientName(m_ClientID));
				GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
			}
			break;
		}

		// Update state
		m_Paused = State;
		m_LastPause = Server()->Tick();

		GameServer()->SendTeamChange(m_ClientID, !m_Paused && !m_TeeControlMode ? m_Team : TEAM_SPECTATORS, true, Server()->Tick(), m_ClientID);

		if (m_pCharacter)
		{
			if (m_Paused == PAUSE_NONE)
				m_pCharacter->SetTeeControlCursor();
			else
				m_pCharacter->RemoveTeeControlCursor();
		}

		if (m_Paused == PAUSE_NONE)
		{
			GameServer()->m_World.ResetSeeOthers(m_ClientID);
		}
	}

	return m_Paused;
}

int CPlayer::ForcePause(int Time)
{
	m_ForcePauseTime = Server()->Tick() + Server()->TickSpeed() * Time;

	if (GameServer()->Config()->m_SvPauseMessages)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "'%s' was force-paused for %ds", Server()->ClientName(m_ClientID), Time);
		GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
	}

	return Pause(PAUSE_SPEC, true);
}

int CPlayer::IsPaused()
{
	return m_ForcePauseTime ? m_ForcePauseTime : -1 * m_Paused;
}

bool CPlayer::IsPlaying()
{
	if (m_pCharacter && m_pCharacter->IsAlive())
		return true;
	return false;
}

void CPlayer::SpectatePlayerName(const char* pName)
{
	if (!pName)
		return;

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (i != m_ClientID && Server()->ClientIngame(i) && !str_comp(pName, Server()->ClientName(i)))
		{
			SetSpectatorID(SPEC_PLAYER, i);
			return;
		}
	}
}

// F-DDrace

void CPlayer::RainbowTick()
{
	// snapping happens every 2 ticks
	if (Server()->Tick() % 2 != 0)
		return;

	if (!m_InfRainbow && (!m_pCharacter || (!m_pCharacter->m_Rainbow && m_pCharacter->GetPowerHooked() != RAINBOW)))
		return;

	if (m_pCharacter && m_pCharacter->GetPowerHooked() == RAINBOW)
		m_pCharacter->m_IsRainbowHooked = true;

	m_RainbowColor = (m_RainbowColor + m_RainbowSpeed) % 256;

	CTeeInfo Info = m_CurrentInfo.m_TeeInfos;
	for (int p = 0; p < NUM_SKINPARTS; p++)
	{
		int BaseColor = m_RainbowColor * 0x010000;
		int Color = 0xff32;
		if (p == SKINPART_MARKING)
			Color *= -256;

		Info.m_aUseCustomColors[p] = 1;
		Info.m_aSkinPartColors[p] = BaseColor + Color;
		Info.m_Sevendown.m_UseCustomColor = 1;
		Info.m_Sevendown.m_ColorBody = Info.m_Sevendown.m_ColorFeet = BaseColor + Color;
	}

	// always update even if not sent to 0.7 clients yet and only updates 0.6 clients
	m_CurrentInfo.m_TeeInfos = Info;

	// 0.7 clients have heavy impact on rainbow, thats why they get a bit stopped here
	if (!m_pCharacter)
		return;

	// only send rainbow updates to people close to you, to reduce network traffic
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (GameServer()->m_apPlayers[i] && !m_pCharacter->NetworkClipped(i, false, true))
			GameServer()->SendSkinChange(Info, m_ClientID, i);
}

void CPlayer::KillMsgNoName(CNetMsg_Sv_KillMsg *pKillMsg)
{
	m_pKillMsgNoName = new CNetMsg_Sv_KillMsg(*pKillMsg);
	ShowNameShort();
}

void CPlayer::ShowNameShort()
{
	m_ShowNameShortTick = Server()->Tick() + Server()->TickSpeed() / 15;
}

int CPlayer::GetAccID()
{
	for (unsigned int i = ACC_START; i < GameServer()->m_Accounts.size(); i++)
		if (GameServer()->m_Accounts[i].m_ClientID == m_ClientID)
			return i;
	return 0;
}

void CPlayer::BankTransaction(int64 Amount, const char *pDescription, bool IsEuro)
{
	if (GetAccID() < ACC_START || Amount == 0)
		return;

	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[GetAccID()];

	if (IsEuro)
	{
		pAccount->m_Euros += Amount;

		char aDescription[256];
		str_format(aDescription, sizeof(aDescription), "%lld %s", Amount, pDescription);
		GameServer()->WriteDonationFile(TYPE_PURCHASE, Amount, GetAccID(), aDescription);
	}
	else
		pAccount->m_Money += Amount;

	ApplyMoneyHistoryMsg(TRANSACTION_BANK, Amount, pDescription);
}

void CPlayer::WalletTransaction(int64 Amount, const char *pDescription)
{
	if (Amount == 0)
		return;
	m_WalletMoney += Amount;
	ApplyMoneyHistoryMsg(TRANSACTION_WALLET, Amount, pDescription);
}

void CPlayer::ApplyMoneyHistoryMsg(int Type, int Amount, const char *pDescription)
{
	if (!pDescription[0] || GetAccID() < ACC_START)
		return;

	const char *pType = Type == TRANSACTION_BANK ? "BANK" : Type == TRANSACTION_WALLET ? "WALLET" : "UNKNOWN";
	char aDescription[256];
	str_format(aDescription, sizeof(aDescription), "[%s] %s%d %s", pType, Amount > 0 ? "+" : "", Amount, pDescription);

	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[GetAccID()];
	str_copy(pAccount->m_aLastMoneyTransaction[4], pAccount->m_aLastMoneyTransaction[3], sizeof(pAccount->m_aLastMoneyTransaction[4]));
	str_copy(pAccount->m_aLastMoneyTransaction[3], pAccount->m_aLastMoneyTransaction[2], sizeof(pAccount->m_aLastMoneyTransaction[3]));
	str_copy(pAccount->m_aLastMoneyTransaction[2], pAccount->m_aLastMoneyTransaction[1], sizeof(pAccount->m_aLastMoneyTransaction[2]));
	str_copy(pAccount->m_aLastMoneyTransaction[1], pAccount->m_aLastMoneyTransaction[0], sizeof(pAccount->m_aLastMoneyTransaction[1]));
	str_copy(pAccount->m_aLastMoneyTransaction[0], aDescription, sizeof(pAccount->m_aLastMoneyTransaction[0]));

	char aFilename[IO_MAX_PATH_LENGTH];
	char aBuf[1024];
	char aTimestamp[256];
	str_timestamp_format(aTimestamp, sizeof(aTimestamp), FORMAT_DATE);
	str_format(aFilename, sizeof(aFilename), "dumps/%s/money_%s.txt", GameServer()->Config()->m_SvMoneyHistoryFilePath, aTimestamp);
	str_timestamp_format(aTimestamp, sizeof(aTimestamp), FORMAT_SPACE);
	str_format(aBuf, sizeof(aBuf),
		"[%s][%s] account='%s' msg='%s%d %s' name='%s'",
		aTimestamp, pType,
		GameServer()->m_Accounts[GetAccID()].m_Username,
		Amount > 0 ? "+" : "", Amount, pDescription,
		Server()->ClientName(m_ClientID)
	);
	IOHANDLE File = GameServer()->Storage()->OpenFile(aFilename, IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(File)
	{
		io_write(File, aBuf, str_length(aBuf));
		io_write_newline(File);
		io_close(File);
	}
	else
	{
		dbg_msg("money", "error: failed to open '%s' for writing", aFilename);
	}
}

void CPlayer::GiveXP(int64 Amount, const char *pMessage)
{
	if (GetAccID() < ACC_START)
		return;

	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[GetAccID()];
	pAccount->m_XP += Amount;

	char aBuf[256];

	if (pMessage[0])
	{
		str_format(aBuf, sizeof(aBuf), "+%lld XP (%s)", Amount, pMessage);
		GameServer()->SendChatTarget(m_ClientID, aBuf);
	}

	if (pAccount->m_XP >= GameServer()->GetNeededXP(pAccount->m_Level))
	{
		pAccount->m_Level++;

		str_format(aBuf, sizeof(aBuf), "You are now level %d!", pAccount->m_Level);
		GameServer()->SendChatTarget(m_ClientID, aBuf);

		GameServer()->CreateFinishConfetti(m_pCharacter->GetPos(), m_pCharacter->TeamMask());
	}
}

void CPlayer::GiveBlockPoints(int Amount, int Victim)
{
	if (GetAccID() < ACC_START)
		return;

	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[GetAccID()];

	if (m_pCharacter && m_pCharacter->HasFlag() != -1)
		Amount += 1;

	pAccount->m_BlockPoints += Amount;

	// visually give the block point
	CCharacter *pVictim = GameServer()->GetPlayerChar(Victim);
	if (pVictim)
		new CFlyingPoint(&GameServer()->m_World, pVictim->GetPos(), m_ClientID, Victim, pVictim->Core()->m_Vel);
}

bool CPlayer::GiveTaserBattery(int Amount)
{
	if (GetAccID() < ACC_START || Amount == 0)
		return false;

	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[GetAccID()];
	if (pAccount->m_TaserLevel <= 0)
		return false;

	char Symbol;
	if (Amount > 0)
	{
		if (pAccount->m_TaserBattery >= MAX_TASER_BATTERY)
			return false;

		Amount = clamp(MAX_TASER_BATTERY-pAccount->m_TaserBattery, 0, Amount);
		Symbol = '+';
	}
	else
	{
		if (pAccount->m_TaserBattery <= 0 || pAccount->m_TaserBattery+Amount < 0)
			return false;
		Symbol = '-';
	}

	pAccount->m_TaserBattery += Amount;
	if (m_pCharacter)
	{
		char aBuf[16];
		str_format(aBuf, sizeof(aBuf), "%c%d", Symbol, abs(Amount));
		GameServer()->CreateLaserText(m_pCharacter->GetPos(), m_ClientID, aBuf, 3);

		if (m_pCharacter->GetActiveWeapon() == WEAPON_TASER)
			m_pCharacter->UpdateWeaponIndicator();
	}

	return true;
}

bool CPlayer::GivePortalBattery(int Amount)
{
	if (GetAccID() < ACC_START || Amount == 0)
		return false;

	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[GetAccID()];
	if (pAccount->m_PortalRifle) // disallow people who have bought portal rifle to pickup or drop any portal battery
		return false;

	char Symbol = '+';
	if (Amount < 0)
	{
		if (pAccount->m_PortalBattery <= 0 || pAccount->m_PortalBattery+Amount < 0)
			return false;
		Symbol = '-';
	}

	pAccount->m_PortalBattery += Amount;
	if (m_pCharacter)
	{
		char aBuf[16];
		str_format(aBuf, sizeof(aBuf), "%c%d", Symbol, abs(Amount));
		GameServer()->CreateLaserText(m_pCharacter->GetPos(), m_ClientID, aBuf, 3);

		if (m_pCharacter->GetActiveWeapon() == WEAPON_PORTAL_RIFLE)
			m_pCharacter->UpdateWeaponIndicator();
	}

	return true;
}

void CPlayer::OnLogin(bool ForceDesignLoad)
{
	GameServer()->SendChatTarget(m_ClientID, "Successfully logged in");

	ExpireItems();

	int AccID = GetAccID();
	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[AccID];
	if (m_pCharacter)
	{
		if (pAccount->m_VIP == VIP_PLUS)
			m_pCharacter->Core()->m_MoveRestrictionExtra.m_VipPlus = true;

		if (!IsMinigame() && !m_JailTime)
		{
			if (pAccount->m_PortalRifle)
				m_pCharacter->GiveWeapon(WEAPON_PORTAL_RIFLE, false, -1, true);
		}
	}

	if (pAccount->m_aContact[0] == '\0')
	{
		GameServer()->SendChatTarget(m_ClientID, "[WARNING] You did not set a contact, it can be used to recover your password or to get back the account after it got stolen.");
		GameServer()->SendChatTarget(m_ClientID, "Set a contact with '/contact <option>' to hide this message and for a free XP reward.");
	}

	if (pAccount->m_aSecurityPin[0] == '\0')
	{
		GameServer()->SendChatTarget(m_ClientID, "[WARNING] You did not set security pin yet. Check '/pin' for more information.");
	}

	if (pAccount->m_Flags&CGameContext::ACCFLAG_ZOOMCURSOR)
		m_ZoomCursor = true;
	if (pAccount->m_Flags&CGameContext::ACCFLAG_PLOTSPAWN)
		m_PlotSpawn = true;
	if (pAccount->m_Flags&CGameContext::ACCFLAG_SILENTFARM)
		m_SilentFarm = true;
	if (pAccount->m_Flags&CGameContext::ACCFLAG_HIDEDRAWINGS)
		m_HideDrawings = true;

	if (ForceDesignLoad)
		Server()->ChangeMapDesign(m_ClientID, GameServer()->GetCurrentDesignFromList(AccID));
	else
		StartVoteQuestion(CPlayer::VOTE_QUESTION_DESIGN);
}

void CPlayer::OnLogout()
{
	GameServer()->SendChatTarget(m_ClientID, "Successfully logged out");

	int AccID = GetAccID();
	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[AccID];
	if (m_pCharacter)
	{
		if (pAccount->m_VIP == VIP_PLUS)
			m_pCharacter->Core()->m_MoveRestrictionExtra.m_VipPlus = false;

		m_pCharacter->UnsetSpookyGhost();
		m_pCharacter->GiveWeapon(WEAPON_PORTAL_RIFLE, true, -1, true);
	}

	if (pAccount->m_VIP == VIP_PLUS)
		m_RainbowName = false;

	StopPlotEditing();
	CancelPlotAuction();
	CancelPlotSwap();

	if (m_TimeoutCode[0] != '\0')
		str_copy(pAccount->m_aTimeoutCode, m_TimeoutCode, sizeof(pAccount->m_aTimeoutCode));

	m_aSecurityPin[0] = '\0';

	pAccount->m_Flags = 0;
	if (m_ZoomCursor)
		pAccount->m_Flags |= CGameContext::ACCFLAG_ZOOMCURSOR;
	if (m_PlotSpawn)
		pAccount->m_Flags |= CGameContext::ACCFLAG_PLOTSPAWN;
	if (m_SilentFarm)
		pAccount->m_Flags |= CGameContext::ACCFLAG_SILENTFARM;
	if (m_HideDrawings)
		pAccount->m_Flags |= CGameContext::ACCFLAG_HIDEDRAWINGS;

	GameServer()->UpdateDesignList(AccID, Server()->GetMapDesign(m_ClientID));

	if (m_VoteQuestionType == CPlayer::VOTE_QUESTION_DESIGN)
		OnEndVoteQuestion();
}

void CPlayer::StartVoteQuestion(VoteQuestionType Type)
{
	char aText[128] = { 0 };
	switch ((int)Type)
	{
	case CPlayer::VOTE_QUESTION_DESIGN:
	{
		const char *pDesign = GameServer()->GetCurrentDesignFromList(GetAccID());
		if (!pDesign[0] || !str_comp(pDesign, Server()->GetMapDesign(m_ClientID)))
			return;

		str_format(aText, sizeof(aText), "Load recent design '%s'?", pDesign);
		break;
	}
	}

	const int TimeoutSec = 30;

	m_VoteQuestionRunning = true;
	m_VoteQuestionType = Type;
	m_VoteQuestionEndTick = Server()->Tick() + Server()->TickSpeed() * TimeoutSec;

	if (!Server()->IsSevendown(m_ClientID))
	{
		CNetMsg_Sv_VoteSet Msg;
		Msg.m_Type = VOTE_START_OP;
		Msg.m_Timeout = TimeoutSec;
		Msg.m_ClientID = VANILLA_MAX_CLIENTS-1;
		Msg.m_pDescription = aText;
		Msg.m_pReason = "";
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NOTRANSLATE, m_ClientID);
	}
	else
	{
		CMsgPacker Msg(NETMSGTYPE_SV_VOTESET);
		Msg.AddInt(TimeoutSec);
		Msg.AddString(aText, -1);
		Msg.AddString("", -1);
		Server()->SendMsg(&Msg, MSGFLAG_VITAL, m_ClientID);
	}
}

void CPlayer::OnEndVoteQuestion(int Result)
{
	switch ((int)m_VoteQuestionType)
	{
	case CPlayer::VOTE_QUESTION_DESIGN:
	{
		if (Result == 1)
		{
			Server()->ChangeMapDesign(m_ClientID, GameServer()->GetCurrentDesignFromList(GetAccID()));
		}
		break;
	}
	}

	m_VoteQuestionRunning = false;
	m_VoteQuestionType = CPlayer::VOTE_QUESTION_NONE;
	m_VoteQuestionEndTick = 0;
	//GameServer()->SendVoteStatus(m_ClientID, 2, Result == 1, Result == -1);

	if (!Server()->IsSevendown(m_ClientID))
	{
		CNetMsg_Sv_VoteSet Msg;
		Msg.m_Type = Result == 1 ? VOTE_END_PASS : Result == -1 ? VOTE_END_FAIL : VOTE_END_ABORT;
		Msg.m_Timeout = 0;
		int id = m_ClientID;
		Server()->Translate(id, m_ClientID);
		Msg.m_ClientID = id;
		Msg.m_pDescription = "";
		Msg.m_pReason = "";
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, m_ClientID);
	}
	else
	{
		CMsgPacker Msg(NETMSGTYPE_SV_VOTESET);
		Msg.AddInt(0);
		Msg.AddString("", -1);
		Msg.AddString("", -1);
		Server()->SendMsg(&Msg, MSGFLAG_VITAL, m_ClientID);
	}
}

void CPlayer::StopPlotEditing()
{
	if (IsMinigame() && m_SavedMinigameTee)
		m_MinigameTee.StopPlotEditing();

	if (m_pCharacter && m_pCharacter->m_DrawEditor.Active())
		m_pCharacter->GiveWeapon(WEAPON_DRAW_EDITOR, true);
}

void CPlayer::CancelPlotAuction()
{
	if (m_PlotAuctionPrice == 0)
		return;

	m_PlotAuctionPrice = 0;
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "The plot auction by '%s' is cancelled", Server()->ClientName(m_ClientID));
	GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
}

void CPlayer::CancelPlotSwap()
{
	if (m_aPlotSwapUsername[0] == 0)
		return;

	m_aPlotSwapUsername[0] = 0;
	GameServer()->SendChatTarget(m_ClientID, "Your plot swap offer got cancelled");
}

void CPlayer::ExpireItems()
{
	for (int i = 0; i < NUM_ITEMS_SHOP; i++)
	{
		if (IsExpiredItem(i))
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "[WARNING] Your %s expired", ((CShop *)GameServer()->m_pHouses[HOUSE_SHOP])->GetItemName(i));
			GameServer()->SendChatTarget(m_ClientID, aBuf);
		}
	}
}

void CPlayer::SetExpireDate(int Item)
{
	if (GetAccID() < ACC_START)
		return;

	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[GetAccID()];

	switch (Item)
	{
	case ITEM_VIP_PLUS:
	case ITEM_VIP:
			GameServer()->SetExpireDateDays(&pAccount->m_ExpireDateVIP, Item == ITEM_VIP_PLUS ? ITEM_EXPIRE_VIP_PLUS : ITEM_EXPIRE_VIP);
			break;
	case ITEM_PORTAL_RIFLE:
			GameServer()->SetExpireDateDays(&pAccount->m_ExpireDatePortalRifle, ITEM_EXPIRE_PORTAL_RIFLE);
			break;
	}
}

bool CPlayer::IsExpiredItem(int Item)
{
	if (GetAccID() < ACC_START)
		return false;

	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[GetAccID()];
	if ((Item == ITEM_VIP && pAccount->m_VIP == VIP_PLUS) || (Item == ITEM_VIP_PLUS && pAccount->m_VIP == VIP_CLASSIC))
		return false;

	int *pVariable;
	time_t *pDate;

	switch (Item)
	{
	case ITEM_VIP_PLUS:
	case ITEM_VIP: pVariable = &pAccount->m_VIP; pDate = &pAccount->m_ExpireDateVIP; break;
	case ITEM_PORTAL_RIFLE: pVariable = &pAccount->m_PortalRifle; pDate = &pAccount->m_ExpireDatePortalRifle; break;
	default: return false;
	}

	if (!*pVariable)
		return false;

	if (GameServer()->IsExpired(*pDate))
	{
		*pVariable = 0;
		*pDate = 0;
		return true;
	}
	return false;
}

void CPlayer::SetDummyMode(int Dummymode)
{
	if (m_DummyMode != Dummymode)
	{
		m_DummyMode = Dummymode;
		if (m_pCharacter)
			m_pCharacter->CreateDummyHandle(Dummymode);
	}
}

void CPlayer::SetPlaying()
{
	Pause(PAUSE_NONE, true);
	SetTeam(TEAM_RED);
	if (m_pCharacter && m_pCharacter->IsPaused())
		m_pCharacter->Pause(false);
}

void CPlayer::UpdateInformation(int ClientID)
{
	CNetMsg_Sv_ClientDrop ClientDropMsg;
	ClientDropMsg.m_ClientID = m_ClientID;
	ClientDropMsg.m_pReason = "";
	ClientDropMsg.m_Silent = 1;

	CNetMsg_Sv_ClientInfo NewClientInfoMsg;
	NewClientInfoMsg.m_ClientID = m_ClientID;
	NewClientInfoMsg.m_Local = 0;
	NewClientInfoMsg.m_Team = m_Team;
	NewClientInfoMsg.m_pName = m_CurrentInfo.m_aName;
	NewClientInfoMsg.m_pClan = m_CurrentInfo.m_aClan;
	NewClientInfoMsg.m_Country = Server()->ClientCountry(m_ClientID);
	NewClientInfoMsg.m_Silent = 1;

	for (int p = 0; p < NUM_SKINPARTS; p++)
	{
		NewClientInfoMsg.m_apSkinPartNames[p] = m_CurrentInfo.m_TeeInfos.m_aaSkinPartNames[p];
		NewClientInfoMsg.m_aUseCustomColors[p] = m_CurrentInfo.m_TeeInfos.m_aUseCustomColors[p];
		NewClientInfoMsg.m_aSkinPartColors[p] = m_CurrentInfo.m_TeeInfos.m_aSkinPartColors[p];
	}

	Server()->SendPackMsg(&ClientDropMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, ClientID);
	Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, ClientID);

	GameServer()->UpdateHidePlayers(m_ClientID);
}

void CPlayer::SetSkin(int Skin, bool Force)
{
	if (Skin <= SKIN_NONE || Skin >= NUM_SKINS)
		return;

	if (Force)
		m_ForcedSkin = Skin;

	if (m_SpookyGhost && Skin != SKIN_SPOOKY_GHOST)
		return;

	GameServer()->SendSkinChange(CTeeInfo(Skin), m_ClientID, -1);
}

void CPlayer::ResetSkin(bool Unforce)
{
	if (Unforce)
		m_ForcedSkin = SKIN_NONE;

	if (m_SpookyGhost)
		SetSkin(SKIN_SPOOKY_GHOST);
	else if (m_ForcedSkin != SKIN_NONE)
		SetSkin(m_ForcedSkin, true);
	else
	{
		// dont send skin updates if its not needed
		if (mem_comp(&m_CurrentInfo.m_TeeInfos, &m_TeeInfos, sizeof(CTeeInfo)) != 0)
			GameServer()->SendSkinChange(m_TeeInfos, m_ClientID, -1);
	}
}

void CPlayer::SetTeeControl(CPlayer *pVictim)
{
	if (!pVictim || pVictim->m_TeeControllerID != -1 || pVictim == this || pVictim == m_pControlledTee || m_IsDummy)
		return;

	if (m_pControlledTee)
	{
		UnsetTeeControl();
		SetTeeControl(pVictim);
		return;
	}

	m_pControlledTee = pVictim;
	m_pControlledTee->Pause(PAUSE_NONE, true);
	m_pControlledTee->m_TeeControllerID = m_ClientID;

	if (m_pControlledTee->m_pCharacter)
	{
		m_pControlledTee->m_pCharacter->ResetNumInputs();
		// update walk prediction for the controlled tee
		GameServer()->SendTuningParams(m_pControlledTee->GetCID(), m_pControlledTee->m_pCharacter->m_TuneZone);
	}

	if (m_pCharacter)
		m_pCharacter->SetTeeControlCursor();
}

void CPlayer::UnsetTeeControl()
{
	if (!m_pControlledTee)
		return;

	m_pControlledTee->m_TeeControllerID = -1;
	if (m_pControlledTee->m_pCharacter)
	{
		m_pControlledTee->m_pCharacter->ResetNumInputs();
		// update walk prediction for the controlled tee
		GameServer()->SendTuningParams(m_pControlledTee->GetCID(), m_pControlledTee->m_pCharacter->m_TuneZone);
	}
	m_pControlledTee = 0;

	if (m_pCharacter)
	{
		m_pCharacter->ResetNumInputs();
		m_pCharacter->RemoveTeeControlCursor();
	}
}

void CPlayer::ResumeFromTeeControl()
{
	if (!m_TeeControlMode)
		return;

	GameServer()->SendTeamChange(m_ClientID, m_Team, true, Server()->Tick(), m_ClientID);
	m_TeeControlMode = false;
	UnsetTeeControl();
}

bool CPlayer::CheckClanProtection()
{
	if (!GameServer()->Config()->m_SvClanProtection)
		return false;

	if (str_comp_nocase(Server()->ClientClan(m_ClientID), "Chilli.*") || !str_comp(m_TeeInfos.m_aaSkinPartNames[SKINPART_BODY], "greensward"))
	{
		if (m_ClanProtectionPunished)
			GameServer()->SendChatTarget(m_ClientID, "You got unfrozen by the clan protection.");

		m_ClanProtectionPunished = false;
		return false;
	}

	GameServer()->SendChatTarget(m_ClientID, "~~~ WARNING ~~~");
	GameServer()->SendChatTarget(m_ClientID, "You got frozen by the clan protection.");
	GameServer()->SendChatTarget(m_ClientID, "Remove your 'Chilli.*' clantag and reconnect, or set your skin body to 'greensward'.");

	m_ClanProtectionPunished = true;
	return true;
}

bool CPlayer::SilentFarmActive()
{
	return m_SilentFarm && m_pCharacter && m_pCharacter->m_MoneyTile && !m_Paused && m_Team != TEAM_SPECTATORS;
}

void CPlayer::OnSetAfk()
{
	// leave current minigame
	if (IsMinigame())
	{
		GameServer()->SetMinigame(m_ClientID, MINIGAME_NONE);
		GameServer()->SendChatTarget(m_ClientID, "You automatically left the minigame because you were afk for too long");
	}

	// exit plot editor
	if (m_pCharacter && m_pCharacter->m_DrawEditor.Active())
	{
		m_pCharacter->SetAvailableWeapon();
		GameServer()->SendChatTarget(m_ClientID, "You automatically exited the plot editor because you were afk for too long");
	}
}

void CPlayer::SaveMinigameTee()
{
	if (m_SavedMinigameTee || !m_pCharacter)
		return;

	m_pCharacter->UnsetSpookyGhost(); // unset spookyghost to avoid conflicts after loading again
	// Pretend we leave no bonus area so we can save the real values, and later override it by calling this function again
	if (m_pCharacter->m_NoBonusContext.m_InArea)
	{
		m_pCharacter->OnNoBonusArea(false, true);
		m_pCharacter->m_NoBonusContext.m_InArea = true;
	}
	m_MinigameTee.Save(m_pCharacter);
	m_SavedMinigameTee = true;
}

bool CPlayer::LoadMinigameTee()
{
	if (!CanLoadMinigameTee() || !m_pCharacter)
		return false;

	m_MinigameTee.Load(m_pCharacter, 0);
	m_pCharacter->Core()->m_Vel.y = -2.f; // avoid stacking in each other
	m_pCharacter->Freeze(3); // avoid too strong meta with backup tees in block areas

	m_SavedMinigameTee = false;
	return true;
}

bool CPlayer::CanLoadMinigameTee()
{
	return m_SavedMinigameTee && m_Minigame == m_MinigameTee.GetMinigame();
}

bool CPlayer::RequestMinigameChange(int RequestedMinigame)
{
	if (m_LastMinigameRequest && m_LastMinigameRequest > Server()->Tick() - Server()->TickSpeed() * 5)
		return true;

	if (RequestedMinigame == m_RequestedMinigame)
		return false; // only return false here, to actually join the minigame

	if (m_EscapeTime)
	{
		GameServer()->SendChatTarget(m_ClientID, "You can't join a minigame while being searched by the police");
		return true;
	}

	if (m_JailTime)
	{
		GameServer()->SendChatTarget(m_ClientID, "You can't join a minigame while being arrested");
		return true;
	}

	// if we dont have a character we can instantly join (e.g. spectator mode)
	if (!GetCharacter())
		return false;

	if (GetCharacter()->m_FreezeTime)
	{
		GameServer()->SendChatTarget(m_ClientID, "You can't join a minigame while being frozen");
	}
	else
	{
		m_RequestedMinigame = RequestedMinigame;
		m_LastMinigameRequest = Server()->Tick();
		GameServer()->SendChatTarget(m_ClientID, "Minigame request sent, please don't move for 5 seconds");
	}
	return true;
}

bool CPlayer::MinigameRequestTick()
{
	if (!m_LastMinigameRequest || m_RequestedMinigame == m_Minigame || !GetCharacter())
		return false;

	if (m_LastMinigameRequest < Server()->Tick() - Server()->TickSpeed() * 5)
	{
		GameServer()->SetMinigame(m_ClientID, m_RequestedMinigame);
		m_RequestedMinigame = MINIGAME_NONE;
		m_LastMinigameRequest = 0;
		return true;
	}
	else if (GetCharacter()->GetPos() != GetCharacter()->m_PrevPos)
	{
		GameServer()->SendChatTarget(m_ClientID, "Your minigame request was cancelled because you moved");
		m_RequestedMinigame = MINIGAME_NONE;
		m_LastMinigameRequest = 0;
	}
	else if ((Server()->Tick() - m_LastMinigameRequest - 1) % Server()->TickSpeed() == 0)
	{
		int Remaining = ((m_LastMinigameRequest + Server()->TickSpeed() * 5) - Server()->Tick()) / Server()->TickSpeed();
		char aBuf[4];
		str_format(aBuf, sizeof(aBuf), "%d", Remaining+1);
		GameServer()->CreateLaserText(GetCharacter()->GetPos(), m_ClientID, aBuf, 1);
	}

	return false;
}

void CPlayer::MinigameAfkCheck()
{
	if (!IsMinigame() || !GameServer()->Config()->m_SvMinigameAfkAutoLeave || m_IsDummy)
		return;

	int TimeLeft = ((m_LastMovementTick + Server()->TickSpeed() * GameServer()->Config()->m_SvMinigameAfkAutoLeave) - Server()->Tick()) / Server()->TickSpeed();
	if (TimeLeft <= 0)
	{
		GameServer()->SetMinigame(m_ClientID, MINIGAME_NONE);
		GameServer()->SendChatTarget(m_ClientID, "You automatically left the minigame because you were afk for too long");
	}
	else if (TimeLeft <= 10 && Server()->Tick() % Server()->TickSpeed() == 0)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Please move within %d seconds or you will leave the minigame", TimeLeft);
		GameServer()->SendChatTarget(m_ClientID, aBuf);
	}
}

bool CPlayer::ShowDDraceHud()
{
	if (!Server()->IsSevendown(m_ClientID) || GameServer()->GetClientDDNetVersion(m_ClientID) < VERSION_DDNET_NEW_HUD)
		return false;
	if (m_pCharacter && m_pCharacter->SendDroppedFlagCooldown(m_ClientID) != -1)
		return false;
	CPlayer *pPlayer = this;
	if ((m_Team == TEAM_SPECTATORS || m_Paused) && m_SpectatorID >= 0 && GameServer()->m_apPlayers[m_SpectatorID])
		pPlayer = GameServer()->m_apPlayers[m_SpectatorID];
	return !pPlayer->IsMinigame() || pPlayer->m_Minigame == MINIGAME_BLOCK || pPlayer->m_Minigame == MINIGAME_1VS1;
}
