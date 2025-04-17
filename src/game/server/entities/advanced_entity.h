// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_ADVANCED_ENTITY_H
#define GAME_SERVER_ENTITIES_ADVANCED_ENTITY_H

#include <game/server/entity.h>
#include <game/server/mask128.h>

class CAdvancedEntity : public CEntity
{
public:
	CAdvancedEntity(CGameWorld *pGameWorld, int Objtype, vec2 Pos, vec2 Size, int Owner = -1, bool CheckDeath = true);
	virtual ~CAdvancedEntity() {}

	virtual void Reset();
	// CAdvancedEntity::Tick() has to be called within the tick function of the child entity
	virtual void Tick();
	virtual void Snap(int SnappingClient) {}

	CCharacter *GetOwner();
	int GetMoveRestrictions() { return m_MoveRestrictions; }
	vec2 GetSize() { return m_Size; }
	vec2 GetVel() { return m_Vel; }
	void SetVel(vec2 Vel) { m_Vel = Vel; }
	void SetPrevPos(vec2 Pos) { m_PrevPos = Pos; }
	virtual void ReleaseHooked() {}

	int GetDDTeam() { return m_DDTeam; }
	Mask128 TeamMask() { return m_TeamMask; }

protected:
	bool IsGrounded(bool GroundVel = false, bool AirVel = false);
	// HandleDropped() has to be called within the tick function of the child entity whenever the entity is dropped and not being carried
	void HandleDropped();

	vec2 m_Vel;
	// m_PrevPos has to be set at the end of the tick function of the child entity
	vec2 m_PrevPos;

	vec2 m_Size;

	int m_Owner;
	int m_TeleCheckpoint;
	int m_TuneZone;

	bool m_CheckDeath;
	bool m_CheckGameLayerClipped;
	Mask128 m_TeamMask;
	int m_DDTeam;

	bool m_Gravity;
	bool m_GroundVel;
	bool m_AirVel;
	float m_Elasticity;
	bool m_AllowVipPlus;

	static bool IsSwitchActiveCb(int Number, void* pUser);
	void HandleTiles(int Index);
	int m_TileIndex;
	int m_TileFIndex;
	int m_MoveRestrictions;
	int m_LastInOutTeleporter;
	CCollision::MoveRestrictionExtra GetMoveRestrictionExtra();
};

#endif
