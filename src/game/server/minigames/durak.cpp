// made by fokkonaut

#include "durak.h"
#include <unordered_set>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include <engine/shared/config.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/server/entities/flyingpoint.h>

const vec2 CCard::ms_CardSizeRadius = vec2(14.f, 16.f);
const vec2 CCard::ms_TableSizeRadius = vec2(4.9f * 32.f, 3.8f * 32.f); // 11*9 blocks around table center tile (all 4 corner tiles can be cut-out)
const vec2 CCard::ms_AttackAreaRadius = vec2(4.f * 32.f, 1.5f * 32.f); // 8x3
const vec2 CCard::ms_AttackAreaCenterOffset = vec2(-32.f, 16.f);

CDurak::CDurak(CGameContext *pGameServer, int Type) : CMinigame(pGameServer, Type)
{
	m_vLastDuraks.clear();
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aLastSeatOccupiedMsg[i] = 0;
		m_aUpdatedPassive[i] = false;
		m_aInDurakGame[i] = false;
		m_aDurakNumReserved[i] = 0;
		m_aSnappedSeatIndex[i] = -1;
	}
	for (int i = 0; i < 5; i++)
	{
		int Inversed = 5 - i - 1;
		float Offset = (Inversed*(Inversed+1)*0.215f);
		m_aStaticCards[DURAK_TEXT_CARDS_STACK_0 + i].m_TableOffset = vec2(3.9f * 32.f + Offset, 32.f - Offset);
	}
	m_aStaticCards[DURAK_TEXT_TRUMP_CARD].m_TableOffset = vec2(4.9f * 32.f, 32.f);
	m_aStaticCards[DURAK_TEXT_KEYBOARD_CONTROL].m_TableOffset = vec2(3.f * 32.f, -2.f * 32.f);
	for (auto &Game : m_vpGames)
		delete Game;
	m_vpGames.clear();
}

CDurak::~CDurak()
{
	for (auto &Game : m_vpGames)
		delete Game;
	m_vpGames.clear();
}

int CDurak::GetGameByNumber(int Number, bool AllowRunning)
{
	if (Number <= 0)
		return -1;
	for (unsigned int g = 0; g < m_vpGames.size(); g++)
		if (m_vpGames[g]->m_Number == Number && (AllowRunning || !m_vpGames[g]->m_Running))
			return g;
	return -1;
}

int CDurak::GetGameByClient(int ClientID)
{
	if (ClientID == -1)
		return -1;
	for (unsigned int g = 0; g < m_vpGames.size(); g++)
		for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
			if (m_vpGames[g]->m_aSeats[i].m_Player.m_ClientID == ClientID)
				return g;
	return -1;
}

CDurakGame *CDurak::GetOrAddGame(int Number)
{
	CDurakGame *pGame = 0;
	int Game = GetGameByNumber(Number);
	if (Game != -1)
	{
		pGame = m_vpGames[Game];
	}
	else
	{
		pGame = new CDurakGame(Number);
		m_vpGames.push_back(pGame);
	}
	return pGame;
}

void CDurak::AddMapTableTile(int Number, vec2 Pos)
{
	if (Number <= 0)
		return;
	CDurakGame *pGame = GetOrAddGame(Number);
	if (pGame)
	{
		pGame->m_TablePos = Pos;
		GameServer()->m_aMinigameDisabled[MINIGAME_DURAK] = false;
	}
}

void CDurak::AddMapSeatTile(int Number, int MapIndex, int SeatIndex)
{
	if (Number <= 0 || SeatIndex < 0 || SeatIndex >= MAX_DURAK_PLAYERS)
		return;
	CDurakGame *pGame = GetOrAddGame(Number);
	if (pGame)
	{
		pGame->m_aSeats[SeatIndex].m_MapIndex = MapIndex;
	}
}

void CDurak::UpdatePassive(int ClientID, int Seconds)
{
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	if (pChr && pChr->UpdatePassiveEndTick(Server()->Tick() + Server()->TickSpeed() * Seconds))
		m_aUpdatedPassive[ClientID] = true;
}

void CDurak::CreateFlyingPoint(int FromClientID, int Game, CCard *pToCard)
{
	if (!GameServer()->GetPlayerChar(FromClientID))
		return;
	vec2 From = GameServer()->GetPlayerChar(FromClientID)->GetPos();
	vec2 To = m_vpGames[Game]->m_TablePos + pToCard->m_TableOffset;
	To.y -= DURAK_CARD_NAME_OFFSET;
	new CFlyingPoint(&GameServer()->m_World, From, -1, FromClientID, normalize(To - From) * 15.f, To);
}

void CDurak::OnCharacterSpawn(CCharacter *pChr)
{
	int ClientID = pChr->GetPlayer()->GetCID();
	if (!InDurakGame(ClientID))
		return;

	int Game = GetGameByClient(ClientID);
	CDurakGame *pGame = m_vpGames[Game];
	CDurakGame::SSeat *pSeat = pGame->GetSeatByClient(ClientID);

	// Player can't move while playing and grenade, hammer, shotgun doesn't affect them, so we dont have to forcefully set the position in Tick anymore
	pChr->ForceSetPos(GameServer()->Collision()->GetPos(m_vpGames[Game]->GetSeatByClient(ClientID)->m_MapIndex));
	// Update velocity, so we are always perfectly on the seat.
	pChr->SetCoreVel(vec2(0.f, 0.f));
	pChr->Core()->m_ActivelyPlayingDurak = ActivelyPlaying(ClientID);
	// Disable passive also when won already, on next round
	pChr->EpicCircle(pGame->m_DefenderIndex == pSeat->m_ID, -1, true);

	CLockedTune TuneGravity("gravity", 0.3f);
	GameServer()->SetLockedTune(&pChr->m_LockedTunings, TuneGravity);
	CLockedTune TuneHookLength("hook_length", 540.f);
	GameServer()->SetLockedTune(&pChr->m_LockedTunings, TuneHookLength);
	pChr->ApplyLockedTunings();
}

void CDurak::OnCharacterSeat(int ClientID, int Number, int SeatIndex)
{
	if (InDurakGame(ClientID) || GameServer()->m_aMinigameDisabled[MINIGAME_DURAK])
		return;

	int Game = GetGameByNumber(Number, false);
	if (Game < 0)
		return;

	char aBuf[128];
	CDurakGame *pGame = m_vpGames[Game];
	CDurakGame::SSeat::SPlayer *pPlayer = &pGame->m_aSeats[SeatIndex].m_Player;
	if (pPlayer->m_ClientID != -1)
	{
		if (pPlayer->m_ClientID != ClientID && (!m_aLastSeatOccupiedMsg[ClientID] || m_aLastSeatOccupiedMsg[ClientID] + Server()->TickSpeed() * 5 < Server()->Tick()))
		{
			str_format(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[ClientID]->Localize("Welcome to the Durák table, %s! This seat is taken already, please try another one."), Server()->ClientName(ClientID));
			GameServer()->SendChatTarget(ClientID, aBuf);
			m_aLastSeatOccupiedMsg[ClientID] = Server()->Tick();
		}
		return;
	}

	// occupy seat
	pPlayer->m_ClientID = ClientID;
	UpdatePassive(ClientID, 5);

	if (pGame->m_Stake == -1)
	{
		str_format(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[ClientID]->Localize("Welcome to Durák, %s! You are first, please enter a stake in the chat."), Server()->ClientName(ClientID));
	}
	else if (pGame->NumParticipants() == 1)
	{
		str_format(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[ClientID]->Localize("Welcome to Durák, %s! You are second, please enter the current stake of '%lld' or propose a new one."), Server()->ClientName(ClientID), pGame->m_Stake);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[ClientID]->Localize("Welcome to Durák, %s! In order to participate, please enter the current stake of '%lld' in the chat."), Server()->ClientName(ClientID), pGame->m_Stake);
	}
	GameServer()->SendChatTarget(ClientID, aBuf);
	GameServer()->m_pController->UpdateGameInfo(ClientID);
}

bool CDurak::TryEnterBetStake(int ClientID, const char *pMessage)
{
	int Game = GetGameByClient(ClientID);
	if (Game < 0)
		return false;
	CDurakGame *pGame = m_vpGames[Game];
	if (pGame->m_Running)
		return false;
	if (str_is_number(pMessage) != 0)
		return false;
	int64 Stake = atoll(pMessage);
	if (Stake <= 0 && pMessage[0] != '0')
		return false;

	#define VALIDATE_WALLET() do { \
			if (GameServer()->m_apPlayers[ClientID]->GetUsableMoney() < Stake) \
			{ \
				GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("You don't have enough money")); \
				return true; \
			} \
		} while(0)

	char aBuf[128];
	CDurakGame::SSeat::SPlayer *pPlayer = &pGame->GetSeatByClient(ClientID)->m_Player;

	if (pGame->m_Stake == -1 && pPlayer->m_Stake == -1)
	{
		VALIDATE_WALLET();

		pGame->m_Stake = Stake;
		pPlayer->m_Stake = Stake;
		UpdatePassive(ClientID, 30);

		str_format(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[ClientID]->Localize("Successfully set the stake for the current round to '%lld'."), Stake);
		GameServer()->SendChatTarget(ClientID, aBuf);
		return true;
	}
	else if (pGame->m_Stake != pPlayer->m_Stake)
	{
		if (pGame->m_Stake == Stake)
		{
			VALIDATE_WALLET();

			pPlayer->m_Stake = Stake;
			UpdatePassive(ClientID, 30);

			GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("Successfully deployed your stake."));

			int NumPlayers = pGame->NumParticipants();
			if (NumPlayers == MAX_DURAK_PLAYERS)
			{
				StartGame(Game);
			}
			else if (NumPlayers >= MIN_DURAK_PLAYERS)
			{
				pGame->m_GameStartTick = Server()->Tick() + Server()->TickSpeed() * 15;
			}
			return true;
		}
		else if (pGame->NumParticipants() <= 1)
		{
			VALIDATE_WALLET();

			ProposeNewStake(Game, ClientID, Localizable("'%s' proposed a new stake, please enter the current stake of '%lld' or propose a new one in the chat."), Server()->ClientName(ClientID), Stake);
			pGame->m_Stake = Stake;
			pPlayer->m_Stake = Stake;
			UpdatePassive(ClientID, 30);

			str_format(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[ClientID]->Localize("Successfully updated the stake for the current round to '%lld'."), Stake);
			GameServer()->SendChatTarget(ClientID, aBuf);
			return true;
		}
	}

	#undef VALIDATE_WALLET
	return false;
}

