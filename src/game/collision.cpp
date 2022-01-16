/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>

#include <antibot/antibot_data.h>

#include <math.h>
#include <engine/map.h>
#include <engine/kernel.h>

#include <game/mapitems.h>
#include <game/layers.h>
#include <game/collision.h>

#include <engine/shared/config.h>

vec2 ClampVel(int MoveRestrictions, vec2 Vel)
{
	if(Vel.x > 0 && (MoveRestrictions&CANTMOVE_RIGHT))
	{
		Vel.x = 0;
	}
	if(Vel.x < 0 && (MoveRestrictions&CANTMOVE_LEFT))
	{
		Vel.x = 0;
	}
	if(Vel.y > 0 && (MoveRestrictions&CANTMOVE_DOWN))
	{
		Vel.y = 0;
	}
	if(Vel.y < 0 && (MoveRestrictions&CANTMOVE_UP))
	{
		Vel.y = 0;
	}
	return Vel;
}

CCollision::CCollision()
{
	m_pTiles = 0;
	m_Width = 0;
	m_Height = 0;
	m_pLayers = 0;

	m_pTele = 0;
	m_pSpeedup = 0;
	m_pFront = 0;
	m_pSwitch = 0;
	m_pDoor = 0;
	m_pSwitchers = 0;
	m_pTune = 0;
	m_apPlotSize = 0;
}

CCollision::~CCollision()
{
	Dest();
}

void CCollision::Init(class CLayers* pLayers, class CConfig *pConfig)
{
	Dest();
	m_NumSwitchers = 0;
	m_NumPlots = 0;
	for (int i = 0; i < NUM_PLOT_SIZES; i++)
		m_aNumPlots[i] = 0;
	m_pLayers = pLayers;
	m_pConfig = pConfig;
	m_Width = m_pLayers->GameLayer()->m_Width;
	m_Height = m_pLayers->GameLayer()->m_Height;
	m_pTiles = static_cast<CTile*>(m_pLayers->Map()->GetData(m_pLayers->GameLayer()->m_Data));

	if (m_pLayers->TeleLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->TeleLayer()->m_Tele);
		if (Size >= m_Width * m_Height * sizeof(CTeleTile))
			m_pTele = static_cast<CTeleTile*>(m_pLayers->Map()->GetData(m_pLayers->TeleLayer()->m_Tele));
	}

	if (m_pLayers->SpeedupLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->SpeedupLayer()->m_Speedup);
		if (Size >= m_Width * m_Height * sizeof(CSpeedupTile))
			m_pSpeedup = static_cast<CSpeedupTile*>(m_pLayers->Map()->GetData(m_pLayers->SpeedupLayer()->m_Speedup));
	}

	if (m_pLayers->SwitchLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->SwitchLayer()->m_Switch);
		if (Size >= m_Width * m_Height * sizeof(CSwitchTile))
			m_pSwitch = static_cast<CSwitchTile*>(m_pLayers->Map()->GetData(m_pLayers->SwitchLayer()->m_Switch));

		m_pDoor = new CDoorTile[m_Width * m_Height];
		mem_zero(m_pDoor, m_Width * m_Height * sizeof(CDoorTile));
	}
	else
	{
		m_pDoor = 0;
		m_pSwitchers = 0;
	}

	if (m_pLayers->TuneLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->TuneLayer()->m_Tune);
		if (Size >= m_Width * m_Height * sizeof(CTuneTile))
			m_pTune = static_cast<CTuneTile*>(m_pLayers->Map()->GetData(m_pLayers->TuneLayer()->m_Tune));
	}

	if (m_pLayers->FrontLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->FrontLayer()->m_Front);
		if (Size >= m_Width * m_Height * sizeof(CTile))
			m_pFront = static_cast<CTile*>(m_pLayers->Map()->GetData(m_pLayers->FrontLayer()->m_Front));
	}

	for (int i = 0; i < m_Width * m_Height; i++)
	{
		int Index;
		if (m_pSwitch)
		{
			Index = m_pSwitch[i].m_Type;
			bool IsPlot = IsPlotTile(Index);

			if (IsPlot && m_pSwitch[i].m_Number > m_NumPlots)
				m_NumPlots = m_pSwitch[i].m_Number;

			if (!IsPlot && m_pSwitch[i].m_Number > m_NumSwitchers)
				m_NumSwitchers = m_pSwitch[i].m_Number;

			/*if (!IsPlot && m_pSwitch[i].m_Number)
				m_pDoor[i].m_Number = m_pSwitch[i].m_Number;
			else
				m_pDoor[i].m_Number = 0;*/

			if (Index <= TILE_NPH_START)
			{
				if ((Index >= TILE_JUMP && Index <= TILE_BONUS)
					|| Index == TILE_ALLOW_TELE_GUN
					|| Index == TILE_ALLOW_BLUE_TELE_GUN)
					m_pSwitch[i].m_Type = Index;
				else
					m_pSwitch[i].m_Type = 0;
			}
		}
	}

	// F-DDrace
	if (m_pSwitch && m_pDoor)
	{
		m_apPlotSize = (int *)calloc(m_NumPlots+1, sizeof(int)*(m_NumPlots+1));
		m_apPlotSize[0] = -1; // plot id 0

		// loop over the map again and correctly set the plot numbers
		for (int i = 0; i < m_Width * m_Height; i++)
		{
			if (IsPlotTile(m_pSwitch[i].m_Type) && m_pSwitch[i].m_Number > 0)
			{
				int PlotID = m_pSwitch[i].m_Number;
				m_pSwitch[i].m_Number = GetSwitchByPlot(PlotID);

				if (m_pSwitch[i].m_Type == TILE_SWITCH_PLOT_DOOR)
				{
					AddDoorTile(i, TILE_STOPA, m_pSwitch[i].m_Number);
				}
				else if (m_pSwitch[i].m_Type == TILE_SWITCH_PLOT_TOTELE)
				{
					if (m_pSwitch[i].m_Delay >= 0 && m_pSwitch[i].m_Delay < NUM_PLOT_SIZES)
					{
						m_aNumPlots[m_pSwitch[i].m_Delay]++;
						m_apPlotSize[PlotID] = m_pSwitch[i].m_Delay;
					}
				}
			}
		}
	}

	if (m_NumSwitchers || m_NumPlots)
	{
		m_pSwitchers = new SSwitchers[GetNumAllSwitchers() + 1]; // always 256, GetNumFreeDrawDoors() fills up the rest from whats available up to 256

		for (int i = 0; i < GetNumAllSwitchers() + 1; ++i)
		{
			m_pSwitchers[i].m_Initial = true;
			for (int j = 0; j < MAX_CLIENTS; ++j)
			{
				m_pSwitchers[i].m_Status[j] = true;
				m_pSwitchers[i].m_EndTick[j] = 0;
				m_pSwitchers[i].m_Type[j] = 0;
				// F-DDrace
				m_pSwitchers[i].m_ClientID[j] = -1;
				m_pSwitchers[i].m_StartTick[j] = 0;
			}
		}
	}
}

void CCollision::FillAntibot(CAntibotMapData *pMapData)
{
	pMapData->m_Width = m_Width;
	pMapData->m_Height = m_Height;
	pMapData->m_pTiles = (unsigned char *)malloc((size_t)m_Width * m_Height);
	for(int i = 0; i < m_Width * m_Height; i++)
	{
		pMapData->m_pTiles[i] = 0;
		if(m_pTiles[i].m_Index >= TILE_SOLID && m_pTiles[i].m_Index <= TILE_NOLASER)
		{
			pMapData->m_pTiles[i] = m_pTiles[i].m_Index;
		}
	}
}

enum
{
	MR_DIR_HERE=0,
	MR_DIR_RIGHT,
	MR_DIR_DOWN,
	MR_DIR_LEFT,
	MR_DIR_UP,
	NUM_MR_DIRS
};

static vec2 DirVec(int Direction)
{
	switch(Direction)
	{
	case MR_DIR_HERE: return vec2(0, 0);
	case MR_DIR_RIGHT: return vec2(1, 0);
	case MR_DIR_DOWN: return vec2(0, 1);
	case MR_DIR_LEFT: return vec2(-1, 0);
	case MR_DIR_UP: return vec2(0, -1);
	default: dbg_assert(false, "invalid dir");
	}
	return vec2(0, 0);
}

static int Twoway(int MoveRestrictions)
{
	if(MoveRestrictions&CANTMOVE_LEFT)
	{
		MoveRestrictions |= CANTMOVE_LEFT_TWOWAY;
	}
	if(MoveRestrictions&CANTMOVE_RIGHT)
	{
		MoveRestrictions |= CANTMOVE_RIGHT_TWOWAY;
	}
	if(MoveRestrictions&CANTMOVE_UP)
	{
		MoveRestrictions |= CANTMOVE_UP_TWOWAY;
	}
	if(MoveRestrictions&CANTMOVE_DOWN)
	{
		MoveRestrictions |= CANTMOVE_DOWN_TWOWAY;
	}
	return MoveRestrictions;
}

