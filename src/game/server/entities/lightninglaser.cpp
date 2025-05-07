// originally made by Loic KASSEL (Rei/ReiTw), updated by fokkonaut

#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>
#include "lightninglaser.h"

CLightningLaser::CLightningLaser(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, int Owner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LIGHTNING_LASER, Pos)
{
	m_Pos = Pos;
	m_Owner = Owner;
	m_Dir = Direction;
	m_StartTick = -2.5f;

	m_Lifespan  = m_StartLifespan = Server()->TickSpeed() / 5; // timer for the laser

	int TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Pos));
	m_Count = max(1, (int)GameServer()->TuningFromChrOrZone(m_Owner, TuneZone)->m_LightningLaserCount);
	m_Length = max(1, (int)GameServer()->TuningFromChrOrZone(m_Owner, TuneZone)->m_LightningLaserLength);

	m_aIDs = (int *)calloc(sizeof(int), m_Count);
	m_aaPositions = (vec2 **)calloc(sizeof(vec2 *), m_Count);
	for (int i = 0; i < m_Count; i++)
		m_aaPositions[i] = (vec2 *)calloc(POS_COUNT, sizeof(vec2));
	
	m_aaPositions[0][POS_START] = Pos;
	
	for(int i = 0; i < m_Count; i++)
		m_aIDs[i] = Server()->SnapNewID();

	InitTarget(); // just in case there's a target
	GenerateLights(); // we generate position of lights

	GameWorld()->InsertEntity(this);
}

CLightningLaser::~CLightningLaser()
{
	free(m_aIDs);
	for (int i = 0; i < m_Count; i++)
		free(m_aaPositions[i]);
	free(m_aaPositions);
}

void CLightningLaser::Reset()
{
	GameWorld()->DestroyEntity(this);
	for(int i = 0; i < m_Count; i ++)
		Server()->SnapFreeID(m_aIDs[i]);
}

void CLightningLaser::InitTarget()
{
	m_Target.Reset();

	const float DetectAngle = 90.f;
	float OwnAngle = GetAngle(m_Dir) * 180 / pi + 75.f;
	float ClosestDist = 0;

	for(int i = 0; i < MAX_CLIENTS; i ++)
	{
		if(i == m_Owner || !GameServer()->GetPlayerChar(i))
			continue;

		float Dist = distance(m_Pos, GameServer()->GetPlayerChar(i)->GetPos());

		if (Dist > m_Count * m_Length)
			continue;

		if(m_Target.m_ID == -1 || Dist < ClosestDist)
		{
			float TargetAngle = GetAngle(GameServer()->GetPlayerChar(i)->GetPos() - m_Pos) * 180 / pi + 90.f;
			
			if(/*!GameServer()->Collision()->IntersectLine(m_Pos, GameServer()->GetPlayerChar(i)->m_Pos, NULL, NULL) &&*/
				((OwnAngle - TargetAngle < DetectAngle && OwnAngle - TargetAngle > -DetectAngle) ||
				(TargetAngle - OwnAngle < DetectAngle && TargetAngle - OwnAngle > -DetectAngle)))
			{
				m_Target.m_IsAlive = true;
				m_Target.m_ID = i;
				m_Target.m_Pos = GameServer()->GetPlayerChar(i)->GetPos();	
				ClosestDist = Dist;
			}
		}
	}
}

void CLightningLaser::UpdateDirection(vec2 From)
{
	if(TargetAlive())
		m_Dir = normalize(m_Target.m_Pos - From);
	else
		m_Target.m_ID = -1;
}

bool CLightningLaser::TargetBehindWall(vec2 From)
{
	return TargetAlive() && GameServer()->Collision()->IntersectLine(From, GameServer()->GetPlayerChar(m_Target.m_ID)->GetPos(), NULL, NULL);
}

void CLightningLaser::GenerateLights()
{
	float randShot;

	for(int i = 0; i < m_Count; i ++)
	{
		m_aaPositions[i][POS_START] = i != 0 ? m_aaPositions[i - 1][POS_END] : m_Pos; 

		if(!TargetBehindWall(m_aaPositions[i][POS_START]))
			UpdateDirection(m_aaPositions[i][POS_START]);

		int randDir = m_Target.m_ID != -1 ? (rand() % 80 - 40) : (rand() % 140 - 70);

		randShot = GetAngle(m_Dir) + randDir * pi / 180.f;
				
		m_aaPositions[i][POS_END] = m_aaPositions[i][POS_START] + GetDir(randShot) * m_Length;

		vec2 CollisionPos;
		if (GameServer()->Collision()->IntersectLine(m_aaPositions[i][POS_START], m_aaPositions[i][POS_END], NULL, &CollisionPos)) 
		{
	        // if here is a wall, then don't go through the wall
			m_aaPositions[i][POS_END] = CollisionPos;
		}
	}
}

