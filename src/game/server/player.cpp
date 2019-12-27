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
	m_Team = AsSpec ? TEAM_SPECTATORS : TEAM_RED;
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

	if (g_Config.m_Events)
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

	m_ShowOthers = g_Config.m_SvShowOthersDefault;
	m_ShowAll = g_Config.m_SvShowAllDefault;
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
		m_FirstVoteTick = Now + g_Config.m_SvJoinVoteDelay * TickSpeed;
	else
		m_FirstVoteTick = Now;

	m_NotEligibleForFinish = false;
	m_EligibleForFinishCheck = 0;

	// F-DDrace

	m_IsDummy = false;
	m_Dummymode = DUMMYMODE_IDLE;
	m_FakePing = 0;

	m_vWeaponLimit.resize(NUM_WEAPONS);

	m_Gamemode = g_Config.m_SvVanillaModeStart ? GAMEMODE_VANILLA : GAMEMODE_DDRACE;

	m_FixNameID = -1;
	m_RemovedName = false;
	m_ShowName = true;
	m_SetRealName = false;
	m_SetRealNameTick = Now;
	m_ChatTeam = CHAT_ALL;
	m_ChatText[0] = '\0';
	m_MsgKiller = -1;
	m_MsgWeapon = -1;
	m_MsgModeSpecial = 0;

	m_ResumeMoved = false;

	m_RainbowSpeed = g_Config.m_SvRainbowSpeedDefault;
	m_RainbowColor = 0;
	m_InfRainbow = false;
	m_InfMeteors = 0;

	m_InstagibScore = 0;

	m_ForceSpawnPos = vec2(-1, -1);
	m_WeaponIndicator = g_Config.m_SvWeaponIndicatorDefault;

	m_Minigame = MINIGAME_NONE;
	m_SurvivalState = SURVIVAL_OFFLINE;

	m_SpookyGhost = false;
	m_HasSpookyGhost = false;

	m_LoadedSkin = true;

	m_ScoreMode = g_Config.m_SvDefaultScoreMode;
	m_HasRoomKey = false;
	m_SmoothFreeze = g_Config.m_SvSmoothFreeze;
	m_UnsavedBlockPoints = 0;
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

	Server()->SetClientScore(m_ClientID, g_Config.m_SvDefaultScoreMode == SCORE_TIME ? m_Score : g_Config.m_SvDefaultScoreMode == SCORE_LEVEL ? GameServer()->m_Accounts[GetAccID()].m_Level : GameServer()->m_Accounts[GetAccID()].m_BlockPoints);

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
				if (!m_Paused)
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
			m_pCharacter->m_ShopMotdTick = 0;
		else
			m_pCharacter->m_NumGhostShots = 0;

		// stop the spinning animation when chat is opened
		if (m_PlayerFlags&PLAYERFLAG_CHATTING)
			m_pCharacter->Core()->m_UpdateAngle = UPDATE_ANGLE_TIME;
	}

	// name
	if (!m_ShowName && !m_RemovedName)
	{
		SetFakeInfo(" ", Server()->ClientName(m_ClientID));
		m_RemovedName = true;
	}
	if ((m_ShowName || m_SetRealName) && m_RemovedName)
	{
		SetFakeInfo(Server()->ClientName(m_ClientID), Server()->ClientClan(m_ClientID));
		m_RemovedName = false;
	}

	// fixing messages if name is hidden
	if (m_SetRealName)
	{
		if (m_SetRealNameTick < Server()->Tick())
		{
			if (m_FixNameID == FIX_CHAT_MSG)
				GameServer()->SendChat(m_ClientID, m_ChatTeam, -1, m_ChatText, m_ClientID);
			else if (m_FixNameID == FIX_KILL_MSG)
			{
				CNetMsg_Sv_KillMsg Msg;
				Msg.m_Killer = m_MsgKiller;
				Msg.m_Victim = GetCID();
				Msg.m_Weapon = m_MsgWeapon;
				Msg.m_ModeSpecial = m_MsgModeSpecial;
				for (int i = 0; i < MAX_CLIENTS; i++)
					if (GameServer()->m_apPlayers[i] && (!g_Config.m_SvHideMinigamePlayers || (m_Minigame == GameServer()->m_apPlayers[i]->m_Minigame)))
						Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
			}

			m_SetRealName = false;
		}
	}
}

void CPlayer::PostTick()
{
	// update latency value
	if(m_PlayerFlags&PLAYERFLAG_SCOREBOARD)
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
}