void CDurak::OnPlayerLeave(int ClientID, bool Disconnect, bool Shutdown)
{
	CGameTeams *pTeams = &((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams;
	for (unsigned int g = 0; g < m_vpGames.size(); g++)
	{
		for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
		{
			if (m_vpGames[g]->m_aSeats[i].m_Player.m_ClientID != ClientID)
			{
				continue;
			}

			int64 PlayerStake = m_vpGames[g]->m_aSeats[i].m_Player.m_Stake;
			if (m_vpGames[g]->m_Running && PlayerStake >= 0)
			{
				if (Shutdown)
				{
					HandleMoneyTransaction(ClientID, PlayerStake, "Durák stake return");
				}
				else
				{
					// Add stake for the winner, or simply the next winner. if everybody leaves, last person gets the win
					m_vpGames[g]->m_LeftPlayersStake += PlayerStake;
				}
			}
			m_vpGames[g]->m_aSeats[i].m_Player.Reset();
			// Don't let others wait, even though we resetted.
			// We don't have to care about the player values anymore because a new CDurakGame is created and this one will get deleted.
			m_vpGames[g]->m_aSeats[i].m_Player.m_EndedMove = true;

			CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
			if (!GameServer()->Collision()->TileUsed(TILE_DURAK_LOBBY) && !Shutdown) // don't kill player on shutdown, we need character for SaveCharacter()
			{
				GameServer()->SetMinigame(ClientID, MINIGAME_NONE, false, false);
			}
			else if (pPlayer->GetCharacter())
			{
				pPlayer->GetCharacter()->EpicCircle(false, -1, true);
			}

			// Other's can click end move, but why wait for nothing
			if (m_vpGames[g]->m_DefenderIndex == i)
			{
				SetNextMoveSoon(g);
			}

			// Set before tunings and team leaving
			m_aInDurakGame[ClientID] = false;
			pTeams->SetForceCharacterTeam(ClientID, 0);
			GameServer()->SendTuningParams(ClientID, pPlayer->GetCharacter() ? pPlayer->GetCharacter()->m_TuneZone : 0);
			
			pPlayer->m_ForceSpawnPos = vec2(-1, -1);
			pPlayer->m_ShowName = true;
			pPlayer->SetName(Server()->ClientName(ClientID));
			pPlayer->SetClan(Server()->ClientClan(ClientID));
			pPlayer->UpdateInformation();
			break;
		}
	}

	if (Disconnect)
	{
		m_aDurakNumReserved[ClientID] = 0;
		m_aCardUpdate[ClientID].clear();
	}
}

bool CDurak::IsPlayerOnSeat(int ClientID)
{
	int Game = GetGameByClient(ClientID);
	if (Game < 0)
		return false;
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	return pChr && GameServer()->Collision()->GetMapIndex(pChr->GetPos()) == m_vpGames[Game]->GetSeatByClient(ClientID)->m_MapIndex;
}

bool CDurak::OnSetSpectator(int ClientID, int SpectatorID)
{
	if (m_vpGames.size() && SpectatorID == m_aLastSnapID[ClientID][&m_aStaticCards[DURAK_TEXT_TOOLTIP]])
	{
		GameServer()->m_apPlayers[ClientID]->SetViewPos(m_vpGames[0]->m_TablePos);
		return true;
	}
	return false;
}

int CDurak::GetTeam(int ClientID, int MapID)
{
	int ProcessedID = ClientID;
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if ((pPlayer->GetTeam() == TEAM_SPECTATORS || pPlayer->IsPaused()) && pPlayer->GetSpectatorID() >= 0)
		ProcessedID = pPlayer->GetSpectatorID();

	if (!InDurakGame(ProcessedID))
		return -1;

	int Team = GameServer()->GetDDRaceTeam(ProcessedID);
	if (Team == TEAM_SUPER && GameServer()->GetPlayerChar(ProcessedID))
		Team = GameServer()->GetPlayerChar(ProcessedID)->m_TeamBeforeSuper;

	int HighestDurakID = GameServer()->m_World.GetFirstDurakID(ClientID);
	if (MapID > HighestDurakID - m_aDurakNumReserved[ClientID] && MapID <= HighestDurakID)
	{
		// Card team
		if (MapID == m_aLastSnapID[ClientID][&m_aStaticCards[DURAK_TEXT_KEYBOARD_CONTROL]] || MapID == m_aLastSnapID[ClientID][&m_aStaticCards[DURAK_TEXT_TOOLTIP]])
			return 0;
		if (GameServer()->Config()->m_SvDurakTeamColor)
			return Team;
		return 0;
	}

	// Normal team for player
	if (GameServer()->Config()->m_SvDurakTeamColor)
		return -1;

	int ID = MapID;
	if (Server()->ReverseTranslate(ID, ClientID))
	{
		// Player or self
		if (ID == ProcessedID || GameServer()->GetDDRaceTeam(ID) == Team)
			return 0;
	}
	return -1;
}

bool CDurak::OnDropMoney(int ClientID, int Amount, bool OnDeath)
{
	// Disallow accidental money dropping in durak game
	// When m_vpGames[Game]->m_Running is true, player is sent to minigame. so we dont have to check for running && ondeath anymore since its included
	if (OnDeath && GameServer()->m_apPlayers[ClientID]->m_Minigame == MINIGAME_DURAK)
		return true;

	int Game = GetGameByClient(ClientID);
	// If the game is running already, the money has been subtracted already
	if (Game < 0 || m_vpGames[Game]->m_Running)
		return false;
	bool CanDrop = GameServer()->m_apPlayers[ClientID]->GetUsableMoney() - Amount >= m_vpGames[Game]->GetSeatByClient(ClientID)->m_Player.m_Stake;
	if (!CanDrop)
	{
		GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("You can't currently drop that much money due to your stake"));
		return true;
	}
	return false;
}

bool CDurak::OnRainbowName(int ClientID, int MapID)
{
	int TableMapID = GameServer()->m_World.GetFirstDurakID(ClientID);
	if (MapID != TableMapID || GameServer()->m_aMinigameDisabled[MINIGAME_DURAK])
		return false;
	return m_aSnappedSeatIndex[ClientID] == -1 && !::NetworkClipped(GameServer(), ClientID, m_vpGames[0]->m_TablePos);
}

void CDurak::OnInput(CCharacter *pChr, CNetObj_PlayerInput *pNewInput)
{
	int ClientID = pChr->GetPlayer()->GetCID();
	int Game = GetGameByClient(ClientID);
	if (Game < 0)
		return;

	CDurakGame *pGame = m_vpGames[Game];
	CDurakGame::SSeat *pSeat = pGame->GetSeatByClient(ClientID);
	CCard *pCard = 0;
	if (pSeat->m_Player.m_HoveredCard != -1)
	{
		pCard = &pSeat->m_Player.m_vHandCards[pSeat->m_Player.m_HoveredCard];
	}

	int Direction = pSeat->m_Player.m_LastInput.m_Direction == 0 ? pNewInput->m_Direction : 0;
	bool HookColl = !pSeat->m_Player.m_LastInput.m_HookColl ? (pChr->GetPlayer()->m_PlayerFlags&PLAYERFLAG_AIM) : false;
	bool Jump = !pSeat->m_Player.m_LastInput.m_Jump ? pNewInput->m_Jump : false;

	pSeat->m_Player.m_LastKeyboardControl = pSeat->m_Player.m_KeyboardControl;
	if ((Direction || HookColl || Jump) && !pSeat->m_Player.m_KeyboardControl)
	{
		pSeat->m_Player.m_KeyboardControl = true;
		// Dont switch back to mouse control for 1/3 sec
		pSeat->m_Player.m_LastCursorMove = Server()->Tick() + Server()->TickSpeed() / 3;
	}

	if (Direction)
	{
		if (pSeat->m_Player.m_Tooltip == CCard::TOOLTIP_NONE || pSeat->m_Player.IsTurnTooltip())
		{
			if (pSeat->m_Player.m_vHandCards.size())
			{
				if (pSeat->m_Player.m_HoveredCard == -1)
				{
					pSeat->m_Player.m_HoveredCard = 0;
				}
				else if (pCard)
				{
					pCard->SetHovered(false);
					pSeat->m_Player.m_HoveredCard = mod(pSeat->m_Player.m_HoveredCard + Direction, pSeat->m_Player.m_vHandCards.size());
				}
				// Update pCard
				pCard = &pSeat->m_Player.m_vHandCards[pSeat->m_Player.m_HoveredCard];
				pCard->SetHovered(true);
			}
		}
		else if (pSeat->m_Player.m_Tooltip == CCard::TOOLTIP_SELECT_ATTACK)
		{
			std::vector<int> vOpenAttackIndices = pGame->GetOpenAttacks();
			int NumOpenAttacks = vOpenAttackIndices.size();;
			if (NumOpenAttacks > 0)
			{
				int CurIndex = -1;
				for (unsigned int i = 0; i < vOpenAttackIndices.size(); i++)
				{
					if (pSeat->m_Player.m_SelectedAttack == vOpenAttackIndices[i])
					{
						CurIndex = i;
						break;
					}
				}
				int Index = mod(CurIndex + Direction, NumOpenAttacks);
				if (pSeat->m_Player.m_SelectedAttack != -1)
					pGame->m_Attacks[pSeat->m_Player.m_SelectedAttack].m_Offense.SetHovered(false);
				pSeat->m_Player.m_SelectedAttack = vOpenAttackIndices[Index];
				pGame->m_Attacks[pSeat->m_Player.m_SelectedAttack].m_Offense.SetHovered(true);
			}
			else
			{
				pSeat->m_Player.m_SelectedAttack = -1;
			}
		}
		else
		{
			int PlayerState = GetPlayerState(ClientID);
			if (PlayerState == DURAK_PLAYERSTATE_ATTACK)
			{
				if (pSeat->m_Player.m_Tooltip == CCard::TOOLTIP_ATTACK)
					pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_END_MOVE;
				else
					pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_ATTACK;
			}
			else if (PlayerState == DURAK_PLAYERSTATE_DEFEND)
			{
				int aOptions[3] = { CCard::TOOLTIP_DEFEND, CCard::TOOLTIP_PASS, CCard::TOOLTIP_TAKE_CARDS };
				int Current = -1;
				for (int i = 0; i < 3; i++)
				{
					if (pSeat->m_Player.m_Tooltip == aOptions[i])
					{
						Current = i;
						pSeat->m_Player.m_Tooltip = aOptions[mod(Current + Direction, 3)];
						break;
					}
				}
				if (Current == -1)
				{
					pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_DEFEND;
				}
			}
		}
	}
	else if (HookColl)
	{
		if (pSeat->m_Player.m_Tooltip == CCard::TOOLTIP_SELECT_ATTACK)
		{
			pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_DEFEND;
			if (pSeat->m_Player.m_SelectedAttack != -1)
				pGame->m_Attacks[pSeat->m_Player.m_SelectedAttack].m_Offense.SetHovered(false);
			pSeat->m_Player.m_SelectedAttack = -1;
		}
		else
		{
			pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_NONE;
			pSeat->m_Player.m_CanSetNextMove = true;
		}
	}
	else if (Jump)
	{
		if (pSeat->m_Player.m_Tooltip == CCard::TOOLTIP_NONE)
		{
			// Hovered / Selected card
			if (pCard && !pSeat->m_Player.m_EndedMove)
			{
				int PlayerState = GetPlayerState(ClientID);
				if (PlayerState == DURAK_PLAYERSTATE_ATTACK)
					pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_ATTACK;
				else if (PlayerState == DURAK_PLAYERSTATE_DEFEND)
					pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_DEFEND;
			}
		}
		else
		{
			if (pSeat->m_Player.m_Tooltip == CCard::TOOLTIP_ATTACK)
			{
				if (TryAttack(Game, pSeat->m_ID, pCard))
				{
					pCard = 0;
				}
			}
			else if (pSeat->m_Player.m_Tooltip == CCard::TOOLTIP_DEFEND)
			{
				std::vector<int> vOpenAttacks = pGame->GetOpenAttacks();
				if (vOpenAttacks.size())
				{
					pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_SELECT_ATTACK;
					pSeat->m_Player.m_SelectedAttack = vOpenAttacks[0];
					pGame->m_Attacks[pSeat->m_Player.m_SelectedAttack].m_Offense.SetHovered(true);
				}
			}
			else if (pSeat->m_Player.m_Tooltip == CCard::TOOLTIP_SELECT_ATTACK)
			{
				if (TryDefend(Game, pSeat->m_ID, pSeat->m_Player.m_SelectedAttack, pCard))
				{
					pGame->m_Attacks[pSeat->m_Player.m_SelectedAttack].m_Offense.SetHovered(false);
					pSeat->m_Player.m_SelectedAttack = -1;
					pCard = 0;
				}
			}
			else if (pSeat->m_Player.m_Tooltip == CCard::TOOLTIP_PASS)
			{
				if (TryPass(Game, pSeat->m_ID, pCard))
				{
					pCard = 0;
				}
			}
			else if (pSeat->m_Player.m_Tooltip == CCard::TOOLTIP_TAKE_CARDS)
			{
				TakeCardsFromTable(Game);
				pSeat->m_Player.m_CanSetNextMove = true;
			}
			else if (pSeat->m_Player.m_Tooltip == CCard::TOOLTIP_END_MOVE)
			{
				EndMove(Game, pSeat);
			}
			else if (pSeat->m_Player.m_Tooltip == CCard::TOOLTIP_NEXT_MOVE)
			{
				pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_NONE;
				pSeat->m_Player.m_CanSetNextMove = false;
			}
			else if (pSeat->m_Player.m_Tooltip == CCard::TOOLTIP_ATTACKERS_TURN || pSeat->m_Player.m_Tooltip == CCard::TOOLTIP_DEFENDER_PASSED)
			{
				pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_NONE;
				pSeat->m_Player.m_CanSetNextMove = true;
			}
		}
	}
	else if (abs(pNewInput->m_TargetX - pSeat->m_Player.m_LastInput.m_TargetX) > 3.f || abs(pNewInput->m_TargetY - pSeat->m_Player.m_LastInput.m_TargetY) > 3.f)
	{
		if (pSeat->m_Player.m_KeyboardControl)
		{
			pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_NONE;
		}

		if (pSeat->m_Player.m_LastCursorMove < Server()->Tick())
		{
			pSeat->m_Player.m_LastCursorMove = Server()->Tick();
			pSeat->m_Player.m_KeyboardControl = false;
			if (pSeat->m_Player.m_SelectedAttack != -1)
				pGame->m_Attacks[pSeat->m_Player.m_SelectedAttack].m_Offense.SetHovered(false);
			pSeat->m_Player.m_SelectedAttack = -1;
		}
	}

	pSeat->m_Player.m_LastInput.m_Direction = pNewInput->m_Direction;
	pSeat->m_Player.m_LastInput.m_Jump = pNewInput->m_Jump;
	pSeat->m_Player.m_LastInput.m_HookColl = (pChr->GetPlayer()->m_PlayerFlags&PLAYERFLAG_AIM);
	pSeat->m_Player.m_LastInput.m_TargetX = pNewInput->m_TargetX;
	pSeat->m_Player.m_LastInput.m_TargetY = pNewInput->m_TargetY;

	if (pSeat->m_Player.m_KeyboardControl != pSeat->m_Player.m_LastKeyboardControl && pGame->m_Running)
	{
		m_aCardUpdate[ClientID][&m_aStaticCards[DURAK_TEXT_KEYBOARD_CONTROL]] = true;
	}
}

template<typename... Args>
void CDurak::ProposeNewStake(int Game, int NotThisID, const char *pFormat, Args&&... args)
{
	if (Game < 0 || Game >= (int)m_vpGames.size())
		return;
	CDurakGame *pGame = m_vpGames[Game];
	for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
	{
		int ClientID = pGame->m_aSeats[i].m_Player.m_ClientID;
		if (ClientID != -1 && pGame->m_aSeats[i].m_Player.m_Stake != -1 && (NotThisID == -1 || ClientID != NotThisID))
		{
			// Reset stake
			pGame->m_aSeats[i].m_Player.m_Stake = -1;
			char aBuf[256];
			CFormatArg aArgs[] = { CFormatArg(std::forward<Args>(args))... };
			str_format_args(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[ClientID]->Localize(pFormat), aArgs, std::size(aArgs));
			GameServer()->SendChatTarget(ClientID, aBuf);
		}
	}
}

template<typename... Args>
void CDurak::SendChatToParticipants(int Game, const char *pFormat, Args&&... args)
{
	if (Game < 0 || Game >= (int)m_vpGames.size())
		return;
	CDurakGame *pGame = m_vpGames[Game];
	for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
	{
		int ClientID = pGame->m_aSeats[i].m_Player.m_ClientID;
		if (ClientID == -1)
			continue;

		// Stake can change as soon as players leave, so no money is removed. That means when the game is running, a player is automatically participant
		if (pGame->m_Running || (pGame->m_Stake != -1 && pGame->m_aSeats[i].m_Player.m_Stake == pGame->m_Stake))
		{
			char aBuf[256];
			if constexpr (sizeof...(args) > 0)
			{
				CFormatArg aArgs[] = { CFormatArg(std::forward<Args>(args))... };
				str_format_args(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[ClientID]->Localize(pFormat), aArgs, std::size(aArgs));
			}
			else
			{
				str_copy(aBuf, GameServer()->m_apPlayers[ClientID]->Localize(pFormat), sizeof(aBuf));
			}
			GameServer()->SendChatTarget(ClientID, aBuf);
		}
	}
}

bool CDurak::StartGame(int Game)
{
	if (Game < 0 || Game >= (int)m_vpGames.size())
		return false;
	CDurakGame *pGame = m_vpGames[Game];
	if (pGame->m_Running)
		return false;

	CGameTeams *pTeams = &((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams;
	int FirstFreeTeam = -1;
	for (int i = 1; i < MAX_CLIENTS; i++)
	{
		if (pTeams->Count(i) == 0)
		{
			FirstFreeTeam = i;
			break;
		}
	}

	// New game for other players at this table, since we move to a new team or this gets deleted if no team was free
	m_vpGames.push_back(new CDurakGame(pGame));
	if (FirstFreeTeam == -1)
	{
		SendChatToParticipants(Game, Localizable("Couldn't start game, no empty team found"));
		delete m_vpGames[Game];
		m_vpGames.erase(m_vpGames.begin() + Game);
		return false;
	}
	
	pGame->m_GameStartTick = Server()->Tick();
	pGame->m_Running = true;
	pGame->m_Deck.Shuffle();

	int LastDurakIndex = -1;
	std::vector<std::pair<int, int64> >::iterator DurakIt = m_vLastDuraks.end();
	char aBuf[128];
	for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
	{
		CDurakGame::SSeat *pSeat = &pGame->m_aSeats[i];
		int ClientID = pSeat->m_Player.m_ClientID;
		if (ClientID == -1)
			continue;

		if (pSeat->m_Player.m_Stake != pGame->m_Stake)
		{
			GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("You didn't deploy your stake in time, game started without you"));
			pSeat->m_Player.Reset();
			continue;
		}

		CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
		if (pGame->m_Stake >= 0)
		{
			if (HandleMoneyTransaction(ClientID, -pGame->m_Stake, "Durák stake"))
			{
				str_format(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[ClientID]->Localize("Your stake got collected: -%lld money."), pGame->m_Stake);
				GameServer()->SendChatTarget(ClientID, aBuf);
			}
		}

		GameServer()->SetMinigame(ClientID, MINIGAME_DURAK, true, false);
		pPlayer->m_ForceSpawnPos = GameServer()->Collision()->GetPos(pSeat->m_MapIndex);
		m_aInDurakGame[ClientID] = true;
		pTeams->SetForceCharacterTeam(ClientID, FirstFreeTeam);

		// Check for previous durak to attack in first round
		auto it = std::find_if(m_vLastDuraks.begin(), m_vLastDuraks.end(),
			[ClientID](const std::pair<int, int64_t>& p) {
				return p.first == ClientID;
			});
		if (it != m_vLastDuraks.end())
		{
			if (DurakIt == m_vLastDuraks.end() || it->second > DurakIt->second)
			{
				DurakIt = it;
				LastDurakIndex = i;
			}
		}
	}

	// Attack previous durak in new round
	if (LastDurakIndex != -1)
	{
		// Get previous player of LastDurakIndex, so that LastDurakIndex ends up getting attacked.
		pGame->m_InitialAttackerIndex = pGame->GetNextPlayer(LastDurakIndex, false, true);
		m_vLastDuraks.erase(DurakIt);
	}
	pGame->DealHandCards();
	StartNextRound(Game);
	return true;
}

void CDurak::EndGame(int Game)
{
	if (Game < 0 || Game >= (int)m_vpGames.size())
		return;
	CDurakGame* pGame = m_vpGames[Game];
	if (!pGame->m_Running)
		return;

	for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
	{
		int ClientID = pGame->m_aSeats[i].m_Player.m_ClientID;
		if (ClientID == -1)
			continue;
		OnPlayerLeave(ClientID);
	}

	delete m_vpGames[Game];
	m_vpGames.erase(m_vpGames.begin() + Game);
}

bool CDurak::HandleMoneyTransaction(int ClientID, int Amount, const char *pMsg)
{
	CPlayer *pPlayer = ClientID >= 0 ? GameServer()->m_apPlayers[ClientID] : 0;
	if (pPlayer && pPlayer->WalletTransaction(Amount, pMsg))
	{
		int AccID = pPlayer->GetAccID();
		if (AccID >= ACC_START)
		{
			CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[AccID];
			pAccount->m_DurakProfit += Amount;
		}
		return true;
	}
	return false;
}

const char *CDurak::GetCardSymbol(int Suit, int Rank, CDurakGame *pGame, int SnappingClient)
{
	if (Suit == -1 && Rank == -1)
	{
		static const char *pBackCard = "🂠";
		return pBackCard;
	}
	if (!(Suit < 0 || Suit > 3 || Rank < 6 || Rank > 14))
	{
		static const char *aapCards[4][9] = {
			{"🂦", "🂧", "🂨", "🂩", "🂪", "🂫", "🂭", "🂮", "🂡"}, // Spades
			{"🂶", "🂷", "🂸", "🂹", "🂺", "🂻", "🂽", "🂾", "🂱"}, // Hearts
			{"🃆", "🃇", "🃈", "🃉", "🃊", "🃋", "🃍", "🃎", "🃁"}, // Diamonds
			{"🃖", "🃗", "🃘", "🃙", "🃚", "🃛", "🃝", "🃞", "🃑"}  // Clubs
		};
		return aapCards[Suit][Rank - 6];
	}
	static char aBuf[MAX_NAME_LENGTH];
	#define Localize(str, context) SnappingClient == -1 ? Localizable((str), (context)) : GameServer()->m_apPlayers[SnappingClient]->Localize((str), (context))
	switch (Suit)
	{
		case CCard::IND_EMPTY: return " ";
		case CCard::IND_DURAK_TABLE_LABEL: return "🂭   Durák";
		case CCard::IND_KEYBOARD_ON: return "⌨☒";
		case CCard::IND_KEYBOARD_OFF: return "⌨☐";
		case CCard::IND_END_MOVE_BUTTON: return "[✓]";
		case CCard::IND_START_TIMER:
		{
			if (pGame->NumParticipants() < MIN_DURAK_PLAYERS) return Localize("Waiting…", "durak-name");
			str_format(aBuf, sizeof(aBuf), "Start in %ds…", (int)(pGame->m_GameStartTick - Server()->Tick()) / Server()->TickSpeed() + 1);
			return aBuf;
		}
		case CCard::IND_PLAYERCOUNTER:
		{
			str_format(aBuf, sizeof(aBuf), Localize("Players - %d/%d", "durak-name"), pGame->NumParticipants(), (int)MAX_DURAK_PLAYERS);
			return aBuf;
		}
		case CCard::IND_STAKE:
		{
			if (pGame->m_Stake == -1) return "> ??? <";
			str_format(aBuf, sizeof(aBuf), "> %lld$ <", pGame->m_Stake);
			return aBuf;
		}
		case CCard::IND_TOOLTIP_SELECT_ATTACK: return Localize("Select attack", "durak-name");
		case CCard::IND_TOOLTIP_ATTACK: return Localize("Attack!", "durak-name");
		case CCard::IND_TOOLTIP_DEFEND: return Localize("Defend", "durak-name");
		case CCard::IND_TOOLTIP_PASS: return Localize("Pass!", "durak-name");
		case CCard::IND_TOOLTIP_END_MOVE: return Localize("End move", "durak-name");
		case CCard::IND_TOOLTIP_TAKE_CARDS: return Localize("Take cards", "durak-name");
		case CCard::IND_TOOLTIP_ATTACKERS_TURN:
		{
			int ClientID = pGame->m_DurakClientID != -1 ? pGame->m_DurakClientID : pGame->m_aSeats[pGame->m_AttackerIndex].m_Player.m_ClientID;
			str_format(aBuf, sizeof(aBuf), "%s", Server()->ClientName(ClientID));
			return aBuf;
		}
		case CCard::IND_TOOLTIP_DEFENDER_PASSED: return Localize("Passed…", "durak-name");
		case CCard::IND_TOOLTIP_NEXT_MOVE:
		{
			str_format(aBuf, sizeof(aBuf), Localize("Next move: %ds", "durak-name"), (int)(pGame->m_NextMove - Server()->Tick()) / Server()->TickSpeed() + 1);
			return aBuf;
		}
		default: return "??";
	}
	#undef Localize
}

int CDurak::GetPlayerState(int ClientID)
{
	if (!InDurakGame(ClientID))
		return DURAK_PLAYERSTATE_NONE;

	int Game = GetGameByClient(ClientID);
	if (Game < 0)
		return DURAK_PLAYERSTATE_NONE;

	CDurakGame *pGame = m_vpGames[Game];
	if (!pGame->m_Running)
		return DURAK_PLAYERSTATE_NONE;

	return pGame->GetStateBySeat(pGame->GetSeatByClient(ClientID)->m_ID);
}

void CDurak::Tick()
{
	for (int g = (int)m_vpGames.size() - 1; g >= 0; g--)
	{
		UpdateGame(g);
	}
}

void CDurak::UpdateGame(int Game)
{
	auto &&HandleCardHover = [this](int Game, int Seat, int Card, vec2 CursorPos, bool Dragging) {
		CDurakGame::SSeat *pSeat = &m_vpGames[Game]->m_aSeats[Seat];
		CCard *pCard = &pSeat->m_Player.m_vHandCards[Card];
		vec2 RelativeCursorPos = CursorPos - m_vpGames[Game]->m_TablePos;
		// Handle card drag logic and hover logic
		if (Dragging)
		{
			if (pCard->m_HoverState > CCard::HOVERSTATE_NONE)
			{
				if (pCard->m_HoverState == CCard::HOVERSTATE_MOUSEOVER)
				{
					// Unset attacker turn tooltip
					pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_NONE;
					pCard->m_HoverState = CCard::HOVERSTATE_DRAGGING;
				}
				float CornerY = 0.f;
				if ((RelativeCursorPos.x < -CCard::ms_TableSizeRadius.x + 32.f) ||
					RelativeCursorPos.x > CCard::ms_TableSizeRadius.x - 32.f)
				{
					CornerY = 32.f;
				}
				pCard->m_TableOffset.x = clamp(RelativeCursorPos.x, -CCard::ms_TableSizeRadius.x, CCard::ms_TableSizeRadius.x);
				pCard->m_TableOffset.y = clamp(RelativeCursorPos.y + DURAK_CARD_NAME_OFFSET,
					-CCard::ms_TableSizeRadius.y + DURAK_CARD_NAME_OFFSET + 4.f + CornerY,
					CCard::ms_TableSizeRadius.y - CornerY);
			}
		}
		else if (pCard->m_HoverState == CCard::HOVERSTATE_DRAGGING)
		{
			pCard->SetHovered(false);
			pSeat->m_Player.m_HoveredCard = -1;
		}
		else
		{
			bool MouseOver = (pSeat->m_Player.m_HoveredCard == -1 || pSeat->m_Player.m_HoveredCard == Card) && pCard->MouseOver(RelativeCursorPos);
			if (MouseOver && pCard->m_HoverState == CCard::HOVERSTATE_NONE)
			{
				pCard->SetHovered(true);
				pSeat->m_Player.m_HoveredCard = Card;
			}
			else if (!MouseOver && pCard->m_HoverState == CCard::HOVERSTATE_MOUSEOVER)
			{
				pCard->SetHovered(false);
				pSeat->m_Player.m_HoveredCard = -1;
			}
		}
	};

	// UpdateGame start

	CDurakGame *pGame = m_vpGames[Game];
	int NumParticipants = pGame->NumParticipants();
	if (NumParticipants < MIN_DURAK_PLAYERS)
	{
		if (!pGame->m_Running)
		{
			// Cancel timer, when players left and we're too few now
			if (pGame->m_GameStartTick)
			{
				pGame->m_GameStartTick = 0;
			}
			// Reset stake when all players left the table and no round started
			if (pGame->NumDeployedStakes() == 0)
			{
				pGame->m_Stake = -1;
			}
		}
		// Only process when the game has not ended yet
		else if (!pGame->m_GameOverTick)
		{
			for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
			{
				CDurakGame::SSeat *pSeat = &pGame->m_aSeats[i];
				if (pSeat->m_Player.m_ClientID != -1 && pGame->CanProcessWin(i))
				{
					// Forcefully process win on last player, we do not want any money glitched away.
					// In this case the "Durak", or simply last player, will benefit from other's leaving the round
					// If anyone has won before, the durak/losers stake has been given to the winner already. Again: No money dupe xD
					ProcessPlayerWin(Game, pSeat, -1);
					break;
				}
			}

			EndGame(Game);
			return;
		}
	}

	bool TimerUpGameStart = !pGame->m_Running && pGame->m_GameStartTick && pGame->m_GameStartTick <= Server()->Tick();
	if (TimerUpGameStart && !StartGame(Game))
	{
		// will only trigger when the timer is up and startgame fails, if no team is free
		return;
	}

	for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
	{
		CDurakGame::SSeat *pSeat = &pGame->m_aSeats[i];
		int ClientID = pSeat->m_Player.m_ClientID;
		if (ClientID == -1)
			continue;

		CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
		CCharacter *pChr = pPlayer->GetCharacter();

		if (!pGame->m_Running)
		{
			if (!pChr || GameServer()->Collision()->GetMapIndex(pChr->GetPos()) != pSeat->m_MapIndex)
			{
				if (pChr && m_aUpdatedPassive[ClientID])
				{
					pChr->m_PassiveEndTick = 0;
					pChr->Passive(false, -1, true);
				}
				m_aUpdatedPassive[ClientID] = false;
				GameServer()->m_pController->UpdateGameInfo(ClientID);
				pSeat->m_Player.Reset();
				continue;
			}

			if (pGame->m_GameStartTick > Server()->Tick() && (pGame->m_GameStartTick - Server()->Tick() + 1) % Server()->TickSpeed() == 0)
			{
				m_aCardUpdate[ClientID][&m_aStaticCards[DURAK_TEXT_TOOLTIP]] = true;
			}
			if (pSeat->m_Player.m_LastNumParticipants != NumParticipants)
			{
				m_aCardUpdate[ClientID][&m_aStaticCards[DURAK_TEXT_KEYBOARD_CONTROL]] = true;
				pSeat->m_Player.m_LastNumParticipants = NumParticipants;
			}
			if (pSeat->m_Player.m_LastStake != pGame->m_Stake)
			{
				m_aCardUpdate[ClientID][&m_aStaticCards[DURAK_TEXT_TRUMP_CARD]] = true;
				pSeat->m_Player.m_LastStake = pGame->m_Stake;
			}
			continue;
		}

		// Game logic
		// Process wins
		bool ProcessedWinAlready = !pGame->CanProcessWin(i);
		for (unsigned int w = 0; w < pGame->m_vWinners.size(); w++)
		{
			if (pGame->m_vWinners[w] == i && !ProcessedWinAlready)
			{
				ProcessPlayerWin(Game, pSeat, w);
				break;
			}
		}

		if (!pChr || ProcessedWinAlready)
			continue;

		if (!pSeat->m_Player.m_EndedMove)
		{
			const int NumHands = (int)pSeat->m_Player.m_vHandCards.size();
			if (pSeat->m_Player.m_LastNumHandCards != NumHands)
			{
				char aBuf[MAX_NAME_LENGTH];
				str_format(aBuf, sizeof(aBuf), "%s x%d", GetCardSymbol(-1, -1), NumHands);
				pPlayer->SetName(aBuf);
				pPlayer->SetClan(Server()->ClientName(ClientID));
				pPlayer->UpdateInformation();
				pSeat->m_Player.m_LastNumHandCards = NumHands;
			}
		}

		// Card sorting
		unsigned int NumCards = pSeat->m_Player.m_vHandCards.size();
		if (NumCards <= 0)
			continue;

		// Dynamically sort hand cards
		float Gap = 4.f;
		const float RequiredSpace = min(NumCards * (CCard::ms_CardSizeRadius.x*2 + Gap) - Gap, (CCard::ms_TableSizeRadius.x - 32.f) * 2);
		float PosX = -RequiredSpace / 2.f;
		if (NumCards > 1)
		{
			Gap = RequiredSpace / (NumCards - 1);
		}
		float PushStrength = 0.5f + min((int)NumCards - 16, 10) * 0.1f;
		for (unsigned int c = 0; c < NumCards; c++)
		{
			CCard *pCard = &pSeat->m_Player.m_vHandCards[c];
			float Offset = 0.f;
			if (pSeat->m_Player.m_HoveredCard != -1 && NumCards > 10)
			{
				int Diff = c - pSeat->m_Player.m_HoveredCard;
				if (Diff != 0 && abs(Diff) <= 3)
				{
					float Falloff = 1.f / abs(Diff);
					if (NumCards < 15)
						Falloff *= 0.5f;
					float Multiplier = (float)(Diff) / abs(Diff); // -1 or +1
					Offset = (Gap * PushStrength * Falloff) * Multiplier;

					bool IsLeftMost = (c == 0);
					bool IsRightMost = (c == NumCards - 1);
					if ((IsLeftMost && Offset < 0) || (IsRightMost && Offset > 0))
						Offset = 0.f;
				}
			}
			pCard->m_TableOffset.x = PosX + Offset;
			PosX += Gap;
		}

		// scrolling order is fucked up otherwise when moving mouse from left to right with too many cards
		vec2 CursorPos = pChr->GetCursorPos();
		bool Dragging = pChr->Input()->m_Fire & 1;

		// Drag release
		CCard *pDraggedCard = 0;
		if (pSeat->m_Player.m_HoveredCard != -1 && !pSeat->m_Player.m_KeyboardControl)
			pDraggedCard = &pSeat->m_Player.m_vHandCards[pSeat->m_Player.m_HoveredCard];
		bool DragRelease = !Dragging && pDraggedCard && pDraggedCard->m_HoverState == CCard::HOVERSTATE_DRAGGING;

		if (!pSeat->m_Player.m_KeyboardControl)
		{
			// Card hover
			if (pSeat->m_Player.m_LastCursorX < CursorPos.x)
			{
				for (unsigned int c = 0; c < NumCards; c++)
					HandleCardHover(Game, i, c, CursorPos, Dragging);
			}
			else
			{
				for (int c = (int)NumCards - 1; c >= 0; c--)
					HandleCardHover(Game, i, c, CursorPos, Dragging);
			}
			pSeat->m_Player.m_LastCursorX = CursorPos.x;

		}

		// Keep track of tooltip
		if (pSeat->m_Player.m_Tooltip != pSeat->m_Player.m_LastTooltip)
		{
			pSeat->m_Player.m_LastTooltip = pSeat->m_Player.m_Tooltip;
			m_aCardUpdate[pSeat->m_Player.m_ClientID][&m_aStaticCards[DURAK_TEXT_TOOLTIP]] = true;
			for (int c = 0; c < MAX_CLIENTS; c++)
			{
				CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
				if ((pPlayer->GetTeam() == TEAM_SPECTATORS || pPlayer->IsPaused()) && pPlayer->GetSpectatorID() == pSeat->m_Player.m_ClientID)
					m_aCardUpdate[c][&m_aStaticCards[DURAK_TEXT_TOOLTIP]] = true;
			}
		}

		if (!pSeat->m_Player.m_KeyboardControl && !pSeat->m_Player.m_EndedMove)
		{
			if (!pSeat->m_Player.IsTurnTooltip())
			{
				pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_NONE;
			}

			// Check for tooltip and what kinda move the player wants to do
			vec2 RelativeCursorPos = CursorPos - pGame->m_TablePos;
			int PlayerState = GetPlayerState(pSeat->m_Player.m_ClientID);
			if (CCard::InAttackArea(RelativeCursorPos))
			{
				if (PlayerState == DURAK_PLAYERSTATE_ATTACK)
				{
					if (Dragging && pDraggedCard)
					{
						pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_ATTACK;
					}
					else if (DragRelease && TryAttack(Game, i, pDraggedCard))
					{
						pDraggedCard = 0;
					}
				}
				else if (PlayerState == DURAK_PLAYERSTATE_DEFEND)
				{
					bool TryingDefend = false;
					for (int a = 0; a < MAX_DURAK_ATTACKS; a++)
					{
						CCard *pOffense = &m_vpGames[Game]->m_Attacks[a].m_Offense;
						if (pOffense->Valid() && pOffense->MouseOver(RelativeCursorPos))
						{
							TryingDefend = true;
							if (Dragging && pDraggedCard)
							{
								pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_DEFEND;
							}
							else if (DragRelease && TryDefend(Game, i, a, pDraggedCard))
							{
								pDraggedCard = 0;
							}
							break;
						}
					}

					if (!TryingDefend)
					{
						if (Dragging && pDraggedCard)
						{
							pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_PASS;
						}
						else if (DragRelease && TryPass(Game, i, pDraggedCard))
						{
							pDraggedCard = 0;
						}
						else
						{
							pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_TAKE_CARDS;
							if (Dragging)
							{
								TakeCardsFromTable(Game);
							}
						}
					}
				}
			}
			else if (m_aStaticCards[DURAK_TEXT_CARDS_STACK_0].MouseOver(RelativeCursorPos) || m_aStaticCards[DURAK_TEXT_TRUMP_CARD].MouseOver(RelativeCursorPos))
			{
				if (PlayerState == DURAK_PLAYERSTATE_ATTACK)
				{
					pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_END_MOVE;
					if (Dragging)
					{
						EndMove(Game, pSeat);
					}
				}
			}
		}
	}

	// Handle game logic
	if (!pGame->m_Running)
		return;

	if (pGame->IsGameOver())
	{
		if (pGame->m_GameOverTick)
		{
			if (pGame->m_GameOverTick < Server()->Tick() - Server()->TickSpeed() * 15)
			{
				EndGame(Game);
			}
			return;
		}

		for (unsigned int w = 0; w < pGame->m_vWinners.size(); w++)
		{
			if (pGame->CanProcessWin(pGame->m_vWinners[w]))
			{
				ProcessPlayerWin(Game, &pGame->m_aSeats[pGame->m_vWinners[w]], w);
				break;
			}
		}

		SendChatToParticipants(Game, Localizable("'%s' is the Durák!"), Server()->ClientName(pGame->m_DurakClientID));
		EndMove(Game, pGame->GetSeatByClient(pGame->m_DurakClientID), true);
		pGame->m_GameOverTick = Server()->Tick();
		std::pair<int, int64> Pair(pGame->m_DurakClientID, pGame->m_GameOverTick);
		m_vLastDuraks.push_back(Pair);

		// Abuse tooltip attackers turn for showing durak's name in the end, it's handled there
		SetTurnTooltip(Game, CCard::TOOLTIP_ATTACKERS_TURN);
		return;
	}

	bool ProcessMove = pGame->ProcessNextMove(Server()->Tick());
	bool AttackersEndedMove = false;
	if(pGame->NextMoveSoon(Server()->Tick()))
	{
		SetTurnTooltip(Game, CCard::TOOLTIP_NEXT_MOVE);
	}
	else
	{
		AttackersEndedMove = pGame->m_aSeats[pGame->m_AttackerIndex].m_Player.m_EndedMove && pGame->m_aSeats[pGame->GetNextPlayer(pGame->m_DefenderIndex)].m_Player.m_EndedMove;
		if (AttackersEndedMove && !ProcessMove && pGame->GetOpenAttacks().empty())
		{
			SetNextMoveSoon(Game);
			// Process next tick, and send tooltip next move
			return;
		}
	}

	if (ProcessMove || AttackersEndedMove)
	{
		bool AllAttacksDefended = true;
		bool HasUndefendedAttacks = false;
		bool HasActiveAttacks = false;

		for (int i = 0; i < MAX_DURAK_ATTACKS; i++)
		{
			if (pGame->m_Attacks[i].m_Offense.Valid())
			{
				HasActiveAttacks = true;
				if (!pGame->m_Attacks[i].m_Defense.Valid())
				{
					AllAttacksDefended = false;
					HasUndefendedAttacks = true;
				}
			}
		}

		if (HasActiveAttacks && AllAttacksDefended)
		{
			int DefenderID = pGame->m_aSeats[pGame->m_DefenderIndex].m_Player.m_ClientID;
			SendChatToParticipants(Game, Localizable("'%s' successfully defended"), Server()->ClientName(DefenderID));
			CCharacter *pChr = GameServer()->GetPlayerChar(DefenderID);
			if (pChr)
			{
				GameServer()->CreateFinishConfetti(pChr->GetPos(), pChr->TeamMask());
			}
			StartNextRound(Game, true);
		}
		else if (HasUndefendedAttacks && ProcessMove)
		{
			TakeCardsFromTable(Game);
			
		}
		else if (ProcessMove)
		{
			int AttackerID = pGame->m_aSeats[pGame->m_InitialAttackerIndex].m_Player.m_ClientID;
			SendChatToParticipants(Game, Localizable("'%s' took too long and was skipped"), Server()->ClientName(AttackerID));
			// Just skip this guy // Emulate successful defense, so that the next guy's turn is now and he is not skipped.
			StartNextRound(Game, true);
		}
	}
}

void CDurak::StartNextRound(int Game, bool SuccessfulDefense)
{
	CDurakGame *pGame = m_vpGames[Game];
	pGame->NextRound(SuccessfulDefense);
	for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
	{
		int ClientID = pGame->m_aSeats[i].m_Player.m_ClientID;
		if (ClientID == -1)
			continue;

		CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
		if (pGame->m_aSeats[i].m_Player.m_Stake >= 0)
		{
			SetPlaying(Game, i);
		}
		else if (pChr)
		{
			// Just disable our epic circle, if we won already and was the defender before.
			pChr->EpicCircle(false, -1, true);
		}
	}
	SetTurnTooltip(Game, CCard::TOOLTIP_ATTACKERS_TURN);
}

void CDurak::SetPlaying(int Game, int Seat)
{
	CDurakGame *pGame = m_vpGames[Game];
	int ClientID = pGame->m_aSeats[Seat].m_Player.m_ClientID;
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);

	// Making sure to update handcards for 0.7 here, because we can not catch every case from within CDurakGame where SortHand() gets called for example.
	UpdateHandcards(Game, &pGame->m_aSeats[Seat]);
	pGame->m_aSeats[Seat].m_Player.m_EndedMove = false;
	pGame->m_aSeats[Seat].m_Player.m_CanSetNextMove = true;

	if (ActivelyPlaying(ClientID))
	{
		pGame->m_aSeats[Seat].m_Player.m_LastNumHandCards = -1; // Get our specific name back
		GameServer()->m_apPlayers[ClientID]->m_ShowName = true;
		// important so we dont override our specific name
		GameServer()->m_apPlayers[ClientID]->m_RemovedName = false;
	}
	else
	{
		EndMove(Game, &pGame->m_aSeats[Seat], true);
	}

	// Process spawning and handling new round
	if (pChr)
	{
		OnCharacterSpawn(pChr);
	}
	else // ApplyLockedTunings takes care of sending tunes already
	{
		GameServer()->SendTuningParams(ClientID);
	}
}

void CDurak::UpdateHandcards(int Game, CDurakGame::SSeat *pSeat)
{
	// 0.7.........
	for (unsigned int i = 0; i < pSeat->m_Player.m_vHandCards.size(); i++)
		m_aCardUpdate[pSeat->m_Player.m_ClientID][&pSeat->m_Player.m_vHandCards[i]] = true;
}

void CDurak::EndMove(int Game, CDurakGame::SSeat *pSeat, bool Force)
{
	if (pSeat->m_Player.m_EndedMove || (!m_vpGames[Game]->m_IsDefenseOngoing && !Force))
		return;

	int ClientID = pSeat->m_Player.m_ClientID;
	pSeat->m_Player.m_EndedMove = true;
	pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_NONE;
	GameServer()->m_apPlayers[ClientID]->m_ShowName = false;
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	GameServer()->SendTuningParams(ClientID, pChr ? pChr->m_TuneZone : 0);
	if (pChr)
	{
		pChr->Core()->m_ActivelyPlayingDurak = false;
	}
}

bool CDurak::TryDefend(int Game, int Seat, int Attack, CCard *pCard)
{
	CDurakGame *pGame = m_vpGames[Game];
	if (pGame->TryDefend(Seat, Attack, pCard))
	{
		if (!pGame->m_IsDefenseOngoing)
		{
			// First offense card placed. Now no more passes are allowed, so we know who has to play and who not.
			pGame->m_IsDefenseOngoing = true;
			for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
			{
				int ClientID = pGame->m_aSeats[i].m_Player.m_ClientID;
				if (ClientID != -1 && pGame->GetStateBySeat(i) == DURAK_PLAYERSTATE_NONE)
				{
					EndMove(Game, &pGame->m_aSeats[i]);
				}
			}
		}
		SetTurnTooltip(Game, CCard::TOOLTIP_NONE);
		ProcessCardPlacement(Game, &pGame->m_aSeats[Seat], &m_vpGames[Game]->m_Attacks[Attack].m_Defense);

		if (pGame->GetOpenAttacks().empty())
		{
			// Let others see what card you placed
			SetNextMoveSoon(Game);
		}
		return true;
	}
	return false;
}

bool CDurak::TryPass(int Game, int Seat, CCard *pCard)
{
	CDurakGame *pGame = m_vpGames[Game];
	int Attack = pGame->TryPass(Seat, pCard);
	if (Attack != -1)
	{
		for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
		{
			int ClientID = pGame->m_aSeats[i].m_Player.m_ClientID;
			if (ClientID == -1)
				continue;

			if (pGame->m_aSeats[i].m_Player.m_Stake >= 0)
			{
				// Don't get false results from GetStateBySeat/ActivelyPlaying
				pGame->m_aSeats[i].m_Player.m_EndedMove = false;
				SetPlaying(Game, i);
			}
		}

		SetTurnTooltip(Game, CCard::TOOLTIP_DEFENDER_PASSED);
		ProcessCardPlacement(Game, &pGame->m_aSeats[Seat], &m_vpGames[Game]->m_Attacks[Attack].m_Offense);

		// update epic circle defender indicator on pass, not only nextround
		CCharacter *pOldDefender = GameServer()->GetPlayerChar(pGame->m_aSeats[Seat].m_Player.m_ClientID);
		if (pOldDefender)
			pOldDefender->EpicCircle(false, -1, true);
		return true;
	}
	return false;
}

bool CDurak::TryAttack(int Game, int Seat, CCard *pCard)
{
	CDurakGame *pGame = m_vpGames[Game];
	int Attack = pGame->TryAttack(Seat, pCard, Server()->Tick());
	if (Attack != -1)
	{
		// Last attack, end move of everyone, only defender has to still play
		if (Attack == MAX_DURAK_ATTACKS - 1)
		{
			EndMove(Game, &pGame->m_aSeats[pGame->m_AttackerIndex]);
			EndMove(Game, &pGame->m_aSeats[pGame->GetNextPlayer(pGame->m_DefenderIndex)]);
		}

		SetTurnTooltip(Game, CCard::TOOLTIP_NONE);
		ProcessCardPlacement(Game, &pGame->m_aSeats[Seat], &m_vpGames[Game]->m_Attacks[Attack].m_Offense);
		return true;
	}
	return false;
}

void CDurak::ProcessCardPlacement(int Game, CDurakGame::SSeat *pSeat, CCard *pFlyingPointToCard)
{
	int ClientID = pSeat->m_Player.m_ClientID;
	if (pSeat->m_Player.m_vHandCards.empty())
		EndMove(Game, pSeat, true);
	UpdateHandcards(Game, pSeat);
	CreateFlyingPoint(ClientID, Game, pFlyingPointToCard);
	pSeat->m_Player.m_CanSetNextMove = true;
}

void CDurak::TakeCardsFromTable(int Game)
{
	CDurakGame *pGame = m_vpGames[Game];
	// Can't take cards if all attacks are defended already
	if (pGame->GetOpenAttacks().empty())
		return;

	CDurakGame::SSeat *pSeat = &pGame->m_aSeats[pGame->m_DefenderIndex];
	bool TimerUp = pGame->ProcessNextMove(Server()->Tick());
	if (!TimerUp)
	{
		if (!pSeat->m_Player.m_EndedMove)
		{
			EndMove(Game, pSeat);
			SetNextMoveSoon(Game);
		}
		// Clicking it again will not help and will not speed up the process
		return;
	}

	// Defender takes all cards from table
	for (int i = 0; i < MAX_DURAK_ATTACKS; i++)
	{
		if (pGame->m_Attacks[i].m_Offense.Valid())
		{
			CCard OffenseCard = pGame->m_Attacks[i].m_Offense;
			OffenseCard.m_TableOffset.y = OffenseCard.m_HeightPos = CCard::ms_TableSizeRadius.y;
			pGame->m_aSeats[pGame->m_DefenderIndex].m_Player.m_vHandCards.push_back(OffenseCard);

			if (pGame->m_Attacks[i].m_Defense.Valid())
			{
				CCard DefenseCard = pGame->m_Attacks[i].m_Defense;
				DefenseCard.m_TableOffset.y = DefenseCard.m_HeightPos = CCard::ms_TableSizeRadius.y;
				pGame->m_aSeats[pGame->m_DefenderIndex].m_Player.m_vHandCards.push_back(DefenseCard);
			}

			// Remove cards from table, they got picked up
			pGame->m_Attacks[i].m_Offense.Invalidate();
			pGame->m_Attacks[i].m_Defense.Invalidate();
		}
	}

	int DefenderID = pGame->m_aSeats[pGame->m_DefenderIndex].m_Player.m_ClientID;
	SendChatToParticipants(Game, Localizable("'%s' couldn't defend all attacks and takes all cards"), Server()->ClientName(DefenderID));

	// Sort hand cards after taking
	pGame->SortHand(&pGame->m_aSeats[pGame->m_DefenderIndex], pGame->m_Deck.GetTrumpSuit());
	UpdateHandcards(Game, &pGame->m_aSeats[pGame->m_DefenderIndex]);
	// Next round
	StartNextRound(Game);
}

void CDurak::SetNextMoveSoon(int Game)
{
	const int64 NextMoveTick = Server()->Tick() + Server()->TickSpeed() * 5;
	if (m_vpGames[Game]->m_NextMove > NextMoveTick)
	{
		m_vpGames[Game]->m_NextMove = NextMoveTick;
	}
}

void CDurak::SetTurnTooltip(int Game, int Tooltip)
{
	m_vpGames[Game]->m_TurnTooltipEnd = Server()->Tick() + Server()->TickSpeed() * 10;
	for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
	{
		CDurakGame::SSeat *pSeat = &m_vpGames[Game]->m_aSeats[i];
		int ClientID = pSeat->m_Player.m_ClientID;
		if (ClientID != -1)
		{
			if (Tooltip == CCard::TOOLTIP_NEXT_MOVE && pSeat->m_Player.m_Tooltip != Tooltip && !pSeat->m_Player.m_CanSetNextMove)
				continue;

			// Keep tooltip away for 2 seconds
			pSeat->m_Player.m_LastCursorMove = Server()->Tick() + Server()->TickSpeed() * (Tooltip == CCard::TOOLTIP_NEXT_MOVE ? 5 : 2);
			pSeat->m_Player.m_Tooltip = Tooltip;
			m_aCardUpdate[ClientID][&m_aStaticCards[DURAK_TEXT_TOOLTIP]] = true;
			for (int c = 0; c < MAX_CLIENTS; c++)
			{
				CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
				if ((pPlayer->GetTeam() == TEAM_SPECTATORS || pPlayer->IsPaused()) && pPlayer->GetSpectatorID() == ClientID)
					m_aCardUpdate[c][&m_aStaticCards[DURAK_TEXT_TOOLTIP]] = true;
			}
		}
	}
}

void CDurak::ProcessPlayerWin(int Game, CDurakGame::SSeat *pSeat, int WinPos)
{
	CDurakGame *pGame = m_vpGames[Game];
	int ClientID = pSeat->m_Player.m_ClientID;
	// Durak doesnt get any stake
	if (!pGame->CanProcessWin(pSeat->m_ID))
		return;

	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];

	// WinPos == -1: Force last player, if somebody won before, 0: nobody won before
	int64 ReturnStake = 0;
	if (pSeat->m_Player.m_Stake >= 0)
	{
		ReturnStake = pSeat->m_Player.m_Stake;
		HandleMoneyTransaction(ClientID, ReturnStake, "Durák stake return");
	}

	// First winner get's the stake of the loser, plus the stake of those who left inbetween
	int64 WinStake = 0;
	if (WinPos == 0)
	{
		WinStake = pGame->m_Stake;
		HandleMoneyTransaction(ClientID, WinStake, "Durák win");
	}
	// Reset stake as we have been processed now
	pSeat->m_Player.m_Stake = -1;

	char aBuf[128];
	char aTemp[128];
	// Chat message for participants
	if (WinPos >= 0)
	{
		SendChatToParticipants(Game, Localizable("'%s' won the game! (#%d)"), Server()->ClientName(ClientID), WinPos + 1);
	}
	else
	{
		SendChatToParticipants(Game, Localizable("'%s' won the game!"), Server()->ClientName(ClientID));
	}

	// Feedback for winner
	if (WinPos <= 0)
	{
		str_format(aBuf, sizeof(aBuf), "Congratulations, you win this Durák game!");
		if (WinStake)
		{
			str_format(aTemp, sizeof(aTemp), " Your earnings: +%lld money.", WinStake);
			str_append(aBuf, aTemp, sizeof(aBuf));
		}
	}
	else if (WinPos > 0)
	{
		str_copy(aBuf, GameServer()->m_apPlayers[ClientID]->Localize("Congratulations, you are not the Durák!"), sizeof(aBuf));
	}
	if (ReturnStake)
	{
		str_append(aBuf, " ", sizeof(aBuf));
		str_format(aTemp, sizeof(aTemp), GameServer()->m_apPlayers[ClientID]->Localize("Your stake got returned: +%lld money."), ReturnStake);
		str_append(aBuf, aTemp, sizeof(aBuf));
	}
	GameServer()->SendChatTarget(ClientID, aBuf);

	if (WinPos == -1 && !pGame->m_vWinners.empty())
	{
		// No money dupe. If a player won already, the stake of the last player got taken already.
		pGame->m_LeftPlayersStake -= pGame->m_Stake;
	}

	if (pGame->m_LeftPlayersStake > 0)
	{
		HandleMoneyTransaction(ClientID, pGame->m_LeftPlayersStake, "Durák stake of left players");
		str_format(aBuf, sizeof(aBuf), GameServer()->m_apPlayers[ClientID]->Localize("You got +%lld money from others leaving the game."), pGame->m_LeftPlayersStake);
		GameServer()->SendChatTarget(ClientID, aBuf);
		// Reset m_LeftPlayersStake, no money dupe
		pGame->m_LeftPlayersStake = 0;
	}

	// Confetti for the winner
	pPlayer->m_ConfettiWinEffectTick = Server()->Tick();

	// Update acc stats
	if (WinPos >= 0 && pPlayer->GetAccID() >= ACC_START)
	{
		GameServer()->m_Accounts[pPlayer->GetAccID()].m_DurakWins++;
	}

	EndMove(Game, pSeat, true);
}

