/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "gamecontext.h"
#include <engine/engine.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <game/server/teams.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/version.h>
#include <game/server/entities/character.h>
#include <stdio.h>
#include "score.h"
#include <engine/server/server.h>

bool CheckClientID(int ClientID);

void CGameContext::ConCredits(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		pPlayer->Localize("F-DDrace is a mod by fokkonaut"));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		pPlayer->Localize("This mod is based on Teeworlds 0.7 and also incorporates features, ideas, and partial implementations"));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		pPlayer->Localize("from DDNet++ by ChillerDragon, DDNet, and other community projects, aswell as many custom features!"));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		pPlayer->Localize("If you want to check out the code or contribute, feel free to check out F-DDrace on GitHub:"));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"https://github.com/fokkonaut/F-DDrace");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits", pPlayer->Localize("English translation by the developers"));
}

void CGameContext::ConInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "F-DDrace Mod. Version: " GAME_VERSION ", by fokkonaut");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", "Source: https://github.com/fokkonaut/F-DDrace");
	if(GIT_SHORTREV_HASH)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Git revision hash: %s", GIT_SHORTREV_HASH);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", aBuf);
	}
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", pPlayer->Localize("For more info, say '/cmdlist' or '/credits'"));
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
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;
	if (pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help",
				pPlayer->Localize("/cmdlist will show a list of all chat commands"));
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help",
				pPlayer->Localize("/help + any command will show you the help for this command"));
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help",
				pPlayer->Localize("Example /help settings will display the help about /settings"));
	}
	else
	{
		const char *pArg = pResult->GetString(0);
		const IConsole::CCommandInfo *pCmdInfo =
				pSelf->Console()->GetCommandInfo(pArg, CFGFLAG_CHAT, false);
		if (pCmdInfo)
		{
			if (pCmdInfo->m_pParams)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "%s: %s %s", pPlayer->Localize("Usage"), pCmdInfo->m_pName, pCmdInfo->m_pParams);
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help", aBuf);
			}

			if (pCmdInfo->m_pHelp)
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help", pCmdInfo->m_pHelp);
		}
		else
			pSelf->Console()->Print(
					IConsole::OUTPUT_LEVEL_STANDARD,
					"help",
					pPlayer->Localize("Command is either unknown or you have given a blank command without any parameters."));
	}
}

void CGameContext::ConSettings(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
				pPlayer->Localize("to check a server setting say /settings and setting's name, setting names are:"));
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
				"teams, collision, hooking, endlesshooking, me, ");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings",
				"hitting, oldlaser, timeout, votes, pause and scores");
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
		else if (str_comp(pArg, "timeout") == 0)
		{
			str_format(aBuf, sizeof(aBuf), "The Server Timeout is currently set to %d seconds", pSelf->Config()->m_ConnTimeout);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings", aBuf);
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
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't join the spectators while you are in survival"));
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
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't join the spectators while you are in survival"));
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

void CGameContext::ConPractice(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if(!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;

	if(!pSelf->Config()->m_SvPractice)
	{
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"print",
				"Practice mode is disabled");
		return;
	}

	CGameTeams &Teams = ((CGameControllerDDRace*) pSelf->m_pController)->m_Teams;

	int Team = Teams.m_Core.Team(pResult->m_ClientID);

	if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
	{
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"print",
				"Join a team to enable practice mode, which means you can use /r, but can't earn a rank.");
		return;
	}

	if(Teams.IsPractice(Team))
	{
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"print",
				"Team is already in practice mode");
		return;
	}

	bool VotedForPractice = pResult->NumArguments() == 0 || pResult->GetInteger(0);

	if(VotedForPractice == pPlayer->m_VotedForPractice)
		return;

	pPlayer->m_VotedForPractice = VotedForPractice;

	int NumCurrentVotes = 0;
	int TeamSize = 0;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(Teams.m_Core.Team(i) == Team)
		{
			CPlayer *pPlayer2 = pSelf->m_apPlayers[i];
			if(pPlayer2 && pPlayer2->m_VotedForPractice)
				NumCurrentVotes++;
			TeamSize++;
		}
	}

	int NumRequiredVotes = TeamSize / 2 + 1;

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "'%s' voted to %s /practice mode for your team, which means you can use /r, but you can't earn a rank. Type /practice to vote (%d/%d required votes)", pSelf->Server()->ClientName(pResult->m_ClientID), VotedForPractice ? "enable" : "disable", NumCurrentVotes, NumRequiredVotes);
	pSelf->SendChatTeam(Team, aBuf);

	if(NumCurrentVotes >= NumRequiredVotes)
	{
		Teams.EnablePractice(Team);
		pSelf->SendChatTeam(Team, "Practice mode enabled for your team, happy practicing!");
	}
}

void CGameContext::ConTimeout(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	const char* pTimeout = pResult->NumArguments() > 0 ? pResult->GetString(0) : pPlayer->m_TimeoutCode;
	if (str_length(pTimeout) < 8)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Timeout code has to be at least 8 characters long, make sure to use a safe code."));
		return;
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if (i == pResult->m_ClientID) continue;
		if (!pSelf->m_apPlayers[i]) continue;
		if (str_comp(pSelf->m_apPlayers[i]->m_TimeoutCode, pTimeout)) continue;

		if (pSelf->Server()->SetTimedOut(i, pResult->m_ClientID))
		{
			if (pSelf->m_apPlayers[i]->GetCharacter())
				pSelf->SendTuningParams(i, pSelf->m_apPlayers[i]->GetCharacter()->m_TuneZone);
			return;
		}
	}

	pSelf->Server()->SetTimeoutProtected(pResult->m_ClientID);
	str_copy(pPlayer->m_TimeoutCode, pResult->GetString(0), sizeof(pPlayer->m_TimeoutCode));
	pSelf->CheckLoadPlayer(pResult->m_ClientID);
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
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if(pSelf->Config()->m_SvTeam == 0 || pSelf->Config()->m_SvTeam == 3)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "lock",
				pPlayer->Localize("Teams are disabled"));
		return;
	}

	int Team = ((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.m_Core.Team(pResult->m_ClientID);

	bool Lock = ((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.TeamLocked(Team);

	if (pResult->NumArguments() > 0)
		Lock = !pResult->GetInteger(0);

	if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
	{
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"print",
				pPlayer->Localize("This team can't be locked"));
		return;
	}

	if(Lock)
	{
		((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.SetTeamLock(Team, false);
		pSelf->SendChatTeamFormat(Team, Localizable("'%s' unlocked your team."), pSelf->Server()->ClientName(pResult->m_ClientID));
	}
	else if(!pSelf->Config()->m_SvTeamLock)
	{
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"print",
				pPlayer->Localize("Team locking is disabled on this server"));
	}
	else
	{
		((CGameControllerDDRace*) pSelf->m_pController)->m_Teams.SetTeamLock(Team, true);

		pSelf->SendChatTeamFormat(Team, Localizable("'%s' locked your team. After the race started killing will kill everyone in your team."), pSelf->Server()->ClientName(pResult->m_ClientID));
	}
}

void CGameContext::ConInviteTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CGameControllerDDRace*pController = (CGameControllerDDRace*)pSelf->m_pController;
	const char *pName = pResult->GetString(0);
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if(pSelf->Config()->m_SvTeam == 0 || pSelf->Config()->m_SvTeam == 3)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
				pPlayer->Localize("Teams are disabled"));
		return;
	}

	if(!pSelf->Config()->m_SvInvite)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "invite", pPlayer->Localize("Invites are disabled"));
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
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "invite", pPlayer->Localize("Player not found"));
			return;
		}

		if(pController->m_Teams.IsInvited(Team, Target))
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "invite", pPlayer->Localize("Player already invited"));
			return;
		}

		if(pPlayer->m_LastInvited + pSelf->Config()->m_SvInviteFrequency * pSelf->Server()->TickSpeed() > pSelf->Server()->Tick())
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "invite", pPlayer->Localize("Can't invite this quickly"));
			return;
		}

		pController->m_Teams.SetClientInvited(Team, Target, true);
		pPlayer->m_LastInvited = pSelf->Server()->Tick();

		char aBuf[512];
		str_format(aBuf, sizeof aBuf, pSelf->m_apPlayers[Target]->Localize("'%s' invited you to team %d."), pSelf->Server()->ClientName(pResult->m_ClientID), Team);
		pSelf->SendChatTarget(Target, aBuf);

		pSelf->SendChatTeamFormat(Team, Localizable("'%s' invited '%s' to your team."), pSelf->Server()->ClientName(pResult->m_ClientID), pSelf->Server()->ClientName(Target));
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
				pPlayer->Localize("You are running a vote please try again after the vote is done!"));
		return;
	}
	else if (pSelf->Config()->m_SvTeam == 2 && pResult->GetInteger(0) == 0 && pPlayer->GetCharacter() && pPlayer->GetCharacter()->m_LastStartWarning < pSelf->Server()->Tick() - 3 * pSelf->Server()->TickSpeed())
	{
		pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"join",
				pPlayer->Localize("You must join a team and play with somebody or else you can't play"));
		pPlayer->GetCharacter()->m_LastStartWarning = pSelf->Server()->Tick();
	}

	if (pResult->NumArguments() > 0)
	{
		if (pPlayer->GetCharacter() == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
					pPlayer->Localize("You can't change teams while you are dead/a spectator."));
		}
		else
		{
			int Team = pResult->GetInteger(0);

			if (pPlayer->m_Last_Team
					+ pSelf->Server()->TickSpeed()
					* pSelf->Config()->m_SvTeamChangeDelay
					> pSelf->Server()->Tick())
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
						pPlayer->Localize("You can't change teams that fast!"));
			}
			else if (pPlayer->IsMinigame() && pPlayer->m_Minigame != MINIGAME_BLOCK && pResult->NumArguments() > 0)
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
					pPlayer->Localize("You can't join teams in this minigame"));
			}
			else if(Team > 0 && Team < MAX_CLIENTS && pController->m_Teams.TeamLocked(Team) && !pController->m_Teams.IsInvited(Team, pResult->m_ClientID))
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
					pSelf->Config()->m_SvInvite ?
						pPlayer->Localize("This team is locked using /lock. Only members of the team can unlock it using /lock."):
						pPlayer->Localize("This team is locked using /lock. Only members of the team can invite you or unlock it using /lock."));
			}
			else if(Team > 0 && Team < MAX_CLIENTS && pController->m_Teams.Count(Team) >= pSelf->Config()->m_SvTeamMaxSize)
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), pPlayer->Localize("This team already has the maximum allowed size of %d players"), pSelf->Config()->m_SvTeamMaxSize);
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join", aBuf);
			}
			else if (pSelf->Config()->m_SvTeam == 0 || pSelf->Config()->m_SvTeam == 3)
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join", pPlayer->Localize("Teams are disabled"));
			}
			else if(pController->m_Teams.SetCharacterTeam(pPlayer->GetCID(), Team))
			{
				pSelf->SendChatFormat(-1, CHAT_ALL, -1, CHATFLAG_ALL, Localizable("%s joined team %d"), pSelf->Server()->ClientName(pPlayer->GetCID()), Team);
				pPlayer->m_Last_Team = pSelf->Server()->Tick();

				if(pController->m_Teams.IsPractice(Team))
					pSelf->SendChatTarget(pPlayer->GetCID(), "Practice mode enabled for your team, happy practicing!");
			}
			else
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
						pPlayer->Localize("You cannot join this team at this time"));
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
					pPlayer->Localize("You can't check your team while you are dead/a spectator."));
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
				pSelf->m_apPlayers[pResult->m_ClientID]->Localize("/me is disabled on this server"));
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

	if (pPlayer->GetCharacter() && pPlayer->GetCharacter()->m_IsZombie)
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
	if (pPlayer) pPlayer->SetNinjaJetpack(pResult->NumArguments() ? pResult->GetInteger(0) : !pPlayer->m_NinjaJetpack);
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
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You will now see all tees on this server, no matter the distance"));
	else
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You will no longer see all tees on this server"));
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

	CGameTeams &Teams = ((CGameControllerDDRace*) pSelf->m_pController)->m_Teams;
	int Team = Teams.m_Core.Team(pResult->m_ClientID);
	if (!pSelf->Config()->m_SvRescue && !Teams.IsPractice(Team)) {
		pSelf->SendChatTarget(pPlayer->GetCID(), "Rescue is not enabled on this server and you're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.");
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
	if((pSelf->Config()->m_SvKillProtection != 0 && CurrTime >= (60 * pSelf->Config()->m_SvKillProtection) && pChr->m_DDRaceState == DDRACE_STARTED) ||
		(pSelf->Config()->m_SvWalletKillProtection != 0 && pPlayer->GetWalletMoney() >= pSelf->Config()->m_SvWalletKillProtection && pChr->m_FreezeTime))
	{
			pPlayer->KillCharacter(WEAPON_SELF);
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

	for (int i = 0; i < NUM_SCORE_MODES; i++)
	{
		if (i == SCORE_BONUS && !pSelf->Config()->m_SvAllowBonusScoreMode)
			continue;
		if (!str_comp_nocase(aFormat, pSelf->GetScoreModeCommand(i)))
		{
			pPlayer->ChangeScoreMode(i);
			return;
		}
	}

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "~~~ %s ~~~", pPlayer->Localize("Score Format"));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Use '/score <format>' to change the displayed score."));
	str_format(aBuf, sizeof(aBuf), "time, level, points%s", pSelf->Config()->m_SvAllowBonusScoreMode ? ", bonus" : "");
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
}

