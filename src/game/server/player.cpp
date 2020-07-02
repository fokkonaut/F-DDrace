/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entities/character.h"
#include "entities/flag.h"
#include "gamecontext.h"
#include "gamecontroller.h"
#include "player.h"
#include <game/server/gamemodes/DDRace.h>
#include <engine/shared/config.h>
#include "score.h"


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
	m_LastSetSpectatorMode = 0;

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

	m_DDraceVersion = VERSION_VANILLA;
	m_ShowOthers = GameServer()->Config()->m_SvShowOthersDefault;
	m_ShowAll = GameServer()->Config()->m_SvShowAllDefault;
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
	m_Dummymode = DUMMYMODE_IDLE;
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

	m_Minigame = MINIGAME_NONE;
	m_SurvivalState = SURVIVAL_OFFLINE;

	m_SpookyGhost = false;
	m_HasSpookyGhost = false;

	m_ScoreMode = GameServer()->Config()->m_SvDefaultScoreMode;
	m_HasRoomKey = false;

	m_TeeInfos.m_ForcedSkin = SKIN_NONE;
	m_TeeInfos.m_aSkinName[0] = '\0';

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
			GameServer()->m_pShop->ResetMotdTick(m_ClientID);
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
		SetClan(Server()->ClientName(m_ClientID));
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
	if (Server()->Tick() % 2 == 0 && (m_InfRainbow || IsHooked(RAINBOW) || (m_pCharacter && m_pCharacter->m_Rainbow)) && GameServer()->Config()->m_SvAllowRainbow)
	{
		m_RainbowColor = (m_RainbowColor + m_RainbowSpeed) % 256;

		TeeInfos pTeeInfos;
		for (int p = 0; p < NUM_SKINPARTS; p++)
		{
			int BaseColor = m_RainbowColor * 0x010000;
			int Color = 0xff32;
			if (p == SKINPART_MARKING)
				Color *= -256;
			str_copy(pTeeInfos.m_aaSkinPartNames[p], m_CurrentInfo.m_TeeInfos.m_aaSkinPartNames[p], 24);
			pTeeInfos.m_aUseCustomColors[p] = 1;
			pTeeInfos.m_aSkinPartColors[p] = BaseColor + Color;
		}

		GameServer()->SendSkinChange(pTeeInfos, m_ClientID, -1);
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

	int Team = pPlayer->m_Team;
	if (Team != TEAM_SPECTATORS && ((GameServer()->Config()->m_SvHideDummies && pPlayer->m_IsDummy)
		|| (GameServer()->Config()->m_SvHideMinigamePlayers && m_Minigame != pPlayer->m_Minigame)))
		Team = TEAM_BLUE;

	CNetMsg_Sv_ClientInfo NewClientInfoMsg;
	NewClientInfoMsg.m_ClientID = FakeID;
	NewClientInfoMsg.m_Local = 0;
	NewClientInfoMsg.m_Team = Team;
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
			((int*)pSpectatorInfo)[0] = SpectatorID;
			((int*)pSpectatorInfo)[1] = m_ViewPos.x;
			((int*)pSpectatorInfo)[2] = m_ViewPos.y;
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
		int *pClientInfo = (int*)Server()->SnapNewItem(11 + 24, id, 17*4); // NETOBJTYPE_CLIENTINFO
		if(!pClientInfo)
			return;

		StrToInts(&pClientInfo[0], 4, m_CurrentInfo.m_aName);
		StrToInts(&pClientInfo[4], 3, m_CurrentInfo.m_aClan);
		pClientInfo[7] = Server()->ClientCountry(m_ClientID);
		StrToInts(&pClientInfo[8], 6, m_CurrentInfo.m_TeeInfos.m_Sevendown.m_SkinName);
		pClientInfo[14] = m_CurrentInfo.m_TeeInfos.m_Sevendown.m_UseCustomColor;
		pClientInfo[15] = m_CurrentInfo.m_TeeInfos.m_Sevendown.m_ColorBody;
		pClientInfo[16] = m_CurrentInfo.m_TeeInfos.m_Sevendown.m_ColorFeet;

		int Team = m_Team;
		if (Team != TEAM_SPECTATORS && ((GameServer()->Config()->m_SvHideDummies && m_IsDummy)
			|| (GameServer()->Config()->m_SvHideMinigamePlayers && pSnapping->m_Minigame != m_Minigame)))
			Team = TEAM_BLUE;

		((int*)pPlayerInfo)[0] = (int)(m_ClientID == SnappingClient);
		((int*)pPlayerInfo)[1] = id;
		((int*)pPlayerInfo)[2] = (m_Paused != PAUSE_PAUSED || m_ClientID != SnappingClient) && m_Paused < PAUSE_SPEC && !m_TeeControlMode ? Team : TEAM_SPECTATORS;
		((int*)pPlayerInfo)[3] = Score;
		((int*)pPlayerInfo)[4] = Latency;
	}
	else
	{
		pPlayerInfo->m_PlayerFlags = m_PlayerFlags&PLAYERFLAG_CHATTING;
		if(Server()->GetAuthedState(m_ClientID) && GameServer()->Config()->m_SvAuthedHighlighted)
			pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_ADMIN;
		if(m_IsReadyToPlay)
			pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_READY;
		if(m_Paused)
			pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_DEAD;
		if(SnappingClient != -1 && (m_Team == TEAM_SPECTATORS || m_Paused) && (SnappingClient == m_SpectatorID))
			pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_WATCHING;
		if (m_IsDummy && GameServer()->Config()->m_SvDummyBotSkin)
			pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_BOT;

		pPlayerInfo->m_Latency = Latency;
		pPlayerInfo->m_Score = Score;

		CNetObj_ExPlayerInfo *pExPlayerInfo = static_cast<CNetObj_ExPlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_EXPLAYERINFO, id, sizeof(CNetObj_ExPlayerInfo)));
		if(!pExPlayerInfo)
			return;

		pExPlayerInfo->m_Flags = 0;
		if (m_Aim)
			pExPlayerInfo->m_Flags |= EXPLAYERFLAG_AIM;
		if(m_Afk)
			pExPlayerInfo->m_Flags |= EXPLAYERFLAG_AFK;
	}
}