void CDurak::Snap(int SnappingClient)
{
	if (SnappingClient == -1)
		return;

	int ClientID = SnappingClient;
	bool IsSpectator = false;

	CPlayer *pSnap = GameServer()->m_apPlayers[SnappingClient];
	if ((pSnap->GetTeam() == TEAM_SPECTATORS || pSnap->IsPaused()) && pSnap->GetSpectatorID() >= 0 && pSnap->GetSpectatorID() != SnappingClient)
	{
		ClientID = pSnap->GetSpectatorID();
		IsSpectator = true;
	}

	int Game = GetGameByClient(ClientID);
	CDurakGame *pGame = Game >= 0 ? m_vpGames[Game] : 0;
	// As spectator, always render hand cards (hidden) of defender
	CDurakGame::SSeat *pSeat = 0;
	if (pGame)
	{
		if (pGame->m_DurakClientID != -1)
		{
			pSeat = pGame->GetSeatByClient(pGame->m_DurakClientID);
			IsSpectator = false;
		}
		else
		{
			pSeat = IsSpectator && pGame->m_DefenderIndex != -1 ? &pGame->m_aSeats[pGame->m_DefenderIndex] : pGame->GetSeatByClient(ClientID);
		}
	}

	// Prepare snap ids..
	PrepareDurakSnap(SnappingClient, pGame, pSeat);
	m_aSnappedSeatIndex[SnappingClient] = pSeat ? pSeat->m_ID : -1;

	if (Game < 0 || (pGame && !pGame->m_Running))
	{
		if (m_vpGames.size())
		{
			vec2 Pos = m_vpGames[0]->m_TablePos;
			if (::NetworkClipped(GameServer(), SnappingClient, Pos))
				return;

			// Abusing tooltip and keyboard control for stats
			Pos.y += DURAK_CARD_NAME_OFFSET;

			if (m_aStaticCards[DURAK_TEXT_TOOLTIP].m_Active)
			{
				if (pSeat)
				{
					Pos.y -= 32.f;
				}
				SnapDurakCard(SnappingClient, pGame, &m_aStaticCards[DURAK_TEXT_TOOLTIP], false, Pos);
				Pos.y += 32.f;
			}
			if (m_aStaticCards[DURAK_TEXT_KEYBOARD_CONTROL].m_Active)
			{
				SnapDurakCard(SnappingClient, pGame, &m_aStaticCards[DURAK_TEXT_KEYBOARD_CONTROL], false, Pos);
				Pos.y += 32.f;
			}
			if (m_aStaticCards[DURAK_TEXT_TRUMP_CARD].m_Active)
			{
				SnapDurakCard(SnappingClient, pGame, &m_aStaticCards[DURAK_TEXT_TRUMP_CARD], false, Pos);
			}
		}
		return;
	}

	// Static cards
	for (int i = 0; i < NUM_DURAK_STATIC_CARDS; i++)
	{
		CCard *pCard = &m_aStaticCards[i];
		if (pCard->m_Active)
			SnapDurakCard(SnappingClient, pGame, pCard);
	}

	// Hand cards
	if (pSeat)
	{
		for (unsigned int i = 0; i < pSeat->m_Player.m_vHandCards.size(); i++)
			SnapDurakCard(SnappingClient, pGame, &pSeat->m_Player.m_vHandCards[i], IsSpectator);
	}

	// Attack cards
	for (int i = 0; i < MAX_DURAK_ATTACKS; i++)
	{
		if (pGame->m_Attacks[i].m_Offense.Valid())
		{
			// Snap defense on higher id than offense card
			if (pGame->m_Attacks[i].m_Defense.Valid())
				SnapDurakCard(SnappingClient, pGame, &pGame->m_Attacks[i].m_Defense);
			SnapDurakCard(SnappingClient, pGame, &pGame->m_Attacks[i].m_Offense);
		}
	}
}

