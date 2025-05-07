// made by fokkonaut

#ifndef GAME_SERVER_MINIGAMES_DURAK_H
#define GAME_SERVER_MINIGAMES_DURAK_H

#include "minigame.h"
#include <vector>
#include <map>
#include <algorithm>
#include <random>

enum
{
	MIN_DURAK_PLAYERS = 2,
	MAX_DURAK_PLAYERS = 6,
	NUM_DURAK_INITIAL_HAND_CARDS = 6,
	MAX_DURAK_ATTACKS = 6,
	MAX_DURAK_GAMES = VANILLA_MAX_CLIENTS-1,
	DURAK_CARD_NAME_OFFSET = 48,
};

enum
{
	DURAK_PLAYERSTATE_NONE,
	DURAK_PLAYERSTATE_ATTACK,
	DURAK_PLAYERSTATE_DEFEND
};

#define DURAK_CARD_HOVER_OFFSET (CCard::ms_CardSizeRadius.y / 2)

class CCard
{
public:
	static const vec2 ms_CardSizeRadius;
	static const vec2 ms_TableSizeRadius;
	static const vec2 ms_AttackAreaRadius;
	static const vec2 ms_AttackAreaCenterOffset;

	CCard() : CCard(-1, -1) {}
	CCard(int Suit, int Rank) : m_Suit(Suit), m_Rank(Rank)
	{
		m_TableOffset = vec2(0, 0);
		m_HoverState = HOVERSTATE_NONE;
		m_Active = false;
		m_HeightPos = 0.f;
	}

	int m_Suit;
	int m_Rank;
	vec2 m_TableOffset;
	enum
	{
		HOVERSTATE_NONE,
		HOVERSTATE_MOUSEOVER,
		HOVERSTATE_DRAGGING,
	};
	int m_HoverState;
	float m_HeightPos;
	bool m_Active;

	bool Valid() { return m_Suit != -1 && m_Rank != -1; }
	void Invalidate() { m_Suit = m_Rank = -1; }

	static bool InAttackArea(vec2 Target) // Target - m_TablePos
	{
		Target.y += DURAK_CARD_NAME_OFFSET;
		return (ms_AttackAreaCenterOffset.x - ms_AttackAreaRadius.x < Target.x && ms_AttackAreaCenterOffset.x + ms_AttackAreaRadius.x > Target.x
			&& ms_AttackAreaCenterOffset.y - ms_AttackAreaRadius.y < Target.y && ms_AttackAreaCenterOffset.y + ms_AttackAreaRadius.y > Target.y);
	}

	bool MouseOver(vec2 Target) // Target - m_TablePos
	{
		vec2 CardPos = vec2(m_TableOffset.x, m_TableOffset.y-DURAK_CARD_NAME_OFFSET);
		float HighY = ms_CardSizeRadius.y;
		if (m_HoverState == HOVERSTATE_MOUSEOVER) // Avoid flickering
			HighY += DURAK_CARD_HOVER_OFFSET;
		return (CardPos.x - ms_CardSizeRadius.x < Target.x && CardPos.x + ms_CardSizeRadius.x > Target.x && CardPos.y - ms_CardSizeRadius.y < Target.y && CardPos.y + HighY > Target.y);
	}

	void SetHovered(bool Set)
	{
		if (Set)
		{
			m_TableOffset.y = m_HeightPos - DURAK_CARD_HOVER_OFFSET;
			m_HoverState = HOVERSTATE_MOUSEOVER;
		}
		else
		{
			m_TableOffset.y = m_HeightPos;
			m_HoverState = HOVERSTATE_NONE;
		}
	}

	bool IsStrongerThan(const CCard &Other, int TrumpSuit) const
	{
		if (m_Suit == TrumpSuit && Other.m_Suit != TrumpSuit)
			return true;
		if (m_Suit != TrumpSuit && Other.m_Suit == TrumpSuit)
			return false;
		return m_Suit == Other.m_Suit && m_Rank > Other.m_Rank;
	}

