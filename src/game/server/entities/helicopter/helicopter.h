// made by fokkonaut and Matq, somewhere around 2021 and in 2025

#ifndef GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_H
#define GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_H

#include "helicopter_models.h"
#include "helicopter_turret.h"
#include "game/server/entities/advanced_entity.h"

#define HELICOPTER_DEFAULT_SCALE 0.8f
#define HELICOPTER_MIN_SCALE 0.8f
#define HELICOPTER_MAX_SCALE 5.0f
#define HELICOPTER_PHYSSIZE (vec2(80, 128) * HELICOPTER_DEFAULT_SCALE)

enum
{
	HELICOPTER_DEFAULT,
	HELICOPTER_APACHE,
	NUM_HELICOPTER_TYPES,

	NUM_MAX_SEATS = 2,
};

struct SHelicopterMeta
{
	const char *m_pName;
	float m_BaseHealth;
	int m_NumHeartsIndicator;
	vec2 m_BaseAccel;

	vec2 m_aSeats[NUM_MAX_SEATS];
	int m_NumSeats;
};
extern SHelicopterMeta aHelicopterMetadata[NUM_HELICOPTER_TYPES];

class CHelicopter : public CAdvancedEntity
{
private:
	enum
	{
		MAX_HEARTS = 6,
		NUM_BUILD_IDS = 3, // sparkles/particles
	};

	int m_HelicopterType;
	int m_aPassengers[NUM_MAX_SEATS];
	int m_NumPassengers;

	int m_InputDirection;
	float m_MaxHealth;
	float m_Health;
	int m_NumHearts;
	vec2 m_BaseAccel;
	const char *m_pName;
	vec2 m_aSeats[NUM_MAX_SEATS];
	int m_NumSeats;
	bool m_EngineOn;
	void HandleSeats();

	float m_Scale;
	void HandlePropellers();
	void ResetAndTurnOff();

	bool m_Flipped;
	void Flip();

	float m_Angle;
	void SetRotation(float NewRotation);
	void SetAngle(float Angle);

	void ApplyAcceleration();
	vec2 m_Accel;

	int m_aFlungCharacters[MAX_CLIENTS];
	void FlingTeesInPropellersPath();

	IHelicopterModel *m_pModel;
	void InitModel();

	CVehicleTurret *m_pTurret;

	// Tile respawn
	bool TryRespawnNewHelicopter();
	int m_DelayTurretType;
	int64 m_NextSpawnTick;
	vec2 m_InitialPosition;
	int m_SpawnTick;

	int m_ExplosionsLeft;
	void HandleExplosions();

	SHeart m_aHearts[MAX_HEARTS];
	int64 m_ShowHeartsUntil;
	int64 m_LastDamage;
	int64 m_LastEnvironmentalDamage;
	int m_LastKnownOwner;
	// void InitHearts();
	void UpdateHeartsIndicator();
	void RegenerateHelicopter();
	void UpdateVisualDamage();
	void DamageInFreeze();
	void DamageInWall();

	int64 m_BroadcastingTick;
	void SendBroadcastIndicator();

	bool m_Build;
	float m_BuildHeight;
	int m_aBuildIDs[NUM_BUILD_IDS];
	void InitUnbuilt();
	void BuildHelicopter();

	void SortBones();

public:
	CHelicopter(
		CGameWorld *pGameWorld,
		int HelicopterType,
		int Spawner,
		int Team,
		vec2 Pos,
		float Scale = 1.f,
		bool Build = false,
		int Number = -1,
		int DelayTurretType = -1
	);
	virtual ~CHelicopter();

	// Sense
	int GetHelicopterType() { return m_HelicopterType; }
	float GetScale() { return m_Scale; }
	bool IsFlipped() { return m_Flipped; }
	float Angle() { return m_Angle; }
	bool IsExploding() { return m_ExplosionsLeft > -1; }
	bool IsBuilding() { return m_Build; }
	bool IsRegenerating();
	CCharacter *GetDriver();
	CCharacter *GetGunner();

	bool IsSpawning() { return m_SpawnTick > -1; }
	bool PlacedByTile() { return m_Number >= 0 && m_DelayTurretType >= TURRETTYPE_NONE; }

	// Manipulating
	void SetNumHeartsIndicator(int NumHearts);
	void SetClassAtributes(int HelicopterType, bool SetFullHealth);
	bool AttachTurret(CVehicleTurret *pTurret);
	void DestroyTurret();
	void FlingTee(CCharacter *pChar);
	void ApplyScale(float HelicopterScale);
	void Explode();
	void TakeDamage(float Damage, vec2 HitPos, int FromID);
	void ExplosionDamage(float Strength, vec2 Pos, int FromID);
	void Heal(float Health);

	// Ticking & Events
	void Tick() override;
	void Snap(int SnappingClient) override;
	void Reset() override;

	bool Mount(int ClientID);
	void Dismount(int ClientID); // -1 for all
	void OnInput(CNetObj_PlayerInput *pNewInput, CCharacter *pController);
};

#endif // GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_H
