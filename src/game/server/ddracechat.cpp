/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "gamecontext.h"
#include <engine/engine.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <game/server/teams.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/version.h>
#include <game/server/entities/character.h>
#include "score.h"

bool CheckClientID(int ClientID);

void CGameContext::ConCredits(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"F-DDrace is a mod by fokkonaut");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"Based on Teeworlds 0.7 by the Teeworlds developers, uses parts of the 0.6 DDRace mod by the DDRace developers.");
}

void CGameContext::ConInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info",
			"F-DDrace Mod. Version: " GAME_VERSION ", by fokkonaut");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info",
			"For more info, say '/cmdlist'");
}

void CGameContext::ConList(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientID = pResult->m_ClientID;
	if(!CheckClientID(ClientID)) return;

	char zerochar = 0;
	if(pResult->NumArguments() > 0)
		pSelf->List(ClientID, pResult->GetString(0));
	else
		pSelf->List(ClientID, &zerochar);
}

void CGameContext::ConHelp(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;

	if (pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help",
				"/cmdlist will show a list of all chat commands");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help",
				"/help + any command will show you the help for this command");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help",
				"Example /help settings will display the help about /settings");
	}
	else
	{
		const char *pArg = pResult->GetString(0);
		const IConsole::CCommandInfo *pCmdInfo =
				pSelf->Console()->GetCommandInfo(pArg, CFGFLAG_SERVER, false);
		if (pCmdInfo)
		{
			if (pCmdInfo->m_pParams)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "Usage: %s %s", pCmdInfo->m_pName, pCmdInfo->m_pParams);
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help", aBuf);
			}

			if (pCmdInfo->m_pHelp)
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help", pCmdInfo->m_pHelp);
		}
		else
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"help",
					"Command is either unknown or you have given a blank command without any parameters.");
	}
}

void CGameContext::ConSettings(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;

	if (pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
				"to check a server setting say /settings and setting's name, setting names are:");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
				"teams, collision, hooking, endlesshooking, me, ");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
				"hitting, oldlaser, votes, pause and scores");
	}
	else
	{
		const char *pArg = pResult->GetString(0);
		char aBuf[256];
		float ColTemp;
		float HookTemp;
		pSelf->m_Tuning.Get("player_collision", &ColTemp);
		pSelf->m_Tuning.Get("player_hooking", &HookTemp);
		if (str_comp(pArg, "teams") == 0)
		{
			str_format(aBuf, sizeof(aBuf), "%s %s",
				pSelf->Config()->m_SvTeam == 1 ?
							"Teams are available on this server" :
							(pSelf->Config()->m_SvTeam == 0 || pSelf->Config()->m_SvTeam == 3) ?
									"Teams are not available on this server" :
									"You have to be in a team to play on this server", /*pSelf->Config()->m_SvTeamStrict ? "and if you die in a team all of you die" : */
									"and all of your team will die if the team is locked");
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings", aBuf);
		}
		else if (str_comp(pArg, "collision") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
				ColTemp ?
						"Players can collide on this server" :
						"Players can't collide on this server");
		}
		else if (str_comp(pArg, "hooking") == 0)
		{
			pSelf->Console()->Print( IConsole::OUTPUT_LEVEL_STANDARD, "settings",
				HookTemp ?
						"Players can hook each other on this server" :
						"Players can't hook each other on this server");
		}
		else if (str_comp(pArg, "endlesshooking") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
				pSelf->Config()->m_SvEndlessDrag ?
						"Players hook time is unlimited" :
						"Players hook time is limited");
		}
		else if (str_comp(pArg, "hitting") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
				pSelf->Config()->m_SvHit ?
						"Players weapons affect others" :
						"Players weapons has no affect on others");
		}
		else if (str_comp(pArg, "oldlaser") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
				pSelf->Config()->m_SvOldLaser ?
						"Lasers can hit you if you shot them and they pull you towards the bounce origin (Like DDRace Beta)" :
						"Lasers can't hit you if you shot them, and they pull others towards the shooter");
		}
		else if (str_comp(pArg, "me") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
				pSelf->Config()->m_SvSlashMe ?
						"Players can use /me commands the famous IRC Command" :
						"Players can't use the /me command");
		}
		else if (str_comp(pArg, "votes") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
				pSelf->Config()->m_SvVoteKick ?
						"Players can use Callvote menu tab to kick offenders" :
						"Players can't use the Callvote menu tab to kick offenders");
			if (pSelf->Config()->m_SvVoteKick)
			{
				str_format(aBuf, sizeof(aBuf),
						"Players are banned for %d minute(s) if they get voted off", pSelf->Config()->m_SvVoteKickBantime);

				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
					pSelf->Config()->m_SvVoteKickBantime ?
								aBuf :
								"Players are just kicked and not banned if they get voted off");
			}
		}
		else if (str_comp(pArg, "pause") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
				pSelf->Config()->m_SvPauseable ?
						"/spec will pause you and your tee will vanish" :
						"/spec will pause you but your tee will not vanish");
		}
		else if (str_comp(pArg, "scores") == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
				pSelf->Config()->m_SvHideScore ?
						"Scores are private on this server" :
						"Scores are public on this server");
		}
		else
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
			"no matching settings found, type /settings to view them");
		}
	}
}

void ToggleSpecPause(IConsole::IResult *pResult, void *pUserData, int PauseType)
{
	if(!CheckClientID(pResult->m_ClientID))
		return;

	CGameContext *pSelf = (CGameContext *) pUserData;
	IServer* pServ = pSelf->Server();
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;

	if (pPlayer->m_Minigame == MINIGAME_SURVIVAL && pPlayer->m_SurvivalState > SURVIVAL_LOBBY && pPlayer->GetTeam() != TEAM_SPECTATORS && !pPlayer->IsPaused())
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You can't join the spectators while you are in survival");
		return;
	}

	int PauseState = pPlayer->IsPaused();
	if(PauseState > 0)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "You are force-paused for %d seconds.", (PauseState - pServ->Tick()) / pServ->TickSpeed());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "spec", aBuf);
	}
	else if(pResult->NumArguments() > 0)
	{
		if(-PauseState == PauseType && pPlayer->GetSpectatorID() != pResult->m_ClientID && pServ->ClientIngame(pPlayer->GetSpectatorID()) && !str_comp(pServ->ClientName(pPlayer->GetSpectatorID()), pResult->GetString(0)))
		{
			pPlayer->Pause(CPlayer::PAUSE_NONE, false);
		}
		else
		{
			pPlayer->Pause(PauseType, false);
			pPlayer->SpectatePlayerName(pResult->GetString(0));
		}
	}
	else if(-PauseState != CPlayer::PAUSE_NONE && PauseType != CPlayer::PAUSE_NONE)
	{
		pPlayer->Pause(CPlayer::PAUSE_NONE, false);
	}
	else if(-PauseState != PauseType)
	{
		pPlayer->Pause(PauseType, false);
	}
}

void ToggleSpecPauseVoted(IConsole::IResult *pResult, void *pUserData, int PauseType)
{
	if(!CheckClientID(pResult->m_ClientID))
		return;

	CGameContext *pSelf = (CGameContext *) pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;

	if (pPlayer->m_Minigame == MINIGAME_SURVIVAL && pPlayer->m_SurvivalState > SURVIVAL_LOBBY && pPlayer->GetTeam() != TEAM_SPECTATORS && !pPlayer->IsPaused())
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You can't join the spectators while you are in survival");
		return;
	}

	int PauseState = pPlayer->IsPaused();
	if(PauseState > 0)
	{
		IServer* pServ = pSelf->Server();
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "You are force-paused for %d seconds.", (PauseState - pServ->Tick()) / pServ->TickSpeed());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "spec", aBuf);
		return;
	}

	bool IsPlayerBeingVoted = pSelf->m_VoteCloseTime &&
		(pSelf->m_VoteKick || pSelf->m_VoteSpec) &&
		pResult->m_ClientID != pSelf->m_VoteClientID;
	if((!IsPlayerBeingVoted && -PauseState == PauseType) ||
		(IsPlayerBeingVoted && PauseState && pPlayer->GetSpectatorID() == pSelf->m_VoteClientID))
	{
		pPlayer->Pause(CPlayer::PAUSE_NONE, false);
	}
	else
	{
		pPlayer->Pause(PauseType, false);
		if(IsPlayerBeingVoted)
			pPlayer->SetSpectatorID(SPEC_PLAYER, pSelf->m_VoteClientID);
	}
}

void CGameContext::ConToggleSpec(IConsole::IResult *pResult, void *pUserData)
{
	ToggleSpecPause(pResult, pUserData, ((CGameContext*)pUserData)->Config()->m_SvPauseable ? CPlayer::PAUSE_SPEC : CPlayer::PAUSE_PAUSED);
}

void CGameContext::ConToggleSpecVoted(IConsole::IResult *pResult, void *pUserData)
{
	ToggleSpecPauseVoted(pResult, pUserData, ((CGameContext*)pUserData)->Config()->m_SvPauseable ? CPlayer::PAUSE_SPEC : CPlayer::PAUSE_PAUSED);
}

