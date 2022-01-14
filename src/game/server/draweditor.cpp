#include "draweditor.h"
#include <game/server/entities/character.h>
#include <game/server/entities/door.h>
#include <game/server/entities/button.h>
#include <game/server/entities/speedup.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include <engine/shared/config.h>

static float s_MaxLength = 10.f;
static int s_MaxThickness = 4;
static float s_DefaultAngle = 90 * pi / 180;

CGameContext *CDrawEditor::GameServer() const { return m_pCharacter->GameServer(); }
IServer *CDrawEditor::Server() const { return GameServer()->Server(); }

CDrawEditor::CDrawEditor(CCharacter *pChr)
{
	m_pCharacter = pChr;
	m_Setting = -1;
	m_Laser.m_Angle = s_DefaultAngle;
	m_Laser.m_Length = 3;
	m_Laser.m_Thickness = s_MaxThickness;
	m_Laser.m_ButtonMode = false;
	m_Laser.m_Number = 0;
	m_Speedup.m_Angle = 0;
	m_Speedup.m_Force = 2;
	m_Speedup.m_MaxSpeed = 0;
	m_Category = CAT_UNINITIALIZED;
	SetCategory(CAT_PICKUPS);
	m_RoundPos = true;
	m_Erasing = false;
	m_Selecting = false;
	m_EditStartTick = 0;
}

bool CDrawEditor::Active()
{
	return m_pCharacter->GetActiveWeapon() == WEAPON_DRAW_EDITOR;
}

bool CDrawEditor::CanPlace(bool Remove)
{
	if (IsCategoryLaser() || m_Category == CAT_SPEEDUPS)
	{
		int rx = round_to_int(m_Pos.x) / 32;
		int ry = round_to_int(m_Pos.y) / 32;
		if (rx <= 0 || rx >= GameServer()->Collision()->GetWidth()-1 || ry <= 0 || ry >= GameServer()->Collision()->GetHeight()-1)
			return false;
	}

	bool ValidTile = !GameServer()->Collision()->CheckPoint(m_Pos);
	if (m_Category == CAT_SPEEDUPS && !Remove)
	{
		int Index = GameServer()->Collision()->GetMapIndex(m_Pos);
		ValidTile = ValidTile && !GameServer()->Collision()->IsSpeedup(Index);
	}

	int TilePlotID = GameServer()->GetTilePlotID(m_Pos);
	if (CurrentPlotID() != TilePlotID)
		return false;

	int OwnPlotID = GetPlotID();
	bool FreeDraw = OwnPlotID < PLOT_START || CurrentPlotID() != OwnPlotID;
	return (ValidTile && ((TilePlotID >= PLOT_START && TilePlotID == OwnPlotID) || FreeDraw));
}

bool CDrawEditor::CanRemove(CEntity *pEnt)
{
	// check whether pEnt->m_PlotID >= 0 because -1 would mean its a map object, so we dont wanna be able to remove it
	return CanPlace(true) && pEnt && !pEnt->IsPlotDoor() && pEnt->m_PlotID >= 0;
}

int CDrawEditor::GetPlotID()
{
	return GameServer()->GetPlotID(m_pCharacter->GetPlayer()->GetAccID());
}

int CDrawEditor::CurrentPlotID()
{
	return m_pCharacter->GetCurrentTilePlotID(true);
}

int CDrawEditor::GetNumMaxDoors()
{
	return GameServer()->Collision()->GetNumMaxDoors(CurrentPlotID());
}

