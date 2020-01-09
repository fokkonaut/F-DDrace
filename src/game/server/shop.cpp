// made by fokkonaut

#include "shop.h"

CShop::CShop(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pServer = m_pGameServer->Server();

	for (int i = 0; i < MAX_CLIENTS; i++)
		Reset(i);

	m_aItems[0].m_Used = true; // item 0 is technically PAGE_MAIN, so we dont want to get conflicts there
	for (int i = 1; i < NUM_ITEMS; i++)
		m_aItems[i].m_Used = false;

	AddItem("Rainbow", 5, 1500, TIME_DEATH, "Rainbow will make your tee change the color very fast.");
	AddItem("Bloody", 15, 3500, TIME_DEATH, "Bloody will give your tee a permanent kill effect.");
	AddItem("Police", -1, 100000, TIME_FOREVER, "Police officers get help from the police bot.\nFor more information about the specific police ranks\nplease say '/policeinfo'.");
	AddItem("Spooky Ghost", 1, 1000000, TIME_FOREVER, "Using this item you can hide from other players behind bushes.\nIf your ghost is activated you will be able to shoot plasma\nprojectiles. For more information please visit '/spookyghostinfo'.");
	AddItem("Room Key", 16, 5000, TIME_DISCONNECT, "If you have the room key you can enter the room.\nIt's under the spawn and there is a money tile.");
	AddItem("VIP", 1, -1, TIME_FOREVER, "VIP gives you some benefits,\ncheck '/vipinfo'.");
	AddItem("Spawn Shotgun", 33, 600000, TIME_FOREVER, "You will have shotgun if you respawn.\nFor more information about spawn weapons,\nplease type '/spawnweaponsinfo'.");
	AddItem("Spawn Grenade", 33, 600000, TIME_FOREVER, "You will have grenade if you respawn.\nFor more information about spawn weapons,\nplease type '/spawnweaponsinfo'.");
	AddItem("Spawn Rifle", 33, 600000, TIME_FOREVER, "You will have rifle if you respawn.\nFor more information about spawn weapons,\nplease type '/spawnweaponsinfo'.");
	AddItem("Ninjajetpack", 21, 10000, TIME_FOREVER, "It will make your jetpack gun be a ninja.\nToggle it using '/ninjajetpack'.");
	AddItem("Taser", 30, -1, TIME_FOREVER, "Taser is a rifle that freezes a player\nFor more information about the taser and your taser stats,\nplase visit '/taserinfo'.");

	static char aaBuf[NUM_POLICE_LEVELS][32];
	for (int i = 0; i < 5; i++)
	{
		str_format(aaBuf[i], sizeof(aaBuf[i]), "Police Rank %d", i+1);
		AddItem(aaBuf[i], m_pGameServer->m_aPoliceLevel[i], m_aItems[ITEM_POLICE].m_Price, m_aItems[ITEM_POLICE].m_Time, m_aItems[ITEM_POLICE].m_pDescription);
	}

	static char aaBuf2[NUM_TASER_LEVELS][32];
	for (int i = 0; i < 7; i++)
	{
		str_format(aaBuf2[i], sizeof(aaBuf2[i]), "Taser Level %d", i+1);
		AddItem(aaBuf2[i], m_aItems[ITEM_TASER].m_Level, m_pGameServer->m_aTaserPrice[i], m_aItems[ITEM_TASER].m_Time, m_aItems[ITEM_TASER].m_pDescription);
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

void CShop::AddItem(const char *pName, int Level, int Price, int Time, const char *pDescription)
{
	for (int i = 0; i < NUM_ITEMS; i++)
	{
		if (!m_aItems[i].m_Used)
		{
			m_aItems[i].m_pName = pName;
			m_aItems[i].m_Level = Level;
			m_aItems[i].m_Price = Price;
			m_aItems[i].m_Time = Time;
			m_aItems[i].m_pDescription = pDescription;
			m_aItems[i].m_Used = true;
			break;
		}
	}
}

void CShop::OnShopEnter(int ClientID)
{
	if (m_InShop[ClientID])
		return;

	if (m_AntiSpamTick[ClientID] < m_pServer->Tick() && !m_pGameServer->IsShopDummy(ClientID))
	{
		CCharacter* pChr = m_pGameServer->GetPlayerChar(ClientID);

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "Welcome to the shop, %s! Press f4 to start shopping.", m_pServer->ClientName(ClientID));
		m_pGameServer->SendChat(m_pGameServer->m_World.GetClosestShopDummy(pChr->GetPos(), pChr, ClientID), CHAT_SINGLE, ClientID, aBuf);
	}

	m_InShop[ClientID] = true;
}

void CShop::OnShopLeave(int ClientID)
{
	if (!m_InShop[ClientID])
		return;

	CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);

	if (pChr->m_TileIndex == TILE_SHOP || pChr->m_TileFIndex == TILE_SHOP)
		return;

	if (m_AntiSpamTick[ClientID] < m_pServer->Tick() && !m_pGameServer->IsShopDummy(ClientID))
	{
		m_pGameServer->SendChat(m_pGameServer->m_World.GetClosestShopDummy(pChr->GetPos(), pChr, ClientID), CHAT_SINGLE, ClientID, "Bye! Come back if you need something.");
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
	if (Item < ITEM_RAINBOW || Item > NUM_ITEMS)
		return "Unknown";

	const char *pStart = "~  ";
	const char *pEnd = "  ~";

	static char aRet[64];
	str_copy(aRet, pStart, sizeof(aRet));

	int s = str_length(pStart);

	int NameLength = str_length(m_aItems[Item].m_pName) + 1;
	for (int i = 0; i < NameLength; i++)
	{
		aRet[s] = str_uppercase(m_aItems[Item].m_pName[i]);
		if (i == NameLength - 2)
			break;
		str_append(aRet, " ", sizeof(aRet));
		s += 2;
	}
	str_append(aRet, pEnd, sizeof(aRet));

	return aRet;
}

const char *CShop::GetTimeMessage(int Time)
{
	switch (Time)
	{
	case TIME_DEATH: return "You own this item until you die.";
	case TIME_DISCONNECT: return "You own this item until\nyou disconnect.";
	case TIME_FOREVER: return "You own this item forever.";
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
			m_pGameServer->SendBroadcast("~ S H O P ~", ClientID, false);
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
		if (m_WindowPage[ClientID] >= NUM_ITEMS_LIST)
			m_WindowPage[ClientID] = PAGE_MAIN;
	}
	else if (Dir == -1)
	{
		m_WindowPage[ClientID]--;
		if (m_WindowPage[ClientID] < PAGE_MAIN)
			m_WindowPage[ClientID] = NUM_ITEMS_LIST-1;
	}

	m_BackgroundItem[ClientID] = m_WindowPage[ClientID];
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

	SendWindow(ClientID, m_BackgroundItem[ClientID]);
}

void CShop::SendWindow(int ClientID, int Item)
{
	int Page = Item;
	if (Item >= POLICE_RANK_1 && Item <= POLICE_RANK_5)
		Page = ITEM_POLICE;
	else if (Item >= TASER_LEVEL_1 && Item <= TASER_LEVEL_7)
		Page = ITEM_TASER;

	char aBase[512];
	if (m_WindowPage[ClientID] > PAGE_MAIN)
	{
		str_format(aBase, sizeof(aBase),
			"***************************\n"
			"         ~  S H O P  ~\n"
			"***************************\n\n"
			"%s\n\n"
			"Level: %d\n"
			"Price: %d\n"
			"Time: %s\n\n"
			"%s\n\n"
			"***************************\n"
			"If you want to buy an item press f3.\n\n\n"
			"              ~ %d ~              ", GetHeadline(Item), m_aItems[Item].m_Level, m_aItems[Item].m_Price, GetTimeMessage(m_aItems[Item].m_Time), m_aItems[Item].m_pDescription, Page);
	}
	else
	{
		str_format(aBase, sizeof(aBase),
			"***************************\n"
			"         ~  S H O P  ~\n"
			"***************************\n\n"
			"Welcome to the shop!\n\n"
			"By shooting to the right you go one site forward,\n"
			"and by shooting left you go one site\n"
			"back.\n\n"
			"***************************\n"
			"If you want to buy an item press f3.");
	}

	m_pGameServer->SendMotd(aBase, ClientID);
	m_MotdTick[ClientID] = m_pServer->Tick() + m_pServer->TickSpeed() * 10; // motd is there for 10 sec
}

void CShop::ConfirmPurchase(int ClientID)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf),
		"***************************\n"
		"        ~  S H O P  ~      \n"
		"***************************\n\n"
		"Are you sure you want to buy this item?\n\n"
		"f3 - yes\n"
		"f4 - no\n\n"
		"***************************\n");

	m_pGameServer->SendMotd(aBuf, ClientID);
	m_PurchaseState[ClientID] = STATE_CONFIRM;
}

