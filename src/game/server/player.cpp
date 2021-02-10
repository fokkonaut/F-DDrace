/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entities/character.h"
#include "entities/flag.h"
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

CPlayer::CPlayer(CGameContext *pGameServer, int ClientID, bool DebugDummy, bool AsSpec)
{
	m_pGameServer = pGameServer;
	m_ClientID = ClientID;
	m_Team = AsSpec ? TEAM_SPECTATORS : GameServer()->m_pController->GetStartTeam(ClientID);
	m_DebugDummy = DebugDummy;
	Reset();
}

CPlayer::~CPlayer()
{
	delete m_pLastTarget;
	delete m_pCharacter;
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

	// pIdMap[0] = m_ClientID means that the id 0 of your fake map equals m_ClientID, which means you are always id 0 for yourself
	int *pIdMap = Server()->GetIdMap(m_ClientID);
	for (int i = 0;i < VANILLA_MAX_CLIENTS;i++)
	{
		pIdMap[i] = -1;
	}
	//pIdMap[0] = m_ClientID;

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
	m_ShowDistance = vec2(1000, 800);
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

	m_IsDummy = false;
	m_DummyMode = DUMMYMODE_IDLE;
	m_FakePing = 0;

	m_vWeaponLimit.resize(NUM_WEAPONS);

	m_Gamemode = GameServer()->Config()->m_SvVanillaModeStart ? GAMEMODE_VANILLA : GAMEMODE_DDRACE;

	m_FixNameID = -1;
	m_RemovedName = false;
	m_ShowName = true;

	m_SetRealName = false;
	m_SetRealNameTick = Now;

	m_ChatFix.m_Mode = CHAT_ALL;
	m_ChatFix.m_Target = -1;
	m_ChatFix.m_Message[0] = '\0';
	m_KillMsgFix.m_Killer = -1;
	m_KillMsgFix.m_Victim = -1;
	m_KillMsgFix.m_Weapon = -1;
	m_KillMsgFix.m_ModeSpecial = 0;

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

	m_ScoreMode = GameServer()->Config()->m_SvDefaultScoreMode;
	m_HasRoomKey = false;

	m_ForcedSkin = SKIN_NONE;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		m_HidePlayerTeam[i] = TEAM_RED;
		m_aSameIP[i] = false;
	}

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
	m_WalletMoney = 0;
	m_CheckedShutdownSaved = false;
	m_LastMoneyXPBomb = 0;
	m_LastVote = 0;
	m_aSecurityPin[0] = '\0';
	m_LocalChat = false;

	m_SpawnBlockScore = 0;
	m_EscapeTime = 0;
	m_JailTime = 0;

	m_SilentFarm = 0;
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
				if (!m_Paused && !m_pControlledTee)
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
			m_pCharacter->m_NumGhostShots = 0;

		// stop the spinning animation when chat is opened
		if (m_PlayerFlags&PLAYERFLAG_CHATTING)
			m_pCharacter->Core()->m_UpdateAngle = UPDATE_ANGLE_TIME;
	}

	// name
	if (!m_ShowName && !m_RemovedName)
	{
		SetName(" ");
		SetClan("");
		UpdateInformation();
		m_RemovedName = true;
	}
	if ((m_ShowName || m_SetRealName) && m_RemovedName)
	{
		SetName(Server()->ClientName(m_ClientID));
		SetClan(Server()->ClientClan(m_ClientID));
		UpdateInformation();
		m_RemovedName = false;
	}

	// fixing messages if name is hidden
	if (m_SetRealName)
	{
		if (m_SetRealNameTick < Server()->Tick())
		{
			if (m_FixNameID == FIX_CHAT_MSG)
				GameServer()->SendChat(m_ClientID, m_ChatFix.m_Mode, m_ChatFix.m_Target, m_ChatFix.m_Message, m_ClientID);
			else if (m_FixNameID == FIX_KILL_MSG)
			{
				CNetMsg_Sv_KillMsg Msg;
				Msg.m_Killer = m_KillMsgFix.m_Killer;
				Msg.m_Victim = GetCID();
				Msg.m_Weapon = m_KillMsgFix.m_Weapon;
				Msg.m_ModeSpecial = m_KillMsgFix.m_ModeSpecial;
				for (int i = 0; i < MAX_CLIENTS; i++)
					if (GameServer()->m_apPlayers[i] && (!GameServer()->Config()->m_SvHideMinigamePlayers || (m_Minigame == GameServer()->m_apPlayers[i]->m_Minigame)))
						Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
			}

			m_SetRealName = false;
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
			if (!m_pCharacter || !m_pCharacter->SendingPortalCooldown())
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Avoid policehammers for the next %lld seconds", m_EscapeTime / Server()->TickSpeed());
				GameServer()->SendBroadcast(aBuf, m_ClientID, false);
			}
		}
	}
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

