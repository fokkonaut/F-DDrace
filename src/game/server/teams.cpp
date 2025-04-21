/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "teams.h"
#include "score.h"
#include <engine/shared/config.h>
#include "gamecontroller.h"

CGameTeams::CGameTeams(CGameContext *pGameContext) :
		m_pGameContext(pGameContext)
{
	Reset();
}

static const int s_aLegacyTeams[VANILLA_MAX_CLIENTS + 1] = { 0, 1, 56, 22, 43, 9, -2/*64*/, 30, 51, 17, 38, 4, 59, 25, 46, 12, 33, 54, 20, 41, 7, 62, 28, 49, 15, 36, 2, 57, 23, 44, 10, 31,
					52, 18, 39, 5, 60, 26, 47, 13, 34, 55, 21, 42, 8, 63, 29, 50, 16, 37, 3, 58, 24, 45, 11, 32, 53, 19, 40, 6, 61, 27, 48, 14, VANILLA_MAX_CLIENTS/*35*/ };

void CGameTeams::Reset()
{
	m_Core.Reset();
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_TeamState[i] = TEAMSTATE_EMPTY;
		m_TeeFinished[i] = false;
		m_MembersCount[i] = 0;
		m_LastChat[i] = 0;
		m_TeamLocked[i] = false;
		m_IsSaving[i] = false;
		m_Invited[i] = CmaskNone();
		m_Practice[i] = false;
	}
}

void CGameTeams::OnCharacterStart(int ClientID)
{
	int Tick = Server()->Tick();
	CCharacter* pStartingChar = Character(ClientID);
	if(!pStartingChar)
		return;
	if(m_Core.Team(ClientID) != TEAM_FLOCK && pStartingChar->m_DDRaceState == DDRACE_FINISHED)
		return;
	if(m_Core.Team(ClientID) == TEAM_FLOCK
			|| m_Core.Team(ClientID) == TEAM_SUPER)
	{
		pStartingChar->m_DDRaceState = DDRACE_STARTED;
		pStartingChar->m_StartTime = Tick;
		return;
	}
	bool Waiting = false;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_Core.Team(ClientID) != m_Core.Team(i))
			continue;
		CPlayer* pPlayer = GetPlayer(i);
		if(!pPlayer || !pPlayer->IsPlaying())
			continue;
		if(GetDDRaceState(pPlayer) != DDRACE_FINISHED)
			continue;

		Waiting = true;
		pStartingChar->m_DDRaceState = DDRACE_NONE;

		if(m_LastChat[ClientID] + Server()->TickSpeed()
				+ GameServer()->Config()->m_SvChatDelay < Tick)
		{
			char aBuf[128];
			str_format(
					aBuf,
					sizeof(aBuf),
					pStartingChar->GetPlayer()->Localize("%s has finished and didn't go through start yet, wait for him or join another team."),
					Server()->ClientName(i));
			GameServer()->SendChatTarget(ClientID, aBuf);
			m_LastChat[ClientID] = Tick;
		}
		if(m_LastChat[i] + Server()->TickSpeed()
				+ GameServer()->Config()->m_SvChatDelay < Tick)
		{
			char aBuf[128];
			str_format(
					aBuf,
					sizeof(aBuf),
					pPlayer->Localize("%s wants to start a new round, kill or walk to start."),
					Server()->ClientName(ClientID));
			GameServer()->SendChatTarget(i, aBuf);
			m_LastChat[i] = Tick;
		}
	}

	if(m_TeamState[m_Core.Team(ClientID)] < TEAMSTATE_STARTED && !Waiting)
	{
		ChangeTeamState(m_Core.Team(ClientID), TEAMSTATE_STARTED);

		char aBuf[512];
		str_format(
				aBuf,
				sizeof(aBuf),
				Localizable("Team %d started with these %d players:"),
				m_Core.Team(ClientID),
				Count(m_Core.Team(ClientID)));

		bool First = true;

		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(m_Core.Team(ClientID) == m_Core.Team(i))
			{
				CPlayer* pPlayer = GetPlayer(i);
				// TODO: THE PROBLEM IS THAT THERE IS NO CHARACTER SO START TIME CAN'T BE SET!
				if(pPlayer && (pPlayer->IsPlaying() || TeamLocked(m_Core.Team(ClientID))))
				{
					SetDDRaceState(pPlayer, DDRACE_STARTED);
					SetStartTime(pPlayer, Tick);

					if(First)
					{
						// For translation, removed space at end
						str_append(aBuf, " ", sizeof(aBuf));
						First = false;
					}
					else
					{
						str_append(aBuf, ", ", sizeof(aBuf));
					}

					str_append(aBuf, GameServer()->Server()->ClientName(i), sizeof(aBuf));
				}
			}
		}

		if(GameServer()->Config()->m_SvTeam < 3 && GameServer()->Config()->m_SvTeamMaxSize != 2 && GameServer()->Config()->m_SvPauseable)
		{
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				CPlayer* pPlayer = GetPlayer(i);
				if(m_Core.Team(ClientID) == m_Core.Team(i) && pPlayer && (pPlayer->IsPlaying() || TeamLocked(m_Core.Team(ClientID))))
				{
					GameServer()->SendChatTarget(i, aBuf);
				}
			}
		}
	}
}