void CLightningLaser::HitCharacter()
{
	for(int i = 1; i < m_Count; i ++)
	{
		for(int j = 0; j < MAX_CLIENTS; j++)
		{
			CCharacter *pChrs = GameServer()->GetPlayerChar(j);

			if(!pChrs || j == m_Owner)
				continue;

			// closest point in the light near of a tee
			vec2 Point;
			if (closest_point_on_line(m_aaPositions[i][POS_END], m_aaPositions[i][POS_START], pChrs->GetPos(), Point))
			{
				bool IntersectLine = GameServer()->Collision()->IntersectLine(m_aaPositions[i][POS_END], pChrs->GetPos(), NULL, NULL);

				// hit
				if(distance(Point, pChrs->GetPos()) <= m_Length/2 && !IntersectLine)
					m_aaPositions[i][POS_END] = pChrs->GetPos();

				// put next laser to -1, better.
				if(m_aaPositions[i-1][POS_END] == pChrs->GetPos())
					m_aaPositions[i][POS_END] = m_aaPositions[i][POS_START] = m_aaPositions[i-1][POS_END]; 

				if(distance(Point, pChrs->GetPos()) < 10)
				{
					// freeze if the player is near the light
					pChrs->Freeze(3);
					pChrs->Core()->m_Vel = vec2(0.f, 0.f);
				}
			}
		}
	}
}

void CLightningLaser::Tick()
{
	m_Lifespan--;

	if(m_Lifespan <= 0)
	{
		Reset();
		return;
	}
	else if(m_Lifespan <= 3)
	{
		m_StartTick--;
	}

	HitCharacter();
}

void CLightningLaser::Snap(int SnappingClient)
{
	// distance between the character & the owner
	if(NetworkClipped(SnappingClient, m_aaPositions[0][POS_START]))
		return;

	float Percentage = 100.f - (m_Lifespan * 100.f / m_StartLifespan);

	int Start = (int)max((float)ceil(Percentage * m_Count / 100.f), 1.f);

	if(GameServer()->GetClientDDNetVersion(SnappingClient) >= VERSION_DDNET_MULTI_LASER)
	{
		CNetObj_DDNetLaser **apObjs = (CNetObj_DDNetLaser **)calloc(sizeof(CNetObj_DDNetLaser *), m_Count);
		for(int i = Start - 1; i >= 0; i--)
		{
			apObjs[i] = static_cast<CNetObj_DDNetLaser *>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, m_aIDs[i], sizeof(CNetObj_DDNetLaser)));
			if(!apObjs[i])
				return;

			// POS_END and POS_START reversed
			apObjs[i]->m_ToX = (int)m_aaPositions[i][POS_START].x;
			apObjs[i]->m_ToY = (int)m_aaPositions[i][POS_START].y;
			apObjs[i]->m_FromX = (int)m_aaPositions[i][POS_END].x;
			apObjs[i]->m_FromY = (int)m_aaPositions[i][POS_END].y;
			apObjs[i]->m_StartTick = Server()->Tick() + m_StartTick;
			apObjs[i]->m_Owner = m_Owner;
			apObjs[i]->m_Type = LASERTYPE_FREEZE;
		}
		free(apObjs);
	}
	else
	{
		CNetObj_Laser **apObjs = (CNetObj_Laser **)calloc(sizeof(CNetObj_Laser *), m_Count);
		for(int i = Start - 1; i >= 0; i--)
		{
			apObjs[i] = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_aIDs[i], sizeof(CNetObj_Laser)));
			if(!apObjs[i])
				return;

			// POS_END and POS_START reversed
			apObjs[i]->m_X = (int)m_aaPositions[i][POS_START].x;
			apObjs[i]->m_Y = (int)m_aaPositions[i][POS_START].y;
			apObjs[i]->m_FromX = (int)m_aaPositions[i][POS_END].x;
			apObjs[i]->m_FromY = (int)m_aaPositions[i][POS_END].y;
			apObjs[i]->m_StartTick = Server()->Tick() + m_StartTick;
		}
		free(apObjs);
	}
}
