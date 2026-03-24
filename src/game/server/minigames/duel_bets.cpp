#include <game/server/minigames/arenas.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>

bool CArenas::CanConfigureBet(int ClientID, int Participant, int64 BetAmount)
{
	if (BetAmount <= 0)
		return true;

	if (Participant == PARTICIPANT_GLOBAL)
	{
		GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("1vs1 bets are only available in direct duels"));
		return false;
	}

	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	CPlayer *pParticipant = Participant >= 0 ? GameServer()->m_apPlayers[Participant] : 0;
	if (pPlayer->GetAccID() < ACC_START)
	{
		GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You have to be logged in to start a 1vs1 bet"));
		return false;
	}
	if (GameServer()->m_Accounts[pPlayer->GetAccID()].m_Money < BetAmount)
	{
		GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You don't have enough money on your bank account"));
		return false;
	}
	if (!pParticipant || pParticipant->GetAccID() < ACC_START)
	{
		GameServer()->SendChatTarget(ClientID, pPlayer->Localize("That player is not logged in"));
		return false;
	}
	if (GameServer()->m_Accounts[pParticipant->GetAccID()].m_Money < BetAmount)
	{
		GameServer()->SendChatTarget(ClientID, pPlayer->Localize("That player doesn't have enough money on the bank account"));
		return false;
	}

	return true;
}

void CArenas::SendInviteMessages(int Fight, int ClientID, int Invited)
{
	char aBuf[128];
	if (m_aFights[Fight].m_BetAmount > 0)
		str_format(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[ClientID]->Localize("You invited '%s' to a 1vs1 bet of %lld money"), Server()->ClientName(Invited), m_aFights[Fight].m_BetAmount);
	else
		str_format(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[ClientID]->Localize("You invited '%s' to a fight"), Server()->ClientName(Invited));
	GameServer()->SendChatTarget(ClientID, aBuf);

	if (m_aFights[Fight].m_BetAmount > 0)
		str_format(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[Invited]->Localize("You have been invited to a 1vs1 bet of %lld money by '%s', type '/1vs1 %s' to join"), m_aFights[Fight].m_BetAmount, Server()->ClientName(ClientID), Server()->ClientName(ClientID));
	else
		str_format(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[Invited]->Localize("You have been invited to a fight by '%s', type '/1vs1 %s' to join"), Server()->ClientName(ClientID), Server()->ClientName(ClientID));
	GameServer()->SendChatTarget(Invited, aBuf);

	if (GameServer()->m_apPlayers[Invited] && GameServer()->m_apPlayers[Invited]->m_Minigame != MINIGAME_1VS1)
		GameServer()->SendChatTarget(Invited, GameServer()->m_apPlayers[Invited]->Localize("Join the 1vs1 lobby using '/1vs1' before you accept the fight"));
}

bool CArenas::TryCollectBetOnStart(int Fight)
{
	CFight *pFight = &m_aFights[Fight];
	if (pFight->m_BetAmount <= 0 || pFight->m_BetCollected)
		return true;

	int ClientID0 = pFight->m_aParticipants[0].m_ClientID;
	int ClientID1 = pFight->m_aParticipants[1].m_ClientID;
	CCharacter *pChr0 = GameServer()->GetPlayerChar(ClientID0);
	CCharacter *pChr1 = GameServer()->GetPlayerChar(ClientID1);
	if (!(HasJoined(Fight, 0) && HasJoined(Fight, 1) && FightStarted(ClientID0) && FightStarted(ClientID1) && pChr0 && pChr1
		&& pChr0->m_SpawnTick >= pFight->m_StartTick && pChr1->m_SpawnTick >= pFight->m_StartTick))
		return true;

	if (CollectBet(Fight))
		return true;

	for (int i = 0; i < 2; i++)
	{
		int ClientID = pFight->m_aParticipants[i].m_ClientID;
		if (ClientID >= 0 && GameServer()->m_apPlayers[ClientID])
			GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("The 1vs1 bet couldn't be collected and the fight was cancelled"));
	}
	return false;
}