void CPlayer::SendConnect(int ClientID, int FakeID)
{
	if (Server()->IsSevendown(m_ClientID))
		return;

	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if (!pPlayer)
		return;

	CNetMsg_Sv_ClientInfo NewClientInfoMsg;
	NewClientInfoMsg.m_ClientID = FakeID;
	NewClientInfoMsg.m_Local = 0;
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

	Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD|MSGFLAG_NO_TRANSLATE, m_ClientID);
}

void CPlayer::SendDisconnect(int ClientID, int FakeID)
{
	if (Server()->IsSevendown(m_ClientID))
		return;

	if (!GameServer()->m_apPlayers[ClientID])
		return;

	CNetMsg_Sv_ClientDrop ClientDropMsg;
	ClientDropMsg.m_ClientID = FakeID;
	ClientDropMsg.m_pReason = "";
	ClientDropMsg.m_Silent = 1;

	Server()->SendPackMsg(&ClientDropMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD|MSGFLAG_NO_TRANSLATE, m_ClientID);
}

void CPlayer::Snap(int SnappingClient)
{
	if(!IsDummy() && !Server()->ClientIngame(m_ClientID))
		return;

	int id = m_ClientID;
	if (SnappingClient > -1 && !Server()->Translate(id, SnappingClient))
		return;

	int Size = Server()->IsSevendown(SnappingClient) ? 5*4 : sizeof(CNetObj_PlayerInfo);
	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, id, Size));
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

	if (m_ClientID == SnappingClient && (m_Team == TEAM_SPECTATORS || m_Paused || m_TeeControlMode))
	{
		int Size = Server()->IsSevendown(m_ClientID) ? 3*4 : sizeof(CNetObj_SpectatorInfo);
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
			else if (m_TeeControlMode)
			{
				SpecMode = SPEC_PLAYER;
				SpectatorID = m_ClientID;
			}
		}

		if (SpecMode != SPEC_FREEVIEW && SpectatorID >= 0)
		{
			if (!Server()->Translate(SpectatorID, m_ClientID))
			{
				SpectatorID = m_ClientID;
				SpecMode = SPEC_PLAYER;
			}
		}

		if (Server()->IsSevendown(m_ClientID))
		{
			if (m_pSpecFlag)
			{
				((int*)pSpectatorInfo)[0] = SpectatorID != -1 ? SpectatorID : id;
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
			if (m_pSpecFlag)
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
		int *pClientInfo = (int*)Server()->SnapNewItem(11 + NUM_NETMSGTYPES, id, 17*4); // NETOBJTYPE_CLIENTINFO
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
		((int*)pPlayerInfo)[1] = id;
		((int*)pPlayerInfo)[2] = (m_Paused != PAUSE_PAUSED || m_ClientID != SnappingClient) && m_Paused < PAUSE_SPEC && (!m_TeeControlMode || m_pControlledTee) ? GetHidePlayerTeam(SnappingClient) : TEAM_SPECTATORS;
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

	CNetObj_DDNetPlayer *pDDNetPlayer = static_cast<CNetObj_DDNetPlayer *>(Server()->SnapNewItem(NETOBJTYPE_DDNETPLAYER, id, sizeof(CNetObj_DDNetPlayer)));
	if(!pDDNetPlayer)
		return;

	pDDNetPlayer->m_AuthLevel = GetAuthedHighlighted();
	pDDNetPlayer->m_Flags = 0;
	if(m_Afk)
		pDDNetPlayer->m_Flags |= EXPLAYERFLAG_AFK;
	if(m_Paused == PAUSE_SPEC)
		pDDNetPlayer->m_Flags |= EXPLAYERFLAG_SPEC;
	if(m_Paused == PAUSE_PAUSED)
		pDDNetPlayer->m_Flags |= EXPLAYERFLAG_PAUSED;

	bool ShowSpec = m_pCharacter && m_pCharacter->IsPaused();
	if(SnappingClient >= 0)
	{
		CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];
		ShowSpec = ShowSpec && (GameServer()->GetDDRaceTeam(id) == GameServer()->GetDDRaceTeam(SnappingClient) || pSnapPlayer->m_ShowOthers == 1 || (pSnapPlayer->GetTeam() == TEAM_SPECTATORS || pSnapPlayer->IsPaused()));
	}

	if(ShowSpec)
	{
		CNetObj_SpecChar *pSpecChar = static_cast<CNetObj_SpecChar *>(Server()->SnapNewItem(NETOBJTYPE_SPECCHAR, id, sizeof(CNetObj_SpecChar)));
		pSpecChar->m_X = m_pCharacter->Core()->m_Pos.x;
		pSpecChar->m_Y = m_pCharacter->Core()->m_Pos.y;
	}
}

