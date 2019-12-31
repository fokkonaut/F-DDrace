#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include "character.h"
#include "trail.h"

CTrail::CTrail(CGameWorld *pGameWorld, vec2 Pos, int Owner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_TRAIL, Pos)
{
	m_Owner = Owner;
	m_Pos = Pos;
	GameWorld()->InsertEntity(this);
}

void CTrail::Reset()
{
	Clear();
	GameWorld()->DestroyEntity(this);
}

void CTrail::Clear()
{
	if (!m_TrailProjs.empty())
	{
		for (std::vector<CStableProjectile *>::iterator it = m_TrailProjs.begin(); it != m_TrailProjs.end(); ++it)
		{
			GameWorld()->DestroyEntity(*it);
		}
		m_TrailProjs.clear();
	}
}

void CTrail::Tick()
{
	if (m_Owner != -1)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(m_Owner);
		CPlayer *pPlayer = GameServer()->m_apPlayers[m_Owner];
		if (!pPlayer || (pChr && !pChr->m_Trail && !pPlayer->IsHooked(TRAIL)))
		{
			Reset();
			return;
		}

		if (!pChr)
		{
			Clear();
			return;
		}

		m_Pos = pChr->GetPos();
	}

	if (m_TrailProjs.empty())
	{
		for (int i = 0; i<NUM_TRAILS; i++)
		{
			m_TrailProjs.push_back(new CStableProjectile(GameWorld(), WEAPON_SHOTGUN, m_Owner, vec2(), true));
		}
		m_TrailHistory.clear();
		m_TrailHistory.push_front(HistoryPoint(m_Pos, 0.0f));
		m_TrailHistory.push_front(HistoryPoint(m_Pos, NUM_TRAILS*TRAIL_DIST));
		m_TrailHistoryLength = NUM_TRAILS * TRAIL_DIST;
	}
	vec2 FrontPos = m_TrailHistory.front().m_Pos;
	if (FrontPos != m_Pos)
	{
		float FrontLength = distance(m_Pos, FrontPos);
		m_TrailHistory.push_front(HistoryPoint(m_Pos, FrontLength));
		m_TrailHistoryLength += FrontLength;
	}

	while (1)
	{
		float LastDist = m_TrailHistory.back().m_Dist;
		if (m_TrailHistoryLength - LastDist >= NUM_TRAILS * TRAIL_DIST)
		{
			m_TrailHistory.pop_back();
			m_TrailHistoryLength -= LastDist;
		}
		else
		{
			break;
		}
	}

	int HistoryPos = 0;
	float HistoryPosLength = 0.0f;
	//float AdditionalLength = 0.0f;
	for (int i = 0; i<NUM_TRAILS; i++)
	{
		float Length = (i + 1)*TRAIL_DIST;
		float NextDist = 0.0f;
		while (1)
		{
			// in case floating point arithmetic errors should fuck us up
			// don't crash and recalculate total history length
			if ((unsigned int)HistoryPos >= m_TrailHistory.size())
			{
				m_TrailHistoryLength = 0.0f;
				for (std::deque<HistoryPoint>::iterator it = m_TrailHistory.begin(); it != m_TrailHistory.end(); ++it)
				{
					m_TrailHistoryLength += it->m_Dist;
				}
				break;
			}
			NextDist = m_TrailHistory[HistoryPos].m_Dist;

			if (Length <= HistoryPosLength + NextDist)
			{
				//AdditionalLength = Length - HistoryPosLength;
				break;
			}
			else
			{
				HistoryPos += 1;
				HistoryPosLength += NextDist;
				//AdditionalLength = 0;
			}
		}
		m_TrailProjs[i]->SetPos(m_TrailHistory[HistoryPos].m_Pos);
		//the line under this comment crashed the server, dont know why but it works without that line too since the position gets set above this line too
		//m_TrailProjs[i]->m_Pos += (m_TrailHistory[HistoryPos + 1].m_Pos - m_TrailProjs[i]->m_Pos)*(AdditionalLength / NextDist);
	}
}
