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

// Needs to be here because we need it in gamecontext.h but also in draweditor.h
class CSelectedArea
{
	CGameContext *m_pGameServer;
public:
	void Init(CGameContext *pGameServer);
	~CSelectedArea();

	int m_aID[4];
	vec2 m_aPos[2];
	vec2 TopLeft() { return vec2(min(m_aPos[0].x, m_aPos[1].x), min(m_aPos[0].y, m_aPos[1].y)); }
	vec2 BottomRight() { return vec2(max(m_aPos[0].x, m_aPos[1].x), max(m_aPos[0].y, m_aPos[1].y)); }
	bool Includes(vec2 Pos) { return (Pos.x >= TopLeft().x-1 && Pos.x <= BottomRight().x+1 && Pos.y >= TopLeft().y-1 && Pos.y <= BottomRight().y+1); }
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
		ENTTYPE_PLAYER_COUNTER,
		ENTTYPE_MISSILE,

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

	struct PlayerMap
	{
		enum SSeeOthers {
			STATE_NONE = -1,
			STATE_PAGE_FIRST,
			STATE_PAGE_SECOND,

			MAX_NUM_SEE_OTHERS = 34,
		};

		void Init(int ClientID, CGameWorld *pGameWorld);
		void InitPlayer(bool Rejoin);
		CGameWorld *m_pGameWorld;
		CPlayer *GetPlayer();
		int m_ClientID;
		int m_NumReserved;
		bool m_UpdateTeamsState;
		bool m_aReserved[MAX_CLIENTS];
		bool m_ResortReserved;
		int *m_pMap;
		int *m_pReverseMap;
		void Update();
		void Add(int MapID, int ClientID);
		int Remove(int MapID);
		void InsertNextEmpty(int ClientID);
		int GetMapSize();
		// See others
		int m_SeeOthersState;
		int m_TotalOverhang;
		int m_NumPages;
		int m_NumSeeOthers;
		bool m_aWasSeeOthers[MAX_CLIENTS];
		void DoSeeOthers();
		void CycleSeeOthers();
		void UpdateSeeOthers();
		void ResetSeeOthers();
		int GetSpecSelectFlag(int SpecFlag);
		void AddToNumReserved(int Summand);
	} m_aMap[MAX_CLIENTS];
	void UpdatePlayerMap(int ClientID);

public:
	class CGameContext *GameServer() { return m_pGameServer; }
	class CConfig *Config() { return m_pConfig; }
	class IServer *Server() { return m_pServer; }

	bool m_ResetRequested;
	bool m_Paused;
	CWorldCore m_Core;

	// F-DDrace
	struct
	{
		int m_NumPoliceTilePlayers = 0;
		int m_MaxPoliceTilePlayers = 0;
		bool IsActive() { return m_MaxPoliceTilePlayers <= 0 || m_NumPoliceTilePlayers <= m_MaxPoliceTilePlayers; }
	} m_PoliceFarm;

	void InitPlayerMap(int ClientID, bool Rejoin = false) { m_aMap[ClientID].InitPlayer(Rejoin); }
	void UpdateTeamsState(int ClientID) { m_aMap[ClientID].m_UpdateTeamsState = true; }
	void ForceInsertPlayer(int Insert, int ClientID) { m_aMap[ClientID].InsertNextEmpty(Insert); }
	void AddToNumReserved(int ClientID, int Summand) { m_aMap[ClientID].AddToNumReserved(Summand); }

	enum
	{
		SEE_OTHERS_IND_NONE = -1,
		SEE_OTHERS_IND_PLAYER,
		SEE_OTHERS_IND_BUTTON,
	};

	// We start after the seeother indicator/button, and can take all the ids up to m_NumReserved, we may not interfere with GetMapSize()
	int GetFirstDurakID(int ClientID) { return GetSeeOthersID(ClientID) - 1; }
	int GetSeeOthersID(int ClientID);
	void DoSeeOthers(int ClientID);
	void ResetSeeOthers(int ClientID);
	int GetTotalOverhang(int ClientID);
	int GetSeeOthersInd(int ClientID, int MapID);
	const char *GetSeeOthersName(int ClientID);

	int GetSpecSelectFlag(int ClientID, int SpecFlag) { return m_aMap[ClientID].GetSpecSelectFlag(SpecFlag); }

	CGameWorld();
	~CGameWorld();

	void SetGameServer(CGameContext *pGameServer);

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
	CEntity *ClosestEntity(vec2 Pos, float Radius, int Type, CEntity *pNotThis, bool CheckWall = false, int Team = -1);

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
	class CCharacter* ClosestCharacter(vec2 Pos, float Radius, CEntity* ppNotThis, int CollideWith = -1, bool CheckPassive = true, bool CheckWall = false, bool CheckMinigameTee = false, int Team = -1, bool CheckDrivers = true);

	/*
		Function: insert_entity
			Adds an entity to the world.

		Arguments:
			entity - Entity to add
	*/
	void InsertEntity(CEntity *pEntity);

	/*
		Function: remove_entity
			Removes an entity from the world.

		Arguments:
			entity - Entity to remove
	*/
	void RemoveEntity(CEntity *pEntity);

	/*
		Function: destroy_entity
			Destroys an entity in the world.

		Arguments:
			entity - Entity to destroy
	*/
	void DestroyEntity(CEntity *pEntity);

	/*
		Function: snap
			Calls snap on all the entities in the world to create
			the snapshot.

		Arguments:
			snapping_client - ID of the client which snapshot
			is being created.
	*/
	void Snap(int SnappingClient);
	
	void PostSnap();

	/*
		Function: tick
			Calls tick on all the entities in the world to progress
			the world to the next tick.

	*/
	void Tick();

	// F-DDrace

	void ReleaseHooked(int ClientID);


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

	class CCharacter* ClosestCharacter(vec2 Pos, CCharacter* pNotThis, int CollideWith = -1, int Mode = 0);
	int GetClosestHouseDummy(vec2 Pos, CCharacter* pNotThis, int Type, int CollideWith = -1);

	// when defining the Types, add them bitwise: 1 << TYPE | 1 << TYPE2...
	CEntity *ClosestEntityTypes(vec2 Pos, float Radius, int Types, CEntity *pNotThis, int CollideWith = -1, bool CheckPassive = true, bool CheckDrivers = true);
	int FindEntitiesTypes(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Types, int Team = -1);
	CEntity *IntersectEntityTypes(vec2 Pos0, vec2 Pos1, float Radius, vec2& NewPos, CEntity *pNotThis, int CollideWith, int Types,
		class CCharacter *pThisOnly = 0, bool CheckPlotTaserDestroy = false, bool PlotDoorOnly = false, bool CheckDrivers = true);
	bool IntersectLinePortalBlocker(vec2 Pos0, vec2 Pos1);
	int IntersectDoorsUniqueNumbers(vec2 Pos, float Radius, CDoor **ppDoors, int Max);
};

#endif