static int Here(int MoveRestrictions)
{
	if(MoveRestrictions&CANTMOVE_LEFT)
	{
		MoveRestrictions |= CANTMOVE_LEFT_HERE;
	}
	if(MoveRestrictions&CANTMOVE_RIGHT)
	{
		MoveRestrictions |= CANTMOVE_RIGHT_HERE;
	}
	if(MoveRestrictions&CANTMOVE_UP)
	{
		MoveRestrictions |= CANTMOVE_UP_HERE;
	}
	if(MoveRestrictions&CANTMOVE_DOWN)
	{
		MoveRestrictions |= CANTMOVE_DOWN_HERE;
	}
	return MoveRestrictions;
}

static int GetMoveRestrictionsRaw(int Direction, int Tile, int Flags, CCollision::MoveRestrictionExtra Extra)
{
	switch(Tile)
	{
	case TILE_STOP:
		{
			int MoveRestrictions = 0;
			switch(Flags)
			{
			case ROTATION_0: MoveRestrictions = CANTMOVE_DOWN; break;
			case ROTATION_90: MoveRestrictions = CANTMOVE_LEFT; break;
			case ROTATION_180: MoveRestrictions = CANTMOVE_UP; break;
			case ROTATION_270: MoveRestrictions = CANTMOVE_RIGHT; break;
			}
			return Direction == MR_DIR_HERE ? Here(MoveRestrictions) : MoveRestrictions;
		}
	case TILE_STOPS:
		switch(Flags)
		{
		case ROTATION_0:
		case ROTATION_180:
			return Twoway(CANTMOVE_DOWN|CANTMOVE_UP);
		case ROTATION_90:
		case ROTATION_270:
			return Twoway(CANTMOVE_LEFT|CANTMOVE_RIGHT);
		}
		break;
	case TILE_STOPA:
		return Twoway(CANTMOVE_LEFT|CANTMOVE_RIGHT|CANTMOVE_UP|CANTMOVE_DOWN);
		// F-DDrace
	case TILE_ROOM:
		if (!Extra.m_CanEnterRoom)
			return Twoway(CANTMOVE_LEFT|CANTMOVE_RIGHT|CANTMOVE_UP|CANTMOVE_DOWN)|CANTMOVE_ROOM;
		break;
	}
	return 0;
}

static int GetMoveRestrictionsMask(int Direction)
{
	switch(Direction)
	{
	case MR_DIR_HERE: return 0;
	case MR_DIR_RIGHT: return Twoway(CANTMOVE_RIGHT);
	case MR_DIR_DOWN: return Twoway(CANTMOVE_DOWN);
	case MR_DIR_LEFT: return Twoway(CANTMOVE_LEFT);
	case MR_DIR_UP: return Twoway(CANTMOVE_UP);
	default: dbg_assert(false, "invalid dir");
	}
	return 0;
}

static int GetMoveRestrictions(int Direction, int Tile, int Flags, CCollision::MoveRestrictionExtra Extra)
{
	int Result = GetMoveRestrictionsRaw(Direction, Tile, Flags, Extra);
	// Generally, stoppers only have an effect if they block us from moving
	// *onto* them. The one exception is one-way blockers, they can also
	// block us from moving if we're on top of them.
	if(Direction == MR_DIR_HERE && Tile == TILE_STOP)
	{
		return Result;
	}

	// F-DDrace
	int Extras = 0;
	if (Tile == TILE_ROOM && !Extra.m_CanEnterRoom)
		Extras |= CANTMOVE_ROOM;

	return (Result&GetMoveRestrictionsMask(Direction))|Extras;
}

int CCollision::GetMoveRestrictions(CALLBACK_SWITCHACTIVE pfnSwitchActive, void *pUser, vec2 Pos, float Distance, int OverrideCenterTileIndex, MoveRestrictionExtra Extra)
{
	dbg_assert(0.0f <= Distance && Distance <= 32.0f, "invalid distance");
	int Restrictions = 0;
	for(int d = 0; d < NUM_MR_DIRS; d++)
	{
		vec2 ModPos = Pos + DirVec(d) * Distance;
		int ModMapIndex = GetPureMapIndex(ModPos);
		if(d == MR_DIR_HERE && OverrideCenterTileIndex >= 0)
		{
			ModMapIndex = OverrideCenterTileIndex;
		}
		for(int Front = 0; Front < 2; Front++)
		{
			int Tile;
			int Flags;
			if(!Front)
			{
				Tile = GetTileIndex(ModMapIndex);
				Flags = GetTileFlags(ModMapIndex);
			}
			else
			{
				Tile = GetFTileIndex(ModMapIndex);
				Flags = GetFTileFlags(ModMapIndex);
			}
			Restrictions |= ::GetMoveRestrictions(d, Tile, Flags, Extra);
		}
		if(pfnSwitchActive)
		{
			int Num = m_pDoor[ModMapIndex].m_vTiles.size();
			for (int i = 0; i < Num; i++)
			{
				if (pfnSwitchActive(m_pDoor[ModMapIndex].m_vTiles[i].m_Number, pUser))
				{
					int Tile = m_pDoor[ModMapIndex].m_vTiles[i].m_Index;
					int Flags = m_pDoor[ModMapIndex].m_vTiles[i].m_Flags;

					// F-DDrace
					int DoorRestrictions = ::GetMoveRestrictions(d, Tile, Flags, Extra);
					Restrictions |= DoorRestrictions;

					if (DoorRestrictions & CANTMOVE_DOWN)
						Restrictions |= CANTMOVE_DOWN_LASERDOOR;
				}
			}
			/*for (int Fight = 0; Fight < 2; Fight++)
			{
				int DoorNumber;
				if (Fight)
				{
					DoorNumber = GetFightNumber(ModMapIndex, false);
					if (!DoorNumber || IsSolid(ModPos.x, ModPos.y))
						continue;
				}
				else
				{
					DoorNumber = GetDTileNumber(ModMapIndex);
				}
				
				
			}*/
		}
	}
	return Restrictions;
}

int CCollision::GetTile(int x, int y)
{
	if (!m_pTiles)
		return 0;

	int Nx = clamp(x / 32, 0, m_Width - 1);
	int Ny = clamp(y / 32, 0, m_Height - 1);
	int pos = Ny * m_Width + Nx;

	if (m_pTiles[pos].m_Index >= TILE_SOLID && m_pTiles[pos].m_Index <= TILE_NOLASER)
		return m_pTiles[pos].m_Index;
	return 0;
}

