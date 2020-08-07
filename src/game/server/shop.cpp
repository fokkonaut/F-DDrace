// made by fokkonaut

#include "shop.h"
#include "gamecontext.h"

// manually checked amount of newlines between the end of the description of the current page and the footer
int pNumNewLines[NUM_ITEMS_LIST+2] = { 13, 10, 10, 8, 7, 8, 5, 9, 9, 9, 10, 9, 3, 14, 17 };

CShop::CShop(CGameContext *pGameServer, int Type)
{
	m_pGameServer = pGameServer;
	m_pServer = m_pGameServer->Server();
	m_Type = Type;

	for (int i = 0; i < MAX_CLIENTS; i++)
		Reset(i);

	m_aItems[0].m_Used = true; // item 0 is technically PAGE_MAIN, so we dont want to get conflicts there
	for (int i = 1; i < MAX_PLOTS; i++)
		m_aItems[i].m_Used = false;

	if (IsType(TYPE_SHOP_NORMAL))
	{
		m_NumItems = NUM_ITEMS;
		m_NumItemsList = NUM_ITEMS_LIST;

		AddItem("Rainbow", 5, 1500, TIME_DEATH, "Rainbow will make your tee change the color very fast.");
		AddItem("Bloody", 15, 3500, TIME_DEATH, "Bloody will give your tee a permanent kill effect.");
		AddItem("Police", -1, 100000, TIME_FOREVER, "Police officers get help from the police bot. For more information about the specific police ranks, please say '/policeinfo'.");
		AddItem("Spooky Ghost", 1, 1000000, TIME_FOREVER, "Using this item you can hide from other players behind bushes. If your ghost is activated you will be able to shoot plasma projectiles. For more information please visit '/spookyghostinfo'.");
		AddItem("Room Key", 16, 5000, TIME_DISCONNECT, "If you have the room key you can enter the room. It's under the spawn and there is a money tile.");
		AddItem("VIP", 1, 5, TIME_30_DAYS, "VIP gives you some benefits, check '/vipinfo'.", true);
		AddItem("Spawn Shotgun", 33, 600000, TIME_FOREVER, "You will have shotgun if you respawn. For more information about spawn weapons, please type '/spawnweaponsinfo'.");
		AddItem("Spawn Grenade", 33, 600000, TIME_FOREVER, "You will have grenade if you respawn. For more information about spawn weapons, please type '/spawnweaponsinfo'.");
		AddItem("Spawn Rifle", 33, 600000, TIME_FOREVER, "You will have rifle if you respawn. For more information about spawn weapons, please type '/spawnweaponsinfo'.");
		AddItem("Ninjajetpack", 21, 10000, TIME_FOREVER, "It will make your jetpack gun be a ninja.Toggle it using '/ninjajetpack'.");
		AddItem("Taser", 30, -1, TIME_FOREVER, "Taser is a rifle that freezes a player. For more information about the taser and your taser stats, plase visit '/taserinfo'.");
		AddItem("Portal Rifle", 1, 10, TIME_20_DAYS, "With Portal Rifle you can create two portals where your cursor is, then teleport between them.", true);

		static char aaBuf[NUM_POLICE_LEVELS][32];
		for (int i = 0; i < NUM_POLICE_LEVELS; i++)
		{
			str_format(aaBuf[i], sizeof(aaBuf[i]), "Police Rank %d", i+1);
			AddItem(aaBuf[i], m_pGameServer->m_aPoliceLevel[i], m_aItems[ITEM_POLICE].m_Price, m_aItems[ITEM_POLICE].m_Time, m_aItems[ITEM_POLICE].m_pDescription);
		}

		static char aaBuf2[NUM_TASER_LEVELS][32];
		for (int i = 0; i < NUM_TASER_LEVELS; i++)
		{
			str_format(aaBuf2[i], sizeof(aaBuf2[i]), "Taser Level %d", i+1);
			AddItem(aaBuf2[i], m_aItems[ITEM_TASER].m_Level, m_pGameServer->m_aTaserPrice[i], m_aItems[ITEM_TASER].m_Time, m_aItems[ITEM_TASER].m_pDescription);
		}
	}
	else if (IsType(TYPE_SHOP_PLOT))
	{
		m_NumItems = m_NumItemsList = m_pGameServer->Collision()->m_NumPlots + 1;

		bool Owned;
		int Size;
		static char aaName[MAX_PLOTS][32];
		int Level;
		int Price;
		static char aaDescription[MAX_PLOTS][64];
		for (int i = PLOT_START; i < m_NumItems; i++)
		{
			Owned = m_pGameServer->m_aPlots[i].m_aOwner[0] != '\0';
			Size = m_pGameServer->m_aPlots[i].m_Size;
			str_format(aaName[i], sizeof(aaName[i]), "Plot %d", i);
			Level = (Size + 1) * 20;
			Price = (Size + 1) * 50000;
			str_format(aaDescription[i], sizeof(aaDescription[i]),
				"Size: %s\n"
				"Owner: %s"
				"Free on, if owner doesn't pay again: %s",
				Size == 0 ? "small" : Size == 1 ? "big" : "?", Owned ? m_pGameServer->m_aPlots[i].m_aOwner : "for sale", Owned ? m_pGameServer->GetDate(m_pGameServer->m_aPlots[i].m_ExpireDate) : "now");
			AddItem(aaName[i], Level, Price, TIME_7_DAYS, aaDescription[i]);
		}
	}
}