void CGameTeams::OnCharacterFinish(int ClientID)
{
	if (m_Core.Team(ClientID) == TEAM_FLOCK
			|| m_Core.Team(ClientID) == TEAM_SUPER)
	{
		CPlayer* pPlayer = GetPlayer(ClientID);
		if (pPlayer && pPlayer->IsPlaying())
		{
			float Time = (float)(Server()->Tick() - GetStartTime(pPlayer))
					/ ((float)Server()->TickSpeed());
			if (Time < 0.000001f)
				return;
			char aTimestamp[TIMESTAMP_STR_LENGTH];
			str_timestamp_format(aTimestamp, sizeof(aTimestamp), FORMAT_SPACE); // 2019-04-02 19:41:58

			OnFinish(pPlayer, Time, aTimestamp);
		}
	}
	else
	{
		m_TeeFinished[ClientID] = true;

		CheckTeamFinished(m_Core.Team(ClientID));
	}
}

void CGameTeams::CheckTeamFinished(int Team)
{
	if (TeamFinished(Team))
	{
		CPlayer *TeamPlayers[MAX_CLIENTS];
		unsigned int PlayersCount = 0;

		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (Team == m_Core.Team(i))
			{
				CPlayer* pPlayer = GetPlayer(i);
				if (pPlayer && pPlayer->IsPlaying())
				{
					m_TeeFinished[i] = false;

					TeamPlayers[PlayersCount++] = pPlayer;
				}
			}
		}

		if (PlayersCount > 0)
		{
			float Time = (float)(Server()->Tick() - GetStartTime(TeamPlayers[0]))
					/ ((float)Server()->TickSpeed());
			if (Time < 0.000001f)
			{
				return;
			}

			if(m_Practice[Team])
			{
				ChangeTeamState(Team, TEAMSTATE_FINISHED);

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf),
					Localizable("Your team would've finished in: %d minute(s) %5.2f second(s). Since you had practice mode enabled your rank doesn't count."),
					(int)Time / 60, Time - ((int)Time / 60 * 60));

				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
					{
						GameServer()->SendChatTarget(i, GameServer()->m_apPlayers[i]->Localize(aBuf));
					}
				}

				for(unsigned int i = 0; i < PlayersCount; ++i)
				{
					SetDDRaceState(TeamPlayers[i], DDRACE_FINISHED);
				}

				return;
			}

			char aTimestamp[TIMESTAMP_STR_LENGTH];
			str_timestamp_format(aTimestamp, sizeof(aTimestamp), FORMAT_SPACE); // 2019-04-02 19:41:58

			for (unsigned int i = 0; i < PlayersCount; ++i)
				OnFinish(TeamPlayers[i], Time, aTimestamp);
			ChangeTeamState(Team, TEAMSTATE_FINISHED); //TODO: Make it better
			//ChangeTeamState(Team, TEAMSTATE_OPEN);
			OnTeamFinish(TeamPlayers, PlayersCount, Time, aTimestamp);
		}
	}
}

