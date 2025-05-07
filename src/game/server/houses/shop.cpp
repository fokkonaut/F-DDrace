// made by fokkonaut

#include "shop.h"
#include <game/server/gamecontext.h>

CShop::CShop(CGameContext *pGameServer, int Type) : CHouse(pGameServer, Type)
{
	// shop types only
	if (Type != HOUSE_SHOP && Type != HOUSE_PLOT_SHOP)
		return;

	m_aItems[PAGE_MAIN].m_Used = true;
	for (int i = PAGE_MAIN+1; i < MAX_PLOTS; i++)
		m_aItems[i].m_Used = false;

	m_CurrentDiscount = GameServer()->Config()->m_SvEuroDiscountPercentage;

	if (IsType(HOUSE_SHOP))
	{
		m_NumItems = NUM_ITEMS_SHOP;
		m_NumItemsList = NUM_ITEMS_SHOP_LIST;

		bool EuroMode = GameServer()->Config()->m_SvEuroMode;
		AddItem("Rainbow", 5, 1500, TIME_DEATH, Localizable("Rainbow will make your tee change the color very fast."));
		AddItem("Bloody", 15, 3500, TIME_DEATH, Localizable("Bloody will give your tee a permanent kill effect."));
		AddItem("Police", -1, 100000, TIME_FOREVER, Localizable("Police officers get help from the police bot. For more information about the specific police ranks, please say '/police'."));
		AddItem("Spooky Ghost", 1, 1000000, TIME_FOREVER, Localizable("Using this item you can hide from other players behind bushes. If your ghost is activated you will be able to shoot plasma projectiles. How it works: '/helptoggle'"));
		AddItem("Room Key", 16, 5000, TIME_DISCONNECT, Localizable("If you have the room key you can enter the room. It's under the spawn and there is a money tile."));
		AddItem("VIP Classic", 1, EuroMode ? 5 : 500000, TIME_30_DAYS, Localizable("VIP Classic gives you some benefits, check '/vip'."), EuroMode);
		AddItem("VIP+", 1, EuroMode ? 10 : 1000000, TIME_20_DAYS, Localizable("VIP+ gives you even more benefits than VIP Classic, check '/vip'."), EuroMode);
		AddItem("Spawn Shotgun", 33, 600000, TIME_FOREVER, Localizable("You will have shotgun if you respawn. For more information about spawn weapons, please type '/spawnweapons'."));
		AddItem("Spawn Grenade", 33, 600000, TIME_FOREVER, Localizable("You will have grenade if you respawn. For more information about spawn weapons, please type '/spawnweapons'."));
		AddItem("Spawn Rifle", 33, 600000, TIME_FOREVER, Localizable("You will have rifle if you respawn. For more information about spawn weapons, please type '/spawnweapons'."));
		AddItem("Ninjajetpack", 21, 10000, TIME_FOREVER, Localizable("It will make your jetpack gun be a ninja. Toggle it using '/ninjajetpack'."));
		AddItem("Taser", 30, -1, TIME_FOREVER, Localizable("Taser is a rifle that freezes a player. For more information about the taser and your taser stats, plase visit '/taser'."));
		AddItem("Taser battery", 30, 100000, TIME_FOREVER, Localizable("Taser battery is required to use the taser. Maximum amount of ammo is 100. Plase visit '/taser'."), false, 10);
		AddItem("Portal Rifle", EuroMode ? 1 : 45, EuroMode ? 10 : 500000, TIME_20_DAYS, Localizable("With Portal Rifle you can create two portals where your cursor is, then teleport between them."), EuroMode);
		AddItem("Portal Blocker", 20, 10000, TIME_FOREVER, Localizable("Create portal blockers hammer and this ammo with your cursor. See '/portal' for your current amount. How it works: '/helptoggle'"), false, 10);

		static char aaBuf[NUM_POLICE_LEVELS][32];
		for (int i = 0; i < NUM_POLICE_LEVELS; i++)
		{
			str_format(aaBuf[i], sizeof(aaBuf[i]), "Police Rank %d", i+1);
			AddItem(aaBuf[i], GameServer()->m_aPoliceLevel[i], m_aItems[ITEM_POLICE].m_Price, m_aItems[ITEM_POLICE].m_Time, m_aItems[ITEM_POLICE].m_pDescription);
		}

		static char aaBuf2[NUM_TASER_LEVELS][32];
		for (int i = 0; i < NUM_TASER_LEVELS; i++)
		{
			str_format(aaBuf2[i], sizeof(aaBuf2[i]), "Taser Level %d", i+1);
			AddItem(aaBuf2[i], m_aItems[ITEM_TASER].m_Level, GameServer()->m_aTaserPrice[i], m_aItems[ITEM_TASER].m_Time, m_aItems[ITEM_TASER].m_pDescription);
		}
	}
	else if (IsType(HOUSE_PLOT_SHOP))
	{
		m_NumItems = m_NumItemsList = GameServer()->Collision()->m_NumPlots + 1;
		int Size;
		static char aaName[MAX_PLOTS][32];
		int Level;
		int Price;
		int Time;
		for (int i = PLOT_START; i < m_NumItems; i++)
		{
			Size = GameServer()->m_aPlots[i].m_Size;
			str_format(aaName[i], sizeof(aaName[i]), "Plot %d", i);
			Level = (Size + 1) * 20;
			Price = (Size + 1) * 50000;
			Time = Size == 0 ? TIME_7_DAYS : Size == 1 ? TIME_5_DAYS : -1;
			AddItem(aaName[i], Level, Price, Time, "");
		}
	}
}

