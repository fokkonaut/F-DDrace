// made by fokkonaut

#include "votingmenu.h"
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include <engine/shared/config.h>
#include <game/server/gamemodes/DDRace.h>
#include <engine/server/server.h>

CGameContext *CVotingMenu::GameServer() const { return m_pGameServer; }
IServer *CVotingMenu::Server() const { return GameServer()->Server(); }

// Font: https://fsymbols.com/generators/smallcaps/

// Not a normal space: https://www.cogsci.ed.ac.uk/~richard/utf-8.cgi?input=%E2%80%8A&mode=char
static const char *PLACEHOLDER_DESC = " ";
// Acc
static const char *COLLAPSE_HEADER_WANTED_PLAYERS = Localizable("Wᴀɴᴛᴇᴅ Pʟᴀʏᴇʀs", "vote-header");
static const char *COLLAPSE_HEADER_ACC_INFO = Localizable("Aᴄᴄᴏᴜɴᴛ Iɴғᴏ", "vote-header");
static const char *COLLAPSE_HEADER_ACC_STATS = Localizable("Aᴄᴄᴏᴜɴᴛ Sᴛᴀᴛs", "vote-header");
static const char *COLLAPSE_HEADER_PLOT_INFO = Localizable("Pʟᴏᴛ", "vote-header");
// Wanted // careful when adding another "Page", it has to differ somehow
static const char *ACC_WANTED_PLAYERS_PAGE = Localizable("Page");
// Acc Misc
static const char *ACC_MISC_SILENTFARM = Localizable("Silent Farm");
static const char *ACC_MISC_NINJAJETPACK = Localizable("Ninjajetpack");
// Acc Plot
static const char *ACC_PLOT_SPAWN = Localizable("Plot Spawn");
static const char *ACC_PLOT_EDIT = Localizable("Edit Plot");
static const char *ACC_PLOT_CLEAR = Localizable("Clear Plot");
// VIP
static const char *ACC_VIP_RAINBOW = Localizable("Rainbow");
static const char *ACC_VIP_BLOODY = Localizable("Bloody");
static const char *ACC_VIP_ATOM = Localizable("Atom");
static const char *ACC_VIP_TRAIL = Localizable("Trail");
static const char *ACC_VIP_SPREADGUN = Localizable("Spread Gun");
// VIP Plus
static const char *ACC_VIP_PLUS_RAINBOWHOOK = Localizable("Rainbow Hook");
static const char *ACC_VIP_PLUS_ROTATINGBALL = Localizable("Rotating Ball");
static const char *ACC_VIP_PLUS_EPICCIRCLE = Localizable("Epic Circle");
static const char *ACC_VIP_PLUS_LOVELY = Localizable("Lovely");
static const char *ACC_VIP_PLUS_RAINBOWNAME = Localizable("Rainbow Name");
static const char *ACC_VIP_PLUS_RAINBOWSPEED = Localizable("Rainbow Speed");
static const char *ACC_VIP_PLUS_SPARKLE = Localizable("Sparkle");
// Misc
static const char *MISC_HIDEDRAWINGS = Localizable("Hide Drawings");
static const char *MISC_WEAPONINDICATOR = Localizable("Weapon Indicator");
static const char *MISC_ZOOMCURSOR = Localizable("Zoom Cursor");
static const char *MISC_RESUMEMOVED = Localizable("Resume Moved");
static const char *MISC_HIDEBROADCASTS = Localizable("Hide Broadcasts");
static const char *MISC_LOCALCHAT = Localizable("Local Chat");

void CVotingMenu::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;

	m_NumCollapseEntries = 0;
	m_vWantedPlayers.clear();

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		Reset(i);
	}

	str_copy(m_aPages[PAGE_VOTES].m_aName, Localizable("Vᴏᴛᴇs", "vote-header"), sizeof(m_aPages[PAGE_VOTES].m_aName));
	str_copy(m_aPages[PAGE_ACCOUNT].m_aName, Localizable("Aᴄᴄᴏᴜɴᴛ", "vote-header"), sizeof(m_aPages[PAGE_ACCOUNT].m_aName));
	str_copy(m_aPages[PAGE_MISCELLANEOUS].m_aName, Localizable("Mɪsᴄᴇʟʟᴀɴᴇᴏᴜs", "vote-header"), sizeof(m_aPages[PAGE_MISCELLANEOUS].m_aName));
	str_copy(m_aPages[PAGE_LANGUAGES].m_aName, Localizable("Lᴀɴɢᴜᴀɢᴇs", "vote-header"), sizeof(m_aPages[PAGE_LANGUAGES].m_aName));

	for (int i = 0; i < NUM_PAGE_MAX_VOTES; i++)
		m_aaTempDesc[i][0] = 0;
	m_TempLanguage = -1;

	// Don't add the placeholders again when the map changes. The voteOptionHeap in CGameContext is not destroyed, so we do not have to add placeholders another time
	if (!m_Initialized)
	{
		AddPlaceholderVotes();
		m_Initialized = true;
	}
}

void CVotingMenu::Reset(int ClientID)
{
	m_aClients[ClientID].m_Page = PAGE_MISCELLANEOUS;
	m_aClients[ClientID].m_LastVoteMsg = 0;
	m_aClients[ClientID].m_NextVoteSendTick = 0;
	m_aClients[ClientID].m_ShowAccountInfo = false;
	m_aClients[ClientID].m_ShowPlotInfo = false;
	m_aClients[ClientID].m_ShowAccountStats = false;
	m_aClients[ClientID].m_ShowWantedPlayers = false;
	m_aClients[ClientID].m_WantedPlayersPage = 0;
}

void CVotingMenu::InitPlayer(int ClientID)
{
	if (!Server()->IsMain(ClientID))
	{
		int DummyID = Server()->GetDummy(ClientID);
		if (DummyID != -1)
		{
			m_aClients[ClientID].m_Page = m_aClients[DummyID].m_Page;
		}
	}
}

void CVotingMenu::AddPlaceholderVotes()
{
	for (int i = 0; i < NUM_OPTION_START_OFFSET; i++)
	{
		GameServer()->AddVote("placeholder_voting_menu", "info");
	}
}

const char *CVotingMenu::GetPageDescription(int ClientID, int Page)
{
	static char aBuf[VOTE_DESC_LENGTH];
	str_format(aBuf, sizeof(aBuf), "%s %s", Page == GetPage(ClientID) ? "☒" : "☐", GameServer()->m_apPlayers[ClientID]->Localize(m_aPages[Page].m_aName, "vote-header"));
	return aBuf;
}

bool CVotingMenu::SetPage(int ClientID, int Page)
{
	if (Page < 0 || Page >= NUM_PAGES || Page == GetPage(ClientID))
		return false;

	m_aClients[ClientID].m_Page = Page;
	// Update dummy page, because otherwise dummy could be in vote page, so if you switch page, swap to dummy, and vote smth, it'll actually call a vote
	int DummyID = Server()->GetDummy(ClientID);
	if (DummyID != -1)
		m_aClients[DummyID].m_Page = Page;

	// Don't process normal votes, e.g. when another vote get's added
	if (Page != PAGE_VOTES)
	{
		if (GameServer()->m_apPlayers[ClientID]->m_SendVoteIndex != -1)
		{
			CNetMsg_Sv_VoteOptionGroupEnd EndMsg;
			Server()->SendPackMsg(&EndMsg, MSGFLAG_VITAL, ClientID);
			GameServer()->m_apPlayers[ClientID]->m_SendVoteIndex = -1;
			if (DummyID != -1)
				GameServer()->m_apPlayers[DummyID]->m_SendVoteIndex = -1;
		}
	}
	SendPageVotes(ClientID);
	return true;
}