void CGameContext::ConTogglePause(IConsole::IResult *pResult, void *pUserData)
{
	ToggleSpecPause(pResult, pUserData, CPlayer::PAUSE_PAUSED);
}

void CGameContext::ConTogglePauseVoted(IConsole::IResult *pResult, void *pUserData)
{
	ToggleSpecPauseVoted(pResult, pUserData, CPlayer::PAUSE_PAUSED);
}

void CGameContext::ConTeamTop5(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

#if defined(CONF_SQL)
	if(pSelf->m_apPlayers[pResult->m_ClientID] && pSelf->Config()->m_SvUseSQL)
		if(pSelf->m_apPlayers[pResult->m_ClientID]->m_LastSQLQuery + pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
			return;
#endif

	if (pSelf->Config()->m_SvHideScore)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "teamtop5",
				"Showing the team top 5 is not allowed on this server.");
		return;
	}

	if (pResult->NumArguments() > 0 && pResult->GetInteger(0) >= 0)
		pSelf->Score()->ShowTeamTop5(pResult, pResult->m_ClientID, pUserData,
				pResult->GetInteger(0));
	else
		pSelf->Score()->ShowTeamTop5(pResult, pResult->m_ClientID, pUserData);

#if defined(CONF_SQL)
	if(pSelf->m_apPlayers[pResult->m_ClientID] && pSelf->Config()->m_SvUseSQL)
		pSelf->m_apPlayers[pResult->m_ClientID]->m_LastSQLQuery = pSelf->Server()->Tick();
#endif
}

void CGameContext::ConTop5(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	if (pSelf->Config()->m_SvHideScore)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "top5",
				"Showing the top 5 is not allowed on this server.");
		return;
	}

	if (pResult->NumArguments() > 0)
		pSelf->Score()->ShowTop5(pResult, pResult->m_ClientID, pUserData,
				pResult->GetInteger(0));
	else
		pSelf->Score()->ShowTop5(pResult, pResult->m_ClientID, pUserData);
}

void CGameContext::ConMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	if (pSelf->Config()->m_SvMapVote == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "map",
				"/map is disabled");
		return;
	}

	if (pResult->NumArguments() <= 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "map", "Example: /map adr3 to call vote for Adrenaline 3. This means that the map name must start with 'a' and contain the characters 'd', 'r' and '3' in that order");
		return;
	}

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

#if defined(CONF_SQL)
	if(pSelf->Config()->m_SvUseSQL)
		if(pPlayer->m_LastSQLQuery + pSelf->Config()->m_SvSqlQueriesDelay * pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
			return;
#endif

	pSelf->Score()->MapVote(&pSelf->m_pMapVoteResult, pResult->m_ClientID, pResult->GetString(0));

#if defined(CONF_SQL)
	if(pSelf->Config()->m_SvUseSQL)
		pPlayer->m_LastSQLQuery = pSelf->Server()->Tick();
#endif
}

void CGameContext::ConMapInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pResult->NumArguments() > 0)
		pSelf->Score()->MapInfo(pResult->m_ClientID, pResult->GetString(0));
	else
		pSelf->Score()->MapInfo(pResult->m_ClientID, pSelf->Config()->m_SvMap);
}

void CGameContext::ConSave(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

#if defined(CONF_SQL)
	if(!pSelf->Config()->m_SvSaveGames)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Save-function is disabled on this server");
		return;
	}

	if(pSelf->Config()->m_SvUseSQL)
		if(pPlayer->m_LastSQLQuery + pSelf->Config()->m_SvSqlQueriesDelay * pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
			return;

	int Team = ((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.m_Core.Team(pResult->m_ClientID);

	const char* pCode = pResult->GetString(0);
	char aCountry[5];
	if(str_length(pCode) > 3 && pCode[0] >= 'A' && pCode[0] <= 'Z' && pCode[1] >= 'A'
		&& pCode[1] <= 'Z' && pCode[2] >= 'A' && pCode[2] <= 'Z')
	{
		if(pCode[3] == ' ')
		{
			str_copy(aCountry, pCode, 4);
			pCode = pCode + 4;
		}
		else if(str_length(pCode) > 4 && pCode[4] == ' ')
		{
			str_copy(aCountry, pCode, 5);
			pCode = pCode + 5;
		}
		else
		{
			str_copy(aCountry, pSelf->Config()->m_SvSqlServerName, sizeof(aCountry));
		}
	}
	else
	{
		str_copy(aCountry, pSelf->Config()->m_SvSqlServerName, sizeof(aCountry));
	}

	if(str_in_list(pSelf->Config()->m_SvSqlValidServerNames, ",", aCountry))
	{
		pSelf->Score()->SaveTeam(Team, pCode, pResult->m_ClientID, aCountry);

		if(pSelf->Config()->m_SvUseSQL)
			pPlayer->m_LastSQLQuery = pSelf->Server()->Tick();
	}
	else
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Unknown server name '%s'.", aCountry);
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}

#endif
}

void CGameContext::ConLoad(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

#if defined(CONF_SQL)
	if(!pSelf->Config()->m_SvSaveGames)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Save-function is disabled on this server");
		return;
	}

	if(pSelf->Config()->m_SvUseSQL)
		if(pPlayer->m_LastSQLQuery + pSelf->Config()->m_SvSqlQueriesDelay * pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
			return;
#endif

	if (pResult->NumArguments() > 0)
		pSelf->Score()->LoadTeam(pResult->GetString(0), pResult->m_ClientID);
	else
		return;

#if defined(CONF_SQL)
	if(pSelf->Config()->m_SvUseSQL)
		pPlayer->m_LastSQLQuery = pSelf->Server()->Tick();
#endif
}

void CGameContext::ConTeamRank(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

#if defined(CONF_SQL)
	if(pSelf->m_apPlayers[pResult->m_ClientID] && pSelf->Config()->m_SvUseSQL)
		if(pSelf->m_apPlayers[pResult->m_ClientID]->m_LastSQLQuery + pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
			return;
#endif

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pResult->NumArguments() > 0)
		if (!pSelf->Config()->m_SvHideScore)
			pSelf->Score()->ShowTeamRank(pResult->m_ClientID, pResult->GetString(0),
					true);
		else
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"teamrank",
					"Showing the team rank of other players is not allowed on this server.");
	else
		pSelf->Score()->ShowTeamRank(pResult->m_ClientID,
				pSelf->Server()->ClientName(pResult->m_ClientID));

#if defined(CONF_SQL)
	if(pSelf->m_apPlayers[pResult->m_ClientID] && pSelf->Config()->m_SvUseSQL)
		pSelf->m_apPlayers[pResult->m_ClientID]->m_LastSQLQuery = pSelf->Server()->Tick();
#endif
}

void CGameContext::ConRank(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pResult->NumArguments() > 0)
		if (!pSelf->Config()->m_SvHideScore)
			pSelf->Score()->ShowRank(pResult->m_ClientID, pResult->GetString(0),
					true);
		else
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"rank",
					"Showing the rank of other players is not allowed on this server.");
	else
		pSelf->Score()->ShowRank(pResult->m_ClientID,
				pSelf->Server()->ClientName(pResult->m_ClientID));
}

void CGameContext::ConLockTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	int Team = ((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.m_Core.Team(pResult->m_ClientID);

	bool Lock = ((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.TeamLocked(Team);

	if (pResult->NumArguments() > 0)
		Lock = !pResult->GetInteger(0);

	if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
	{
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"print",
				"This team can't be locked");
		return;
	}

	char aBuf[512];
	if(Lock)
	{
		((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.SetTeamLock(Team, false);

		str_format(aBuf, sizeof(aBuf), "'%s' unlocked your team.", pSelf->Server()->ClientName(pResult->m_ClientID));

		for (int i = 0; i < MAX_CLIENTS; i++)
			if (((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.m_Core.Team(i) == Team)
				pSelf->SendChatTarget(i, aBuf);
	}
	else if(!pSelf->Config()->m_SvTeamLock)
	{
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"print",
				"Team locking is disabled on this server");
	}
	else
	{
		((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.SetTeamLock(Team, true);

		str_format(aBuf, sizeof(aBuf), "'%s' locked your team. After the race started killing will kill everyone in your team.", pSelf->Server()->ClientName(pResult->m_ClientID));

		for (int i = 0; i < MAX_CLIENTS; i++)
			if (((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.m_Core.Team(i) == Team)
				pSelf->SendChatTarget(i, aBuf);
	}
}

void CGameContext::ConInviteTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CGameControllerDDRace*pController = (CGameControllerDDRace*)pSelf->m_pController;
	const char *pName = pResult->GetString(0);

	if(pSelf->Config()->m_SvTeam == 0 || pSelf->Config()->m_SvTeam == 3)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
				"Teams are disabled");
		return;
	}

	if(!pSelf->Config()->m_SvInvite)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "invite", "Invites are disabled");
		return;
	}

	int Team = pController->m_Teams.m_Core.Team(pResult->m_ClientID);
	if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
	{
		int Target = -1;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!str_comp(pName, pSelf->Server()->ClientName(i)))
			{
				Target = i;
				break;
			}
		}

		if(Target < 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "invite", "Player not found");
			return;
		}

		if(pController->m_Teams.IsInvited(Team, Target))
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "invite", "Player already invited");
			return;
		}

		if(pSelf->m_apPlayers[pResult->m_ClientID] && pSelf->m_apPlayers[pResult->m_ClientID]->m_LastInvited + pSelf->Config()->m_SvInviteFrequency * pSelf->Server()->TickSpeed() > pSelf->Server()->Tick())
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "invite", "Can't invite this quickly");
			return;
		}

		pController->m_Teams.SetClientInvited(Team, Target, true);
		pSelf->m_apPlayers[pResult->m_ClientID]->m_LastInvited = pSelf->Server()->Tick();

		char aBuf[512];
		str_format(aBuf, sizeof aBuf, "'%s' invited you to team %d.", pSelf->Server()->ClientName(pResult->m_ClientID), Team);
		pSelf->SendChatTarget(Target, aBuf);

		str_format(aBuf, sizeof aBuf, "'%s' invited '%s' to your team.", pSelf->Server()->ClientName(pResult->m_ClientID), pSelf->Server()->ClientName(Target));;
		for (int i = 0; i < MAX_CLIENTS; i++)
			if (((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.m_Core.Team(i) == Team)
				pSelf->SendChatTarget(i, aBuf);
	}
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "invite", "Can't invite players to this team");
}

