// made by fokkonaut

#include "plots.h"
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include <engine/shared/config.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/server/entities/map/draweditor/button.h>
#include <game/server/entities/map/draweditor/speedup.h>
#include <game/server/entities/map/draweditor/teleporter.h>
#include <game/server/houses/shop.h>
#include <fstream>

CGameContext *CPlots::GameServer() const { return m_pGameServer; }
IServer *CPlots::Server() const { return GameServer()->Server(); }
CConfig *CPlots::Config() const { return GameServer()->Config(); }
CCollision *CPlots::Collision() const { return GameServer()->Collision(); }
CGameWorld *CPlots::GameWorld() const { return &GameServer()->m_World; }

void CPlots::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;

	for (int i = 0; i < MAX_PLOTS; i++)
	{
		m_aPlots[i].m_aOwner[0] = 0;
		m_aPlots[i].m_aDisplayName[0] = 0;
		m_aPlots[i].m_ExpireDate = 0;
		m_aPlots[i].m_Size = 0;
		m_aPlots[i].m_ToTele = vec2(-1, -1);
		m_aPlots[i].m_vObjects.clear();
		m_aPlots[i].m_DestroyEndTick = 0;
		m_aPlots[i].m_DoorHealth = Config()->m_SvPlotDoorHealth;
	}
}

void CPlots::InitPlot(int PlotID, vec2 ToTele, int Size)
{
	m_aPlots[PlotID].m_ToTele = ToTele;
	m_aPlots[PlotID].m_Size = Size;
}

void CPlots::LoadData()
{
	for (int i = 0; i < Collision()->m_NumPlots + 1; i++)
		ReadPlotStats(i);
	ExpirePlots();

	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "%s/presets", Config()->m_SvPlotFilePath);
	GameServer()->Storage()->ListDirectory(IStorage::TYPE_ALL, aPath, LoadPresetListCallback, this);
}

void CPlots::WriteData()
{
	for (int i = 0; i < Collision()->m_NumPlots + 1; i++)
		WritePlotStats(i);
}

void CPlots::Tick()
{
	// Check if plot destroy is over, player is not wanted anymore as it seems
	for (int i = PLOT_START; i < Collision()->m_NumPlots + 1; i++)
	{
		if (m_aPlots[i].m_DestroyEndTick && !PlotCanBeRaided(i))
		{
			// Reset door health
			m_aPlots[i].m_DestroyEndTick = 0;
			m_aPlots[i].m_DoorHealth = Config()->m_SvPlotDoorHealth;
			int AccID = GameServer()->m_Accounts.GetAccIDByUsername(m_aPlots[i].m_aOwner);
			if (AccID >= ACC_START)
			{
				int ClientID = GameServer()->m_Accounts.Get(AccID).m_ClientID;
				if (ClientID >= 0 && GameServer()->m_apPlayers[ClientID])
				{
					GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("Your plot is no longer subject to a search warrant and can no longer be destroyed"));
				}
			}
		}
	}
}

void CPlots::ReadPlotStats(int ID)
{
	std::string data;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s/%s/%d.plot", Config()->m_SvPlotFilePath, Server()->GetCurrentMapName(), ID);
	std::fstream PlotFile(aBuf);
	if (!PlotFile.is_open())
		return;

	for (int i = 0; i < NUM_PLOT_VARIABLES; i++)
	{
		getline(PlotFile, data);
		const char *pData = data.c_str();

		switch (i)
		{
		case PLOT_OWNER_ACC_USERNAME:		str_copy(m_aPlots[ID].m_aOwner, pData, sizeof(m_aPlots[ID].m_aOwner)); break;
		case PLOT_DISPLAY_NAME:				str_copy(m_aPlots[ID].m_aDisplayName, pData, sizeof(m_aPlots[ID].m_aDisplayName)); break;
		case PLOT_EXPIRE_DATE:				m_aPlots[ID].m_ExpireDate = atoi(pData); break;
		case PLOT_DOOR_STATUS:				SetPlotDoorStatus(ID, atoi(pData)); break;
		case PLOT_OBJECTS:
		{
			std::vector<CEntity *> vEntities = ReadPlotObjects(pData, ID);
			for (unsigned int j = 0; j < vEntities.size(); j++)
			{
				vEntities[j]->m_PlotID = ID;
				m_aPlots[ID].m_vObjects.push_back(vEntities[j]);
			}
		} break;
		}
	}
}