void CVotingMenu::OnProgressVoteOptions(int ClientID, CMsgPacker *pPacker, int *pCurIndex, CVoteOptionServer **ppCurrent)
{
	// Just started sending votes, add the pages
	bool IsPageVotes = GetPage(ClientID) == PAGE_VOTES;
	int Index = IsPageVotes ? GameServer()->m_apPlayers[ClientID]->m_SendVoteIndex : 0;
	if (IsPageVotes && (Index < 0 || Index >= NUM_OPTION_START_OFFSET))
		return;

	// Depending on votesPerTick... That's why we start with an offset maybe
	for (int i = Index; i < NUM_OPTION_START_OFFSET; i++)
	{
		if (i < NUM_PAGES)
		{
			pPacker->AddString(GetPageDescription(ClientID, i), VOTE_DESC_LENGTH);
		}
		else
		{
			pPacker->AddString(PLACEHOLDER_DESC, VOTE_DESC_LENGTH);
		}
		if (ppCurrent && *ppCurrent)
			*ppCurrent = (*ppCurrent)->m_pNext;
		(*pCurIndex)++;
	}
}

bool CVotingMenu::OnMessage(int ClientID, CNetMsg_Cl_CallVote *pMsg)
{
	if (m_aClients[ClientID].m_LastVoteMsg + Server()->TickSpeed() / 2 > Server()->Tick())
		return true;
	m_aClients[ClientID].m_LastVoteMsg = Server()->Tick();

	if(!str_utf8_check(pMsg->m_Type) || !str_utf8_check(pMsg->m_Value) || !str_utf8_check(pMsg->m_Reason))
	{
		return true;
	}

	bool VoteOption = str_comp_nocase(pMsg->m_Type, "option") == 0;
	//bool VoteKick = str_comp_nocase(pMsg->m_Type, "kick") == 0;
	//bool VoteSpectate = str_comp_nocase(pMsg->m_Type, "spectate") == 0;

	if (!VoteOption)
	{
		// Process normal voting, we don't use kick/spec player votes currently
		return false;
	}

	int WantedPage = -1;
	for (int i = 0; i < NUM_PAGES; i++)
	{
		if (str_comp(pMsg->m_Value, GetPageDescription(ClientID, i)) == 0)
		{
			WantedPage = i;
			break;
		}
	}

	if (WantedPage != -1)
	{
		SetPage(ClientID, WantedPage);
		return true;
	}
	if (GetPage(ClientID) == PAGE_VOTES)
	{
		// placeholder
		if (!pMsg->m_Value[0])
			return true;
		// Process normal voting
		return false;
	}

	PrepareTempDescriptions(ClientID);

	const char *pDesc = 0;
	for (int i = 0; i < NUM_PAGE_MAX_VOTES; i++)
	{
		const char *pFullDesc = str_skip_voting_menu_prefixes(m_aaTempDesc[i]);
		if (!pFullDesc || !pFullDesc[0])
			continue;

		const char *pValue = str_skip_voting_menu_prefixes(pMsg->m_Value);
		if (pValue && pValue[0] && str_comp(pFullDesc, pValue) == 0)
		{
			pDesc = pFullDesc;
			break;
		}
	}

	// shouldnt happen, silent ignore
	if (!pDesc)
		return true;

	// Process voting option
	if (OnMessageSuccess(ClientID, pDesc, pMsg->m_Reason))
	{
		SendPageVotes(ClientID);
	}
	return true;
}

bool CVotingMenu::IsOptionWithSuffix(const char *pDesc, const char *pWantedOption)
{
	return str_startswith(pDesc, Localize(pWantedOption, m_TempLanguage, "vote-header")) != 0;
}

bool CVotingMenu::IsOption(const char *pDesc, const char *pWantedOption)
{
	return str_comp(pDesc, Localize(pWantedOption, m_TempLanguage)) == 0;
}

