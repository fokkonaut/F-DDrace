#include <game/server/gamecontext.h>
#include "character.h"
#include <game/server/player.h>

#define MAX_SHOP_PAGES 11 // UPDATE THIS WITH EVERY PAGE YOU ADD!!!!!

void CCharacter::ShopWindow(int Dir)
{
	m_ShopMotdTick = 0;

	if (Dir == 0)
		m_ShopWindowPage = SHOP_PAGE_MAIN;
	else if (Dir == 1)
	{
		m_ShopWindowPage++;
		if (m_ShopWindowPage > MAX_SHOP_PAGES)
			m_ShopWindowPage = SHOP_PAGE_MAIN;
	}
	else if (Dir == -1)
	{
		m_ShopWindowPage--;
		if (m_ShopWindowPage < SHOP_PAGE_MAIN)
			m_ShopWindowPage = MAX_SHOP_PAGES;
	}

	char aItem[256];
	char aLevelTmp[128];
	char aPriceTmp[16];
	char aTimeTmp[256];
	char aInfo[1028];

	if (m_ShopWindowPage == 0)
	{
		str_format(aItem, sizeof(aItem), "Welcome to the shop!\n\n"
			"By shooting to the right you go one site forward,\n"
			"and by shooting left you go one site\n"
			"backwards.");
	}
	else if (m_ShopWindowPage == 1)
	{
		str_format(aItem, sizeof(aItem), "        ~  R A I N B O W  ~      ");
		str_format(aLevelTmp, sizeof(aLevelTmp), "5");
		str_format(aPriceTmp, sizeof(aPriceTmp), "1.500");
		str_format(aTimeTmp, sizeof(aTimeTmp), "You own this item until you die.");
		str_format(aInfo, sizeof(aInfo), "Rainbow will make your tee change the color very fast.");
	}
	else if (m_ShopWindowPage == 2)
	{
		str_format(aItem, sizeof(aItem), "        ~  B L O O D Y  ~      ");
		str_format(aLevelTmp, sizeof(aLevelTmp), "15");
		str_format(aPriceTmp, sizeof(aPriceTmp), "3.500");
		str_format(aTimeTmp, sizeof(aTimeTmp), "You own this item until you die.");
		str_format(aInfo, sizeof(aInfo), "Bloody will give your tee a permanent kill effect.");
	}
	else if (m_ShopWindowPage == 3)
	{
		str_format(aItem, sizeof(aItem), "        ~  P O L I C E  ~      ");
		str_format(aLevelTmp, sizeof(aLevelTmp), "18");
		str_format(aPriceTmp, sizeof(aPriceTmp), "100.000");
		str_format(aTimeTmp, sizeof(aTimeTmp), "You own this item forever.");
		str_format(aInfo, sizeof(aInfo), "Police officers get help from the police bot.\n"
			"For more information about the specific police ranks\n"
			"please say '/policeinfo'.");
	}
	else if (m_ShopWindowPage == 4)
	{
		str_format(aItem, sizeof(aItem), "       ~  S P O O K Y G H O S T  ~     ");
		str_format(aLevelTmp, sizeof(aLevelTmp), "1");
		str_format(aPriceTmp, sizeof(aPriceTmp), "1.000.000");
		str_format(aTimeTmp, sizeof(aTimeTmp), "You own this item forever.");
		str_format(aInfo, sizeof(aInfo), "Using this item you can hide from other players behind bushes.\n"
			"If your ghost is activated you will be able to shoot plasma\n"
			"projectiles. For more information please visit '/spookyghostinfo'.");
	}
	else if (m_ShopWindowPage == 5)
	{
		str_format(aItem, sizeof(aItem), "        ~  R O O M K E Y  ~      ");
		str_format(aLevelTmp, sizeof(aLevelTmp), "16");
		str_format(aPriceTmp, sizeof(aPriceTmp), "5.000");
		str_format(aTimeTmp, sizeof(aTimeTmp), "You own this item until\n"
			"you disconnect.");
		str_format(aInfo, sizeof(aInfo), "If you have the room key you can enter the room.\n"
			"It's under the spawn and there is a money tile.");
	}
	else if (m_ShopWindowPage == 6)
	{
		str_format(aItem, sizeof(aItem), "        ~  V I P ~      ");
		str_format(aLevelTmp, sizeof(aLevelTmp), "0");
		str_format(aPriceTmp, sizeof(aPriceTmp), "5 euros");
		str_format(aTimeTmp, sizeof(aTimeTmp), "You own this item forever.");
		str_format(aInfo, sizeof(aInfo), "VIP gives you some benefits,\ncheck '/vipinfo'.");
	}
	else if (m_ShopWindowPage == 7)
	{
		str_format(aItem, sizeof(aItem), "     ~  S P A W N S H O T G U N  ~   ");
		str_format(aLevelTmp, sizeof(aLevelTmp), "33");
		str_format(aPriceTmp, sizeof(aPriceTmp), "600.000");
		str_format(aTimeTmp, sizeof(aTimeTmp), "You own this item forever.");
		str_format(aInfo, sizeof(aInfo), "You will have shotgun if you respawn.\n"
			"For more information about spawn weapons,\n"
			"please type '/spawnweaponsinfo'.");
	}
	else if (m_ShopWindowPage == 8)
	{
		str_format(aItem, sizeof(aItem), "      ~  S P A W N G R E N A D E  ~    ");
		str_format(aLevelTmp, sizeof(aLevelTmp), "33");
		str_format(aPriceTmp, sizeof(aPriceTmp), "600.000");
		str_format(aTimeTmp, sizeof(aTimeTmp), "You own this item forever.");
		str_format(aInfo, sizeof(aInfo), "You will have grenade if you respawn.\n"
			"For more information about spawn weapons,\n"
			"please type '/spawnweaponsinfo'.");
	}
	else if (m_ShopWindowPage == 9)
	{
		str_format(aItem, sizeof(aItem), "       ~  S P A W N R I F L E  ~       ");
		str_format(aLevelTmp, sizeof(aLevelTmp), "33");
		str_format(aPriceTmp, sizeof(aPriceTmp), "600.000");
		str_format(aTimeTmp, sizeof(aTimeTmp), "You own this item forever.");
		str_format(aInfo, sizeof(aInfo), "You will have rifle if you respawn.\n"
			"For more information about spawn weapons,\n"
			"please type '/spawnweaponsinfo'.");
	}
	else if (m_ShopWindowPage == 10)
	{
		str_format(aItem, sizeof(aItem), "       ~  N I N J A J E T P A C K  ~     ");
		str_format(aLevelTmp, sizeof(aLevelTmp), "21");
		str_format(aPriceTmp, sizeof(aPriceTmp), "10.000");
		str_format(aTimeTmp, sizeof(aTimeTmp), "You own this item forever.");
		str_format(aInfo, sizeof(aInfo), "It will make your jetpack gun be a ninja.\n"
			"Toggle it using '/ninjajetpack'.");
	}
	else if (m_ShopWindowPage == 11)
	{
		str_format(aItem, sizeof(aItem), "        ~  T A S E R  ~      ");
		str_format(aLevelTmp, sizeof(aLevelTmp), "30");
		str_format(aPriceTmp, sizeof(aPriceTmp), "50.000");
		str_format(aTimeTmp, sizeof(aTimeTmp), "You own this item forever.");
		str_format(aInfo, sizeof(aInfo), "Taser is a rifle that freezes a player\n"
			"For more information about the taser and your taser stats,\n"
			"plase visit '/taserinfo'.");
	}
	else
	{
		aItem[0] = 0;
	}
	//////////////////// UPDATE MAX_SHOP_PAGES ON TOP OF THIS FILE !!! /////////////////////////

	char aLevel[128];
	str_format(aLevel, sizeof(aLevel), "Needed level: %s", aLevelTmp);
	char aPrice[32];
	str_format(aPrice, sizeof(aPrice), "Price: %s", aPriceTmp);
	char aTime[256];
	str_format(aTime, sizeof(aTime), "Time: %s", aTimeTmp);

	char aBase[512];
	if (m_ShopWindowPage > SHOP_PAGE_MAIN)
	{
		str_format(aBase, sizeof(aBase),
			"***************************\n"
			"        ~  S H O P  ~      \n"
			"***************************\n\n"
			"%s\n\n"
			"%s\n"
			"%s\n"
			"%s\n\n"
			"%s\n\n"
			"***************************\n"
			"If you want to buy an item press f3.\n\n\n"
			"              ~ %d ~              ", aItem, aLevel, aPrice, aTime, aInfo, m_ShopWindowPage);
	}
	else
	{
		str_format(aBase, sizeof(aBase),
			"***************************\n"
			"        ~  S H O P  ~      \n"
			"***************************\n\n"
			"%s\n\n"
			"***************************\n"
			"If you want to buy an item press f3.", aItem);
	}

	GameServer()->SendMotd(aBase, GetPlayer()->GetCID());
	m_ShopMotdTick = Server()->Tick() + Server()->TickSpeed() * 10; // motd is there for 10 sec
}

