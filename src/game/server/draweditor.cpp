#include "draweditor.h"
#include <game/server/entities/character.h>
#include <game/server/entities/door.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>

CGameContext *CDrawEditor::GameServer() const { return m_pCharacter->GameServer(); }
IServer *CDrawEditor::Server() const { return GameServer()->Server(); }

CDrawEditor::CDrawEditor(CCharacter *pChr)
{
	m_pCharacter = pChr;

	// default options
	m_Entity = CGameWorld::ENTTYPE_PICKUP;
	m_RoundPos = true;
	m_Data.m_Pickup.m_Type = POWERUP_HEALTH;
	m_Data.m_Pickup.m_SubType = 0;

	m_DrawMode = DRAW_HEART;
	m_Erasing = false;
	m_Selecting = false;
	m_RotateStartTick = 0;
	m_pPreview = 0;
}

bool CDrawEditor::Active()
{
	return m_pCharacter->GetActiveWeapon() == WEAPON_DRAW_EDITOR;
}

void CDrawEditor::Tick()
{
	if (!Active())
		return;

	m_Pos = m_pCharacter->m_CursorPos;
	if (m_RoundPos && !m_Erasing)
	{
		m_Pos.x -= (int)m_Pos.x % 32 - 16;
		m_Pos.y -= (int)m_Pos.y % 32 - 16;
	}

	if (m_pPreview)
		m_pPreview->SetPos(m_Pos);
}

void CDrawEditor::OnPlayerFire()
{
	if (!Active())
		return;

	if (m_Selecting)
	{
		int Dir = m_pCharacter->GetAimDir();
		int Mode = m_DrawMode;
		if (Dir == 1)
		{
			Mode++;
			if (Mode >= NUM_DRAW_MODES)
				Mode = 0;
		}
		else if (Dir == -1)
		{
			Mode--;
			if (Mode < 0)
				Mode = NUM_DRAW_MODES-1;
		}
		SetDrawMode(Mode);
		SendWindow();
		return;
	}

	CCollision *pCol = GameServer()->Collision();
	int Index = pCol->GetMapIndex(m_Pos);
	if (pCol->CheckPoint(m_Pos))
		return;

	CEntity *pEntity = CreateEntity();

	m_pCharacter->SetAttackTick(Server()->Tick());
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, CmaskAll());
}

void CDrawEditor::OnInput(CNetObj_PlayerInput *pNewInput)
{
	if (pNewInput->m_Jump)
	{
		if (!m_Selecting || Server()->Tick() % 100 == 0)
			SendWindow();

		m_Selecting = true;
	}
	else
	{
		GameServer()->SendMotd("", GetCID());
		m_Selecting = false;
	}

	if (pNewInput->m_Hook && !m_Selecting)
	{
		m_Erasing = true;

		int Types = (1<<CGameWorld::ENTTYPE_PICKUP) | (1<<CGameWorld::ENTTYPE_DOOR);
		CEntity *pEntity = GameServer()->m_World.ClosestEntityTypes(m_Pos, 16.f, Types, m_pPreview, GetCID());

		if (pEntity)
		{
			GameServer()->m_World.DestroyEntity(pEntity);
			m_pCharacter->SetAttackTick(Server()->Tick());
			GameServer()->CreateSound(m_Pos, SOUND_HOOK_LOOP, CmaskAll());
		}
	}
	else
		m_Erasing = false;

	if (pNewInput->m_Direction != 0 && m_Entity == CGameWorld::ENTTYPE_DOOR)
	{
		if (m_RotateStartTick == 0)
			m_RotateStartTick = Server()->Tick();

		bool Faster = m_RotateStartTick < Server()->Tick() - Server()->TickSpeed();
		int Add = pNewInput->m_Direction * (1 + 4*(int)Faster);
		m_Data.m_Laser.m_Angle = ((m_Data.m_Laser.m_Angle * 180 / pi) + Add) * pi / 180;
		((CDoor *)m_pPreview)->SetDirection(m_Data.m_Laser.m_Angle);
	}
	else
		m_RotateStartTick = 0;
}