bool CVotingMenu::OnMessageSuccess(int ClientID, const char *pDesc, const char *pReason)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	CCharacter *pChr = pPlayer->GetCharacter();

	// For now set it to -1, if not a number
	int Reason = (pReason[0] && str_is_number(pReason) == 0) ? str_toint(pReason) : -1;

	if (GetPage(ClientID) == PAGE_ACCOUNT)
	{
		if (IsOptionWithSuffix(pDesc, COLLAPSE_HEADER_WANTED_PLAYERS))
		{
			m_aClients[ClientID].m_ShowWantedPlayers = !m_aClients[ClientID].m_ShowWantedPlayers;
			return true;
		}
		if (IsOptionWithSuffix(pDesc, ACC_WANTED_PLAYERS_PAGE))
		{
			if (Reason > 0 && Reason <= GetNumWantedPages())
				m_aClients[ClientID].m_WantedPlayersPage = Reason - 1;
			else
				GameServer()->SendChatTarget(ClientID, pPlayer->Localize("Please specify the page using the reason field"));
			return true;
		}
		// Acc info
		if (IsOptionWithSuffix(pDesc, COLLAPSE_HEADER_ACC_INFO))
		{
			m_aClients[ClientID].m_ShowAccountInfo = !m_aClients[ClientID].m_ShowAccountInfo;
			return true;
		}
		if (IsOptionWithSuffix(pDesc, COLLAPSE_HEADER_ACC_STATS))
		{
			m_aClients[ClientID].m_ShowAccountStats = !m_aClients[ClientID].m_ShowAccountStats;
			return true;
		}
		if (IsOptionWithSuffix(pDesc, COLLAPSE_HEADER_PLOT_INFO))
		{
			m_aClients[ClientID].m_ShowPlotInfo = !m_aClients[ClientID].m_ShowPlotInfo;
			return true;
		}

		// Acc Misc
		if (IsOption(pDesc, ACC_MISC_SILENTFARM))
		{
			pPlayer->SetSilentFarm(!pPlayer->m_SilentFarm);
			return true;
		}
		if (IsOption(pDesc, ACC_MISC_NINJAJETPACK))
		{
			pPlayer->SetNinjaJetpack(!pPlayer->m_NinjaJetpack);
			return true;
		}

		// Plot
		if (IsOption(pDesc, ACC_PLOT_SPAWN))
		{
			pPlayer->SetPlotSpawn(!pPlayer->m_PlotSpawn);
			return true;
		}
		if (IsOption(pDesc, ACC_PLOT_EDIT))
		{
			pPlayer->StartPlotEdit();
			return true;
		}
		if (IsOption(pDesc, ACC_PLOT_CLEAR))
		{
			pPlayer->ClearPlot();
			return true;
		}

		// VIP
		if (IsOption(pDesc, ACC_VIP_RAINBOW))
		{
			if (pChr) pChr->OnRainbowVIP();
			return true;
		}
		if (IsOption(pDesc, ACC_VIP_BLOODY))
		{
			if (pChr) pChr->OnBloodyVIP();
			return true;
		}
		if (IsOption(pDesc, ACC_VIP_ATOM))
		{
			if (pChr) pChr->OnAtomVIP();
			return true;
		}
		if (IsOption(pDesc, ACC_VIP_TRAIL))
		{
			if (pChr) pChr->OnTrailVIP();
			return true;
		}
		if (IsOption(pDesc, ACC_VIP_SPREADGUN))
		{
			if (pChr) pChr->OnSpreadGunVIP();
			return true;
		}
		// VIP Plus
		if (IsOption(pDesc, ACC_VIP_PLUS_RAINBOWHOOK))
		{
			if (pChr) pChr->OnRainbowHookVIP();
			return true;
		}
		if (IsOption(pDesc, ACC_VIP_PLUS_ROTATINGBALL))
		{
			if (pChr) pChr->OnRotatingBallVIP();
			return true;
		}
		if (IsOption(pDesc, ACC_VIP_PLUS_EPICCIRCLE))
		{
			if (pChr) pChr->OnEpicCircleVIP();
			return true;
		}
		if (IsOption(pDesc, ACC_VIP_PLUS_LOVELY))
		{
			if (pChr) pChr->OnLovelyVIP();
			return true;
		}
		if (IsOption(pDesc, ACC_VIP_PLUS_RAINBOWNAME))
		{
			if (pChr) pChr->OnRainbowNameVIP();
			return true;
		}
		if (IsOption(pDesc, ACC_VIP_PLUS_SPARKLE))
		{
			if (pChr) pChr->OnSparkleVIP();
			return true;
		}
		if (IsOptionWithSuffix(pDesc, ACC_VIP_PLUS_RAINBOWSPEED))
		{
			if (Reason != -1)
				pPlayer->SetRainbowSpeedVIP(Reason);
			else
				GameServer()->SendChatTarget(ClientID, pPlayer->Localize("Please specify the rainbow speed using the reason field"));
			return true;
		}

		for (unsigned int i = 0; i < m_vWantedPlayers.size(); i++)
		{
			int ID = m_vWantedPlayers[i];
			char aNameWithSpace[VOTE_DESC_LENGTH];
			str_format(aNameWithSpace, sizeof(aNameWithSpace), "%s%s", Server()->ClientName(ID), PLACEHOLDER_DESC);
			if (IsOption(pDesc, aNameWithSpace))
			{
				// spec wanted player
				if (ID != ClientID)
				{
					pPlayer->Pause(CPlayer::PAUSE_PAUSED, false);
					pPlayer->SetSpectatorID(SPEC_PLAYER, ID);
				}
				return true;
			}
		}
	}
	else if (GetPage(ClientID) == PAGE_MISCELLANEOUS)
	{
		if (IsOption(pDesc, MISC_HIDEDRAWINGS))
		{
			pPlayer->SetHideDrawings(!pPlayer->m_HideDrawings);
			return true;
		}
		if (IsOption(pDesc, MISC_WEAPONINDICATOR))
		{
			pPlayer->SetWeaponIndicator(!pPlayer->m_WeaponIndicator);
			return true;
		}
		if (IsOption(pDesc, MISC_ZOOMCURSOR))
		{
			pPlayer->SetZoomCursor(!pPlayer->m_ZoomCursor);
			return true;
		}
		if (IsOption(pDesc, MISC_RESUMEMOVED))
		{
			pPlayer->SetResumeMoved(!pPlayer->m_ResumeMoved);
			return true;
		}
		if (IsOption(pDesc, MISC_HIDEBROADCASTS))
		{
			pPlayer->SetHideBroadcasts(!pPlayer->m_HideBroadcasts);
			return true;
		}
		if (IsOption(pDesc, MISC_LOCALCHAT))
		{
			pPlayer->JoinChat(!pPlayer->m_LocalChat);
			return true;
		}

		for (int i = -1; i < CServer::NUM_MAP_DESIGNS; i++)
		{
			const char *pDesign = Server()->GetMapDesignName(i);
			if (!pDesign[0] || !IsOption(pDesc, pDesign))
				continue;
			Server()->ChangeMapDesign(ClientID, pDesign);
			return true;
		}

		for (int i = -1; i < NUM_MINIGAMES; i++)
		{
			const char *pMinigame = i == -1 ? Localizable("No minigame") : GameServer()->GetMinigameName(i);
			if (!IsOption(pDesc, pMinigame))
				continue;
			GameServer()->SetMinigame(ClientID, i);
			return true;
		}

		for (int i = 0; i < NUM_SCORE_MODES; i++)
		{
			if (i == SCORE_BONUS && !GameServer()->Config()->m_SvAllowBonusScoreMode)
				continue;
			if (!IsOption(pDesc, GameServer()->GetScoreModeName(i)))
				continue;
			pPlayer->ChangeScoreMode(i);
			return true;
		}
	}
	else if (GetPage(ClientID) == PAGE_LANGUAGES)
	{
		for (unsigned int i = 0; i < g_Localization.Languages().size(); i++)
		{
			const char *pLanguage = g_Localization.GetLanguageString(i);
			if (!IsOption(pDesc, pLanguage))
				continue;
			pPlayer->SetLanguage(i);
			return true;
		}
	}

	return false;
}

int CVotingMenu::PrepareTempDescriptions(int ClientID)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if (!pPlayer)
		return 0;

	m_TempLanguage = pPlayer->m_Language;
	m_NumCollapseEntries = 0;
	int NumOptions = 0;

	const int Page = GetPage(ClientID);
	bool DoAnnouncement = false;
	char aBuf[VOTE_DESC_LENGTH];
	{
		if (pPlayer->m_JailTime)
		{
			DoAnnouncement = true;
			str_format(aBuf, sizeof(aBuf), pPlayer->Localize("You are arrested for %lld seconds"), pPlayer->m_JailTime / Server()->TickSpeed());
		}
		else if (pPlayer->m_EscapeTime)
		{
			DoAnnouncement = true;
			str_format(aBuf, sizeof(aBuf), pPlayer->Localize("You are wanted by the police for %lld seconds"), pPlayer->m_EscapeTime / Server()->TickSpeed());
		}

		if (DoAnnouncement)
		{
			DoLineText(Page, &NumOptions, aBuf, BULLET_POINT);
		}
	}

	if (pPlayer->GetCharacter() && pPlayer->m_Permille)
	{
		DoAnnouncement = true;
		str_format(aBuf, sizeof(aBuf), pPlayer->Localize("Grog permille: %.1f‰ / %.1f‰"), pPlayer->m_Permille / 10.f, pPlayer->GetCharacter()->GetPermilleLimit() / 10.f);
		DoLineText(Page, &NumOptions, aBuf, BULLET_POINT);
	}

	if (DoAnnouncement)
	{
		DoLineSeperator(Page, &NumOptions);
	}

	if (GetPage(ClientID) == PAGE_ACCOUNT)
	{
		DoPageAccount(ClientID, &NumOptions);
	}
	else if (GetPage(ClientID) == PAGE_MISCELLANEOUS)
	{
		DoPageMiscellaneous(ClientID, &NumOptions);
	}
	else if (GetPage(ClientID) == PAGE_LANGUAGES)
	{
		DoPageLanguages(ClientID, &NumOptions);
	}

	return NumOptions;
}

