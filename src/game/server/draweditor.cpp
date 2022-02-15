#include "draweditor.h"
#include <game/server/entities/character.h>
#include <game/server/entities/door.h>
#include <game/server/entities/button.h>
#include <game/server/entities/speedup.h>
#include <game/server/entities/teleporter.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include <engine/shared/config.h>

static float s_MaxLength = 10.f;
static int s_MaxThickness = 4;
static float s_DefaultAngle = 90 * pi / 180;

CGameContext *CDrawEditor::GameServer() const { return m_pCharacter->GameServer(); }
IServer *CDrawEditor::Server() const { return GameServer()->Server(); }

void CDrawEditor::Init(CCharacter *pChr)
{
	m_pCharacter = pChr;

	m_Laser.m_Angle = s_DefaultAngle;
	m_Laser.m_Length = 3;
	m_Laser.m_Thickness = s_MaxThickness;
	m_Laser.m_ButtonMode = false;
	m_Laser.m_Number = 0;
	m_Speedup.m_Angle = 0;
	m_Speedup.m_Force = 2;
	m_Speedup.m_MaxSpeed = 0;
	m_Teleporter.m_Number = 0;
	m_Teleporter.m_Evil = true;
	m_Transform.m_State = TRANSFORM_STATE_SETTING_FIRST;
	for (int i = 0; i < 4; i++)
		m_Transform.m_aID[i] = Server()->SnapNewID();
	
	m_Setting = -1;
	m_RoundPos = true;
	m_Erasing = false;
	m_Selecting = false;
	m_EditStartTick = 0;

	m_Category = CAT_UNINITIALIZED;
	for (int i = 0; i < NUM_DRAW_CATEGORIES; i++)
	{
		if (IsCategoryAllowed(i))
		{
			SetCategory(i);
			break;
		}
	}
}

CDrawEditor::~CDrawEditor()
{
	for (int i = 0; i < 4; i++)
		Server()->SnapFreeID(m_Transform.m_aID[i]);
}

bool CDrawEditor::Active()
{
	return m_pCharacter->GetActiveWeapon() == WEAPON_DRAW_EDITOR;
}

