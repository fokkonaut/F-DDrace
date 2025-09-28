// made by fokkonaut and Matq, somewhere around 2021 and in 2025

#ifndef GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_H
#define GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_H

#include "helicopter_turret.h"
#include "game/server/entities/advanced_entity.h"

#define HELICOPTER_DEFAULT_SCALE 0.8f
#define HELICOPTER_MIN_SCALE 0.8f
#define HELICOPTER_MAX_SCALE 5.0f
#define HELICOPTER_PHYSSIZE (vec2(80, 128) * HELICOPTER_DEFAULT_SCALE)

class CHelicopter : public CAdvancedEntity
{
private:
	enum
	{
		NUM_BONES_BODY = 14,
		NUM_BONES_PROPELLERS_TOP = 2,
		NUM_BONES_PROPELLERS_BACK = 2,
		NUM_BONES_PROPELLERS = NUM_BONES_PROPELLERS_TOP + NUM_BONES_PROPELLERS_BACK,
		NUM_BONES = NUM_BONES_BODY + NUM_BONES_PROPELLERS,
		NUM_TRAILS = 2,
		NUM_HEARTS = 4,

		NUM_BUILD_IDS = 3, // sparkles/particles
		NUM_HELICOPTER_IDS = NUM_BONES + NUM_TRAILS + NUM_HEARTS + NUM_BUILD_IDS,
	};

	int m_InputDirection;
	float m_MaxHealth;
	float m_Health;
	bool m_EngineOn;

	float m_Scale;
	void InitBody();
	void InitPropellers();
	void SpinPropellers();
	void ResetAndTurnOff();

	bool m_Flipped;
	void Flip();

	float m_Angle;
	void Rotate(float Angle);
	void SetAngle(float Angle);

	void ApplyAcceleration();
	vec2 m_Accel;

	int m_aFlungCharacters[MAX_CLIENTS];
	void FlingTeesInPropellersPath();

	float m_BackPropellerRadius;
	float m_TopPropellerRadius;
	vec2 m_LastTopPropellerA, m_LastTopPropellerB;
	void GetFullPropellerPositions(vec2& outPosA, vec2& outPosB);

	STrail m_aTrails[NUM_TRAILS];
	SBone m_aBones[NUM_BONES];
	CVehicleTurret *m_pTurret;

	// Tile respawn
	bool TryRespawnNewHelicopter();
	int m_DelayTurretType;
	int64 m_NextSpawnTick;
	vec2 m_InitialPosition;
	int m_SpawnTick;

	int m_ExplosionsLeft;
	void HandleExplosions();

	SHeart m_aHearts[NUM_HEARTS];
	int64 m_ShowHeartsUntil;
	int64 m_LastDamage;
	int64 m_LastEnvironmentalDamage;
	int m_LastKnownOwner;
	void InitHearts();
	void UpdateHearts();
	void RegenerateHelicopter();
	void UpdateVisualDamage();
	void DamageInFreeze();
	void DamageInWall();

	int64 m_BroadcastingTick;
	void SendBroadcastIndicator();

	bool m_Build;
	float m_BuildHeight;
	int m_aBuildIDs[NUM_BUILD_IDS];
	float m_BuildBottom, m_BuildTop, m_BuildLeft, m_BuildRight;
	float m_BuildTotalHeight, m_BuildTotalWidth;
	void InitBuild();
	void InitUnbuilt();
	void BuildHelicopter();

	SBone *Body() { return &m_aBones[0]; } // size: NUM_BONES_BODY
	SBone *TopPropeller() { return &m_aBones[NUM_BONES_BODY]; } // size: NUM_BONES_PROPELLERS_TOP
	SBone *BackPropeller() { return &m_aBones[NUM_BONES_BODY + NUM_BONES_PROPELLERS_TOP]; } // size: NUM_BONES_PROPELLERS_BACK
	void SortBones();

public:
	CHelicopter(CGameWorld *pGameWorld, int Spawner, int Team, vec2 Pos, float Scale = 1.f, bool Build = false, int Number = -1, int DelayTurretType = -1);
	virtual ~CHelicopter();

	// Sense
	float GetScale() { return m_Scale; }
	bool IsFlipped() { return m_Flipped; }
	float Angle() { return m_Angle; }
	bool IsExploding() { return m_ExplosionsLeft > -1; }
	bool IsBuilding() { return m_Build; }
	bool IsRegenerating();

	bool IsSpawning() { return m_SpawnTick > -1; }
	bool PlacedByTile() { return m_Number >= 0 && m_DelayTurretType >= TURRETTYPE_NONE; }

	// Manipulating
	bool AttachTurret(CVehicleTurret *helicopterTurret);
	void DestroyTurret();
	void FlingTee(CCharacter *pChar);
	void ApplyScale(float HelicopterScale);
	void Explode();
	void TakeDamage(float Damage, vec2 HitPos, int FromID);
	void ExplosionDamage(float Strength, vec2 Pos, int FromID);

	// Ticking
	void Tick() override;
	void Snap(int SnappingClient) override;
	void Reset() override;

	bool Mount(int ClientID);
	void Dismount();
	void OnInput(CNetObj_PlayerInput *pNewInput);
};

#endif // GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_H