void CPlots::WritePlotStats(int ID)
{
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s/%s/%d.plot", Config()->m_SvPlotFilePath, Server()->GetCurrentMapName(), ID);
	std::ofstream PlotFile(aBuf);

	if (PlotFile.is_open())
	{
		int PlotDoorStatus = Collision()->m_pSwitchers ? Collision()->m_pSwitchers[Collision()->GetSwitchByPlot(ID)].m_Status[0] : 0;
		PlotFile << m_aPlots[ID].m_aOwner << "\n";
		PlotFile << m_aPlots[ID].m_aDisplayName << "\n";
		PlotFile << m_aPlots[ID].m_ExpireDate << "\n";
		PlotFile << PlotDoorStatus << "\n";
		
		for (unsigned int i = 0; i < m_aPlots[ID].m_vObjects.size(); i++)
			WritePlotObject(m_aPlots[ID].m_vObjects[i], &PlotFile);

		PlotFile << "\n";
	}
}

void CPlots::WritePlotObject(CEntity *pEntity, std::ofstream *pFile, vec2 *pPos)
{
	vec2 Pos = pPos ? *pPos : pEntity->GetPos();
	char aEntry[128];
	switch (pEntity->GetObjType())
	{
		case CGameWorld::ENTTYPE_PICKUP:
		{
			CPickup *pPickup = (CPickup *)pEntity;
			str_format(aEntry, sizeof(aEntry), "%d:%.2f/%.2f:%d:%d,", CGameWorld::ENTTYPE_PICKUP, Pos.x/32.f, Pos.y/32.f, pPickup->GetType(), pPickup->GetSubtype());
			*pFile << aEntry;
			break;
		}
		case CGameWorld::ENTTYPE_DOOR:
		{
			CDoor *pDoor = (CDoor *)pEntity;
			str_format(aEntry, sizeof(aEntry), "%d:%.2f/%.2f:%.2f:%d:%d:%d:%d:%d:%d,", CGameWorld::ENTTYPE_DOOR, Pos.x/32.f, Pos.y/32.f, pDoor->GetRotation(), pDoor->GetLength(), (int)pDoor->m_Collision, pDoor->GetThickness(), pDoor->m_Number, (int)Collision()->m_pSwitchers[pDoor->m_Number].m_Status[0], pDoor->GetColor());
			*pFile << aEntry;
			break;
		}
		case CGameWorld::ENTTYPE_BUTTON:
		{
			CButton *pButton = (CButton *)pEntity;
			str_format(aEntry, sizeof(aEntry), "%d:%.2f/%.2f:%d,", CGameWorld::ENTTYPE_BUTTON, Pos.x/32.f, Pos.y/32.f, pButton->m_Number);
			*pFile << aEntry;
			break;
		}
		case CGameWorld::ENTTYPE_SPEEDUP:
		{
			CSpeedup *pSpeedup = (CSpeedup *)pEntity;
			str_format(aEntry, sizeof(aEntry), "%d:%.2f/%.2f:%d:%d:%d:%d,", CGameWorld::ENTTYPE_SPEEDUP, Pos.x/32.f, Pos.y/32.f, pSpeedup->GetAngle(), pSpeedup->GetForce(), pSpeedup->GetMaxSpeed(), (int)pSpeedup->IsModeOld());
			*pFile << aEntry;
			break;
		}
		case CGameWorld::ENTTYPE_TELEPORTER:
		{
			CTeleporter *pTeleporter = (CTeleporter *)pEntity;
			str_format(aEntry, sizeof(aEntry), "%d:%.2f/%.2f:%d:%d,", CGameWorld::ENTTYPE_TELEPORTER, Pos.x/32.f, Pos.y/32.f, pTeleporter->GetType(), pTeleporter->m_Number);
			*pFile << aEntry;
			break;
		}
		case CGameWorld::ENTTYPE_DRAWTILE:
		{
			CDrawTile *pDrawTile = (CDrawTile *)pEntity;
			str_format(aEntry, sizeof(aEntry), "%d:%.2f/%.2f:%d:%d:%d,", CGameWorld::ENTTYPE_DRAWTILE, Pos.x/32.f, Pos.y/32.f, pDrawTile->GetIndex(), pDrawTile->GetColor(), pDrawTile->GetTuneNumber());
			*pFile << aEntry;
			break;
		}
	}
}