void CPlayer::FakeSnap()
{
	if (!Server()->IsSevendown(m_ClientID))
		return;

	int FakeID = VANILLA_MAX_CLIENTS - 1;

	int *pClientInfo = (int*)Server()->SnapNewItem(11 + NUM_NETMSGTYPES, FakeID, 17*4); // NETOBJTYPE_CLIENTINFO
	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo[0], 4, " ");
	StrToInts(&pClientInfo[4], 3, "");
	StrToInts(&pClientInfo[8], 6, "default");

	if (!GameServer()->FlagsUsed())
		return;

	for (int i = 0; i < 2; i++)
	{
		FakeID--;
		pClientInfo = (int*)Server()->SnapNewItem(11 + NUM_NETMSGTYPES, FakeID, 17*4); // NETOBJTYPE_CLIENTINFO
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
		((int*)pPlayerInfo)[3] = m_ScoreMode == SCORE_TIME ? -9999 : 0;
		((int*)pPlayerInfo)[4] = 0;
	}
}

void CPlayer::SetFakeID()
{
	int FakeID = 0;
	NETADDR Addr, Addr2;
	Server()->GetClientAddr(m_ClientID, &Addr);
	while (1)
	{
		bool Break = true;
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (!GameServer()->m_apPlayers[i] || GameServer()->m_apPlayers[i]->m_IsDummy || i == m_ClientID)
				continue;

			Server()->GetClientAddr(i, &Addr2);
			if (net_addr_comp(&Addr, &Addr2, false) == 0)
			{
				if (GameServer()->m_apPlayers[i]->m_FakeID == FakeID)
				{
					FakeID++;
					Break = false;
				}
				m_aSameIP[i] = true;
				GameServer()->m_apPlayers[i]->m_aSameIP[m_ClientID] = true;
			}
		}

		if (Break)
			break;
	}
	m_FakeID = FakeID;

	int *pIdMap = Server()->GetIdMap(m_ClientID);
	pIdMap[m_FakeID] = m_ClientID;
}