void CGameContext::ConAccount(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pPlayer->GetAccID() < ACC_START)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "~~~ %s ~~~", pPlayer->Localize("Accounts"));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Accounts are used to save your stats."));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can farm money and buy things in the shop, kill tees and get points."));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("These stats will be saved inside of your account."));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can create an account using '/register'."));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Once you are logged in, you can use this command again to see information about your account."));
		return;
	}

	char aBuf[128];
	time_t tmp;
	CGameContext::AccountInfo *pAccount = &pSelf->m_Accounts[pPlayer->GetAccID()];

	str_format(aBuf, sizeof(aBuf), "--- %s ---", pPlayer->Localize("Account Info"));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "%s: %s", pPlayer->Localize("Account Name"), pAccount->m_Username);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);

	if (pAccount->m_RegisterDate != 0)
	{
		tmp = pAccount->m_RegisterDate;
		str_format(aBuf, sizeof(aBuf), "%s: %s", pPlayer->Localize("Registered"), pSelf->GetDate(tmp, false));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "%s: before April 9th 2021", pPlayer->Localize("Registered"));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}

	if (pSelf->Config()->m_SvEuroMode || pAccount->m_Euros > 0)
	{
		str_format(aBuf, sizeof(aBuf), "EUR: %.2f", pAccount->m_Euros);
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}

	if (pAccount->m_VIP)
	{
		tmp = pAccount->m_ExpireDateVIP;
		str_format(aBuf, sizeof(aBuf), "VIP%s: %s %s", pAccount->m_VIP == VIP_PLUS ? "+" : "", pPlayer->Localize("until", "/account"), pSelf->GetDate(tmp));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "VIP: %s", pPlayer->Localize("not bought"));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}

	if (pAccount->m_PortalRifle)
	{
		tmp = pAccount->m_ExpireDatePortalRifle;
		str_format(aBuf, sizeof(aBuf), "%s: %s %s", pPlayer->Localize("Portal Rifle"), pPlayer->Localize("until", "/account"), pSelf->GetDate(tmp));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	else if (pSelf->Config()->m_SvPortalRifleShop)
	{
		str_format(aBuf, sizeof(aBuf), "%s: %s", pPlayer->Localize("Portal Rifle"), pPlayer->Localize("not bought"));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}

	str_format(aBuf, sizeof(aBuf), "%s: %s", pPlayer->Localize("Contact"), pAccount->m_aContact);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "%s: %s", pPlayer->Localize("E-Mail"), pAccount->m_aEmail);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
}

