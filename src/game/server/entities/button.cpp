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

	if(GameServer()->GetClientDDNetVersion(SnappingClient) >= VERSION_DDNET_MULTI_LASER)
	{
		CNetObj_DDNetLaser *pButton = static_cast<CNetObj_DDNetLaser *>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, GetID(), sizeof(CNetObj_DDNetLaser)));
		if(!pButton)
			return;

		pButton->m_ToX = round_to_int(m_Pos.x);
		pButton->m_ToY = round_to_int(m_Pos.y);
		pButton->m_FromX = round_to_int(m_Pos.x);
		pButton->m_FromY = round_to_int(m_Pos.y);
		pButton->m_StartTick = 0;
		pButton->m_Owner = -1;
		pButton->m_Type = LASERTYPE_DOOR;
	}
	else
	{
		CNetObj_Laser *pButton = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
		if (!pButton)
			return;

		pButton->m_X = round_to_int(m_Pos.x);
		pButton->m_Y = round_to_int(m_Pos.y);
		pButton->m_FromX = round_to_int(m_Pos.x);
		pButton->m_FromY = round_to_int(m_Pos.y);
		pButton->m_StartTick = 0;
	}

	if (!Status)
		return;

	for (int i = 0; i < NUM_SIDES; i++)
	{
		int To = i == POINT_LEFT ? POINT_TOP : i+1;

		if(GameServer()->GetClientDDNetVersion(SnappingClient) >= VERSION_DDNET_MULTI_LASER)
		{
			CNetObj_DDNetLaser * pSide = static_cast<CNetObj_DDNetLaser *>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, m_aSides[i].m_ID, sizeof(CNetObj_DDNetLaser)));
			if(!pSide)
				return;

			pSide->m_ToX = round_to_int(m_Pos.x + m_aSides[i].m_Pos.x);
			pSide->m_ToY = round_to_int(m_Pos.y + m_aSides[i].m_Pos.y);
			pSide->m_FromX = round_to_int(m_Pos.x + m_aSides[To].m_Pos.x);
			pSide->m_FromY = round_to_int(m_Pos.y + m_aSides[To].m_Pos.y);
			pSide->m_StartTick = Server()->Tick();
			pSide->m_Owner = -1;
			pSide->m_Type = LASERTYPE_DOOR;
		}
		else
		{
			CNetObj_Laser *pSide = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_aSides[i].m_ID, sizeof(CNetObj_Laser)));
			if (!pSide)
				return;

			pSide->m_X = round_to_int(m_Pos.x + m_aSides[i].m_Pos.x);
			pSide->m_Y = round_to_int(m_Pos.y + m_aSides[i].m_Pos.y);
			pSide->m_FromX = round_to_int(m_Pos.x + m_aSides[To].m_Pos.x);
			pSide->m_FromY = round_to_int(m_Pos.y + m_aSides[To].m_Pos.y);
			pSide->m_StartTick = Server()->Tick();
		}
	}
}