void CDurak::PrepareStaticCards(CDurakGame *pGame, CDurakGame::SSeat *pSeat)
{
	bool InGame = pGame && pGame->m_Running;
	for (int i = 0; i < NUM_DURAK_STATIC_CARDS; i++)
	{
		CCard *pCard = &m_aStaticCards[i];
		pCard->m_Active = false;
		switch (i)
		{
			case DURAK_TEXT_CARDS_STACK_0:
			case DURAK_TEXT_CARDS_STACK_1:
			case DURAK_TEXT_CARDS_STACK_2:
			case DURAK_TEXT_CARDS_STACK_3:
			case DURAK_TEXT_CARDS_STACK_4:
			{
				if (!InGame)
					break;

				if (pGame->m_Deck.IsEmpty())
				{
					if (i == 0 && pSeat && pGame->GetStateBySeat(pSeat->m_ID) == DURAK_PLAYERSTATE_ATTACK)
					{
						pCard->m_Active = true;
						pCard->SetInd(CCard::IND_END_MOVE_BUTTON);
					}
				}
				else if (pGame->m_Deck.Size() != 1)
				{
					int Inversed = 5 - i - 1;
					if (Inversed  < (int)pGame->m_Deck.Size())
					{
						pCard->m_Active = true;
						pCard->Invalidate();
					}
				}
			} break;
			case DURAK_TEXT_TRUMP_CARD:
			{
				if (InGame && !pGame->m_Deck.IsEmpty())
				{
					CCard *pTrumpCard = pGame->m_Deck.GetTrumpCard();
					pCard->m_Suit = pTrumpCard->m_Suit;
					pCard->m_Rank = pTrumpCard->m_Rank;
					pCard->m_Active = true;
				}
				else if (pGame && !pGame->m_Running)
				{
					pCard->m_Active = true;
					pCard->SetInd(CCard::IND_STAKE);
				}
			} break;
			case DURAK_TEXT_KEYBOARD_CONTROL:
			{
				if (InGame && pSeat)
				{
					pCard->m_Active = true;
					pCard->SetInd(pSeat->m_Player.m_KeyboardControl ? CCard::IND_KEYBOARD_ON : CCard::IND_KEYBOARD_OFF);
				}
				else if (pGame && !pGame->m_Running)
				{
					pCard->m_Active = true;
					pCard->SetInd(CCard::IND_PLAYERCOUNTER);
				}
			} break;
			case DURAK_TEXT_TOOLTIP:
			{
				pCard->m_Active = true;
				pCard->SetInd(InGame ? CCard::IND_EMPTY : CCard::IND_DURAK_TABLE_LABEL);
				pCard->m_TableOffset = vec2(0, DURAK_CARD_NAME_OFFSET);
				if (InGame && pSeat && pSeat->m_Player.m_Tooltip != CCard::TOOLTIP_NONE)
				{
					if (pSeat->m_Player.IsTurnTooltip())
					{
						if (pGame->m_TurnTooltipEnd > Server()->Tick())
						{
							pCard->SetTooltip(pSeat->m_Player.m_Tooltip);
							break;
						}
						else
						{
							pSeat->m_Player.m_Tooltip = CCard::TOOLTIP_NONE;
						}
					}

					if (pSeat->m_Player.m_KeyboardControl || pSeat->m_Player.m_LastCursorMove < Server()->Tick() - Server()->TickSpeed() / 10)
					{
						pCard->SetTooltip(pSeat->m_Player.m_Tooltip);
					}
				}
				else if (pGame && !pGame->m_Running)
				{
					pCard->SetInd(CCard::IND_START_TIMER);
				}
			} break;
		}
	}
}