void CShop::Reset(int ClientID)
{
	m_WindowPage[ClientID] = PAGE_NONE;
	m_PurchaseState[ClientID] = STATE_NONE;
	m_InShop[ClientID] = false;
	m_AntiSpamTick[ClientID] = m_pServer->Tick();
	m_MotdTick[ClientID] = m_pServer->Tick();
	m_BackgroundItem[ClientID] = 0;
}

void CShop::AddItem(const char *pName, int Level, int Price, int Time, const char *pDescription, bool IsEuro)
{
	for (int i = 0; i < m_NumItems; i++)
	{
		if (!m_aItems[i].m_Used)
		{
			m_aItems[i].m_pName = pName;
			m_aItems[i].m_Level = Level;
			m_aItems[i].m_Price = Price;
			m_aItems[i].m_Time = Time;
			m_aItems[i].m_pDescription = pDescription;
			m_aItems[i].m_IsEuro = IsEuro;
			m_aItems[i].m_Used = true;
			break;
		}
	}
}

void CShop::OnShopEnter(int ClientID)
{
	if (m_InShop[ClientID])
		return;

	if (m_AntiSpamTick[ClientID] < m_pServer->Tick() && !m_pGameServer->IsShopDummy(ClientID, m_Type))
	{
		CCharacter* pChr = m_pGameServer->GetPlayerChar(ClientID);

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "Welcome to the shop, %s! Press F4 to start shopping.", m_pServer->ClientName(ClientID));
		m_pGameServer->SendChat(m_pGameServer->m_World.GetClosestShopDummy(pChr->GetPos(), pChr, m_Type, ClientID), CHAT_SINGLE, ClientID, aBuf);
	}

	m_InShop[ClientID] = true;
}

void CShop::OnShopLeave(int ClientID)
{
	if (!m_InShop[ClientID])
		return;

	CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);

	if (m_AntiSpamTick[ClientID] < m_pServer->Tick() && !m_pGameServer->IsShopDummy(ClientID, m_Type))
	{
		m_pGameServer->SendChat(m_pGameServer->m_World.GetClosestShopDummy(pChr->GetPos(), pChr, m_Type, ClientID), CHAT_SINGLE, ClientID, "Bye! Come back if you need something.");
		m_AntiSpamTick[ClientID] = m_pServer->Tick() + m_pServer->TickSpeed() * 5;
	}

	if (m_WindowPage[ClientID] != PAGE_NONE)
		m_pGameServer->SendMotd("", ClientID);
	m_pGameServer->SendBroadcast("", ClientID, false);

	m_PurchaseState[ClientID] = STATE_NONE;
	m_WindowPage[ClientID] = PAGE_NONE;

	m_InShop[ClientID] = false;
}