void CGameContext::ConJoinTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	CGameControllerDDRace*pController = (CGameControllerDDRace*)pSelf->m_pController;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pSelf->m_VoteCloseTime && pSelf->m_VoteCreator == pResult->m_ClientID && (pSelf->m_VoteKick || pSelf->m_VoteSpec))
	{
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"join",
				"You are running a vote please try again after the vote is done!");
		return;
	}
	else if (pSelf->Config()->m_SvTeam == 0 || pSelf->Config()->m_SvTeam == 3)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
				"Teams are disabled");
		return;
	}
	else if (pSelf->Config()->m_SvTeam == 2 && pResult->GetInteger(0) == 0 && pPlayer->GetCharacter() && pPlayer->GetCharacter()->m_LastStartWarning < pSelf->Server()->Tick() - 3 * pSelf->Server()->TickSpeed())
	{
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"join",
				"You must join a team and play with somebody or else you can\'t play");
		pPlayer->GetCharacter()->m_LastStartWarning = pSelf->Server()->Tick();
	}

	if (pResult->NumArguments() > 0)
	{
		if (pPlayer->GetCharacter() == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
					"You can't change teams while you are dead/a spectator.");
		}
		else
		{
			if (pPlayer->m_Last_Team
					+ pSelf->Server()->TickSpeed()
					* pSelf->Config()->m_SvTeamChangeDelay
					> pSelf->Server()->Tick())
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
						"You can\'t change teams that fast!");
			}
			else if (pPlayer->m_Minigame == MINIGAME_SURVIVAL && pResult->NumArguments() > 0)
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
					"You can\'t join teams in survival");
			}
			else if(pResult->GetInteger(0) > 0 && pResult->GetInteger(0) < MAX_CLIENTS && pController->m_Teams.TeamLocked(pResult->GetInteger(0)) && !pController->m_Teams.IsInvited(pResult->GetInteger(0), pResult->m_ClientID))
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
					pSelf->Config()->m_SvInvite ?
						"This team is locked using /lock. Only members of the team can unlock it using /lock." :
						"This team is locked using /lock. Only members of the team can invite you or unlock it using /lock.");
			}
			else if(pResult->GetInteger(0) > 0 && pResult->GetInteger(0) < MAX_CLIENTS && pController->m_Teams.Count(pResult->GetInteger(0)) >= pSelf->Config()->m_SvTeamMaxSize)
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "This team already has the maximum allowed size of %d players", pSelf->Config()->m_SvTeamMaxSize);
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join", aBuf);
			}
			else if (((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.SetCharacterTeam(
					pPlayer->GetCID(), pResult->GetInteger(0)))
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "%s joined team %d",
						pSelf->Server()->ClientName(pPlayer->GetCID()),
						pResult->GetInteger(0));
				pSelf->SendChat(-1, CHAT_ALL, -1, aBuf);
				pPlayer->m_Last_Team = pSelf->Server()->Tick();
			}
			else
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
						"You cannot join this team at this time");
			}
		}
	}
	else
	{
		char aBuf[512];
		if (!pPlayer->IsPlaying())
		{
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"join",
					"You can't check your team while you are dead/a spectator.");
		}
		else
		{
			str_format(
					aBuf,
					sizeof(aBuf),
					"You are in team %d",
					((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.m_Core.Team(
							pResult->m_ClientID));
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
					aBuf);
		}
	}
}

void CGameContext::ConMe(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	char aBuf[256 + 24];

	str_format(aBuf, 256 + 24, "'%s' %s",
			pSelf->Server()->ClientName(pResult->m_ClientID),
			pResult->GetString(0));
	if (pSelf->Config()->m_SvSlashMe)
		pSelf->SendChat(-2, CHAT_ALL, -1, aBuf, pResult->m_ClientID);
	else
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"me",
				"/me is disabled on this server");
}

void CGameContext::ConEyeEmote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (pSelf->Config()->m_SvEmotionalTees == -1)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "emote",
				"Emotes are disabled.");
		return;
	}

	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"emote",
				"Emote commands are: /emote surprise /emote blink /emote close /emote angry /emote happy /emote pain");
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"emote",
				"Example: /emote surprise 10 for 10 seconds or /emote surprise (default 1 second)");
	}
	else
	{
			if(pPlayer->m_LastEyeEmote + pSelf->Config()->m_SvEyeEmoteChangeDelay * pSelf->Server()->TickSpeed() >= pSelf->Server()->Tick())
				return;

			if (!str_comp(pResult->GetString(0), "angry"))
				pPlayer->m_DefEmote = EMOTE_ANGRY;
			else if (!str_comp(pResult->GetString(0), "blink"))
				pPlayer->m_DefEmote = EMOTE_BLINK;
			else if (!str_comp(pResult->GetString(0), "close"))
				pPlayer->m_DefEmote = EMOTE_BLINK;
			else if (!str_comp(pResult->GetString(0), "happy"))
				pPlayer->m_DefEmote = EMOTE_HAPPY;
			else if (!str_comp(pResult->GetString(0), "pain"))
				pPlayer->m_DefEmote = EMOTE_PAIN;
			else if (!str_comp(pResult->GetString(0), "surprise"))
				pPlayer->m_DefEmote = EMOTE_SURPRISE;
			else if (!str_comp(pResult->GetString(0), "normal"))
				pPlayer->m_DefEmote = EMOTE_NORMAL;
			else
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD,
						"emote", "Unknown emote... Say /emote");

			int Duration = 1;
			if (pResult->NumArguments() > 1)
				Duration = pResult->GetInteger(1);

			pPlayer->m_DefEmoteReset = pSelf->Server()->Tick()
							+ Duration * pSelf->Server()->TickSpeed();
			pPlayer->m_LastEyeEmote = pSelf->Server()->Tick();
	}
}

void CGameContext::ConNinjaJetpack(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (!pSelf->m_Accounts[pPlayer->GetAccID()].m_Ninjajetpack)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You don't have ninjajetpack, buy it in the shop");
		return;
	}

	if (pResult->NumArguments())
		pPlayer->m_NinjaJetpack = pResult->GetInteger(0);
	else
		pPlayer->m_NinjaJetpack = !pPlayer->m_NinjaJetpack;
}

void CGameContext::ConShowOthers(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;
	if (pSelf->Config()->m_SvShowOthers)
	{
		if (pResult->NumArguments())
			pPlayer->m_ShowOthers = pResult->GetInteger(0);
		else
			pPlayer->m_ShowOthers = !pPlayer->m_ShowOthers;
	}
	else
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"showotherschat",
				"Showing players from other teams is disabled");
}

void CGameContext::ConShowAll(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pResult->NumArguments())
	{
		if (pPlayer->m_ShowAll == (bool)pResult->GetInteger(0))
			return;

		pPlayer->m_ShowAll = pResult->GetInteger(0);
	}
	else
	{
		pPlayer->m_ShowAll = !pPlayer->m_ShowAll;
	}

	if (pPlayer->m_ShowAll)
		pSelf->SendChatTarget(pResult->m_ClientID, "You will now see all tees on this server, no matter the distance");
	else
		pSelf->SendChatTarget(pResult->m_ClientID, "You will no longer see all tees on this server");
}

void CGameContext::ConSpecTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pResult->NumArguments())
		pPlayer->m_SpecTeam = pResult->GetInteger(0);
	else
		pPlayer->m_SpecTeam = !pPlayer->m_SpecTeam;
}

