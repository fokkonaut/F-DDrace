// made by fokkonaut

#include "house.h"
#include <game/server/gamecontext.h>
#include <engine/shared/config.h>

IServer *CHouse::Server() { return GameServer()->Server(); }

CHouse::CHouse(CGameContext *pGameServer, int Type)
{
	m_pGameServer = pGameServer;
	m_Type = Type;

	switch (m_Type)
	{
	case HOUSE_SHOP: m_pHeadline = "~ S H O P ~"; break;
	case HOUSE_PLOT_SHOP: m_pHeadline = "~ P L O T - S H O P ~"; break;
	case HOUSE_BANK: m_pHeadline = "~ B A N K ~"; break;
	default: m_pHeadline = "~ I N V A L I D ~";
	}

	for (int i = 0; i < MAX_CLIENTS; i++)
		Reset(i);
}

void CHouse::Reset(int ClientID)
{
	m_aClients[ClientID].m_Inside = false;
	m_aClients[ClientID].m_LastMotd = 0;
	m_aClients[ClientID].m_NextMsg = Server()->Tick();
	m_aClients[ClientID].m_Page = PAGE_NONE;
	m_aClients[ClientID].m_State = STATE_NONE;
}

void CHouse::SendWindow(int ClientID, const char *pMsg, const char *pFooterMsg)
{
	char aMsg[900];
	const char *pCut = "**************************************\n";

	char aPage[8] = "";
	if (m_aClients[ClientID].m_Page > PAGE_MAIN && m_Type != HOUSE_BANK)
		str_format(aPage, sizeof(aPage), "~ %d ~", m_aClients[ClientID].m_Page);

	char aFooter[128];
	str_format(aFooter, sizeof(aFooter), "%s%s\n                       %s", pCut, pFooterMsg, aPage);

	str_format(aMsg, sizeof(aMsg),
		"%s"
		"                  %s\n"
		"%s\n"
		"%s", pCut, m_pHeadline, pCut, pMsg);

	GameServer()->SendMotd(GameServer()->AppendMotdFooter(aMsg, aFooter), ClientID);
}

void CHouse::Tick(int ClientID)
{
	if (m_aClients[ClientID].m_LastMotd < Server()->Tick() - Server()->TickSpeed() * 10)
	{
		m_aClients[ClientID].m_Page = PAGE_NONE;
		m_aClients[ClientID].m_State = STATE_NONE;
	}

	if (m_aClients[ClientID].m_Inside)
	{
		if (Server()->Tick() % 50 == 0)
			GameServer()->SendBroadcast(m_pHeadline, ClientID, false);
	}
}

void CHouse::OnEnter(int ClientID)
{
	if (m_aClients[ClientID].m_Inside)
		return;

	m_aClients[ClientID].m_Inside = true;

	if (GameServer()->IsHouseDummy(ClientID, m_Type))
		return;

	if (m_aClients[ClientID].m_NextMsg < Server()->Tick())
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
		int From = GameServer()->m_World.GetClosestHouseDummy(pChr->GetPos(), pChr, m_Type, ClientID);
		GameServer()->SendChat(From, CHAT_SINGLE, ClientID, GetWelcomeMessage(ClientID));
	}
}

void CHouse::OnLeave(int ClientID)
{
	if (!m_aClients[ClientID].m_Inside)
		return;

	m_aClients[ClientID].m_Inside = false;

	if (GameServer()->IsHouseDummy(ClientID, m_Type))
		return;

	if (m_aClients[ClientID].m_NextMsg < Server()->Tick())
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
		int From = GameServer()->m_World.GetClosestHouseDummy(pChr->GetPos(), pChr, m_Type, ClientID);
		GameServer()->SendChat(From, CHAT_SINGLE, ClientID, "Bye! Come back if you need something.");
		m_aClients[ClientID].m_NextMsg = Server()->Tick() + Server()->TickSpeed() * 5;
	}

	if (m_aClients[ClientID].m_Page != PAGE_NONE)
		GameServer()->SendMotd("", ClientID);
	GameServer()->SendBroadcast("", ClientID, false);

	m_aClients[ClientID].m_State = STATE_NONE;
	m_aClients[ClientID].m_Page = PAGE_NONE;
}

void CHouse::OnKeyPress(int ClientID, int Dir)
{
	if (m_Type == HOUSE_BANK && m_aClients[ClientID].m_State == STATE_OPENED_WINDOW)
	{
		m_aClients[ClientID].m_State = STATE_CHOSE_ASSIGNMENT;
		SetAssignment(ClientID, Dir);
		return;
	}

	if (Dir == 1)
	{
		int NeedConfirmState = m_Type == HOUSE_BANK ? STATE_CHOSE_ASSIGNMENT : STATE_OPENED_WINDOW;
		if (m_aClients[ClientID].m_State == NeedConfirmState)
		{
			if (m_aClients[ClientID].m_Page > PAGE_MAIN)
				ConfirmAssignment(ClientID);
		}
		else if (m_aClients[ClientID].m_State == STATE_CONFIRM)
		{
			EndSession(ClientID, false);
		}
	}
	else if (Dir == -1)
	{
		if (m_aClients[ClientID].m_Page == PAGE_NONE)
		{
			SetPage(ClientID, PAGE_MAIN);
			m_aClients[ClientID].m_State = STATE_OPENED_WINDOW;
		}
		else if (m_aClients[ClientID].m_State == STATE_CONFIRM)
		{
			EndSession(ClientID, true);
		}
	}
}

void CHouse::ConfirmAssignment(int ClientID)
{
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s\n\nF3: yes\nF4: no", GetConfirmMessage(ClientID));
	SendWindow(ClientID, aBuf);
	m_aClients[ClientID].m_State = STATE_CONFIRM;
}

void CHouse::EndSession(int ClientID, bool Cancelled)
{
	if (Cancelled)
	{
		SendWindow(ClientID, GetEndMessage(ClientID));
	}
	else
	{
		OnSuccess(ClientID);
		SetPage(ClientID, PAGE_MAIN);
	}

	m_aClients[ClientID].m_State = STATE_OPENED_WINDOW;
}

bool CHouse::CanChangePage(int ClientID)
{
	return m_aClients[ClientID].m_Page != PAGE_NONE && m_aClients[ClientID].m_State >= STATE_OPENED_WINDOW && m_aClients[ClientID].m_State != STATE_CONFIRM;
}

void CHouse::SetPage(int ClientID, int Page)
{
	m_aClients[ClientID].m_Page = Page;
	OnPageChange(ClientID);
}

void CHouse::DoPageChange(int ClientID, int Dir)
{
	m_aClients[ClientID].m_LastMotd = Server()->Tick();

	if (m_Type != HOUSE_BANK || m_aClients[ClientID].m_Page > PAGE_MAIN)
	{
		if (Dir == PAGE_MAIN)
		{
			m_aClients[ClientID].m_Page = PAGE_MAIN;
		}
		else if (Dir == 1)
		{
			m_aClients[ClientID].m_Page++;
			if (m_aClients[ClientID].m_Page >= NumPages())
				m_aClients[ClientID].m_Page = FirstPage();
		}
		else if (Dir == -1)
		{
			m_aClients[ClientID].m_Page--;
			if (m_aClients[ClientID].m_Page < FirstPage())
				m_aClients[ClientID].m_Page = NumPages() - 1;
		}
	}

	OnPageChange(ClientID);
}