	enum EIndicatorSuit
	{
		IND_EMPTY = -2,
		IND_DURAK_TABLE_LABEL = -3,
		IND_KEYBOARD_ON = -4,
		IND_KEYBOARD_OFF = -5,
		IND_END_MOVE_BUTTON = -6,
		IND_START_TIMER = -7,
		IND_PLAYERCOUNTER = -8,
		IND_STAKE = -9,
		IND_TOOLTIP_ATTACK = -10,
		IND_TOOLTIP_DEFEND = -11,
		IND_TOOLTIP_PASS = -12,
		IND_TOOLTIP_END_MOVE = -13,
		IND_TOOLTIP_TAKE_CARDS = -14,
		IND_TOOLTIP_SELECT_ATTACK = -15,
		IND_TOOLTIP_ATTACKERS_TURN = -16,
		IND_TOOLTIP_DEFENDER_PASSED = -17,
		IND_TOOLTIP_NEXT_MOVE = -18,
	};
	enum
	{
		TOOLTIP_NONE = -1,
		TOOLTIP_ATTACK,
		TOOLTIP_DEFEND,
		TOOLTIP_PASS,
		TOOLTIP_END_MOVE,
		TOOLTIP_TAKE_CARDS,
		TOOLTIP_SELECT_ATTACK,
		TOOLTIP_ATTACKERS_TURN,
		TOOLTIP_DEFENDER_PASSED,
		TOOLTIP_NEXT_MOVE,
		NUM_TOOLTIPS
	};
	void SetInd(EIndicatorSuit IndSuit)
	{
		m_Suit = (int)IndSuit;
	}
	void SetTooltip(int Tooltip)
	{
		// Don't pass TOOLTIP_NONE(0), will be handled at a different place xd
		m_Suit = IND_TOOLTIP_ATTACK - Tooltip;
		m_TableOffset = vec2(-40.f, -40.f);
	}
};

class CDeck
{
	std::vector<CCard> m_vDeck;
	int m_TrumpSuit;

public:
	CDeck()
	{
		for (int suit = 0; suit < 4; suit++)
			for (int rank = 6; rank <= 14; rank++)
				m_vDeck.emplace_back(suit, rank);
	}

	void Shuffle()
	{
		std::random_device rd;
		std::mt19937 g(rd());
		std::shuffle(m_vDeck.begin(), m_vDeck.end(), g);
	}

	CCard DrawCard()
	{
		if (IsEmpty()) // Invalid card if deck is empty
			return CCard();
		CCard TopCard = m_vDeck.back();
		m_vDeck.pop_back();
		return TopCard;
	}

	void PushFrontTrumpCard(CCard TrumpCard)
	{
		m_vDeck.insert(m_vDeck.begin(), TrumpCard);
	}

	bool IsEmpty() const { return m_vDeck.empty(); }
	int Size() const { return m_vDeck.size(); }

	void SetTrumpSuit(int Suit) { m_TrumpSuit = Suit; }
	int GetTrumpSuit() const { return m_TrumpSuit; }

	CCard *GetTrumpCard() { return m_vDeck.size() ? &m_vDeck[0] : 0; }
};

class CDurakGame
{
public:
	CDurakGame(CDurakGame *pDurakBase) : CDurakGame(pDurakBase->m_Number)
	{
		m_TablePos = pDurakBase->m_TablePos;
		for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
		{
			m_aSeats[i].m_MapIndex = pDurakBase->m_aSeats[i].m_MapIndex;
		}
	}
	CDurakGame(int Number)
	{
		m_Number = Number;
		m_TablePos = vec2(-1, -1);
		m_Stake = -1;
		m_GameStartTick = 0;
		m_Running = false;
		m_InitialAttackerIndex = -1;
		m_AttackerIndex = -1;
		m_DefenderIndex = -1;
		m_RoundCount = 0;
		m_NextMove = 0;
		m_GameOverTick = 0;
		m_LeftPlayersStake = 0;
		m_TurnTooltipEnd = 0;
		m_IsDefenseOngoing = false;
		m_DurakClientID = -1;
		for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
		{
			m_aSeats[i].m_ID = i;
			m_aSeats[i].m_MapIndex = -1;
			m_aSeats[i].m_Player.Reset();
		}
		float CardSize = CCard::ms_CardSizeRadius.x * 2 + 4.f; // Gap
		float PosX = CCard::ms_AttackAreaCenterOffset.x - (MAX_DURAK_ATTACKS / 2) * CardSize;
		for (int i = 0; i < MAX_DURAK_ATTACKS; i++)
		{
			m_Attacks[i].m_Offense.m_TableOffset = vec2(PosX, CCard::ms_AttackAreaCenterOffset.y);
			m_Attacks[i].m_Offense.m_HeightPos = CCard::ms_AttackAreaCenterOffset.y;
			m_Attacks[i].m_Defense.m_TableOffset = vec2(PosX - CCard::ms_CardSizeRadius.x / 3.f, CCard::ms_AttackAreaCenterOffset.y + CCard::ms_CardSizeRadius.y * 1.25f);
			PosX += CardSize;
		}
	}