bool CheckClientID(int ClientID)
{
	if (ClientID < 0 || ClientID >= MAX_CLIENTS)
		return false;
	return true;
}

void CGameContext::ConSayTime(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	int ClientID;
	char aBufname[MAX_NAME_LENGTH];

	if (pResult->NumArguments() > 0)
	{
		for(ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
			if (str_comp(pResult->GetString(0), pSelf->Server()->ClientName(ClientID)) == 0)
				break;

		if(ClientID == MAX_CLIENTS)
			return;

		str_format(aBufname, sizeof(aBufname), "%s's", pSelf->Server()->ClientName(ClientID));
	}
	else
	{
		str_copy(aBufname, "Your", sizeof(aBufname));
		ClientID = pResult->m_ClientID;
	}

	CPlayer *pPlayer = pSelf->m_apPlayers[ClientID];
	if (!pPlayer)
		return;
	CCharacter* pChr = pPlayer->GetCharacter();
	if (!pChr)
		return;
	if(pChr->m_DDRaceState != DDRACE_STARTED)
		return;

	char aBuftime[64];
	int IntTime = (int)((float)(pSelf->Server()->Tick() - pChr->m_StartTime)
			/ ((float)pSelf->Server()->TickSpeed()));
	str_format(aBuftime, sizeof(aBuftime), "%s time is %s%d:%s%d",
			aBufname,
			((IntTime / 60) > 9) ? "" : "0", IntTime / 60,
			((IntTime % 60) > 9) ? "" : "0", IntTime % 60);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "time", aBuftime);
}

void CGameContext::ConSayTimeAll(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;
	CCharacter* pChr = pPlayer->GetCharacter();
	if (!pChr)
		return;
	if(pChr->m_DDRaceState != DDRACE_STARTED)
		return;

	char aBuftime[64];
	int IntTime = (int)((float)(pSelf->Server()->Tick() - pChr->m_StartTime)
			/ ((float)pSelf->Server()->TickSpeed()));
	str_format(aBuftime, sizeof(aBuftime),
			"%s\'s current race time is %s%d:%s%d",
			pSelf->Server()->ClientName(pResult->m_ClientID),
			((IntTime / 60) > 9) ? "" : "0", IntTime / 60,
			((IntTime % 60) > 9) ? "" : "0", IntTime % 60);
	pSelf->SendChat(-1, CHAT_ALL, -1, aBuftime, pResult->m_ClientID);
}

void CGameContext::ConTime(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;
	CCharacter* pChr = pPlayer->GetCharacter();
	if (!pChr)
		return;

	char aBuftime[64];
	int IntTime = (int)((float)(pSelf->Server()->Tick() - pChr->m_StartTime)
			/ ((float)pSelf->Server()->TickSpeed()));
	str_format(aBuftime, sizeof(aBuftime), "Your time is %s%d:%s%d",
				((IntTime / 60) > 9) ? "" : "0", IntTime / 60,
				((IntTime % 60) > 9) ? "" : "0", IntTime % 60);
	pSelf->SendBroadcast(aBuftime, pResult->m_ClientID);
}

void CGameContext::ConRescue(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;
	CCharacter* pChr = pPlayer->GetCharacter();
	if (!pChr)
		return;

	if (!pSelf->Config()->m_SvRescue) {
		pSelf->SendChatTarget(pPlayer->GetCID(), "Rescue is not enabled on this server");
		return;
	}

	pChr->Rescue();
}

void CGameContext::ConProtectedKill(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;
	CCharacter* pChr = pPlayer->GetCharacter();
	if (!pChr)
		return;

	int CurrTime = (pSelf->Server()->Tick() - pChr->m_StartTime) / pSelf->Server()->TickSpeed();
	if(pSelf->Config()->m_SvKillProtection != 0 && CurrTime >= (60 * pSelf->Config()->m_SvKillProtection) && pChr->m_DDRaceState == DDRACE_STARTED)
	{
			pPlayer->KillCharacter(WEAPON_SELF);

			//char aBuf[64];
			//str_format(aBuf, sizeof(aBuf), "You killed yourself in: %s%d:%s%d",
			//		((CurrTime / 60) > 9) ? "" : "0", CurrTime / 60,
			//		((CurrTime % 60) > 9) ? "" : "0", CurrTime % 60);
			//pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
}

// F-DDrace

void CGameContext::ConScore(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;
	char aFormat[32];
	str_copy(aFormat, pResult->GetString(0), sizeof(aFormat));
	bool Changed = true;

	if (!str_comp_nocase(aFormat, "time"))
		pPlayer->m_ScoreMode = SCORE_TIME;
	else if (!str_comp_nocase(aFormat, "level"))
		pPlayer->m_ScoreMode = SCORE_LEVEL;
	else if (!str_comp_nocase(aFormat, "points"))
		pPlayer->m_ScoreMode = SCORE_BLOCK_POINTS;
	else
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "~~~ Score Format ~~~");
		pSelf->SendChatTarget(pResult->m_ClientID, "Use '/score <format>' to change the displayed score.");
		pSelf->SendChatTarget(pResult->m_ClientID, "time, level, points");
		Changed = false;
	}

	if (Changed)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Changed displayed score to '%s'", aFormat);
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);

		// Update the gameinfo, add or remove GAMEFLAG_RACE as wanted (time score needs it, the others dont)
		pSelf->m_pController->UpdateGameInfo(pResult->m_ClientID);
	}
}

void CGameContext::ConAccount(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pPlayer->GetAccID() < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You are not logged in");
		return;
	}

	char aBuf[128];
	time_t tmp;
	CGameContext::AccountInfo* Account = &pSelf->m_Accounts[pPlayer->GetAccID()];

	pSelf->SendChatTarget(pResult->m_ClientID, "--- Account Info ---");
	str_format(aBuf, sizeof(aBuf), "Euros: %d", (*Account).m_Euros);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);

	if ((*Account).m_VIP)
	{
		tmp = (*Account).m_ExpireDateVIP;
		str_format(aBuf, sizeof(aBuf), "VIP: until %s", pSelf->GetDate(tmp));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	else
		pSelf->SendChatTarget(pResult->m_ClientID, "VIP: not bought");

	if ((*Account).m_TeleRifle)
	{
		tmp = (*Account).m_ExpireDateTeleRifle;
		str_format(aBuf, sizeof(aBuf), "Tele Rifle: until %s", pSelf->GetDate(tmp));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	else
		pSelf->SendChatTarget(pResult->m_ClientID, "Tele Rifle: not bought");
}

void CGameContext::ConStats(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int ID = pResult->NumArguments() ? pSelf->GetCIDByName(pResult->GetString(0)) : pResult->m_ClientID;
	CPlayer* pPlayer = pSelf->m_apPlayers[ID];
	if (ID == -1 || !pPlayer)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Player not found");
		return;
	}

	char aBuf[128];
	int Minigame = pSelf->m_apPlayers[pResult->m_ClientID]->m_Minigame;
	CGameContext::AccountInfo* Account = &pSelf->m_Accounts[pPlayer->GetAccID()];

	switch (Minigame)
	{
		case MINIGAME_NONE:
		{
			str_format(aBuf, sizeof(aBuf), "--- %s's Stats ---", pSelf->Server()->ClientName(ID));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "Level [%d]%s", (*Account).m_Level, pPlayer->GetAccID() < ACC_START ? " (not logged in)" : (*Account).m_Level >= MAX_LEVEL ? " (max)" : "");
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "XP [%d/%d]", (*Account).m_XP, pSelf->m_aNeededXP[(*Account).m_Level]);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "Money [%llu]", (*Account).m_Money);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "Police [%d]%s", (*Account).m_PoliceLevel, (*Account).m_PoliceLevel >= NUM_POLICE_LEVELS ? " (max)" : "");
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);

			// dont expose euros to other players than you
			if (ID == pResult->m_ClientID)
			{
				str_format(aBuf, sizeof(aBuf), "Euros [%d]", (*Account).m_Euros);
				pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			}
		} //fallthrough

		case MINIGAME_BLOCK:
		{
			if (Minigame == MINIGAME_NONE)
			{
				pSelf->SendChatTarget(pResult->m_ClientID, "---- BLOCK ----");
			}
			else
			{
				str_format(aBuf, sizeof(aBuf), "--- %s's Block Stats ---", pSelf->Server()->ClientName(ID));
				pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			}
			str_format(aBuf, sizeof(aBuf), "Points: %d", (*Account).m_BlockPoints);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "Kills: %d", (*Account).m_Kills);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "Deaths: %d", (*Account).m_Deaths);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		} break;

		case MINIGAME_SURVIVAL:
		{
			str_format(aBuf, sizeof(aBuf), "--- %s's Survival Stats ---", pSelf->Server()->ClientName(ID));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "Wins: %d", (*Account).m_SurvivalWins);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "Kills: %d", (*Account).m_SurvivalKills);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "Deaths: %d", (*Account).m_SurvivalDeaths);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		} break;

		case MINIGAME_INSTAGIB_BOOMFNG: // fallthrough
		case MINIGAME_INSTAGIB_FNG:
		{
			str_format(aBuf, sizeof(aBuf), "--- %s's Instagib Stats ---", pSelf->Server()->ClientName(ID));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "Wins: %d", (*Account).m_InstagibWins);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "Kills: %d", (*Account).m_InstagibKills);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "Deaths: %d", (*Account).m_InstagibDeaths);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		} break;
	}
}