void CPlayer::FakeSnap()
{
	if (Server()->IsSevendown(m_ClientID))
	{
		int FakeID = VANILLA_MAX_CLIENTS - 1;

		int *pClientInfo = (int*)Server()->SnapNewItem(11 + 24, FakeID, 17*4); // NETOBJTYPE_CLIENTINFO
		if(!pClientInfo)
			return;

		StrToInts(&pClientInfo[0], 4, " ");
		StrToInts(&pClientInfo[4], 3, "");
		StrToInts(&pClientInfo[8], 6, "default");
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
			if (net_addr_comp_noport(&Addr, &Addr2) == 0)
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

void CPlayer::OnDisconnect()
{
	KillCharacter();

	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
	Controller->m_Teams.SetForceCharacterTeam(m_ClientID, 0);

	if (GetAccID() >= ACC_START)
		GameServer()->Logout(GetAccID());

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

	m_Aim = NewInput->m_PlayerFlags&16;

	int PlayerFlags = 0;
	if (NewInput->m_PlayerFlags&4) PlayerFlags |= PLAYERFLAG_CHATTING;
	if (NewInput->m_PlayerFlags&8) PlayerFlags |= PLAYERFLAG_SCOREBOARD;
	NewInput->m_PlayerFlags = PlayerFlags;
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *NewInput, bool TeeControlled)
{
	// F-DDrace
	if (m_pControlledTee && !m_Paused && !TeeControlled)
	{
		m_pControlledTee->OnPredictedInput(NewInput, true);
		return;
	}
	else if (m_TeeControllerID != -1 && !TeeControlled)
		return;

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
		m_pControlledTee->OnDirectInput(NewInput, true);
		return;
	}
	else if (m_TeeControllerID != -1 && !TeeControlled)
		return;

	if (AfkTimer(NewInput->m_TargetX, NewInput->m_TargetY))
		return; // we must return if kicked, as player struct is already deleted
	AfkVoteTimer(NewInput);

	TranslatePlayerFlags(NewInput);

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

	vec2 SpawnPos;

	int Index = ENTITY_SPAWN;

	if (m_ForceSpawnPos != vec2(-1, -1))
	{
		SpawnPos = m_ForceSpawnPos;
	}
	else if (m_Dummymode == DUMMYMODE_SHOP_DUMMY)
	{
		if (GameServer()->Collision()->GetRandomTile(ENTITY_SHOP_DUMMY_SPAWN) != vec2(-1, -1))
			Index = ENTITY_SHOP_DUMMY_SPAWN;
		else
			Index = TILE_SHOP;
	}
	else if (m_Minigame == MINIGAME_BLOCK || m_Dummymode == DUMMYMODE_V3_BLOCKER)
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

	if (GameServer()->Collision()->GetRandomTile(Index) == vec2(-1, -1))
		Index = ENTITY_SPAWN;

	if (m_ForceSpawnPos == vec2(-1, -1) && !GameServer()->m_pController->CanSpawn(&SpawnPos, Index))
		return;

	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;

	m_WeakHookSpawn = false;
	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_pCharacter->Spawn(this, SpawnPos);
	GameServer()->CreatePlayerSpawn(SpawnPos, m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));

	if (GameServer()->Config()->m_SvTeam == 3)
	{
		int NewTeam = 0;
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
		m_LastPlaytime = time_get();
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
		m_LastPlaytime = time_get();
		mem_copy(m_pLastTarget, NewTarget, sizeof(CNetObj_PlayerInput));
	}
	else if (m_LastPlaytime < time_get() - time_freq() * GameServer()->Config()->m_SvMaxAfkVoteTime)
	{
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

void CPlayer::MoneyTransaction(int Amount, const char *pDescription, bool IsEuro)
{
	if (GetAccID() < ACC_START)
		return;

	CGameContext::AccountInfo *Account = &GameServer()->m_Accounts[GetAccID()];

	if (IsEuro)
	{
		(*Account).m_Euros += Amount;
		GameServer()->WriteDonationFile(TYPE_PURCHASE, Amount, GetAccID(), pDescription);
	}
	else
		(*Account).m_Money += Amount;

	if (!pDescription[0])
		return;

	str_copy((*Account).m_aLastMoneyTransaction[4], (*Account).m_aLastMoneyTransaction[3], sizeof((*Account).m_aLastMoneyTransaction[4]));
	str_copy((*Account).m_aLastMoneyTransaction[3], (*Account).m_aLastMoneyTransaction[2], sizeof((*Account).m_aLastMoneyTransaction[3]));
	str_copy((*Account).m_aLastMoneyTransaction[2], (*Account).m_aLastMoneyTransaction[1], sizeof((*Account).m_aLastMoneyTransaction[2]));
	str_copy((*Account).m_aLastMoneyTransaction[1], (*Account).m_aLastMoneyTransaction[0], sizeof((*Account).m_aLastMoneyTransaction[1]));
	str_copy((*Account).m_aLastMoneyTransaction[0], pDescription, sizeof((*Account).m_aLastMoneyTransaction[0]));
}

void CPlayer::GiveXP(int Amount, const char *pMessage)
{
	if (GetAccID() < ACC_START)
		return;

	CGameContext::AccountInfo* Account = &GameServer()->m_Accounts[GetAccID()];
	if ((*Account).m_Level >= MAX_LEVEL)
		return;

	char aBuf[256];

	while ((*Account).m_XP + Amount > GameServer()->m_aNeededXP[MAX_LEVEL])
		Amount--;
	(*Account).m_XP += Amount;

	if (pMessage[0])
	{
		str_format(aBuf, sizeof(aBuf), "+%d XP (%s)", Amount, pMessage);
		GameServer()->SendChatTarget(m_ClientID, aBuf);
	}

	if ((*Account).m_XP >= GameServer()->m_aNeededXP[(*Account).m_Level])
	{
		(*Account).m_Level++;

		str_format(aBuf, sizeof(aBuf), "You are now Level %d!", (*Account).m_Level);
		GameServer()->SendChatTarget(m_ClientID, aBuf);
	}
}

void CPlayer::GiveBlockPoints(int Amount)
{
	CGameContext::AccountInfo* Account = &GameServer()->m_Accounts[GetAccID()];

	if (m_pCharacter && m_pCharacter->HasFlag() != -1)
		Amount += 1;

	(*Account).m_BlockPoints += Amount;
}

void CPlayer::OnLogin()
{
	CGameContext::AccountInfo* Account = &GameServer()->m_Accounts[GetAccID()];
	if (m_pCharacter)
	{
		if (m_pCharacter->GetWeaponGot(WEAPON_LASER) && (*Account).m_TaserLevel >= 1)
			m_pCharacter->GiveWeapon(WEAPON_TASER, false, m_pCharacter->GetWeaponAmmo(WEAPON_LASER));

		if ((*Account).m_PortalRifle)
			m_pCharacter->GiveWeapon(WEAPON_PORTAL_RIFLE);
	}

	// has vip from the old system, remove it and give him 5 euros
	if ((*Account).m_VIP && (*Account).m_ExpireDateVIP == 0)
	{
		(*Account).m_VIP = 0;
		MoneyTransaction(5, "Had VIP from the old system", true);
		GameServer()->SendChatTarget(m_ClientID, "[WARNING] Due to an update your VIP was removed. You got 5 Euros back, saved in your account. Go to the shop and buy VIP again.");
	}

	for (int i = 0; i < NUM_ITEMS; i++)
	{
		if (IsExpiredItem(i))
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "[WARNING] Your %s expired", GameServer()->m_pShop->GetItemName(i));
			GameServer()->SendChatTarget(m_ClientID, aBuf);
		}
	}
}