bool CGameTeams::SetCharacterTeam(int ClientID, int Team)
{
	//Check on wrong parameters. +1 for TEAM_SUPER
	if (ClientID < 0 || ClientID >= MAX_CLIENTS || Team < 0
			|| Team >= MAX_CLIENTS + 1)
		return false;
	//You can join to TEAM_SUPER at any time, but any other group you cannot if it started
	if (Team != TEAM_SUPER && m_TeamState[Team] > TEAMSTATE_OPEN)
		return false;
	//No need to switch team if you there
	if (m_Core.Team(ClientID) == Team)
		return false;
	if (!Character(ClientID))
		return false;
	//You cannot be in TEAM_SUPER if you not super
	if (Team == TEAM_SUPER && !Character(ClientID)->m_Super)
		return false;
	//if you begin race
	if (Character(ClientID)->m_DDRaceState != DDRACE_NONE && Team != TEAM_SUPER)
		return false;
	//No cheating through noob filter with practice and then leaving team
	if (m_Practice[m_Core.Team(ClientID)])
		return false;

	SetForceCharacterTeam(ClientID, Team);

	//GameServer()->CreatePlayerSpawn(Character(id)->m_Core.m_Pos, TeamMask());
	return true;
}

void CGameTeams::SetForceCharacterTeam(int ClientID, int Team)
{
	int OldTeam = m_Core.Team(ClientID);

	if (Team != m_Core.Team(ClientID))
		ForceLeaveTeam(ClientID);
	else
	{
		m_TeeFinished[ClientID] = false;
		if (Count(m_Core.Team(ClientID)) > 0)
			m_MembersCount[m_Core.Team(ClientID)]--;
	}

	SetForceCharacterNewTeam(ClientID, Team);

	if (OldTeam != Team)
	{
		for(int LoopClientID = 0; LoopClientID < MAX_CLIENTS; ++LoopClientID)
			if(GetPlayer(LoopClientID))
				SendTeamsState(LoopClientID);

		if(GetPlayer(ClientID))
			GetPlayer(ClientID)->m_VotedForPractice = false;
	}
}

void CGameTeams::SetForceCharacterNewTeam(int ClientID, int Team)
{
	m_Core.Team(ClientID, Team);

	if (m_Core.Team(ClientID) != TEAM_SUPER)
		m_MembersCount[m_Core.Team(ClientID)]++;
	if (Team != TEAM_SUPER && (m_TeamState[Team] == TEAMSTATE_EMPTY || m_TeamLocked[Team]))
	{
		if (!m_TeamLocked[Team])
			ChangeTeamState(Team, TEAMSTATE_OPEN);

		if (GameServer()->Collision()->m_HighestSwitchNumber > 0) {
			for (int i = 0; i < GameServer()->Collision()->m_HighestSwitchNumber+1; ++i)
			{
				GameServer()->Collision()->m_pSwitchers[i].m_Status[Team] = GameServer()->Collision()->m_pSwitchers[i].m_Initial;
				GameServer()->Collision()->m_pSwitchers[i].m_EndTick[Team] = 0;
				GameServer()->Collision()->m_pSwitchers[i].m_Type[Team] = TILE_SWITCHOPEN;
			}
		}
	}
}

void CGameTeams::ForceLeaveTeam(int ClientID)
{
	m_TeeFinished[ClientID] = false;

	if (m_Core.Team(ClientID) != TEAM_FLOCK
			&& m_Core.Team(ClientID) != TEAM_SUPER
			&& m_TeamState[m_Core.Team(ClientID)] != TEAMSTATE_EMPTY)
	{
		bool NoOneInOldTeam = true;
		for (int i = 0; i < MAX_CLIENTS; ++i)
			if (i != ClientID && m_Core.Team(ClientID) == m_Core.Team(i))
			{
				NoOneInOldTeam = false; //all good exists someone in old team
				break;
			}
		if (NoOneInOldTeam)
		{
			m_TeamState[m_Core.Team(ClientID)] = TEAMSTATE_EMPTY;

			// unlock team when last player leaves
			SetTeamLock(m_Core.Team(ClientID), false);
			ResetInvited(m_Core.Team(ClientID));
			m_Practice[m_Core.Team(ClientID)] = false;
		}
	}

	if (Count(m_Core.Team(ClientID)) > 0)
		m_MembersCount[m_Core.Team(ClientID)]--;
}

int CGameTeams::Count(int Team) const
{
	if (Team == TEAM_SUPER)
		return -1;
	return m_MembersCount[Team];
}

void CGameTeams::ChangeTeamState(int Team, int State)
{
	int OldState = m_TeamState[Team];
	m_TeamState[Team] = State;
	onChangeTeamState(Team, State, OldState);
}