// TODO: rewrite this smarter!
int CCollision::IntersectLine(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision)
{
	const int End = distance(Pos0, Pos1)+1;
	const float InverseEnd = 1.0f/End;
	vec2 Last = Pos0;
	int ix = 0, iy = 0; // Temporary position for checking collision
	for (int i = 0; i <= End; i++)
	{
		vec2 Pos = mix(Pos0, Pos1, i*InverseEnd);
		ix = round_to_int(Pos.x);
		iy = round_to_int(Pos.y);

		if (CheckPoint(ix, iy))
		{
			if (pOutCollision)
				* pOutCollision = Pos;
			if (pOutBeforeCollision)
				* pOutBeforeCollision = Last;
			return GetCollisionAt(ix, iy);
		}

		Last = Pos;
	}
	if (pOutCollision)
		* pOutCollision = Pos1;
	if (pOutBeforeCollision)
		* pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectLineTeleHook(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision, int* pTeleNr)
{
	const int End = distance(Pos0, Pos1)+1;
	const float InverseEnd = 1.0f/End;
	vec2 Last = Pos0;
	int ix = 0, iy = 0; // Temporary position for checking collision
	int dx = 0, dy = 0; // Offset for checking the "through" tile
	ThroughOffset(Pos0, Pos1, &dx, &dy);
	for (int i = 0; i <= End; i++)
	{
		vec2 Pos = mix(Pos0, Pos1, i*InverseEnd);
		ix = round_to_int(Pos.x);
		iy = round_to_int(Pos.y);

		int Index = GetPureMapIndex(Pos);
		if (m_pConfig->m_SvOldTeleportHook)
			* pTeleNr = IsTeleport(Index);
		else
			*pTeleNr = IsTeleportHook(Index);
		if (*pTeleNr)
		{
			if (pOutCollision)
				* pOutCollision = Pos;
			if (pOutBeforeCollision)
				* pOutBeforeCollision = Last;
			return TILE_TELEINHOOK;
		}

		int hit = 0;
		if (CheckPoint(ix, iy))
		{
			if (!IsThrough(ix, iy, dx, dy, Pos0, Pos1))
				hit = GetCollisionAt(ix, iy);
		}
		else if (IsHookBlocker(ix, iy, Pos0, Pos1))
		{
			hit = TILE_NOHOOK;
		}
		if (hit)
		{
			if (pOutCollision)
				* pOutCollision = Pos;
			if (pOutBeforeCollision)
				* pOutBeforeCollision = Last;
			return hit;
		}

		Last = Pos;
	}
	if (pOutCollision)
		* pOutCollision = Pos1;
	if (pOutBeforeCollision)
		* pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectLineTeleWeapon(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision, int* pTeleNr)
{
	const int End = distance(Pos0, Pos1)+1;
	const float InverseEnd = 1.0f/End;
	vec2 Last = Pos0;
	int ix = 0, iy = 0; // Temporary position for checking collision
	for (int i = 0; i <= End; i++)
	{
		vec2 Pos = mix(Pos0, Pos1, i*InverseEnd);
		ix = round_to_int(Pos.x);
		iy = round_to_int(Pos.y);

		int Index = GetPureMapIndex(Pos);
		if (m_pConfig->m_SvOldTeleportWeapons)
			* pTeleNr = IsTeleport(Index);
		else
			*pTeleNr = IsTeleportWeapon(Index);
		if (*pTeleNr)
		{
			if (pOutCollision)
				* pOutCollision = Pos;
			if (pOutBeforeCollision)
				* pOutBeforeCollision = Last;
			return TILE_TELEINWEAPON;
		}

		if (CheckPoint(ix, iy))
		{
			if (pOutCollision)
				* pOutCollision = Pos;
			if (pOutBeforeCollision)
				* pOutBeforeCollision = Last;
			return GetCollisionAt(ix, iy);
		}

		Last = Pos;
	}
	if (pOutCollision)
		* pOutCollision = Pos1;
	if (pOutBeforeCollision)
		* pOutBeforeCollision = Pos1;
	return 0;
}

// TODO: OPT: rewrite this smarter!
void CCollision::MovePoint(vec2* pInoutPos, vec2* pInoutVel, float Elasticity, int* pBounces)
{
	if (pBounces)
		* pBounces = 0;

	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;
	if (CheckPoint(Pos + Vel))
	{
		int Affected = 0;
		if (CheckPoint(Pos.x + Vel.x, Pos.y))
		{
			pInoutVel->x *= -Elasticity;
			if (pBounces)
				(*pBounces)++;
			Affected++;
		}

		if (CheckPoint(Pos.x, Pos.y + Vel.y))
		{
			pInoutVel->y *= -Elasticity;
			if (pBounces)
				(*pBounces)++;
			Affected++;
		}

		if (Affected == 0)
		{
			pInoutVel->x *= -Elasticity;
			pInoutVel->y *= -Elasticity;
		}
	}
	else
	{
		*pInoutPos = Pos + Vel;
	}
}

bool CCollision::TestBox(vec2 Pos, vec2 Size)
{
	Size *= 0.5f;
	if (CheckPoint(Pos.x - Size.x, Pos.y - Size.y))
		return true;
	if (CheckPoint(Pos.x + Size.x, Pos.y - Size.y))
		return true;
	if (CheckPoint(Pos.x - Size.x, Pos.y + Size.y))
		return true;
	if (CheckPoint(Pos.x + Size.x, Pos.y + Size.y))
		return true;
	return false;
}

static float *DirCoord(int Direction, vec2 *pVec)
{
	switch(Direction)
	{
	case MR_DIR_RIGHT: case MR_DIR_LEFT: return &pVec->x;
	case MR_DIR_DOWN: case MR_DIR_UP: return &pVec->y;
	default: dbg_assert(false, "invalid dir");
	}
	return 0;
}

static int DirTwoway(int Direction)
{
	switch(Direction)
	{
	case MR_DIR_HERE: return 0;
	case MR_DIR_RIGHT: return CANTMOVE_RIGHT_TWOWAY;
	case MR_DIR_DOWN: return CANTMOVE_DOWN_TWOWAY;
	case MR_DIR_LEFT: return CANTMOVE_LEFT_TWOWAY;
	case MR_DIR_UP: return CANTMOVE_UP_TWOWAY;
	default: dbg_assert(false, "invalid dir");
	}
	return 0;
}

static int DirHere(int Direction)
{
	switch(Direction)
	{
	case MR_DIR_HERE: return 0;
	case MR_DIR_RIGHT: return CANTMOVE_RIGHT_HERE;
	case MR_DIR_DOWN: return CANTMOVE_DOWN_HERE;
	case MR_DIR_LEFT: return CANTMOVE_LEFT_HERE;
	case MR_DIR_UP: return CANTMOVE_UP_HERE;
	default: dbg_assert(false, "invalid dir");
	}
	return 0;
}

static int DirSign(int Direction)
{
	switch(Direction)
	{
	case MR_DIR_HERE: return 0;
	case MR_DIR_RIGHT: case MR_DIR_DOWN: return 1;
	case MR_DIR_LEFT: case MR_DIR_UP: return -1;
	default: dbg_assert(false, "invalid dir");
	}
	return 0;
}

void CCollision::MoveBox(CALLBACK_SWITCHACTIVE pfnSwitchActive, void *pUser, vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity, bool CheckStoppers, MoveRestrictionExtra Extra)
{
	if (Size.x > ms_MinStaticPhysSize || Size.y > ms_MinStaticPhysSize)
	{
		MoveBoxBig(pInoutPos, pInoutVel, Size, Elasticity);
		return;
	}

	// do the move
	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;

	const float Distance = length(Vel);
	const int Max = (int)Distance;

	if (Distance > 0.00001f)
	{
		const float Fraction = 1.0f/(Max+1);
		for (int i = 0; i <= Max; i++)
		{
			// Early break as optimization to stop checking for collisions for
			// large distances after the obstacles we have already hit reduced
			// our speed to exactly 0.
			if (Vel == vec2(0, 0))
			{
				break;
			}

			vec2 NewPos = Pos + Vel * Fraction; // TODO: this row is not nice

			if (TestBox(vec2(NewPos.x, NewPos.y), Size))
			{
				int Hits = 0;

				if (TestBox(vec2(Pos.x, NewPos.y), Size))
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					Hits++;
				}

				if (TestBox(vec2(NewPos.x, Pos.y), Size))
				{
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
					Hits++;
				}

				// neither of the tests got a collision.
				// this is a real _corner case_!
				if (Hits == 0)
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
				}
			}

			if(CheckStoppers)
			{
				// Yay. Backward-compatibility. Isn't that fun?
				//
				// Since you previously managed to get through
				// stoppers (or into them) at high speeds, some
				// maps started using it. A lot of maps
				// actually. So we have to maintain bug-for-bug
				// compatibility.
				//
				// The strategy for still preventing players to
				// go through stoppers is as follows: If you're
				// going so fast that you'd skip a stopper, you
				// will instead be stopped at the last possible
				// position where the stopper still has
				// influence on you.
				//
				// We have to differentiate between one-way
				// stoppers and multiple-way stoppers.
				//
				// One-way stoppers affect you until your
				// center leaves the tile, so we just have to
				// stop you from exiting the stopper in the
				// wrong direction.
				//
				// Multiple-way stoppers affect you in a more
				// complicated way: If you're blocked from,
				// e.g. the right, then you're blocked as long
				// as the position 18 units to your right is in
				// that stopper. So we have to stop you from
				// getting the position 18 units away from you
				// out of that stopper tile in the wrong
				// direction.
				//
				// Backward-compatibility. \o/
				static const float OFFSET = 18.0f;
				int MoveRestrictions = GetMoveRestrictions(pfnSwitchActive, pUser, Pos, OFFSET, -1, Extra);
				for(int d = 1; d < NUM_MR_DIRS; d++)
				{
					static const int TILESIZE = 32;
					float *pPos = DirCoord(d, &Pos);
					float *pNewPos = DirCoord(d, &NewPos);
					float *pVel = DirCoord(d, &Vel);
					int Sign = DirSign(d);
					// Are we actually going in the
					// direction we're checking?
					if(*pVel * Sign <= 0)
					{
						continue;
					}
					bool Stop = false;
					if(MoveRestrictions&DirTwoway(d)
						&& round_to_int(*pPos + OFFSET * Sign) / TILESIZE != round_to_int(*pNewPos + OFFSET * Sign) / TILESIZE)
					{
						Stop = true;
					}
					if(MoveRestrictions&DirHere(d)
						&& round_to_int(*pPos) / TILESIZE != round_to_int(*pNewPos) / TILESIZE)
					{
						Stop = true;
					}
					if(Stop)
					{
						*pVel = 0;
						*pNewPos = *pPos;
					}
				}
			}

			Pos = NewPos;
		}
	}

	*pInoutPos = Pos;
	*pInoutVel = Vel;
}