void CPlayer::OnLogout()
{
	if (m_pCharacter)
	{
		if (m_pCharacter->GetWeaponGot(WEAPON_TASER))
			m_pCharacter->GiveWeapon(WEAPON_TASER, true);

		if (m_pCharacter->GetWeaponGot(WEAPON_PORTAL_RIFLE))
			m_pCharacter->GiveWeapon(WEAPON_PORTAL_RIFLE, true);

		m_pCharacter->UnsetSpookyGhost();
	}
}

void CPlayer::SetExpireDate(int Item)
{
	if (GetAccID() < ACC_START)
		return;

	CGameContext::AccountInfo* Account = &GameServer()->m_Accounts[GetAccID()];

	time_t Now, tmp;
	struct tm ExpireDate;
	time(&Now);
	ExpireDate = *localtime(&Now);

	int Days;
	switch (Item)
	{
	case ITEM_VIP:
		{
			tmp = (*Account).m_ExpireDateVIP;
			Days = ITEM_EXPIRE_VIP;
			break;
		}
	case ITEM_PORTAL_RIFLE:
		{
			tmp = (*Account).m_ExpireDatePortalRifle;
			Days = ITEM_EXPIRE_PORTAL_RIFLE;
			break;
		}
	default:
		return;
	}

	// add another x days if we have the item already
	if (tmp != 0)
	{
		struct tm AccDate;
		AccDate = *localtime(&tmp);

		ExpireDate.tm_year = AccDate.tm_year;
		ExpireDate.tm_mon = AccDate.tm_mon;
		ExpireDate.tm_mday = AccDate.tm_mday;
	}

	const time_t ONE_DAY = 24 * 60 * 60;
	time_t DateSeconds = mktime(&ExpireDate) + (Days * ONE_DAY);
	ExpireDate = *localtime(&DateSeconds);

	switch (Item)
	{
	case ITEM_VIP: (*Account).m_ExpireDateVIP = mktime(&ExpireDate); break;
	case ITEM_PORTAL_RIFLE: (*Account).m_ExpireDatePortalRifle = mktime(&ExpireDate); break;
	}
}

