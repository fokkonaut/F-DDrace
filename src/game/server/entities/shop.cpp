/*************************************************
*                                                *
*              B L O C K D D R A C E             *
*                                                *
**************************************************/

#include <game/server/gamecontext.h>
#include "character.h"
#include <game/server/player.h>

#define MAX_SHOP_PAGES 9 // UPDATE THIS WITH EVERY PAGE YOU ADD!!!!!

void CCharacter::ShopWindow(int Dir)
{
	m_ShopMotdTick = 0;
	int m_MaxShopPage = MAX_SHOP_PAGES;

	if (Dir == 0)
		m_ShopWindowPage = SHOP_PAGE_MAIN;
	else if (Dir == 1)
	{
		m_ShopWindowPage++;
		if (m_ShopWindowPage > m_MaxShopPage)
			m_ShopWindowPage = SHOP_PAGE_MAIN;
	}
	else if (Dir == -1)
	{
		m_ShopWindowPage--;
		if (m_ShopWindowPage < SHOP_PAGE_MAIN)
			m_ShopWindowPage = m_MaxShopPage;
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
	else
	{
		aItem[0] = 0;
	}
	//////////////////// UPDATE MAX_SHOP_PAGES ON TOP OF THIS FILE !!! /////////////////////////

	char aLevel[128];
	str_format(aLevel, sizeof(aLevel), "Needed level: %s", aLevelTmp);
	char aPrice[16];
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

void CCharacter::PurchaseEnd(bool canceled)
{
	char aResult[256];
	if (canceled)
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
		ShopWindow(0);
	}

	m_PurchaseState = SHOP_STATE_OPENED_WINDOW;
}

void CCharacter::BuyItem(int ItemID)
{
	if (!m_InShop)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You have to be in the shop to buy some items.");
		return;
	}

	char aBuf[256];
	CGameContext::AccountInfo *Account = &GameServer()->m_Accounts[m_pPlayer->GetAccID()];

	if (ItemID == 1)
	{
		if (m_Rainbow || m_pPlayer->m_InfRainbow)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You already own rainbow.");
			return;
		}

		if ((*Account).m_Level < 5)
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Your level is too low! You need to be level 5 to buy rainbow.");
		else
		{
			if ((*Account).m_Money >= 1500)
			{
				m_pPlayer->MoneyTransaction(-1500, "-1.500 money. (bought 'rainbow')");
				Rainbow(true, -1, true);
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You bought rainbow until death.");
			}
			else
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You don't have enough money!");
		}
	}
	else if (ItemID == 2)
	{
		if (m_Bloody || m_StrongBloody)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You already own bloody.");
			return;
		}

		if ((*Account).m_Level < 15)
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Your level is too low! You need to be level 15 to buy bloody.");
		else
		{
			if ((*Account).m_Money >= 3500)
			{
				m_pPlayer->MoneyTransaction(-3500, "-3.500 money. (bought 'bloody')");
				Bloody(true, -1, true);
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You bought bloody until death.");
			}
			else
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You don't have enough money!");
		}
	}
	else if (ItemID == 3)
	{
		if (!(*Account).m_aHasItem[POLICE])
		{
			if ((*Account).m_Level < 18)
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Your level is too low! You need to be level 18 to buy police.");
				return;
			}
		}
		else if ((*Account).m_PoliceLevel == 1)
		{
			if ((*Account).m_Level < 25)
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Your level is too low! You need to be level 25 to upgrade to police rank 2.");
				return;
			}
		}
		else if ((*Account).m_PoliceLevel == 2)
		{
			if ((*Account).m_Level < 30)
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Your level is too low! You need to be level 30 to upgrade to police rank 3.");
				return;
			}
		}
		else if ((*Account).m_PoliceLevel == 3)
		{
			if ((*Account).m_Level < 40)
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Your level is too low! You need to be level 40 to upgrade to police rank 4.");
				return;
			}
		}
		else if ((*Account).m_PoliceLevel == 4)
		{
			if ((*Account).m_Level < 50)
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Your level is too low! You need to be level 50 to upgrade to police rank 5.");
				return;
			}
		}
		if ((*Account).m_PoliceLevel >= 5)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You already have the highest police rank.");
			return;
		}
		if ((*Account).m_Money >= 100000)
		{
			m_pPlayer->MoneyTransaction(-100000, "-100.000 money. (bought 'police')");
			(*Account).m_PoliceLevel++;
			str_format(aBuf, sizeof(aBuf), "You bought Police Level %d", (*Account).m_PoliceLevel);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		}
		else
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You don't have enough money!");
	}
	else if (ItemID == 4)
	{
		if ((*Account).m_Level < 1)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Level is too low! You need lvl 1 to buy the spooky ghost.");
			return;
		}
		else if ((*Account).m_aHasItem[SPOOKY_GHOST])
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You already have the spooky ghost.");
		}
		else if ((*Account).m_Money >= 1000000)
		{
			m_pPlayer->MoneyTransaction(-1000000, "bought 'spooky ghost'");

			(*Account).m_aHasItem[SPOOKY_GHOST] = true;
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You bought the spooky ghost. For more infos check '/spookyghost'.");
		}
		else
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You don't have enough money!");
		}
	}
	else if (ItemID == 5)
	{
		if ((*Account).m_Level < 16)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Level is too low! You need lvl 16 to buy the room key.");
			return;
		}
		else if (m_Core.m_CanEnterRoom)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You already have the room key.");
		}
		else if ((*Account).m_Money >= 5000)
		{
			m_pPlayer->MoneyTransaction(-5000, "bought 'room key'");

			m_pPlayer->m_HasRoomKey = true;
			m_Core.m_CanEnterRoom = true;
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You bought the room key");
		}
		else
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You don't have enough money!");
		}
	}
	else if (ItemID == 6)
	{
		if ((*Account).m_Level < 0)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Level is too low! You need lvl 0 to buy VIP.");
			return;
		}
		else if ((*Account).m_VIP)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You already have VIP.");
		}
	}
	else if (ItemID == 7)
	{
		if ((*Account).m_Level < 33)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Level is too low! You need lvl 33 to buy spawn shotgun.");
			return;
		}
		else if ((*Account).m_SpawnWeapon[0] == 5)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You already have the maximum level for spawn shotgun.");
		}
		else if ((*Account).m_Money >= 600000)
		{
			m_pPlayer->MoneyTransaction(-600000, "bought 'spawn_shotgun'");

			(*Account).m_SpawnWeapon[0]++;
			if ((*Account).m_SpawnWeapon[0] == 1)
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You bought spawn shotgun. For more infos check '/spawnweaponsinfo'.");
			}
			else
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Spawn shotgun upgraded.");
			}
		}
		else
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You don't have enough money!");
		}
	}
	else if (ItemID == 8)
	{
		if ((*Account).m_Level < 33)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Level is too low! You need lvl 33 to buy spawn grenade.");
			return;
		}
		else if ((*Account).m_SpawnWeapon[1] == 5)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You already have the maximum level for spawn grenade.");
		}
		else if ((*Account).m_Money >= 600000)
		{
			m_pPlayer->MoneyTransaction(-600000, "bought 'spawn_grenade'");

			(*Account).m_SpawnWeapon[1]++;
			if ((*Account).m_SpawnWeapon[1] == 1)
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You bought spawn grenade. For more infos check '/spawnweaponsinfo'.");
			}
			else
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Spawn grenade upgraded.");
			}
		}
		else
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You don't have enough money!");
		}
	}
	else if (ItemID == 9)
	{
		if ((*Account).m_Level < 33)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Level is too low! You need lvl 33 to buy spawn rifle.");
			return;
		}
		else if ((*Account).m_SpawnWeapon[2] == 5)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You already have the maximum level for spawn rifle.");
		}
		else if ((*Account).m_Money >= 600000)
		{
			m_pPlayer->MoneyTransaction(-600000, "bought 'spawn_rifle'");

			(*Account).m_SpawnWeapon[2]++;
			if ((*Account).m_SpawnWeapon[2] == 1)
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You bought spawn rifle. For more infos check '/spawnweaponsinfo'.");
			}
			else
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Spawn rifle upgraded.");
			}
		}
		else
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You don't have enough money!");
		}
	}
	else
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Invalid shop item. Choose another one.");
}