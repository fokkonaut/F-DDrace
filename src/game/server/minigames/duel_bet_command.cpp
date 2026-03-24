#include "duel_bet_command.h"

#include <climits>
#include <game/server/gamecontext.h>

namespace Defaif
{
bool TryStart1vs1Bet(CGameContext *pGameContext, IConsole::IResult *pResult, CPlayer *pPlayer, int &OtherID)
{
	char aBetArgs[128];
	str_copy(aBetArgs, pResult->GetFullString(), sizeof(aBetArgs));
	char *pBetArgs = str_skip_whitespaces(aBetArgs);
	if (str_comp_nocase_num(pBetArgs, "bet ", 4) != 0)
		return false;

	char *pBetAmount = str_skip_whitespaces(pBetArgs + 4);
	char *pAfterAmount = str_skip_to_whitespace(pBetAmount);
	if (!*pBetAmount)
	{
		pGameContext->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Usage: /1vs1 bet <amount> <playername>"));
		return true;
	}

	if (!*pAfterAmount)
	{
		if (str_is_number(pBetAmount) == 0)
			pGameContext->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Usage: /1vs1 bet <amount> <playername>"));
		else
			pGameContext->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Invalid 1vs1 bet amount"));
		return true;
	}

	*pAfterAmount = 0;
	if (str_is_number(pBetAmount) != 0)
	{
		pGameContext->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Invalid 1vs1 bet amount"));
		return true;
	}

	int64 BetAmount = atoll(pBetAmount);
	if (BetAmount <= 0 || BetAmount > INT_MAX / 2)
	{
		pGameContext->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Invalid 1vs1 bet amount"));
		return true;
	}

	char *pName = str_skip_whitespaces(pAfterAmount + 1);
	if (!pName[0])
	{
		pGameContext->SendChatTarget(pResult->m_ClientID, pPlayer->Localize("Usage: /1vs1 bet <amount> <playername>"));
		return true;
	}

	int ScoreLimit = 10;
	int KillBorder = 0;
	char aNameBuf[128];
	str_copy(aNameBuf, pName, sizeof(aNameBuf));
	const char *pBetRest = pGameContext->ParseWhisperTarget(aNameBuf, &OtherID);
	if (pBetRest[0])
	{
		int Num = sscanf(pBetRest, "%d %d", &ScoreLimit, &KillBorder);
		if (Num == 1)
			KillBorder = 0;
	}

	pGameContext->Arenas()->StartConfiguration(pResult->m_ClientID, OtherID, ScoreLimit, KillBorder, BetAmount);
	return true;
}
}