void CGameContext::ConStats(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int ID = pResult->NumArguments() ? pSelf->GetCIDByName(pResult->GetString(0)) : pResult->m_ClientID;
	int AskerID = pResult->m_ClientID;
	CPlayer *pPlayer = pSelf->m_apPlayers[ID];
	CPlayer *pAsker = pSelf->m_apPlayers[AskerID];
	if (ID == -1 || !pPlayer || !pAsker)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pAsker->Localize("Player not found"));
		return;
	}

	char aBuf[128];
	int Minigame = pSelf->m_apPlayers[pResult->m_ClientID]->m_Minigame;
	CGameContext::AccountInfo *pAccount = &pSelf->m_Accounts[pPlayer->GetAccID()];

	// Show durak stats when on seat
	if (pSelf->Durak()->IsPlayerOnSeat(pResult->m_ClientID))
	{
		Minigame = MINIGAME_DURAK;
	}

	bool BankEnabled = pSelf->Config()->m_SvMoneyBankMode != 0;
	switch (Minigame)
	{
		case MINIGAME_1VS1:
		case MINIGAME_NONE:
		{
			str_format(aBuf, sizeof(aBuf), "--- %s's %s ---", pSelf->Server()->ClientName(ID), pAsker->Localize("Stats"));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "%s [%d] %s", pAsker->Localize("Level"), pAccount->m_Level, pPlayer->GetAccID() < ACC_START ? pAsker->Localize("(not logged in)") : "");
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "XP [%lld/%lld]", pAccount->m_XP, pSelf->GetNeededXP(pAccount->m_Level));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "%s [%lld]", BankEnabled ? pAsker->Localize("Bank") : pAsker->Localize("Wallet"), pPlayer->GetWalletOrBankDisplay());
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);

			// dont expose some info to other players than you
			if (ID == pResult->m_ClientID)
			{
				if (BankEnabled)
				{
					str_format(aBuf, sizeof(aBuf), "%s [%lld]", pAsker->Localize("Wallet"), pPlayer->GetWalletMoney());
					pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
				}
				if (pSelf->Config()->m_SvEuroMode || pAccount->m_Euros > 0)
				{
					str_format(aBuf, sizeof(aBuf), "EUR [%.2f]", pAccount->m_Euros);
					pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
				}
			}

			str_format(aBuf, sizeof(aBuf), "%s [%d]", pAsker->Localize("Police"), pAccount->m_PoliceLevel);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);

			str_format(aBuf, sizeof(aBuf), "--- %s ---", pAsker->Localize("Collectables"));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "%s [%d]", pAsker->Localize("Taser battery"), pAccount->m_TaserBattery);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "%s [%d]", pAsker->Localize("Portal Battery"), pAccount->m_PortalBattery);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		} //fallthrough

		case MINIGAME_BLOCK:
		{
			if (Minigame == MINIGAME_NONE)
			{
				pSelf->SendChatTarget(pResult->m_ClientID, "--- Block ---");
			}
			else
			{
				str_format(aBuf, sizeof(aBuf), "--- %s's Block %s ---", pSelf->Server()->ClientName(ID), pAsker->Localize("Stats"));
				pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			}
			str_format(aBuf, sizeof(aBuf), "%s: %d", pAsker->Localize("Points"), pAccount->m_BlockPoints);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "%s: %d", pAsker->Localize("Kills"), pAccount->m_Kills);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "%s: %d", pAsker->Localize("Deaths"), pAccount->m_Deaths);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		} break;

		case MINIGAME_DURAK:
		{
			if (ID == pResult->m_ClientID)
			{
				str_format(aBuf, sizeof(aBuf), "--- %s's %s ---", pSelf->Server()->ClientName(ID), pAsker->Localize("Stats"));
				pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
				str_format(aBuf, sizeof(aBuf), "%s [%lld]", pAsker->Localize("Wallet"), pPlayer->GetUsableMoney());
				pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
				pSelf->SendChatTarget(pResult->m_ClientID, "--- Durák ---");
			}
			else
			{
				str_format(aBuf, sizeof(aBuf), "--- %s's Durák %s ---", pSelf->Server()->ClientName(ID), pAsker->Localize("Stats"));
				pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			}
			str_format(aBuf, sizeof(aBuf), "%s: %d", pAsker->Localize("Games won"), pAccount->m_DurakWins);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "%s: %s%lld$", pAsker->Localize("Profit"), pAccount->m_DurakProfit > 0 ? "+" : "", pAccount->m_DurakProfit);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		} break;

		case MINIGAME_SURVIVAL:
		{
			str_format(aBuf, sizeof(aBuf), "--- %s's Survival %s ---", pSelf->Server()->ClientName(ID), pAsker->Localize("Stats"));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "%s: %d", pAsker->Localize("Wins"), pAccount->m_SurvivalWins);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "%s: %d", pAsker->Localize("Kills"), pAccount->m_SurvivalKills);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "%s: %d", pAsker->Localize("Deaths"), pAccount->m_SurvivalDeaths);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		} break;

		case MINIGAME_INSTAGIB_BOOMFNG: // fallthrough
		case MINIGAME_INSTAGIB_FNG:
		{
			str_format(aBuf, sizeof(aBuf), "--- %s's Instagib %s ---", pSelf->Server()->ClientName(ID), pAsker->Localize("Stats"));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "%s: %d", pAsker->Localize("Wins"), pAccount->m_InstagibWins);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "%s: %d", pAsker->Localize("Kills"), pAccount->m_InstagibKills);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "%s: %d", pAsker->Localize("Deaths"), pAccount->m_InstagibDeaths);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		} break;
	}
}

void CGameContext::ConHelpToggle(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "~~~ %s | %s & %s ~~~", pPlayer->Localize("Toggle"), pPlayer->Localize("Spooky Ghost"), pPlayer->Localize("Portal Blocker"));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Spooky ghost (gun) and portal blocker (hammer) can be enabled like the following:"));
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Hold TAB (or other scoreboard key) and during that fire your weapon two times."));
}

void CGameContext::ConVIPInfo(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	pSelf->SendChatTarget(pResult->m_ClientID, "~~~ VIP Classic ~~~");
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("With VIP Classic you get an XP and money boost of '+2' per second, aswell as access to the following commands:"));
	pSelf->SendChatTarget(pResult->m_ClientID, "rainbow, bloody, atom, trail, spreadgun, room");
	pSelf->SendChatTarget(pResult->m_ClientID, "~~~ VIP+ ~~~");
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("VIP+ includes every feature of VIP Classic."));
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Additionally to that you can enter the VIP+ room to farm safely there, aswell as you gain access to the following commands:"));
	pSelf->SendChatTarget(pResult->m_ClientID, "rainbowhook, rotatingball, epiccircle, lovely, rainbowname, rainbowspeed, sparkle");
}

void CGameContext::ConSpawnWeaponsInfo(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "~~~ %s ~~~", pPlayer->Localize("Spawn Weapons"));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can buy spawn weapons in the shop."));
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You will have the bought weapon on spawn."));
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can have max. 5 bullets per weapon."));
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Each bullet costs 600.000 money."));
	str_format(aBuf, sizeof(aBuf), "~~~ %s ~~~", pPlayer->Localize("Your Spawn Weapons"));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "%s: %d", pPlayer->Localize("Spawn shotgun bullets"), pSelf->m_Accounts[pSelf->m_apPlayers[pResult->m_ClientID]->GetAccID()].m_SpawnWeapon[0]);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "%s: %d", pPlayer->Localize("Spawn grenade bullets"), pSelf->m_Accounts[pSelf->m_apPlayers[pResult->m_ClientID]->GetAccID()].m_SpawnWeapon[1]);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "%s: %d", pPlayer->Localize("Spawn rifle bullets"), pSelf->m_Accounts[pSelf->m_apPlayers[pResult->m_ClientID]->GetAccID()].m_SpawnWeapon[2]);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
}

void CGameContext::ConWeaponIndicator(IConsole::IResult * pResult, void * pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (pPlayer) pPlayer->SetWeaponIndicator(pResult->NumArguments() ? pResult->GetInteger(0) : !pPlayer->m_WeaponIndicator);
}

void CGameContext::ConZoomCursor(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (pPlayer) pPlayer->SetZoomCursor(pResult->NumArguments() ? pResult->GetInteger(0) : !pPlayer->m_ZoomCursor);
}

void CGameContext::ConRegister(IConsole::IResult * pResult, void * pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (!pSelf->Config()->m_SvAccounts)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Accounts are not supported on this server"));
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
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("The username is too long or too short"));
		return;
	}

	if (str_check_special_chars(aUsername) == 0)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Your username can only consist of letters and numbers"));
		return;
	}

	if (str_comp_nocase(aPassword, aPassword2) != 0)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("The passwords need to be identical"));
		return;
	}

	if (str_length(aPassword) > MAX_PASSWORD_LENGTH || str_length(aPassword) < 3)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("The password is too long or too short"));
		return;
	}

	int ID = pSelf->AddAccount();
	pSelf->ReadAccountStats(ID, aUsername);

	if (!str_comp_nocase(pSelf->m_Accounts[ID].m_Username, aUsername))
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Username already exists"));
		pSelf->FreeAccount(ID);
		return;
	}

	// we reset the account again and read it again to get the correct values
	pSelf->FreeAccount(ID);

	// process register spam protection before really adding the account
	if (pSelf->ProcessAccountSystemBan(pResult->m_ClientID, ACC_SYS_REGISTER))
		return;

	ID = pSelf->AddAccount();

	pSelf->SetPassword(ID, aPassword);
	str_copy(pSelf->m_Accounts[ID].m_Username, aUsername, sizeof(pSelf->m_Accounts[ID].m_Username));
	str_copy(pSelf->m_Accounts[ID].m_aLastPlayerName, pSelf->Server()->ClientName(pResult->m_ClientID), sizeof(pSelf->m_Accounts[ID].m_aLastPlayerName));
	time_t Now;
	time(&Now);
	pSelf->m_Accounts[ID].m_RegisterDate = Now;
	pSelf->Logout(ID);

	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Successfully registered an account, you can login now"));
	dbg_msg("acc", "account created, file '%s/%s.acc'", pSelf->Config()->m_SvAccFilePath, aUsername);

	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Set a security pin to avoid account stealing. For more info, say '/pin'"));
}

void CGameContext::ConLogin(IConsole::IResult * pResult, void * pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (!pSelf->Config()->m_SvAccounts)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Accounts are not supported on this server"));
		return;
	}

	if (pSelf->Login(pResult->m_ClientID, pResult->GetString(0), pResult->GetString(1)))
		pSelf->CheckLoadPlayer(pResult->m_ClientID);
}

void CGameContext::ConLogout(IConsole::IResult * pResult, void * pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	int ID = pPlayer->GetAccID();
	if (!pSelf->Config()->m_SvAccounts && ID < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Accounts are not supported on this server"));
		return;
	}

	if (ID < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You are not logged in"));
		return;
	}

	if (pPlayer->IsMinigame())
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't logout in a minigame"));
		return;
	}

	if (pPlayer->m_JailTime)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't logout while being arrested"));
		return;
	}

	if (pPlayer->m_EscapeTime)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't logout while being searched by the police"));
		return;
	}

	if (pSelf->Config()->m_SvKillLogout && pPlayer->GetCharacter())
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), pPlayer->Localize("Kill logout is activated, kill within %d seconds to logout"), pSelf->Config()->m_SvKillLogout);
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		pPlayer->GetCharacter()->m_LastWantedLogout = pSelf->Server()->Tick();
		return;
	}

	pSelf->Logout(ID);
}

