#ifndef GAME_SERVER_MINIGAMES_DUEL_BET_COMMAND_H
#define GAME_SERVER_MINIGAMES_DUEL_BET_COMMAND_H

#include <engine/console.h>

class CGameContext;
class CPlayer;

namespace Defaif
{
bool TryStart1vs1Bet(CGameContext *pGameContext, IConsole::IResult *pResult, CPlayer *pPlayer, int &OtherID);
}

#endif // GAME_SERVER_MINIGAMES_DUEL_BET_COMMAND_H
