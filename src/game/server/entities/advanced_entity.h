// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_ADVANCED_ENTITY_H
#define GAME_SERVER_ENTITIES_ADVANCED_ENTITY_H

#include <game/server/entity.h>
#include <game/server/mask128.h>

class CAdvancedEntity : public CEntity
{
public:
	CAdvancedEntity(CGameWorld *pGameWorld, int Objtype, vec2 Pos, vec2 Size, int Owner = -1);
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
	void SetSize(vec2 NewSize)
	{
		m_Size = NewSize;
		// Set ProximityRadius to the smaller value for now
		SetProximityRadius(min(NewSize.x, NewSize.y));
	}
	virtual void ReleaseHooked() {}

	int GetDDTeam() { return m_DDTeam; }
	Mask128 TeamMask() { return m_TeamMask; }

	bool IsFlags(int Flags) { return (m_Flags & Flags) == Flags; }
	void SetFlags(int Flags, bool Enabled)
	{
		if (Enabled)
			m_Flags |= Flags;
		else
			m_Flags &= ~Flags;
	}

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
	int m_DDTeam;
	Mask128 m_TeamMask;
	vec2 m_Elasticity;

	int m_Flags;

	enum EFlags
	{
		CHECK_DEATH = 1<<0,
		CHECK_GAME_LAYER_CLIPPED = 1<<1,
		APPLY_GRAVITY = 1<<2,
		APPLY_GROUND_VEL = 1<<3,
		APPLY_AIR_VEL = 1<<4,
		ALLOW_VIP_PLUS = 1<<5,
	};

	static bool IsSwitchActiveCb(int Number, void *pUser);
	void HandleTiles(int Index);
	int m_TileIndex;
	int m_TileFIndex;
	int m_MoveRestrictions;
	int m_LastInOutTeleporter;
	CCollision::MoveRestrictionExtra GetMoveRestrictionExtra();
};

#endif