void CGameContext::ConChangePassword(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (!pSelf->Config()->m_SvAccounts)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Accounts are not supported on this server"));
		return;
	}

	int ID = pPlayer->GetAccID();
	if (ID < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You are not logged in"));
		return;
	}

	if (pSelf->m_Accounts[ID].m_aSecurityPin[0] && !pPlayer->m_aSecurityPin[0])
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You are not verified, please enter your security pin using '/pin'"));
		return;
	}

	if (pSelf->CheckPassword(ID, pResult->GetString(0)))
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Wrong password"));
		return;
	}

	if (str_comp(pResult->GetString(1), pResult->GetString(2)))
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("The passwords need to be identical"));
		return;
	}

	if (str_length(pResult->GetString(1)) > MAX_PASSWORD_LENGTH || str_length(pResult->GetString(1)) < 3)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("The password is too long or too short"));
		return;
	}

	pSelf->SetPassword(ID, pResult->GetString(1));
	pSelf->WriteAccountStats(ID);
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Successfully changed password"));
}

void CGameContext::ConContact(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (!pSelf->Config()->m_SvAccounts)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Accounts are not supported on this server"));
		return;
	}

	if (pPlayer->GetAccID() < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You are not logged in"));
		return;
	}

	if (pSelf->m_Accounts[pPlayer->GetAccID()].m_aSecurityPin[0] && !pPlayer->m_aSecurityPin[0])
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You are not verified, please enter your security pin using '/pin'"));
		return;
	}

	const char *pContact = pResult->GetString(0);
	if (!pContact || !pContact[0])
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You need to enter a way to contact you, for example: Discord, E-Mail, Skype, etc."));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("To see your current contact information, use '/account'"));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Example: '/contact Discord: fokkonaut'"));
		return;
	}

	if (pSelf->m_Accounts[pPlayer->GetAccID()].m_aContact[0] == '\0')
		pPlayer->GiveXP(500, "for setting initial contact info");

	str_copy(pSelf->m_Accounts[pPlayer->GetAccID()].m_aContact, pContact, sizeof(pSelf->m_Accounts[pPlayer->GetAccID()].m_aContact));
	pSelf->WriteAccountStats(pPlayer->GetAccID());
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Successfully updated contact information, check '/account'"));
}

void CGameContext::ConPin(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pSelf->IsAccountSystemBanned(pResult->m_ClientID, true))
		return;

	if (!pSelf->Config()->m_SvAccounts)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Accounts are not supported on this server"));
		return;
	}

	if (pPlayer->GetAccID() < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You are not logged in"));
		return;
	}

	if (!pResult->NumArguments())
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "~~~ %s ~~~", pPlayer->Localize("SECURITY PIN"));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You should set a security pin to avoid account stealing. The pin itself has to be a 4-digit long number code."));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("This pin will be required to change your contact information or your password."));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("If your account is not secured yet, you can set a pin with '/pin <new-pin>'."));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("If you want to change your pin, you first need to verify using '/pin <account-pin>'."));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("After verifying, you can set a new pin with '/pin <new-pin>'."));
		return;
	}

	CGameContext::AccountInfo *pAccount = &pSelf->m_Accounts[pPlayer->GetAccID()];

	const char *pNewPin = pResult->GetString(0);
	if (str_length(pNewPin) != 4 || str_is_number(pNewPin) != 0)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You have to enter a 4-digit number"));
		return;
	}

	if (pAccount->m_aSecurityPin[0] == 0)
	{
		str_copy(pAccount->m_aSecurityPin, pNewPin, sizeof(pAccount->m_aSecurityPin));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Successfully set the security pin"));
	}
	else if (pPlayer->m_aSecurityPin[0] == 0)
	{
		if (pAccount->m_aSecurityPin[0] && str_comp(pAccount->m_aSecurityPin, pNewPin) != 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Verification failed"));
			pSelf->ProcessAccountSystemBan(pResult->m_ClientID, ACC_SYS_PIN);
			return;
		}

		str_copy(pPlayer->m_aSecurityPin, pNewPin, sizeof(pPlayer->m_aSecurityPin));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Verification successful"));
	}
	else
	{
		str_copy(pAccount->m_aSecurityPin, pNewPin, sizeof(pAccount->m_aSecurityPin));
		str_copy(pPlayer->m_aSecurityPin, pNewPin, sizeof(pPlayer->m_aSecurityPin));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Successfully changed the security pin"));
	}
}

void CGameContext::ConPayMoney(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (!pSelf->Config()->m_SvAccounts)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Accounts are not supported on this server"));
		return;
	}
	if (pPlayer->GetAccID() < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You are not logged in"));
		return;
	}
	if (!pSelf->m_pHouses[HOUSE_BANK]->IsInside(pResult->m_ClientID))
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You have to be inside of a bank to pay money from your bank account to others"));
		return;
	}
	if (pPlayer->m_LastMoneyPay > pSelf->Server()->Tick() - pSelf->Server()->TickSpeed())
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't pay money that frequently"));
		return;
	}

	int64 Money = atoll(pResult->GetString(0));
	int ID = pSelf->GetCIDByName(pResult->GetString(1));
	CPlayer* pTo = pSelf->m_apPlayers[ID];
	if (ID == -1 || !pTo)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("That player doesn't exist"));
		return;
	}
	if (pTo->GetCID() == pResult->m_ClientID)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't pay money to yourself"));
		return;
	}
	if (pTo->GetAccID() < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("That player is not logged in"));
		return;
	}
	if (pSelf->m_Accounts[pPlayer->GetAccID()].m_Money < Money)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You don't have enough money"));
		return;
	}
	if (Money <= 0)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't pay nothing"));
		return;
	}

	pPlayer->m_LastMoneyPay = pSelf->Server()->Tick();

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "paid to '%s'", pSelf->Server()->ClientName(pTo->GetCID()));
	pPlayer->BankTransaction(-Money, aBuf);

	str_format(aBuf, sizeof(aBuf), "received from '%s'", pSelf->Server()->ClientName(pResult->m_ClientID));
	pTo->BankTransaction(Money, aBuf);

	str_format(aBuf, sizeof(aBuf), pPlayer->Localize("You paid %lld money from your bank account to '%s'"), Money, pSelf->Server()->ClientName(pTo->GetCID()));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);

	str_format(aBuf, sizeof(aBuf), pPlayer->Localize("You got %lld money to your bank account from '%s'"), Money, pSelf->Server()->ClientName(pResult->m_ClientID));
	pSelf->SendChatTarget(pTo->GetCID(), aBuf);
}

void CGameContext::ConMoney(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pResult->NumArguments() > 0)
	{
		if (str_comp_nocase(pResult->GetString(0), "drop") != 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Invalid argument"));
			return;
		}

		if (pPlayer->GetCharacter())
		{
			int Amount = pResult->GetInteger(1);
			if (Amount <= 0)
			{
				pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't drop nothing"));
				return;
			}
			if (Amount > 100000)
			{
				pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't drop money bags over 100.000"));
				return;
			}
			if (Amount > pPlayer->GetUsableMoney())
			{
				pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You don't have enough money in your wallet"));
				return;
			}

			if (pPlayer->GetCharacter()->m_LastMoneyDrop < pSelf->Server()->Tick() - pSelf->Server()->TickSpeed() * pSelf->Config()->m_SvMoneyDropDelay)
			{
				pPlayer->GetCharacter()->DropMoney(Amount);
				pPlayer->GetCharacter()->m_LastMoneyDrop = pSelf->Server()->Tick();
			}
		}
		return;
	}

	bool BankEnabled = pSelf->Config()->m_SvMoneyBankMode != 0;

	pSelf->SendChatTarget(pResult->m_ClientID, "~~~~~~~~~~");
	for (int i = 4; i >= 0; i--)
		pSelf->SendChatTarget(pResult->m_ClientID, pSelf->m_Accounts[pPlayer->GetAccID()].m_aLastMoneyTransaction[i]);
	char aBuf[256];
	pSelf->SendChatTarget(pResult->m_ClientID, "~~~~~~~~~~");
	str_format(aBuf, sizeof(aBuf), "%s [%lld]", BankEnabled ? pPlayer->Localize("Bank") : pPlayer->Localize("Wallet"), pPlayer->GetWalletOrBankDisplay());
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	if (BankEnabled)
	{
		str_format(aBuf, sizeof(aBuf), "%s [%lld]", pPlayer->Localize("Wallet"), pPlayer->GetWalletMoney());
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	if (pSelf->Config()->m_SvEuroMode || pSelf->m_Accounts[pPlayer->GetAccID()].m_Euros > 0)
	{
		str_format(aBuf, sizeof(aBuf), "EUR [%.2f]", pSelf->m_Accounts[pPlayer->GetAccID()].m_Euros);
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	pSelf->SendChatTarget(pResult->m_ClientID, "~~~~~~~~~~");
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Drop money: '/money drop <amount>'"));
}

void CGameContext::ConPortal(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	CGameContext::AccountInfo *pAccount = &pSelf->m_Accounts[pSelf->m_apPlayers[pResult->m_ClientID]->GetAccID()];

	if (pResult->NumArguments() > 0)
	{
		if (str_comp_nocase(pResult->GetString(0), "drop") != 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Invalid argument"));
			return;
		}

		if (pPlayer->GetCharacter())
		{
			int Amount = pResult->GetInteger(1);
			if (Amount <= 0)
			{
				pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't drop nothing"));
				return;
			}
			if (Amount > 100)
			{
				pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't drop more than 100 portal batteries at the same time"));
				return;
			}
			if (Amount > pAccount->m_PortalBattery)
			{
				pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You don't have enough portal batteries"));
				return;
			}

			pPlayer->GetCharacter()->DropBattery(WEAPON_PORTAL_RIFLE, Amount);
		}
		return;
	}

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "~~~ %s ~~~", pPlayer->Localize("Portal Rifle"));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can collect portal batteries in the map, which lets you use the portal rifle."));
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("The portal rifle can shoot two portals at cursor position, which tees, flags and weapons can use to travel between them."));
	str_format(aBuf, sizeof(aBuf), "~~~ %s ~~~", pPlayer->Localize("Your portal stats"));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "%s: %d", pPlayer->Localize("Portal Battery"), pAccount->m_PortalBattery);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "%s: %d", pPlayer->Localize("Portal Blocker"), pAccount->m_PortalBlocker);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	pSelf->SendChatTarget(pResult->m_ClientID, "~~~~~~~~~~");
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Drop portal battery: '/portal drop <amount>'"));
}