void CGameContext::ConSpookyGhostInfo(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	pSelf->SendChatTarget(pResult->m_ClientID, "~~~ Spooky Ghost ~~~");
	pSelf->SendChatTarget(pResult->m_ClientID, "The Spooky Ghost is an extra, that can be toggled like this:");
	pSelf->SendChatTarget(pResult->m_ClientID, "Hold TAB (or other scoreboard key) and shoot two times with your gun.");
}

void CGameContext::ConVIPInfo(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	pSelf->SendChatTarget(pResult->m_ClientID, "~~~ VIP ~~~");
	pSelf->SendChatTarget(pResult->m_ClientID, "VIP's have access to some extras. They can use following commands:");
	pSelf->SendChatTarget(pResult->m_ClientID, "rainbow, bloody, atom, trail, spreadgun, spinbot, aimclosest");
	pSelf->SendChatTarget(pResult->m_ClientID, "You can use '/room' to invite other players to the room.");
	pSelf->SendChatTarget(pResult->m_ClientID, "Also, you get 2 xp and 2 money more per second.");
}

void CGameContext::ConSpawnWeaponsInfo(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	char aBuf[256];
	pSelf->SendChatTarget(pResult->m_ClientID, "~~~ Spawn Weapons ~~~");
	pSelf->SendChatTarget(pResult->m_ClientID, "You can buy spawn weapons in the shop.");
	pSelf->SendChatTarget(pResult->m_ClientID, "You will have the bought weapon on spawn.");
	pSelf->SendChatTarget(pResult->m_ClientID, "You can have max. 5 bullets per weapon.");
	pSelf->SendChatTarget(pResult->m_ClientID, "Each bullet costs 600.000 money.");
	pSelf->SendChatTarget(pResult->m_ClientID, "~~~ Your Spawn Weapons ~~~");
	str_format(aBuf, sizeof(aBuf), "Spawn shotgun bullets: %d", pSelf->m_Accounts[pSelf->m_apPlayers[pResult->m_ClientID]->GetAccID()].m_SpawnWeapon[0]);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Spawn grenade bullets: %d", pSelf->m_Accounts[pSelf->m_apPlayers[pResult->m_ClientID]->GetAccID()].m_SpawnWeapon[1]);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Spawn rifle bullets: %d", pSelf->m_Accounts[pSelf->m_apPlayers[pResult->m_ClientID]->GetAccID()].m_SpawnWeapon[2]);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
}

void CGameContext::ConAccountInfo(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	pSelf->SendChatTarget(pResult->m_ClientID, "~~~ Accounts ~~~");
	pSelf->SendChatTarget(pResult->m_ClientID, "Accounts are used to save your stats.");
	pSelf->SendChatTarget(pResult->m_ClientID, "You can farm money and buy things in the shop, kill tees and get points.");
	pSelf->SendChatTarget(pResult->m_ClientID, "These stats will be saved inside of your account.");
	pSelf->SendChatTarget(pResult->m_ClientID, "You can create an account using '/register'.");
}

void CGameContext::ConWeaponIndicator(IConsole::IResult * pResult, void * pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	pPlayer->m_WeaponIndicator = !pPlayer->m_WeaponIndicator;

	if (pPlayer->m_WeaponIndicator)
		pSelf->SendChatTarget(pResult->m_ClientID, "Weapon indicator enabled");
	else
		pSelf->SendChatTarget(pResult->m_ClientID, "Weapon indicator disabled");
}

bool CGameContext::TryRegisterBan(const NETADDR *pAddr, int Secs)
{
	// find a matching register ban for this ip, update expiration time if found
	for(int i = 0; i < m_NumRegisterBans; i++)
	{
		if(net_addr_comp_noport(&m_aRegisterBans[i].m_Addr, pAddr) == 0)
		{
			m_aRegisterBans[i].m_LastAttempt = Server()->Tick();
			m_aRegisterBans[i].m_NumRegistrations++;

			if (m_aRegisterBans[i].m_NumRegistrations > Config()->m_SvMaxRegistrationsPerIP)
			{
				m_aRegisterBans[i].m_Expire = Server()->Tick() + Secs * Server()->TickSpeed();
				return true;
			}
			return false;
		}
	}

	// nothing to update create new one
	if(m_NumRegisterBans < MAX_REGISTER_BANS)
	{
		m_aRegisterBans[m_NumRegisterBans].m_Addr = *pAddr;
		m_aRegisterBans[m_NumRegisterBans].m_Expire = 0;
		m_aRegisterBans[m_NumRegisterBans].m_NumRegistrations = 1;
		m_aRegisterBans[m_NumRegisterBans].m_LastAttempt = Server()->Tick();
		m_NumRegisterBans++;
		return false;
	}
	// no free slot found
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "regban", "register ban array is full");
	return false;
}

int CGameContext::ProcessRegisterBan(int ClientID)
{
	if(!m_apPlayers[ClientID])
		return 0;

	NETADDR Addr;
	Server()->GetClientAddr(ClientID, &Addr);
	int RegisterBanned = 0;

	for(int i = 0; i < m_NumRegisterBans; i++)
	{
		if(net_addr_comp_noport(&Addr, &m_aRegisterBans[i].m_Addr) == 0)
		{
			RegisterBanned = (m_aRegisterBans[i].m_Expire - Server()->Tick()) / Server()->TickSpeed();
			break;
		}
	}

	if (RegisterBanned > 0)
	{
		char aBuf[128];
		str_format(aBuf, sizeof aBuf, "You are not permitted to register accounts for the next %d seconds.", RegisterBanned);
		SendChatTarget(ClientID, aBuf);
		return 1;
	}

	if (TryRegisterBan(&Addr, REGISTER_BAN_DELAY))
	{
		char aBuf[128];
		str_format(aBuf, sizeof aBuf, "You have been banned from registering accounts for %d seconds", REGISTER_BAN_DELAY);
		SendChatTarget(ClientID, aBuf);
		return 1;
	}

	return 0;
}

void CGameContext::ConRegister(IConsole::IResult * pResult, void * pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (!pSelf->Config()->m_SvAccounts)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Accounts are not supported on this server");
		return;
	}

	char aUsername[32];
	char aPassword[32];
	char aPassword2[32];
	str_copy(aUsername, pResult->GetString(0), sizeof(aUsername));
	str_copy(aPassword, pResult->GetString(1), sizeof(aPassword));
	str_copy(aPassword2, pResult->GetString(2), sizeof(aPassword2));

	if (str_length(aUsername) > 20 || str_length(aUsername) < 3)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "The username is too long or too short");
		return;
	}

	char aAllowedCharSet[64] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	bool UnallowedChar = false;

	for (int i = 0; i < str_length(aUsername); i++)
	{
		bool NoUnallowedChars = false;

		for (int j = 0; j < str_length(aAllowedCharSet); j++)
		{
			if (aUsername[i] == aAllowedCharSet[j])
			{
				NoUnallowedChars = true;
				break;
			}
		}

		if (!NoUnallowedChars)
		{
			UnallowedChar = true;
			break;
		}
	}
	if (UnallowedChar)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Your username can only consist of letters and numbers");
		return;
	}

	if (str_comp_nocase(aPassword, aPassword2) != 0)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "The passwords need to be identical");
		return;
	}

	if (str_length(aPassword) > 20 || str_length(aPassword) < 3 || str_length(aPassword2) > 20 || str_length(aPassword2) < 3)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "The password is too long or too short");
		return;
	}

	int ID = pSelf->AddAccount();
	pSelf->ReadAccountStats(ID, aUsername);

	if (!str_comp_nocase(pSelf->m_Accounts[ID].m_Username, aUsername))
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Username already exsists");
		pSelf->FreeAccount(ID);
		return;
	}

	// we reset the account again and read it again to get the correct values
	pSelf->FreeAccount(ID, true);

	// process register spam protection before really adding the account
	if (pSelf->ProcessRegisterBan(pResult->m_ClientID))
		return;

	ID = pSelf->AddAccount();

	str_copy(pSelf->m_Accounts[ID].m_Password, aPassword, sizeof(pSelf->m_Accounts[ID].m_Password));
	str_copy(pSelf->m_Accounts[ID].m_Username, aUsername, sizeof(pSelf->m_Accounts[ID].m_Username));
	str_copy(pSelf->m_Accounts[ID].m_aLastPlayerName, pSelf->Server()->ClientName(pResult->m_ClientID), sizeof(pSelf->m_Accounts[ID].m_aLastPlayerName));

	// also update topaccounts
	pSelf->SetTopAccStats(ID);

	pSelf->Logout(ID);

	pSelf->SendChatTarget(pResult->m_ClientID, "Successfully registered an account, you can login now");
	dbg_msg("acc", "account created, file '%s/%s.acc'", pSelf->Config()->m_SvAccFilePath, aUsername);
}