void CShop::OnKeyPress(int ClientID, int Dir)
{
	if (Dir == 1)
	{
		if (m_PurchaseState[ClientID] == STATE_CONFIRM)
		{
			EndPurchase(ClientID, false);
		}
		else if (m_PurchaseState[ClientID] == STATE_OPENED_WINDOW)
		{
			if (m_WindowPage[ClientID] != PAGE_NONE && m_WindowPage[ClientID] != PAGE_MAIN)
				ConfirmPurchase(ClientID);
		}
	}
	else if (Dir == -1)
	{
		if (m_PurchaseState[ClientID] == STATE_CONFIRM)
		{
			EndPurchase(ClientID, true);
		}
		else if (m_WindowPage[ClientID] == PAGE_NONE)
		{
			ShopWindow(ClientID, PAGE_MAIN);
			m_PurchaseState[ClientID] = STATE_OPENED_WINDOW;
		}
	}
}

bool CShop::CanChangePage(int ClientID)
{
	return m_WindowPage[ClientID] != PAGE_NONE && m_PurchaseState[ClientID] == STATE_OPENED_WINDOW;
}

const char *CShop::GetHeadline(int Item)
{
	if (Item < ITEM_RAINBOW || Item >= m_NumItems)
		return "Unknown";

	static char aRet[64];
	str_copy(aRet, "", sizeof(aRet));

	int s = 0;

	int NameLength = str_length(m_aItems[Item].m_pName) + 1;
	for (int i = 0; i < NameLength; i++)
	{
		aRet[s] = str_uppercase(m_aItems[Item].m_pName[i]);
		if (i == NameLength - 2)
			break;
		str_append(aRet, " ", sizeof(aRet));
		s += 2;
	}

	return aRet;
}

const char *CShop::FormatMotd(const char *pMsg, int Item)
{
	char aTemp[64];
	char aTemp2[64];
	char aPage[64];
	static char aRet[900];

	int Page = Item;
	if (Item == -1) Page = NUM_ITEMS_LIST;
	else if (Item == -2) Page = NUM_ITEMS_LIST+1;
	else if (IsType(TYPE_SHOP_NORMAL))
	{
		if (Item >= POLICE_RANK_1 && Item <= POLICE_RANK_5) Page = ITEM_POLICE;
		else if (Item >= TASER_LEVEL_1 && Item <= TASER_LEVEL_7) Page = ITEM_TASER;
	}

	str_format(aPage, sizeof(aPage), "~ %d ~", Page);

	int NumNewLines = IsType(TYPE_SHOP_NORMAL) ? pNumNewLines[Page] : IsType(TYPE_SHOP_PLOT) ? 10 : 0;
	aTemp[0] = 0;
	for (int i = 0; i < NumNewLines; i++)
	{
		str_format(aTemp2, sizeof(aTemp2), "%s", aTemp);
		str_format(aTemp, sizeof(aTemp), "%s%s", aTemp2, "\n");
	}

	str_format(aRet, sizeof(aRet),
		"**************************************\n"
		"                  ~  S H O P  ~\n"
		"**************************************\n\n"
		"%s"
		"%s"
		"**************************************\n"
		"%s\n"
		"%s", pMsg, aTemp, Item >= PAGE_MAIN ? "If you want to buy an item, press F3" : "", Item > PAGE_MAIN ? aPage : "");
	return aRet;
}

const char *CShop::GetTimeMessage(int Time)
{
	switch (Time)
	{
	case TIME_DEATH: return "You own this item until you die.";
	case TIME_DISCONNECT: return "You own this item until\nyou disconnect.";
	case TIME_FOREVER: return "You own this item forever.";
	case TIME_30_DAYS: return "You own this item for 30 days.";
	case TIME_20_DAYS: return "You own this item for 20 days.";
	case TIME_7_DAYS: return "You own this item for 7 days.";
	}
	return "Unknown";
}