void CVotingMenu::DoPageAccount(int ClientID, int *pNumOptions)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	CCharacter *pChr = pPlayer->GetCharacter();
	const int Page = GetPage(ClientID);

	char aBuf[128];
	time_t tmp;
	int AccID = pPlayer->GetAccID();
	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[AccID];

	if (AccID < ACC_START)
	{
		DoLineText(Page, pNumOptions, pPlayer->Localize("You're not logged in."));
		DoLineText(Page, pNumOptions, pPlayer->Localize("Use '/login' to see more information about your account."));
		return;
	}

	if (pAccount->m_PoliceLevel && m_vWantedPlayers.size())
	{
		if (m_vWantedPlayers.size() <= NUM_WANTEDS_PER_PAGE)
		{
			m_aClients[ClientID].m_WantedPlayersPage = 0;
		}
		int StartIndex = m_aClients[ClientID].m_WantedPlayersPage * NUM_WANTEDS_PER_PAGE;
		int NumWantedPlayers = clamp((int)m_vWantedPlayers.size() - StartIndex, 0, (int)NUM_WANTEDS_PER_PAGE);
		int NumPages = GetNumWantedPages();
		int NumEntriesCollapse = NumPages > 1 ? NUM_WANTEDS_PER_PAGE + 1 : NumWantedPlayers;
		if (DoLineCollapse(Page, pNumOptions, pPlayer->Localize(COLLAPSE_HEADER_WANTED_PLAYERS, "vote-header"), m_aClients[ClientID].m_ShowWantedPlayers, NumEntriesCollapse))
		{
			int PlayersLeft = NumWantedPlayers;
			for (unsigned int i = StartIndex; i < m_vWantedPlayers.size() && PlayersLeft; i++)
			{
				int ID = m_vWantedPlayers[i];
				// We add the placeholder string to the end, so we can check if the name has this "space" added.
				// Otherwise a wanted player with the name of another vote option would cause interferences... This should be safe
				str_format(aBuf, sizeof(aBuf), "%s%s", Server()->ClientName(ID), PLACEHOLDER_DESC);
				DoLineText(Page, pNumOptions, aBuf, BULLET_POINT);
				PlayersLeft--;
			}

			if (NumPages > 1)
			{
				for (int i = 0; i < NUM_WANTEDS_PER_PAGE - NumWantedPlayers; i++)
				{
					DoLineSeperator(Page, pNumOptions);
				}
				DoLineValueOption(Page, pNumOptions, ACC_WANTED_PLAYERS_PAGE, m_aClients[ClientID].m_WantedPlayersPage + 1, NumPages, BULLET_ARROW);
			}

			// This does not count to NumEntries anymore, s_NumCollapseEntries is 0 by now again. We wanna add a seperator only if this thing is opened
			DoLineSeperator(Page, pNumOptions);
		}
	}

	bool ShowEuros = GameServer()->Config()->m_SvEuroMode || pAccount->m_Euros > 0;
	bool ShowPortalDate = GameServer()->Config()->m_SvPortalRifleShop || pAccount->m_PortalRifle;
	if (DoLineCollapse(Page, pNumOptions, pPlayer->Localize(COLLAPSE_HEADER_ACC_INFO, "vote-header"), m_aClients[ClientID].m_ShowAccountInfo, 5 + (int)ShowEuros + (int)ShowPortalDate))
	{
		str_format(aBuf, sizeof(aBuf), "%s: %s", pPlayer->Localize("Account Name"), pAccount->m_Username);
		DoLineText(Page, pNumOptions, aBuf);

		if (pAccount->m_RegisterDate != 0)
		{
			tmp = pAccount->m_RegisterDate;
			str_format(aBuf, sizeof(aBuf), "%s: %s", pPlayer->Localize("Registered"), GameServer()->GetDate(tmp, false));
			DoLineText(Page, pNumOptions, aBuf);
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "%s: before April 9th 2021", pPlayer->Localize("Registered"));
			DoLineText(Page, pNumOptions, aBuf);
		}

		if (ShowEuros)
		{
			str_format(aBuf, sizeof(aBuf), "EUR: %.2f", pAccount->m_Euros);
			DoLineText(Page, pNumOptions, aBuf);
		}

		if (pAccount->m_VIP)
		{
			tmp = pAccount->m_ExpireDateVIP;
			str_format(aBuf, sizeof(aBuf), "VIP%s: %s %s", pAccount->m_VIP == VIP_PLUS ? "+" : "", pPlayer->Localize("until", "/account"), GameServer()->GetDate(tmp));
			DoLineText(Page, pNumOptions, aBuf);
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "VIP: %s", pPlayer->Localize("not bought"));
			DoLineText(Page, pNumOptions, aBuf);
		}

		if (pAccount->m_PortalRifle)
		{
			tmp = pAccount->m_ExpireDatePortalRifle;
			str_format(aBuf, sizeof(aBuf), "%s: %s %s", pPlayer->Localize("Portal Rifle"), pPlayer->Localize("until", "/account"), GameServer()->GetDate(tmp));
			DoLineText(Page, pNumOptions, aBuf);
		}
		else if (GameServer()->Config()->m_SvPortalRifleShop)
		{
			str_format(aBuf, sizeof(aBuf), "%s: %s", pPlayer->Localize("Portal Rifle"), pPlayer->Localize("not bought"));
			DoLineText(Page, pNumOptions, aBuf);
		}

		str_format(aBuf, sizeof(aBuf), "%s: %s", pPlayer->Localize("Contact"), pAccount->m_aContact);
		DoLineText(Page, pNumOptions, aBuf);
		str_format(aBuf, sizeof(aBuf), "%s: %s", pPlayer->Localize("E-Mail"), pAccount->m_aEmail);
		DoLineText(Page, pNumOptions, aBuf);

		// This does not count to NumEntries anymore, s_NumCollapseEntries is 0 by now again. We wanna add a seperator only if this thing is opened
		DoLineSeperator(Page, pNumOptions);
	}

	int PlotID = GameServer()->GetPlotID(AccID);
	bool BankEnabled = GameServer()->Config()->m_SvMoneyBankMode != 0;
	if (DoLineCollapse(Page, pNumOptions, pPlayer->Localize(COLLAPSE_HEADER_ACC_STATS, "vote-header"), m_aClients[ClientID].m_ShowAccountStats, 11 + (int)BankEnabled))
	{
		str_format(aBuf, sizeof(aBuf), "%s [%d]", pPlayer->Localize("Level"), pAccount->m_Level);
		DoLineText(Page, pNumOptions, aBuf);
		if (pChr && pChr->m_aLineExp[0] != '\0')
		{
			DoLineText(Page, pNumOptions, pChr->m_aLineExp);
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "XP [%lld/%lld]", pAccount->m_XP, GameServer()->GetNeededXP(pAccount->m_Level));
			DoLineText(Page, pNumOptions, aBuf);
		}

		bool IsInstantDeposit = GameServer()->Config()->m_SvMoneyBankMode == 2;
		if (IsInstantDeposit && pChr && pChr->m_aLineMoney[0] != '\0')
		{
			DoLineText(Page, pNumOptions, pChr->m_aLineMoney);
		}
		else if (BankEnabled)
		{
			str_format(aBuf, sizeof(aBuf), "%s [%lld]", pPlayer->Localize("Bank"), pAccount->m_Money);
			DoLineText(Page, pNumOptions, aBuf);
		}

		if (!IsInstantDeposit && pChr && pChr->m_aLineMoney[0] != '\0')
		{
			DoLineText(Page, pNumOptions, pChr->m_aLineMoney);
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "%s [%lld]", pPlayer->Localize("Wallet"), BankEnabled ? pPlayer->GetWalletMoney() : pAccount->m_Money);
			DoLineText(Page, pNumOptions, aBuf);
		}
		str_format(aBuf, sizeof(aBuf), "%s [%d]", pPlayer->Localize("Police"), pAccount->m_PoliceLevel);
		DoLineText(Page, pNumOptions, aBuf);

		DoLineTextSubheader(Page, pNumOptions, pPlayer->Localize("Cᴏʟʟᴇᴄᴛᴀʙʟᴇs", "vote-header"));
		str_format(aBuf, sizeof(aBuf), "%s [%d]", pPlayer->Localize("Taser battery"), pAccount->m_TaserBattery);
		DoLineText(Page, pNumOptions, aBuf);
		str_format(aBuf, sizeof(aBuf), "%s [%d]", pPlayer->Localize("Portal Battery"), pAccount->m_PortalBattery);
		DoLineText(Page, pNumOptions, aBuf);

		DoLineTextSubheader(Page, pNumOptions, "Bʟᴏᴄᴋ");
		str_format(aBuf, sizeof(aBuf), "%s: %d", pPlayer->Localize("Points"), pAccount->m_BlockPoints);
		DoLineText(Page, pNumOptions, aBuf);
		str_format(aBuf, sizeof(aBuf), "%s: %d", pPlayer->Localize("Kills"), pAccount->m_Kills);
		DoLineText(Page, pNumOptions, aBuf);
		str_format(aBuf, sizeof(aBuf), "%s: %d", pPlayer->Localize("Deaths"), pAccount->m_Deaths);
		DoLineText(Page, pNumOptions, aBuf);

		// only when we have the next category below
		if (PlotID >= PLOT_START)
		{
			// This does not count to NumEntries anymore, s_NumCollapseEntries is 0 by now again. We wanna add a seperator only if this thing is opened
			DoLineSeperator(Page, pNumOptions);
		}
	}

	if (PlotID >= PLOT_START)
	{
		char aPlotHeader[32];
		str_format(aPlotHeader, sizeof(aPlotHeader), "%s %d", pPlayer->Localize(COLLAPSE_HEADER_PLOT_INFO, "vote-header"), PlotID);
		bool IsPlotDestroy = GameServer()->m_aPlots[PlotID].m_DestroyEndTick;
		if (DoLineCollapse(Page, pNumOptions, aPlotHeader, m_aClients[ClientID].m_ShowPlotInfo, 4 + (int)IsPlotDestroy*2))
		{
			str_format(aBuf, sizeof(aBuf), "%s: %s", pPlayer->Localize("Rented until"), GameServer()->GetDate(GameServer()->m_aPlots[PlotID].m_ExpireDate));
			DoLineText(Page, pNumOptions, aBuf, BULLET_POINT);

			if (IsPlotDestroy)
			{
				str_format(aBuf, sizeof(aBuf), "%s: %d/%d", pPlayer->Localize("Door Health"), GameServer()->m_aPlots[PlotID].m_DoorHealth, GameServer()->Config()->m_SvPlotDoorHealth);
				DoLineText(Page, pNumOptions, aBuf, BULLET_POINT);
				str_format(aBuf, sizeof(aBuf), "%s: %lld", pPlayer->Localize("Destroy Seconds"), (GameServer()->m_aPlots[PlotID].m_DestroyEndTick - Server()->Tick()) / Server()->TickSpeed());
				DoLineText(Page, pNumOptions, aBuf, BULLET_POINT);
			}

			DoLineToggleOption(Page, pNumOptions, ACC_PLOT_SPAWN, pPlayer->m_PlotSpawn);
			DoLineText(Page, pNumOptions, ACC_PLOT_EDIT, BULLET_ARROW);
			DoLineText(Page, pNumOptions, ACC_PLOT_CLEAR, BULLET_ARROW);
		}
	}

	DoLineSeperator(Page, pNumOptions);
	DoLineTextSubheader(Page, pNumOptions, pPlayer->Localize("Oᴘᴛɪᴏɴs", "vote-header"));
	DoLineToggleOption(Page, pNumOptions, ACC_MISC_SILENTFARM, pPlayer->m_SilentFarm);
	if (pAccount->m_Ninjajetpack)
	{
		DoLineToggleOption(Page, pNumOptions, ACC_MISC_NINJAJETPACK, pPlayer->m_NinjaJetpack);
	}

	if (pAccount->m_VIP)
	{
		DoLineSeperator(Page, pNumOptions);
		DoLineTextSubheader(Page, pNumOptions, pAccount->m_VIP == VIP_PLUS ? "ᴠɪᴘ+" : "ᴠɪᴘ");
		DoLineToggleOption(Page, pNumOptions, ACC_VIP_RAINBOW, (pChr && pChr->m_Rainbow) || pPlayer->m_InfRainbow);
		if (pAccount->m_VIP == VIP_PLUS)
		{
			// let's put it here, next to rainbow which is influenced by it, not somewhere close to rainbowname i guess
			DoLineValueOption(Page, pNumOptions, ACC_VIP_PLUS_RAINBOWSPEED, pPlayer->m_RainbowSpeed, 20, BULLET_ARROW);
		}
		DoLineToggleOption(Page, pNumOptions, ACC_VIP_BLOODY, pChr && (pChr->m_Bloody || pChr->m_StrongBloody));
		DoLineToggleOption(Page, pNumOptions, ACC_VIP_ATOM, pChr && pChr->m_Atom);
		DoLineToggleOption(Page, pNumOptions, ACC_VIP_TRAIL, pChr && pChr->m_Trail);
		DoLineToggleOption(Page, pNumOptions, ACC_VIP_SPREADGUN, pChr && pChr->m_aSpreadWeapon[WEAPON_GUN]);

		if (pAccount->m_VIP == VIP_PLUS)
		{
			DoLineToggleOption(Page, pNumOptions, ACC_VIP_PLUS_RAINBOWHOOK, pChr && pChr->m_HookPower == RAINBOW);
			DoLineToggleOption(Page, pNumOptions, ACC_VIP_PLUS_ROTATINGBALL, pChr && pChr->m_RotatingBall);
			DoLineToggleOption(Page, pNumOptions, ACC_VIP_PLUS_EPICCIRCLE, pChr && pChr->m_EpicCircle);
			DoLineToggleOption(Page, pNumOptions, ACC_VIP_PLUS_LOVELY, pChr && pChr->m_Lovely);
			DoLineToggleOption(Page, pNumOptions, ACC_VIP_PLUS_RAINBOWNAME, pPlayer->m_RainbowName);
			DoLineToggleOption(Page, pNumOptions, ACC_VIP_PLUS_SPARKLE, pPlayer->m_Sparkle);
		}
	}
}

