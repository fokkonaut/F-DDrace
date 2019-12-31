#include <game/server/gamecontext.h>
#include "atom.h"
#include "character.h"
#include <game/server/player.h>

#define _USE_MATH_DEFINES
#include <math.h>

CAtom::CAtom(CGameWorld *pGameWorld, vec2 Pos, int Owner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_ATOM, Pos)
{
	m_Owner = Owner;
	m_Pos = Pos;
	GameWorld()->InsertEntity(this);
}

void CAtom::Reset()
{
	Clear();
	GameWorld()->DestroyEntity(this);
}

void CAtom::Clear()
{
	if (!m_AtomProjs.empty())
	{
		for (std::vector<CStableProjectile *>::iterator it = m_AtomProjs.begin(); it != m_AtomProjs.end(); ++it)
		{
			GameWorld()->DestroyEntity(*it);
		}
		m_AtomProjs.clear();
	}
}

void CAtom::Tick()
{
	if (m_Owner != -1)
	{
		CCharacter* pChr = GameServer()->GetPlayerChar(m_Owner);
		CPlayer *pPlayer = GameServer()->m_apPlayers[m_Owner];
		if (!pPlayer || (pChr && !pChr->m_Atom && !pPlayer->IsHooked(ATOM)))
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

	if (m_AtomProjs.empty())
	{
		for (int i = 0; i<NUM_ATOMS; i++)
		{
			m_AtomProjs.push_back(new CStableProjectile(GameWorld(), i % 2 ? WEAPON_GRENADE : WEAPON_SHOTGUN, m_Owner, vec2(), true));
		}
		m_AtomPosition = 0;
	}
	if (++m_AtomPosition >= 60)
	{
		m_AtomPosition = 0;
	}
	vec2 AtomPos;
	AtomPos.x = m_Pos.x + 200 * cos(m_AtomPosition*M_PI * 2 / 60);
	AtomPos.y = m_Pos.y + 80 * sin(m_AtomPosition*M_PI * 2 / 60);
	for (int i = 0; i<NUM_ATOMS; i++)
	{
		m_AtomProjs[i]->SetPos(rotate_around_point(AtomPos, m_Pos, i * M_PI * 2 / NUM_ATOMS));
	}
}
