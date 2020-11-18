// made by fokkonaut

#ifndef GAME_BANK_H
#define GAME_BANK_H

#include "house.h"
#include <engine/shared/protocol.h>

enum BankPages
{
	NUM_PAGES_BANK
};

class CBank : public CHouse
{
private:

	virtual int NumPages() { return NUM_PAGES_BANK; }

public:
	CBank(CGameContext *pGameServer);
	virtual ~CBank() {};

	virtual void OnPageChange(int ClientID);
	virtual void OnKeyPress(int ClientID, int Dir);
	virtual const char *GetWelcomeMessage(int ClientID);
};

#endif