void CGameTeams::onChangeTeamState(int Team, int State, int OldState)
{
	if (OldState != State && State == TEAMSTATE_STARTED)
	{
		// OnTeamStateStarting
	}
	if (OldState != State && State == TEAMSTATE_FINISHED)
	{
		// OnTeamStateFinishing
	}
}

bool CGameTeams::TeamFinished(int Team)
{
	for (int i = 0; i < MAX_CLIENTS; ++i)
		if (m_Core.Team(i) == Team && !m_TeeFinished[i])
			return false;
	return true;
}

Mask128 CGameTeams::TeamMask(int Team, int ExceptID, int Asker, bool SevendownOnly)
{
	Mask128 Mask = CmaskNone();

	if(Team == TEAM_SUPER)
	{
		if (ExceptID == -1)
			return CmaskAll();
		return CmaskAllExceptOne(ExceptID);
	}

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (i == ExceptID)
			continue; // Explicitly excluded
		if (!GetPlayer(i))
			continue; // Player doesn't exist

		if (SevendownOnly && !Server()->IsSevendown(i))
			continue; // sevendown only because 0.7 clients handle hook sounds clientside so we exclude them

		if (!(GetPlayer(i)->GetTeam() == -1 || GetPlayer(i)->IsPaused()))
		{ // Not spectator
			if (i != Asker)
			{ // Actions of other players
				if (!Character(i))
					continue; // Player is currently dead
				if (!GetPlayer(i)->m_ShowOthers)
				{
					if (m_Core.GetSolo(Asker))
						continue; // When in solo part don't show others
					if (m_Core.GetSolo(i))
						continue; // When in solo part don't show others
					if (m_Core.Team(i) != Team && m_Core.Team(i) != TEAM_SUPER)
						continue; // In different teams
				} // ShowOthers
			} // See everything of yourself
		}
		else if (GetPlayer(i)->GetSpecMode() == SPEC_PLAYER)
		{ // Spectating specific player
			if (GetPlayer(i)->GetSpectatorID() != Asker)
			{ // Actions of other players
				if (!Character(GetPlayer(i)->GetSpectatorID()))
					continue; // Player is currently dead
				if (!GetPlayer(i)->m_ShowOthers)
				{
					if (m_Core.GetSolo(Asker))
						continue; // When in solo part don't show others
					if (m_Core.GetSolo(GetPlayer(i)->GetSpectatorID()))
						continue; // When in solo part don't show others
					if (m_Core.Team(GetPlayer(i)->GetSpectatorID()) != Team && m_Core.Team(GetPlayer(i)->GetSpectatorID()) != TEAM_SUPER)
						continue; // In different teams
				} // ShowOthers
			} // See everything of player you're spectating
		}
		else
		{ // Freeview
			if (GetPlayer(i)->m_SpecTeam)
			{ // Show only players in own team when spectating
				if (m_Core.Team(i) != Team && m_Core.Team(i) != TEAM_SUPER)
					continue; // in different teams
			}
		}

		Mask |= CmaskOne(i);
	}
	return Mask;
}