int CPlayer::GetHidePlayerTeam(int Asker)
{
	CPlayer *pAsker = GameServer()->m_apPlayers[Asker];
	if (m_TeeControllerID != Asker && m_Team != TEAM_SPECTATORS && ((GameServer()->Config()->m_SvHideDummies && m_IsDummy)
		|| (GameServer()->Config()->m_SvHideMinigamePlayers && pAsker->m_Minigame != m_Minigame)))
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
	// Make sure to call this before the character dies because on disconnect it should drop the money even when frozen
	if (GameServer()->Config()->m_SvDropsOnDeath && m_pCharacter)
		m_pCharacter->DropMoney(GetWalletMoney());
	KillCharacter();

	GameServer()->SavePlayer(m_ClientID);
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
			GameServer()->m_apPlayers[i]->m_aSameIP[m_ClientID] = false;

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

	if (((!m_pCharacter && m_Team == TEAM_SPECTATORS) || m_Paused) && m_SpecMode == SPEC_FREEVIEW)
		m_ViewPos = vec2(NewInput->m_TargetX, NewInput->m_TargetY);

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

	if (m_pCharacter)
	{
		if (!m_Paused && (!m_TeeControlMode || TeeControlled))
			m_pCharacter->OnDirectInput(NewInput);
		else
		{
			m_pCharacter->ResetInput();
			m_pCharacter->ResetNumInputs();

			if (m_pControlledTee && m_pControlledTee->m_pCharacter)
				m_pControlledTee->m_pCharacter->ResetNumInputs();
		}
	}

	if(!m_pCharacter && m_Team != TEAM_SPECTATORS && (NewInput->m_Fire&1))
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

				CCharacter *pChar = (CCharacter *)GameServer()->m_World.ClosestEntity(m_ViewPos, 6.0f*32, CGameWorld::ENTTYPE_CHARACTER, pNotThis);
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
	if(NewInput->m_Direction || m_LatestActivity.m_TargetX != NewInput->m_TargetX ||
		m_LatestActivity.m_TargetY != NewInput->m_TargetY || NewInput->m_Jump ||
		NewInput->m_Fire&1 || NewInput->m_Hook)
	{
		m_LatestActivity.m_TargetX = NewInput->m_TargetX;
		m_LatestActivity.m_TargetY = NewInput->m_TargetY;
		m_LastActionTick = Server()->Tick();
	}
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
				return true;
			}
			m_pSpecFlag = 0;
			m_SpecMode = SpecMode;
			m_SpectatorID = SpectatorID;
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
	else if (m_PlotSpawn)
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

	m_WeakHookSpawn = false;
	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_pCharacter->Spawn(this, SpawnPos);
	GameServer()->CreatePlayerSpawn(SpawnPos, m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));

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
		GameServer()->CreateDeath(m_pCharacter->GetPos(), m_ClientID, m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));
		GameServer()->CreateSound(m_pCharacter->GetPos(), SOUND_PLAYER_DIE, m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));
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
				GameServer()->CreatePlayerSpawn(m_pCharacter->GetPos(), m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));
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
	bool IsRainbowHooked = IsHooked(RAINBOW);
	if (Server()->Tick() % 6 != 0 || (!m_InfRainbow && !IsRainbowHooked && !(m_pCharacter && m_pCharacter->m_Rainbow)))
		return;

	if (m_pCharacter && IsRainbowHooked)
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

	// only send rainbow updates to people close to you, to reduce network traffic
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (GameServer()->m_apPlayers[i] && m_pCharacter && !((CEntity *)m_pCharacter)->NetworkClipped(i))
			GameServer()->SendSkinChange(Info, m_ClientID, i);
}

void CPlayer::FixForNoName(int ID)
{
	m_FixNameID = ID;
	m_SetRealName = true;
	m_SetRealNameTick = Server()->Tick() + Server()->TickSpeed() / 20;
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
	if (GetAccID() < ACC_START)
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
	}
}

void CPlayer::GiveBlockPoints(int Amount)
{
	if (GetAccID() < ACC_START)
		return;

	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[GetAccID()];

	if (m_pCharacter && m_pCharacter->HasFlag() != -1)
		Amount += 1;

	pAccount->m_BlockPoints += Amount;
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
		if (pAccount->m_TaserBattery <= 0)
			return false;

		if (pAccount->m_TaserBattery+Amount < 0)
			Amount -= abs(Amount)-pAccount->m_TaserBattery;
		Symbol = '-';
	}

	if (m_pCharacter)
	{
		if (m_pCharacter->GetWeaponGot(WEAPON_TASER))
		{
			int Ammo = m_pCharacter->GetWeaponAmmo(WEAPON_TASER) + Amount;
			m_pCharacter->SetWeaponAmmo(WEAPON_TASER, Ammo);
		}

		char aBuf[16];
		str_format(aBuf, sizeof(aBuf), "%c%d", Symbol, abs(Amount));
		GameServer()->CreateLaserText(m_pCharacter->GetPos(), m_ClientID, aBuf, 3);
	}

	pAccount->m_TaserBattery += Amount;
	return true;
}

