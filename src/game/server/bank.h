// made by fokkonaut

#ifndef GAME_BANK_H
#define GAME_BANK_H

#include "house.h"
#include <engine/shared/protocol.h>

enum BankPages
{
	AMOUNT_EVERYTHING = PAGE_MAIN+1,
	AMOUNT_100,
	AMOUNT_1K,
	AMOUNT_5K,
	AMOUNT_10K,
	AMOUNT_50K,
	AMOUNT_100K,
	AMOUNT_500K,
	AMOUNT_1MIL,
	AMOUNT_5MIL,
	AMOUNT_10MIL,
	AMOUNT_50MIL,
	AMOUNT_100MIL,
	NUM_PAGES_BANK
};

enum BankAssignment
{
	ASSIGNMENT_NONE,
	ASSIGNMENT_DEPOSIT,
	ASSIGNMENT_WITHDRAW,
};

class CBank : public CHouse
{
private:
	int GetAmount(int Type, int ClientID = -1);
	int m_aAssignmentMode[MAX_CLIENTS];
	bool NotLoggedIn(int ClientID);

	virtual int FirstPage() { return AMOUNT_EVERYTHING; }
	virtual int NumPages() { return NUM_PAGES_BANK; }

public:
	CBank(CGameContext *pGameServer);
	virtual ~CBank() {};

	virtual void OnPageChange(int ClientID);
	virtual void OnSuccess(int ClientID);
	virtual const char *GetWelcomeMessage(int ClientID);
	virtual const char *GetConfirmMessage(int ClientID);
	virtual const char *GetEndMessage(int ClientID);
	virtual void SetAssignment(int ClientID, int Dir);
};

#endif