bool CArenas::CanStartBetFight(int Fight, bool SendMessages)
{
	if (Fight < 0 || m_aFights[Fight].m_BetAmount <= 0)
		return true;

	int64 BetAmount = m_aFights[Fight].m_BetAmount;
	int aID[2] = {m_aFights[Fight].m_aParticipants[0].m_ClientID, m_aFights[Fight].m_aParticipants[1].m_ClientID};
	for (int i = 0; i < 2; i++)
	{
		int ClientID = aID[i];
		int OtherID = aID[i == 0 ? 1 : 0];
		CPlayer *pPlayer = ClientID >= 0 ? GameServer()->m_apPlayers[ClientID] : 0;
		CPlayer *pOther = OtherID >= 0 ? GameServer()->m_apPlayers[OtherID] : 0;
		if (!pPlayer)
			return false;

		if (pPlayer->GetAccID() < ACC_START)
		{
			if (SendMessages)
			{
				GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You have to be logged in to join a 1vs1 bet"));
				if (pOther)
				{
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), pOther->Localize("'%s' is not logged in and can't join this 1vs1 bet"), Server()->ClientName(ClientID));
					GameServer()->SendChatTarget(OtherID, aBuf);
				}
			}
			return false;
		}

		if (GameServer()->m_Accounts[pPlayer->GetAccID()].m_Money < BetAmount)
		{
			if (SendMessages)
			{
				GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You don't have enough money on your bank account"));
				if (pOther)
					GameServer()->SendChatTarget(OtherID, pOther->Localize("The other player doesn't have enough money on the bank account for this 1vs1 bet"));
			}
			return false;
		}
	}

	return true;
}

bool CArenas::CollectBet(int Fight)
{
	if (Fight < 0 || m_aFights[Fight].m_BetAmount <= 0 || m_aFights[Fight].m_BetCollected)
		return true;
	if (!CanStartBetFight(Fight, true))
		return false;

	int64 BetAmount = m_aFights[Fight].m_BetAmount;
	int Collected = 0;
	for (int i = 0; i < 2; i++)
	{
		int ClientID = m_aFights[Fight].m_aParticipants[i].m_ClientID;
		int OtherID = m_aFights[Fight].m_aParticipants[i == 0 ? 1 : 0].m_ClientID;
		CPlayer *pPlayer = ClientID >= 0 ? GameServer()->m_apPlayers[ClientID] : 0;
		if (!pPlayer)
			return false;

		char aHistory[128];
		str_format(aHistory, sizeof(aHistory), "1vs1 bet against '%s'", Server()->ClientName(OtherID));
		if (!pPlayer->BankTransaction(-(int)BetAmount, aHistory))
		{
			for (int j = 0; j < Collected; j++)
			{
				int RollbackID = m_aFights[Fight].m_aParticipants[j].m_ClientID;
				CPlayer *pRollback = RollbackID >= 0 ? GameServer()->m_apPlayers[RollbackID] : 0;
				if (pRollback)
					pRollback->BankTransaction((int)BetAmount, "1vs1 bet rollback");
			}
			return false;
		}
		Collected++;
	}

	m_aFights[Fight].m_BetCollected = true;
	for (int i = 0; i < 2; i++)
	{
		int ClientID = m_aFights[Fight].m_aParticipants[i].m_ClientID;
		CPlayer *pPlayer = ClientID >= 0 ? GameServer()->m_apPlayers[ClientID] : 0;
		if (!pPlayer)
			continue;

		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), pPlayer->Localize("Your 1vs1 bet of %lld money got collected from your bank account"), BetAmount);
		GameServer()->SendChatTarget(ClientID, aBuf);
	}
	return true;
}

void CArenas::PayBet(int Fight, int Winner)
{
	if (Fight < 0 || m_aFights[Fight].m_BetAmount <= 0 || !m_aFights[Fight].m_BetCollected)
		return;

	int WinnerID = m_aFights[Fight].m_aParticipants[Winner].m_ClientID;
	int Loser = Winner == 0 ? 1 : 0;
	int LoserID = m_aFights[Fight].m_aParticipants[Loser].m_ClientID;
	CPlayer *pWinner = WinnerID >= 0 ? GameServer()->m_apPlayers[WinnerID] : 0;
	if (!pWinner)
		return;

	int64 BetAmount = m_aFights[Fight].m_BetAmount;
	int64 PotAmount = BetAmount * 2;
	char aHistory[128];
	str_format(aHistory, sizeof(aHistory), "won 1vs1 bet against '%s'", Server()->ClientName(LoserID));
	pWinner->BankTransaction((int)PotAmount, aHistory);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), pWinner->Localize("You won the 1vs1 bet against '%s': +%lld money on your bank account"), Server()->ClientName(LoserID), PotAmount);
	GameServer()->SendChatTarget(WinnerID, aBuf);

	CPlayer *pLoser = LoserID >= 0 ? GameServer()->m_apPlayers[LoserID] : 0;
	if (pLoser)
	{
		str_format(aBuf, sizeof(aBuf), pLoser->Localize("You lost the 1vs1 bet against '%s'. The full pot of %lld money was paid to their bank account"), Server()->ClientName(WinnerID), PotAmount);
		GameServer()->SendChatTarget(LoserID, aBuf);
	}

	m_aFights[Fight].m_BetCollected = false;
	m_aFights[Fight].m_BetAmount = 0;
}
