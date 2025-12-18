// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include "playercounter.h"

CPlayerCounter::CPlayerCounter(CGameWorld *pGameWorld, vec2 Pos, int Port)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PLAYER_COUNTER, Pos)
{
	m_Pos = Pos;
	m_Port = Port;
	Update(-1);
	GameWorld()->InsertEntity(this);
}

CPlayerCounter::~CPlayerCounter()
{
	if (m_pLaserText)
	{
		GameWorld()->DestroyEntity(m_pLaserText);
	}
}

void CPlayerCounter::OnUpdate(int Port, int PlayerCount)
{
	if (Port != m_Port)
		return;
	Update(PlayerCount);
}

void CPlayerCounter::Update(int PlayerCount)
{
	m_PlayerCount = PlayerCount;

	if (m_pLaserText)
	{
		GameWorld()->DestroyEntity(m_pLaserText);
	}

	char aBuf[8];
	if (m_PlayerCount == -1)
	{
		str_copy(aBuf, "OFF", sizeof(aBuf));
		m_LastUpdate = 0;
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "%d", m_PlayerCount);
		m_LastUpdate = Server()->Tick();
	}

	int Len = strlen(aBuf);
	vec2 Pos = m_Pos;
	if (Len >= 2)
		Pos.x -= 26.f;
	if (Len >= 3)
		Pos.x -= 26.f;
	m_pLaserText = GameServer()->CreateLaserText(Pos, -1, aBuf, -1, false);
}

void CPlayerCounter::Tick()
{
	if (m_LastUpdate && m_LastUpdate + Server()->TickSpeed() * 65 < Server()->Tick())
	{
		Update(-1);
	}
}

void CPlayerCounter::Snap(int SnappingClient)
{
	// Nothing to do...
}
