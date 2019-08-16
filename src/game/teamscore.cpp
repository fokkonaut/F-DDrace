/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "teamscore.h"

CTeamsCore::CTeamsCore()
{
	Reset();
}

bool CTeamsCore::SameTeam(int ClientID1, int ClientID2)
{
	return m_Team[ClientID1] == m_Team[ClientID2];
}

int CTeamsCore::Team(int ClientID)
{
	return m_Team[ClientID];
}

void CTeamsCore::Team(int ClientID, int Team)
{
	m_Team[ClientID] = Team;
}

bool CTeamsCore::CanKeepHook(int ClientID1, int ClientID2)
{
	if (m_Team[ClientID1] == TEAM_SUPER || m_Team[ClientID2] == TEAM_SUPER || ClientID1 == ClientID2)
		return true;
	return m_Team[ClientID1] == m_Team[ClientID2];
}

bool CTeamsCore::CanCollide(int ClientID1, int ClientID2, bool CheckPassive)
{
	if (m_Team[ClientID1] == TEAM_SUPER || m_Team[ClientID2] == TEAM_SUPER || ClientID1 == ClientID2)
		return true;
	if (m_IsSolo[ClientID1] || m_IsSolo[ClientID2])
		return false;
	if (CheckPassive && (m_IsPassive[ClientID1] || m_IsPassive[ClientID2]))
		return false;
	return m_Team[ClientID1] == m_Team[ClientID2];
}

void CTeamsCore::Reset()
{
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_Team[i] = TEAM_FLOCK;
		m_IsSolo[i] = false;
		m_IsPassive[i] = false;
	}
}