std::vector<CEntity *> CPlots::ReadPlotObjects(const char *pLine, int PlotID)
{
	const char *pData = pLine;
	std::vector<CEntity *> vEntities;
	std::vector< std::pair<int, int> > vNumbers;
	while (1)
	{
		if (!pData)
			break;

		vec2 Pos = vec2(-1, -1);
		int EntityType = -1;

		sscanf(pData, "%d", &EntityType);
		switch (EntityType)
		{
			case CGameWorld::ENTTYPE_PICKUP:
			{
				int Type = -1;
				int Subtype = -1;
				sscanf(pData, "%d:%f/%f:%d:%d", &EntityType, &Pos.x, &Pos.y, &Type, &Subtype);
				if (Type >= 0 && Subtype >= 0)
				{
					vEntities.push_back(new CPickup(GameWorld(), vec2(Pos.x*32.f, Pos.y*32.f), Type, Subtype));
				}
				break;
			}
			case CGameWorld::ENTTYPE_DOOR:
			{
				float Rotation = -1.f;
				int Length = -1;
				int CollisionActive = -1;
				int Thickness = -1;
				int Number = -1;
				int Status = -1;
				int Color = LASERTYPE_DOOR;
				sscanf(pData, "%d:%f/%f:%f:%d:%d:%d:%d:%d:%d", &EntityType, &Pos.x, &Pos.y, &Rotation, &Length, &CollisionActive, &Thickness, &Number, &Status, &Color);
				if (Rotation >= 0 && Length >= 0 && CollisionActive >= 0 && Thickness >= 0 && Number >= 0 && Status >= 0)
				{
					int NewNumber = -1;
					if (Number == 0)
					{
						NewNumber = 0;
					}
					else
					{
						for (unsigned int i = 0; i < vNumbers.size(); i++)
							if (vNumbers[i].first == Number)
								NewNumber = vNumbers[i].second;

						if (NewNumber == -1)
						{
							if ((int)vNumbers.size() >= Collision()->GetNumMaxDoors(PlotID))
								break;

							NewNumber = Collision()->GetSwitchByPlotLaserDoor(PlotID, vNumbers.size());
							SetPlotDrawDoorStatus(PlotID, vNumbers.size(), Status);

							std::pair<int, int> Pair;
							Pair.first = Number;
							Pair.second = NewNumber;
							vNumbers.push_back(Pair);
						}
					}

					vEntities.push_back(new CDoor(GameWorld(), vec2(Pos.x*32.f, Pos.y*32.f), Rotation, Length, NewNumber, CollisionActive, Thickness, Color));
				}
				break;
			}
			case CGameWorld::ENTTYPE_BUTTON:
			{
				int Number = -1;
				sscanf(pData, "%d:%f/%f:%d", &EntityType, &Pos.x, &Pos.y, &Number);
				if (Number >= 0)
				{
					int NewNumber = -1;
					for (unsigned int i = 0; i < vNumbers.size(); i++)
						if (vNumbers[i].first == Number)
							NewNumber = vNumbers[i].second;

					if (NewNumber == -1)
					{
						if ((int)vNumbers.size() >= Collision()->GetNumMaxDoors(PlotID))
							break;

						NewNumber = Collision()->GetSwitchByPlotLaserDoor(PlotID, vNumbers.size());
						std::pair<int, int> Pair;
						Pair.first = Number;
						Pair.second = NewNumber;
						vNumbers.push_back(Pair);
					}

					vEntities.push_back(new CButton(GameWorld(), vec2(Pos.x*32.f, Pos.y*32.f), NewNumber));
				}
				break;
			}
			case CGameWorld::ENTTYPE_SPEEDUP:
			{
				int Angle = -1;
				int Force = -1;
				int MaxSpeed = -1;
				int ModeOld = 1;
				sscanf(pData, "%d:%f/%f:%d:%d:%d:%d", &EntityType, &Pos.x, &Pos.y, &Angle, &Force, &MaxSpeed, &ModeOld);
				if (Angle >= 0 && Force > 0 && MaxSpeed >= 0)
				{
					vEntities.push_back(new CSpeedup(GameWorld(), vec2(Pos.x*32.f, Pos.y*32.f), Angle, Force, MaxSpeed, ModeOld));
				}
				break;
			}
			case CGameWorld::ENTTYPE_TELEPORTER:
			{
				int Type = 0;
				int Number = -1;
				sscanf(pData, "%d:%f/%f:%d:%d", &EntityType, &Pos.x, &Pos.y, &Type, &Number);
				if (Type > 0 && Number >= 0)
				{
					int NewNumber = -1;
					for (unsigned int i = 0; i < vNumbers.size(); i++)
						if (vNumbers[i].first == Number)
							NewNumber = vNumbers[i].second;

					if (NewNumber == -1)
					{
						if ((int)vNumbers.size() >= Collision()->GetNumMaxTeleporters(PlotID))
							break;

						NewNumber = Collision()->GetSwitchByPlotTeleporter(PlotID, vNumbers.size());
						std::pair<int, int> Pair;
						Pair.first = Number;
						Pair.second = NewNumber;
						vNumbers.push_back(Pair);
					}

					vEntities.push_back(new CTeleporter(GameWorld(), vec2(Pos.x*32.f, Pos.y*32.f), Type, NewNumber));
				}
				break;
			}
			case CGameWorld::ENTTYPE_DRAWTILE:
			{
				int Index = -1;
				int Color = LASERTYPE_RIFLE;
				int TuneNumber = -1;
				sscanf(pData, "%d:%f/%f:%d:%d:%d", &EntityType, &Pos.x, &Pos.y, &Index, &Color, &TuneNumber);
				if (Index > TILE_AIR)
				{
					vEntities.push_back(new CDrawTile(GameWorld(), vec2(Pos.x*32.f, Pos.y*32.f), Index, Color, TuneNumber));
				}
				break;
			}
		}

		// jump to next comma, if it exists skip it so we can start the next loop run with the next data
		if ((pData = str_find(pData, ",")))
			pData++;
	}

	return vEntities;
}