void CVotingMenu::DoPageMiscellaneous(int ClientID, int *pNumOptions)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	int Page = GetPage(ClientID);

	DoLineTextSubheader(Page, pNumOptions, pPlayer->Localize("Oᴘᴛɪᴏɴs", "vote-header"));
	DoLineToggleOption(Page, pNumOptions, MISC_HIDEDRAWINGS, pPlayer->m_HideDrawings);
	DoLineToggleOption(Page, pNumOptions, MISC_WEAPONINDICATOR, pPlayer->m_WeaponIndicator);
	DoLineToggleOption(Page, pNumOptions, MISC_ZOOMCURSOR, pPlayer->m_ZoomCursor);
	DoLineToggleOption(Page, pNumOptions, MISC_RESUMEMOVED, pPlayer->m_ResumeMoved);
	DoLineToggleOption(Page, pNumOptions, MISC_HIDEBROADCASTS, pPlayer->m_HideBroadcasts);
	if (GameServer()->Config()->m_SvLocalChat)
	{
		DoLineToggleOption(Page, pNumOptions, MISC_LOCALCHAT, pPlayer->m_LocalChat);
	}

	std::vector<const char *> vpDesigns;
	for (int i = -1; i < CServer::NUM_MAP_DESIGNS; i++)
	{
		const char *pDesign = Server()->GetMapDesignName(i);
		if (pDesign[0])
		{
			vpDesigns.push_back(pDesign);
		}
	}

	// Default is always there, thats why 1
	if (vpDesigns.size() > 1)
	{
		DoLineSeperator(Page, pNumOptions);
		DoLineTextSubheader(Page, pNumOptions, pPlayer->Localize("Dᴇsɪɢɴs", "vote-header"));
		// I have no idea what happens when a design has the same name as another vote, but it'll probably take the first and it'll be bugged out. try to avoid that xd
		const char *pOwnDesign = Server()->GetMapDesign(ClientID);
		for (unsigned int i = 0; i < vpDesigns.size(); i++)
		{
			bool CurDesign = str_comp(pOwnDesign, vpDesigns[i]) == 0;
			DoLineToggleOption(Page, pNumOptions, vpDesigns[i], CurDesign);
		}
	}

	DoLineSeperator(Page, pNumOptions);
	DoLineTextSubheader(Page, pNumOptions, pPlayer->Localize("Mɪɴɪɢᴀᴍᴇs", "vote-header"));
	for (int i = -1; i < NUM_MINIGAMES; i++)
	{
		if (i != -1 && GameServer()->m_aMinigameDisabled[i])
			continue;
		bool CurMinigame = i == pPlayer->m_Minigame;
		const char *pMinigame = i == -1 ? Localizable("No minigame") : GameServer()->GetMinigameName(i);
		DoLineToggleOption(Page, pNumOptions, pMinigame, CurMinigame);
	}

	DoLineSeperator(Page, pNumOptions);
	DoLineTextSubheader(Page, pNumOptions, pPlayer->Localize("Sᴄᴏʀᴇ Mᴏᴅᴇ", "vote-header"));
	for (int i = 0; i < NUM_SCORE_MODES; i++)
	{
		if (i == SCORE_BONUS && !GameServer()->Config()->m_SvAllowBonusScoreMode)
			continue;
		bool CurScoreMode = i == pPlayer->m_ScoreMode;
		DoLineToggleOption(Page, pNumOptions, GameServer()->GetScoreModeName(i), CurScoreMode);
	}
}