void CGameContext::ConRoom(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (!pSelf->m_Accounts[pPlayer->GetAccID()].m_VIP)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You are not VIP"));
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
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("That player doesn't exist"));
			return;
		}
		else if (!pPlayer->m_HasRoomKey)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You don't have a room key"));
			return;
		}
		else if (pChr->GetPlayer()->m_HasRoomKey)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("This player has a key already"));
			return;
		}
		else if (pChr->Core()->m_MoveRestrictionExtra.m_RoomKey)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("This player got invited already"));
			return;
		}

		pChr->Core()->m_MoveRestrictionExtra.m_RoomKey = true;
		str_format(aBuf, sizeof(aBuf), pPlayer->Localize("'%s' invited you to the room"), pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(pChr->GetPlayer()->GetCID(), aBuf);

		str_format(aBuf, sizeof(aBuf), pPlayer->Localize("You invited '%s' to the room"), pSelf->Server()->ClientName(pChr->GetPlayer()->GetCID()));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	else if (!str_comp_nocase(aCmd, "kick"))
	{
		if (ID == -1 || !pChr)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("That player doesn't exist"));
			return;
		}
		else if (!pPlayer->m_HasRoomKey)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't kick others without a key"));
			return;
		}
		else if (pChr->GetPlayer()->m_HasRoomKey)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't kick a player with a key"));
			return;
		}
		else if (!pChr->Core()->m_MoveRestrictionExtra.m_RoomKey)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("This player is not invited"));
			return;
		}

		pChr->Core()->m_MoveRestrictionExtra.m_RoomKey = false;
		str_format(aBuf, sizeof(aBuf), pPlayer->Localize("'%s' kicked you out of room"), pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(pChr->GetPlayer()->GetCID(), aBuf);

		str_format(aBuf, sizeof(aBuf), pPlayer->Localize("You kicked '%s' out of room"), pSelf->Server()->ClientName(pChr->GetPlayer()->GetCID()));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
}

void CGameContext::ConSpawn(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	CCharacter *pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if (!pPlayer || !pChr)
		return;

	if (pPlayer->GetAccID() < ACC_START)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You are not logged in"));
		return;
	}

	if (!pSelf->Config()->m_SvSlashSpawn)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Teleporting to spawn is disabled"));
		return;
	}

	if (pPlayer->IsMinigame())
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't use this command in minigames"));
		return;
	}

	if (pPlayer->m_JailTime)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't use this command while being arrested"));
		return;
	}

	if (pChr->m_FreezeTime)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't use this command while frozen"));
		return;
	}

	if (pPlayer->GetUsableMoney() < 50000)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You don't have enough money"));
		return;
	}

	vec2 Pos = pSelf->Collision()->GetRandomTile(ENTITY_SPAWN);
	if (Pos == vec2(-1, -1))
		return;

	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You lost 50.000 money for teleporting to spawn"));
	pPlayer->WalletTransaction(-50000, "teleported to spawn");

	pChr->ReleaseHook();
	pChr->ForceSetPos(Pos);
	pChr->ResetOnlyFirstPortal();

	// create death effect and do a nice sound when teleporting to spawn
	Mask128 TeamMask = pChr->TeamMask();
	pSelf->CreateDeath(Pos, pResult->m_ClientID, TeamMask);
	pSelf->CreateSound(Pos, SOUND_WEAPON_SPAWN, TeamMask);
}