bool CDrawEditor::CanPlace(bool Remove)
{
	if (IsCategoryLaser() || m_Category == CAT_SPEEDUPS || m_Category == CAT_TELEPORTER)
	{
		int rx = round_to_int(m_Pos.x) / 32;
		int ry = round_to_int(m_Pos.y) / 32;
		if (rx <= 0 || rx >= GameServer()->Collision()->GetWidth()-1 || ry <= 0 || ry >= GameServer()->Collision()->GetHeight()-1)
			return false;
	}

	int CursorPlotID = GetCursorPlotID();
	bool ValidTile = !GameServer()->Collision()->CheckPoint(m_Pos);
	if (!Remove)
	{
		int Index = GameServer()->Collision()->GetPureMapIndex(m_Pos);
		if (m_Category == CAT_SPEEDUPS)
		{
			if (CursorPlotID >= PLOT_START && GetNumSpeedups(CursorPlotID) >= GameServer()->GetMaxPlotSpeedups(CursorPlotID))
				return false;

			ValidTile = ValidTile && !GameServer()->Collision()->IsSpeedup(Index);
		}
		else if (m_Category == CAT_LASERDOORS && m_Laser.m_ButtonMode)
		{
			// disallow placing buttons on already existing buttons with the same number
			int Number = GameServer()->Collision()->GetSwitchByPlotLaserDoor(CursorPlotID, m_Laser.m_Number);
			ValidTile = ValidTile && GameServer()->Collision()->GetDoorIndex(Index, TILE_SWITCHTOGGLE, Number) == -1;
		}
		else if (m_Category == CAT_TELEPORTER)
		{
			ValidTile = ValidTile && !GameServer()->Collision()->IsTeleportTile(Index);
		}
	}

	int OwnPlotID = GetPlotID();
	bool FreeDraw = OwnPlotID < PLOT_START || CurrentPlotID() != OwnPlotID;
	return (ValidTile && ((CursorPlotID >= PLOT_START && CursorPlotID == OwnPlotID) || FreeDraw));
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

int CDrawEditor::GetCursorPlotID()
{
	return GameServer()->GetTilePlotID(m_Pos);
}

int CDrawEditor::GetNumMaxDoors()
{
	return GameServer()->Collision()->GetNumMaxDoors(CurrentPlotID());
}

int CDrawEditor::GetNumMaxTeleporters()
{
	return GameServer()->Collision()->GetNumMaxTeleporters(CurrentPlotID());
}

int CDrawEditor::GetFirstFreeNumber()
{
	bool IsDoor = m_Category == CAT_LASERDOORS;
	bool IsTeleporter = m_Category == CAT_TELEPORTER;
	if (!IsDoor && !IsTeleporter)
		return -1;

	int PlotID = CurrentPlotID();
	std::vector<int> vNumbers;

	for (unsigned int i = 0; i < GameServer()->m_aPlots[PlotID].m_vObjects.size(); i++)
	{
		CEntity *pEnt = GameServer()->m_aPlots[PlotID].m_vObjects[i];
		if (IsDoor && pEnt->GetObjType() != CGameWorld::ENTTYPE_BUTTON && (pEnt->GetObjType() != CGameWorld::ENTTYPE_DOOR || pEnt->m_Number == 0))
			continue;
		if (IsTeleporter && pEnt->GetObjType() != CGameWorld::ENTTYPE_TELEPORTER)
			continue;

		bool Found = false;
		for (unsigned int i = 0; i < vNumbers.size(); i++)
			if (vNumbers[i] == pEnt->m_Number)
				Found = true;

		if (!Found)
			vNumbers.push_back(pEnt->m_Number);
	}

	int FirstFree = 0;
	int Max = IsDoor ? GetNumMaxDoors() : IsTeleporter ? GetNumMaxTeleporters() : 0;
	for (int i = 0; i < Max; i++)
	{
		int Number = IsDoor ? GameServer()->Collision()->GetSwitchByPlotLaserDoor(PlotID, i) : IsTeleporter ? GameServer()->Collision()->GetSwitchByPlotTeleporter(PlotID, i) : 0;
		bool Found = false;

		for (unsigned int j = 0; j < vNumbers.size(); j++)
		{
			if (vNumbers[j] == Number)
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

int CDrawEditor::GetNumSpeedups(int PlotID)
{
	if (PlotID < PLOT_START)
		return 0; // doesnt matter on free draw, has unlimited anyways

	int Num = 0;
	for (unsigned int i = 0; i < GameServer()->m_aPlots[PlotID].m_vObjects.size(); i++)
		if (GameServer()->m_aPlots[PlotID].m_vObjects[i]->GetObjType() == CGameWorld::ENTTYPE_SPEEDUP)
			Num++;

	return Num;
}

bool CDrawEditor::IsCategoryAllowed(int Category)
{
	if (CurrentPlotID() < PLOT_START || !GameServer()->Config()->m_SvPlotEditorCategories[0])
		return true;
	return str_in_list(GameServer()->Config()->m_SvPlotEditorCategories, ",", GetCategoryListName(Category));
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
		else if (m_Category == CAT_TELEPORTER && m_Teleporter.m_Number >= GetNumMaxTeleporters())
			m_Teleporter.m_Number = 0;
	}

	if (m_Category == CAT_TRANSFORM)
	{
		if ((m_Transform.m_State == TRANSFORM_STATE_SETTING_FIRST || m_Transform.m_State == TRANSFORM_STATE_SETTING_SECOND))
		{
			if (m_Transform.m_State == TRANSFORM_STATE_SETTING_FIRST)
				m_Transform.m_aPos[0] = m_Pos;
			m_Transform.m_aPos[1] = m_Pos;
		}
		if (m_Transform.m_State == TRANSFORM_STATE_RUNNING)
		{
			for (unsigned int i = 0; i < m_Transform.m_vSelected.size(); i++)
				m_Transform.m_vSelected[i].m_pEnt->SetPos(m_Pos + m_Transform.m_vSelected[i].m_Offset);
		}
	}

	HandleInput();
	m_PrevInput = m_Input;
	m_PrevPlotID = PlotID;

	if (Active() && Server()->Tick() % Server()->TickSpeed() == 0)
	{
		// if you have a plot and if you are in your own plot dont show object counts of free draw or nearby plots
		if (PlotID < PLOT_START || PlotID != GetPlotID())
			PlotID = GetCursorPlotID();

		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "Objects [%d/%d]", (int)GameServer()->m_aPlots[PlotID].m_vObjects.size(), GameServer()->GetMaxPlotObjects(PlotID));
		GameServer()->SendBroadcast(aBuf, GetCID(), false);
	}
}

void CDrawEditor::Snap()
{
	if (!Active() || m_Category != CAT_TRANSFORM)
		return;

	vec2 TopLeft = m_Transform.TopLeft();
	vec2 BottomRight = m_Transform.BottomRight();
	vec2 aPoints[4] = { TopLeft, vec2(BottomRight.x, TopLeft.y), BottomRight, vec2(TopLeft.x, BottomRight.y) };

	for (int i = 0; i < 4; i++)
	{
		CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_Transform.m_aID[i], sizeof(CNetObj_Laser)));
		if (!pObj)
			return;

		int To = i == 3 ? 0 : i+1;
		pObj->m_X = round_to_int(aPoints[i].x);
		pObj->m_Y = round_to_int(aPoints[i].y);
		pObj->m_FromX = round_to_int(aPoints[To].x);
		pObj->m_FromY = round_to_int(aPoints[To].y);
		pObj->m_StartTick = Server()->Tick() - 2;
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

	if (m_Category == CAT_TRANSFORM)
	{
		int PlotID = CurrentPlotID();
		if (m_Transform.m_State == TRANSFORM_STATE_CONFIRM)
		{
			std::vector<CEntity *> vSelectedEnts;
			for (unsigned i = 0; i < GameServer()->m_aPlots[PlotID].m_vObjects.size(); i++)
			{
				CEntity *pEntity = GameServer()->m_aPlots[PlotID].m_vObjects[i];
				if (m_Transform.IsInArea(pEntity->GetPos()))
					vSelectedEnts.push_back(pEntity);
			}

			if (m_Setting == TRANSFORM_CUT || m_Setting == TRANSFORM_COPY)
			{
				for (unsigned int i = 0; i < vSelectedEnts.size(); i++)
				{
					SelectedEnt Entity;
					Entity.m_pEnt = CreateTransformEntity(vSelectedEnts[i], true);
					Entity.m_pEnt->m_BrushCID = GetCID();
					Entity.m_Offset = Entity.m_pEnt->GetPos() - m_Pos;
					m_Transform.m_vSelected.push_back(Entity);
				}
			}
			else if (m_Setting == TRANSFORM_ERASE)
			{
				for (unsigned int i = 0; i < vSelectedEnts.size(); i++)
					for (unsigned int j = 0; j < GameServer()->m_aPlots[PlotID].m_vObjects.size(); j++)
						if (vSelectedEnts[i] == GameServer()->m_aPlots[PlotID].m_vObjects[j])
						{
							GameServer()->m_aPlots[PlotID].m_vObjects.erase(GameServer()->m_aPlots[PlotID].m_vObjects.begin() + j);
							vSelectedEnts[i]->MarkForDestroy();
							j--;
						}

				m_Transform.m_State = TRANSFORM_STATE_SETTING_FIRST;
				return;
			}
		}
		else if (m_Transform.m_State == TRANSFORM_STATE_RUNNING)
		{
			for (unsigned int i = 0; i < m_Transform.m_vSelected.size(); i++)
			{
				CEntity *pEntity = CreateTransformEntity(m_Transform.m_vSelected[i].m_pEnt, false);
				pEntity->m_PlotID = PlotID;
			}
			RemoveTransformPreview();
			m_Transform.m_State = TRANSFORM_STATE_SETTING_FIRST;
			return;
		}

		m_Transform.m_State++;
		return;
	}

	if (m_pCharacter->m_FreezeTime || !CanPlace())
		return;

	int PlotID = GetCursorPlotID();
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
			do
			{
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
			} while (!IsCategoryAllowed(Category) && Category != m_Category); // when only wrong categories has been written to the list fall back to the category we had before
			SetCategory(Category);
			SendWindow();
		}
		else if (!m_Selecting)
		{
			if (m_Category == CAT_TRANSFORM)
			{
				m_Transform.m_State = TRANSFORM_STATE_SETTING_FIRST;
				m_Transform.m_vSelected.clear();
			}
			else if (!m_pCharacter->m_FreezeTime)
			{
				m_Erasing = true;

				int Types = (1<<CGameWorld::ENTTYPE_PICKUP) | (1<<CGameWorld::ENTTYPE_DOOR) | (1<<CGameWorld::ENTTYPE_SPEEDUP) | (1<<CGameWorld::ENTTYPE_BUTTON) | (1<<CGameWorld::ENTTYPE_TELEPORTER);
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
			else if (m_Category == CAT_TELEPORTER)
			{
				if (m_Setting == TELEPORTER_NUMBER)
				{
					m_Teleporter.m_Number += m_Input.m_Direction;
					if (m_Teleporter.m_Number >= GetNumMaxTeleporters())
						m_Teleporter.m_Number = 0;
					else if (m_Teleporter.m_Number < 0)
						m_Teleporter.m_Number = GetNumMaxTeleporters()-1;
				}
				else if (m_Setting == TELEPORTER_MODE)
				{
					m_Teleporter.m_Mode += m_Input.m_Direction;
					if (m_Teleporter.m_Mode >= NUM_TELE_MODES)
						m_Teleporter.m_Mode = 0;
					else if (m_Teleporter.m_Mode < 0)
						m_Teleporter.m_Mode = NUM_TELE_MODES-1;
					UpdatePreview();
				}
				else if (m_Setting == TELEPORTER_EVIL)
				{
					m_Teleporter.m_Evil = !m_Teleporter.m_Evil;
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
	else if (Category == CAT_TELEPORTER)
	{
		m_Entity = CGameWorld::ENTTYPE_TELEPORTER;
		m_RoundPos = true;
	}
	else if (Category == CAT_TRANSFORM)
	{
		m_Entity = -1;
		m_RoundPos = true;
		m_Transform.m_State = TRANSFORM_STATE_SETTING_FIRST;
		m_Transform.m_vSelected.clear();
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
		return new CPickup(m_pCharacter->GameWorld(), m_Pos, m_Pickup.m_Type, m_Pickup.m_SubType, !Preview);
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
	case CGameWorld::ENTTYPE_TELEPORTER:
	{
		int Number = !Preview ? GameServer()->Collision()->GetSwitchByPlotTeleporter(CurrentPlotID(), m_Teleporter.m_Number) : 0;
		return new CTeleporter(m_pCharacter->GameWorld(), m_Pos, GetTeleporterType(), Number, !Preview);
	}
	}
	return 0;
}

CEntity *CDrawEditor::CreateTransformEntity(CEntity *pTemplate, bool Preview)
{
	switch (pTemplate->GetObjType())
	{
	case CGameWorld::ENTTYPE_PICKUP:
		return new CPickup(pTemplate->GameWorld(), pTemplate->GetPos(), ((CPickup *)pTemplate)->GetType(), ((CPickup *)pTemplate)->GetSubtype(), !Preview);
	case CGameWorld::ENTTYPE_DOOR:
		return new CDoor(pTemplate->GameWorld(), pTemplate->GetPos(), ((CDoor *)pTemplate)->GetRotation(), ((CDoor *)pTemplate)->GetLength(), pTemplate->m_Number, !Preview, ((CDoor *)pTemplate)->GetThickness());
	case CGameWorld::ENTTYPE_BUTTON:
		return new CButton(pTemplate->GameWorld(), pTemplate->GetPos(), pTemplate->m_Number, !Preview);
	case CGameWorld::ENTTYPE_SPEEDUP:
		return new CSpeedup(pTemplate->GameWorld(), pTemplate->GetPos(), ((CSpeedup *)pTemplate)->GetAngle(), ((CSpeedup *)pTemplate)->GetForce(), ((CSpeedup *)pTemplate)->GetMaxSpeed(), !Preview);
	case CGameWorld::ENTTYPE_TELEPORTER:
		return new CTeleporter(pTemplate->GameWorld(), pTemplate->GetPos(), ((CTeleporter *)pTemplate)->GetType(), pTemplate->m_Number, !Preview);
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
	if (IsCategoryLaser() || m_Category == CAT_SPEEDUPS || m_Category == CAT_TELEPORTER)
		str_append(aMsg, "Change setting: A/D\n", sizeof(aMsg));
	if (m_Category == CAT_LASERDOORS || m_Category == CAT_TELEPORTER)
		str_append(aMsg, "First free number: kill\n", sizeof(aMsg));

	str_append(aMsg, "\n", sizeof(aMsg));
	str_append(aMsg,
		"     Controls:\n\n"
		"Stop editing: Switch weapon\n", sizeof(aMsg));
	if (m_Category == CAT_TRANSFORM)
	{
		str_append(aMsg, "Abort selection: Right mouse\n", sizeof(aMsg));
		str_append(aMsg, "Confirm: Left mouse\n", sizeof(aMsg));
	}
	else
	{
		str_append(aMsg,
			"Eraser: Right mouse\n"
			"Place object: Left mouse\n", sizeof(aMsg));
	}
	if (m_Category != CAT_SPEEDUPS && m_Category != CAT_TELEPORTER && m_Category != CAT_TRANSFORM)
		str_append(aMsg, "Toggle position rounding: kill\n", sizeof(aMsg));

	if (IsCategoryLaser() || m_Category == CAT_SPEEDUPS || m_Category == CAT_TRANSFORM)
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
	else if (m_Category == CAT_TELEPORTER)
	{
		str_append(aMsg, "     Settings:\n\n", sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Number: %d/%d", m_Teleporter.m_Number+1, GetNumMaxTeleporters());
		str_append(aMsg, FormatSetting(aBuf, TELEPORTER_NUMBER), sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Mode: %s", GetTeleporterMode());
		str_append(aMsg, FormatSetting(aBuf, TELEPORTER_MODE), sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Evil: %s", m_Teleporter.m_Evil ? "Yes" : "No");
		str_append(aMsg, FormatSetting(aBuf, TELEPORTER_EVIL), sizeof(aMsg));
	}
	else if (m_Category == CAT_TRANSFORM)
	{
		str_append(aMsg, "     Settings:\n\n", sizeof(aMsg));
		str_append(aMsg, FormatSetting("Cut", TRANSFORM_CUT), sizeof(aMsg));
		str_append(aMsg, FormatSetting("Copy", TRANSFORM_COPY), sizeof(aMsg));
		str_append(aMsg, FormatSetting("Erase", TRANSFORM_ERASE), sizeof(aMsg));
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
	case CAT_TELEPORTER: return "Teleporters";
	case CAT_TRANSFORM: return "Transformation";
	default: return "Unknown";
	}
}

const char *CDrawEditor::GetCategoryListName(int Category)
{
	switch (Category)
	{
	case CAT_PICKUPS: return "pickups";
	case CAT_LASERWALLS: return "walls";
	case CAT_LASERDOORS: return "doors";
	case CAT_SPEEDUPS: return "speedups";
	case CAT_TELEPORTER: return "teleporters";
	case CAT_TRANSFORM: "return transform";
	default: return "";
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
	case CAT_TELEPORTER: return NUM_TELEPORTERS_SETTINGS;
	case CAT_TRANSFORM: return NUM_TRANSFORM_SETTINGS;
	default: return 0;
	}
}

const char *CDrawEditor::GetTeleporterMode()
{
	switch (m_Teleporter.m_Mode)
	{
	case TELE_MODE_TOGGLE: return "In/Out";
	case TELE_MODE_IN: return "From";
	case TELE_MODE_OUT: return "To";
	case TELE_MODE_WEAPON: return "Weapon From";
	case TELE_MODE_HOOK: return "Hook From";
	default: return "Unknown";
	}
}

int CDrawEditor::GetTeleporterType()
{
	switch (m_Teleporter.m_Mode)
	{
	case TELE_MODE_TOGGLE: return m_Teleporter.m_Evil ? TILE_TELE_INOUT_EVIL : TILE_TELE_INOUT;
	case TELE_MODE_IN: return m_Teleporter.m_Evil ? TILE_TELEINEVIL : TILE_TELEIN;
	case TELE_MODE_OUT: return TILE_TELEOUT;
	case TELE_MODE_WEAPON: return TILE_TELEINWEAPON;
	case TELE_MODE_HOOK: return TILE_TELEINHOOK;
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
	else if (m_Category != CAT_LASERDOORS && m_Category != CAT_SPEEDUPS && m_Category != CAT_TELEPORTER)
	{
		m_RoundPos = !m_RoundPos;
	}
	else if (m_Selecting)
	{
		bool IsDoor = m_Category == CAT_LASERDOORS;
		bool IsTeleporter = m_Category == CAT_TELEPORTER;
		int Max = IsDoor ? GetNumMaxDoors() : IsTeleporter ? GetNumMaxTeleporters() : -1;

		int FreeNumber = GetFirstFreeNumber(); // returns -1, so when not a wanted category -1 == -1 = return :D
		if (FreeNumber != Max)
		{
			if (IsDoor) m_Laser.m_Number = FreeNumber;
			else if (IsTeleporter) m_Teleporter.m_Number = FreeNumber;
			SendWindow();
		}
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
	if (IsCategoryLaser() && !m_Laser.m_ButtonMode)
	{
		m_Laser.m_Length = clamp(m_Laser.m_Length + Add/10.f, 0.0f, s_MaxLength);
		((CDoor *)m_pPreview)->SetLength(round_to_int(m_Laser.m_Length * 32));
	}
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
	RemoveTransformPreview();
}

void CDrawEditor::SetPreview()
{
	if (m_pPreview || m_Entity == -1)
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

void CDrawEditor::RemoveTransformPreview()
{
	for (unsigned int i = 0; i < m_Transform.m_vSelected.size(); i++)
		m_Transform.m_vSelected[i].m_pEnt->Destroy();
	m_Transform.m_vSelected.clear();
}