void CDurak::PrepareDurakSnap(int SnappingClient, CDurakGame *pGame, CDurakGame::SSeat *pSeat)
{
	// Prepare static card values for SnappingClient
	PrepareStaticCards(pGame, pSeat);

	int NumStatic = 0;
	for (int i = 0; i < NUM_DURAK_STATIC_CARDS; i++)
		if (m_aStaticCards[i].m_Active)
			NumStatic++;

	int NumAttack = 0;
	if (pGame)
	{
		for (int i = 0; i < MAX_DURAK_ATTACKS; i++)
		{
			if (pGame->m_Attacks[i].m_Offense.Valid())
			{
				NumAttack++;
				if (pGame->m_Attacks[i].m_Defense.Valid())
					NumAttack++;
			}
		}
	}

	const int NumHand = pSeat ? (int)pSeat->m_Player.m_vHandCards.size() : 0;
	const int NumNeeded = NumStatic + NumHand + NumAttack;
	const int Diff = NumNeeded - m_aDurakNumReserved[SnappingClient];
	if (Diff != 0)
	{
		GameServer()->m_World.AddToNumReserved(SnappingClient, Diff);
		m_aDurakNumReserved[SnappingClient] += Diff;
	}

	std::map<CCard *, int> NewSnapMap;
	int SnapID = GameServer()->m_World.GetFirstDurakID(SnappingClient);

	// Static cards
	for (int i = 0; i < NUM_DURAK_STATIC_CARDS; i++)
	{
		CCard *pCard = &m_aStaticCards[i];
		if (pCard->m_Active)
		{
			NewSnapMap[pCard] = SnapID--;
		}
	}

	// Hand cards
	if (pSeat)
	{
		for (unsigned int i = 0; i < pSeat->m_Player.m_vHandCards.size(); i++)
		{
			NewSnapMap[&pSeat->m_Player.m_vHandCards[i]] = SnapID--;
		}
	}

	// Attack cards
	if (pGame)
	{
		for (int i = 0; i < MAX_DURAK_ATTACKS; i++)
		{
			if (pGame->m_Attacks[i].m_Offense.Valid())
			{
				// Snap defense on higher id than offense card
				if (pGame->m_Attacks[i].m_Defense.Valid())
					NewSnapMap[&pGame->m_Attacks[i].m_Defense] = SnapID--;
				NewSnapMap[&pGame->m_Attacks[i].m_Offense] = SnapID--;
			}
		}
	}

	// 0.7
	UpdateCardSnapMapping(SnappingClient, NewSnapMap, pGame, pSeat && pSeat->m_Player.m_ClientID != SnappingClient);
}