void CGameContext::ConPlot(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	char aBuf[256];
	const char *pCommand = pResult->GetString(0);
	bool Help = !str_comp_nocase(pCommand, "help");
	if (pResult->NumArguments() == 0 || (Help && pResult->NumArguments() == 1))
	{
		str_format(aBuf, sizeof(aBuf), "%s: edit, clear, sell, cancel, buy, swap, spawn, list", pPlayer->Localize("Plot subcommands"));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("For detailed info, type '/plot help <command>'"));
		return;
	}
	else if (Help)
	{
		pCommand = pResult->GetString(1);
		if (!str_comp_nocase(pCommand, "edit"))
		{
			str_format(aBuf, sizeof(aBuf), "%s: /plot edit", pPlayer->Localize("Usage"));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Hold SPACE while editing for more control infos"));
		}
		else if (!str_comp_nocase(pCommand, "clear"))
		{
			str_format(aBuf, sizeof(aBuf), "%s: /plot clear", pPlayer->Localize("Usage"));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Clears all objects of your plot"));
		}
		else if (!str_comp_nocase(pCommand, "sell"))
		{
			str_format(aBuf, sizeof(aBuf), "%s: /plot sell <price>", pPlayer->Localize("Usage"));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Starts an auction for your plot, everyone can buy it using the buy command"));
		}
		else if (!str_comp_nocase(pCommand, "buy"))
		{
			str_format(aBuf, sizeof(aBuf), "%s: /plot <price> <playername>", pPlayer->Localize("Usage"));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Buys a plot if the given player is running a plot auction"));
		}
		else if (!str_comp_nocase(pCommand, "spawn"))
		{
			str_format(aBuf, sizeof(aBuf), "%s: /plot spawn", pPlayer->Localize("Usage"));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Lets you respawn at your plot"));
		}
		else if (!str_comp_nocase(pCommand, "cancel"))
		{
			str_format(aBuf, sizeof(aBuf), "%s: /plot cancel", pPlayer->Localize("Usage"));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Cancels the current running auction on your plot"));
		}
		else if (!str_comp_nocase(pCommand, "swap"))
		{
			str_format(aBuf, sizeof(aBuf), "%s: /plot swap <playername>", pPlayer->Localize("Usage"));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Swaps plot with given player"));
		}
		else if (!str_comp_nocase(pCommand, "list"))
		{
			str_format(aBuf, sizeof(aBuf), "%s: /plot list", pPlayer->Localize("Usage"));
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Shows a list with all currently open plot auctions and swap offers"));
		}
		return;
	}

	int Price = pResult->NumArguments() > 1 ? max(1, str_toint(pResult->GetString(1))) : 0; // clamp price to 0
	int OwnAccID = pSelf->m_apPlayers[pResult->m_ClientID]->GetAccID();
	int OwnPlotID = pSelf->GetPlotID(OwnAccID);

	if (!str_comp_nocase(pCommand, "buy"))
	{
		if (pPlayer->GetAccID() < ACC_START)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You are not logged in"));
			return;
		}

		const char *pName = pResult->NumArguments() > 2 ? pResult->GetString(2) : "";
		int ID = pSelf->GetCIDByName(pName);
		CPlayer *pSeller = pSelf->m_apPlayers[ID];
		if (ID == -1 || !pSeller)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("This player doesn't exists"));
			return;
		}

		if (ID == pResult->m_ClientID)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't buy your own plot"));
			return;
		}

		if (!pSeller->m_PlotAuctionPrice)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("This player doesn't sell a plot"));
			return;
		}

		if (OwnPlotID > 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You already have a plot"));
			return;
		}

		if (pSeller->m_PlotAuctionPrice != Price)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("The price you entered does not match the offer"));
			return;
		}

		if (pPlayer->GetUsableMoney() < Price)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You don't have enough money"));
			return;
		}

		if (pSelf->HasPlotByIP(pResult->m_ClientID))
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Your IP address already owns one plot"));
			return;
		}

		// success
		int PlotID = pSelf->GetPlotID(pSeller->GetAccID());

		pSelf->SendChatFormat(-1, CHAT_ALL, -1, CHATFLAG_ALL, Localizable("Plot %d has been bought by '%s'"), PlotID, pSelf->Server()->ClientName(pResult->m_ClientID));

		// a message to you
		str_format(aBuf, sizeof(aBuf), pPlayer->Localize("You bought plot %d from %s"), PlotID, pName);
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);

		// and one to the seller
		str_format(aBuf, sizeof(aBuf), pPlayer->Localize("%s bought your plot, the money is moved to your bank account"), pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(ID, aBuf);

		// get money from buyer
		str_format(aBuf, sizeof(aBuf), "bought plot %d", PlotID);
		pPlayer->WalletTransaction(-Price, aBuf);

		// give money to seller
		pSeller->BankTransaction(Price, "sold plot");
		pSeller->m_PlotAuctionPrice = 0;

		pSeller->StopPlotEditing();

		pSelf->SetPlotInfo(PlotID, pPlayer->GetAccID());
	}
	else if (!str_comp_nocase(pCommand, "list"))
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "~~~ %s ~~~", pPlayer->Localize("Plot list"));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Buy from auction: '/plot buy <price> <playername>'"));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Swap with offer: '/plot swap <playername>'"));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("List of all plot auctions and swap offers:"));

		bool Anything = false;
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (!pSelf->m_apPlayers[i])
				continue;

			if (pSelf->m_apPlayers[i]->m_PlotAuctionPrice != 0)
			{
				str_format(aBuf, sizeof(aBuf), "Type: auction, name: '%s', price: %d", pSelf->Server()->ClientName(i), pSelf->m_apPlayers[i]->m_PlotAuctionPrice);
				pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
				Anything = true;
			}
			if (!str_comp(pSelf->m_apPlayers[i]->m_aPlotSwapUsername, pSelf->Server()->ClientName(pResult->m_ClientID)))
			{
				str_format(aBuf, sizeof(aBuf), "Type: swap, name: '%s'", pSelf->Server()->ClientName(i));
				pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
				Anything = true;
			}
		}

		if (!Anything)
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("There are currently no auctions or swap offers"));
	}
	else if (OwnPlotID == 0)
	{
		// check for the important commands
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You need a plot to use this command"));
	}
	else if (!str_comp_nocase(pCommand, "sell"))
	{
		if (pPlayer->GetAccID() < ACC_START)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You are not logged in"));
			return;
		}

		if (pSelf->PlotCanBeRaided(OwnPlotID))
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't sell your plot when being wanted"));
			return;
		}

		if (pPlayer->m_LastPlotAuction && pPlayer->m_LastPlotAuction + pSelf->Server()->TickSpeed() * 60 > pSelf->Server()->Tick())
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can only do auctions once a minute"));
			return;
		}

		if (pPlayer->m_PlotAuctionPrice != 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You already sell your plot"));
			return;
		}

		if (Price <= 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You need to enter a valid price"));
			return;
		}

		pPlayer->CancelPlotSwap();
		pPlayer->m_PlotAuctionPrice = Price;
		pPlayer->m_LastPlotAuction = pSelf->Server()->Tick();

		pSelf->SendChatFormat(-1, CHAT_ALL, -1, CHATFLAG_ALL, Localizable("'%s' started an auction on plot %d for %d money (plot expires on %s)"),
			pSelf->Server()->ClientName(pResult->m_ClientID), OwnPlotID, Price, pSelf->GetDate(pSelf->m_aPlots[OwnPlotID].m_ExpireDate));

		pSelf->SendChatFormat(-1, CHAT_ALL, -1, CGameContext::CHATFLAG_ALL, Localizable("Use '/plot buy %d %s' to buy the plot"),
			Price, pSelf->Server()->ClientName(pResult->m_ClientID));
	}
	else if (!str_comp_nocase(pCommand, "cancel"))
	{
		pPlayer->CancelPlotAuction();
		pPlayer->CancelPlotSwap();
	}
	else if (!str_comp_nocase(pCommand, "clear"))
	{
		pPlayer->ClearPlot();
	}
	else if (!str_comp_nocase(pCommand, "spawn"))
	{
		pPlayer->SetPlotSpawn(!pPlayer->m_PlotSpawn);
	}
	else if (!str_comp_nocase(pCommand, "swap"))
	{
		if (pPlayer->GetAccID() < ACC_START)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You are not logged in"));
			return;
		}

		if (pSelf->PlotCanBeRaided(OwnPlotID))
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't swap plots when being wanted"));
			return;
		}

		const char *pSwapName = pResult->GetString(1);
		int ID = pSelf->GetCIDByName(pSwapName);
		CPlayer * pSwap = pSelf->m_apPlayers[ID];
		if (ID == -1 || !pSwap)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("This player doesn't exists"));
			return;
		}

		if (ID == pResult->m_ClientID)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't swap with yourself"));
			return;
		}

		int SwapAccID = pSwap->GetAccID();
		int SwapPlotID = pSelf->GetPlotID(SwapAccID);
		if (SwapPlotID == 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("This player doesn't own a plot"));
			return;
		}

		if (pPlayer->m_LastPlotSwap && pPlayer->m_LastPlotSwap + pSelf->Server()->TickSpeed() * 60 > pSelf->Server()->Tick())
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can only swap once a minute"));
			return;
		}

		pPlayer->m_LastPlotSwap = pSelf->Server()->Tick();
		pPlayer->CancelPlotAuction();
		str_copy(pPlayer->m_aPlotSwapUsername, pSelf->m_Accounts[SwapAccID].m_Username, sizeof(pPlayer->m_aPlotSwapUsername));
		
		const char *pOwnName = pSelf->Server()->ClientName(pResult->m_ClientID);
		if (str_comp(pPlayer->m_aPlotSwapUsername, pSelf->m_Accounts[SwapAccID].m_Username) == 0
			&& str_comp(pSwap->m_aPlotSwapUsername, pSelf->m_Accounts[OwnAccID].m_Username) == 0)
		{
			pSelf->SetPlotInfo(OwnPlotID, SwapAccID);
			pSelf->SetPlotInfo(SwapPlotID, OwnAccID);
			pPlayer->m_aPlotSwapUsername[0] = 0;
			pSwap->m_aPlotSwapUsername[0] = 0;
			pPlayer->StopPlotEditing();
			pSwap->StopPlotEditing();

			str_format(aBuf, sizeof(aBuf), pPlayer->Localize("You swapped plots with '%s'"), pSwapName);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);

			str_format(aBuf, sizeof(aBuf), pPlayer->Localize("You swapped plots with '%s'"), pOwnName);
			pSelf->SendChatTarget(ID, aBuf);
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), pPlayer->Localize("Offer to swap plots was sent to '%s'"), pSwapName);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), pPlayer->Localize("'%s' has offered to swap plots with you, type '/plot swap %s' to swap"), pOwnName, pOwnName);
			pSelf->SendChatTarget(ID, aBuf);
		}
	}
	else if (!str_comp_nocase(pCommand, "edit"))
	{
		pPlayer->StartPlotEdit();
	}
}

void CGameContext::ConHideDrawings(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (pPlayer) pPlayer->SetHideDrawings(pResult->NumArguments() ? pResult->GetInteger(0) : !pPlayer->m_HideDrawings);
}

void CGameContext::ConHideBroadcasts(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (pPlayer) pPlayer->SetHideBroadcasts(pResult->NumArguments() ? pResult->GetInteger(0) : !pPlayer->m_HideBroadcasts);
}

void CGameContext::ConSilentFarm(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (pPlayer) pPlayer->SetSilentFarm(pResult->NumArguments() ? pResult->GetInteger(0) : !pPlayer->m_SilentFarm);
}

void CGameContext::ConPoliceInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	int Page = pResult->GetInteger(0);
	int MaxPages = 6;	//////UPDATE THIS WITH EVERY PAGE YOU ADD
	if (!Page || Page > MaxPages)
		Page = 1;

	char aBuf[128];
	char aPage[128];
	str_format(aPage, sizeof(aPage), "-- %s %d/%d --", pPlayer->Localize("Page"), Page, MaxPages);

	str_format(aBuf, sizeof(aBuf), "~~~ %s ~~~", pPlayer->Localize("Police Info"));
	if (Page >= 2 && Page <= MaxPages)
	{
		int Level = 0;
		int Policelevel = Page - 1;
		char aPolice[64];

		if (Policelevel == 1)
			Level = 18;
		else if (Policelevel == 2)
			Level = 25;
		else if (Policelevel == 3)
			Level = 30;
		else if (Policelevel == 4)
			Level = 40;
		else if (Policelevel == 5)
			Level = 50;

		str_format(aPolice, sizeof(aPolice), pPlayer->Localize("[POLICE %d]"), Policelevel);
		pSelf->SendChatTarget(pResult->m_ClientID, aPolice);

		str_format(aPolice, sizeof(aPolice), "%s: [LVL %d]", pPlayer->Localize("Level needed to buy"), Level);
		pSelf->SendChatTarget(pResult->m_ClientID, aPolice);

		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Benefits:"));
		if (Policelevel == 1)
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("- The police bot will help you"));
		else if (Policelevel == 2)
			pSelf->SendChatTarget(pResult->m_ClientID, "- '/policehelper'");
		else if (Policelevel == 3)
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("- taser license ('/taser')"));
		else if (Policelevel == 4)
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("- Officers can taser a plot door of a wanted player to deal damage and destroy it"));
		else if (Policelevel == 5)
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("- Taser will destroy plot objects of a wanted player"));
	}
	else
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("[GENERAL INFORMATION]"));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Police ranks can be bought in shop."));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("There are multiple police ranks, each cost 100.000 money."));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("The policebot will help every police officer."));
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Every police rank will give you more benefits."));
	}
	pSelf->SendChatTarget(pResult->m_ClientID, "------------------------");
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Use '/police <page>' for information about other ranks"));
	pSelf->SendChatTarget(pResult->m_ClientID, aPage);
}

void CGameContext::ConResumeMoved(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (pPlayer) pPlayer->SetResumeMoved(pResult->NumArguments() ? pResult->GetInteger(0) : !pPlayer->m_ResumeMoved);
}