// DDRace

void CCollision::Dest()
{
	if (m_pDoor)
		delete[] m_pDoor;
	if (m_pSwitchers)
		delete[] m_pSwitchers;
	if (m_apPlotSize)
		delete[] m_apPlotSize;
	m_pTiles = 0;
	m_Width = 0;
	m_Height = 0;
	m_pLayers = 0;
	m_pTele = 0;
	m_pSpeedup = 0;
	m_pFront = 0;
	m_pSwitch = 0;
	m_pTune = 0;
	m_pDoor = 0;
	m_pSwitchers = 0;
	m_apPlotSize = 0;
}

int CCollision::IsSolid(int x, int y)
{
	int index = GetTile(x, y);
	return index == TILE_SOLID || index == TILE_NOHOOK;
}

bool CCollision::IsThrough(int x, int y, int xoff, int yoff, vec2 pos0, vec2 pos1)
{
	int pos = GetPureMapIndex(x, y);
	if (m_pFront && (m_pFront[pos].m_Index == TILE_THROUGH_ALL || m_pFront[pos].m_Index == TILE_THROUGH_CUT))
		return true;
	if (m_pFront && m_pFront[pos].m_Index == TILE_THROUGH_DIR && (
		(m_pFront[pos].m_Flags == ROTATION_0 && pos0.y > pos1.y) ||
		(m_pFront[pos].m_Flags == ROTATION_90 && pos0.x < pos1.x) ||
		(m_pFront[pos].m_Flags == ROTATION_180 && pos0.y < pos1.y) ||
		(m_pFront[pos].m_Flags == ROTATION_270 && pos0.x > pos1.x)))
		return true;
	int offpos = GetPureMapIndex(x + xoff, y + yoff);
	if (m_pTiles[offpos].m_Index == TILE_THROUGH || (m_pFront && m_pFront[offpos].m_Index == TILE_THROUGH))
		return true;
	return false;
}

bool CCollision::IsHookBlocker(int x, int y, vec2 pos0, vec2 pos1)
{
	int pos = GetPureMapIndex(x, y);
	if (m_pTiles[pos].m_Index == TILE_THROUGH_ALL || (m_pFront && m_pFront[pos].m_Index == TILE_THROUGH_ALL))
		return true;
	if (m_pTiles[pos].m_Index == TILE_THROUGH_DIR && (
		(m_pTiles[pos].m_Flags == ROTATION_0 && pos0.y < pos1.y) ||
		(m_pTiles[pos].m_Flags == ROTATION_90 && pos0.x > pos1.x) ||
		(m_pTiles[pos].m_Flags == ROTATION_180 && pos0.y > pos1.y) ||
		(m_pTiles[pos].m_Flags == ROTATION_270 && pos0.x < pos1.x)))
		return true;
	if (m_pFront && m_pFront[pos].m_Index == TILE_THROUGH_DIR && (
		(m_pFront[pos].m_Flags == ROTATION_0 && pos0.y < pos1.y) ||
		(m_pFront[pos].m_Flags == ROTATION_90 && pos0.x > pos1.x) ||
		(m_pFront[pos].m_Flags == ROTATION_180 && pos0.y > pos1.y) ||
		(m_pFront[pos].m_Flags == ROTATION_270 && pos0.x < pos1.x)))
		return true;
	return false;
}

int CCollision::IsWallJump(int Index)
{
	if (Index < 0)
		return 0;

	return m_pTiles[Index].m_Index == TILE_WALLJUMP;
}

int CCollision::IsNoLaser(int x, int y)
{
	return (CCollision::GetTile(x, y) == TILE_NOLASER);
}

int CCollision::IsFNoLaser(int x, int y)
{
	return (CCollision::GetFTile(x, y) == TILE_NOLASER);
}