void CPlayer::PostPostTick()
{
#ifdef CONF_DEBUG
	if (!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS - g_Config.m_DbgDummies)
#endif
	if (!Server()->ClientIngame(m_ClientID))
		return;

	if(!GameServer()->m_World.m_Paused && !m_pCharacter && m_Spawning && m_WeakHookSpawn)
		TryRespawn();
}

void CPlayer::Snap(int SnappingClient)
{
	if(!IsDummy() && !Server()->ClientIngame(m_ClientID))
		return;

	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, m_ClientID, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	CPlayer *pSnapping = GameServer()->m_apPlayers[SnappingClient];

	pPlayerInfo->m_PlayerFlags = m_PlayerFlags&PLAYERFLAG_CHATTING;
	if(Server()->GetAuthedState(m_ClientID))
		pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_ADMIN;
	if(m_IsReadyToPlay)
		pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_READY;
	if(m_Paused)
		pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_DEAD;
	if(SnappingClient != -1 && (m_Team == TEAM_SPECTATORS || m_Paused) && (SnappingClient == m_SpectatorID))
		pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_WATCHING;
	if (m_IsDummy && g_Config.m_SvDummyBotSkin)
		pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_BOT;

	// realistic ping for dummies
	if (m_IsDummy && g_Config.m_SvFakeDummyPing && SnappingClient == m_ClientID)
	{
		if (Server()->Tick() % 200 == 0)
			m_FakePing = 32 + rand() % 11;
		pPlayerInfo->m_Latency = m_FakePing;
	}
	else
		pPlayerInfo->m_Latency = SnappingClient == -1 ? m_Latency.m_Min : GameServer()->m_apPlayers[SnappingClient]->m_aActLatency[m_ClientID];
	
	int Score = 0;
	bool AccUsed = true;

	// check for minigames first, then normal score modes, as minigames of course overwrite the wanted scoremodes
	if (pSnapping->m_Minigame == MINIGAME_BLOCK)
	{
		Score = GameServer()->m_Accounts[GetAccID()].m_Kills;
	}
	else if (pSnapping->m_Minigame == MINIGAME_SURVIVAL)
	{
		Score = GameServer()->m_Accounts[GetAccID()].m_SurvivalKills;
	}
	else if (pSnapping->m_Minigame == MINIGAME_INSTAGIB_BOOMFNG || pSnapping->m_Minigame == MINIGAME_INSTAGIB_FNG)
	{
		Score = m_InstagibScore;
		AccUsed = false;
	}
	else if (pSnapping->m_ScoreMode == SCORE_TIME)
	{
		// send 0 if times of others are not shown
		if (SnappingClient != m_ClientID && g_Config.m_SvHideScore)
			Score = -1;
		else
			Score = m_Score == -1 ? -1 : abs(m_Score) * 1000.0f;
		AccUsed = false;
	}
	else if (pSnapping->m_ScoreMode == SCORE_LEVEL)
	{
		Score = GameServer()->m_Accounts[GetAccID()].m_Level;
	}
	else if (pSnapping->m_ScoreMode == SCORE_BLOCK_POINTS)
	{
		Score = GameServer()->m_Accounts[GetAccID()].m_BlockPoints;
	}

	if (AccUsed && GetAccID() < ACC_START)
		Score = 0;
	pPlayerInfo->m_Score = Score;

	if ((m_InfRainbow || IsHooked(RAINBOW) || (m_pCharacter && m_pCharacter->m_Rainbow)) && Server()->GetAuthedState(m_ClientID))
	{
		CNetMsg_Sv_SkinChange Msg;
		Msg.m_ClientID = m_ClientID;
		m_RainbowColor = (m_RainbowColor + m_RainbowSpeed) % 256;
		for (int p = 0; p < NUM_SKINPARTS; p++)
		{
			int BaseColor = m_RainbowColor * 0x010000;
			int Color = 0xff32;
			if (p == SKINPART_MARKING)
				Color *= -256;
			Msg.m_apSkinPartNames[p] = m_TeeInfos.m_aaSkinPartNames[p];
			Msg.m_aUseCustomColors[p] = 1;
			Msg.m_aSkinPartColors[p] = BaseColor + Color;
		}
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, SnappingClient);

		m_LoadedSkin = false;
	}
	else if (!m_LoadedSkin)
	{
		m_LoadedSkin = true;
		LoadSkin();
	}

	if (m_ClientID == SnappingClient && (m_Team == TEAM_SPECTATORS || m_Paused))
	{
		CNetObj_SpectatorInfo* pSpectatorInfo = static_cast<CNetObj_SpectatorInfo*>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(CNetObj_SpectatorInfo)));
		if (!pSpectatorInfo)
			return;

		pSpectatorInfo->m_SpecMode = m_SpecMode;
		pSpectatorInfo->m_SpectatorID = m_SpectatorID;
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

	// demo recording
	if(SnappingClient == -1)
	{
		CNetObj_De_ClientInfo *pClientInfo = static_cast<CNetObj_De_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_DE_CLIENTINFO, m_ClientID, sizeof(CNetObj_De_ClientInfo)));
		if(!pClientInfo)
			return;

		pClientInfo->m_Local = 0;
		pClientInfo->m_Team = m_Team;
		StrToInts(pClientInfo->m_aName, 4, Server()->ClientName(m_ClientID));
		StrToInts(pClientInfo->m_aClan, 3, Server()->ClientClan(m_ClientID));
		pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);

		for(int p = 0; p < NUM_SKINPARTS; p++)
		{
			StrToInts(pClientInfo->m_aaSkinPartNames[p], 6, m_TeeInfos.m_aaSkinPartNames[p]);
			pClientInfo->m_aUseCustomColors[p] = m_TeeInfos.m_aUseCustomColors[p];
			pClientInfo->m_aSkinPartColors[p] = m_TeeInfos.m_aSkinPartColors[p];
		}
	}
}