void CDurak::PostSnap()
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_aUpdateTeamsState[i])
		{
			((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams.SendTeamsState(i);
			m_aUpdateTeamsState[i] = false;
		}
	}
}

void CDurak::UpdateCardSnapMapping(int SnappingClient, const std::map<CCard *, int> &NewMap, CDurakGame *pGame, bool IsSpectator)
{
	auto &OldMap = m_aLastSnapID[SnappingClient];
	// First pass: identify disconnects
	for (auto &[pOldCard, OldID] : OldMap)
	{
		auto it = NewMap.find(pOldCard);
		if (it == NewMap.end() || it->second != OldID || m_aCardUpdate[SnappingClient][pOldCard])
		{
			GameServer()->m_apPlayers[SnappingClient]->SendDisconnect(OldID);
			m_aUpdateTeamsState[SnappingClient] = true;
		}
	}

	// Second pass: identify connects/updates
	for (auto &[pCard, NewID] : NewMap)
	{
		auto it = OldMap.find(pCard);
		
		if (it == OldMap.end() || it->second != NewID || m_aCardUpdate[SnappingClient][pCard])
		{
			m_aUpdateTeamsState[SnappingClient] = true;
			if (!Server()->IsSevendown(SnappingClient))
			{
				const char *pName = IsSpectator ? GetCardSymbol(-1, -1) : GetCardSymbol(pCard->m_Suit, pCard->m_Rank, pGame, SnappingClient);
				CNetMsg_Sv_ClientInfo NewClientInfoMsg;
				NewClientInfoMsg.m_ClientID = NewID;
				NewClientInfoMsg.m_Local = 0;
				NewClientInfoMsg.m_Team = TEAM_BLUE;
				NewClientInfoMsg.m_pName = pName;
				NewClientInfoMsg.m_pClan = "";
				NewClientInfoMsg.m_Country = -1;
				NewClientInfoMsg.m_Silent = 1;
				for (int p = 0; p < NUM_SKINPARTS; p++)
				{
					NewClientInfoMsg.m_apSkinPartNames[p] = "default";
					NewClientInfoMsg.m_aUseCustomColors[p] = 1;
					NewClientInfoMsg.m_aSkinPartColors[p] = 255;
				}
				Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD|MSGFLAG_NOTRANSLATE, SnappingClient);
			}
			m_aCardUpdate[SnappingClient][pCard] = false;
		}
	}

	// Update the old map with the new map
	OldMap = NewMap;
}

