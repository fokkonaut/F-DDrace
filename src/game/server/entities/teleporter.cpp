// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>
#include "teleporter.h"

CTeleporter::CTeleporter(CGameWorld *pGameWorld, vec2 Pos, int Type, int Number, bool Collision)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_TELEPORTER, Pos, 14)
{
	m_Type = Type;
	m_Number = Number;
	m_Collision = Collision;

	ResetCollision();
	GameWorld()->InsertEntity(this);
}

CTeleporter::~CTeleporter()
{
	ResetCollision(true);
}

void CTeleporter::ResetCollision(bool Remove)
{
	// For preview, we cant use m_BrushCID here yet because when the entity is created its not set yet
	if (!m_Collision)
		return;

	CGameControllerDDRace *pController = (CGameControllerDDRace *)GameServer()->m_pController;
	if (!pController)
		return; // when server reloads or shuts down

	int Type = m_Type;

	if (Remove)
	{
		Type = 0;
		for (unsigned int i = 0; i < pController->m_TeleOuts[m_Number - 1].size(); i++)
		{
			if (pController->m_TeleOuts[m_Number - 1][i] == m_Pos)
			{
				pController->m_TeleOuts[m_Number - 1].erase(pController->m_TeleOuts[m_Number - 1].begin() + i);
				break;
			}
		}
	}
	else
	{
		pController->m_TeleOuts[m_Number - 1].push_back(m_Pos);
	}

	GameServer()->Collision()->SetTeleporter(m_Pos, Type, m_Number);
}

void CTeleporter::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	if (m_BrushCID != -1)
	{
		CCharacter *pBrushChr = GameServer()->GetPlayerChar(m_BrushCID);
		if (pBrushChr && pBrushChr->m_DrawEditor.OnSnapPreview(SnappingClient))
			return;
	}

	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
	if (!pObj)
		return;

	pObj->m_X = round_to_int(m_Pos.x);
	pObj->m_Y = round_to_int(m_Pos.y);
	pObj->m_FromX = round_to_int(m_Pos.x);
	pObj->m_FromY = round_to_int(m_Pos.y);
	pObj->m_StartTick = 0;
}
