#include "draweditor.h"
#include <game/server/entities/character.h>
#include <game/server/entities/door.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>

#define PLOTID_TEST 1

CGameContext *CDrawEditor::GameServer() const { return m_pCharacter->GameServer(); }
IServer *CDrawEditor::Server() const { return GameServer()->Server(); }

CDrawEditor::CDrawEditor(CCharacter *pChr)
{
	m_pCharacter = pChr;
	m_DrawMode = DRAW_UNINITIALIZED;
	SetDrawMode(DRAW_HEART);
	m_RoundPos = true;
	m_Erasing = false;
	m_Selecting = false;
	m_EditStartTick = 0;
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
	//int Index = pCol->GetMapIndex(m_Pos);
	if (pCol->CheckPoint(m_Pos)/* || pCol->IsSwitch(Index) != TILE_SWITCH_PLOT || pCol->GetSwitchNumber(Index) != PLOTID_TEST*/)
		return;

	CEntity *pEntity = CreateEntity();
	pEntity->m_PlotID = PLOTID_TEST;

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

		if (pEntity && pEntity->m_PlotID == PLOTID_TEST)
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
		if (m_EditStartTick == 0)
			m_EditStartTick = Server()->Tick();

		bool Faster = m_EditStartTick < Server()->Tick() - Server()->TickSpeed();
		int Add = pNewInput->m_Direction * (1 + 4*(int)Faster);

		if (m_pCharacter->GetPlayer()->m_PlayerFlags&PLAYERFLAG_SCOREBOARD)
		{
			// change length
			m_Data.m_Laser.m_Length = clamp(m_Data.m_Laser.m_Length + (float)Add/10, 0.f, 10.f);
			((CDoor *)m_pPreview)->SetLength(round_to_int(m_Data.m_Laser.m_Length * 32));
		}
		else
		{
			// change rotation
			m_Data.m_Laser.m_Angle = ((m_Data.m_Laser.m_Angle * 180 / pi) + Add) * pi / 180;
			((CDoor *)m_pPreview)->SetDirection(m_Data.m_Laser.m_Angle);
		}
	}
	else
		m_EditStartTick = 0;
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

	if (m_DrawMode != DRAW_UNINITIALIZED)
	{
		// update the preview entity
		RemovePreview();
		SetPreview();
	}

	m_DrawMode = Mode;
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
	return 0;
}

void CDrawEditor::SendWindow()
{
	char aMsg[900];
	str_copy(aMsg,
		"DrawEditor\n\n"
		"     Controls:\n\n"
		"Object picker: Hold SPACE + shoot left/right\n"
		"Place object: Left mouse\n"
		"Eraser: Right mouse\n"
		"Change angle of wall: A/D\n"
		"Change length of wall: TAB + A/D\n"
		"Toggle position rounding: kill\n\n"
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

void CDrawEditor::OnPlayerKill()
{
	m_RoundPos = !m_RoundPos;
}

void CDrawEditor::OnWeaponSwitch()
{
	if (Active())
		SetPreview();
	else if (m_pCharacter->GetLastWeapon() == WEAPON_DRAW_EDITOR)
		RemovePreview();
	else
		return;

	GameServer()->SendTuningParams(GetCID(), m_pCharacter->m_TuneZone);
}

void CDrawEditor::OnPlayerDeath()
{
	RemovePreview();
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