void CShop::AddItem(const char *pName, int Level, float Price, int Time, const char *pDescription, bool IsEuro, int Amount)
{
	for (int i = 0; i < m_NumItems; i++)
	{
		if (!m_aItems[i].m_Used)
		{
			if (GameServer()->Config()->m_SvEuroMode && IsEuro)
			{
				Price = (Price / 100.f) * (100.f - m_CurrentDiscount);
			}

			m_aItems[i].m_pName = pName;
			m_aItems[i].m_Level = Level;
			m_aItems[i].m_Price = Price;
			m_aItems[i].m_Time = Time;
			m_aItems[i].m_pDescription = pDescription;
			m_aItems[i].m_IsEuro = IsEuro;
			m_aItems[i].m_Amount = Amount;
			m_aItems[i].m_Used = true;
			break;
		}
	}
}

bool CShop::PageValid(int Page)
{
	return !IsType(HOUSE_SHOP) || (Page != ITEM_PORTAL_RIFLE || GameServer()->Config()->m_SvPortalRifleShop);
}

const char *CShop::GetWelcomeMessage(int ClientID)
{
	return Localizable("Welcome to the shop, %s! Press F4 to start shopping.");
}

const char *CShop::GetConfirmMessage(int ClientID)
{
	return Localizable("Are you sure you want to buy this item?");
}

const char *CShop::GetEndMessage(int ClientID)
{
	return Localizable("You canceled the purchase.");
}