void CVotingMenu::DoPageLanguages(int ClientID, int *pNumOptions)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	int Page = GetPage(ClientID);

	DoLineTextSubheader(Page, pNumOptions, pPlayer->Localize("Aᴠᴀɪʟᴀʙʟᴇ Lᴀɴɢᴜᴀɢᴇs", "vote-header"));
	const char *pOwnLanguage = g_Localization.GetLanguageString(pPlayer->m_Language);
	for (unsigned int i = 0; i < g_Localization.Languages().size(); i++)
	{
		if (g_Localization.Languages()[i].m_Available)
		{
			const char *pLanguage = g_Localization.GetLanguageString(i);
			bool CurLang = str_comp(pOwnLanguage, pLanguage) == 0;
			DoLineToggleOption(Page, pNumOptions, pLanguage, CurLang);
		}
	}
}

void CVotingMenu::Tick()
{
	// Check once per second if we have to auto update
	if (Server()->Tick() % Server()->TickSpeed() == 0)
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			CPlayer *pPlayer = GameServer()->m_apPlayers[i];
			if (!pPlayer)
				continue;
			if (m_aClients[i].m_NextVoteSendTick > Server()->Tick())
				continue;

			CVotingMenu::SClientVoteInfo::SPrevStats Stats;
			// 0.7 doesnt have playerflag_in_menu anymore, so we update them automatically every 3s if something changed, even when not in menu /shrug
			bool Update = (pPlayer->m_PlayerFlags&PLAYERFLAG_IN_MENU) || !Server()->IsSevendown(i);
			if (!Update || m_aClients[i].m_Page == PAGE_VOTES || !FillStats(i, &Stats))
				continue;

			// Design doesn't have to be checked, because on design loading finish it will resend the votes anyways so it will be up to date
			if (mem_comp(&Stats, &m_aClients[i].m_PrevStats, sizeof(Stats)) != 0)
			{
				SendPageVotes(i);
			}
		}
	}
	// Clear for next tick, we wanna keep wantedplayers up-to-date at all time
	m_vWantedPlayers.clear();
}

