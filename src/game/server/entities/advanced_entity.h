// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_ADVANCED_ENTITY_H
#define GAME_SERVER_ENTITIES_ADVANCED_ENTITY_H

#include <game/server/entity.h>

class CAdvancedEntity : public CEntity
{
public:
	CAdvancedEntity(CGameWorld *pGameWorld, int Objtype, vec2 Pos, int ProimityRadius, int Owner = -1);
	virtual ~CAdvancedEntity() {}

	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient) {}

protected:
	bool IsGrounded(bool SetVel = false);
	// HandleDropped() has to be called within the tick function of the child entity whenever the entity is dropped and not being carried
	void HandleDropped();

	vec2 m_Vel;
	// m_PrevPos has to be set at the end of the tick function of the child entity
	vec2 m_PrevPos;

	CCharacter *m_pOwner;
	int m_Owner;
	int m_TeleCheckpoint;
	int m_TuneZone;

	static bool IsSwitchActiveCb(int Number, void* pUser);
	void HandleTiles(int Index);
	int m_TileIndex;
	int m_TileFIndex;
	int m_MoveRestrictions;
};

#endif