// originally made by Loic KASSEL (Rei/ReiTw)

#include <game/server/gamecontext.h>
#include "epic_circle.h"

CEpicCircle::CEpicCircle(CGameWorld *pGameWorld, vec2 Pos, int Owner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE, Pos)
{
	m_Owner = Owner;
	m_TeamMask = Mask128();

	for(int i = 0; i < MAX_PARTICLES; i ++)
		m_aIDs[i] = Server()->SnapNewID();
	GameWorld()->InsertEntity(this);
}

void CEpicCircle::Reset()
{
	for(int i = 0; i < MAX_PARTICLES; i++)
		Server()->SnapFreeID(m_aIDs[i]);
	GameWorld()->DestroyEntity(this);
}

void CEpicCircle::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner || !pOwner->m_EpicCircle)
	{
		Reset();
		return;
	}

	m_Pos = pOwner->GetPos();
	m_TeamMask = pOwner->TeamMask();

	for(int i = 0; i < MAX_PARTICLES; i++)
	{
		float rad = 16.0f * powf(sinf(Server()->Tick() / 30.0f), 3) * 1 + 50;
		float TurnFac = 0.025f;
		m_RotatePos[i].x = cosf(2 * pi * (i / (float)MAX_PARTICLES) + Server()->Tick()*TurnFac) * rad;
		m_RotatePos[i].y = sinf(2 * pi * (i / (float)MAX_PARTICLES) + Server()->Tick()*TurnFac) * rad;
	}
}

void CEpicCircle::Snap(int SnappingClient)
{   
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if (!CmaskIsSet(m_TeamMask, SnappingClient) || (pOwner && pOwner->IsPaused()))
		return;

	CNetObj_Projectile *pParticle[MAX_PARTICLES];
	for (int i = 0; i < MAX_PARTICLES; i++)
	{
		pParticle[i] = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_aIDs[i], sizeof(CNetObj_Projectile)));
		if (pParticle[i])
		{
			pParticle[i]->m_X = m_Pos.x + m_RotatePos[i].x;
			pParticle[i]->m_Y = m_Pos.y + m_RotatePos[i].y;
			pParticle[i]->m_VelX = 0;
			pParticle[i]->m_VelY = 0;
			pParticle[i]->m_StartTick = 0;
			pParticle[i]->m_Type = WEAPON_HAMMER;
		}
	}
}