int CCollision::IsTeleport(int Index)
{
	if (Index < 0 || !m_pTele)
		return 0;

	if (m_pTele[Index].m_Type == TILE_TELEIN)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsEvilTeleport(int Index)
{
	if (Index < 0)
		return 0;
	if (!m_pTele)
		return 0;

	if (m_pTele[Index].m_Type == TILE_TELEINEVIL)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsCheckTeleport(int Index)
{
	if (Index < 0)
		return 0;
	if (!m_pTele)
		return 0;

	if (m_pTele[Index].m_Type == TILE_TELECHECKIN)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsCheckEvilTeleport(int Index)
{
	if (Index < 0)
		return 0;
	if (!m_pTele)
		return 0;

	if (m_pTele[Index].m_Type == TILE_TELECHECKINEVIL)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsTCheckpoint(int Index)
{
	if (Index < 0)
		return 0;

	if (!m_pTele)
		return 0;

	if (m_pTele[Index].m_Type == TILE_TELECHECK)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsTeleportWeapon(int Index)
{
	if (Index < 0 || !m_pTele)
		return 0;

	if (m_pTele[Index].m_Type == TILE_TELEINWEAPON)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsTeleportHook(int Index)
{
	if (Index < 0 || !m_pTele)
		return 0;

	if (m_pTele[Index].m_Type == TILE_TELEINHOOK)
		return m_pTele[Index].m_Number;

	return 0;
}


int CCollision::IsSpeedup(int Index)
{
	if (Index < 0 || !m_pSpeedup)
		return 0;

	if (m_pSpeedup[Index].m_Force > 0)
		return Index;

	return 0;
}

int CCollision::IsTune(int Index)
{
	if (Index < 0 || !m_pTune)
		return 0;

	if (m_pTune[Index].m_Type)
		return m_pTune[Index].m_Number;

	return 0;
}

void CCollision::GetSpeedup(int Index, vec2* Dir, int* Force, int* MaxSpeed)
{
	if (Index < 0 || !m_pSpeedup)
		return;
	float Angle = m_pSpeedup[Index].m_Angle * (pi / 180.0f);
	*Force = m_pSpeedup[Index].m_Force;
	*Dir = vec2(cos(Angle), sin(Angle));
	if (MaxSpeed)
		* MaxSpeed = m_pSpeedup[Index].m_MaxSpeed;
}

int CCollision::IsSwitch(int Index)
{
	if (Index < 0 || !m_pSwitch)
		return 0;

	if (m_pSwitch[Index].m_Type > 0)
		return m_pSwitch[Index].m_Type;

	return 0;
}

int CCollision::GetSwitchNumber(int Index)
{
	if (Index < 0 || !m_pSwitch)
		return 0;

	if (m_pSwitch[Index].m_Type > 0 && m_pSwitch[Index].m_Number > 0)
		return m_pSwitch[Index].m_Number;

	return 0;
}

int CCollision::GetSwitchDelay(int Index)
{
	if (Index < 0 || !m_pSwitch)
		return 0;

	if (m_pSwitch[Index].m_Type > 0)
		return m_pSwitch[Index].m_Delay;

	return 0;
}

int CCollision::IsMover(int x, int y, int* pFlags)
{
	int Nx = clamp(x / 32, 0, m_Width - 1);
	int Ny = clamp(y / 32, 0, m_Height - 1);
	int Index = m_pTiles[Ny * m_Width + Nx].m_Index;
	*pFlags = m_pTiles[Ny * m_Width + Nx].m_Flags;
	if (Index < 0)
		return 0;
	if (Index == TILE_CP || Index == TILE_CP_F)
		return Index;
	else
		return 0;
}

vec2 CCollision::CpSpeed(int Index, int Flags)
{
	if (Index < 0)
		return vec2(0, 0);
	vec2 target;
	if (Index == TILE_CP || Index == TILE_CP_F)
		switch (Flags)
		{
		case ROTATION_0:
			target.x = 0;
			target.y = -4;
			break;
		case ROTATION_90:
			target.x = 4;
			target.y = 0;
			break;
		case ROTATION_180:
			target.x = 0;
			target.y = 4;
			break;
		case ROTATION_270:
			target.x = -4;
			target.y = 0;
			break;
		default:
			target = vec2(0, 0);
			break;
		}
	if (Index == TILE_CP_F)
		target *= 4;
	return target;
}

int CCollision::GetPureMapIndex(float x, float y)
{
	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);
	return Ny * m_Width + Nx;
}

bool CCollision::TileExists(int Index)
{
	if (Index < 0)
		return false;

	int MaxIndexValue = NUM_INDICES-1;

	if (m_pTiles[Index].m_Index >= TILE_FREEZE && m_pTiles[Index].m_Index <= MaxIndexValue)
		return true;
	if (m_pFront && m_pFront[Index].m_Index >= TILE_FREEZE && m_pFront[Index].m_Index <= MaxIndexValue)
		return true;
	if (m_pTele && (m_pTele[Index].m_Type == TILE_TELEIN || m_pTele[Index].m_Type == TILE_TELEINEVIL || m_pTele[Index].m_Type == TILE_TELECHECKINEVIL || m_pTele[Index].m_Type == TILE_TELECHECK || m_pTele[Index].m_Type == TILE_TELECHECKIN))
		return true;
	if (m_pSpeedup && m_pSpeedup[Index].m_Force > 0)
		return true;
	if (m_pDoor && m_pDoor[Index].m_vTiles.size())
		return true;
	if (m_pSwitch && m_pSwitch[Index].m_Type)
		return true;
	if (m_pTune && m_pTune[Index].m_Type)
		return true;
	return TileExistsNext(Index);
}

bool CCollision::TileExistsNext(int Index)
{
	if (Index < 0)
		return false;
	int TileOnTheLeft = (Index - 1 > 0) ? Index - 1 : Index;
	int TileOnTheRight = (Index + 1 < m_Width * m_Height) ? Index + 1 : Index;
	int TileBelow = (Index + m_Width < m_Width * m_Height) ? Index + m_Width : Index;
	int TileAbove = (Index - m_Width > 0) ? Index - m_Width : Index;

	if (m_pTiles[TileOnTheRight].m_Index == TILE_ROOM || m_pTiles[TileOnTheLeft].m_Index == TILE_ROOM || m_pTiles[TileBelow].m_Index == TILE_ROOM || m_pTiles[TileAbove].m_Index == TILE_ROOM)
		return true;
	if ((m_pTiles[TileOnTheRight].m_Index == TILE_STOP && m_pTiles[TileOnTheRight].m_Flags == ROTATION_270) || (m_pTiles[TileOnTheLeft].m_Index == TILE_STOP && m_pTiles[TileOnTheLeft].m_Flags == ROTATION_90))
		return true;
	if ((m_pTiles[TileBelow].m_Index == TILE_STOP && m_pTiles[TileBelow].m_Flags == ROTATION_0) || (m_pTiles[TileAbove].m_Index == TILE_STOP && m_pTiles[TileAbove].m_Flags == ROTATION_180))
		return true;
	if (m_pTiles[TileOnTheRight].m_Index == TILE_STOPA || m_pTiles[TileOnTheLeft].m_Index == TILE_STOPA || ((m_pTiles[TileOnTheRight].m_Index == TILE_STOPS || m_pTiles[TileOnTheLeft].m_Index == TILE_STOPS)))
		return true;
	if (m_pTiles[TileBelow].m_Index == TILE_STOPA || m_pTiles[TileAbove].m_Index == TILE_STOPA || ((m_pTiles[TileBelow].m_Index == TILE_STOPS || m_pTiles[TileAbove].m_Index == TILE_STOPS) && m_pTiles[TileBelow].m_Flags | ROTATION_180 | ROTATION_0))
		return true;
	if (m_pFront)
	{
		if (m_pFront[TileOnTheRight].m_Index == TILE_ROOM || m_pFront[TileOnTheLeft].m_Index == TILE_ROOM || m_pFront[TileBelow].m_Index == TILE_ROOM || m_pFront[TileAbove].m_Index == TILE_ROOM)
			return true;
		if (m_pFront[TileOnTheRight].m_Index == TILE_STOPA || m_pFront[TileOnTheLeft].m_Index == TILE_STOPA || ((m_pFront[TileOnTheRight].m_Index == TILE_STOPS || m_pFront[TileOnTheLeft].m_Index == TILE_STOPS)))
			return true;
		if (m_pFront[TileBelow].m_Index == TILE_STOPA || m_pFront[TileAbove].m_Index == TILE_STOPA || ((m_pFront[TileBelow].m_Index == TILE_STOPS || m_pFront[TileAbove].m_Index == TILE_STOPS) && m_pFront[TileBelow].m_Flags | ROTATION_180 | ROTATION_0))
			return true;
		if ((m_pFront[TileOnTheRight].m_Index == TILE_STOP && m_pFront[TileOnTheRight].m_Flags == ROTATION_270) || (m_pFront[TileOnTheLeft].m_Index == TILE_STOP && m_pFront[TileOnTheLeft].m_Flags == ROTATION_90))
			return true;
		if ((m_pFront[TileBelow].m_Index == TILE_STOP && m_pFront[TileBelow].m_Flags == ROTATION_0) || (m_pFront[TileAbove].m_Index == TILE_STOP && m_pFront[TileAbove].m_Flags == ROTATION_180))
			return true;
	}
	if (m_pDoor)
	{
		enum
		{
			DOOR_RIGHT,
			DOOR_BELOW,
			DOOR_LEFT,
			DOOR_ABOVE,
			NUM_DOOR_SIDES
		};
		struct Door
		{
			bool m_StopA = false;
			bool m_StopS = false;
			bool m_Stop = false;
		} aDoors[NUM_DOOR_SIDES];

		bool BelowRotation = false;

		for (int i = 0; i < NUM_DOOR_SIDES; i++)
		{
			int MapIndex = -1;
			if (i == DOOR_RIGHT) MapIndex = TileOnTheRight;
			if (i == DOOR_BELOW) MapIndex = TileBelow;
			if (i == DOOR_LEFT) MapIndex = TileOnTheLeft;
			if (i == DOOR_ABOVE) MapIndex = TileAbove;

			for (unsigned int j = 0; j < m_pDoor[MapIndex].m_vTiles.size(); j++)
			{
				CDoorTile::SInfo Info = m_pDoor[MapIndex].m_vTiles[j];
				if (Info.m_Index == TILE_STOPA)
					aDoors[i].m_StopA = true;
				if (Info.m_Index == TILE_STOPS)
					aDoors[i].m_StopS = true;

				if (Info.m_Index == TILE_STOP)
				{
					int Flags = -1;
					if (i == DOOR_RIGHT) Flags = ROTATION_270;
					else if (i == DOOR_BELOW) Flags = ROTATION_0;
					else if (i == DOOR_LEFT) Flags = ROTATION_90;
					else if (i == DOOR_ABOVE) Flags = ROTATION_180;

					aDoors[i].m_Stop = Info.m_Flags == Flags;
				}

				if (i == DOOR_BELOW)
				{
					if (Info.m_Flags | ROTATION_180 | ROTATION_0)
						BelowRotation = true;
				}
			}
		}

		if (aDoors[DOOR_RIGHT].m_StopA || aDoors[DOOR_LEFT].m_StopA || (aDoors[DOOR_RIGHT].m_StopS || aDoors[DOOR_LEFT].m_StopS))
			return true;
		if (aDoors[DOOR_BELOW].m_StopA || aDoors[DOOR_ABOVE].m_StopA || ((aDoors[DOOR_BELOW].m_StopS || aDoors[DOOR_ABOVE].m_StopS) && BelowRotation))
			return true;
		if (aDoors[DOOR_RIGHT].m_Stop || aDoors[DOOR_LEFT].m_Stop)
			return true;
		if (aDoors[DOOR_BELOW].m_Stop || aDoors[DOOR_ABOVE].m_Stop)
			return true;

		/*if (m_pDoor[TileOnTheRight].m_Index == TILE_STOPA || m_pDoor[TileOnTheLeft].m_Index == TILE_STOPA || ((m_pDoor[TileOnTheRight].m_Index == TILE_STOPS || m_pDoor[TileOnTheLeft].m_Index == TILE_STOPS)))
			return true;
		if (m_pDoor[TileBelow].m_Index == TILE_STOPA || m_pDoor[TileAbove].m_Index == TILE_STOPA || ((m_pDoor[TileBelow].m_Index == TILE_STOPS || m_pDoor[TileAbove].m_Index == TILE_STOPS) && m_pDoor[TileBelow].m_Flags | ROTATION_180 | ROTATION_0))
			return true;
		if ((m_pDoor[TileOnTheRight].m_Index == TILE_STOP && m_pDoor[TileOnTheRight].m_Flags == ROTATION_270) || (m_pDoor[TileOnTheLeft].m_Index == TILE_STOP && m_pDoor[TileOnTheLeft].m_Flags == ROTATION_90))
			return true;
		if ((m_pDoor[TileBelow].m_Index == TILE_STOP && m_pDoor[TileBelow].m_Flags == ROTATION_0) || (m_pDoor[TileAbove].m_Index == TILE_STOP && m_pDoor[TileAbove].m_Flags == ROTATION_180))
			return true;
		if (GetFightNumber(TileOnTheRight) || GetFightNumber(TileOnTheLeft) || GetFightNumber(TileAbove) || GetFightNumber(TileBelow))
			return true;*/
	}
	return false;
}

int CCollision::GetMapIndex(vec2 Pos)
{
	int Nx = clamp((int)Pos.x / 32, 0, m_Width - 1);
	int Ny = clamp((int)Pos.y / 32, 0, m_Height - 1);
	int Index = Ny * m_Width + Nx;

	if (TileExists(Index))
		return Index;
	else
		return -1;
}

std::list<int> CCollision::GetMapIndices(vec2 PrevPos, vec2 Pos, unsigned MaxIndices)
{
	std::list< int > Indices;
	float d = distance(PrevPos, Pos);
	int End(d + 1);
	if (!d)
	{
		int Nx = clamp((int)Pos.x / 32, 0, m_Width - 1);
		int Ny = clamp((int)Pos.y / 32, 0, m_Height - 1);
		int Index = Ny * m_Width + Nx;

		if (TileExists(Index))
		{
			Indices.push_back(Index);
			return Indices;
		}
		else
			return Indices;
	}
	else
	{
		float a = 0.0f;
		vec2 Tmp = vec2(0, 0);
		int Nx = 0;
		int Ny = 0;
		int Index, LastIndex = 0;
		for (int i = 0; i < End; i++)
		{
			a = i / d;
			Tmp = mix(PrevPos, Pos, a);
			Nx = clamp((int)Tmp.x / 32, 0, m_Width - 1);
			Ny = clamp((int)Tmp.y / 32, 0, m_Height - 1);
			Index = Ny * m_Width + Nx;
			if (TileExists(Index) && LastIndex != Index)
			{
				if (MaxIndices && Indices.size() > MaxIndices)
					return Indices;
				Indices.push_back(Index);
				LastIndex = Index;
			}
		}

		return Indices;
	}
}

vec2 CCollision::GetPos(int Index)
{
	if (Index < 0)
		return vec2(0, 0);

	int x = Index % m_Width;
	int y = Index / m_Width;
	return vec2(x * 32 + 16, y * 32 + 16);
}

int CCollision::GetTileIndex(int Index)
{
	if (Index < 0)
		return 0;
	return m_pTiles[Index].m_Index;
}

int CCollision::GetFTileIndex(int Index)
{
	if (Index < 0 || !m_pFront)
		return 0;
	return m_pFront[Index].m_Index;
}

int CCollision::GetTileFlags(int Index)
{
	if (Index < 0)
		return 0;
	return m_pTiles[Index].m_Flags;
}

int CCollision::GetFTileFlags(int Index)
{
	if (Index < 0 || !m_pFront)
		return 0;
	return m_pFront[Index].m_Flags;
}

int CCollision::GetIndex(int Nx, int Ny)
{
	return m_pTiles[Ny * m_Width + Nx].m_Index;
}

int CCollision::GetIndex(vec2 PrevPos, vec2 Pos)
{
	float Distance = distance(PrevPos, Pos);

	if (!Distance)
	{
		int Nx = clamp((int)Pos.x / 32, 0, m_Width - 1);
		int Ny = clamp((int)Pos.y / 32, 0, m_Height - 1);

		if ((m_pTele) ||
			(m_pSpeedup && m_pSpeedup[Ny * m_Width + Nx].m_Force > 0))
		{
			return Ny * m_Width + Nx;
		}
	}

	float a = 0.0f;
	vec2 Tmp = vec2(0, 0);
	int Nx = 0;
	int Ny = 0;

	for (float f = 0; f < Distance; f++)
	{
		a = f / Distance;
		Tmp = mix(PrevPos, Pos, a);
		Nx = clamp((int)Tmp.x / 32, 0, m_Width - 1);
		Ny = clamp((int)Tmp.y / 32, 0, m_Height - 1);
		if ((m_pTele) ||
			(m_pSpeedup && m_pSpeedup[Ny * m_Width + Nx].m_Force > 0))
		{
			return Ny * m_Width + Nx;
		}
	}

	return -1;
}

int CCollision::GetFIndex(int Nx, int Ny)
{
	if (!m_pFront) return 0;
	return m_pFront[Ny * m_Width + Nx].m_Index;
}

int CCollision::GetFTile(int x, int y)
{
	if (!m_pFront)
		return 0;
	int Nx = clamp(x / 32, 0, m_Width - 1);
	int Ny = clamp(y / 32, 0, m_Height - 1);
	if (m_pFront[Ny * m_Width + Nx].m_Index == TILE_DEATH
		|| m_pFront[Ny * m_Width + Nx].m_Index == TILE_NOLASER)
		return m_pFront[Ny * m_Width + Nx].m_Index;
	else
		return 0;
}

int CCollision::Entity(int x, int y, int Layer)
{
	if ((0 > x || x >= m_Width) || (0 > y || y >= m_Height))
	{
		char aBuf[12];
		switch (Layer)
		{
		case LAYER_GAME:
			str_format(aBuf, sizeof(aBuf), "Game");
			break;
		case LAYER_FRONT:
			str_format(aBuf, sizeof(aBuf), "Front");
			break;
		case LAYER_SWITCH:
			str_format(aBuf, sizeof(aBuf), "Switch");
			break;
		case LAYER_TELE:
			str_format(aBuf, sizeof(aBuf), "Tele");
			break;
		case LAYER_SPEEDUP:
			str_format(aBuf, sizeof(aBuf), "Speedup");
			break;
		case LAYER_TUNE:
			str_format(aBuf, sizeof(aBuf), "Tune");
			break;
		default:
			str_format(aBuf, sizeof(aBuf), "Unknown");
		}
		dbg_msg("collision", "something is VERY wrong with the %s layer please report this at https://github.com/ddnet/ddnet, you will need to post the map as well and any steps that u think may have led to this", aBuf);
		return 0;
	}
	switch (Layer)
	{
	case LAYER_GAME:
		return m_pTiles[y * m_Width + x].m_Index;
	case LAYER_FRONT:
		return m_pFront[y * m_Width + x].m_Index;
	case LAYER_SWITCH:
		return m_pSwitch[y * m_Width + x].m_Type;
	case LAYER_TELE:
		return m_pTele[y * m_Width + x].m_Type;
	case LAYER_SPEEDUP:
		return m_pSpeedup[y * m_Width + x].m_Type;
	case LAYER_TUNE:
		return m_pTune[y * m_Width + x].m_Type;
	default:
		return 0;
		break;
	}
}

void CCollision::SetCollisionAt(float x, float y, int id)
{
	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);

	m_pTiles[Ny * m_Width + Nx].m_Index = id;
}

/*void CCollision::SetDCollisionAt(float x, float y, int Type, int Flags, int Number)
{
	if (!m_pDoor)
		return;
	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);
	int Index = Ny * m_Width + Nx;

	AddDoorTile(Index, Type, Number, Flags);

	if (Number < 0)
	{
		if (!m_pDoor[].m_Fight)
			m_pDoor[Ny * m_Width + Nx].m_Fight = Number;
	}
	else if (m_pDoor[Ny * m_Width + Nx].m_Number == 0 || m_pDoor[Ny * m_Width + Nx].m_Number == Number)
	{
		m_pDoor[Ny * m_Width + Nx].m_Index = Type;
		m_pDoor[Ny * m_Width + Nx].m_Flags = Flags;
		m_pDoor[Ny * m_Width + Nx].m_Number = Number;
	}

	if (Number == 0 || IsPlotDrawDoor(Number))
		m_pDoor[Ny * m_Width + Nx].m_Usage++;
}*/

/*int CCollision::GetDTileIndex(int Index)
{
	if (!m_pDoor || Index < 0 || !m_pDoor[Index].m_Index)
		return 0;
	return m_pDoor[Index].m_Index;
}

int CCollision::GetDTileNumber(int Index)
{
	if (!m_pDoor || Index < 0 || !m_pDoor[Index].m_Index)
		return 0;
	if (m_pDoor[Index].m_Number) return m_pDoor[Index].m_Number;
	return 0;
}

int CCollision::GetDTileFlags(int Index)
{
	if (!m_pDoor || Index < 0 || !m_pDoor[Index].m_Index)
		return 0;
	return m_pDoor[Index].m_Flags;
}*/

void ThroughOffset(vec2 Pos0, vec2 Pos1, int* Ox, int* Oy)
{
	float x = Pos0.x - Pos1.x;
	float y = Pos0.y - Pos1.y;
	if (fabs(x) > fabs(y))
	{
		if (x < 0)
		{
			*Ox = -32;
			*Oy = 0;
		}
		else
		{
			*Ox = 32;
			*Oy = 0;
		}
	}
	else
	{
		if (y < 0)
		{
			*Ox = 0;
			*Oy = -32;
		}
		else
		{
			*Ox = 0;
			*Oy = 32;
		}
	}
}

int CCollision::IntersectNoLaser(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision, int Number)
{
	float d = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	for (float f = 0; f < d; f++)
	{
		float a = f / d;
		vec2 Pos = mix(Pos0, Pos1, a);
		int Nx = clamp(round_to_int(Pos.x) / 32, 0, m_Width - 1);
		int Ny = clamp(round_to_int(Pos.y) / 32, 0, m_Height - 1);
		int Index = Ny * m_Width + Nx;

		bool PlotDoor = false;
		if (Number != -1 && !IsPlotDoor(Number))
		{
			for (unsigned int i = 0; i < m_pDoor[Index].m_vTiles.size(); i++)
			{
				if (m_pDoor[Index].m_vTiles[i].m_Index == TILE_STOPA && IsPlotDoor(m_pDoor[Index].m_vTiles[i].m_Number))
				{
					PlotDoor = true;
					break;
				}
			}
		}

		if (GetIndex(Nx, Ny) == TILE_SOLID
			|| GetIndex(Nx, Ny) == TILE_NOHOOK
			|| GetIndex(Nx, Ny) == TILE_NOLASER
			|| GetFIndex(Nx, Ny) == TILE_NOLASER
			|| PlotDoor)
		{
			if (pOutCollision)
				* pOutCollision = Pos;
			if (pOutBeforeCollision)
				* pOutBeforeCollision = Last;
			if (PlotDoor) return TILE_STOPA;
			else if (GetFIndex(Nx, Ny) == TILE_NOLASER)	return GetFCollisionAt(Pos.x, Pos.y);
			else return GetCollisionAt(Pos.x, Pos.y);

		}
		Last = Pos;
	}
	if (pOutCollision)
		* pOutCollision = Pos1;
	if (pOutBeforeCollision)
		* pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectNoLaserNW(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision)
{
	float d = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	for (float f = 0; f < d; f++)
	{
		float a = f / d;
		vec2 Pos = mix(Pos0, Pos1, a);
		if (IsNoLaser(round_to_int(Pos.x), round_to_int(Pos.y)) || IsFNoLaser(round_to_int(Pos.x), round_to_int(Pos.y)))
		{
			if (pOutCollision)
				* pOutCollision = Pos;
			if (pOutBeforeCollision)
				* pOutBeforeCollision = Last;
			if (IsNoLaser(round_to_int(Pos.x), round_to_int(Pos.y))) return GetCollisionAt(Pos.x, Pos.y);
			else return  GetFCollisionAt(Pos.x, Pos.y);
		}
		Last = Pos;
	}
	if (pOutCollision)
		* pOutCollision = Pos1;
	if (pOutBeforeCollision)
		* pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectAir(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision)
{
	float d = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	for (float f = 0; f < d; f++)
	{
		float a = f / d;
		vec2 Pos = mix(Pos0, Pos1, a);
		if (IsSolid(round_to_int(Pos.x), round_to_int(Pos.y)) || (!GetTile(round_to_int(Pos.x), round_to_int(Pos.y)) && !GetFTile(round_to_int(Pos.x), round_to_int(Pos.y))))
		{
			if (pOutCollision)
				* pOutCollision = Pos;
			if (pOutBeforeCollision)
				* pOutBeforeCollision = Last;
			if (!GetTile(round_to_int(Pos.x), round_to_int(Pos.y)) && !GetFTile(round_to_int(Pos.x), round_to_int(Pos.y)))
				return -1;
			else
				if (!GetTile(round_to_int(Pos.x), round_to_int(Pos.y))) return GetTile(round_to_int(Pos.x), round_to_int(Pos.y));
				else return GetFTile(round_to_int(Pos.x), round_to_int(Pos.y));
		}
		Last = Pos;
	}
	if (pOutCollision)
		* pOutCollision = Pos1;
	if (pOutBeforeCollision)
		* pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IsCheckpoint(int Index)
{
	if (Index < 0)
		return -1;

	int z = m_pTiles[Index].m_Index;
	if (z >= TILE_CHECKPOINT_FIRST && z <= TILE_CHECKPOINT_LAST)
		return z - TILE_CHECKPOINT_FIRST;
	return -1;
}

int CCollision::IsFCheckpoint(int Index)
{
	if (Index < 0 || !m_pFront)
		return -1;

	int z = m_pFront[Index].m_Index;
	if (z >= 35 && z <= 59)
		return z - 35;
	return -1;
}

// F-DDrace
vec2 CCollision::GetRandomTile(int Index)
{
	if (m_vTiles[Index].size())
	{
		int Rand = rand() % m_vTiles[Index].size();
		return m_vTiles[Index][Rand];
	}
	return vec2(-1, -1);
}

int CCollision::GetTileRaw(int x, int y)
{
	if(!m_pTiles)
		return 0;
	return GetTileIndex(GetPureMapIndex(x, y));
}

int CCollision::GetFTileRaw(int x, int y)
{
	if(!m_pFront)
		return 0;
	return GetFTileIndex(GetPureMapIndex(x, y));
}

int CCollision::GetDoorIndex(int Index, int Number)
{
	for (unsigned int i = 0; i < m_pDoor[Index].m_vTiles.size(); i++)
		if (m_pDoor[Index].m_vTiles[i].m_Number == Number)
			return i;
	return -1;
}

bool CCollision::AddDoorTile(int Index, int Type, int Number, int Flags)
{
	if (GetDoorIndex(Index, Number) == -1)
	{
		CDoorTile::SInfo Info(Type, Number, Flags);
		m_pDoor[Index].m_vTiles.push_back(Info);
		return true;
	}
	return false;
}

bool CCollision::RemoveDoorTile(int Index, int Number)
{
	int DoorIndex = GetDoorIndex(Index, Number);
	if (DoorIndex != -1)
	{
		m_pDoor[Index].m_vTiles.erase(m_pDoor[Index].m_vTiles.begin() + DoorIndex);
		return true;
	}
	return false;
}

/*int CCollision::GetDoorNumber(vec2 Pos)
{
	return GetDTileNumber(GetPureMapIndex(Pos));
}

int CCollision::GetFightNumber(int Index, bool RealFight)
{
	if (!m_pDoor || Index < 0)
		return 0;
	int Fight = m_pDoor[Index].m_Fight;
	if (RealFight)
		Fight = -(Fight+1);
	return Fight;
}

int CCollision::GetDTile(int x, int y)
{
	if (!m_pDoor)
		return 0;

	int Nx = clamp(x / 32, 0, m_Width - 1);
	int Ny = clamp(y / 32, 0, m_Height - 1);
	return m_pDoor[Ny * m_Width + Nx].m_Index;
}*/

void CCollision::SetSpeedup(vec2 Pos, int Angle, int Force, int MaxSpeed)
{
	if (!m_pSpeedup)
		return;

	int Nx = clamp(round_to_int(Pos.x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(Pos.y) / 32, 0, m_Height - 1);
	int Index = Ny * m_Width + Nx;

	m_pSpeedup[Index].m_Angle = Angle;
	m_pSpeedup[Index].m_Force = Force;
	m_pSpeedup[Index].m_MaxSpeed = MaxSpeed;
	m_pSpeedup[Index].m_Type = Force ? TILE_BOOST : 0;
}

/*void CCollision::SetButtonNumber(vec2 Pos, int Number)
{
	if (!m_pDoor)
		return;

	int Nx = clamp(round_to_int(Pos.x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(Pos.y) / 32, 0, m_Height - 1);
	int Index = Ny * m_Width + Nx;

	m_pDoor[Index].m_Button = Number;
}

int CCollision::GetButtonNumber(int Index)
{
	if (!m_pDoor)
		return 0;
	return m_pDoor[Index].m_Button;
}*/

bool CCollision::IsFightBorder(vec2 Pos, int Fight)
{
	if (!m_pDoor)
		return false;

	int Index = GetPureMapIndex(Pos);
	for (unsigned int i = 0; i < m_pDoor[Index].m_vTiles.size(); i++)
		if (m_pDoor[Index].m_vTiles[i].m_Number  * -1 == Fight + 1)
			return true;
	return false;
}

std::vector<int> CCollision::GetButtonNumbers(int Index)
{
	std::vector<int> vNumbers;
	if (!m_pSwitch || !m_pDoor)
		return vNumbers;

	// to support toggle tiles aswell
	if (m_pSwitch[Index].m_Type == TILE_SWITCHTOGGLE)
		vNumbers.push_back(m_pSwitch[Index].m_Number);

	for (unsigned int i = 0; i < m_pDoor[Index].m_vTiles.size(); i++)
		if (m_pDoor[Index].m_vTiles[i].m_Index == TILE_SWITCHTOGGLE)
			vNumbers.push_back(m_pDoor[Index].m_vTiles[i].m_Number);

	return vNumbers;
}

bool CCollision::IsPlotTile(int Index)
{
	return Index == TILE_SWITCH_PLOT || Index == TILE_SWITCH_PLOT_DOOR || Index == TILE_SWITCH_PLOT_TOTELE;
}

int CCollision::GetPlotID(int Index)
{
	if (Index >= 0 && m_pSwitch && m_pSwitch[Index].m_Type == TILE_SWITCH_PLOT && m_pSwitch[Index].m_Number > 0)
		return GetPlotBySwitch(m_pSwitch[Index].m_Number);
	return 0;
}

int CCollision::GetSwitchByPlot(int PlotID)
{
	if (PlotID <= 0 || PlotID > m_NumPlots)
		return 0;
	return PlotID + m_NumSwitchers;
}

int CCollision::GetPlotBySwitch(int SwitchID)
{
	if (!IsPlotDoor(SwitchID))
		return 0;
	return SwitchID - m_NumSwitchers;
}

int CCollision::GetSwitchByPlotLaserDoor(int PlotID, int Door)
{
	int Num = 0;
	if (PlotID == 0)
	{
		Num = GetNumPlotLaserDoors();
	}
	else
	{
		for (int i = 1; i < PlotID; i++)
			Num += GetNumMaxDoors(i);
	}
	return m_NumSwitchers + m_NumPlots + Num + Door + 1;
}

int CCollision::GetNumMaxDoors(int PlotID)
{
	if (PlotID == 0)
		return GetNumFreeDrawDoors();
	return m_apPlotSize[PlotID] == PLOT_SMALL ? PLOT_SMALL_MAX_DOORS : m_apPlotSize[PlotID] == PLOT_BIG ? PLOT_BIG_MAX_DOORS : 0;
}

int CCollision::IntersectLinePortalRifleStop(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision)
{
	float d = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	for (float f = 0; f < d; f++)
	{
		float a = f / d;
		vec2 Pos = mix(Pos0, Pos1, a);
		int Nx = clamp(round_to_int(Pos.x) / 32, 0, m_Width - 1);
		int Ny = clamp(round_to_int(Pos.y) / 32, 0, m_Height - 1);
		bool GameLayerBlocked = GetIndex(Nx, Ny) == TILE_SOLID || GetIndex(Nx, Ny) == TILE_NOHOOK || GetIndex(Nx, Ny) == TILE_PORTAL_RIFLE_STOP || GetIndex(Nx, Ny) == TILE_DFREEZE;
		bool FrontLayerBlocked = GetFIndex(Nx, Ny) == TILE_PORTAL_RIFLE_STOP || GetFIndex(Nx, Ny) == TILE_DFREEZE;
		if (GameLayerBlocked || FrontLayerBlocked)
		{
			if (pOutCollision)
				* pOutCollision = Pos;
			if (pOutBeforeCollision)
				* pOutBeforeCollision = Last;
			if (FrontLayerBlocked)
				return GetFIndex(Nx, Ny);
			if (GameLayerBlocked)
				return GetIndex(Nx, Ny);
			return 0;
		}
		Last = Pos;
	}
	if (pOutCollision)
		* pOutCollision = Pos1;
	if (pOutBeforeCollision)
		* pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectLineDoor(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision, int Team, bool PlotDoorOnly, bool ClosedOnly)
{
	if (!m_pDoor || (PlotDoorOnly && !m_NumPlots))
		return 0;

	float d = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	for (float f = 0; f < d; f++)
	{
		float a = f / d;
		vec2 Pos = mix(Pos0, Pos1, a);
		int Nx = clamp(round_to_int(Pos.x) / 32, 0, m_Width - 1);
		int Ny = clamp(round_to_int(Pos.y) / 32, 0, m_Height - 1);
		int Index = Ny * m_Width + Nx;

		for (unsigned int i = 0; i < m_pDoor[Index].m_vTiles.size(); i++)
		{
			int Number = m_pDoor[Index].m_vTiles[i].m_Number;
			bool IsDoor = (m_pDoor[Index].m_vTiles[i].m_Index == TILE_STOPA && (m_pSwitchers[Number].m_Status[Team] || !ClosedOnly));
			bool PlotLaserWall = (Number == 0 && m_pSwitch[Index].m_Type == TILE_SWITCH_PLOT);
			bool PlotDoor = (IsDoor && (PlotLaserWall || IsPlotDoor(Number)));
			if (PlotDoor || (!PlotDoorOnly && IsDoor))
			{
				if (pOutCollision)
					*pOutCollision = Pos;
				if (pOutBeforeCollision)
					*pOutBeforeCollision = Last;
				if (PlotLaserWall)
					return -1;
				return Number;
			}
		}
		Last = Pos;
	}
	if (pOutCollision)
		* pOutCollision = Pos1;
	if (pOutBeforeCollision)
		* pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::CheckPointDoor(vec2 Pos, int Team, bool PlotDoorOnly, bool ClosedOnly)
{
	int Index = GetPureMapIndex(Pos);
	for (unsigned int i = 0; i < m_pDoor[Index].m_vTiles.size(); i++)
	{
		int Number = m_pDoor[Index].m_vTiles[i].m_Number;
		if (m_pDoor[Index].m_vTiles[i].m_Index == TILE_STOPA && (!PlotDoorOnly || IsPlotDoor(Number)) && (m_pSwitchers[Number].m_Status[Team] || !ClosedOnly))
			return Number;
	}
	return 0;
}

bool CCollision::TestBoxDoor(vec2 Pos, vec2 Size, int Team, bool PlotDoorOnly, bool ClosedOnly)
{
	Size *= 0.5f;
	if (CheckPointDoor(vec2(Pos.x - Size.x, Pos.y - Size.y), Team, PlotDoorOnly, ClosedOnly))
		return true;
	if (CheckPointDoor(vec2(Pos.x + Size.x, Pos.y - Size.y), Team, PlotDoorOnly, ClosedOnly))
		return true;
	if (CheckPointDoor(vec2(Pos.x - Size.x, Pos.y + Size.y), Team, PlotDoorOnly, ClosedOnly))
		return true;
	if (CheckPointDoor(vec2(Pos.x + Size.x, Pos.y + Size.y), Team, PlotDoorOnly, ClosedOnly))
		return true;
	return false;
}

bool CCollision::TestBoxBig(vec2 Pos, vec2 Size)
{
	if(TestBox(Pos, Size))
		return true;

	// multi sample the rest
	const int MsCountX = (int)(Size.x / ms_MinStaticPhysSize);
	const float MsGapX = Size.x / MsCountX;
	const int MsCountY = (int)(Size.y / ms_MinStaticPhysSize);
	const float MsGapY = Size.y / MsCountY;

	if(!MsCountX && !MsCountY)
		return false;

	Size *= 0.5;

	// top
	for(int i = 0; i < MsCountX; i++)
	{
		if(CheckPoint(Pos.x-Size.x + (i+1) * MsGapX, Pos.y-Size.y))
			return true;
	}

	// bottom
	for(int i = 0; i < MsCountX; i++)
	{
		if(CheckPoint(Pos.x-Size.x + (i+1) * MsGapX, Pos.y+Size.y))
			return true;
	}

	// left
	for(int i = 0; i < MsCountY; i++)
	{
		if(CheckPoint(Pos.x-Size.x, Pos.y-Size.y + (i+1) * MsGapY))
			return true;
	}

	// right
	for(int i = 0; i < MsCountY; i++)
	{
		if(CheckPoint(Pos.x+Size.x, Pos.y-Size.y + (i+1) * MsGapY))
			return true;
	}
	return false;
}

void CCollision::MoveBoxBig(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity)
{
	// do the move
	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;

	float Distance = length(Vel);
	int Max = (int)Distance;

	if(Distance > 0.00001f)
	{
		//vec2 old_pos = pos;
		float Fraction = 1.0f/(float)(Max+1);
		for(int i = 0; i <= Max; i++)
		{
			//float amount = i/(float)max;
			//if(max == 0)
				//amount = 0;

			vec2 NewPos = Pos + Vel*Fraction; // TODO: this row is not nice

			if(TestBoxBig(vec2(NewPos.x, NewPos.y), Size))
			{
				int Hits = 0;

				if(TestBoxBig(vec2(Pos.x, NewPos.y), Size))
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					Hits++;
				}

				if(TestBoxBig(vec2(NewPos.x, Pos.y), Size))
				{
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
					Hits++;
				}

				// neither of the tests got a collision.
				// this is a real _corner case_!
				if(Hits == 0)
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
				}
			}

			Pos = NewPos;
		}
	}

	*pInoutPos = Pos;
	*pInoutVel = Vel;
}

bool CCollision::IsBoxGrounded(vec2 Pos, vec2 Size)
{
	// multi sample
	const int MsCount = (int)(Size.x / ms_MinStaticPhysSize);
	const float MsGap = Size.x / MsCount;

	Size *= 0.5;

	if(MsCount)
	{
		// bottom
		for(int i = 0; i < MsCount; i++)
		{
			if(CheckPoint(Pos.x-Size.x + (i+1) * MsGap, Pos.y+Size.y+5))
				return true;
		}
	}

	if(CheckPoint(Pos.x-Size.x, Pos.y+Size.y+5))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y+Size.y+5))
		return true;
	return false;
}