	struct SSeat
	{
		int m_ID;
		int m_MapIndex;
		struct SPlayer
		{
			void Reset()
			{
				m_ClientID = -1;
				m_Stake = -1;
				m_vHandCards.clear();
				m_HoveredCard = -1;
				m_LastCursorX = -1.f;
				m_Tooltip = CCard::TOOLTIP_NONE;
				m_LastTooltip = CCard::TOOLTIP_NONE;
				m_KeyboardControl = false;
				m_LastKeyboardControl = false;
				m_LastCursorMove = 0;
				m_SelectedAttack = -1;
				m_EndedMove = false;
				m_LastStake = -1;
				m_LastNumParticipants = -1;
				m_LastNumHandCards = -1;
				m_CanSetNextMove = true;
			}
			bool IsTurnTooltip() { return m_Tooltip == CCard::TOOLTIP_ATTACKERS_TURN || m_Tooltip == CCard::TOOLTIP_DEFENDER_PASSED || m_Tooltip == CCard::TOOLTIP_NEXT_MOVE; }
			struct
			{
				int m_Direction = 0;
				bool m_Jump = false;
				bool m_HookColl = false;
				int m_TargetX = 0;
				int m_TargetY = 0;
			} m_LastInput;
			int m_ClientID;
			int64 m_Stake;
			std::vector<CCard> m_vHandCards;
			int m_HoveredCard;
			float m_LastCursorX;
			int m_Tooltip;
			int m_LastTooltip;
			bool m_KeyboardControl;
			bool m_LastKeyboardControl;
			int64 m_LastCursorMove;
			int m_SelectedAttack;
			bool m_EndedMove;
			int m_LastNumParticipants;
			int64 m_LastStake;
			int m_LastNumHandCards;
			bool m_CanSetNextMove;
		} m_Player;
	} m_aSeats[MAX_DURAK_PLAYERS];

	struct SAttack
	{
		CCard m_Offense;
		CCard m_Defense;
	} m_Attacks[MAX_DURAK_ATTACKS];

	int m_Number;
	vec2 m_TablePos;
	int64 m_Stake;
	int64 m_GameStartTick;
	bool m_Running;
	int m_InitialAttackerIndex;
	int m_AttackerIndex;
	int m_DefenderIndex;
	CDeck m_Deck;
	int m_RoundCount;
	int64 m_NextMove;
	std::vector<int> m_vWinners;
	int64 m_GameOverTick;
	int64 m_LeftPlayersStake;
	int64 m_TurnTooltipEnd;
	bool m_IsDefenseOngoing;
	int m_DurakClientID;

	SSeat *GetSeatByClient(int ClientID)
	{
		for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
			if (m_aSeats[i].m_Player.m_ClientID == ClientID)
				return &m_aSeats[i];
		return 0;
	}

	int GetStateBySeat(int SeatIndex)
	{
		if (m_aSeats[SeatIndex].m_Player.m_EndedMove)
			return DURAK_PLAYERSTATE_NONE;
		if (SeatIndex == m_AttackerIndex || SeatIndex == GetNextPlayer(m_DefenderIndex))
			return DURAK_PLAYERSTATE_ATTACK;
		if (SeatIndex == m_DefenderIndex)
			return DURAK_PLAYERSTATE_DEFEND;
		return DURAK_PLAYERSTATE_NONE;
	}

