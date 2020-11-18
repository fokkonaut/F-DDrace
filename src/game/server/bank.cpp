// made by fokkonaut

#include "bank.h"
#include "gamecontext.h"

CBank::CBank(CGameContext *pGameServer) : CHouse(pGameServer, HOUSE_BANK)
{

}

const char *CBank::GetWelcomeMessage(int ClientID)
{
	static char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Welcome to the bank, %s! Press F4 to manage your bank account.", Server()->ClientName(ClientID));
	return aBuf;
}

void CBank::OnKeyPress(int ClientID, int Dir)
{
	if (Dir == 1)
	{

	}
	else if (Dir == -1)
	{

	}
}

void CBank::OnPageChange(int ClientID)
{

}