void CShop::Tick(int ClientID)
{
	if (m_MotdTick[ClientID] < m_pServer->Tick())
	{
		m_WindowPage[ClientID] = PAGE_NONE;
		m_PurchaseState[ClientID] = STATE_NONE;
	}

	if (m_InShop[ClientID])
	{
		if (m_pServer->Tick() % 50 == 0)
		{
			if (IsType(TYPE_SHOP_NORMAL))
				m_pGameServer->SendBroadcast("~ S H O P ~", ClientID, false);
			else if (IsType(TYPE_SHOP_PLOT))
				m_pGameServer->SendBroadcast("~ P L O T - S H O P ~", ClientID, false);
		}
	}
}

void CShop::ShopWindow(int ClientID, int Dir)
{
	m_MotdTick[ClientID] = 0;

	if (Dir == PAGE_MAIN)
	{
		m_WindowPage[ClientID] = PAGE_MAIN;
	}
	else if (Dir == 1)
	{
		m_WindowPage[ClientID]++;
		if (m_WindowPage[ClientID] >= m_NumItemsList)
			m_WindowPage[ClientID] = PAGE_MAIN;
	}
	else if (Dir == -1)
	{
		m_WindowPage[ClientID]--;
		if (m_WindowPage[ClientID] < PAGE_MAIN)
			m_WindowPage[ClientID] = m_NumItemsList-1;
	}

	m_BackgroundItem[ClientID] = m_WindowPage[ClientID];

	if (IsType(TYPE_SHOP_NORMAL))
	{
		if (m_WindowPage[ClientID] == ITEM_POLICE)
		{
			CGameContext::AccountInfo* Account = &m_pGameServer->m_Accounts[m_pGameServer->m_apPlayers[ClientID]->GetAccID()];
			m_BackgroundItem[ClientID] = clamp(POLICE_RANK_1 + (*Account).m_PoliceLevel, (int)POLICE_RANK_1, (int)POLICE_RANK_5);
		}
		else if (m_WindowPage[ClientID] == ITEM_TASER)
		{
			CGameContext::AccountInfo* Account = &m_pGameServer->m_Accounts[m_pGameServer->m_apPlayers[ClientID]->GetAccID()];
			m_BackgroundItem[ClientID] = clamp(TASER_LEVEL_1 + (*Account).m_TaserLevel, (int)TASER_LEVEL_1, (int)TASER_LEVEL_7);
		}
	}

	SendWindow(ClientID, m_BackgroundItem[ClientID]);
}

void CShop::SendWindow(int ClientID, int Item)
{
	char aMsg[512];
	if (m_WindowPage[ClientID] > PAGE_MAIN)
	{
		str_format(aMsg, sizeof(aMsg),
			"%s\n\n"
			"Level: %d\n"
			"Price: %d%s\n"
			"Time: %s\n\n"
			"%s%s",
			GetHeadline(Item),
			m_aItems[Item].m_Level,
			m_aItems[Item].m_Price,
			m_aItems[Item].m_IsEuro ? " Euros" : "",
			GetTimeMessage(m_aItems[Item].m_Time),
			m_aItems[Item].m_pDescription,
			m_aItems[Item].m_IsEuro ? "\n\nHow to get euros ingame? Contact the admin and donate to the server, it will get added to your ingame euros.\n\nCheck '/account' for your details." : ""
		);
	}
	else
	{
		str_format(aMsg, sizeof(aMsg), "Welcome to the shop!\n\nBy shooting to the right you go one site forward, and by shooting left you go one site back.");
	}

	m_pGameServer->SendMotd(FormatMotd(aMsg, Item), ClientID);
	m_MotdTick[ClientID] = m_pServer->Tick() + m_pServer->TickSpeed() * 10; // motd is there for 10 sec
}

void CShop::ConfirmPurchase(int ClientID)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf),
		"Are you sure you want to buy this item?\n\n"
		"F3 - yes\n"
		"F4 - no");

	m_pGameServer->SendMotd(FormatMotd(aBuf, -1), ClientID);
	m_PurchaseState[ClientID] = STATE_CONFIRM;
}

void CShop::EndPurchase(int ClientID, bool Cancelled)
{
	if (Cancelled)
	{
		m_pGameServer->SendMotd(FormatMotd("You canceled the purchase.", -2), ClientID);
	}
	else
	{
		BuyItem(ClientID, m_WindowPage[ClientID]);
		ShopWindow(ClientID, PAGE_MAIN);
	}

	m_PurchaseState[ClientID] = STATE_OPENED_WINDOW;
}