	int NumDeployedStakes()
	{
		int Num = 0;
		for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
			if (m_aSeats[i].m_Player.m_ClientID != -1 && m_aSeats[i].m_Player.m_Stake >= 0)
				Num++;
		return Num;
	}

	int NumParticipants()
	{
		int Num = 0;
		for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
			if (m_aSeats[i].m_Player.m_ClientID != -1)
				if (m_Stake != -1 && m_aSeats[i].m_Player.m_Stake == m_Stake)
					Num++;
		return Num;
	}

	void DealHandCards()
	{
		for (int i = 0; i < NUM_DURAK_INITIAL_HAND_CARDS; i++)
		{
			for (int s = 0; s < MAX_DURAK_PLAYERS; s++)
			{
				if (m_aSeats[s].m_Player.m_ClientID != -1)
				{
					CCard Card = m_Deck.DrawCard();
					Card.m_TableOffset.y = Card.m_HeightPos = CCard::ms_TableSizeRadius.y;
					m_aSeats[s].m_Player.m_vHandCards.push_back(Card);
					if (m_Deck.IsEmpty())
					{
						m_Deck.SetTrumpSuit(Card.m_Suit);
						break; // Should be last card anyways, but better be sure xd
					}
				}
			}
		}

		if (!m_Deck.IsEmpty())
		{
			CCard Card = m_Deck.DrawCard();
			m_Deck.SetTrumpSuit(Card.m_Suit);
			m_Deck.PushFrontTrumpCard(Card);
		}

		for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
		{
			SortHand(&m_aSeats[i], m_Deck.GetTrumpSuit());
		}
	}

	void SortHand(SSeat *pSeat, int TrumpSuit)
	{
		std::vector<CCard> &vHandCards = pSeat->m_Player.m_vHandCards;
		for (int i = 0; i < (int)vHandCards.size(); i++)
		{
			if (pSeat->m_Player.m_HoveredCard == i)
			{
				vHandCards[i].SetHovered(false);
				pSeat->m_Player.m_HoveredCard = -1;
				break;
			}
		}

		// Card count per suit
		int aSuitCount[4] = { 0 };
		for (const CCard &Card : vHandCards)
			aSuitCount[Card.m_Suit]++;

		// Highest rank within each suit
		int aSuitMaxRank[4] = { 0 };
		for (const CCard &Card : vHandCards)
			aSuitMaxRank[Card.m_Suit] = max(aSuitMaxRank[Card.m_Suit], Card.m_Rank);

		std::sort(vHandCards.begin(), vHandCards.end(), [&](const CCard &a, const CCard &b) {
			// Trump cards first
			if (a.m_Suit == TrumpSuit && b.m_Suit != TrumpSuit)
				return true;
			if (a.m_Suit != TrumpSuit && b.m_Suit == TrumpSuit)
				return false;

			// Both cards same suit, sort by rank (higher = left)
			if (a.m_Suit == b.m_Suit)
				return a.m_Rank > b.m_Rank;

			// Sort by count of cards per suit (more = left)
			if (aSuitCount[a.m_Suit] != aSuitCount[b.m_Suit])
				return aSuitCount[a.m_Suit] > aSuitCount[b.m_Suit];

			// If everything is same, sort by highest rank of suit
			return aSuitMaxRank[a.m_Suit] > aSuitMaxRank[b.m_Suit];
		});
	}

	int GetNextPlayer(int CurrentIndex, bool CheckHands = false, bool Prev = false)
	{
		for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
		{
			int NextIndex = (CurrentIndex + (Prev ? -1 : 1)*(i + 1) + MAX_DURAK_PLAYERS) % MAX_DURAK_PLAYERS;
			if (m_aSeats[NextIndex].m_Player.m_ClientID != -1 && m_aSeats[NextIndex].m_Player.m_Stake >= 0 && (!CheckHands || m_aSeats[NextIndex].m_Player.m_vHandCards.size()))
				return NextIndex;
		}
		return -1;
	}