void CShop::EndPurchase(int ClientID, bool Cancelled)
{
	char aResult[256];
	if (Cancelled)
	{
		char aBuf[256];
		str_format(aResult, sizeof(aResult), "You canceled the purchase.");
		str_format(aBuf, sizeof(aBuf),
			"***************************\n"
			"        ~  S H O P  ~      \n"
			"***************************\n\n"
			"%s\n\n"
			"***************************\n", aResult);

		m_pGameServer->SendMotd(aBuf, ClientID);
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
	if (Item < ITEM_RAINBOW || Item >= NUM_ITEMS_LIST)
	{
		m_pGameServer->SendChatTarget(ClientID, "Invalid item");
		return;
	}

	CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);
	CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientID];
	CGameContext::AccountInfo *Account = &m_pGameServer->m_Accounts[pPlayer->GetAccID()];

	char aMsg[128];

	int ItemID = Item == ITEM_POLICE || Item == ITEM_TASER ? m_BackgroundItem[ClientID] : Item;

	// Check whether we have the item already
	if ((Item == ITEM_RAINBOW				&& (pChr->m_Rainbow || pPlayer->m_InfRainbow))
		|| (Item == ITEM_BLOODY				&& (pChr->m_Bloody || pChr->m_StrongBloody))
		|| (Item == ITEM_POLICE				&& (*Account).m_PoliceLevel == NUM_POLICE_LEVELS)
		|| (Item == ITEM_SPOOKY_GHOST		&& (*Account).m_SpookyGhost)
		|| (Item == ITEM_ROOM_KEY			&& (pPlayer->m_HasRoomKey))
		|| (Item == ITEM_VIP				&& (*Account).m_VIP)
		|| (Item == ITEM_SPAWN_SHOTGUN		&& (*Account).m_SpawnWeapon[0] == 5)
		|| (Item == ITEM_SPAWN_GRENADE		&& (*Account).m_SpawnWeapon[1] == 5)
		|| (Item == ITEM_SPAWN_RIFLE		&& (*Account).m_SpawnWeapon[2] == 5)
		|| (Item == ITEM_NINJAJETPACK		&& (*Account).m_Ninjajetpack)
		|| (Item == ITEM_TASER				&& (*Account).m_TaserLevel == NUM_TASER_LEVELS))
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

	// vip cant be bought ingame
	if (ItemID == ITEM_VIP)
	{
		m_pGameServer->SendChatTarget(ClientID, "VIP can only be bought using real money. Check '/vipinfo'");
		return;
	}

	// TEMPORARY, RAINBOW IS NOT OPTIMIZED YET, CRASHES SERVER, SO NOT POSSIBLE TO BUY
	if (ItemID == ITEM_RAINBOW)
	{
		m_pGameServer->SendChatTarget(ClientID, "Rainbow is currently disabled.");
		return;
	}

	// check for the correct price
	if ((*Account).m_Money < m_aItems[ItemID].m_Price)
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
	str_format(aMsg, sizeof(aMsg), "-%d money, bought '%s'", m_aItems[ItemID].m_Price, m_aItems[ItemID].m_pName);
	pPlayer->MoneyTransaction(-m_aItems[Item].m_Price, aMsg);

	// give us the bought item
	int Weapon = -1;

	switch (Item)
	{
	case ITEM_RAINBOW:			pChr->Rainbow(true, -1, true); break;
	case ITEM_BLOODY:			pChr->Bloody(true, -1, true); break;
	case ITEM_POLICE:			(*Account).m_PoliceLevel++; break;
	case ITEM_SPOOKY_GHOST:		(*Account).m_SpookyGhost = true; break;
	case ITEM_ROOM_KEY:			pPlayer->m_HasRoomKey = true; pChr->Core()->m_MoveRestrictionExtra.m_CanEnterRoom = true; break;
	case ITEM_VIP: break;
	case ITEM_SPAWN_SHOTGUN:	if (Weapon == -1) Weapon = 0;
		// fallthrough
	case ITEM_SPAWN_GRENADE:	if (Weapon == -1) Weapon = 1;
		// fallthrough
	case ITEM_SPAWN_RIFLE:		if (Weapon == -1) Weapon = 2;
								(*Account).m_SpawnWeapon[Weapon]++; break;
	case ITEM_NINJAJETPACK:		(*Account).m_Ninjajetpack = true; break;
	case ITEM_TASER:			(*Account).m_TaserLevel++; break;
	}
}
