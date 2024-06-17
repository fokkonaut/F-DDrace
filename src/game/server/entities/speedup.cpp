// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include "speedup.h"

CSpeedup::CSpeedup(CGameWorld *pGameWorld, vec2 Pos, float Angle, int Force, int MaxSpeed, bool Collision)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_SPEEDUP, Pos, 14, Collision)
{
	m_Angle = 0;
	m_Force = Force;
	m_MaxSpeed = MaxSpeed;

	vec2 aOffsets[NUM_DOTS] = {
		vec2(5, 0),
		vec2(2, 3),
		vec2(2, -3),
		vec2(-1, 6),
		vec2(-1, -6),
		vec2(-4, 9),
		vec2(-4, -9)
	};

	for (int i = 0; i < NUM_DOTS; i++)
	{
		m_aDots[i].m_Pos = aOffsets[i];
		m_aDots[i].m_ID = Server()->SnapNewID();
	}

	m_CurrentDist = 0.f;
	m_Distance = length(m_aDots[DOT_END_TOP].m_Pos - m_aDots[DOT_CENTER].m_Pos);

	SetAngle(Angle);
	ResetCollision();
	GameWorld()->InsertEntity(this);
}

CSpeedup::~CSpeedup()
{
	ResetCollision(true);
	for (int i = 0; i < NUM_DOTS; i++)
		Server()->SnapFreeID(m_aDots[i].m_ID);
}

void CSpeedup::ResetCollision(bool Remove)
{
	// For preview, we cant use m_BrushCID here yet because when the entity is created its not set yet
	if (!m_Collision)
		return;

	int Angle = m_Angle;
	int Force = m_Force;
	int MaxSpeed = m_MaxSpeed;
	if (Remove)
	{
		Angle = 0;
		Force = 0;
		MaxSpeed = 0;
		m_Collision = false;
	}
	GameServer()->Collision()->SetSpeedup(m_Pos, Angle, Force, MaxSpeed);
}

void CSpeedup::Rotate(int Angle)
{
	m_Angle += Angle;
	for (int i = 0; i < NUM_DOTS; i++)
		m_aDots[i].Rotate(Angle);
}

void CSpeedup::SetAngle(int Angle)
{
	Rotate(-m_Angle);
	Rotate(Angle);
}

void CSpeedup::Tick()
{
	if (Server()->Tick() % 2 == 0)
	{
		if (m_CurrentDist < m_Distance)
			m_CurrentDist = clamp(m_CurrentDist+3.25f, 0.f, m_Distance);
		else
			m_CurrentDist = 0.f;
	}
}

void CSpeedup::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	CCharacter *pChr = GameServer()->GetPlayerChar(SnappingClient);
	if (pChr && pChr->m_DrawEditor.OnSnapPreview(this))
		return;

	for (int i = 0; i < NUM_DOTS; i++)
	{
		vec2 Pos = m_aDots[i].m_Pos;
		if (Config()->m_SvLightSpeedups && m_BrushCID == -1) // preview should always be stable to see better
		{
			if (i >= 2)
				break;

			int To = NUM_DOTS - 1 - i;
			float Diff = m_CurrentDist / m_Distance;
			Pos.x = mix(m_aDots[To].m_Pos.x, m_aDots[DOT_CENTER].m_Pos.x, Diff);
			Pos.y = mix(m_aDots[To].m_Pos.y, m_aDots[DOT_CENTER].m_Pos.y, Diff);
		}

		CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_aDots[i].m_ID, sizeof(CNetObj_Projectile)));
		if (!pObj)
			return;

		pObj->m_X = round_to_int(m_Pos.x + Pos.x);
		pObj->m_Y = round_to_int(m_Pos.y + Pos.y);
		pObj->m_VelX = 0;
		pObj->m_VelY = 0;
		pObj->m_Type = WEAPON_HAMMER;
		pObj->m_StartTick = 0;
	}
}
