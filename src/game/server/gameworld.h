/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEWORLD_H
#define GAME_SERVER_GAMEWORLD_H

#include <game/gamecore.h>

#include <list>

class CEntity;
class CCharacter;

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

	void UpdatePlayerMaps(int ForcedID = -1);

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

	void ForceUpdatePlayerMap(int ClientID) { UpdatePlayerMaps(ClientID); }

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
	int FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type);

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
	CEntity *ClosestEntity(vec2 Pos, float Radius, int Type, CEntity *pNotThis, bool CheckWall = false);

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
	class CCharacter* ClosestCharacter(vec2 Pos, float Radius, CEntity* ppNotThis, int CollideWith = -1, bool CheckPassive = true, bool CheckWall = false);

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
	CEntity *ClosestEntityTypes(vec2 Pos, float Radius, int Types, CEntity *pNotThis, int CollideWith = -1, bool CheckPassive = true);
	int FindEntitiesTypes(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Types);
	CEntity *IntersectEntityTypes(vec2 Pos0, vec2 Pos1, float Radius, vec2& NewPos, CEntity *pNotThis, int CollideWith, int Types, class CCharacter *pThisOnly = 0);
};

#endif