void CPlayer::OnLogin()
{
	GameServer()->SendChatTarget(m_ClientID, "Successfully logged in");

	ExpireItems();

	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[GetAccID()];
	if (!IsMinigame() && !m_JailTime && m_pCharacter)
	{
		m_pCharacter->GiveWeapon(WEAPON_TASER, false, pAccount->m_TaserBattery);

		if (pAccount->m_PortalRifle)
			m_pCharacter->GiveWeapon(WEAPON_PORTAL_RIFLE, false, -1, true);
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
}

void CPlayer::OnLogout()
{
	GameServer()->SendChatTarget(m_ClientID, "Successfully logged out");

	if (m_pCharacter)
	{
		m_pCharacter->UnsetSpookyGhost();
		m_pCharacter->GiveWeapon(WEAPON_TASER, true);
		m_pCharacter->GiveWeapon(WEAPON_PORTAL_RIFLE, true, -1, true);
	}

	StopPlotEditing();
	CancelPlotAuction();
	CancelPlotSwap();

	if (m_TimeoutCode[0] != '\0')
		str_copy(GameServer()->m_Accounts[GetAccID()].m_aTimeoutCode, m_TimeoutCode, sizeof(GameServer()->m_Accounts[GetAccID()].m_aTimeoutCode));

	m_aSecurityPin[0] = '\0';
}

void CPlayer::StopPlotEditing()
{
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
	GameServer()->SendChat(-1, CHAT_ALL, -1, "Your plot swap offer got cancelled");
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
	case ITEM_VIP:
			GameServer()->SetExpireDate(&pAccount->m_ExpireDateVIP, ITEM_EXPIRE_VIP);
			break;
	case ITEM_PORTAL_RIFLE:
			GameServer()->SetExpireDate(&pAccount->m_ExpireDatePortalRifle, ITEM_EXPIRE_PORTAL_RIFLE);
			break;
	}
}

bool CPlayer::IsExpiredItem(int Item)
{
	if (GetAccID() < ACC_START)
		return false;

	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[GetAccID()];
	time_t tmp;

	switch (Item)
	{
	case ITEM_VIP:
		{
			if (!pAccount->m_VIP)
				return false;

			tmp = pAccount->m_ExpireDateVIP;
			break;
		}
	case ITEM_PORTAL_RIFLE:
		{
			if (!pAccount->m_PortalRifle)
				return false;

			tmp = pAccount->m_ExpireDatePortalRifle;
			break;
		}
	default:
		return false;
	}

	if (GameServer()->IsExpired(tmp))
	{
		switch (Item)
		{
		case ITEM_VIP:
			{
				pAccount->m_VIP = false;
				pAccount->m_ExpireDateVIP = 0;
				break;
			}
		case ITEM_PORTAL_RIFLE:
			{
				pAccount->m_PortalRifle = false;
				pAccount->m_ExpireDatePortalRifle = 0;
				break;
			}
		}
		return true;
	}
	return false;
}

bool CPlayer::IsHooked(int Power)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter* pChr = GameServer()->GetPlayerChar(i);
		if (!pChr)
			continue;

		if (pChr->Core()->m_HookedPlayer == m_ClientID)
			return Power >= 0 ? pChr->m_HookPower == Power : true;
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
	m_MinigameTee.Save(m_pCharacter);
	m_SavedMinigameTee = true;
}

bool CPlayer::LoadMinigameTee()
{
	if (!CanLoadMinigameTee() || !m_pCharacter)
		return false;

	m_MinigameTee.Load(m_pCharacter, 0);
	m_pCharacter->Core()->m_Vel.y = -2.f; // avoid stacking in each other
	m_pCharacter->Freeze(); // avoid too strong meta with backup tees in block areas

	m_SavedMinigameTee = false;
	return true;
}

bool CPlayer::CanLoadMinigameTee()
{
	return m_SavedMinigameTee && m_Minigame == m_MinigameTee.GetMinigame();
}