void CGameContext::ConLogin(IConsole::IResult * pResult, void * pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (!pSelf->Config()->m_SvAccounts)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Accounts are not supported on this server");
		return;
	}

	char aUsername[32];
	char aPassword[128];
	str_copy(aUsername, pResult->GetString(0), sizeof(aUsername));
	str_copy(aPassword, pResult->GetString(1), sizeof(aPassword));

	if (pPlayer->GetAccID() >= ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You are already logged in");
		return;
	}

	int ID = pSelf->AddAccount();
	pSelf->ReadAccountStats(ID, aUsername);

	if (pSelf->m_Accounts[ID].m_Username[0] == '\0')
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "That account doesn't exist, please register first");
		pSelf->FreeAccount(ID);
		return;
	}

	if (pSelf->m_Accounts[ID].m_LoggedIn)
	{
		if (pSelf->m_Accounts[ID].m_Port == pSelf->Config()->m_SvPort)
			pSelf->SendChatTarget(pResult->m_ClientID, "This account is already logged in");
		else
			pSelf->SendChatTarget(pResult->m_ClientID, "This account is already logged in on another server");
		pSelf->FreeAccount(ID);
		return;
	}

	if (pSelf->m_Accounts[ID].m_Disabled)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "This account is disabled");
		pSelf->FreeAccount(ID);
		return;
	}

	if (str_comp(pSelf->m_Accounts[ID].m_Password, aPassword))
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Wrong password");
		pSelf->FreeAccount(ID);
		return;
	}

	pSelf->m_Accounts[ID].m_Port = pSelf->Config()->m_SvPort;
	pSelf->m_Accounts[ID].m_LoggedIn = true;
	pSelf->m_Accounts[ID].m_ClientID = pResult->m_ClientID;
	str_copy(pSelf->m_Accounts[ID].m_aLastPlayerName, pSelf->Server()->ClientName(pResult->m_ClientID), sizeof(pSelf->m_Accounts[ID].m_aLastPlayerName));
	pSelf->WriteAccountStats(ID);

	pSelf->SendChatTarget(pResult->m_ClientID, "Successfully logged in");
	pSelf->m_apPlayers[pResult->m_ClientID]->OnLogin();
}

void CGameContext::ConLogout(IConsole::IResult * pResult, void * pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (!pSelf->Config()->m_SvAccounts && pPlayer->GetAccID() < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Accounts are not supported on this server");
		return;
	}

	if (pPlayer->GetAccID() < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You are not logged in");
		return;
	}

	pSelf->Logout(pPlayer->GetAccID());
}

void CGameContext::ConChangePassword(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (!pSelf->Config()->m_SvAccounts)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Accounts are not supported on this server");
		return;
	}

	if (pPlayer->GetAccID() < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You are not logged in");
		return;
	}

	if (str_comp(pSelf->m_Accounts[pPlayer->GetAccID()].m_Password, pResult->GetString(0)))
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Wrong password");
		return;
	}

	if (str_comp(pResult->GetString(1), pResult->GetString(2)))
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "The passwords need to be identical");
		return;
	}

	str_copy(pSelf->m_Accounts[pPlayer->GetAccID()].m_Password, pResult->GetString(1), sizeof(pSelf->m_Accounts[pPlayer->GetAccID()].m_Password));
	pSelf->WriteAccountStats(pPlayer->GetAccID());
	pSelf->SendChatTarget(pResult->m_ClientID, "Successfully changed password");
}

void CGameContext::ConPayMoney(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (!pSelf->Config()->m_SvAccounts)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Accounts are not supported on this server");
		return;
	}
	if (pPlayer->GetAccID() < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You are not logged in");
		return;
	}

	int ID = pSelf->GetCIDByName(pResult->GetString(1));
	CPlayer* pTo = pSelf->m_apPlayers[ID];
	if (ID == -1 || !pTo)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "That player doesn't exist");
		return;
	}
	if (pTo->GetCID() == pResult->m_ClientID)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You can't pay money to yourself");
		return;
	}
	if (pTo->GetAccID() < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "That player is not logged in");
		return;
	}
	if (pSelf->m_Accounts[pPlayer->GetAccID()].m_Money < pResult->GetInteger(0))
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You don't have enough money");
		return;
	}
	if (pResult->GetInteger(0) <= 0)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You can't pay nothing");
		return;
	}

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "paid %d money to '%s'", pResult->GetInteger(0), pSelf->Server()->ClientName(pTo->GetCID()));
	pPlayer->MoneyTransaction(-pResult->GetInteger(0), aBuf);

	str_format(aBuf, sizeof(aBuf), "got %d money from '%s'", pResult->GetInteger(0), pSelf->Server()->ClientName(pResult->m_ClientID));
	pTo->MoneyTransaction(pResult->GetInteger(0), aBuf);

	str_format(aBuf, sizeof(aBuf), "You paid %d money to '%s'", pResult->GetInteger(0), pSelf->Server()->ClientName(pTo->GetCID()));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);

	str_format(aBuf, sizeof(aBuf), "You got %d money from '%s'", pResult->GetInteger(0), pSelf->Server()->ClientName(pResult->m_ClientID));
	pSelf->SendChatTarget(pTo->GetCID(), aBuf);
}

void CGameContext::ConMoney(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (!pSelf->Config()->m_SvAccounts)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "Accounts are not supported on this server");
		return;
	}

	if (pPlayer->GetAccID() < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You are not logged in");
		return;
	}

	char aBuf[256];
	pSelf->SendChatTarget(pResult->m_ClientID, "~~~~~~~~~~");
	str_format(aBuf, sizeof(aBuf), "Money: %llu", pSelf->m_Accounts[pPlayer->GetAccID()].m_Money);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "Euros: %d", pSelf->m_Accounts[pPlayer->GetAccID()].m_Euros);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	pSelf->SendChatTarget(pResult->m_ClientID, "~~~~~~~~~~");
	for (int i = 0; i < 5; i++)
		pSelf->SendChatTarget(pResult->m_ClientID, pSelf->m_Accounts[pPlayer->GetAccID()].m_aLastMoneyTransaction[i]);
	pSelf->SendChatTarget(pResult->m_ClientID, "~~~~~~~~~~");
}

void CGameContext::ConRoom(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (!pSelf->m_Accounts[pPlayer->GetAccID()].m_VIP)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You are not VIP");
		return;
	}

	int ID = pSelf->GetCIDByName(pResult->GetString(1));
	CCharacter* pChr = pSelf->GetPlayerChar(ID);

	char aBuf[128];
	char aCmd[64];
	str_copy(aCmd, pResult->GetString(0), sizeof(aCmd));

	if (!str_comp_nocase(aCmd, "invite"))
	{
		if (ID == -1 || !pChr)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, "That player doesn't exist");
			return;
		}
		else if (!pPlayer->m_HasRoomKey)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, "You don't have a room key");
			return;
		}
		else if (pChr->GetPlayer()->m_HasRoomKey)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, "This player has a key already");
			return;
		}
		else if (pChr->Core()->m_MoveRestrictionExtra.m_CanEnterRoom)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, "This player got invited already");
			return;
		}

		pChr->Core()->m_MoveRestrictionExtra.m_CanEnterRoom = true;
		str_format(aBuf, sizeof(aBuf), "'%s' invited you to the room", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(pChr->GetPlayer()->GetCID(), aBuf);

		str_format(aBuf, sizeof(aBuf), "You invited '%s' to the room", pSelf->Server()->ClientName(pChr->GetPlayer()->GetCID()));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	else if (!str_comp_nocase(aCmd, "kick"))
	{
		if (ID == -1 || !pChr)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, "That player doesn't exist");
			return;
		}
		else if (!pPlayer->m_HasRoomKey)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, "You can't kick others without a key");
			return;
		}
		else if (pChr->GetPlayer()->m_HasRoomKey)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, "You can't kick a player with a key");
			return;
		}
		else if (!pChr->Core()->m_MoveRestrictionExtra.m_CanEnterRoom)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, "This player is not invited");
			return;
		}

		pChr->Core()->m_MoveRestrictionExtra.m_CanEnterRoom = false;
		str_format(aBuf, sizeof(aBuf), "'%s' kicked you out of room", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(pChr->GetPlayer()->GetCID(), aBuf);

		str_format(aBuf, sizeof(aBuf), "You kicked '%s' out of room", pSelf->Server()->ClientName(pChr->GetPlayer()->GetCID()));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
}

