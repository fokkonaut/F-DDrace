/*************************************************
*                                                *
*              B L O C K D D R A C E             *
*                                                *
**************************************************/

#include <game/server/gamecontext.h>
#include "character.h"
#include <game/server/player.h>

#define MAX_SHOP_PAGES 3 // UPDATE THIS WITH EVERY PAGE YOU ADD!!!!!

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
	CGameContext::AccountInfo Account = GameServer()->m_Accounts[m_pPlayer->GetAccID()];

	if (ItemID == 1)
	{
		if (m_Rainbow || m_pPlayer->m_InfRainbow)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You already own rainbow.");
			return;
		}

		if (Account.m_Level < 5)
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Your level is too low! You need to be level 5 to buy rainbow.");
		else
		{
			if (Account.m_Money >= 1500)
			{
				m_pPlayer->MoneyTransaction(-1500, "-1.500 money. (bought 'rainbow')");
				Rainbow(false, -1, true);
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You bought rainbow until death.");
			}
			else
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You don't have enough money! You need 1.500 money.");
		}
	}
	else if (ItemID == 2)
	{
		if (m_Bloody || m_StrongBloody)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You already own bloody.");
			return;
		}

		if (Account.m_Level < 15)
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Your level is too low! You need to be level 15 to buy bloody.");
		else
		{
			if (Account.m_Money >= 3500)
			{
				m_pPlayer->MoneyTransaction(-3500, "-3.500 money. (bought 'bloody')");
				Bloody(false, -1, true);
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You bought bloody until death.");
			}
			else
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You don't have enough money! You need 3.500 money.");
		}
	}
	else if (ItemID == 3)
	{
		if (!Account.m_aHasItem[POLICE])
		{
			if (Account.m_Level < 18)
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Your level is too low! You need to be level 18 to buy police.");
				return;
			}
		}
		else if (Account.m_PoliceLevel == 1)
		{
			if (Account.m_Level < 25)
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Your level is too low! You need to be level 25 to upgrade to police rank 2.");
				return;
			}
		}
		else if (Account.m_PoliceLevel == 2)
		{
			if (Account.m_Level < 30)
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Your level is too low! You need to be level 30 to upgrade to police rank 3.");
				return;
			}
		}
		else if (Account.m_PoliceLevel == 3)
		{
			if (Account.m_Level < 40)
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Your level is too low! You need to be level 40 to upgrade to police rank 4.");
				return;
			}
		}
		else if (Account.m_PoliceLevel == 4)
		{
			if (Account.m_Level < 50)
			{
				GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Your level is too low! You need to be level 50 to upgrade to police rank 5.");
				return;
			}
		}
		if (Account.m_PoliceLevel >= 5)
		{
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You already have the highest police rank.");
			return;
		}
		if (Account.m_Money >= 100000)
		{
			m_pPlayer->MoneyTransaction(-100000, "-100.000 money. (bought 'police')");
			Account.m_PoliceLevel++;
			str_format(aBuf, sizeof(aBuf), "You bought Police Level %d", Account.m_PoliceLevel);
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		}
		else
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You don't have enough money! You need 100.000 money.");
	}
	else
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "Invalid shop item. Choose another one.");
}