bool CPlayer::IsExpiredItem(int Item)
{
	if (GetAccID() < ACC_START)
		return false;

	CGameContext::AccountInfo* Account = &GameServer()->m_Accounts[GetAccID()];
	time_t tmp;

	switch (Item)
	{
	case ITEM_VIP:
		{
			if (!(*Account).m_VIP)
				return false;

			tmp = (*Account).m_ExpireDateVIP;
			break;
		}
	case ITEM_PORTAL_RIFLE:
		{
			if (!(*Account).m_PortalRifle)
				return false;

			tmp = (*Account).m_ExpireDatePortalRifle;
			break;
		}
	default:
		return false;
	}

	struct tm AccDate;
	AccDate = *localtime(&tmp);

	time_t Now;
	struct tm ExpireDate;
	time(&Now);
	ExpireDate = *localtime(&Now);

	ExpireDate.tm_year = AccDate.tm_year;
	ExpireDate.tm_mon = AccDate.tm_mon;
	ExpireDate.tm_mday = AccDate.tm_mday;

	double Seconds = difftime(Now, mktime(&ExpireDate));
	const time_t ONE_DAY = 24 * 60 * 60;
	int Days = Seconds / ONE_DAY;

	if (Days >= 0)
	{
		switch (Item)
		{
		case ITEM_VIP:
			{
				(*Account).m_VIP = false;
				(*Account).m_ExpireDateVIP = 0;
				break;
			}
		case ITEM_PORTAL_RIFLE:
			{
				(*Account).m_PortalRifle = false;
				(*Account).m_ExpireDatePortalRifle = 0;
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
	if (Skin < 0 || Skin >= NUM_SKINS)
		return;

	if (Force)
		m_TeeInfos.m_ForcedSkin = Skin;

	if (m_SpookyGhost)
		return;

	GameServer()->SendSkinChange(GameServer()->m_Skins.GetSkin(Skin), m_ClientID, -1);
}

void CPlayer::ResetSkin(bool Unforce)
{
	if (Unforce)
		m_TeeInfos.m_ForcedSkin = SKIN_NONE;

	if (m_SpookyGhost)
		SetSkin(SKIN_SPOOKY_GHOST);
	else if (m_TeeInfos.m_ForcedSkin != SKIN_NONE)
		SetSkin(m_TeeInfos.m_ForcedSkin, true);
	else
		GameServer()->SendSkinChange(m_TeeInfos, m_ClientID, -1);
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
