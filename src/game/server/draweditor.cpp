#include "draweditor.h"
#include <game/server/entities/character.h>
#include <game/server/entities/door.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include <engine/shared/config.h>

static float s_MaxLength = 10.f;
static float s_DefaultAngle = 90 * pi / 180;

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

bool CDrawEditor::CanPlace()
{
	if (m_DrawMode == DRAW_WALL)
	{
		int rx = round_to_int(m_Pos.x) / 32;
		int ry = round_to_int(m_Pos.y) / 32;
		if (rx <= 0 || rx >= GameServer()->Collision()->GetWidth()-1 || ry <= 0 || ry >= GameServer()->Collision()->GetHeight()-1)
			return false;
	}

	int TilePlotID = GameServer()->GetTilePlotID(m_Pos);
	int OwnPlotID = GetPlotID();
	bool FreeDraw = OwnPlotID < PLOT_START || m_pCharacter->GetCurrentTilePlotID() != OwnPlotID;
	return (!GameServer()->Collision()->CheckPoint(m_Pos) && ((TilePlotID >= PLOT_START && TilePlotID == OwnPlotID) || FreeDraw));
}

bool CDrawEditor::CanRemove(CEntity *pEnt)
{
	// check whether pEnt->m_PlotID >= 0 because -1 would mean its a map object, so we dont wanna be able to remove it
	return CanPlace() && pEnt && !pEnt->IsPlotDoor() && pEnt->m_PlotID >= 0;
}

int CDrawEditor::GetPlotID()
{
	return GameServer()->GetPlotID(m_pCharacter->GetPlayer()->GetAccID());
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

	if (Server()->Tick() % Server()->TickSpeed() == 0)
	{
		int PlotID = m_pCharacter->GetCurrentTilePlotID();
		if (PlotID == GetPlotID() || PlotID == 0)
		{
			char aBuf[32];
			str_format(aBuf, sizeof(aBuf), "Objects [%d/%d]", (int)GameServer()->m_aPlots[PlotID].m_vObjects.size(), GameServer()->GetMaxPlotObjects(PlotID));
			GameServer()->SendBroadcast(aBuf, GetCID(), false);
		}
	}
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

	if (!CanPlace())
		return;

	int PlotID = GameServer()->GetTilePlotID(m_Pos);
	if (GameServer()->m_aPlots[PlotID].m_vObjects.size() >= GameServer()->GetMaxPlotObjects(PlotID))
		return;

	CEntity *pEntity = CreateEntity();
	pEntity->m_PlotID = PlotID;

	GameServer()->m_aPlots[PlotID].m_vObjects.push_back(pEntity);

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
		if (m_Selecting)
			GameServer()->SendMotd("", GetCID());
		m_Selecting = false;
	}

	if (pNewInput->m_Hook && !m_Selecting && !m_pCharacter->m_FreezeTime)
	{
		m_Erasing = true;

		int Types = (1<<CGameWorld::ENTTYPE_PICKUP) | (1<<CGameWorld::ENTTYPE_DOOR);
		CEntity *pEntity = GameServer()->m_World.ClosestEntityTypes(m_Pos, 16.f, Types, m_pPreview, GetCID());

		if (CanRemove(pEntity))
		{
			for (unsigned i = 0; i < GameServer()->m_aPlots[pEntity->m_PlotID].m_vObjects.size(); i++)
				if (GameServer()->m_aPlots[pEntity->m_PlotID].m_vObjects[i] == pEntity)
					GameServer()->m_aPlots[pEntity->m_PlotID].m_vObjects.erase(GameServer()->m_aPlots[pEntity->m_PlotID].m_vObjects.begin() + i);

			GameServer()->m_World.DestroyEntity(pEntity);
			m_pCharacter->SetAttackTick(Server()->Tick());
			GameServer()->CreateSound(m_Pos, SOUND_HOOK_LOOP, CmaskAll());
		}
	}
	else
		m_Erasing = false;

	if (pNewInput->m_Direction != 0 && m_DrawMode == DRAW_WALL)
	{
		if (m_EditStartTick == 0)
			m_EditStartTick = Server()->Tick();

		bool Faster = m_EditStartTick < Server()->Tick() - Server()->TickSpeed();
		int Add = pNewInput->m_Direction * (1 + 4*(int)Faster);

		if (m_pCharacter->GetPlayer()->m_PlayerFlags&PLAYERFLAG_SCOREBOARD)
			AddLength(Add);
		else
			AddAngle(Add);
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
		m_Data.m_Laser.m_Angle = s_DefaultAngle;
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
	char aExtraOptions[256];

	str_format(aExtraOptions, sizeof(aExtraOptions), "     %s options:\n\n"
		"Change angle of wall: A/D\n"
		"Change length of wall: TAB + A/D\n"
		"Add 45 degree steps: TAB + kill\n", GetMode(DRAW_WALL));

	str_format(aMsg, sizeof(aMsg),
		"     Controls:\n\n"
		"Stop editing: Switch weapon\n"
		"Object picker: Hold SPACE + shoot left/right\n"
		"Place object: Left mouse\n"
		"Eraser: Right mouse\n"
		"Toggle position rounding: kill\n"
		"\n%s"
		"\n     Objects:\n\n", aExtraOptions);

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
	if (m_pCharacter->GetPlayer()->m_PlayerFlags&PLAYERFLAG_SCOREBOARD)
	{
		if (m_DrawMode == DRAW_WALL)
		{
			int Angle = round_to_int(m_Data.m_Laser.m_Angle * 180 / pi);
			if (Angle % 45 != 0 || Angle > 360) // its not precise enough when we rotate some rounds and get a higher value
				SetAngle(s_DefaultAngle);
			else
				AddAngle(45);
		}
	}
	else
		m_RoundPos = !m_RoundPos;
}

void CDrawEditor::SetAngle(float Angle)
{
	m_Data.m_Laser.m_Angle = Angle;
	((CDoor *)m_pPreview)->SetDirection(m_Data.m_Laser.m_Angle);
}

void CDrawEditor::AddAngle(int Add)
{
	SetAngle(((m_Data.m_Laser.m_Angle * 180 / pi) + Add) * pi / 180);
}

void CDrawEditor::AddLength(int Add)
{
	m_Data.m_Laser.m_Length = clamp(m_Data.m_Laser.m_Length + (float)Add/10, 0.f, s_MaxLength);
	((CDoor *)m_pPreview)->SetLength(round_to_int(m_Data.m_Laser.m_Length * 32));
}

void CDrawEditor::OnWeaponSwitch()
{
	if (Active())
	{
		SetPreview();

		int PlotID = GetPlotID();
		if (m_pCharacter->GetCurrentTilePlotID() == PlotID)
		{
			for (int i = 0; i < MAX_CLIENTS; i++)
				if (GameServer()->GetPlayerChar(i) && i != GetCID())
					GameServer()->GetPlayerChar(i)->TeleOutOfPlot(PlotID);

			GameServer()->SetPlotDoorStatus(PlotID, true);
			GameServer()->RemovePortalsFromPlot(PlotID);
		}
	}
	else if (m_pCharacter->GetLastWeapon() == WEAPON_DRAW_EDITOR)
	{
		RemovePreview();

		int PlotID = GetPlotID();
		if (m_pCharacter->GetCurrentTilePlotID() == PlotID)
		{
			for (int i = 0; i < MAX_CLIENTS; i++)
				if (GameServer()->GetPlayerChar(i))
					GameServer()->GetPlayerChar(i)->TeleOutOfPlot(PlotID);
		}
	}
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