void CGameTeams::SendTeamsState(int ClientID)
{
	if (GameServer()->Config()->m_SvTeam == 3 || !m_pGameContext->m_apPlayers[ClientID])
		return;

	int DDNetVersion = m_pGameContext->GetClientDDNetVersion(ClientID);
	if (DDNetVersion < VERSION_DDNET)
		return;

	bool LegacyTeams = DDNetVersion >= VERSION_DDNET_UNIQUE_TEAMS;
	CMsgPacker Msg(NETMSGTYPE_SV_TEAMSSTATE);

	for(int i = 0; i < Server()->GetMaxClients(ClientID); i++)
	{
		if (Server()->IsSevendown(ClientID) && GameServer()->FlagsUsed())
		{
			int Team = -1;
			if (i == GameServer()->m_World.GetSpecSelectFlag(ClientID, SPEC_FLAGRED))
				Team = LegacyTeams ? 56 : 63; // red colored team, used 1 before but that is the most common team for 1vs1
			else if (i == GameServer()->m_World.GetSpecSelectFlag(ClientID, SPEC_FLAGBLUE))
				Team = LegacyTeams ? 60 : 36; // blue colored team
			// try to make hook visible in most cases, i dont want to use TEAM_SUPER cause that would make names red when NONAME e.g. spookyghost
			// but this right now means, that if u r in a team and ur dummy isnt, and u swap, and both try to hook the flag, one will have invisible hook
			else if (i == Server()->GetMaxClients(ClientID)-1 && Server()->IsSevendown(ClientID))
			{
				Team = m_Core.Team(ClientID);
				if (Team == TEAM_SUPER) // Todo: Rainbowname.cpp, TEAM_SUPER but it's 128
					Team = VANILLA_MAX_CLIENTS;
			}
			

			if (Team != -1)
			{
				Msg.AddInt(Team);
				continue;
			}
		}

		// durak handling
		int DurakTeam = GameServer()->Durak()->GetTeam(ClientID, i);
		if (DurakTeam != -1)
		{
			Msg.AddInt(DurakTeam);
			continue;
		}

		// see others selector
		int Indicator = GameServer()->m_World.GetSeeOthersInd(ClientID, i);
		if (Indicator != -1)
		{
			int Team = -1;
			if (Indicator == CGameWorld::SEE_OTHERS_IND_BUTTON)
				Team = LegacyTeams ? 49 : 25;
			else if (Indicator == CGameWorld::SEE_OTHERS_IND_PLAYER)
				Team = LegacyTeams ? 28 : 24;

			if (Team != -1)
			{
				Msg.AddInt(Team);
				continue;
			}
		}

		// Rainbow name
		int Color = m_pGameContext->m_RainbowName.GetColor(ClientID, i);
		if (Color != -1)
		{
			int Team = LegacyTeams ? s_aLegacyTeams[Color] : Color;

			// If color is >= 56 we simply use the previous color, so that the spectate menu won't change order all the time.
			bool IsSpec = LegacyTeams && Team >= 56 && Team < VANILLA_MAX_CLIENTS && (GameServer()->m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS || GameServer()->m_apPlayers[ClientID]->IsPaused());
			if (Team == -2 || IsSpec) // TEAM_SUPER
			{
				// keep the previous color
				Team = s_aLegacyTeams[Color - 1];
			}

			Msg.AddInt(Team);
			continue;
		}

		// Else -> Normal teams
		int Team = 0;
		int ID = i;
		if (Server()->ReverseTranslate(ID, ClientID))
		{
			Team = m_Core.Team(ID);
			if (Team == TEAM_SUPER)
				Team = VANILLA_MAX_CLIENTS;
			else if (Team > VANILLA_MAX_CLIENTS)
				Team = 0;
		}
		Msg.AddInt(Team);
	}

	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

int CGameTeams::GetDDRaceState(CPlayer* Player)
{
	if (!Player)
		return DDRACE_NONE;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		return pChar->m_DDRaceState;
	return DDRACE_NONE;
}

void CGameTeams::SetDDRaceState(CPlayer* Player, int DDRaceState)
{
	if (!Player)
		return;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		pChar->m_DDRaceState = DDRaceState;
}

int CGameTeams::GetStartTime(CPlayer* Player)
{
	if (!Player)
		return 0;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		return pChar->m_StartTime;
	return 0;
}

void CGameTeams::SetStartTime(CPlayer* Player, int StartTime)
{
	if (!Player)
		return;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		pChar->m_StartTime = StartTime;
}

void CGameTeams::SetCpActive(CPlayer* Player, int CpActive)
{
	if (!Player)
		return;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		pChar->m_CpActive = CpActive;
}

float *CGameTeams::GetCpCurrent(CPlayer* Player)
{
	if (!Player)
		return NULL;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		return pChar->m_CpCurrent;
	return NULL;
}

void CGameTeams::OnTeamFinish(CPlayer** Players, unsigned int Size, float Time, const char *pTimestamp)
{
	bool CallSaveScore = false;

#if defined(CONF_SQL)
	CallSaveScore = GameServer()->Config()->m_SvUseSQL;
#endif

	int PlayerCIDs[MAX_CLIENTS];

	for(unsigned int i = 0; i < Size; i++)
	{
		PlayerCIDs[i] = Players[i]->GetCID();

		if(GameServer()->Config()->m_SvRejoinTeam0 && GameServer()->Config()->m_SvTeam != 3 && (m_Core.Team(Players[i]->GetCID()) >= TEAM_SUPER || !m_TeamLocked[m_Core.Team(Players[i]->GetCID())]))
		{
			SetForceCharacterTeam(Players[i]->GetCID(), 0);
			GameServer()->SendChatFormat(-1, CHAT_ALL, -1, CGameContext::CHATFLAG_ALL, Localizable("%s joined team 0"),
				GameServer()->Server()->ClientName(Players[i]->GetCID()));
		}
	}

	if (CallSaveScore && Size >= 2)
		GameServer()->Score()->SaveTeamScore(PlayerCIDs, Size, Time, pTimestamp);
}

void CGameTeams::OnFinish(CPlayer* Player, float Time, const char *pTimestamp)
{
	if (!Player || !Player->IsPlaying())
		return;
	//TODO:DDRace:btd: this ugly
	CPlayerData *pData = GameServer()->Score()->PlayerData(Player->GetCID());
	char aBuf[128];
	SetCpActive(Player, -2);
	str_format(aBuf, sizeof(aBuf),
			Localizable("%s finished in: %d minute(s) %5.2f second(s)"),
			Server()->ClientName(Player->GetCID()), (int)Time / 60,
			Time - ((int)Time / 60 * 60));
	if (GameServer()->Config()->m_SvHideScore)
		GameServer()->SendChatTarget(Player->GetCID(), aBuf);
	else
		GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);

	float Diff = fabs(Time - pData->m_BestTime);

	if (Time - pData->m_BestTime < 0)
	{
		// new record \o/

		CFormatArg aArgs[2];
		int NumArgs = 1;
		if (Diff >= 60)
		{
			str_copy(aBuf, Localizable("New record: %d minute(s) %5.2f second(s) better."), sizeof(aBuf));
			aArgs[0] = (int)Diff / 60;
			aArgs[1] = Diff - ((int)Diff / 60 * 60);
			NumArgs++;
		}
		else
		{
			str_copy(aBuf, Localizable("New record: %5.2f second(s) better."), sizeof(aBuf));
			aArgs[0] = Diff;
			aArgs[1] = 0;
		}
		if (GameServer()->Config()->m_SvHideScore)
			GameServer()->SendChat(-1, CHAT_ALL, Player->GetCID(), aBuf, -1, CGameContext::CHATFLAG_ALL, aArgs, NumArgs);
		else
			GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf, -1, CGameContext::CHATFLAG_ALL, aArgs, NumArgs);
	}
	else if (pData->m_BestTime != 0) // tee has already finished?
	{
		if (Diff <= 0.005f)
		{
			GameServer()->SendChatTarget(Player->GetCID(), Player->Localize("You finished with your best time."));
		}
		else
		{
			if (Diff >= 60)
				str_format(aBuf, sizeof(aBuf), Player->Localize("%d minute(s) %5.2f second(s) worse, better luck next time."),
						(int)Diff / 60, Diff - ((int)Diff / 60 * 60));
			else
				str_format(aBuf, sizeof(aBuf),
						Player->Localize("%5.2f second(s) worse, better luck next time."),
						Diff);
			GameServer()->SendChatTarget(Player->GetCID(), aBuf); //this is private, sent only to the tee
		}
	}

	bool CallSaveScore = GameServer()->Config()->m_SvSaveWorseScores;

	if (!pData->m_BestTime || Time < pData->m_BestTime)
	{
		// update the score
		pData->Set(Time, GetCpCurrent(Player));
		CallSaveScore = true;
	}

	if (CallSaveScore)
		if (GameServer()->Config()->m_SvNamelessScore || !str_startswith(Server()->ClientName(Player->GetCID()), "nameless tee"))
			GameServer()->Score()->SaveScore(Player->GetCID(), Time, pTimestamp,
					GetCpCurrent(Player), Player->m_NotEligibleForFinish);

	// update server best time
	if (GameServer()->m_pController->m_CurrentRecord == 0
			|| Time < GameServer()->m_pController->m_CurrentRecord)
	{
		// check for nameless
		if (GameServer()->Config()->m_SvNamelessScore || !str_startswith(Server()->ClientName(Player->GetCID()), "nameless tee"))
		{
			GameServer()->m_pController->m_CurrentRecord = Time;
			//dbg_msg("character", "Finish");
		}
	}

	SetDDRaceState(Player, DDRACE_FINISHED);
	// set player score
	if (!pData->m_CurrentTime || pData->m_CurrentTime > Time)
	{
		pData->m_CurrentTime = Time;
	}

	int TTime = 0 - (int)Time;
	if (Player->m_Score < TTime || !Player->m_HasFinishScore)
	{
		Player->m_Score = TTime;
		Player->m_HasFinishScore = true;
	}

	// F-DDrace
	Player->m_Score = GameServer()->Score()->PlayerData(Player->GetCID())->m_BestTime;

	// Confetti
	CCharacter *pChar = Player->GetCharacter();
	m_pGameContext->CreateFinishConfetti(pChar->GetPos(), pChar->TeamMask());
}