int CPlots::LoadPresetListCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CPlots *pSelf = (CPlots *)pUser;
	if (!IsDir && str_endswith(pName, ".plot"))
	{
		std::string Name = pName;
		Name = Name.erase(Name.size() - 5); // remove .plot
		for (unsigned int i = 0; i < pSelf->m_vPresetList.size(); i++)
			if (pSelf->m_vPresetList[i] == Name)
				return 0;

		pSelf->m_vPresetList.push_back(Name);
	}
	return 0;
}

int CPlots::GetPlotID(int AccID)
{
	if (AccID < ACC_START)
		return 0;

	for (int i = PLOT_START; i < Collision()->m_NumPlots + 1; i++)
		if (str_comp(GameServer()->m_Accounts.Get(AccID).m_Username, m_aPlots[i].m_aOwner) == 0)
			return i;
	return 0;
}

int CPlots::GetTilePlotID(vec2 Pos, bool CheckDoor)
{
	int PlotDoor = CheckDoor ? Collision()->GetPlotBySwitch(Collision()->CheckPointDoor(Pos, 0, true, false)) : 0; // can use team 0 for checkpointdoor because closedonly = false
	return PlotDoor >= PLOT_START ? PlotDoor : Collision()->GetPlotID(Collision()->GetMapIndex(Pos));
}

void CPlots::SetPlotInfo(int PlotID, int AccID)
{
	if (PlotID <= 0 || PlotID > Collision()->m_NumPlots || AccID < ACC_START)
		return;

	str_copy(m_aPlots[PlotID].m_aOwner, GameServer()->m_Accounts.Get(AccID).m_Username, sizeof(m_aPlots[PlotID].m_aOwner));
	str_copy(m_aPlots[PlotID].m_aDisplayName, GameServer()->m_Accounts.Get(AccID).m_aLastPlayerName, sizeof(m_aPlots[PlotID].m_aDisplayName));
	WritePlotStats(PlotID);
}

void CPlots::SetPlotExpire(int PlotID)
{
	if (PlotID <= 0 || PlotID > Collision()->m_NumPlots)
		return;

	int Days = m_aPlots[PlotID].m_Size == 0 ? ITEM_EXPIRE_PLOT_SMALL : m_aPlots[PlotID].m_Size == 1 ? ITEM_EXPIRE_PLOT_BIG : 0;
	GameServer()->SetExpireDateDays(&m_aPlots[PlotID].m_ExpireDate, Days);
}

