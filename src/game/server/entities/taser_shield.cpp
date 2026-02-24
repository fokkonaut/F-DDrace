// made by fokkonaut

#include <game/server/gamecontext.h>
#include "taser_shield.h"
#include "character.h"
#include <game/server/player.h>

CTaserShield::CTaserShield(CGameWorld *pGameWorld, vec2 Pos, int Owner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_TASER_SHIELD, Pos)
{
	m_Owner = Owner;
	m_SpawnDelay = 0;
	m_TeamMask = Mask128();
	for (int i = 0; i < MAX_SHIELDS; i++)
		m_aShieldData[i].m_ID = Server()->SnapNewID();
	GameWorld()->InsertEntity(this);
}

void CTaserShield::Reset()
{
	for (int i = 0; i < MAX_SHIELDS; i++)
		Server()->SnapFreeID(m_aShieldData[i].m_ID);
	GameWorld()->DestroyEntity(this);
}

void CTaserShield::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if (!pOwner)
	{
		Reset();
		return;
	}

	m_Pos = pOwner->GetPos();
	m_TeamMask = pOwner->TeamMask();

	m_SpawnDelay--;
	if (m_SpawnDelay <= 0)
	{
		bool AllUsed = true;
		for (int i = 0; i < MAX_SHIELDS; i++)
			if (!m_aShieldData[i].m_Used || m_aShieldData[i].m_Lifespan != -1)
				AllUsed = false;
		if (AllUsed)
		{
			MarkForDestroy();
			return;
		}

		SpawnNewShield();
		int SpawnTime = 45;
		m_SpawnDelay = Server()->TickSpeed() - (rand() % (SpawnTime - (SpawnTime - 5) + 1) + (SpawnTime - 5));
	}

	for (int i = 0; i < MAX_SHIELDS; i++)
	{
		if (m_aShieldData[i].m_Lifespan == -1)
			continue;

		m_aShieldData[i].m_Lifespan--;
		m_aShieldData[i].m_Pos.y -= 5.f;

		if (m_aShieldData[i].m_Lifespan == 0 || GameServer()->Collision()->TestBox(m_aShieldData[i].m_Pos, vec2(14.f, 14.f)))
			m_aShieldData[i].m_Lifespan = -1;
	}
}

void CTaserShield::SpawnNewShield()
{
	for (int i = 0; i < MAX_SHIELDS; i++)
	{
		if (m_aShieldData[i].m_Lifespan > 0 || m_aShieldData[i].m_Used)
			continue;

		CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
		m_aShieldData[i].m_Lifespan = Server()->TickSpeed() / 2;
		m_aShieldData[i].m_Pos = vec2(pOwner->GetPos().x + (rand() % 50 - 25), pOwner->GetPos().y - 30);
		m_aShieldData[i].m_Used = true;
		pOwner->SetEmote(EMOTE_PAIN, Server()->Tick() + Server()->TickSpeed());
		GameServer()->CreateSound(m_aShieldData[i].m_Pos, SOUND_PICKUP_ARMOR, m_TeamMask);
		break;
	}
}

void CTaserShield::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if (!CmaskIsSet(m_TeamMask, SnappingClient) || (pOwner && pOwner->IsPaused()))
		return;

	int SnappingClientVersion = GameServer()->GetClientDDNetVersion(SnappingClient);
	CSnapContext Context(SnappingClientVersion, Server()->IsSevendown(SnappingClient), SnappingClient);

	for (int i = 0; i < MAX_SHIELDS; i++)
	{
		if (m_aShieldData[i].m_Lifespan == -1)
			continue;
		GameServer()->SnapPickupObject(Context, m_aShieldData[i].m_ID, m_aShieldData[i].m_Pos, POWERUP_ARMOR, 0, -1, PICKUPFLAG_NO_PREDICT);
	}
}