void CCharacter::ConfirmPurchase()
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

	GameServer()->SendMotd(aBuf, GetPlayer()->GetCID());
	m_PurchaseState = SHOP_STATE_CONFIRM;;
}

void CCharacter::PurchaseEnd(bool Canceled)
{
	char aResult[256];
	if (Canceled)
	{
		char aBuf[256];
		str_format(aResult, sizeof(aResult), "You canceled the purchase.");
		str_format(aBuf, sizeof(aBuf),
			"***************************\n"
			"        ~  S H O P  ~      \n"
			"***************************\n\n"
			"%s\n\n"
			"***************************\n", aResult);

		GameServer()->SendMotd(aBuf, GetPlayer()->GetCID());
	}
	else
	{
		BuyItem(m_ShopWindowPage);
		ShopWindow(SHOP_STATE_NONE);
	}

	m_PurchaseState = SHOP_STATE_OPENED_WINDOW;
}

void CCharacter::BuyItem(int ItemID)
{
	if (ItemID < 1 || ItemID > MAX_SHOP_PAGES)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Invalid item");
		return;
	}

	CGameContext::AccountInfo *Account = &GameServer()->m_Accounts[m_pPlayer->GetAccID()];

	char aMsg[128];
	char aItem[64];
	int Level = -1;
	int Price = -1;
	int Time = -1; // 0 death, 1 disconnect, 2 forever

	switch (ItemID)
	{
		case 1:
		{
			str_format(aItem, sizeof(aItem), "Rainbow");
			Level = 5;
			Price = 1500;
			Time = 0;
		} break;
		case 2:
		{
			str_format(aItem, sizeof(aItem), "Bloody");
			Level = 15;
			Price = 3500;
			Time = 0;
		} break;
		case 3:
		{
			str_format(aItem, sizeof(aItem), "Police Rank %d", (*Account).m_PoliceLevel + 1);
			if (!(*Account).m_aHasItem[POLICE])
				Level = 18;
			else if ((*Account).m_PoliceLevel == 1)
				Level = 25;
			else if ((*Account).m_PoliceLevel == 2)
				Level = 30;
			else if ((*Account).m_PoliceLevel == 3)
				Level = 40;
			else if ((*Account).m_PoliceLevel == 4)
				Level = 50;
			Price = 100000;
			Time = 2;
		} break;
		case 4:
		{
			str_format(aItem, sizeof(aItem), "Spooky Ghost");
			Level = 1;
			Price = 1000000;
			Time = 2;
		} break;
		case 5:
		{
			str_format(aItem, sizeof(aItem), "Room Key");
			Level = 16;
			Price = 5000;
			Time = 1;
		} break;
		case 6:
		{
			str_format(aItem, sizeof(aItem), "VIP");
			Level = 0;
			Price = 5;
			Time = 2;
		} break;
		case 7:
		{
			str_format(aItem, sizeof(aItem), "Spawn Shotgun");
			Level = 33;
			Price = 600000;
			Time = 2;
		} break;
		case 8:
		{
			str_format(aItem, sizeof(aItem), "Spawn Grenade");
			Level = 33;
			Price = 600000;
			Time = 2;
		} break;
		case 9:
		{
			str_format(aItem, sizeof(aItem), "Spawn Rifle");
			Level = 33;
			Price = 600000;
			Time = 2;
		} break;
		case 10:
		{
			str_format(aItem, sizeof(aItem), "Ninjajetpack");
			Level = 21;
			Price = 10000;
			Time = 2;
		} break;
		case 11:
		{
			str_format(aItem, sizeof(aItem), "Taser Level %d", (*Account).m_TaserLevel + 1);
			Level = 30;
			Price = GameServer()->m_aTaserPrice[(*Account).m_TaserLevel];
			Time = 2;
		}
	}

	// check whether we have the item already
	bool HasAlready = false;

	switch (ItemID)
	{
	case 1: if (m_Rainbow || m_pPlayer->m_InfRainbow) HasAlready = true; break;
	case 2: if (m_Bloody || m_StrongBloody) HasAlready = true; break;
	case 3: if ((*Account).m_PoliceLevel == 5) HasAlready = true; break;
	case 4: if ((*Account).m_aHasItem[SPOOKY_GHOST]) HasAlready = true; break;
	case 5: if (m_pPlayer->m_HasRoomKey) HasAlready = true; break;
	case 6: if ((*Account).m_VIP) HasAlready = true; break;
	case 7: if ((*Account).m_SpawnWeapon[0] == 5) HasAlready = true; break;
	case 8: if ((*Account).m_SpawnWeapon[1] == 5) HasAlready = true; break;
	case 9: if ((*Account).m_SpawnWeapon[2] == 5) HasAlready = true; break;
	case 10: if ((*Account).m_Ninjajetpack) HasAlready = true; break;
	case 11: if ((*Account).m_TaserLevel == 7) HasAlready = true; break;
	}

	if (HasAlready)
	{
		bool UseThe = false;
		switch (ItemID)
		{
		case 3: GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You already have the highest police rank"); break;
		case 7: case 8: case 9: GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You already have the maximum amount of bullets"); break;
		case 11: GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You already have the maximum taser level"); break;
		case 4: case 5: UseThe = true;
			// fallthrough
		default:
			str_format(aMsg, sizeof(aMsg), "You already have %s%s", UseThe ? "the " : "", aItem);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aMsg);
		}
		return;
	}

	if (ItemID == 6) // vip can only be bought with real money
		return;

	if ((*Account).m_Money < Price)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You don't have enough money");
		return;
	}

	if ((*Account).m_Level < Level)
	{
		str_format(aMsg, sizeof(aMsg), "Your level is too low, you need to be level %d to buy %s", Level, aItem);
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), aMsg);
		return;
	}

	str_format(aMsg, sizeof(aMsg), "You bought %s %s", aItem, Time == 0 ? "until death" : Time == 1 ? "until disconnect" : "");
	GameServer()->SendChatTarget(m_pPlayer->GetCID(), aMsg);

	str_format(aMsg, sizeof(aMsg), "-%d money, bought '%s'", Price, aItem);
	m_pPlayer->MoneyTransaction(-Price, aMsg);

	// give us the bought item
	int Weapon = -1;
	switch (ItemID)
	{
	case 1: Rainbow(true, -1, true); break;
	case 2: Bloody(true, -1, true); break;
	case 3:
		if (!(*Account).m_aHasItem[POLICE])
			(*Account).m_aHasItem[POLICE] = true;
		(*Account).m_PoliceLevel++; break;
	case 4: (*Account).m_aHasItem[SPOOKY_GHOST] = true; break;
	case 5: m_pPlayer->m_HasRoomKey = true; m_Core.m_MoveRestrictionExtra.m_CanEnterRoom = true; break;
	case 7: if (Weapon == -1) Weapon = 0;
		// fallthrough
	case 8: if (Weapon == -1) Weapon = 1;
		// fallthrough
	case 9: if (Weapon == -1) Weapon = 2;
		// fallthrough
		(*Account).m_SpawnWeapon[Weapon]++; break;
	case 10: (*Account).m_Ninjajetpack = true; break;
	case 11: (*Account).m_TaserLevel++; break;
	}
}