int CDrawEditor::GetFirstFreeNumber()
{
	int PlotID = CurrentPlotID();
	std::vector<int> vNumbers;

	for (unsigned int i = 0; i < GameServer()->m_aPlots[PlotID].m_vObjects.size(); i++)
	{
		CEntity *pEnt = GameServer()->m_aPlots[PlotID].m_vObjects[i];
		if (pEnt->GetObjType() != CGameWorld::ENTTYPE_BUTTON && (pEnt->GetObjType() != CGameWorld::ENTTYPE_DOOR || pEnt->m_Number == 0))
			continue;

		bool Found = false;
		for (unsigned int i = 0; i < vNumbers.size(); i++)
			if (vNumbers[i] == pEnt->m_Number)
				Found = true;

		if (!Found)
			vNumbers.push_back(pEnt->m_Number);
	}

	int FirstFree = 0;
	for (int i = 0; i < GetNumMaxDoors(); i++)
	{
		int Switch = GameServer()->Collision()->GetSwitchByPlotLaserDoor(PlotID, i);
		bool Found = false;

		for (unsigned int j = 0; j < vNumbers.size(); j++)
		{
			if (vNumbers[j] == Switch)
			{
				FirstFree++;
				Found = true;
				break;
			}
		}

		if (!Found)
			break;
	}

	return FirstFree;
}

void CDrawEditor::Tick()
{
	if (!Active())
		return;

	m_Pos = m_pCharacter->GetCursorPos();
	if (m_RoundPos && !m_Erasing)
		m_Pos = GameServer()->RoundPos(m_Pos);

	if (m_pPreview)
		m_pPreview->SetPos(m_Pos);

	int PlotID = CurrentPlotID();
	if (PlotID != m_PrevPlotID)
	{
		if (m_Category == CAT_LASERDOORS && m_Laser.m_Number >= GetNumMaxDoors())
			m_Laser.m_Number = 0;
	}

	HandleInput();
	m_PrevInput = m_Input;
	m_PrevPlotID = PlotID;

	if (Active() && Server()->Tick() % Server()->TickSpeed() == 0)
	{
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "Objects [%d/%d]", (int)GameServer()->m_aPlots[PlotID].m_vObjects.size(), GameServer()->GetMaxPlotObjects(PlotID));
		GameServer()->SendBroadcast(aBuf, GetCID(), false);
	}
}

void CDrawEditor::OnPlayerFire()
{
	if (!Active())
		return;

	if (m_Selecting)
	{
		int Dir = m_pCharacter->GetAimDir();
		int Setting = m_Setting;
		int NumSettings = GetNumSettings();
		if (Dir == 1)
		{
			Setting++;
			if (Setting >= NumSettings)
				Setting = 0;
		}
		else if (Dir == -1)
		{
			Setting--;
			if (Setting < 0)
				Setting = NumSettings-1;
		}
		SetSetting(Setting);
		SendWindow();
		return;
	}

	if (m_pCharacter->m_FreezeTime || !CanPlace())
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
	m_Input.m_Jump = pNewInput->m_Jump;
	m_Input.m_Hook = pNewInput->m_Hook;
	m_Input.m_Direction = pNewInput->m_Direction;
}

