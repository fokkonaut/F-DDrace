/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEWORLD_H
#define GAME_SERVER_GAMEWORLD_H

#include <game/gamecore.h>

#include <list>

class CEntity;
class CCharacter;
class CPlayer;
class CGameContext;
class CDoor;
class CDrawTile;
class CLaserText;

// Needs to be here because we need it in gamecontext.h but also in draweditor.h
class CSelectedArea
{
	CGameContext *m_pGameServer;
public:
	void Init(CGameContext *pGameServer);
	~CSelectedArea();

	int m_aID[4];
	vec2 m_aPos[2];
	vec2 TopLeft() { return vec2(minimum(m_aPos[0].x, m_aPos[1].x), minimum(m_aPos[0].y, m_aPos[1].y)); }
	vec2 BottomRight() { return vec2(maximum(m_aPos[0].x, m_aPos[1].x), maximum(m_aPos[0].y, m_aPos[1].y)); }
	bool Includes(vec2 Pos) { return (Pos.x >= TopLeft().x-1 && Pos.x <= BottomRight().x+1 && Pos.y >= TopLeft().y-1 && Pos.y <= BottomRight().y+1); }
};

class CNotTheseEntities
{
private:
	CEntity* m_pSingleEntity;
	CEntity** m_apExcludeEntities;
	int m_NumEntities;

public:
	// Automatic cast (short scope)
	CNotTheseEntities(CEntity* pSingleEntity)
	{
		m_pSingleEntity = pSingleEntity;
		m_apExcludeEntities = &m_pSingleEntity;
		m_NumEntities = 1;
	}
	CNotTheseEntities(CEntity** apEntities, int NumEntities)
	{
		m_pSingleEntity = nullptr;
		m_apExcludeEntities = apEntities;
		m_NumEntities = NumEntities;
	}

	// Getting
	bool IsExcluded(CEntity* pEntity) const
	{
		for (int i = 0; i < m_NumEntities; i++)
			if (m_apExcludeEntities[i] == pEntity)
				return true;

		return false;
	}
};

/*
	Class: Game World
		Tracks all entities in the game. Propagates tick and
		snap calls to all entities.
*/
class CGameWorld
{
public:
	enum
	{
		ENTTYPE_PROJECTILE = 0,
		ENTTYPE_LASER,
		ENTTYPE_PICKUP,
		ENTTYPE_CHARACTER,
		ENTTYPE_FLAG,

		// F-DDrace
		ENTTYPE_DOOR,
		ENTTYPE_DRAGGER,
		ENTTYPE_LASER_GUN,
		ENTTYPE_LIGHT,
		ENTTYPE_PLASMA,

		ENTTYPE_ATOM,
		ENTTYPE_CLOCK,
		ENTTYPE_CUSTOM_PROJECTILE,
		ENTTYPE_PICKUP_DROP,
		ENTTYPE_STABLE_PROJECTILE,
		ENTTYPE_TRAIL,
		ENTTYPE_LIGHTSABER,
		ENTTYPE_LASERTEXT,
		ENTTYPE_PORTAL,
		ENTTYPE_MONEY,
		ENTTYPE_HELICOPTER,
		ENTTYPE_FLYINGPOINT,
		ENTTYPE_SPEEDUP,
		ENTTYPE_BUTTON,
		ENTTYPE_TELEPORTER,
		ENTTYPE_LOVELY,
		ENTTYPE_ROTATING_BALL,
		ENTTYPE_STAFF_IND,
		ENTTYPE_PORTAL_BLOCKER,
		ENTTYPE_LIGHTNING_LASER,
		ENTTYPE_GROG,
		ENTTYPE_TASER_SHIELD,
		ENTTYPE_MISSILE,
		ENTTYPE_PLAYER_COUNTER,
		ENTTYPE_DRAWTILE,
		ENTTYPE_SPIDER,

		NUM_ENTTYPES
	};

private:
	void Reset();
	void RemoveEntities();

	CEntity *m_pNextTraverseEntity;
	CEntity *m_apFirstEntityTypes[NUM_ENTTYPES];

	class CGameContext *m_pGameServer;
	class CConfig *m_pConfig;
	class IServer *m_pServer;

public:
	class CGameContext *GameServer() { return m_pGameServer; }
	class CConfig *Config() { return m_pConfig; }
	class IServer *Server() { return m_pServer; }

	bool m_ResetRequested;
	bool m_Paused;
	CWorldCore m_Core;

	CGameWorld();
	~CGameWorld();
	void SetGameServer(CGameContext *pGameServer);

	void InsertEntity(CEntity *pEntity);
	void RemoveEntity(CEntity *pEntity);
	void DestroyEntity(CEntity *pEntity);

	void Snap(int SnappingClient);
	void PostSnap();

	void Tick();

	// F-DDrace
	void IntraTick();

	struct
	{
		int m_NumPoliceTilePlayers = 0;
		int m_MaxPoliceTilePlayers = 0;
		bool IsActive() { return m_MaxPoliceTilePlayers <= 0 || m_NumPoliceTilePlayers <= m_MaxPoliceTilePlayers; }
	} m_PoliceFarm;
	struct
	{
		int m_PlayersInHotzone = 0;
		int m_MaxHotzoneTilePlayers = 0;
		bool IsActive() { return m_MaxHotzoneTilePlayers <= 0 || m_PlayersInHotzone <= m_MaxHotzoneTilePlayers; }
	} m_Hotzone;
	class CDrawTileContext
	{
		std::set<CDrawTile *> m_vpResponsibleTiles;
		std::set<CDrawTile *> m_vPendingRemovals;

