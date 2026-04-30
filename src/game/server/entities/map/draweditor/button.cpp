// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include "button.h"

CButton::CButton(CGameWorld *pGameWorld, vec2 Pos, int Number, bool Collision)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_BUTTON, Pos, 14, Collision)
{
	m_Number = Number;

	vec2 aOffsets[NUM_SIDES] = {
		vec2(0, -16),
		vec2(16, 0),
		vec2(0, 16),
		vec2(-16, 0)
	};

	for (int i = 0; i < NUM_SIDES; i++)
	{
		m_aSides[i].m_Pos = aOffsets[i];
		m_aSides[i].m_ID = Server()->SnapNewID();
	}

	ResetCollision();
	GameWorld()->InsertEntity(this);
}

CButton::~CButton()
{
	ResetCollision(true);
	for (int i = 0; i < NUM_SIDES; i++)
		Server()->SnapFreeID(m_aSides[i].m_ID);
}

void CButton::ResetCollision(bool Remove)
{
	// For preview, we cant use m_BrushCID here yet because when the entity is created its not set yet
	if (!m_Collision)
		return;

	int Index = GameServer()->Collision()->GetPureMapIndex(m_Pos);
	if (Remove)
	{
		GameServer()->Collision()->RemoveDoorTile(Index, TILE_SWITCHTOGGLE, m_Number);
		m_Collision = false;
	}
	else
		GameServer()->Collision()->AddDoorTile(Index, TILE_SWITCHTOGGLE, m_Number);
}

void CButton::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	CCharacter *pChr = GameServer()->GetPlayerChar(SnappingClient);
	if (pChr && pChr->m_DrawEditor.OnSnapPreview(this))
		return;

	bool Status = pChr && pChr->GetActiveWeapon() == WEAPON_DRAW_EDITOR && GameServer()->Collision()->IsPlotDrawDoor(m_Number);

	if (!Status)
	{
		if (SnappingClient > -1 && (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == -1 || GameServer()->m_apPlayers[SnappingClient]->IsPaused()) && GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID() != -1)
			pChr = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID());

		Status = GameServer()->Collision()->m_pSwitchers && pChr && pChr->Team() != TEAM_SUPER && GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[pChr->Team()];
	}

	if (!Status && (Server()->Tick() % Server()->TickSpeed()) % 11 == 0)
		return;

	int SnappingClientVersion = GameServer()->GetClientDDNetVersion(SnappingClient);
	GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion, Server()->IsSevendown(SnappingClient), SnappingClient), GetID(),
		m_Pos, m_Pos, 0, -1, LASERTYPE_RIFLE, -1, m_Number, LASERFLAG_NO_PREDICT);

	if (!Status)
		return;

	for (int i = 0; i < NUM_SIDES; i++)
	{
		int To = i == POINT_LEFT ? POINT_TOP : i+1;

		vec2 Pos = m_Pos + m_aSides[i].m_Pos;
		vec2 From = m_Pos + m_aSides[To].m_Pos;
		GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion, Server()->IsSevendown(SnappingClient), SnappingClient), m_aSides[i].m_ID,
			Pos, From, Server()->Tick(), -1, LASERTYPE_RIFLE, -1, m_Number, LASERFLAG_NO_PREDICT);
	}
}