bool CVotingMenu::FillStats(int ClientID, CVotingMenu::SClientVoteInfo::SPrevStats *pStats)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	if (!pPlayer || !pStats)
		return false;

	const int Page = GetPage(ClientID);

	int AccID = pPlayer->GetAccID();
	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[AccID];
	int Flags = 0;

	// Misc
	if (Page == PAGE_MISCELLANEOUS)
	{
		if (pPlayer->m_HideDrawings)
			Flags |= PREVFLAG_MISC_HIDEDRAWINGS;
		if (pPlayer->m_WeaponIndicator)
			Flags |= PREVFLAG_MISC_WEAPONINDICATOR;
		if (pPlayer->m_ZoomCursor)
			Flags |= PREVFLAG_MISC_ZOOMCURSOR;
		if (pPlayer->m_ResumeMoved)
			Flags |= PREVFLAG_MISC_RESUMEMOVED;
		if (pPlayer->m_HideBroadcasts)
			Flags |= PREVFLAG_MISC_HIDEBROADCASTS;
		pStats->m_Minigame = pPlayer->m_Minigame;
		pStats->m_ScoreMode = pPlayer->m_ScoreMode;
	}
	else if (Page == PAGE_ACCOUNT)
	{
		// Wanted list
		if (m_aClients[ClientID].m_ShowWantedPlayers && pAccount->m_PoliceLevel)
		{
			pStats->m_NumWanted = m_vWantedPlayers.size();
		}

		// VIP
		if (pAccount->m_VIP)
		{
			if ((pChr && pChr->m_Rainbow) || pPlayer->m_InfRainbow)
				Flags |= PREVFLAG_ACC_VIP_RAINBOW;
			if (pChr && (pChr->m_Bloody || pChr->m_StrongBloody))
				Flags |= PREVFLAG_ACC_VIP_BLOODY;
			if (pChr && pChr->m_Atom)
				Flags |= PREVFLAG_ACC_VIP_ATOM;
			if (pChr && pChr->m_Trail)
				Flags |= PREVFLAG_ACC_VIP_TRAIL;
			if (pChr && pChr->m_aSpreadWeapon[WEAPON_GUN])
				Flags |= PREVFLAG_ACC_VIP_SPREADGUN;

			// VIP Plus
			if (pAccount->m_VIP == VIP_PLUS)
			{
				if (pChr && pChr->m_HookPower == RAINBOW)
					Flags |= PREVFLAG_ACC_VIP_PLUS_RAINBOWHOOK;
				if (pChr && pChr->m_RotatingBall)
					Flags |= PREVFLAG_ACC_VIP_PLUS_ROTATINGBALL;
				if (pChr && pChr->m_EpicCircle)
					Flags |= PREVFLAG_ACC_VIP_PLUS_EPICCIRCLE;
				if (pChr && pChr->m_Lovely)
					Flags |= PREVFLAG_ACC_VIP_PLUS_LOVELY;
				if (pPlayer->m_RainbowName)
					Flags |= PREVFLAG_ACC_VIP_PLUS_RAINBOWNAME;
				if (pPlayer->m_Sparkle)
					Flags |= PREVFLAG_ACC_VIP_PLUS_SPARKLE;
				pStats->m_RainbowSpeed = pPlayer->m_RainbowSpeed;
			}
		}
		
		// Acc Misc
		if (pPlayer->m_SilentFarm)
			Flags |= PREVFLAG_ACC_MISC_SILENTFARM;
		if (pAccount->m_Ninjajetpack && pPlayer->m_NinjaJetpack)
			Flags |= PREVFLAG_ACC_NINJAJETPACK;
		// Plot
		int PlotID = GameServer()->GetPlotID(AccID);
		if (m_aClients[ClientID].m_ShowPlotInfo && PlotID >= PLOT_START)
		{
			if (pPlayer->m_PlotSpawn)
				Flags |= PREVFLAG_PLOT_SPAWN;
			pStats->m_DestroyEndTick = GameServer()->m_aPlots[PlotID].m_DestroyEndTick;
		}

		if (m_aClients[ClientID].m_ShowAccountStats)
		{
			pStats->m_Acc.m_XP = pAccount->m_XP;
			pStats->m_Acc.m_Money = pAccount->m_Money;
			pStats->m_Acc.m_WalletMoney = pPlayer->GetWalletMoney();
			pStats->m_Acc.m_PoliceLevel = pAccount->m_PoliceLevel;
			pStats->m_Acc.m_TaserBattery = pAccount->m_TaserBattery;
			pStats->m_Acc.m_PortalBattery = pAccount->m_PortalBattery;
			pStats->m_Acc.m_Points = pAccount->m_BlockPoints;
			pStats->m_Acc.m_Kills = pAccount->m_Kills;
			pStats->m_Acc.m_Deaths = pAccount->m_Deaths;
			pStats->m_Acc.m_Euros = pAccount->m_Euros;
			str_copy(pStats->m_Acc.m_aContact, pAccount->m_aContact, sizeof(pStats->m_Acc.m_aContact));
			if (pChr && pChr->m_aLineExp[0] != '\0')
				Flags |= PREVFLAG_ISPLUSXP;
		}
	}

	if (Page != PAGE_VOTES)
	{
		pStats->m_JailTime = pPlayer->m_JailTime;
		pStats->m_EscapeTime = pPlayer->m_EscapeTime;
		pStats->m_Permille = pChr ? pPlayer->m_Permille : 0; // Got moved to player, but still check for character due to other dependencies
		pStats->m_Language = pPlayer->m_Language;
	}

	pStats->m_Flags = Flags;
	return true;
}

void CVotingMenu::SendPageVotes(int ClientID, bool ResendVotesPage)
{
	if (!GameServer()->m_apPlayers[ClientID])
		return;

	int Seconds = Server()->IsSevendown(ClientID) ? 1 : 3;
	m_aClients[ClientID].m_NextVoteSendTick = Server()->Tick() + Server()->TickSpeed() * Seconds;
	const int Page = GetPage(ClientID);
	if (Page == PAGE_VOTES)
	{
		// No need to resend votes when we login, we only want to update the other pages with new values
		if (ResendVotesPage)
		{
			CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
			Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, ClientID);

			// begin sending vote options
			GameServer()->m_apPlayers[ClientID]->m_SendVoteIndex = 0;
		}
		return;
	}

	CVotingMenu::SClientVoteInfo::SPrevStats Stats;
	if (FillStats(ClientID, &Stats))
	{
		m_aClients[ClientID].m_PrevStats = Stats;
	}

	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, ClientID);

	const int NumVotesPage = PrepareTempDescriptions(ClientID);
	const int NumVotesToSend = NUM_OPTION_START_OFFSET + NumVotesPage;
	int TotalVotesSent = 0;
	CMsgPacker Msg(NETMSGTYPE_SV_VOTEOPTIONLISTADD);
	while (TotalVotesSent < NumVotesToSend)
	{
		const int VotesLeft = min(NumVotesToSend - TotalVotesSent, (int)MAX_VOTES_PER_PACKET);
		Msg.AddInt(VotesLeft);

		int CurIndex = 0;
		if (TotalVotesSent == 0)
		{
			OnProgressVoteOptions(ClientID, &Msg, &TotalVotesSent);
			CurIndex = TotalVotesSent;
		}

		while(CurIndex < VotesLeft)
		{
			Msg.AddString(m_aaTempDesc[TotalVotesSent], VOTE_DESC_LENGTH);
			TotalVotesSent++;
			CurIndex++;
		}

		if (Server()->IsSevendown(ClientID))
		{
			while (CurIndex < MAX_VOTES_PER_PACKET)
			{
				Msg.AddString("", VOTE_DESC_LENGTH);
				CurIndex++;
			}
		}
		Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
		// Reset for next potentional run
		Msg.Reset();
	}
}

void CVotingMenu::ApplyFlags(int ClientID, int Flags)
{
	if (Flags & VOTEFLAG_PAGE_VOTES)
		m_aClients[ClientID].m_Page = PAGE_VOTES;
	if (Flags & VOTEFLAG_PAGE_ACCOUNT)
		m_aClients[ClientID].m_Page = PAGE_ACCOUNT;
	if (Flags & VOTEFLAG_PAGE_MISCELLANEOUS)
		m_aClients[ClientID].m_Page = PAGE_MISCELLANEOUS;
	if (Flags & VOTEFLAG_PAGE_LANGUAGES)
		m_aClients[ClientID].m_Page = PAGE_LANGUAGES;
	if (Flags & VOTEFLAG_SHOW_ACC_STATS)
		m_aClients[ClientID].m_ShowAccountStats = true;
	if (Flags & VOTEFLAG_SHOW_PLOT_INFO)
		m_aClients[ClientID].m_ShowPlotInfo = true;
	if (Flags & VOTEFLAG_SHOW_WANTED_PLAYERS)
		m_aClients[ClientID].m_ShowWantedPlayers = true;
}