void CGameTeams::OnCharacterSpawn(int ClientID)
{
	m_Core.SetSolo(ClientID, false);

	if ((m_Core.Team(ClientID) >= TEAM_SUPER || !m_TeamLocked[m_Core.Team(ClientID)])
		&& !GameServer()->Arenas()->FightStarted(ClientID) && !GameServer()->Durak()->InDurakGame(ClientID))
		// Important to only set a new team here, don't remove from an existing
		// team since a newly joined player does by definition not have an old team
		// to remove from. Doing so would destroy the count in m_MembersCount.
		SetForceCharacterNewTeam(ClientID, 0);
}

void CGameTeams::OnCharacterDeath(int ClientID, int Weapon)
{
	m_Core.SetSolo(ClientID, false);

	// we don't need team updating on every kill
	if (GameServer()->Arenas()->FightStarted(ClientID) || GameServer()->Durak()->InDurakGame(ClientID))
		return;

	int Team = m_Core.Team(ClientID);
	bool Locked = TeamLocked(Team) && Weapon != WEAPON_GAME;

	if(!Locked)
	{
		SetForceCharacterTeam(ClientID, 0);
		CheckTeamFinished(Team);
	}
	else
	{
		SetForceCharacterTeam(ClientID, Team);

		if(GetTeamState(Team) != TEAMSTATE_OPEN)
		{
			ChangeTeamState(Team, CGameTeams::TEAMSTATE_OPEN);

			char aBuf[512];
			if (Weapon == WEAPON_SELF)
				str_copy(aBuf, Localizable("Everyone in your locked team was killed because '%s' killed."), sizeof(aBuf));
			else
				str_copy(aBuf, Localizable("Everyone in your locked team was killed because '%s' died."), sizeof(aBuf));

			m_Practice[Team] = false;

			for(int i = 0; i < MAX_CLIENTS; i++)
				if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
				{
					GameServer()->m_apPlayers[i]->m_VotedForPractice = false;

					if(i != ClientID)
					{
						GameServer()->m_apPlayers[i]->KillCharacter(WEAPON_SELF);
						if (Weapon == WEAPON_SELF)
							GameServer()->m_apPlayers[i]->Respawn(true); // spawn the rest of team with weak hook on the killer
					}
					if(m_MembersCount[Team] > 1)
						GameServer()->SendChatTarget(i, GameServer()->m_apPlayers[i]->Localize(aBuf));
				}
		}
	}
}