bool CPlots::HasPlotByIP(int ClientID)
{
	bool HasPlot = false;
	NETADDR Addr;
	Server()->GetClientAddr(ClientID, &Addr);

	for (int i = PLOT_START; i < Collision()->m_NumPlots + 1; i++)
	{
		int ID = GameServer()->m_Accounts.GetAccount(m_aPlots[i].m_aOwner);
		if (ID < ACC_START)
			continue;

		if (GameServer()->m_Accounts.SameIP(ID, &Addr))
			HasPlot = true;

		if (!GameServer()->m_Accounts.IsAccLoggedInThisPort(ID))
			GameServer()->m_Accounts.FreeAccount(ID);

		if (HasPlot)
			break;
	}

	return HasPlot;
}

unsigned int CPlots::GetMaxPlotObjects(int PlotID)
{
	if (PlotID < 0 || PlotID > Collision()->m_NumPlots)
		return 0;

	if (PlotID >= PLOT_START)
	{
		switch (m_aPlots[PlotID].m_Size)
		{
		case PLOT_SMALL: return Config()->m_SvMaxObjectsPlotSmall;
		case PLOT_BIG: return Config()->m_SvMaxObjectsPlotBig;
		}
	}

	return Config()->m_SvMaxObjectsFreeDraw;
}

const char *CPlots::GetPlotSizeString(int PlotID)
{
	if (PlotID <= 0 || PlotID > Collision()->m_NumPlots)
		return "Unknown";

	switch (m_aPlots[PlotID].m_Size)
	{
	case PLOT_SMALL: return "small";
	case PLOT_BIG: return "big";
	}

	return "Unkown";
}

int CPlots::GetMaxPlotSpeedups(int PlotID)
{
	if (PlotID <= 0 || PlotID > Collision()->m_NumPlots)
		return 0; // free draw has unlimited, so doesnt matter

	switch (m_aPlots[PlotID].m_Size)
	{
	case PLOT_SMALL: return 15;
	case PLOT_BIG: return 40;
	}
	return 0;
}

int CPlots::GetMaxPlotTeleporters(int PlotID)
{
	if (PlotID <= 0 || PlotID > Collision()->m_NumPlots)
		return 0; // free draw has unlimited, so doesnt matter

	switch (m_aPlots[PlotID].m_Size)
	{
	case PLOT_SMALL: return 4;
	case PLOT_BIG: return 10;
	}
	return 0;
}

void CPlots::ExpirePlots()
{
	for (int i = PLOT_START; i < Collision()->m_NumPlots + 1; i++)
	{
		if (GameServer()->IsExpired(m_aPlots[i].m_ExpireDate))
		{
			int AccID = GameServer()->m_Accounts.GetAccIDByUsername(m_aPlots[i].m_aOwner);
			if (AccID >= ACC_START)
			{
				int ClientID = GameServer()->m_Accounts.Get(AccID).m_ClientID;
				if (ClientID >= 0 && GameServer()->m_apPlayers[ClientID])
				{
					GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("Your plot expired"));
					GameServer()->m_apPlayers[ClientID]->CancelPlotAuction();
					GameServer()->m_apPlayers[ClientID]->CancelPlotSwap();
					GameServer()->m_apPlayers[ClientID]->StopPlotEditing();
				}
			}

			m_aPlots[i].m_aOwner[0] = 0;
			m_aPlots[i].m_aDisplayName[0] = 0;
			m_aPlots[i].m_ExpireDate = 0;
			m_aPlots[i].m_DestroyEndTick = 0;
			m_aPlots[i].m_DoorHealth = Config()->m_SvPlotDoorHealth;
			ClearPlot(i);
			SetPlotDoorStatus(i, true);
		}
	}
}

void CPlots::SetPlotDoorStatus(int PlotID, bool Close)
{
	if (PlotID <= 0 || PlotID > Collision()->m_NumPlots || !Collision()->m_pSwitchers)
		return;

	int Switch = Collision()->GetSwitchByPlot(PlotID);
	for (int i = 0; i < VANILLA_MAX_CLIENTS; i++)
		Collision()->m_pSwitchers[Switch].m_Status[i] = Close;
}