void CDrawEditor::SetDrawMode(int Mode)
{
	if (m_DrawMode == Mode)
		return;

	if (Mode == DRAW_HEART || Mode == DRAW_SHIELD)
	{
		m_Entity = CGameWorld::ENTTYPE_PICKUP;
		m_Data.m_Pickup.m_Type = Mode; // POWERUP_HEALTH=0, POWERUP_ARMOR=1
		m_Data.m_Pickup.m_SubType = 0;
	}
	else if (Mode >= DRAW_HAMMER && Mode <= DRAW_LASER)
	{
		m_Entity = CGameWorld::ENTTYPE_PICKUP;
		m_Data.m_Pickup.m_Type = POWERUP_WEAPON;
		m_Data.m_Pickup.m_SubType = Mode - 2;
	}
	else if (Mode == DRAW_WALL)
	{
		m_Entity = CGameWorld::ENTTYPE_DOOR;
		m_Data.m_Laser.m_Angle = 0;
		m_Data.m_Laser.m_Length = 3;
	}

	m_DrawMode = Mode;

	// update the preview entity
	RemovePreview();
	SetPreview();
}

CEntity *CDrawEditor::CreateEntity(bool Preview)
{
	switch (m_Entity)
	{
	case CGameWorld::ENTTYPE_PICKUP:
		return new CPickup(m_pCharacter->GameWorld(), m_Pos, m_Data.m_Pickup.m_Type, m_Data.m_Pickup.m_SubType);
	case CGameWorld::ENTTYPE_DOOR:
		return new CDoor(m_pCharacter->GameWorld(), m_Pos, m_Data.m_Laser.m_Angle, 32 * m_Data.m_Laser.m_Length, 0, !Preview);
	}
}

void CDrawEditor::SendWindow()
{
	char aMsg[900];
	str_copy(aMsg,
		"DrawEditor\n\n"
		"     Controls:\n\n"
		"Object picker: Hold space, shoot left/right\n"
		"Place object: Left mouse\n"
		"Eraser: Right mouse\n"
		"Change angle of wall: A/D\n\n"
		"     Objects:\n\n", sizeof(aMsg));

	for (int i = 0; i < NUM_DRAW_MODES; i++)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s%s%s\n", i == m_DrawMode ? ">  " : "", GetMode(i), i == m_DrawMode ? "  <" : "");
		str_append(aMsg, aBuf, sizeof(aMsg));
	}

	GameServer()->SendMotd(aMsg, GetCID());
}

const char *CDrawEditor::GetMode(int Mode)
{
	switch (Mode)
	{
	case DRAW_HEART:
		return "Heart";
	case DRAW_SHIELD:
		return "Shield";
	case DRAW_HAMMER:
		return "Hammer";
	case DRAW_GUN:
		return "Gun";
	case DRAW_SHOTGUN:
		return "Shotgun";
	case DRAW_GRENADE:
		return "Grenade";
	case DRAW_LASER:
		return "Rifle";
	case DRAW_WALL:
		return "Laser wall";
	}
	return "Unknown";
}

int CDrawEditor::GetCID()
{
	return m_pCharacter->GetPlayer()->GetCID();
}

void CDrawEditor::OnWeaponSwitch()
{
	if (Active())
		SetPreview();
	else
		RemovePreview();

	GameServer()->SendTuningParams(GetCID(), m_pCharacter->m_TuneZone);
}

void CDrawEditor::SetPreview()
{
	if (m_pPreview)
		return;

	m_pPreview = CreateEntity(true);
	m_pPreview->m_BrushCID = GetCID();
}

void CDrawEditor::RemovePreview()
{
	if (!m_pPreview)
		return;

	GameServer()->m_World.DestroyEntity(m_pPreview);
	m_pPreview = 0;
}

bool CDrawEditor::OnSnapPreview(int SnappingClient)
{
	return SnappingClient != GetCID() || m_Erasing;
}