void CGameContext::ConMutePlayer(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	int ID = pSelf->GetCIDByName(pResult->GetString(0));
	if (ID == -1)
		return;

	if (ID == pResult->m_ClientID)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't mute yourself"));
		return;
	}

	pPlayer->m_aMuted[ID] = !pPlayer->m_aMuted[ID];

	char aBuf[128];
	if (pPlayer->m_aMuted[ID])
		str_format(aBuf, sizeof(aBuf), "'%s' %s", pSelf->Server()->ClientName(ID), pPlayer->Localize("muted"));
	else
		str_format(aBuf, sizeof(aBuf), "'%s' %s", pSelf->Server()->ClientName(ID), pPlayer->Localize("unmuted"));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
}

void CGameContext::ConDesign(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pResult->NumArguments())
	{
		pSelf->Server()->ChangeMapDesign(pResult->m_ClientID, pResult->GetString(0));
		return;
	}

	char aDesigns[256] = "default";
	char aTemp[128];
	aTemp[0] = 0;
	for (int i = 0; i < CServer::NUM_MAP_DESIGNS; i++)
	{
		if (((CServer *)pSelf->Server())->m_aMapDesign[i].m_aName[0] == '\0')
			continue;

		str_format(aTemp, sizeof(aTemp), "%s, ", aDesigns);
		str_format(aDesigns, sizeof(aDesigns), "%s%s", aTemp, ((CServer *)pSelf->Server())->m_aMapDesign[i].m_aName);
	}

	str_format(aTemp, sizeof(aTemp), "~~~ %s ~~~", pPlayer->Localize("Map Designs"));
	pSelf->SendChatTarget(pResult->m_ClientID, aTemp);
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("If you have your dummy connected, you need to reconnect it after the design change so it can get back it's state."));
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can set one of the following designs by using '/design <name>':"));
	pSelf->SendChatTarget(pResult->m_ClientID, aDesigns);
}

void CGameContext::ConLanguage(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientID = pResult->m_ClientID;
	CPlayer *pPlayer = pSelf->m_apPlayers[ClientID];
	if (!pPlayer)
		return;

	if (pResult->NumArguments())
	{
		const char *pLanguage = pResult->GetString(0);
		int Language = g_Localization.GetLanguage(pLanguage);
		if (Language == -1 || !g_Localization.Languages()[Language].m_Available)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Invalid language"));
			return;
		}

		// Update language
		pPlayer->SetLanguage(Language);
		return;
	}

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "~~~ %s ~~~", pPlayer->Localize("Server Language"));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "%s: %s", pPlayer->Localize("Current language"), g_Localization.GetLanguageString(pPlayer->m_Language));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("English translation by the developers"));
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can set one of the following languages for server-side translation by using '/language <option>':"));
	pSelf->SendChatTarget(pResult->m_ClientID, g_Localization.ListAvailable());
}

void CGameContext::ConChatLanguage(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	const char *apLanguages[] = { "none", "en", "ar", "zh", "fr", "de", "hi", "id", "ga", "it", "ja", "ko", "pl", "pt", "ru", "es", "tr", "vi" };
	int NumLanguages = (sizeof(apLanguages)/sizeof(*apLanguages));

	if (pResult->NumArguments())
	{
		if (!str_comp(pSelf->Server()->GetChatLanguage(pResult->m_ClientID), pResult->GetString(0)))
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("This chat language is already selected"));
			return;
		}

		for (int i = 0; i < NumLanguages; i++)
		{
			if (str_comp(pResult->GetString(0), apLanguages[i]) == 0)
			{
				pSelf->Server()->SetChatLanguage(pResult->m_ClientID, apLanguages[i]);
				int Dummy = pSelf->Server()->GetDummy(pResult->m_ClientID);
				if (Dummy != -1)
					pSelf->Server()->SetChatLanguage(Dummy, apLanguages[i]);
				pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Successfully updated chat language"));
				return;
			}
		}

		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Invalid language"));
		return;
	}

	char aLanguages[256] = "";
	char aTemp[128];
	aTemp[0] = 0;
	for (int i = 0; i < NumLanguages; i++)
	{
		if (i > 0)
			str_format(aTemp, sizeof(aTemp), "%s, ", aLanguages);
		str_format(aLanguages, sizeof(aLanguages), "%s%s", aTemp, apLanguages[i]);
	}

	str_format(aTemp, sizeof(aTemp), "~~~ %s ~~~", pPlayer->Localize("Chat Language"));
	pSelf->SendChatTarget(pResult->m_ClientID, aTemp);
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can set one of the following languages by using '/chatlanguage <option>':"));
	pSelf->SendChatTarget(pResult->m_ClientID, aLanguages);
}

void CGameContext::ConDiscord(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pSelf->Config()->m_SvDiscordURL[0])
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "%s: %s", pPlayer->Localize("Discord server"), pSelf->Config()->m_SvDiscordURL);
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	else
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("This server does not have a linked Discord server"));
}

void CGameContext::ConMinigames(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	char aMinigames[256];
	char aTemp[256];
	aMinigames[0] = 0;
	aTemp[0] = 0;
	for (int i = 0; i < NUM_MINIGAMES; i++)
	{
		if (i != MINIGAME_BLOCK)
			str_format(aTemp, sizeof(aTemp), "%s, ", aMinigames);
		str_format(aMinigames, sizeof(aMinigames), "%s%s", aTemp, pSelf->GetMinigameCommand(i));
	}

	str_format(aTemp, sizeof(aTemp), "~~~ %s ~~~", pPlayer->Localize("Minigames"));
	pSelf->SendChatTarget(pResult->m_ClientID, aTemp);
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can join any minigame using '/<minigame>'"));
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("To leave a minigame, just type '/leave'"));
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Here is a list of all minigames:"));
	pSelf->SendChatTarget(pResult->m_ClientID, aMinigames);
}

void CGameContext::ConLeaveMinigame(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pPlayer->m_Minigame == MINIGAME_1VS1 && pSelf->Arenas()->FightStarted(pResult->m_ClientID))
		pSelf->Arenas()->OnPlayerLeave(pResult->m_ClientID);
	else
		pSelf->SetMinigame(pResult->m_ClientID, MINIGAME_NONE);
}

void CGameContext::ConJoinBlock(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SetMinigame(pResult->m_ClientID, MINIGAME_BLOCK);
}

void CGameContext::ConJoinSurvival(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SetMinigame(pResult->m_ClientID, MINIGAME_SURVIVAL);
}

void CGameContext::ConJoinBoomFNG(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SetMinigame(pResult->m_ClientID, MINIGAME_INSTAGIB_BOOMFNG);
}

void CGameContext::ConJoinFNG(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SetMinigame(pResult->m_ClientID, MINIGAME_INSTAGIB_FNG);
}

void CGameContext::ConJoinDurak(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SetMinigame(pResult->m_ClientID, MINIGAME_DURAK);
}

void CGameContext::Con1VS1(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pPlayer->m_Minigame != MINIGAME_1VS1 || !pResult->NumArguments())
	{
		pSelf->SetMinigame(pResult->m_ClientID, MINIGAME_1VS1);
		return;
	}

	int OtherID;
	char aBuf[128];
	str_copy(aBuf, pResult->GetFullString(), sizeof(aBuf));
	const char *pRest = pSelf->GetWhisper(aBuf, &OtherID);
	if (pSelf->Arenas()->AcceptFight(OtherID, pResult->m_ClientID))
		return;

	int ScoreLimit = 10;
	int KillBorder = 0;
	if (pResult->NumArguments() > 1) // more than just name
	{
		int Num = sscanf(pRest, "%d %d", &ScoreLimit, &KillBorder);
		if (Num == 1)
			KillBorder = 0;
	}

	pSelf->Arenas()->StartConfiguration(pResult->m_ClientID, OtherID, ScoreLimit, KillBorder);
}