int CVotingMenu::GetFlags(int ClientID)
{
	int Flags = 0;
	int Page = GetPage(ClientID);
	if (Page == PAGE_VOTES)
		Flags |= VOTEFLAG_PAGE_VOTES;
	else if (Page == PAGE_ACCOUNT)
		Flags |= VOTEFLAG_PAGE_ACCOUNT;
	else if (Page == PAGE_MISCELLANEOUS)
		Flags |= VOTEFLAG_PAGE_MISCELLANEOUS;
	else if (Page == PAGE_LANGUAGES)
		Flags |= VOTEFLAG_PAGE_LANGUAGES;
	if (m_aClients[ClientID].m_ShowAccountInfo)
		Flags |= VOTEFLAG_SHOW_ACC_INFO;
	if (m_aClients[ClientID].m_ShowAccountStats)
		Flags |= VOTEFLAG_SHOW_ACC_STATS;
	if (m_aClients[ClientID].m_ShowPlotInfo)
		Flags |= VOTEFLAG_SHOW_PLOT_INFO;
	if (m_aClients[ClientID].m_ShowWantedPlayers)
		Flags |= VOTEFLAG_SHOW_WANTED_PLAYERS;
	return Flags;
}

#define ADDLINE_IMPL(desc, prefix, collapseHeader) do \
{ \
	dbg_assert((NUM_OPTION_START_OFFSET + *pNumOptions < NUM_PAGE_MAX_VOTES), "added too many votes options per page"); \
	char aLine[VOTE_DESC_LENGTH]; \
	if ((prefix)[0]) \
	{ \
		str_format(aLine, sizeof(aLine), "%s %s", (prefix), (desc)); \
	} \
	else \
	{ \
		str_copy(aLine, (desc), sizeof(aLine)); \
	} \
	bool AddCollapseFooter = false; \
	if (m_NumCollapseEntries > 0 && !(collapseHeader)) \
	{ \
		m_NumCollapseEntries--; \
		if (m_NumCollapseEntries == 0) \
		{ \
			AddCollapseFooter = true; \
		} \
		char aTemp[VOTE_DESC_LENGTH]; \
		str_format(aTemp, sizeof(aTemp), "│ %s", aLine); \
		str_copy(aLine, aTemp, sizeof(aLine)); \
	} \
	str_copy(m_aaTempDesc[NUM_OPTION_START_OFFSET + *pNumOptions], aLine, VOTE_DESC_LENGTH); \
	(*pNumOptions)++; \
	if (AddCollapseFooter) \
	{ \
		dbg_assert((NUM_OPTION_START_OFFSET + *pNumOptions < NUM_PAGE_MAX_VOTES), "added too many votes options per page"); \
		str_copy(m_aaTempDesc[NUM_OPTION_START_OFFSET + *pNumOptions], "╰───────────────────────", VOTE_DESC_LENGTH); \
		(*pNumOptions)++; \
	} \
} while(0)

#define ADDLINE(desc) ADDLINE_IMPL(desc, "", false)
#define ADDLINE_PREFIX(desc, prefix) ADDLINE_IMPL(desc, prefix, false)
#define ADDLINE_COLLAPSE(desc) ADDLINE_IMPL(desc, "", true)

void CVotingMenu::DoLineToggleOption(int Page, int *pNumOptions, const char *pDescription, bool Value)
{
	ADDLINE_PREFIX(Localize(pDescription, m_TempLanguage), Value ? "☒" : "☐");
}

void CVotingMenu::DoLineSeperator(int Page, int *pNumOptions)
{
	ADDLINE(PLACEHOLDER_DESC);
}

void CVotingMenu::DoLineTextSubheader(int Page, int *pNumOptions, const char *pDescription)
{
	char aBuf[VOTE_DESC_LENGTH];
	str_format(aBuf, sizeof(aBuf), "─── %s ───", pDescription);
	ADDLINE(aBuf);
}

void CVotingMenu::DoLineValueOption(int Page, int *pNumOptions, const char *pDescription, int Value, int Max, int BulletPoint)
{
	char aBuf[VOTE_DESC_LENGTH];
	if (Max == -1)
	{
		str_format(aBuf, sizeof(aBuf), "%s: %d", Localize(pDescription, m_TempLanguage), Value);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "%s: %d/%d", Localize(pDescription, m_TempLanguage), Value, Max);
	}
	DoLineText(Page, pNumOptions, aBuf, BulletPoint);
}

void CVotingMenu::DoLineText(int Page, int *pNumOptions, const char *pDescription, int BulletPoint)
{
	const char *pPrefix = "";
	if (BulletPoint == BULLET_POINT) pPrefix = "• ";
	else if (BulletPoint == BULLET_ARROW) pPrefix = "⇨ ";
	ADDLINE_PREFIX(Localize(pDescription, m_TempLanguage), pPrefix);
}

bool CVotingMenu::DoLineCollapse(int Page, int *pNumOptions, const char *pDescription, bool ShowContent, int NumEntries)
{
	char aBuf[VOTE_DESC_LENGTH];
	const char *pPrefix = ShowContent ? "╭─" : ">";
	const char *pSuffix = ShowContent ? "[‒]" : "[+]";
	int PrefixLength = str_length(pPrefix);
	int DescLength = str_length(pDescription);
	int SuffixLength = str_length(pSuffix);
	int TotalWidth = VOTE_DESC_LENGTH - 1; // -1 for null terminator ofc
	int TotalSpaces = clamp(TotalWidth - PrefixLength - DescLength - SuffixLength, 0, 20);
	int SpacesBefore = min(13, (int)(TotalSpaces * 0.7f));
	int SpacesAfter = TotalSpaces - SpacesBefore;

	char aSpacesBefore[VOTE_DESC_LENGTH] = { '\0' };
	if (SpacesBefore)
	{
		memset(aSpacesBefore, ' ', SpacesBefore);
		aSpacesBefore[SpacesBefore] = '\0';
	}

	char aSpacesAfter[VOTE_DESC_LENGTH] = { '\0' };
	if (SpacesAfter)
	{
		memset(aSpacesAfter, ' ', SpacesAfter);
		aSpacesAfter[SpacesAfter] = '\0';
	}

	if (ShowContent)
		m_NumCollapseEntries = NumEntries;

	str_format(aBuf, sizeof(aBuf), "%s%s%s%s%s", pPrefix, aSpacesBefore, pDescription, aSpacesAfter, pSuffix);
	ADDLINE_COLLAPSE(aBuf);
	return ShowContent;
}

#undef ADDLINE
#undef ADDLINE_PREFIX
#undef ADDLINE_COLLAPSE
#undef ADDLINE_IMPL