void CDurak::SnapDurakCard(int SnappingClient, CDurakGame *pGame, CCard *pCard, bool IsSpectator, vec2 ForcePos)
{
	int ID = m_aLastSnapID[SnappingClient][pCard];
	if (!Server()->IsSevendown(SnappingClient))
	{
		// 0.7 uses UpdateCardSnapMapping()
		CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, ID, sizeof(CNetObj_PlayerInfo)));
		if(!pPlayerInfo)
			return;
		pPlayerInfo->m_PlayerFlags = 0;
		pPlayerInfo->m_Score = -1;
		pPlayerInfo->m_Latency = 0;
	}
	else
	{
		int *pClientInfo = (int*)Server()->SnapNewItem(11 + NUM_NETOBJTYPES, ID, 17*4); // NETOBJTYPE_CLIENTINFO
		if(!pClientInfo)
			return;
		const char *pName = IsSpectator ? GetCardSymbol(-1, -1) : GetCardSymbol(pCard->m_Suit, pCard->m_Rank, pGame, SnappingClient);
		StrToInts(&pClientInfo[0], 4, pName);
		StrToInts(&pClientInfo[4], 3, "");
		StrToInts(&pClientInfo[8], 6, pGame ? "x_spec" : "default");
		pClientInfo[14] = 1;
		pClientInfo[15] = pClientInfo[16] = 255;

		CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, ID, 5*4));
		if(!pPlayerInfo)
			return;
		((int*)pPlayerInfo)[0] = 0; // local
		((int*)pPlayerInfo)[1] = ID;
		((int*)pPlayerInfo)[2] = TEAM_BLUE;
		((int*)pPlayerInfo)[3] = -1; // score
		((int*)pPlayerInfo)[4] = 0;
	}

	CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, ID, sizeof(CNetObj_Character)));
	if(!pCharacter)
		return;

	vec2 TablePos = ForcePos;
	vec2 Pos = ForcePos;
	if (pGame && Pos == vec2(-1, -1))
	{
		TablePos = pGame->m_TablePos;
		Pos = TablePos + pCard->m_TableOffset;
	}
	pCharacter->m_X = Pos.x;
	pCharacter->m_Y = Pos.y;
	if (GameServer()->GetClientDDNetVersion(SnappingClient) < VERSION_DDNET_TEE_NO_WEAPON)
	{
		pCharacter->m_Weapon = WEAPON_GUN;
		if (Pos.x > TablePos.x)
			pCharacter->m_Angle = 804;
	}
	else
	{
		pCharacter->m_Weapon = -1;
	}

	if(Server()->IsSevendown(SnappingClient))
	{
		int PlayerFlags = 0;
		int Health = pCharacter->m_Health;
		int Armor = pCharacter->m_Armor;
		int AmmoCount = pCharacter->m_AmmoCount;
		int Weapon = pCharacter->m_Weapon;
		int Emote = pCharacter->m_Emote;
		int AttackTick = pCharacter->m_AttackTick;

		int Offset = sizeof(CNetObj_CharacterCore) / 4;
		((int*)pCharacter)[Offset+0] = PlayerFlags;
		((int*)pCharacter)[Offset+1] = Health;
		((int*)pCharacter)[Offset+2] = Armor;
		((int*)pCharacter)[Offset+3] = AmmoCount;
		((int*)pCharacter)[Offset+4] = Weapon;
		((int*)pCharacter)[Offset+5] = Emote;
		((int*)pCharacter)[Offset+6] = AttackTick;
	}
	else
	{
		if(pCharacter->m_Angle > (int)(pi * 256.0f))
		{
			pCharacter->m_Angle -= (int)(2.0f * pi * 256.0f);
		}
	}
}
