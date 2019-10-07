#include <game/server/gamecontext.h>
#include "character.h"
#include <game/server/player.h>

void CCharacter::JobWindow(int Dir)
{
	m_JobMotdTick = 0;

	if (Dir == 0)
		m_JobWindowPage = MENU_PAGE_MAIN;
	else if (Dir == 1)
	{
		m_JobWindowPage++;
		if (m_JobWindowPage >= NUM_JOBS)
			m_JobWindowPage = MENU_PAGE_MAIN;
	}
	else if (Dir == -1)
	{
		m_JobWindowPage--;
		if (m_JobWindowPage < MENU_PAGE_MAIN)
			m_JobWindowPage = NUM_JOBS-1;
	}

	char aJob[256];
	char aLevelTmp[128];
	char aInfo[1028];

	if (m_JobWindowPage == 0)
	{
		str_format(aJob, sizeof(aJob), "Hello! You can choose a job here.\n\n"
			"By shooting to the right you go one site forward,\n"
			"and by shooting left you go one site\n"
			"backwards.");
	}
	else if (m_JobWindowPage == 1)
	{
		str_format(aJob, sizeof(aJob), "        ~  F A R M E R  ~      ");
		str_format(aLevelTmp, sizeof(aLevelTmp), "10");
		str_format(aInfo, sizeof(aInfo), "Farmers can plant some plants. Later, when\n"
			"they are fully grown, they can sell them for money.");
	}

	if (m_JobWindowPage == 2)
	{
		str_format(aJob, sizeof(aJob), "        ~  D E A L E R  ~      ");
		str_format(aLevelTmp, sizeof(aLevelTmp), "30");
		str_format(aInfo, sizeof(aInfo), "Drug dealers buy and sell drugs.\n");
	}
	else
	{
		aJob[0] = 0;
	}

	char aLevel[128];
	str_format(aLevel, sizeof(aLevel), "Needed level: %s", aLevelTmp);

	char aBase[512];
	if (m_JobWindowPage > MENU_PAGE_MAIN)
	{
		str_format(aBase, sizeof(aBase),
			"***************************\n"
			"        ~  J O B S  ~      \n"
			"***************************\n\n"
			"%s\n\n"
			"%s\n\n"
			"%s\n\n"
			"***************************\n"
			"If you want to get a job press f3.\n\n\n"
			"              ~ %d ~              ", aJob, aLevel, aInfo, m_JobWindowPage);
	}
	else
	{
		str_format(aBase, sizeof(aBase),
			"***************************\n"
			"        ~  J O B S  ~      \n"
			"***************************\n\n"
			"%s\n\n"
			"***************************\n"
			"If you want to get a job press f3.", aJob);
	}

	GameServer()->SendMotd(aBase, GetPlayer()->GetCID());
	m_JobMotdTick = Server()->Tick() + Server()->TickSpeed() * 10; // motd is there for 10 sec
}

void CCharacter::ConfirmJob()
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf),
		"***************************\n"
		"        ~  J O B S  ~      \n"
		"***************************\n\n"
		"Are you sure you want to get this job?\n\n"
		"f3 - yes\n"
		"f4 - no\n\n"
		"***************************\n");

	GameServer()->SendMotd(aBuf, GetPlayer()->GetCID());
	m_JobFinderState = MENU_STATE_CONFIRM;;
}

void CCharacter::JobFinderEnd(bool Canceled)
{
	char aResult[256];
	if (Canceled)
	{
		char aBuf[256];
		str_format(aResult, sizeof(aResult), "You canceled the application.");
		str_format(aBuf, sizeof(aBuf),
			"***************************\n"
			"        ~  J O B S  ~      \n"
			"***************************\n\n"
			"%s\n\n"
			"***************************\n", aResult);

		GameServer()->SendMotd(aBuf, GetPlayer()->GetCID());
	}
	else
	{
		GetJob(m_JobWindowPage);
		JobWindow(MENU_STATE_NONE);
	}

	m_JobFinderState = MENU_STATE_OPENED_WINDOW;
}

void CCharacter::GetJob(int JobID)
{
	if (JobID < 0 || JobID >= NUM_JOBS)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Invalid job");
		return;
	}

	CGameContext::AccountInfo *Account = &GameServer()->m_Accounts[m_pPlayer->GetAccID()];

	char aMsg[128];
	char aJob[64];
	int Level = -1;

	switch (JobID)
	{
		case 1:
		{
			str_format(aJob, sizeof(aJob), "Farmer");
			Level = 10;
		} break;
	}

	if ((*Account).m_Level < Level)
	{
		str_format(aMsg, sizeof(aMsg), "Your level is too low, you need to be level %d to be a %s", Level, aJob);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), aMsg);
		return;
	}

	if (JobID-1 == (*Account).m_Job)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You can't apply for your current job");
		return;
	}

	str_format(aMsg, sizeof(aMsg), "You are now a %s", aJob);
	GameServer()->SendChatTarget(m_pPlayer->GetCID(), aMsg);
	(*Account).m_Job = JobID-1;
}