void CGameContext::ConSpawn(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	CCharacter *pChr = pPlayer->GetCharacter();
	if (!pPlayer || !pChr)
		return;

	if (pPlayer->GetAccID() < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You are not logged in");
		return;
	}

	if (pPlayer->m_Minigame != MINIGAME_NONE)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You can't use this command in minigames");
		return;
	}

	if (pChr->m_FreezeTime)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You can't use this command while frozen");
		return;
	}

	if (pSelf->m_Accounts[pSelf->m_apPlayers[pResult->m_ClientID]->GetAccID()].m_Money < 2000000)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You need at least 2.000.000 money to use this command");
		return;
	}

	vec2 Pos = pSelf->Collision()->GetRandomTile(ENTITY_SPAWN);
	if (Pos == vec2(-1, -1))
		return;

	pSelf->SendChatTarget(pResult->m_ClientID, "You lost 50.000 money for teleporting to spawn");
	pPlayer->MoneyTransaction(-50000, "-50000 (teleported to spawn)");
	pChr->Core()->m_Pos = Pos;

	// create death effect and do a nice sound when teleporting to spawn
	int64_t TeamMask = pChr->Teams()->TeamMask(pChr->Team(), -1, pResult->m_ClientID);
	pSelf->CreateDeath(Pos, pResult->m_ClientID, TeamMask);
	pSelf->CreateSound(Pos, SOUND_WEAPON_SPAWN, TeamMask);
}

void CGameContext::ConPoliceInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	int Page = pResult->GetInteger(0);
	int MaxPages = 4;	//////UPDATE THIS WITH EVERY PAGE YOU ADD
	if (!Page || Page > MaxPages)
		Page = 1;

	char aInfo[128];
	char aPage[128];
	str_format(aInfo, sizeof(aInfo), "Use '/policeinfo <page>' to check out what other police ranks can do.");
	str_format(aPage, sizeof(aPage), "-- Page %d/%d --", Page, MaxPages);

	pSelf->SendChatTarget(pResult->m_ClientID, "~~~ Police Info ~~~");
	if (Page >= 2 && Page <= MaxPages)
	{
		int Level = 0;
		int Policelevel = Page - 1;
		char aPolice[32];

		if (Policelevel == 1)
			Level = 18;
		else if (Policelevel == 2)
			Level = 25;
		else if (Policelevel == 3)
			Level = 30;

		str_format(aPolice, sizeof(aPolice), "[POLICE %d]", Policelevel);
		pSelf->SendChatTarget(pResult->m_ClientID, aPolice);

		str_format(aPolice, sizeof(aPolice), "Level needed to buy: [LVL %d]", Level);
		pSelf->SendChatTarget(pResult->m_ClientID, aPolice);

		pSelf->SendChatTarget(pResult->m_ClientID, "Benefits:");
		if (Policelevel == 1)
			pSelf->SendChatTarget(pResult->m_ClientID, "- The police bot will help you");
		else if (Policelevel == 2)
			pSelf->SendChatTarget(pResult->m_ClientID, "- '/policehelper'");
		else if (Policelevel == 3)
			pSelf->SendChatTarget(pResult->m_ClientID, "- taser license ('/taserinfo')");
	}
	else
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "[GENERAL INFORMATION]");
		pSelf->SendChatTarget(pResult->m_ClientID, "Police can be bought in shop using '/buy police'.");
		pSelf->SendChatTarget(pResult->m_ClientID, "There are multiple police ranks, each cost 100.000 money.");
		pSelf->SendChatTarget(pResult->m_ClientID, "The policebot will help every police officer.");
		pSelf->SendChatTarget(pResult->m_ClientID, "Every police rank will give you more benefits.");
	}
	pSelf->SendChatTarget(pResult->m_ClientID, "------------------------");
	pSelf->SendChatTarget(pResult->m_ClientID, "Use '/policeinfo <page>' for information about other ranks");
	pSelf->SendChatTarget(pResult->m_ClientID, aPage);
}

void CGameContext::ConResumeMoved(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pResult->NumArguments())
	{
		if (pPlayer->m_ResumeMoved == (bool)pResult->GetInteger(0))
			return;

		pPlayer->m_ResumeMoved = pResult->GetInteger(0);
	}
	else
	{
		pPlayer->m_ResumeMoved = !pPlayer->m_ResumeMoved;
	}

	if (pPlayer->m_ResumeMoved)
		pSelf->SendChatTarget(pResult->m_ClientID, "You will now resume from pause if your tee gets moved");
	else
		pSelf->SendChatTarget(pResult->m_ClientID, "You will no longer resume from pause if your tee gets moved");
}

void CGameContext::ConMinigames(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aMinigames[256];
	char aTemp2[256];
	aMinigames[0] = 0;
	aTemp2[0] = 0;
	for (int i = MINIGAME_BLOCK; i < NUM_MINIGAMES; i++)
	{
		if (i != MINIGAME_BLOCK)
			str_format(aTemp2, sizeof(aTemp2), "%s, ", aMinigames);
		str_format(aMinigames, sizeof(aMinigames), "%s%s", aTemp2, pSelf->GetMinigameCommand(i));
	}

	pSelf->SendChatTarget(pResult->m_ClientID, "~~~ Minigames ~~~");
	pSelf->SendChatTarget(pResult->m_ClientID, "You can join any minigame using '/<minigame>'");
	pSelf->SendChatTarget(pResult->m_ClientID, "To leave a minigame, just type '/leave'");
	pSelf->SendChatTarget(pResult->m_ClientID, "Here is a list of all minigames:");
	pSelf->SendChatTarget(pResult->m_ClientID, aMinigames);
}

void CGameContext::SetMinigame(IConsole::IResult *pResult, void *pUserData, int Minigame)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer* pPlayer = m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	char aMsg[128];

	// admins can enable or disable minigames with /<minigame> <enable/disable>
	if (pResult->NumArguments() && pSelf->Server()->GetAuthedState(pResult->m_ClientID) && Minigame != MINIGAME_NONE)
	{
		bool Disable;
		if (!str_comp_nocase(pResult->GetString(0), "enable"))
			Disable = false;
		else if (!str_comp_nocase(pResult->GetString(0), "disable"))
			Disable = true;
		else return;

		str_format(aMsg, sizeof(aMsg), "Minigame '%s' %s%sd", pSelf->GetMinigameName(Minigame), (pSelf->m_aMinigameDisabled[Minigame] == Disable ? "is already " : ""), pResult->GetString(0));
		pSelf->SendChatTarget(pResult->m_ClientID, aMsg);

		pSelf->m_aMinigameDisabled[Minigame] = Disable;
		return;
	}

	// check whether minigame is disabled
	if (Minigame != MINIGAME_NONE && pSelf->m_aMinigameDisabled[Minigame])
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "This minigame is disabled");
		return;
	}

	// check if we are already in a minigame
	if (pPlayer->m_Minigame == Minigame)
	{
		// you can't leave when you're not in a minigame
		if (Minigame == MINIGAME_NONE)
			pSelf->SendChatTarget(pResult->m_ClientID, "You are not in a minigame");
		else
		{
			str_format(aMsg, sizeof(aMsg), "You are already in minigame '%s'", pSelf->GetMinigameName(Minigame));
			pSelf->SendChatTarget(pResult->m_ClientID, aMsg);
		}
		return;
	}

	// leave minigame
	if (Minigame == MINIGAME_NONE)
	{
		str_format(aMsg, sizeof(aMsg), "'%s' left the minigame '%s'", pSelf->Server()->ClientName(pResult->m_ClientID), pSelf->GetMinigameName(pPlayer->m_Minigame));
		pSelf->SendChat(-1, CHAT_ALL, -1, aMsg);

		//reset everything
		if (pPlayer->m_Minigame == MINIGAME_SURVIVAL)
		{
			pPlayer->m_Gamemode = pSelf->Config()->m_SvVanillaModeStart ? GAMEMODE_VANILLA : GAMEMODE_DDRACE;
			pPlayer->m_SurvivalState = SURVIVAL_OFFLINE;
			pPlayer->m_ShowName = true;
		}
	}
	// join minigame
	else if (pPlayer->m_Minigame == MINIGAME_NONE)
	{
		str_format(aMsg, sizeof(aMsg), "'%s' joined the minigame '%s', use '/%s' to join aswell", pSelf->Server()->ClientName(pResult->m_ClientID), pSelf->GetMinigameName(Minigame), pSelf->GetMinigameCommand(Minigame));
		pSelf->SendChat(-1, CHAT_ALL, -1, aMsg);
		pSelf->SendChatTarget(pResult->m_ClientID, "Say '/leave' to join the normal area again");

		//set minigame required stuff
		((CGameControllerDDRace*)pSelf->m_pController)->m_Teams.SetCharacterTeam(pPlayer->GetCID(), 0);

		if (Minigame == MINIGAME_SURVIVAL)
		{
			pPlayer->m_Gamemode = GAMEMODE_VANILLA;
			pPlayer->m_SurvivalState = SURVIVAL_LOBBY;
		}
	}
	else
	{
		// you can't join minigames if you are already in another mingame
		pSelf->SendChatTarget(pResult->m_ClientID, "You have to leave first in order to join another minigame");
		return;
	}

	pPlayer->KillCharacter(WEAPON_GAME);
	pPlayer->m_Minigame = Minigame;

	pSelf->UpdateHidePlayers();

	// Update the gameinfo, add or remove GAMEFLAG_RACE as wanted (in minigames we disable it to properly show the scores)
	pSelf->m_pController->UpdateGameInfo(pResult->m_ClientID);
}

