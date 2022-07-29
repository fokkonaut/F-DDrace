#include <game/server/gamecontext.h>
#include "lovely.h"
#include "character.h"
#include <game/server/player.h>

CLovely::CLovely(CGameWorld *pGameWorld, vec2 Pos, int Owner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LOVELY, Pos)
{
	m_Owner = Owner;
	m_SpawnDelay = 0;
	GameWorld()->InsertEntity(this);
}

void CLovely::Reset()
{
	for (unsigned int i = 0; i < m_vLovelyData.size(); i++)
		Server()->SnapFreeID(m_vLovelyData[i].m_ID);
	m_vLovelyData.clear();
	GameWorld()->DestroyEntity(this);
}

void CLovely::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if (!pOwner || !pOwner->m_Lovely)
	{
		Reset();
		return;
	}

	m_Pos = pOwner->GetPos();

	m_SpawnDelay--;
	if (!m_vLovelyData.size() || m_SpawnDelay <= 0)
	{
		SpawnNewHeart();
		int SpawnTime = 45;
		m_SpawnDelay = Server()->TickSpeed() - (rand() % (SpawnTime - (SpawnTime - 10) + 1) + (SpawnTime - 10));
	}

	for (unsigned int i = 0; i < m_vLovelyData.size(); i++)
	{
		m_vLovelyData[i].m_Lifespan--;
		m_vLovelyData[i].m_Pos.y -= 5.f;

		if (m_vLovelyData[i].m_Lifespan <= 0 || GameServer()->Collision()->TestBox(m_vLovelyData[i].m_Pos, vec2(14.f, 14.f)))
		{
			Server()->SnapFreeID(m_vLovelyData[i].m_ID);
			m_vLovelyData.erase(m_vLovelyData.begin() + i);
			i--;
		}
	}
}

void CLovely::SpawnNewHeart()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	SLovelyData Data;
	Data.m_ID = Server()->SnapNewID();
	Data.m_Lifespan = Server()->TickSpeed() / 2;
	Data.m_Pos = vec2(pOwner->GetPos().x + (rand() % 50 - 25), pOwner->GetPos().y - 30);
	m_vLovelyData.push_back(Data);
	pOwner->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed() * 2);
}

void CLovely::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	for (unsigned int i = 0; i < m_vLovelyData.size(); i++)
	{
		int Size = Server()->IsSevendown(SnappingClient) ? 4*4 : sizeof(CNetObj_Pickup);
		CNetObj_Pickup* pP = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_vLovelyData[i].m_ID, Size));
		if (!pP)
			return;

		pP->m_X = round_to_int(m_vLovelyData[i].m_Pos.x);
		pP->m_Y = round_to_int(m_vLovelyData[i].m_Pos.y);
		if (Server()->IsSevendown(SnappingClient))
		{
			pP->m_Type = POWERUP_HEALTH;
			((int*)pP)[3] = 0;
		}
		else
			pP->m_Type = POWERUP_HEALTH;
	}
}