const char *CShop::GetHeadline(int Item)
{
	if (Item <= 0 || Item >= m_NumItems)
		return "Unknown";

	static char aRet[64];
	mem_zero(aRet, sizeof(aRet));

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

const char *CShop::GetTimeMessage(int Time)
{
	switch (Time)
	{
	case TIME_DEATH: return Localizable("You own this item until you die.");
	case TIME_DISCONNECT: return Localizable("You own this item until\nyou disconnect.");
	case TIME_FOREVER: return Localizable("You own this item forever.");
	case TIME_30_DAYS: return Localizable("You own this item for 30 days.");
	case TIME_20_DAYS: return Localizable("You own this item for 20 days.");
	case TIME_7_DAYS: return Localizable("You own this item for 7 days.");
	case TIME_5_DAYS: return Localizable("You own this item for 5 days.");
	}
	return "Unknown";
}

void CShop::OnSuccess(int ClientID)
{
	BuyItem(ClientID, m_aClients[ClientID].m_Page);
}

void CShop::OnPageChange(int ClientID)
{
	m_aBackgroundItem[ClientID] = m_aClients[ClientID].m_Page;

	if (IsType(HOUSE_SHOP))
	{
		if (m_aClients[ClientID].m_Page == ITEM_POLICE)
		{
			CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[GameServer()->m_apPlayers[ClientID]->GetAccID()];
			m_aBackgroundItem[ClientID] = clamp(POLICE_RANK_1 + pAccount->m_PoliceLevel, (int)POLICE_RANK_1, (int)POLICE_RANK_5);
		}
		else if (m_aClients[ClientID].m_Page == ITEM_TASER)
		{
			CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[GameServer()->m_apPlayers[ClientID]->GetAccID()];
			m_aBackgroundItem[ClientID] = clamp(TASER_LEVEL_1 + pAccount->m_TaserLevel, (int)TASER_LEVEL_1, (int)TASER_LEVEL_10);
		}
	}

	// send page
	int Item = m_aBackgroundItem[ClientID];
	char aMsg[512];
	if (m_aClients[ClientID].m_Page <= PAGE_MAIN)
	{
		str_copy(aMsg, GameServer()->m_apPlayers[ClientID]->Localize("Welcome to the shop!\n\nBy shooting to the right you go one site forward, and by shooting left you go one site back."), sizeof(aMsg));
	}
	else
	{
		char aDescription[256];
		if (IsType(HOUSE_SHOP))
			str_copy(aDescription, GameServer()->m_apPlayers[ClientID]->Localize(m_aItems[Item].m_pDescription), sizeof(aDescription));
		else if (IsType(HOUSE_PLOT_SHOP))
		{
			char aOwner[32];
			char aRented[64];
			bool Owned = GameServer()->m_aPlots[Item].m_aOwner[0] != 0;

			if (Owned)
			{
				str_format(aOwner, sizeof(aOwner), "'%s'", GameServer()->m_aPlots[Item].m_aDisplayName);
				str_format(aRented, sizeof(aRented), "%s: %s", GameServer()->m_apPlayers[ClientID]->Localize("Rented until"), GameServer()->GetDate(GameServer()->m_aPlots[Item].m_ExpireDate));
			}
			else
			{
				str_copy(aOwner, GameServer()->m_apPlayers[ClientID]->Localize("for sale"), sizeof(aOwner));
				str_copy(aRented, "", sizeof(aRented));
			}

			str_format(aDescription, sizeof(aDescription),
				"Size: %s\n"
				"Max. objects: %d\n"
				"Owner: %s\n"
				"%s",
				GameServer()->GetPlotSizeString(Item),
				GameServer()->GetMaxPlotObjects(Item),
				aOwner, aRented);
		}

		char aAmount[64] = "\n";
		if (IsType(HOUSE_SHOP) && m_aItems[Item].m_Amount > 1)
			str_format(aAmount, sizeof(aAmount), "Amount: %d\n\n", m_aItems[Item].m_Amount);

		char aPrice[16];
		if (m_aItems[Item].m_IsEuro)
			str_format(aPrice, sizeof(aPrice), "%.2f", m_aItems[Item].m_Price);
		else
			str_format(aPrice, sizeof(aPrice), "%d", (int)m_aItems[Item].m_Price);

		char aDiscountMsg[64] = "\0";
		if (m_CurrentDiscount && m_aItems[Item].m_IsEuro)
			str_format(aDiscountMsg, sizeof(aDiscountMsg), " (-%d%% %s!)", m_CurrentDiscount, GameServer()->m_apPlayers[ClientID]->Localize("discount"));

		str_format(aMsg, sizeof(aMsg),
			"%s\n\n"
			"Level: %d\n"
			"Price: %s%s%s\n"
			"Time: %s\n"
			"%s"
			"%s%s",
			GetHeadline(Item),
			m_aItems[Item].m_Level,
			aPrice,
			m_aItems[Item].m_IsEuro ? " EUR" : "", aDiscountMsg,
			GameServer()->m_apPlayers[ClientID]->Localize(GetTimeMessage(m_aItems[Item].m_Time)),
			aAmount,
			aDescription,
			(m_aItems[Item].m_IsEuro && GameServer()->Config()->m_SvEuroMode) ? "\n\nHow to get EUR ingame? Contact the admin and donate to the server, it will get added to your ingame euros.\n\nCheck '/account' for your details." : ""
		);
	}

	int Page = m_aClients[ClientID].m_Page;
	if (IsType(HOUSE_SHOP) && Page > ITEM_PORTAL_RIFLE && !GameServer()->Config()->m_SvPortalRifleShop)
		Page--;
	SendWindow(ClientID, aMsg, GameServer()->m_apPlayers[ClientID]->Localize("If you want to buy an item, press F3"), Page);
	m_aClients[ClientID].m_LastMotd = Server()->Tick();
}

void CShop::BuyItem(int ClientID, int Item)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if (Item <= PAGE_MAIN || Item >= m_NumItemsList)
	{
		GameServer()->SendChatTarget(ClientID, pPlayer->Localize("Invalid item"));
		return;
	}

	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[pPlayer->GetAccID()];

	char aMsg[128];
	int ItemID = Item;
	int Amount = m_aItems[Item].m_Amount;

	char aDescription[64];
	str_copy(aDescription, m_aItems[ItemID].m_pName, sizeof(aDescription));

	if (IsType(HOUSE_SHOP))
	{
		if (Item == ITEM_POLICE || Item == ITEM_TASER)
			ItemID = m_aBackgroundItem[ClientID];

		// Check whether we have the item already
		if ((Item == ITEM_RAINBOW				&& (pChr->m_Rainbow || pPlayer->m_InfRainbow))
			|| (Item == ITEM_BLOODY				&& (pChr->m_Bloody || pChr->m_StrongBloody))
			|| (Item == ITEM_POLICE				&& pAccount->m_PoliceLevel >= NUM_POLICE_LEVELS)
			|| (Item == ITEM_SPOOKY_GHOST		&& pAccount->m_SpookyGhost)
			|| (Item == ITEM_ROOM_KEY			&& (pPlayer->m_HasRoomKey))
			//|| (Item == ITEM_VIP				&& pAccount->m_VIP) // vip can be bought unlimited times
			|| (Item == ITEM_SPAWN_SHOTGUN		&& pAccount->m_SpawnWeapon[0] >= 5)
			|| (Item == ITEM_SPAWN_GRENADE		&& pAccount->m_SpawnWeapon[1] >= 5)
			|| (Item == ITEM_SPAWN_RIFLE		&& pAccount->m_SpawnWeapon[2] >= 5)
			|| (Item == ITEM_NINJAJETPACK		&& pAccount->m_Ninjajetpack)
			|| (Item == ITEM_TASER				&& pAccount->m_TaserLevel >= NUM_TASER_LEVELS)
			|| (Item == ITEM_TASER_BATTERY && pAccount->m_TaserBattery >= MAX_TASER_BATTERY)
			//|| (Item == ITEM_PORTAL_RIFLE		&& pAccount->m_PortalRifle) // portal rifle can be bought unlimited times
			)
		{
			bool UseThe = false;

			switch (Item)
			{
			case ITEM_POLICE:															GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You already have the highest police rank")); break;
			case ITEM_SPAWN_SHOTGUN: case ITEM_SPAWN_GRENADE: case ITEM_SPAWN_RIFLE:	GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You already have the maximum amount of bullets")); break;
			case ITEM_TASER:															GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You already have the maximum taser level")); break;
			case ITEM_TASER_BATTERY:													GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You already have a fully filled taser battery")); break;
			case ITEM_SPOOKY_GHOST: case ITEM_ROOM_KEY:									UseThe = true;
				// fallthrough
			default:
				if (UseThe)
					str_format(aMsg, sizeof(aMsg), pPlayer->Localize("You already have the %s"), m_aItems[ItemID].m_pName);
				else
					str_format(aMsg, sizeof(aMsg), pPlayer->Localize("You already have %s"), m_aItems[ItemID].m_pName);
				GameServer()->SendChatTarget(ClientID, aMsg);
			}
			return;
		}

		// check police lvl 3 for taser
		if ((Item == ITEM_TASER || Item == ITEM_TASER_BATTERY) && pAccount->m_PoliceLevel < 3)
		{
			GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You need to be police level 3 or higher to get a taser license"));
			return;
		}

		if (pAccount->m_VIP && ((Item == ITEM_VIP && pAccount->m_VIP != VIP_CLASSIC) || (Item == ITEM_VIP_PLUS && pAccount->m_VIP != VIP_PLUS)))
		{
			GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You can not buy this VIP level while owning another one"));
			return;
		}

		if (Item == ITEM_BLOODY && (pChr->m_Atom || pChr->m_Trail || pChr->m_RotatingBall || pChr->m_EpicCircle))
		{
			GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You can not buy bloody while specific cosmetics are activated"));
			return;
		}

		if (Item == ITEM_TASER_BATTERY)
		{
			Amount = clamp(MAX_TASER_BATTERY-pAccount->m_TaserBattery, 0, Amount);
		}

		if (Item == ITEM_TASER_BATTERY || Item == ITEM_PORTAL_BLOCKER)
		{
			str_format(aDescription, sizeof(aDescription), "%d %s", Amount, m_aItems[ItemID].m_pName);
		}
	}
	else if (IsType(HOUSE_PLOT_SHOP))
	{
		int OwnPlotID = GameServer()->GetPlotID(pPlayer->GetAccID());
		if (GameServer()->m_aPlots[Item].m_aOwner[0] != 0)
		{
			GameServer()->SendChatTarget(ClientID, pPlayer->Localize("This plot is already sold to someone else"));
			return;
		}
		else if (OwnPlotID == Item)
		{
			GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You already own that plot"));
			return;
		}
		else if (OwnPlotID != 0)
		{
			GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You already own another plot"));
			return;
		}
		else if (GameServer()->HasPlotByIP(ClientID))
		{
			GameServer()->SendChatTarget(ClientID, pPlayer->Localize("Your IP address already owns one plot"));
			return;
		}
	}

	if (Amount <= 0)
		return;

	float Price = Amount * (m_aItems[ItemID].m_Price/m_aItems[ItemID].m_Amount);

	// check for the correct price
	if ((m_aItems[ItemID].m_IsEuro && pAccount->m_Euros < Price)
		|| (!m_aItems[ItemID].m_IsEuro && pPlayer->GetUsableMoney() < Price))
	{
		GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You don't have enough money"));
		return;
	}

	// check for the correct level
	if (pAccount->m_Level < m_aItems[ItemID].m_Level)
	{
		str_format(aMsg, sizeof(aMsg), pPlayer->Localize("Your level is too low, you need to be level %d to buy %s"), m_aItems[ItemID].m_Level, m_aItems[ItemID].m_pName);
		GameServer()->SendChatTarget(ClientID, aMsg);
		return;
	}

	if (IsType(HOUSE_SHOP) && Item == ITEM_TASER_BATTERY && !pPlayer->GiveTaserBattery(Amount))
	{
		GameServer()->SendChatTarget(ClientID, pPlayer->Localize("Taser battery purchase failed"));
		return;
	}

	// send a message that we bought the item
	str_format(aMsg, sizeof(aMsg), pPlayer->Localize("You bought %s %s"), aDescription, m_aItems[ItemID].m_Time == TIME_DEATH ? pPlayer->Localize("until death") : m_aItems[ItemID].m_Time == TIME_DISCONNECT ? pPlayer->Localize("until disconnect") : "");
	GameServer()->SendChatTarget(ClientID, aMsg);
	if (Item == ITEM_VIP || Item == ITEM_VIP_PLUS || Item == ITEM_PORTAL_RIFLE)
		GameServer()->SendChatTarget(ClientID, pPlayer->Localize("Check '/account' for more information about the expiration date"));

	// apply a message to the history
	char aDiscountMsg[64] = "\0";
	if (m_CurrentDiscount && m_aItems[Item].m_IsEuro)
		str_format(aDiscountMsg, sizeof(aDiscountMsg), " (-%d%% discount)", m_CurrentDiscount);
	str_format(aMsg, sizeof(aMsg), "%s, bought '%s'%s", m_aItems[ItemID].m_IsEuro ? "EUR" : "money", aDescription, aDiscountMsg);
	if (m_aItems[ItemID].m_IsEuro)
		pPlayer->BankCurrTransaction(-Price, aMsg);
	else
		pPlayer->WalletTransaction(-Price, aMsg);


	if (IsType(HOUSE_SHOP))
	{
		// give us the bought item
		int Weapon = -1;

		switch (Item)
		{
		case ITEM_RAINBOW:			pChr->Rainbow(true, -1, true); break;
		case ITEM_BLOODY:			pChr->Bloody(true, -1, true); break;
		case ITEM_POLICE:			pAccount->m_PoliceLevel++; break;
		case ITEM_SPOOKY_GHOST:		pAccount->m_SpookyGhost = true; break;
		case ITEM_ROOM_KEY:			pPlayer->m_HasRoomKey = true; pChr->Core()->m_MoveRestrictionExtra.m_RoomKey = true; break;
		case ITEM_VIP_PLUS:			pChr->Core()->m_MoveRestrictionExtra.m_VipPlus = true;
			// fallthrough
		case ITEM_VIP:				pAccount->m_VIP = Item == ITEM_VIP ? VIP_CLASSIC : VIP_PLUS; pPlayer->SetExpireDate(Item); break;
		case ITEM_SPAWN_SHOTGUN:	if (Weapon == -1) Weapon = 0;
			// fallthrough
		case ITEM_SPAWN_GRENADE:	if (Weapon == -1) Weapon = 1;
			// fallthrough
		case ITEM_SPAWN_RIFLE:		if (Weapon == -1) Weapon = 2;
									pAccount->m_SpawnWeapon[Weapon]++; break;
		case ITEM_NINJAJETPACK:		pAccount->m_Ninjajetpack = true; break;
		case ITEM_TASER:			pAccount->m_TaserLevel++; break;
		case ITEM_TASER_BATTERY:	break; // done above
		case ITEM_PORTAL_RIFLE:		pAccount->m_PortalRifle = true; pPlayer->SetExpireDate(Item);
									if (pPlayer->GetCharacter())
										pPlayer->GetCharacter()->GiveWeapon(WEAPON_PORTAL_RIFLE);
									break;
		case ITEM_PORTAL_BLOCKER:	pAccount->m_PortalBlocker += Amount; break;
		}
	}
	else if (IsType(HOUSE_PLOT_SHOP))
	{
		GameServer()->SetPlotExpire(Item);
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), pPlayer->Localize("The plot will expire on %s"), GameServer()->GetDate(GameServer()->m_aPlots[Item].m_ExpireDate));
		GameServer()->SendChatTarget(ClientID, aBuf);
		GameServer()->SetPlotInfo(Item, pPlayer->GetAccID());
	}
}