void CGameContext::ConLeaveMinigame(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SetMinigame(pResult, pUserData, MINIGAME_NONE);
}

void CGameContext::ConJoinBlock(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SetMinigame(pResult, pUserData, MINIGAME_BLOCK);
}

void CGameContext::ConJoinSurvival(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SetMinigame(pResult, pUserData, MINIGAME_SURVIVAL);
}

void CGameContext::ConJoinBoomFNG(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SetMinigame(pResult, pUserData, MINIGAME_INSTAGIB_BOOMFNG);
}

void CGameContext::ConJoinFNG(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SetMinigame(pResult, pUserData, MINIGAME_INSTAGIB_FNG);
}

void CGameContext::SendTop5AccMessage(IConsole::IResult* pResult, void* pUserData, int Type)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	char aType[8];

	pSelf->UpdateTopAccounts(Type);

	char aBuf[512];
	int Debut = pResult->NumArguments() >= 1 && pResult->GetInteger(0) != 0 ? pResult->GetInteger(0) : 1;
	Debut = max(1, Debut < 0 ? (int)pSelf->m_TopAccounts.size() + Debut - 3 : Debut);

	str_format(aType, sizeof(aType), "%s", Type == TOP_LEVEL ? "Level" : Type == TOP_POINTS ? "Points" : Type == TOP_MONEY ? "Money" : "Spree");
	str_format(aBuf, sizeof(aBuf), "----------- Top 5 %s -----------", aType);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	for (int i = 0; i < 5; i++)
	{
		if (i + Debut > (int)pSelf->m_TopAccounts.size())
			break;
		CGameContext::TopAccounts* r = &pSelf->m_TopAccounts[i + Debut - 1];

		if (Type == TOP_MONEY)
			str_format(aBuf, sizeof(aBuf), "%d. %s Money: %llu", i + Debut, r->m_aUsername, r->m_Money);
		else
			str_format(aBuf, sizeof(aBuf), "%d. %s %s: %d", i + Debut, r->m_aUsername, aType, Type == TOP_LEVEL ? r->m_Level : Type == TOP_POINTS ? r->m_Points : r->m_KillStreak);

		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	pSelf->SendChatTarget(pResult->m_ClientID, "----------------------------------------");
}

void CGameContext::ConTop5Level(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->SendTop5AccMessage(pResult, pUserData, TOP_LEVEL);
}

void CGameContext::ConTop5Points(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->SendTop5AccMessage(pResult, pUserData, TOP_POINTS);
}

void CGameContext::ConTop5Money(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->SendTop5AccMessage(pResult, pUserData, TOP_MONEY);
}

void CGameContext::ConTop5Spree(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->SendTop5AccMessage(pResult, pUserData, TOP_SPREE);
}

void CGameContext::ConPoliceHelper(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pSelf->m_Accounts[pPlayer->GetAccID()].m_PoliceLevel < 2)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You need to be police level 2 to use this command");
		return;
	}

	int ID = pSelf->GetCIDByName(pResult->GetString(1));
	CCharacter* pChr = pSelf->GetPlayerChar(ID);

	char aBuf[128];
	char aCmd[64];
	str_copy(aCmd, pResult->GetString(0), sizeof(aCmd));

	if (!str_comp_nocase(aCmd, "add"))
	{
		if (ID == -1 || !pChr)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, "That player doesn't exist");
			return;
		}
		else if (pSelf->m_Accounts[pChr->GetPlayer()->GetAccID()].m_PoliceLevel)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, "This player is a police officer");
			return;
		}
		else if (pChr->m_PoliceHelper)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, "This player is a police helper already");
			return;
		}

		pChr->m_PoliceHelper = true;
		str_format(aBuf, sizeof(aBuf), "'%s' added you to the police helpers", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(pChr->GetPlayer()->GetCID(), aBuf);

		str_format(aBuf, sizeof(aBuf), "You added '%s' to the police helpers", pSelf->Server()->ClientName(pChr->GetPlayer()->GetCID()));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	else if (!str_comp_nocase(aCmd, "remove"))
	{
		if (ID == -1 || !pChr)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, "That player doesn't exist");
			return;
		}
		else if (pSelf->m_Accounts[pChr->GetPlayer()->GetAccID()].m_PoliceLevel)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, "This player is a police officer");
			return;
		}
		else if (!pChr->m_PoliceHelper)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, "This player is not a police helper");
			return;
		}

		pChr->m_PoliceHelper = false;
		str_format(aBuf, sizeof(aBuf), "'%s' removed you from the police helpers", pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(pChr->GetPlayer()->GetCID(), aBuf);

		str_format(aBuf, sizeof(aBuf), "You removed '%s' from the police helpers", pSelf->Server()->ClientName(pChr->GetPlayer()->GetCID()));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
}

void CGameContext::ConTaserInfo(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	char aBuf[128];
	CGameContext::AccountInfo* Account = &pSelf->m_Accounts[pSelf->m_apPlayers[pResult->m_ClientID]->GetAccID()];

	pSelf->SendChatTarget(pResult->m_ClientID, "~~~ Taser ~~~");
	pSelf->SendChatTarget(pResult->m_ClientID, "Police officers with level 3 or higher get a taser license.");
	pSelf->SendChatTarget(pResult->m_ClientID, "The taser is a rifle that freezes players for a short time.");
	pSelf->SendChatTarget(pResult->m_ClientID, "The taser relies on the normal rifle, if you loose it, you loose the taser.");
	pSelf->SendChatTarget(pResult->m_ClientID, "~~~ Your taser stats ~~~");
	str_format(aBuf, sizeof(aBuf), "Taser level: %d/%d", (*Account).m_TaserLevel, NUM_TASER_LEVELS);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	if ((*Account).m_TaserLevel < NUM_TASER_LEVELS)
	{
		str_format(aBuf, sizeof(aBuf), "Price for the next level: %d", pSelf->m_aTaserPrice[(*Account).m_TaserLevel]);
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	str_format(aBuf, sizeof(aBuf), "FreezeTime: 0.%d seconds", (*Account).m_TaserLevel * 10);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
}

void CGameContext::ConRainbowVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (!pChr)
		return;

	if (!pSelf->m_Accounts[pChr->GetPlayer()->GetAccID()].m_VIP)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You are not VIP");
		return;
	}

	pChr->Rainbow(!(pChr->m_Rainbow || pChr->GetPlayer()->m_InfRainbow), pResult->m_ClientID);
}

void CGameContext::ConBloodyVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (!pChr)
		return;

	if (!pSelf->m_Accounts[pChr->GetPlayer()->GetAccID()].m_VIP)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You are not VIP");
		return;
	}

	pChr->Bloody(!(pChr->m_Bloody || pChr->m_StrongBloody), pResult->m_ClientID);
}

void CGameContext::ConAtomVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (!pChr)
		return;

	if (!pSelf->m_Accounts[pChr->GetPlayer()->GetAccID()].m_VIP)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You are not VIP");
		return;
	}

	pChr->Atom(!pChr->m_Atom, pResult->m_ClientID);
}

void CGameContext::ConTrailVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (!pChr)
		return;

	if (!pSelf->m_Accounts[pChr->GetPlayer()->GetAccID()].m_VIP)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You are not VIP");
		return;
	}

	pChr->Trail(!pChr->m_Trail, pResult->m_ClientID);
}

void CGameContext::ConSpreadGunVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter *pChr = pSelf->GetPlayerChar(Victim);
	if (!pChr)
		return;

	if (!pSelf->m_Accounts[pChr->GetPlayer()->GetAccID()].m_VIP)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You are not VIP");
		return;
	}

	pChr->SpreadWeapon(WEAPON_GUN, !pChr->m_aSpreadWeapon[WEAPON_GUN], pResult->m_ClientID);
}

void CGameContext::ConAimClosestVIP(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (!pChr)
		return;

	if (!pSelf->m_Accounts[pChr->GetPlayer()->GetAccID()].m_VIP)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You are not VIP");
		return;
	}

	pChr->AimClosest(!pChr->Core()->m_AimClosest, pResult->m_ClientID);
}

void CGameContext::ConSpinBotVIP(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Victim = pResult->NumArguments() ? pResult->GetVictim() : pResult->m_ClientID;
	CCharacter* pChr = pSelf->GetPlayerChar(Victim);
	if (!pChr)
		return;

	if (!pSelf->m_Accounts[pChr->GetPlayer()->GetAccID()].m_VIP)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, "You are not VIP");
		return;
	}

	pChr->SpinBot(!pChr->Core()->m_SpinBot, pResult->m_ClientID);
}