void CPlots::SetPlotDrawDoorStatus(int PlotID, int Door, bool Close)
{
	if (PlotID <= 0 || PlotID > Collision()->m_NumPlots || Door >= Collision()->GetNumMaxDoors(PlotID) || !Collision()->m_pSwitchers)
		return;

	int Switch = Collision()->GetSwitchByPlotLaserDoor(PlotID, Door);
	for (int i = 0; i < VANILLA_MAX_CLIENTS; i++)
		Collision()->m_pSwitchers[Switch].m_Status[i] = Close;
}

void CPlots::SetPlotDrawDoorStatus(int Number, bool Close)
{
	if (!Collision()->IsPlotDrawDoor(Number) || !Collision()->m_pSwitchers)
		return;

	for (int i = 0; i < VANILLA_MAX_CLIENTS; i++)
		Collision()->m_pSwitchers[Number].m_Status[i] = Close;
}

void CPlots::ClearPlot(int PlotID)
{
	if (PlotID >= 0 && PlotID <= Collision()->m_NumPlots)
	{
		for (unsigned i = 0; i < m_aPlots[PlotID].m_vObjects.size(); i++)
			GameWorld()->DestroyEntity(m_aPlots[PlotID].m_vObjects[i]);
		m_aPlots[PlotID].m_vObjects.clear();
	}
}

void CPlots::RemovePortalsFromPlot(int PlotID)
{
	if (PlotID >= PLOT_START && PlotID <= Collision()->m_NumPlots)
	{
		CPortal *pPortal = (CPortal *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_PORTAL);
		for (; pPortal; pPortal = (CPortal *)pPortal->TypeNext())
		{
			if (GetTilePlotID(pPortal->GetPos(), true) == PlotID)
			{
				pPortal->DestroyLinkedPortal();
				pPortal->Reset();
			}
		}
	}
}

CDrawTile *CPlots::HasDrawTile(int MapIndex, CDrawTile *pMatch)
{
	int BrushCID = -1;
	int Index = -1;
	int TuneNumber = -1;
	bool HasCollision = true;
	if (pMatch)
	{
		BrushCID = pMatch->m_BrushCID;
		Index = pMatch->GetIndex();
		TuneNumber = pMatch->GetTuneNumber();
		HasCollision = pMatch->m_Collision;
	}

	vec2 Pos = GameServer()->RoundPos(Collision()->GetPos(MapIndex));
	int rx = round_to_int(Pos.x) / 32;
	int ry = round_to_int(Pos.y) / 32;
	if (rx <= 0 || rx >= Collision()->GetWidth()-1 || ry <= 0 || ry >= Collision()->GetHeight()-1)
		return 0;

	CDrawTile *apEnts[3]; // game, front, tune is currently possible with draweditor
	int Num = GameWorld()->FindEntities(Pos, 14.0f, (CEntity **)apEnts, 3, CGameWorld::ENTTYPE_DRAWTILE);
	for (int i = 0; i < Num; i++)
	{
		if (apEnts[i]->m_Collision == HasCollision && (
			(BrushCID == -1 || apEnts[i]->m_BrushCID == BrushCID) &&
			(Index == -1 || apEnts[i]->GetIndex() == Index) &&
			(TuneNumber == -1 || apEnts[i]->GetTuneNumber() == TuneNumber)
		))
			return apEnts[i];
	}
	return 0;
}

bool CPlots::IsPlotEmpty(int PlotID)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (GameServer()->GetPlayerChar(i) && GameServer()->GetPlayerChar(i)->GetCurrentTilePlotID(true) == PlotID)
			return false;
	return true;
}

bool CPlots::PlotCanBeRaided(int PlotID)
{
	return PlotID >= PLOT_START && m_aPlots[PlotID].m_DestroyEndTick > Server()->Tick() && Config()->m_SvPoliceTaserPlotRaid;
}

bool CPlots::PlotDoorDestroyed(int PlotID)
{
	return m_aPlots[PlotID].m_DoorHealth <= 0;
}