void CGameContext::SendTop5AccMessage(IConsole::IResult* pResult, void* pUserData, int Type)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	pSelf->LazyLoadTopAccounts(Type);

	char aBuf[512];
	int Debut = pResult->NumArguments() >= 1 && pResult->GetInteger(0) != 0 ? pResult->GetInteger(0) : 1;
	Debut = max(1, Debut < 0 ? (int)pSelf->m_TopAccounts.size() + Debut - 3 : Debut);

	// Header
	const char *pType = "";
	switch (Type)
	{
	case TOP_LEVEL: pType = pPlayer->Localize("Level"); break;
	case TOP_POINTS: pType = pPlayer->Localize("Points"); break;
	case TOP_MONEY: pType = pPlayer->Localize("Money"); break;
	case TOP_SPREE: pType = pPlayer->Localize("Spree"); break;
	case TOP_PORTAL_BATTERY: pType = pPlayer->Localize("Portal Battery"); break;
	case TOP_PORTAL_BLOCKER: pType = pPlayer->Localize("Portal Blocker"); break;
	case TOP_DURAK_WINS: pType = pPlayer->Localize("Durák Wins"); break;
	case TOP_DURAK_PROFIT: pType = pPlayer->Localize("Durák Profit"); break;
	}

	str_format(aBuf, sizeof(aBuf), "----------- Top 5 %s -----------", pType);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);

	// Short name for entries
	switch (Type)
	{
	case TOP_PORTAL_BATTERY: pType = "Batteries"; break;
	case TOP_PORTAL_BLOCKER: pType = "Blockers"; break;
	case TOP_DURAK_WINS: pType = "Wins"; break;
	case TOP_DURAK_PROFIT: pType = "Profit"; break;
	}

	for (int i = 0; i < 5; i++)
	{
		if (i + Debut > (int)pSelf->m_TopAccounts.size())
			break;
		CGameContext::TopAccounts* r = &pSelf->m_TopAccounts[i + Debut - 1];

		if (Type == TOP_MONEY)
		{
			str_format(aBuf, sizeof(aBuf), "%d. %s %s: %lld", i + Debut, r->m_aUsername, pPlayer->Localize("Money"), r->m_Money);
		}
		else
		{
			int Value = -1;
			switch (Type)
			{
			case TOP_LEVEL: Value = r->m_Level; break;
			case TOP_POINTS: Value = r->m_Points; break;
			case TOP_SPREE: Value = r->m_KillStreak; break;
			case TOP_PORTAL_BATTERY: Value = r->m_PortalBattery; break;
			case TOP_PORTAL_BLOCKER: Value = r->m_PortalBlocker; break;
			case TOP_DURAK_WINS: Value = r->m_DurakWins; break;
			case TOP_DURAK_PROFIT: Value = r->m_DurakProfit; break;
			}
			char aValue[64];
			str_format(aValue, sizeof(aValue), "%s%d%s", Type == TOP_DURAK_PROFIT && Value > 0 ? "+" : "", Value, Type == TOP_DURAK_PROFIT ? "$" : "");
			str_format(aBuf, sizeof(aBuf), "%d. %s %s: %s", i + Debut, r->m_aUsername, pType, aValue);
		}

		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	pSelf->SendChatTarget(pResult->m_ClientID, "----------------------------------------");

	// Unload top accounts again after lazy loading
	pSelf->m_TopAccounts.clear();
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

void CGameContext::ConTop5PortalBattery(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->SendTop5AccMessage(pResult, pUserData, TOP_PORTAL_BATTERY);
}

void CGameContext::ConTop5PortalBlocker(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->SendTop5AccMessage(pResult, pUserData, TOP_PORTAL_BLOCKER);
}

void CGameContext::ConTop5DurakWins(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->SendTop5AccMessage(pResult, pUserData, TOP_DURAK_WINS);
}

void CGameContext::ConTop5DurakProfit(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->SendTop5AccMessage(pResult, pUserData, TOP_DURAK_PROFIT);
}

void CGameContext::ConPoliceHelper(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pSelf->m_Accounts[pPlayer->GetAccID()].m_PoliceLevel < 2)
	{
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You need to be police level 2 to use this command"));
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
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("That player doesn't exist"));
			return;
		}
		else if (pSelf->m_Accounts[pChr->GetPlayer()->GetAccID()].m_PoliceLevel)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("This player is a police officer"));
			return;
		}
		else if (pChr->m_PoliceHelper)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("This player is a police helper already"));
			return;
		}

		pChr->m_PoliceHelper = true;
		str_format(aBuf, sizeof(aBuf), pPlayer->Localize("'%s' added you to the police helpers"), pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(pChr->GetPlayer()->GetCID(), aBuf);

		str_format(aBuf, sizeof(aBuf), pPlayer->Localize("You added '%s' to the police helpers"), pSelf->Server()->ClientName(pChr->GetPlayer()->GetCID()));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	else if (!str_comp_nocase(aCmd, "remove"))
	{
		if (ID == -1 || !pChr)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("That player doesn't exist"));
			return;
		}
		else if (pSelf->m_Accounts[pChr->GetPlayer()->GetAccID()].m_PoliceLevel)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("This player is a police officer"));
			return;
		}
		else if (!pChr->m_PoliceHelper)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("This player is not a police helper"));
			return;
		}

		pChr->m_PoliceHelper = false;
		str_format(aBuf, sizeof(aBuf), pPlayer->Localize("'%s' removed you from the police helpers"), pSelf->Server()->ClientName(pResult->m_ClientID));
		pSelf->SendChatTarget(pChr->GetPlayer()->GetCID(), aBuf);

		str_format(aBuf, sizeof(aBuf), pPlayer->Localize("You removed '%s' from the police helpers"), pSelf->Server()->ClientName(pChr->GetPlayer()->GetCID()));
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
}

void CGameContext::ConWanted(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "~~~ %s ~~~", pPlayer->Localize("WANTED"));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Listing all players that are wanted by the police:"));

	int Total = 0;
	int Bufcnt = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (pSelf->m_apPlayers[i] && pSelf->m_apPlayers[i]->m_EscapeTime)
		{
			Total++;
			const char* pName = pSelf->Server()->ClientName(i);
			if (Bufcnt + str_length(pName) + 4 > 128)
			{
				pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
				Bufcnt = 0;
			}
			if (Bufcnt != 0)
			{
				str_format(&aBuf[Bufcnt], sizeof(aBuf) - Bufcnt, ", %s", pName);
				Bufcnt += 2 + str_length(pName);
			}
			else
			{
				str_format(&aBuf[Bufcnt], sizeof(aBuf) - Bufcnt, "%s", pName);
				Bufcnt += str_length(pName);
			}
		}
	}
	if (Bufcnt != 0)
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);

	if (Total)
	{
		if (Total == 1)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("1 wanted player"));
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), pPlayer->Localize("%d wanted players"), Total);
			pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
		}
	}
	else
		pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("There are no wanted players right now"));
}

void CGameContext::ConTaserInfo(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CPlayer* pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	CGameContext::AccountInfo *pAccount = &pSelf->m_Accounts[pSelf->m_apPlayers[pResult->m_ClientID]->GetAccID()];

	if (pResult->NumArguments() > 0)
	{
		if (str_comp_nocase(pResult->GetString(0), "drop") != 0)
		{
			pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Invalid argument"));
			return;
		}

		if (pPlayer->GetCharacter())
		{
			int Amount = pResult->GetInteger(1);
			if (Amount <= 0)
			{
				pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can't drop nothing"));
				return;
			}
			if (Amount > pAccount->m_TaserBattery)
			{
				pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You don't have enough taser batteries"));
				return;
			}

			pPlayer->GetCharacter()->DropBattery(WEAPON_TASER, Amount);
		}
		return;
	}

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "~~~ %s ~~~", pPlayer->Localize("Taser"));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Police officers with level 3 or higher get a taser license."));
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("The taser is a rifle that freezes players for a short time."));
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("You can pick up the taser and taser batteries throughout the map."));
	str_format(aBuf, sizeof(aBuf), "~~~ %s ~~~", pPlayer->Localize("Your taser stats"));
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "%s: %d/%d", pPlayer->Localize("Taser level"), pAccount->m_TaserLevel, NUM_TASER_LEVELS);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	if (pAccount->m_TaserLevel < NUM_TASER_LEVELS)
	{
		str_format(aBuf, sizeof(aBuf), "%s: %d", pPlayer->Localize("Price for the next level"), pSelf->m_aTaserPrice[pAccount->m_TaserLevel]);
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
	str_format(aBuf, sizeof(aBuf), pPlayer->Localize("Freeze time: %.2f seconds"), pAccount->m_TaserLevel * 0.1f);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "%s: %d/%d", pPlayer->Localize("Taser battery"), pAccount->m_TaserBattery, MAX_TASER_BATTERY);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "%s: %d%%", pPlayer->Localize("Taser shield"), pPlayer->m_TaserShield);
	pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	pSelf->SendChatTarget(pResult->m_ClientID, "~~~~~~~~~~");
	pSelf->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Drop taser battery: '/taser drop <amount>'"));
}

void CGameContext::ConRainbowVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter *pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if (pChr) pChr->OnRainbowVIP();
}

void CGameContext::ConBloodyVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter *pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if (pChr) pChr->OnBloodyVIP();
}

void CGameContext::ConAtomVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter *pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if (pChr) pChr->OnAtomVIP();
}

void CGameContext::ConTrailVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter *pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if (pChr) pChr->OnTrailVIP();
}

void CGameContext::ConSpreadGunVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter *pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if (pChr) pChr->OnSpreadGunVIP();
}

void CGameContext::ConRotatingBallVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter *pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if (pChr) pChr->OnRotatingBallVIP();
}

void CGameContext::ConEpicCircleVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter *pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if (pChr) pChr->OnEpicCircleVIP();
}

void CGameContext::ConLovelyVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter *pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if (pChr) pChr->OnLovelyVIP();
}

void CGameContext::ConRainbowHookVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter *pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if (pChr) pChr->OnRainbowHookVIP();
}

void CGameContext::ConRainbowNameVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter *pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if (pChr) pChr->OnRainbowNameVIP();
}

void CGameContext::ConSparkleVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CCharacter *pChr = pSelf->GetPlayerChar(pResult->m_ClientID);
	if (pChr) pChr->OnSparkleVIP();
}

void CGameContext::ConRainbowSpeedVIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if (!pPlayer)
		return;

	if (pResult->NumArguments())
	{
		pPlayer->SetRainbowSpeedVIP(pResult->GetInteger(0));
	}
	else
	{
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "Current rainbow speed: %d", pPlayer->m_RainbowSpeed);
		pSelf->SendChatTarget(pResult->m_ClientID, aBuf);
	}
}

void CGameContext::ConShrug(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(pPlayer) pSelf->SendChat(pResult->m_ClientID, pPlayer->m_LocalChat ? CHAT_LOCAL : CHAT_ALL, -1, "¯\\_(ツ)_/¯", pResult->m_ClientID);
}