void CPlayer::OnDisconnect()
{
	KillCharacter();

	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
	Controller->m_Teams.SetForceCharacterTeam(m_ClientID, 0);

	if (m_Team != TEAM_SPECTATORS)
	{
		// update spectator modes
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_SpecMode == SPEC_PLAYER && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
			{
				GameServer()->m_apPlayers[i]->m_SpecMode = SPEC_FREEVIEW;
				GameServer()->m_apPlayers[i]->m_SpectatorID = -1;
			}
		}
	}
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *NewInput)
{
	// skip the input if chat is active
	if((m_PlayerFlags&PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING))
		return;

	AfkVoteTimer(NewInput);

	if(m_pCharacter && !m_Paused)
		m_pCharacter->OnPredictedInput(NewInput);
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput *NewInput)
{
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

	if (m_pCharacter && m_Paused)
		m_pCharacter->ResetInput();

	if(m_pCharacter && !m_Paused)
		m_pCharacter->OnDirectInput(NewInput);

	if(!m_pCharacter && m_Team != TEAM_SPECTATORS && (NewInput->m_Fire&1))
		m_Spawning = true;

	if(((!m_pCharacter && m_Team == TEAM_SPECTATORS) || m_Paused) && (NewInput->m_Fire&1))
	{
		if(!m_ActiveSpecSwitch)
		{
			m_ActiveSpecSwitch = true;
			if(m_SpecMode == SPEC_FREEVIEW)
			{
				CCharacter *pChar = (CCharacter *)GameServer()->m_World.ClosestEntity(m_ViewPos, 6.0f*32, CGameWorld::ENTTYPE_CHARACTER, (m_pCharacter && m_pCharacter->IsAlive()) ? m_pCharacter : 0);
				CFlag *pFlag = (CFlag *)GameServer()->m_World.ClosestEntity(m_ViewPos, 6.0f*32, CGameWorld::ENTTYPE_FLAG, GetCharacter());
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

void CPlayer::KillCharacter(int Weapon)
{
	if(m_pCharacter)
	{
		m_pCharacter->Die(m_ClientID, Weapon);
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

	if (Team == TEAM_SPECTATORS)
	{
		// update spectator modes
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
			{
				GameServer()->m_apPlayers[i]->m_SpectatorID = -1;
				GameServer()->m_apPlayers[i]->m_SpecMode = SPEC_FREEVIEW;
			}
		}
	}

	// notify clients
	CNetMsg_Sv_Team Msg;
	Msg.m_ClientID = m_ClientID;
	Msg.m_Team = Team;
	Msg.m_Silent = DoChatMsg ? 0 : 1;
	Msg.m_CooldownTick = m_TeamChangeTick;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

	GameServer()->OnClientTeamChange(m_ClientID);

	GameServer()->UpdateHidePlayers(m_ClientID);
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

	if (g_Config.m_SvTeam == 3)
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

	if (Server()->GetAuthedState(m_ClientID))
		return false; // don't kick admins
	if (g_Config.m_SvMaxAfkTime == 0)
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
			if (m_Sent1stAfkWarning == 0 && m_LastPlaytime < time_get() - time_freq() * (int)(g_Config.m_SvMaxAfkTime * 0.5))
			{
				str_format(m_pAfkMsg, sizeof(m_pAfkMsg),
					"You have been afk for %d seconds now. Please note that you get kicked after not playing for %d seconds.",
					(int)(g_Config.m_SvMaxAfkTime * 0.5),
					g_Config.m_SvMaxAfkTime
				);
				m_pGameServer->SendChatTarget(m_ClientID, m_pAfkMsg);
				m_Sent1stAfkWarning = 1;
			}
			else if (m_Sent2ndAfkWarning == 0 && m_LastPlaytime < time_get() - time_freq() * (int)(g_Config.m_SvMaxAfkTime * 0.9))
			{
				str_format(m_pAfkMsg, sizeof(m_pAfkMsg),
					"You have been afk for %d seconds now. Please note that you get kicked after not playing for %d seconds.",
					(int)(g_Config.m_SvMaxAfkTime * 0.9),
					g_Config.m_SvMaxAfkTime
				);
				m_pGameServer->SendChatTarget(m_ClientID, m_pAfkMsg);
				m_Sent2ndAfkWarning = 1;
			}
			else if (m_LastPlaytime < time_get() - time_freq() * g_Config.m_SvMaxAfkTime)
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
	if (g_Config.m_SvMaxAfkVoteTime == 0)
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
	else if (m_LastPlaytime < time_get() - time_freq() * g_Config.m_SvMaxAfkVoteTime)
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

	char aBuf[128];
	if (State != m_Paused)
	{
		// Get to wanted state
		switch (State) {
		case PAUSE_PAUSED:
		case PAUSE_NONE:
			if (m_pCharacter->IsPaused()) // First condition might be unnecessary
			{
				if (!Force && m_LastPause && m_LastPause + g_Config.m_SvSpecFrequency * Server()->TickSpeed() > Server()->Tick())
				{
					GameServer()->SendChatTarget(m_ClientID, "Can't /spec that quickly.");
					return m_Paused; // Do not update state. Do not collect $200
				}
				m_pCharacter->Pause(false);
				GameServer()->CreatePlayerSpawn(m_pCharacter->GetPos(), m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));
			}
			// fall-thru
		case PAUSE_SPEC:
			if (g_Config.m_SvPauseMessages)
			{
				str_format(aBuf, sizeof(aBuf), (State > PAUSE_NONE) ? "'%s' speced" : "'%s' resumed", Server()->ClientName(m_ClientID));
				GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
			}
			break;
		}

		// Update state
		m_Paused = State;
		m_LastPause = Server()->Tick();

		CNetMsg_Sv_Team Msg;
		Msg.m_ClientID = m_ClientID;
		Msg.m_Team = !m_Paused ? m_Team : TEAM_SPECTATORS;
		Msg.m_Silent = 1;
		Msg.m_CooldownTick = Server()->Tick();
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, m_ClientID);
	}

	return m_Paused;
}

int CPlayer::ForcePause(int Time)
{
	m_ForcePauseTime = Server()->Tick() + Server()->TickSpeed() * Time;

	if (g_Config.m_SvPauseMessages)
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

void CPlayer::MoneyTransaction(int Amount, const char *pDescription)
{
	if (GetAccID() < ACC_START)
		return;

	CGameContext::AccountInfo *Account = &GameServer()->m_Accounts[GetAccID()];

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

void CPlayer::OnLogin()
{
	CGameContext::AccountInfo* Account = &GameServer()->m_Accounts[GetAccID()];
	if (m_pCharacter)
	{
		if (m_pCharacter->GetWeaponGot(WEAPON_LASER) && (*Account).m_TaserLevel >= 1)
			m_pCharacter->GiveWeapon(WEAPON_TASER, false, m_pCharacter->GetWeaponAmmo(WEAPON_LASER));
	}
}

void CPlayer::OnLogout()
{
	if (m_pCharacter)
	{
		if (m_pCharacter->GetWeaponGot(WEAPON_TASER))
			m_pCharacter->GiveWeapon(WEAPON_TASER, true);
	}
}

void CPlayer::GiveBlockPoints(int Amount)
{
	if (GetAccID() < ACC_START)
	{
		m_UnsavedBlockPoints++;
		if (m_UnsavedBlockPoints % 5 == 0)
			GameServer()->SendChatTarget(m_ClientID, "You made unsaved block points. Save your stats using an account. Check '/accountinfo'");
		return;
	}

	CGameContext::AccountInfo* Account = &GameServer()->m_Accounts[GetAccID()];

	if (m_pCharacter && m_pCharacter->HasFlag() != -1)
		Amount += 1;

	(*Account).m_BlockPoints += Amount;
}

bool CPlayer::IsHooked(int Power)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter* pChr = GameServer()->GetPlayerChar(i);
		if (!pChr)
			continue;

		if (pChr->Core()->m_HookedPlayer == m_ClientID)
		{
			if (Power == -2 && m_pCharacter)
				m_pCharacter->Core()->m_Killer.m_ClientID = i;
			return Power >= 0 ? pChr->m_HookPower == Power : true;
		}
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

void CPlayer::UpdateFakeInformation(int ClientID)
{
	CNetMsg_Sv_ClientDrop ClientDropMsg;
	ClientDropMsg.m_ClientID = m_ClientID;
	ClientDropMsg.m_pReason = "";
	ClientDropMsg.m_Silent = 1;

	CNetMsg_Sv_ClientInfo NewClientInfoMsg;
	NewClientInfoMsg.m_ClientID = m_ClientID;
	NewClientInfoMsg.m_Local = 0;
	NewClientInfoMsg.m_Team = m_Team;
	NewClientInfoMsg.m_pName = m_aFakeName;
	NewClientInfoMsg.m_pClan = m_aFakeClan;
	NewClientInfoMsg.m_Country = Server()->ClientCountry(m_ClientID);
	NewClientInfoMsg.m_Silent = 1;

	for (int p = 0; p < NUM_SKINPARTS; p++)
	{
		NewClientInfoMsg.m_apSkinPartNames[p] = m_TeeInfos.m_aaSkinPartNames[p];
		NewClientInfoMsg.m_aUseCustomColors[p] = m_TeeInfos.m_aUseCustomColors[p];
		NewClientInfoMsg.m_aSkinPartColors[p] = m_TeeInfos.m_aSkinPartColors[p];
	}

	Server()->SendPackMsg(&ClientDropMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, ClientID);
	Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, ClientID);

	GameServer()->UpdateHidePlayers(m_ClientID);
}

void CPlayer::SendSpookyGhostSkin()
{
	str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_BODY], "spiky", 24);
	str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_MARKING], "tricircular", 24);
	str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_DECORATION], "", 24);
	str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_HANDS], "standard", 24);
	str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_FEET], "standard", 24);
	str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_EYES], "colorable", 24);

	for (int p = 0; p < NUM_SKINPARTS; p++)
	{
		m_TeeInfos.m_aUseCustomColors[p] = 1;
	}

	m_TeeInfos.m_aSkinPartColors[SKINPART_BODY] = 255;
	m_TeeInfos.m_aSkinPartColors[SKINPART_MARKING] = -16777016;
	m_TeeInfos.m_aSkinPartColors[SKINPART_DECORATION] = 255;
	m_TeeInfos.m_aSkinPartColors[SKINPART_HANDS] = 184;
	m_TeeInfos.m_aSkinPartColors[SKINPART_FEET] = 9765959;
	m_TeeInfos.m_aSkinPartColors[SKINPART_EYES] = 255;

	GameServer()->SendSkinChange(m_ClientID, -1);
}

void CPlayer::BackupSkin()
{
	if (m_SpookyGhost)
		return;

	for (int p = 0; p < NUM_SKINPARTS; p++)
	{
		str_copy(m_SavedTeeInfos.m_aaSkinPartNames[p], m_TeeInfos.m_aaSkinPartNames[p], 24);
		m_SavedTeeInfos.m_aUseCustomColors[p] = m_TeeInfos.m_aUseCustomColors[p];
		m_SavedTeeInfos.m_aSkinPartColors[p] = m_TeeInfos.m_aSkinPartColors[p];
	}
}

void CPlayer::LoadSkin()
{
	if (m_SpookyGhost)
	{
		SendSpookyGhostSkin();
		return;
	}

	for (int p = 0; p < NUM_SKINPARTS; p++)
	{
		str_copy(m_TeeInfos.m_aaSkinPartNames[p], m_SavedTeeInfos.m_aaSkinPartNames[p], 24);
		m_TeeInfos.m_aUseCustomColors[p] = m_SavedTeeInfos.m_aUseCustomColors[p];
		m_TeeInfos.m_aSkinPartColors[p] = m_SavedTeeInfos.m_aSkinPartColors[p];
	}
	GameServer()->SendSkinChange(m_ClientID, -1);
}