	bool NextMoveSoon(int Tick)
	{
		return m_NextMove && m_NextMove > Tick && m_NextMove < Tick + SERVER_TICK_SPEED * 5;
	}

	bool ProcessNextMove(int64 CurrentTick)
	{
		if (!m_NextMove)
			m_NextMove = CurrentTick + SERVER_TICK_SPEED * 60;
		return m_NextMove <= CurrentTick;
	}

	void NextRound(bool SuccessfulDefense = false)
	{
		for (int i = 0; i < MAX_DURAK_ATTACKS; i++)
		{
			m_Attacks[i].m_Offense.Invalidate();
			m_Attacks[i].m_Defense.Invalidate();
		}

		if (m_RoundCount == 0)
		{
			if (m_InitialAttackerIndex != -1)
			{
				// Determined by m_vLastDuraks
				m_AttackerIndex = m_InitialAttackerIndex;
			}
			else
			{
				int LowestTrumpPlayerIndex = -1;
				int LowestTrumpRank = 15;
				for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
				{
					if (m_aSeats[i].m_Player.m_ClientID == -1)
						continue;

					for (auto& Card : m_aSeats[i].m_Player.m_vHandCards)
					{
						if (Card.m_Suit == m_Deck.GetTrumpSuit() && Card.m_Rank < LowestTrumpRank)
						{
							LowestTrumpRank = Card.m_Rank;
							LowestTrumpPlayerIndex = i;
						}
					}
				}
				// In case no trump was dealt, just begin somewhere
				if (LowestTrumpPlayerIndex == -1)
				{
					for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
					{
						if (m_aSeats[i].m_Player.m_ClientID != -1)
						{
							LowestTrumpPlayerIndex = i;
							break;
						}
					}
				}
				m_AttackerIndex = m_InitialAttackerIndex = LowestTrumpPlayerIndex;
			}
		}
		else
		{
			// Do before determining next attacker / defender to keep the order
			DrawCardsAfterRound();

			// If he won, we move on :D
			if (SuccessfulDefense && m_aSeats[m_DefenderIndex].m_Player.m_Stake >= 0)
			{
				// If successfully defended, defender turns to attacker
				m_AttackerIndex = m_InitialAttackerIndex = m_DefenderIndex;
			}
			else
			{
				// Otherwise skip us, next player starts attacking
				m_AttackerIndex = m_InitialAttackerIndex = GetNextPlayer(m_DefenderIndex);
			}
		}
		m_DefenderIndex = GetNextPlayer(m_InitialAttackerIndex);
		m_RoundCount++;
		m_IsDefenseOngoing = false;
		m_NextMove = 0;
	}

	void DrawCardsAfterRound()
	{
		std::vector<int> vDrawOrder;
		vDrawOrder.push_back(m_InitialAttackerIndex);
		int CurPlayer = m_InitialAttackerIndex;
		for (int i = 1; i < MAX_DURAK_PLAYERS; i++)
		{
			CurPlayer = (CurPlayer + 1) % MAX_DURAK_PLAYERS;
			if (CurPlayer != m_DefenderIndex && m_aSeats[CurPlayer].m_Player.m_ClientID != -1 && m_aSeats[CurPlayer].m_Player.m_Stake >= 0)
			{
				vDrawOrder.push_back(CurPlayer);
			}
		}
		vDrawOrder.push_back(m_DefenderIndex);

		for (unsigned int i = 0; i < vDrawOrder.size(); i++)
		{
			int Index = vDrawOrder[i];
			if (Index != -1 && m_aSeats[Index].m_Player.m_ClientID != -1)
			{
				while ((int)m_aSeats[Index].m_Player.m_vHandCards.size() < NUM_DURAK_INITIAL_HAND_CARDS)
				{
					if (!m_Deck.IsEmpty())
					{
						CCard Card = m_Deck.DrawCard();
						Card.m_TableOffset.y = Card.m_HeightPos = CCard::ms_TableSizeRadius.y;
						m_aSeats[Index].m_Player.m_vHandCards.push_back(Card);
					}
					else
					{
						if (m_aSeats[Index].m_Player.m_vHandCards.empty() && std::find(m_vWinners.begin(), m_vWinners.end(), Index) == m_vWinners.end())
						{
							m_vWinners.push_back(Index);
						}
						break;
					}
				}
				SortHand(&m_aSeats[Index], m_Deck.GetTrumpSuit());
			}
		}
	}

