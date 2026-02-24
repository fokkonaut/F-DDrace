/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <engine/shared/config.h>
#include <game/server/gamemodes/DDRace.h>
#include "character.h"

#include "door.h"

CDoor::CDoor(CGameWorld *pGameWorld, vec2 Pos, float Rotation, int Length,
		int Number, bool Collision, int Thickness, int Color) :
		CEntity(pGameWorld, CGameWorld::ENTTYPE_DOOR, Pos, 0, Collision)
{
	m_Number = Number;
	m_Pos = Pos;
	m_PrevPos = m_Pos;
	m_Thickness = Thickness;
	m_Length = Length;
	m_Color = Color;

	SetDirection(Rotation);
	ResetCollision();
	GameWorld()->InsertEntity(this);
}

CDoor::~CDoor()
{
	ResetCollision(true);
}

bool CDoor::GetIntersectPos(vec2 Pos0, vec2 Pos1, float Radius, vec2 *pOutPosIntersected)
{
	float d = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	for (float f = 0; f < d; f++)
	{
		float a = f / d;
		vec2 Pos = mix(Pos0, Pos1, a);

		float Dist = distance(Pos, m_Pos);
		if (Dist < m_Length)
		{
			vec2 ClosestPoint;
			if (closest_point_on_line(m_Pos, m_To, Pos, ClosestPoint))
			{
				Dist = distance(Pos, ClosestPoint);
				if (Dist < Radius)
				{
					if (pOutPosIntersected)
						*pOutPosIntersected = Pos;
					return true;
				}
			}
		}
		Last = Pos;
	}
	if (pOutPosIntersected)
		*pOutPosIntersected = Pos1;
	return false;
}

void CDoor::SetDirection(float Rotation)
{
	m_Rotation = Rotation;
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
	GameServer()->Collision()->IntersectNoLaser(m_Pos, To, &this->m_To, 0, m_Number);
}

void CDoor::Open(int Tick, bool ActivatedTeam[])
{
	m_EvalTick = Server()->Tick();
}

void CDoor::ResetCollision(bool Remove)
{
	if (!m_Collision)
		return;

	int Length = max(m_Length - 1, 1); // make sure to always set at least the one tile
	for (int i = 0; i < Length; i++)
	{
		vec2 CurrentPos(m_Pos.x + (m_Direction.x * i),
				m_Pos.y + (m_Direction.y * i));

		bool PlotDoor = GameServer()->Collision()->CheckPointDoor(CurrentPos, 0, true, false) != -1 && !GameServer()->Collision()->IsPlotDoor(m_Number); // extra check so plot doors dont invalidate theirselves

		if (GameServer()->Collision()->CheckPoint(CurrentPos)
				|| GameServer()->Collision()->GetTile(CurrentPos.x, CurrentPos.y)
				|| GameServer()->Collision()->GetFTile(CurrentPos.x, CurrentPos.y)
				|| PlotDoor)
		{
			break;
		}
		else
		{
			int Index = GameServer()->Collision()->GetPureMapIndex(CurrentPos);
			if (Remove)
			{
				GameServer()->Collision()->RemoveDoorTile(Index, TILE_STOPA, m_Number);
				m_Collision = false;
			}
			else
				GameServer()->Collision()->AddDoorTile(Index, TILE_STOPA, m_Number);
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
	if (NetworkClipped(SnappingClient, m_Pos) && NetworkClipped(SnappingClient, m_To))
		return;

	CCharacter *pChr = GameServer()->GetPlayerChar(SnappingClient);
	if (pChr && pChr->m_DrawEditor.OnSnapPreview(this))
		return;

	// plot id 0 = free draw, dont hide objects on normal plots. Always show objects when editing currently
	if (SnappingClient >= 0 && GameServer()->m_apPlayers[SnappingClient]->m_HideDrawings && m_PlotID == 0 && !m_Collision
		&& (!GameServer()->GetPlayerChar(SnappingClient) || GameServer()->GetPlayerChar(SnappingClient)->GetActiveWeapon() != WEAPON_DRAW_EDITOR))
		return;

	bool ForceOn = pChr && pChr->GetActiveWeapon() == WEAPON_DRAW_EDITOR && GameServer()->Collision()->IsPlotDrawDoor(m_Number);

	CNetObj_EntityEx *pEntData = 0;
	if (m_Collision && !ForceOn)
	{
		if (pChr && pChr->SendExtendedEntity(this) && m_Color == LASERTYPE_DOOR) // REMOVE THIS COLOR CHECK ONCE https://github.com/ddnet/ddnet/pull/5886 got merged
		{
			pEntData = static_cast<CNetObj_EntityEx *>(Server()->SnapNewItem(NETOBJTYPE_ENTITYEX, GetID(), sizeof(CNetObj_EntityEx)));
			if (pEntData)
			{
				pEntData->m_SwitchNumber = m_Number;
				pEntData->m_Layer = m_Layer;
				pEntData->m_EntityClass = ENTITYCLASS_DOOR;
			}
		}
	}

	// Build the object
	vec2 From = m_To;
	int StartTick = 0;

	if (!pEntData)
	{
		if (SnappingClient > -1 && (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == -1 || GameServer()->m_apPlayers[SnappingClient]->IsPaused()) && GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID() != -1)
			pChr = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID());

		if (!m_Collision || (pChr && pChr->Team() != TEAM_SUPER && pChr->IsAlive() && (ForceOn || (GameServer()->Collision()->GetNumAllSwitchers() > 0 && GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[pChr->Team()]))))
			From = m_To;
		else
			From = m_Pos;
		StartTick = Server()->Tick() - 4 + m_Thickness;
	}

	int LaserFlags = 0;
	if (!m_Collision)
		LaserFlags = LASERFLAG_NO_PREDICT;

	int SnappingClientVersion = GameServer()->GetClientDDNetVersion(SnappingClient);
	GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion, Server()->IsSevendown(SnappingClient), SnappingClient), GetID(),
		m_Pos, From, StartTick, -1, m_Color, 0, m_Number, LaserFlags);
}