bool CPlots::OnPlotDoorTaser(int PlotID, int TaserStrength, int ClientID, vec2 Pos)
{
	if (PlotID < PLOT_START || !PlotCanBeRaided(PlotID) || m_aPlots[PlotID].m_DoorHealth <= 0)
		return false;

	int Diff = TaserStrength - maximum(TaserStrength - m_aPlots[PlotID].m_DoorHealth, 0);
	m_aPlots[PlotID].m_DoorHealth -= Diff;
	GameServer()->CreateDamage(Pos, ClientID, vec2(0, 0), Diff, 0, false);

	if (m_aPlots[PlotID].m_DoorHealth <= 0)
	{
		m_aPlots[PlotID].m_DoorHealth = 0;
		GameServer()->SendBroadcast("", ClientID, false);
		GameServer()->CreateDeath(Pos, ClientID);
		int AccID = GameServer()->m_Accounts.GetAccIDByUsername(m_aPlots[PlotID].m_aOwner);
		if (AccID >= ACC_START)
		{
			int PlotOwner = GameServer()->m_Accounts.Get(AccID).m_ClientID;
			if (PlotOwner >= 0 && GameServer()->m_apPlayers[PlotOwner])
			{
				GameServer()->SendChatTarget(PlotOwner, GameServer()->m_apPlayers[PlotOwner]->Localize("The police have gained access to your plot in hopes of finding you"));
			}
		}
		return true;
	}

	GameServer()->SendBroadcastFormat(ClientID, true, Localizable("Plot %d Door Health [%d/%d]"), PlotID, m_aPlots[PlotID].m_DoorHealth, Config()->m_SvPlotDoorHealth);
	return false;
}

bool CPlots::SetPlotDestroyEndTick(int PlotID, int64 NewEndTick)
{
	if (PlotID >= PLOT_START)
	{
		m_aPlots[PlotID].m_DestroyEndTick = NewEndTick;
		return true;
	}
	return false;
}

bool CPlots::CanInsertNewEntity(int PlotID)
{
	return m_aPlots[PlotID].m_vObjects.size() < GetMaxPlotObjects(PlotID);
}

bool CPlots::InsertPlotDrawEntity(CEntity *pEntity)
{
	if (pEntity->m_PlotID == -1)
		return false;
	m_aPlots[pEntity->m_PlotID].m_vObjects.push_back(pEntity);
	return true;
}

bool CPlots::ErasePlotDrawEntity(CEntity *pEntity)
{
	for (unsigned i = 0; i < m_aPlots[pEntity->m_PlotID].m_vObjects.size(); i++)
		if (m_aPlots[pEntity->m_PlotID].m_vObjects[i] == pEntity)
		{
			m_aPlots[pEntity->m_PlotID].m_vObjects.erase(m_aPlots[pEntity->m_PlotID].m_vObjects.begin() + i);
			return true;
		}
	return false;
}

const char *CPlots::GetPlotExpireDate(int PlotID)
{
	if (m_aPlots[PlotID].m_ExpireDate == 0)
		return "";
	return GameServer()->GetDate(m_aPlots[PlotID].m_ExpireDate);
}

void CPlots::PrintPlotInfo(int PlotID)
{
	if (PlotID <= 0 || PlotID > Collision()->m_NumPlots)
		return;

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "Owner account: %s", m_aPlots[PlotID].m_aOwner);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "plot", aBuf);
	str_format(aBuf, sizeof(aBuf), "Size: %s", GetPlotSizeString(PlotID));
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "plot", aBuf);
	str_format(aBuf, sizeof(aBuf), "Expire date: %s", m_aPlots[PlotID].m_ExpireDate == 0 ? "" : GameServer()->GetDate(m_aPlots[PlotID].m_ExpireDate));
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "plot", aBuf);
	str_format(aBuf, sizeof(aBuf), "Door status: %d", Collision()->m_pSwitchers ? Collision()->m_pSwitchers[Collision()->GetSwitchByPlot(PlotID)].m_Status[0] : 0);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "plot", aBuf);
	str_format(aBuf, sizeof(aBuf), "Destroy Seconds: %lld", m_aPlots[PlotID].m_DestroyEndTick ? (m_aPlots[PlotID].m_DestroyEndTick - Server()->Tick()) / Server()->TickSpeed() : 0);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "plot", aBuf);
	str_format(aBuf, sizeof(aBuf), "Door Health: %d", m_aPlots[PlotID].m_DoorHealth);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "plot", aBuf);
}