	std::vector<int> GetOpenAttacks()
	{
		std::vector<int> vOpenAttackIndices;
		for (int i = 0; i < MAX_DURAK_ATTACKS; i++)
			if (m_Attacks[i].m_Offense.Valid() && !m_Attacks[i].m_Defense.Valid())
				vOpenAttackIndices.push_back(i);
		return vOpenAttackIndices;
	}

	bool IsGameOver()
	{
		int PlayersLeft = 0;
		for (int i = 0; i < MAX_DURAK_PLAYERS; i++)
		{
			if (m_aSeats[i].m_Player.m_ClientID != -1 && !m_aSeats[i].m_Player.m_vHandCards.empty())
			{
				PlayersLeft++;
				m_DurakClientID = m_aSeats[i].m_Player.m_ClientID;
			}
		}
		if (PlayersLeft >= MIN_DURAK_PLAYERS || !m_Deck.IsEmpty())
		{
			m_DurakClientID = -1;
			return false;
		}
		return true;
	}

	bool CanProcessWin(int Seat)
	{
		return m_Running && m_aSeats[Seat].m_Player.m_Stake >= 0 && m_aSeats[Seat].m_Player.m_ClientID != m_DurakClientID;
	}

	int TryAttack(int Seat, CCard *pCard, int Tick)
	{
		if (!pCard || !pCard->Valid())
			return -1;

		int Used = 0;
		int Required = 0;
		for (auto &pair : m_Attacks)
		{
			if (pair.m_Offense.Valid())
			{
				Used++;
				if (!pair.m_Defense.Valid())
					Required++;
			}
		}

		if (Used >= MAX_DURAK_ATTACKS)
			return -1;

		// allow attack from left guy too, as soon as at least 1 slot has been used
		if (Seat != m_AttackerIndex && (!Used || GetStateBySeat(Seat) != DURAK_PLAYERSTATE_ATTACK))
			return -1;

		if ((int)m_aSeats[m_DefenderIndex].m_Player.m_vHandCards.size() < Required + 1)
			return -1;

		// If not first attack, card has to match existing ranks on table
		if (Used > 0)
		{
			bool Match = false;
			for (auto &pair : m_Attacks)
			{
				if ((pair.m_Offense.Valid() && pair.m_Offense.m_Rank == pCard->m_Rank) ||
					(pair.m_Defense.Valid() && pair.m_Defense.m_Rank == pCard->m_Rank))
				{
					Match = true;
					break;
				}
			}
			if (!Match)
				return -1;
		}

		// Assign card to attack slot
		m_Attacks[Used].m_Offense.m_Suit = pCard->m_Suit;
		m_Attacks[Used].m_Offense.m_Rank = pCard->m_Rank;
		// Remove card from hand
		RemoveCard(Seat, pCard);

		if (!NextMoveSoon(Tick))
		{
			// Reset timer to 60 sec
			m_NextMove = 0;
		}
		else if (GetOpenAttacks().size() == 1)
		{
			// Also update timer if attacker waited 59 seconds to place first card.
			// give defender the opportunity to defend, but only 30 sec, dont delay it even more
			m_NextMove = -1;
			m_NextMove = Tick + SERVER_TICK_SPEED * 30;
		}
		return Used;
	}

	bool TryDefend(int Seat, int Attack, CCard *pCard)
	{
		if (Seat != m_DefenderIndex || !pCard || !pCard->Valid())
			return false;

		if (Attack < 0 || Attack >= MAX_DURAK_ATTACKS)
			return false;

		CCard &AttackCard = m_Attacks[Attack].m_Offense;
		if (!AttackCard.Valid() || m_Attacks[Attack].m_Defense.Valid())
			return false;

		if (!pCard->IsStrongerThan(AttackCard, m_Deck.GetTrumpSuit()))
			return false;

		m_Attacks[Attack].m_Defense.m_Suit = pCard->m_Suit;
		m_Attacks[Attack].m_Defense.m_Rank = pCard->m_Rank;
		RemoveCard(Seat, pCard);

		m_NextMove = 0;
		return true;
	}

