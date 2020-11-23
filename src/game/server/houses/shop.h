// made by fokkonaut

#ifndef GAME_SHOP_H
#define GAME_SHOP_H

#include "house.h"
#include <engine/shared/protocol.h>
#include <game/mapitems.h>

enum ShopItems
{
	ITEM_RAINBOW = PAGE_MAIN+1,
	ITEM_BLOODY,
	ITEM_POLICE,
	ITEM_SPOOKY_GHOST,
	ITEM_ROOM_KEY,
	ITEM_VIP,
	ITEM_SPAWN_SHOTGUN,
	ITEM_SPAWN_GRENADE,
	ITEM_SPAWN_RIFLE,
	ITEM_NINJAJETPACK,
	ITEM_TASER,
	ITEM_TASER_BATTERY,
	ITEM_PORTAL_RIFLE,
	NUM_ITEMS_SHOP_LIST,

	POLICE_RANK_1 = NUM_ITEMS_SHOP_LIST,
	POLICE_RANK_2,
	POLICE_RANK_3,
	POLICE_RANK_4,
	POLICE_RANK_5,

	TASER_LEVEL_1,
	TASER_LEVEL_2,
	TASER_LEVEL_3,
	TASER_LEVEL_4,
	TASER_LEVEL_5,
	TASER_LEVEL_6,
	TASER_LEVEL_7,
	NUM_ITEMS_SHOP
};

enum ShopTime
{
	TIME_DEATH,
	TIME_DISCONNECT,
	TIME_FOREVER,
	TIME_30_DAYS,
	TIME_20_DAYS,
	TIME_7_DAYS,
	TIME_5_DAYS,
};

enum ShopExpire
{
	// days until item expired
	ITEM_EXPIRE_VIP = 30,
	ITEM_EXPIRE_PORTAL_RIFLE = 20,
	ITEM_EXPIRE_PLOT_SMALL = 7,
	ITEM_EXPIRE_PLOT_BIG = 5,
};

class CShop : public CHouse
{
private:
	struct ItemInfo
	{
		bool m_Used;
		const char *m_pName;
		int m_Level;
		int m_Price;
		int m_Time;
		const char *m_pDescription;
		bool m_IsEuro;
	} m_aItems[MAX_PLOTS];

	void AddItem(const char *pName, int Level, int Price, int Time, const char *pDescription, bool IsEuro = false);
	void BuyItem(int ClientID, int Item);

	const char *GetHeadline(int Item);
	const char *GetTimeMessage(int Time);

	virtual int NumPages() { return m_NumItemsList; }
	int m_aBackgroundItem[MAX_CLIENTS];

	// Because it differs for shop/plot shop
	int m_NumItems;
	int m_NumItemsList;

public:
	CShop(CGameContext *pGameServer, int Type);
	virtual ~CShop() {};

	const char *GetItemName(int Item) { return m_aItems[Item].m_pName; }

	virtual void OnPageChange(int ClientID);
	virtual void OnSuccess(int ClientID);
	virtual const char *GetWelcomeMessage(int ClientID);
	virtual const char *GetConfirmMessage(int ClientID);
	virtual const char *GetEndMessage(int ClientID);
};

#endif
