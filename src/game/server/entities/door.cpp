/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <engine/shared/config.h>
#include <game/server/gamemodes/DDRace.h>
#include "character.h"

#include "door.h"

CDoor::CDoor(CGameWorld *pGameWorld, vec2 Pos, float Rotation, int Length,
		int Number, bool Collision) :
		CEntity(pGameWorld, CGameWorld::ENTTYPE_DOOR, Pos)
{
	m_Number = Number;
	m_Pos = Pos;
	m_PrevPos = m_Pos;
	m_Collision = Collision;
	m_Length = Length;
	
	SetDirection(Rotation);
	ResetCollision();
	GameWorld()->InsertEntity(this);
}

CDoor::~CDoor()
{
	ResetCollision(true);
}

void CDoor::SetDirection(float Rotation)
{
	m_Direction = vec2(sin(Rotation), cos(Rotation));
	Update();
}

void CDoor::SetLength(int Length)
{
	m_Length = Length;
	Update();
}

void CDoor::Update()
{
	vec2 To = m_Pos + normalize(m_Direction) * m_Length;
	GameServer()->Collision()->IntersectNoLaser(m_Pos, To, &this->m_To, 0);
}

void CDoor::Open(int Tick, bool ActivatedTeam[])
{
	m_EvalTick = Server()->Tick();
}

void CDoor::ResetCollision(bool Remove)
{
	for (int i = 0; i < m_Length - 1; i++)
	{
		vec2 CurrentPos(m_Pos.x + (m_Direction.x * i),
				m_Pos.y + (m_Direction.y * i));
		if (GameServer()->Collision()->CheckPoint(CurrentPos)
				|| GameServer()->Collision()->GetTile(CurrentPos.x, CurrentPos.y)
				|| GameServer()->Collision()->GetFTile(CurrentPos.x, CurrentPos.y))
		{
			break;
		}
		else if (m_Collision)
		{
			if (!Remove)
				GameServer()->Collision()->SetDCollisionAt(CurrentPos.x, CurrentPos.y, TILE_STOPA, 0/*Flags*/, m_Number);
			else
				GameServer()->Collision()->UnsetDCollisionAt(CurrentPos.x, CurrentPos.y);
		}
	}
}

void CDoor::Open(int Team)
{

}

void CDoor::Close(int Team)
{

}

void CDoor::Reset()
{

}

void CDoor::Tick()
{
	if (m_PrevPos != m_Pos)
		Update();
	m_PrevPos = m_Pos;
}

void CDoor::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient, m_Pos)
			&& NetworkClipped(SnappingClient, m_To))
		return;

	if (m_BrushCID != -1)
	{
		CCharacter *pBrushChr = GameServer()->GetPlayerChar(m_BrushCID);
		if (pBrushChr && pBrushChr->m_DrawEditor.OnSnapPreview(SnappingClient))
			return;
	}

	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(
			NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));

	if (!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;

	CCharacter * Char = GameServer()->GetPlayerChar(SnappingClient);
	int Tick = (Server()->Tick() % Server()->TickSpeed()) % 11;

	if(SnappingClient > -1 && (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == TEAM_SPECTATORS
				|| GameServer()->m_apPlayers[SnappingClient]->IsPaused())
			&& GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID() != -1)
		Char = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID());

	if (Char == 0)
		return;

	if (Char->IsAlive() && GameServer()->Collision()->m_NumSwitchers > 0
			&& !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[Char->Team()]
			&& (!Tick))
		return;

	if (Char->Team() == TEAM_SUPER)
	{
		pObj->m_FromX = (int)m_Pos.x;
		pObj->m_FromY = (int)m_Pos.y;
	}
	else if (Char->IsAlive() && GameServer()->Collision()->m_NumSwitchers > 0
		&& GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[Char->Team()])
	{
		pObj->m_FromX = (int)m_To.x;
		pObj->m_FromY = (int)m_To.y;
	}
	else
	{
		pObj->m_FromX = (int)m_Pos.x;
		pObj->m_FromY = (int)m_Pos.y;
	}
	pObj->m_StartTick = Server()->Tick();
}