void CDrawEditor::HandleInput()
{
	if (m_Input.m_Jump)
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

	if (m_Input.m_Hook)
	{
		if (m_Selecting && !m_PrevInput.m_Hook)
		{
			int Dir = m_pCharacter->GetAimDir();
			int Category = m_Category;
			if (Dir == 1)
			{
				Category++;
				if (Category >= NUM_DRAW_CATEGORIES)
					Category = 0;
			}
			else if (Dir == -1)
			{
				Category--;
				if (Category < 0)
					Category = NUM_DRAW_CATEGORIES - 1;
			}
			SetCategory(Category);
			SendWindow();
		}
		else if (!m_Selecting && !m_pCharacter->m_FreezeTime)
		{
			m_Erasing = true;

			int Types = (1<<CGameWorld::ENTTYPE_PICKUP) | (1<<CGameWorld::ENTTYPE_DOOR) | (1<<CGameWorld::ENTTYPE_SPEEDUP) | (1<<CGameWorld::ENTTYPE_BUTTON);
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
	}
	else
		m_Erasing = false;

	if (m_Input.m_Direction != 0)
	{
		if (m_EditStartTick == 0)
			m_EditStartTick = Server()->Tick();

		if (m_Selecting && (m_PrevInput.m_Direction == 0 || m_EditStartTick < Server()->Tick() - Server()->TickSpeed() / 2))
		{
			if (m_Category == CAT_LASERWALLS)
			{
				if (m_Setting == LASERWALL_COLLISION)
				{
					m_Laser.m_Collision = !m_Laser.m_Collision;
					m_Laser.m_Thickness = s_MaxThickness;
					((CDoor *)m_pPreview)->SetThickness(m_Laser.m_Thickness);
				}
				else if (m_Setting == LASERWALL_THICKNESS)
				{
					m_Laser.m_Collision = false; // disallow collision with thickness change
					m_Laser.m_Thickness += m_Input.m_Direction;
					if (m_Laser.m_Thickness > s_MaxThickness)
						m_Laser.m_Thickness = 0;
					else if (m_Laser.m_Thickness < 0)
						m_Laser.m_Thickness = s_MaxThickness;
					((CDoor *)m_pPreview)->SetThickness(m_Laser.m_Thickness);
				}
			}
			else if (m_Category == CAT_LASERDOORS)
			{
				if (m_Setting == LASERDOOR_NUMBER)
				{
					m_Laser.m_Number += m_Input.m_Direction;
					if (m_Laser.m_Number >= GetNumMaxDoors())
						m_Laser.m_Number = 0;
					else if (m_Laser.m_Number < 0)
						m_Laser.m_Number = GetNumMaxDoors()-1;
				}
				else if (m_Setting == LASERDOOR_MODE)
				{
					m_Laser.m_ButtonMode = !m_Laser.m_ButtonMode;
					m_Entity = m_Laser.m_ButtonMode ? CGameWorld::ENTTYPE_BUTTON : CGameWorld::ENTTYPE_DOOR;
					UpdatePreview();
				}
			}
			else if (m_Category == CAT_SPEEDUPS)
			{
				if (m_Setting == SPEEDUP_FORCE)
				{
					m_Speedup.m_Force += m_Input.m_Direction;
					if (m_Speedup.m_Force > 255)
						m_Speedup.m_Force = 1;
					else if (m_Speedup.m_Force < 1)
						m_Speedup.m_Force = 255;
				}
				else if (m_Setting == SPEEDUP_MAXSPEED)
				{
					m_Speedup.m_MaxSpeed += m_Input.m_Direction;
					if (m_Speedup.m_MaxSpeed > 255)
						m_Speedup.m_MaxSpeed = 0;
					else if (m_Speedup.m_MaxSpeed < 0)
						m_Speedup.m_MaxSpeed = 255;
				}
			}
			SendWindow();
		}
		else if (!m_Selecting)
		{
			bool Faster = m_EditStartTick < Server()->Tick() - Server()->TickSpeed();
			float Add = m_Input.m_Direction * (1 + 2*(int)Faster);
			if (IsCategoryLaser())
				Add -= m_Input.m_Direction * 0.5f;

			if ((IsCategoryLaser() && !m_Laser.m_ButtonMode) || m_Category == CAT_SPEEDUPS)
			{
				if ((m_pCharacter->GetPlayer()->m_PlayerFlags&PLAYERFLAG_SCOREBOARD) && IsCategoryLaser())
					AddLength(Add);
				else
					AddAngle(Add);
			}
		}
	}
	else
		m_EditStartTick = 0;
}

void CDrawEditor::SetPickup(int Pickup)
{
	if (m_Setting == Pickup)
		return;

	m_Entity = CGameWorld::ENTTYPE_PICKUP;
	if (Pickup == DRAW_PICKUP_HEART || Pickup == DRAW_PICKUP_SHIELD)
	{
		m_Pickup.m_Type = Pickup; // POWERUP_HEALTH=0, POWERUP_ARMOR=1
		m_Pickup.m_SubType = 0;
	}
	else if (Pickup >= DRAW_PICKUP_HAMMER && Pickup <= DRAW_PICKUP_LASER)
	{
		m_Pickup.m_Type = POWERUP_WEAPON;
		m_Pickup.m_SubType = Pickup - 2;
	}

	UpdatePreview();
	m_Setting = Pickup;
}

void CDrawEditor::SetCategory(int Category)
{
	if (m_Category == Category)
		return;

	m_Setting = -1; // so we dont fuck up setpickup
	if (Category == CAT_PICKUPS)
	{
		SetPickup(DRAW_PICKUP_HEART);
	}
	else if (Category == CAT_LASERWALLS || Category == CAT_LASERDOORS)
	{
		m_Entity = CGameWorld::ENTTYPE_DOOR;
		m_RoundPos = true;
		m_Laser.m_Collision = true;
		m_Laser.m_Thickness = s_MaxThickness;
		m_Laser.m_ButtonMode = false;
	}
	else if (Category == CAT_SPEEDUPS)
	{
		m_Entity = CGameWorld::ENTTYPE_SPEEDUP;
		m_RoundPos = true;
	}

	// done in SetPickup()
	if (Category != CAT_PICKUPS)
	{
		UpdatePreview();
		m_Setting = 0; // always first setting
	}

	m_Category = Category;
}

void CDrawEditor::SetSetting(int Setting)
{
	if (m_Category == CAT_PICKUPS)
		SetPickup(Setting);
	else
		m_Setting = Setting;
}

CEntity *CDrawEditor::CreateEntity(bool Preview)
{
	switch (m_Entity)
	{
	case CGameWorld::ENTTYPE_PICKUP:
		return new CPickup(m_pCharacter->GameWorld(), m_Pos, m_Pickup.m_Type, m_Pickup.m_SubType);
	case CGameWorld::ENTTYPE_DOOR:
	{
		int Number = m_Category == CAT_LASERDOORS && !Preview ? GameServer()->Collision()->GetSwitchByPlotLaserDoor(CurrentPlotID(), m_Laser.m_Number) : 0;
		return new CDoor(m_pCharacter->GameWorld(), m_Pos, m_Laser.m_Angle, 32 * m_Laser.m_Length, Number, !Preview && m_Laser.m_Collision, m_Laser.m_Thickness);
	}
	case CGameWorld::ENTTYPE_BUTTON:
	{
		int Number = !Preview ? GameServer()->Collision()->GetSwitchByPlotLaserDoor(CurrentPlotID(), m_Laser.m_Number) : 0;
		return new CButton(m_pCharacter->GameWorld(), m_Pos, Number, !Preview);
	}
	case CGameWorld::ENTTYPE_SPEEDUP:
		return new CSpeedup(m_pCharacter->GameWorld(), m_Pos, m_Speedup.m_Angle, m_Speedup.m_Force, m_Speedup.m_MaxSpeed, !Preview);
	}
	return 0;
}

void CDrawEditor::SendWindow()
{
	char aMsg[900];
	str_format(aMsg, sizeof(aMsg), "     > %s <\n\n", GetCategory(m_Category));

	str_append(aMsg,
		"     Menu controls:\n\n"
		"Change category: hook left/right\n"
		"Move up/down: shoot left/right\n", sizeof(aMsg));
	if (IsCategoryLaser() || m_Category == CAT_SPEEDUPS)
		str_append(aMsg, "Change setting: A/D\n", sizeof(aMsg));
	if (m_Category == CAT_LASERDOORS)
		str_append(aMsg, "First free number: kill\n", sizeof(aMsg));

	str_append(aMsg, "\n", sizeof(aMsg));
	str_append(aMsg,
		"     Controls:\n\n"
		"Stop editing: Switch weapon\n"
		"Place object: Left mouse\n"
		"Eraser: Right mouse\n", sizeof(aMsg));
	if (m_Category != CAT_SPEEDUPS)
		str_append(aMsg, "Toggle position rounding: kill\n", sizeof(aMsg));

	if (IsCategoryLaser() || m_Category == CAT_SPEEDUPS)
	{
		if (IsCategoryLaser())
			str_append(aMsg, "Change length: TAB + A/D\n", sizeof(aMsg));
		str_append(aMsg,
			"Change angle: A/D\n"
			"Add 45 degree steps: TAB + kill\n", sizeof(aMsg));
	}

	str_append(aMsg, "\n", sizeof(aMsg));
	char aBuf[256];
	if (m_Category == CAT_PICKUPS)
	{
		str_append(aMsg, "     Objects:\n\n", sizeof(aMsg));
		for (int i = 0; i < NUM_DRAW_PICKUPS; i++)
			str_append(aMsg, FormatSetting(GetPickup(i), i), sizeof(aMsg));
	}
	else if (m_Category == CAT_LASERWALLS)
	{
		str_append(aMsg, "     Settings:\n\n", sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Collision: %s", m_Laser.m_Collision ? "Yes" : "No");
		str_append(aMsg, FormatSetting(aBuf, LASERWALL_COLLISION), sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Thickness: %d/%d", m_Laser.m_Thickness+1, s_MaxThickness+1);
		str_append(aMsg, FormatSetting(aBuf, LASERWALL_THICKNESS), sizeof(aMsg));
	}
	else if (m_Category == CAT_LASERDOORS)
	{
		str_append(aMsg, "     Settings:\n\n", sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Number: %d/%d", m_Laser.m_Number+1, GetNumMaxDoors());
		str_append(aMsg, FormatSetting(aBuf, LASERDOOR_NUMBER), sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Mode: %s", m_Laser.m_ButtonMode ? "Button" : "Door");
		str_append(aMsg, FormatSetting(aBuf, LASERDOOR_MODE), sizeof(aMsg));
	}
	else if (m_Category == CAT_SPEEDUPS)
	{
		str_append(aMsg, "     Settings:\n\n", sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Force speed: %d", m_Speedup.m_Force);
		str_append(aMsg, FormatSetting(aBuf, SPEEDUP_FORCE), sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Max speed: %d", m_Speedup.m_MaxSpeed);
		str_append(aMsg, FormatSetting(aBuf, SPEEDUP_MAXSPEED), sizeof(aMsg));
	}

	GameServer()->SendMotd(aMsg, GetCID());
}

const char *CDrawEditor::FormatSetting(const char *pSetting, int Setting)
{
	static char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s%s%s\n", m_Setting == Setting ? "> " : "", pSetting, m_Setting == Setting ? " <" : "");
	return aBuf;
}

const char *CDrawEditor::GetPickup(int Pickup)
{
	switch (Pickup)
	{
	case DRAW_PICKUP_HEART: return "Heart";
	case DRAW_PICKUP_SHIELD: return "Shield";
	case DRAW_PICKUP_HAMMER: return "Hammer";
	case DRAW_PICKUP_GUN: return "Gun";
	case DRAW_PICKUP_SHOTGUN: return "Shotgun";
	case DRAW_PICKUP_GRENADE: return "Grenade";
	case DRAW_PICKUP_LASER: return "Rifle";
	default: return "Unknown";
	}
}

const char *CDrawEditor::GetCategory(int Category)
{
	switch (Category)
	{
	case CAT_PICKUPS: return "Pickups";
	case CAT_LASERWALLS: return "Laser Walls";
	case CAT_LASERDOORS: return "Laser Doors";
	case CAT_SPEEDUPS: return "Speedups";
	default: return "Unknown";
	}
}

int CDrawEditor::GetNumSettings()
{
	switch (m_Category)
	{
	case CAT_PICKUPS: return NUM_DRAW_PICKUPS;
	case CAT_LASERWALLS: return NUM_LASERWALL_SETTINGS;
	case CAT_LASERDOORS: return NUM_LASERDOOR_SETTINGS;
	case CAT_SPEEDUPS: return NUM_SPEEDUP_SETTINGS;
	default: return 0;
	}
}

int CDrawEditor::GetCID()
{
	return m_pCharacter->GetPlayer()->GetCID();
}

void CDrawEditor::OnPlayerKill()
{
	if (m_pCharacter->GetPlayer()->m_PlayerFlags&PLAYERFLAG_SCOREBOARD)
	{
		int Angle = -1;
		float DefaultAngle;
		if (IsCategoryLaser() && !m_Laser.m_ButtonMode)
		{
			Angle = round_to_int(m_Laser.m_Angle * 180 / pi);
			DefaultAngle = s_DefaultAngle;
		}
		else if (m_Category == CAT_SPEEDUPS)
		{
			Angle = m_Speedup.m_Angle;
			DefaultAngle = 0;
		}

		if (Angle != -1)
		{
			if (Angle % 45 != 0 || Angle >= 360) // its not precise enough when we rotate some rounds and get a higher value
				SetAngle(DefaultAngle);
			else
				AddAngle(45);
		}
	}
	else if (m_Category == CAT_LASERDOORS && m_Selecting)
	{
		int FreeNumber = GetFirstFreeNumber();
		if (FreeNumber != GetNumMaxDoors())
		{
			m_Laser.m_Number = FreeNumber;
			SendWindow();
		}
	}
	else if (m_Category != CAT_LASERDOORS && m_Category != CAT_SPEEDUPS)
	{
		m_RoundPos = !m_RoundPos;
	}
}

void CDrawEditor::SetAngle(float Angle)
{
	if (IsCategoryLaser() && !m_Laser.m_ButtonMode)
	{
		m_Laser.m_Angle = Angle;
		((CDoor *)m_pPreview)->SetDirection(m_Laser.m_Angle);
	}
	else if (m_Category == CAT_SPEEDUPS)
	{
		m_Speedup.m_Angle = Angle;
		((CSpeedup *)m_pPreview)->SetAngle(m_Speedup.m_Angle);
	}
}

void CDrawEditor::AddAngle(float Add)
{
	if (IsCategoryLaser() && !m_Laser.m_ButtonMode)
	{
		float NewAngle = (m_Laser.m_Angle * 180 / pi) + Add;
		if (NewAngle >= 360.f)
			NewAngle -= 360.f;
		else if (NewAngle < 0.f)
			NewAngle += 360.f;
		SetAngle(NewAngle * pi / 180);
	}
	else if (m_Category == CAT_SPEEDUPS)
	{
		int NewAngle = m_Speedup.m_Angle - Add; // negative to move counter clockwise, same as laserwalls always did
		if (NewAngle >= 360)
			NewAngle -= 360;
		else if (NewAngle < 0)
			NewAngle += 360;
		SetAngle(NewAngle);
	}
}

void CDrawEditor::AddLength(float Add)
{
	m_Laser.m_Length = clamp(m_Laser.m_Length + Add/10.f, 0.0f, s_MaxLength);
	((CDoor *)m_pPreview)->SetLength(round_to_int(m_Laser.m_Length * 32));
}

void CDrawEditor::OnWeaponSwitch()
{
	if (Active())
	{
		SetPreview();

		int PlotID = GetPlotID();
		if (CurrentPlotID() == PlotID)
		{
			GameServer()->SetPlotDoorStatus(PlotID, true);
			GameServer()->RemovePortalsFromPlot(PlotID);

			for (int i = 0; i < GetNumMaxDoors(); i++)
				GameServer()->SetPlotDrawDoorStatus(PlotID, i, true);

			for (int i = 0; i < MAX_CLIENTS; i++)
				if (GameServer()->GetPlayerChar(i) && i != GetCID())
					GameServer()->GetPlayerChar(i)->TeleOutOfPlot(PlotID);
		}
	}
	else if (m_pCharacter->GetLastWeapon() == WEAPON_DRAW_EDITOR)
	{
		RemovePreview();

		int PlotID = GetPlotID();
		if (CurrentPlotID() == PlotID)
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

void CDrawEditor::UpdatePreview()
{
	if (m_Category == CAT_UNINITIALIZED)
		return;

	RemovePreview();
	SetPreview();
}

bool CDrawEditor::OnSnapPreview(int SnappingClient)
{
	return SnappingClient != GetCID() || m_Erasing;
}