	int TryPass(int Seat, CCard *pCard)
	{
		if (Seat != m_DefenderIndex || !pCard || !pCard->Valid())
			return -1;

		int NumAttacks = 0;
		int TargetRank = -1;
		for (auto &pair : m_Attacks)
		{
			if (pair.m_Offense.Valid())
			{
				if (pair.m_Defense.Valid()) // No defense has happened already
					return -1;

				NumAttacks++;
				if (TargetRank == -1)
					TargetRank = pair.m_Offense.m_Rank;
				else if (pair.m_Offense.m_Rank != TargetRank)
					return -1; // Can only pass on same ranks
			}
		}

		if (TargetRank == -1 || pCard->m_Rank != TargetRank)
			return -1;

		// Find first free attack slot
		for (int i = 0; i < MAX_DURAK_ATTACKS; i++)
		{
			if (!m_Attacks[i].m_Offense.Valid())
			{
				int NewDefender = GetNextPlayer(m_DefenderIndex, true);
				if (NewDefender == -1 || (int)m_aSeats[NewDefender].m_Player.m_vHandCards.size() < NumAttacks + 1)
					return -1;

				m_Attacks[i].m_Offense.m_Suit = pCard->m_Suit;
				m_Attacks[i].m_Offense.m_Rank = pCard->m_Rank;
				RemoveCard(Seat, pCard);

				// Successfully moved
				m_AttackerIndex = m_DefenderIndex;
				m_DefenderIndex = NewDefender;
				m_NextMove = 0;
				return i;
			}
		}
		return -1;
	}

	void RemoveCard(int Seat, CCard *pCard)
	{
		auto &vHand = m_aSeats[Seat].m_Player.m_vHandCards;
		for (int i = 0; i < (int)vHand.size(); i++)
		{
			if (m_aSeats[Seat].m_Player.m_HoveredCard == i)
			{
				vHand[i].SetHovered(false);
				m_aSeats[Seat].m_Player.m_HoveredCard = -1;
				break;
			}
		}

		vHand.erase(std::remove_if(vHand.begin(), vHand.end(),
			[&](const CCard &c) { return c.m_Suit == pCard->m_Suit && c.m_Rank == pCard->m_Rank; }),
			vHand.end());

		m_aSeats[Seat].m_Player.m_Tooltip = CCard::TOOLTIP_NONE;
		if (m_Deck.IsEmpty() && m_aSeats[Seat].m_Player.m_vHandCards.empty() && std::find(m_vWinners.begin(), m_vWinners.end(), Seat) == m_vWinners.end())
		{
			m_vWinners.push_back(Seat);
		}
	}
};

class CDurak : public CMinigame
{
	enum
	{
		DURAK_TEXT_CARDS_STACK_0 = 0,
		DURAK_TEXT_CARDS_STACK_1,
		DURAK_TEXT_CARDS_STACK_2,
		DURAK_TEXT_CARDS_STACK_3,
		DURAK_TEXT_CARDS_STACK_4,
		DURAK_TEXT_TRUMP_CARD,
		DURAK_TEXT_KEYBOARD_CONTROL,
		DURAK_TEXT_TOOLTIP,
		NUM_DURAK_STATIC_CARDS
	};

	CCard m_aStaticCards[NUM_DURAK_STATIC_CARDS];
	std::map<int, std::map<CCard*, int>> m_aLastSnapID; // [ClientID][Card] = SnapID
	std::map<int, std::map<CCard*, bool>> m_aCardUpdate; // [ClientID][Card] = true
	void PrepareStaticCards(CDurakGame *pGame, CDurakGame::SSeat *pSeat);
	void PrepareDurakSnap(int SnappingClient, CDurakGame *pGame, CDurakGame::SSeat *pSeat);
	void UpdateCardSnapMapping(int SnappingClient, const std::map<CCard *, int> &NewMap, CDurakGame *pGame, bool IsSpectator);
	void SnapDurakCard(int SnappingClient, CDurakGame *pGame, CCard *pCard, bool IsSpectator = false, vec2 ForcePos = vec2(-1, -1));