void CGameTeams::SetTeamLock(int Team, bool Lock)
{
	if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
		m_TeamLocked[Team] = Lock;
}

void CGameTeams::ResetInvited(int Team)
{
	m_Invited[Team] = CmaskNone();
}

void CGameTeams::SetClientInvited(int Team, int ClientID, bool Invited)
{
	if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
	{
		if(Invited)
			m_Invited[Team] |= CmaskOne(ClientID);
		else
			m_Invited[Team] &= ~CmaskOne(ClientID);
	}
}

void CGameTeams::KillSavedTeam(int Team)
{
	// Set so that no finish is accidentally given to some of the players
	ChangeTeamState(Team, CGameTeams::TEAMSTATE_OPEN);

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
		{
			// Set so that no finish is accidentally given to some of the players
			GameServer()->m_apPlayers[i]->GetCharacter()->m_DDRaceState = DDRACE_NONE;
			m_TeeFinished[i] = false;
		}
	}

	for (int i = 0; i < MAX_CLIENTS; i++)
		if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
			GameServer()->m_apPlayers[i]->ThreadKillCharacter(-2);

	ChangeTeamState(Team, CGameTeams::TEAMSTATE_EMPTY);

	// unlock team when last player leaves
	SetTeamLock(Team, false);
	ResetInvited(Team);

	m_Practice[Team] = false;
}