	public:
		void Insert(CDrawTile *pEnt) { m_vpResponsibleTiles.insert(pEnt); }
		void MarkForRemoval(CDrawTile *pEnt) { m_vPendingRemovals.insert(pEnt); }
		void ProcessRemovals()
		{
			for (auto &pPendingRemoval : m_vPendingRemovals)
				m_vpResponsibleTiles.erase(pPendingRemoval);
			m_vPendingRemovals.clear();
		}
		const std::set<CDrawTile *> &ResponsibleTiles() const { return m_vpResponsibleTiles; }
	};
	CDrawTileContext m_DrawTiles;

	void ReleaseHooked(int ClientID);
	void UnsetTelekinesis(CEntity *pEntity);
	void UnsetKiller(int ClientID);

	bool FlagsUsed();

	CLaserText *CreateLaserText(vec2 Pos, int Owner, const char* pText, int Seconds = 3, bool AboveTee = true);
	int MoneyLaserTextTime(int64 Amount);

	bool SpawnSpider(int Spawner, int Team, vec2 Pos, float Scale = 1.f, bool SpawnOnFloor = true, int Number = -1);
	bool SpawnHelicopter(int Spawner, int Team, vec2 Pos, int HelicopterType, int TurretType, float Scale = 1.f, bool SpawnOnFloor = true, int Number = -1);
	int GetHelicopterTileType();


	// Find functions

	CEntity *FindFirst(int Type);

	/*
		Function: find_entities
			Finds entities close to a position and returns them in a list.

		Arguments:
			pos - Position.
			radius - How close the entities have to be.
			ents - Pointer to a list that should be filled with the pointers
				to the entities.
			max - Number of entities that fits into the ents array.
			type - Type of the entities to find.

		Returns:
			Number of entities found and added to the ents array.
	*/
	int FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type, int Team = -1);

	/*
		Function: closest_CEntity
			Finds the closest CEntity of a type to a specific point.

		Arguments:
			pos - The center position.
			radius - How far off the CEntity is allowed to be
			type - Type of the entities to find.
			notthis - Entity to ignore

		Returns:
			Returns a pointer to the closest CEntity or NULL if no CEntity is close enough.
	*/
	CEntity *ClosestEntity(vec2 Pos, float Radius, int Type, CEntity *pNotThis, int Team = -1, bool CheckWall = false);

	/*
		Function: interserct_CCharacter
			Finds the closest CCharacter that intersects the line.

		Arguments:
			pos0 - Start position
			pos2 - End position
			radius - How for from the line the CCharacter is allowed to be.
			new_pos - Intersection position
			notthis - Entity to ignore intersecting with

		Returns:
			Returns a pointer to the closest hit or NULL of there is no intersection.
	*/
	class CCharacter* IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2& NewPos, class CCharacter* pNotThis = 0, int CollideWith = -1, class CCharacter* pThisOnly = 0);


	enum EFindEntFlag
	{
		PASSIVE = 1<<0,
		WALL = 1<<1,
		MINIGAME_TEE = 1<<2,
		IN_HELICOPTER = 1<<3,
		SAFE_AREA = 1<<4,
	};

	/*
		Function: closest_CCharacter
			Finds the closest CCharacter to a specific point.

		Arguments:
			pos - The center position.
			radius - How far off the CCharacter is allowed to be
			notthis - Entity to ignore

		Returns:
			Returns a pointer to the closest CCharacter or NULL if no CCharacter is close enough.
	*/
	class CCharacter* ClosestCharacter(vec2 Pos, float Radius, CEntity* ppNotThis, int CollideWith = -1, int Team = -1, int Flags = -1);

	// F-DDrace

	/*
		Function: interserct_CCharacters
			Finds all CCharacters that intersect the line.

		Arguments:
			pos0 - Start position
			pos2 - End position
			radius - How for from the line the CCharacter is allowed to be.
			new_pos - Intersection position
			notthis - Entity to ignore intersecting with

		Returns:
			Returns list with all Characters on line.
	*/
	std::list<class CCharacter*> IntersectedCharacters(vec2 Pos0, vec2 Pos1, float Radius, class CEntity* pNotThis = 0, int CollideWith = -1);

	class CCharacter* ClosestCharacterMode(vec2 Pos, CCharacter* pNotThis, int CollideWith = -1, int Mode = 0);
	int GetClosestHouseDummy(vec2 Pos, CCharacter* pNotThis, int Type, int CollideWith = -1);

	// when defining the Types, add them bitwise: 1 << TYPE | 1 << TYPE2... (or 1ULL << TYPE32 for types over 31)
	CEntity *ClosestEntityTypes(vec2 Pos, float Radius, int64 Types, CEntity *pNotThis, int CollideWith = -1, int Flags = -1);
	int FindEntitiesTypes(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int64 Types, int Team = -1, bool ProjHammer = false);

	enum EIntersectEntTypesFlag
	{
		PLOT_TASER_DESTROY = 1<<0,
		PLOT_DOOR_ONLY = 1<<1,
		IN_VEHICLE = 1<<2,
		PREVENT_EVENT_PREDICTION = 1<<3,
	};
	CEntity *IntersectEntityTypes(vec2 Pos0, vec2 Pos1, float Radius, vec2& NewPos, const CNotTheseEntities& NotThese, int CollideWith, int64 Types, CCharacter *pThisOnly = nullptr, int Flags = -1);
	bool IntersectLinePortalBlocker(vec2 Pos0, vec2 Pos1);
	int IntersectDoorsUniqueNumbers(vec2 Pos, float Radius, CDoor **ppDoors, int Max);
};

#endif