	int m_aDurakNumReserved[MAX_CLIENTS];
	int64 m_aLastSeatOccupiedMsg[MAX_CLIENTS];
	bool m_aUpdatedPassive[MAX_CLIENTS];
	bool m_aInDurakGame[MAX_CLIENTS];
	bool m_aUpdateTeamsState[MAX_CLIENTS];
	int m_aSnappedSeatIndex[MAX_CLIENTS];
	std::vector<std::pair<int, int64> > m_vLastDuraks; // first: ClientID, second: DurakLoseTick

	std::vector<CDurakGame *> m_vpGames;
	void UpdateGame(int Game);
	bool StartGame(int Game);
	void EndGame(int Game);
	void StartNextRound(int Game, bool SuccessfulDefense = false);
	void SetPlaying(int Game, int Seat);
	void UpdateHandcards(int Game, CDurakGame::SSeat *pSeat);
	void TakeCardsFromTable(int Game);
	void EndMove(int Game, CDurakGame::SSeat *pSeat, bool Force = false);
	bool TryDefend(int Game, int Seat, int Attack, CCard *pCard);
	bool TryPass(int Game, int Seat, CCard *pCard);
	bool TryAttack(int Game, int Seat, CCard *pCard);
	void ProcessCardPlacement(int Game, CDurakGame::SSeat *pSeat, CCard *pFlyingPointToCard);
	void SetTurnTooltip(int Game, int Tooltip);
	void SetNextMoveSoon(int Game);
	void ProcessPlayerWin(int Game, CDurakGame::SSeat *pSeat, int WinPos);
	bool HandleMoneyTransaction(int ClientID, int Amount, const char *pMsg);

	template<typename... Args>
	void ProposeNewStake(int Game, int NotThisID, const char *pFormat, Args&&... args);
	template<typename... Args>
	void SendChatToParticipants(int Game, const char *pFormat, Args&&... args);

	int GetPlayerState(int ClientID);
	const char *GetCardSymbol(int Suit, int Rank, CDurakGame *pGame = 0, int SnappingClient = -1);
	void UpdatePassive(int ClientID, int Seconds);
	void CreateFlyingPoint(int FromClientID, int Game, CCard *pToCard);

	CDurakGame *GetOrAddGame(int Number);

public:
	CDurak(CGameContext *pGameServer, int Type);
	virtual ~CDurak();

	virtual void Tick();
	virtual void Snap(int SnappingClient);
	void PostSnap();

	void AddMapTableTile(int Number, vec2 Pos);
	void AddMapSeatTile(int Number, int MapIndex, int SeatIndex);

	int GetGameByNumber(int Number, bool AllowRunning = false);
	int GetGameByClient(int ClientID);

	int GetTeam(int ClientID, int MapID);
	bool InDurakGame(int ClientID) { return ClientID >= 0 && m_aInDurakGame[ClientID]; }
	bool ActivelyPlaying(int ClientID) { return GetPlayerState(ClientID) != DURAK_PLAYERSTATE_NONE; }
	bool OnDropMoney(int ClientID, int Amount, bool OnDeath);
	bool OnRainbowName(int ClientID, int MapID);
	void OnCharacterSeat(int ClientID, int Number, int SeatIndex);
	void OnCharacterSpawn(class CCharacter *pChr);
	bool TryEnterBetStake(int ClientID, const char *pMessage);
	void OnInput(class CCharacter *pChr, CNetObj_PlayerInput *pNewInput);
	void OnPlayerLeave(int ClientID, bool Disconnect = false, bool Shutdown = false);
	bool OnSetSpectator(int ClientID, int SpectatorID);
	bool IsPlayerOnSeat(int ClientID);
};
#endif // GAME_SERVER_MINIGAMES_DURAK_H