void CShop::BuyItem(int ClientID, int Item)
{
	if (Item < ITEM_RAINBOW || Item >= m_NumItemsList)
	{
		m_pGameServer->SendChatTarget(ClientID, "Invalid item");
		return;
	}

	CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);
	CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientID];
	CGameContext::AccountInfo *Account = &m_pGameServer->m_Accounts[pPlayer->GetAccID()];

	char aMsg[128];
	int ItemID = Item;

	if (IsType(TYPE_SHOP_NORMAL))
	{
		if (Item == ITEM_POLICE || Item == ITEM_TASER)
			ItemID = m_BackgroundItem[ClientID];

		// Check whether we have the item already
		if ((Item == ITEM_RAINBOW				&& (pChr->m_Rainbow || pPlayer->m_InfRainbow))
			|| (Item == ITEM_BLOODY				&& (pChr->m_Bloody || pChr->m_StrongBloody))
			|| (Item == ITEM_POLICE				&& (*Account).m_PoliceLevel == NUM_POLICE_LEVELS)
			|| (Item == ITEM_SPOOKY_GHOST		&& (*Account).m_SpookyGhost)
			|| (Item == ITEM_ROOM_KEY			&& (pPlayer->m_HasRoomKey))
			//|| (Item == ITEM_VIP				&& (*Account).m_VIP) // vip can be bought unlimited times
			|| (Item == ITEM_SPAWN_SHOTGUN		&& (*Account).m_SpawnWeapon[0] == 5)
			|| (Item == ITEM_SPAWN_GRENADE		&& (*Account).m_SpawnWeapon[1] == 5)
			|| (Item == ITEM_SPAWN_RIFLE		&& (*Account).m_SpawnWeapon[2] == 5)
			|| (Item == ITEM_NINJAJETPACK		&& (*Account).m_Ninjajetpack)
			|| (Item == ITEM_TASER				&& (*Account).m_TaserLevel == NUM_TASER_LEVELS)
			//|| (Item == ITEM_PORTAL_RIFLE		&& (*Account).m_PortalRifle) // portal rifle can be bought unlimited times
			)
		{
			bool UseThe = false;

			switch (Item)
			{
			case ITEM_POLICE:															m_pGameServer->SendChatTarget(ClientID, "You already have the highest police rank"); break;
			case ITEM_SPAWN_SHOTGUN: case ITEM_SPAWN_GRENADE: case ITEM_SPAWN_RIFLE:	m_pGameServer->SendChatTarget(ClientID, "You already have the maximum amount of bullets"); break;
			case ITEM_TASER:															m_pGameServer->SendChatTarget(ClientID, "You already have the maximum taser level"); break;
			case ITEM_SPOOKY_GHOST: case ITEM_ROOM_KEY:									UseThe = true;
				// fallthrough
			default:
				str_format(aMsg, sizeof(aMsg), "You already have %s%s", UseThe ? "the " : "", m_aItems[ItemID].m_pName);
				m_pGameServer->SendChatTarget(ClientID, aMsg);
			}
			return;
		}

		// check police lvl 3 for taser
		if (Item == ITEM_TASER && (*Account).m_PoliceLevel < 3)
		{
			m_pGameServer->SendChatTarget(ClientID, "You need to be police level 3 or higher to get a taser license");
			return;
		}
	}
	else if (IsType(TYPE_SHOP_PLOT))
	{
		int OwnPlotID = m_pGameServer->GetPlotID(pPlayer->GetAccID());
		if (m_pGameServer->m_aPlots[Item].m_aOwner[0] != '\0')
		{
			m_pGameServer->SendChatTarget(ClientID, "This plot is already sold to someone else");
			return;
		}
		else if (OwnPlotID == Item)
		{
			m_pGameServer->SendChatTarget(ClientID, "You already own that plot");
			return;
		}
		else if (OwnPlotID != 0)
		{
			m_pGameServer->SendChatTarget(ClientID, "You already own another plot");
			return;
		}
	}

	// check for the correct price
	if ((m_aItems[ItemID].m_IsEuro && (*Account).m_Euros < m_aItems[ItemID].m_Price)
		|| (!m_aItems[ItemID].m_IsEuro && (*Account).m_Money < m_aItems[ItemID].m_Price))
	{
		m_pGameServer->SendChatTarget(ClientID, "You don't have enough money");
		return;
	}

	// check for the correct level
	if ((*Account).m_Level < m_aItems[ItemID].m_Level)
	{
		str_format(aMsg, sizeof(aMsg), "Your level is too low, you need to be level %d to buy %s", m_aItems[ItemID].m_Level, m_aItems[ItemID].m_pName);
		m_pGameServer->SendChatTarget(ClientID, aMsg);
		return;
	}

	// send a message that we bought the item
	str_format(aMsg, sizeof(aMsg), "You bought %s %s", m_aItems[ItemID].m_pName, m_aItems[ItemID].m_Time == TIME_DEATH ? "until death" : m_aItems[ItemID].m_Time == TIME_DISCONNECT ? "until disconnect" : "");
	m_pGameServer->SendChatTarget(ClientID, aMsg);

	// apply a message to the history
	str_format(aMsg, sizeof(aMsg), "-%d %s, bought '%s'", m_aItems[ItemID].m_Price, m_aItems[ItemID].m_IsEuro ? "euros" : "money", m_aItems[ItemID].m_pName);
	pPlayer->MoneyTransaction(-m_aItems[ItemID].m_Price, aMsg, m_aItems[ItemID].m_IsEuro);


	if (IsType(TYPE_SHOP_NORMAL))
	{
		// give us the bought item
		int Weapon = -1;

		switch (Item)
		{
		case ITEM_RAINBOW:			pChr->Rainbow(true, -1, true); break;
		case ITEM_BLOODY:			pChr->Bloody(true, -1, true); break;
		case ITEM_POLICE:			(*Account).m_PoliceLevel++; break;
		case ITEM_SPOOKY_GHOST:		(*Account).m_SpookyGhost = true; break;
		case ITEM_ROOM_KEY:			pPlayer->m_HasRoomKey = true; pChr->Core()->m_MoveRestrictionExtra.m_CanEnterRoom = true; break;
		case ITEM_VIP:				(*Account).m_VIP = true; pPlayer->SetExpireDate(Item); break;
		case ITEM_SPAWN_SHOTGUN:	if (Weapon == -1) Weapon = 0;
			// fallthrough
		case ITEM_SPAWN_GRENADE:	if (Weapon == -1) Weapon = 1;
			// fallthrough
		case ITEM_SPAWN_RIFLE:		if (Weapon == -1) Weapon = 2;
									(*Account).m_SpawnWeapon[Weapon]++; break;
		case ITEM_NINJAJETPACK:		(*Account).m_Ninjajetpack = true; break;
		case ITEM_TASER:			(*Account).m_TaserLevel++; break;
		case ITEM_PORTAL_RIFLE:		(*Account).m_PortalRifle = true; pPlayer->SetExpireDate(Item);
									if (pPlayer->GetCharacter())
										pPlayer->GetCharacter()->GiveWeapon(WEAPON_PORTAL_RIFLE);
									break;
		}
	}
	else if (IsType(TYPE_SHOP_PLOT))
	{
		m_pGameServer->SetExpireDate(&m_pGameServer->m_aPlots[Item].m_ExpireDate, ITEM_EXPIRE_PLOT);
		str_copy(m_pGameServer->m_aPlots[Item].m_aOwner, m_pGameServer->m_Accounts[pPlayer->GetAccID()].m_Username, sizeof(m_pGameServer->m_aPlots[Item].m_aOwner));
		str_copy(m_pGameServer->m_aPlots[Item].m_aDisplayName, m_pGameServer->m_Accounts[pPlayer->GetAccID()].m_aLastPlayerName, sizeof(m_pGameServer->m_aPlots[Item].m_aDisplayName));
		m_pGameServer->WritePlotStats(Item);
	}
}
