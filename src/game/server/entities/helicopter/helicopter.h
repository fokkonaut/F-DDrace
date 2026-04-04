// made by fokkonaut and Matq, somewhere around 2021 and in 2025

#ifndef GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_H
#define GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_H

#include "helicopter_models.h"
#include "helicopter_turret.h"
#include "game/server/entities/advanced_entity.h"

#define HELICOPTER_DEFAULT_SCALE 0.64f
#define HELICOPTER_MIN_SCALE 1.0f
#define HELICOPTER_MAX_SCALE 5.0f
#define HELICOPTER_PHYSSIZE vec2(80, 128)

enum
{
	HELICOPTER_DEFAULT,
	HELICOPTER_APACHE,
	HELICOPTER_CHINOOK,
	NUM_HELICOPTER_TYPES,

	NUM_MAX_SEATS = 2,
};

namespace Helicopters
{
struct SHelicopterMeta
{
	const char *m_pName;
	float m_BaseHealth;
	float m_BaseArmorHealth;
	int m_NumHeartsIndicator;
	int m_NumArmorIndicator;
	int m_NumExplosions;
	vec2 m_BaseSize;
	vec2 m_BaseAccel;
	IHelicopterModel *(*m_pModelFactory)(CHelicopter *pHelicopter);
};

extern SHelicopterMeta aHelicopterMetadata[NUM_HELICOPTER_TYPES];
}

class CHelicopter : public CAdvancedEntity // class CVehicle : public CAdvancedEntity
{
private:
	enum
	{
		// Snap IDs
		MAX_HEARTS = 4,
		MAX_ARMOR = 4,
		NUM_BUILD_IDS = 3, // sparkles/particles

		// Other
		MAX_BONES_SORT = 128,
	};

	int m_HelicopterType;

	int m_InputDirection;
	float m_MaxHealth;
	float m_Health;
	float m_MaxArmor;
	float m_Armor;
	vec2 m_BaseAccel;
	const char *m_pName;
	bool m_EngineOn;

	void HandleSeats();

	float m_Scale;
	void HandlePropellers();
	void ResetAndTurnOff();

	bool m_Flipped;

	float m_Angle;
	float m_VisualAngle;
	void SetRotation(float NewRotation);

	void ApplyAcceleration();
	vec2 m_Accel;

	int m_aFlungCharacters[MAX_CLIENTS];
	void FlingTeesInPropellersPath();

	IHelicopterModel *m_pModel;
	CVehicleTurret *m_pTurret;

	// Tile respawn
	bool TryRespawnNewHelicopter();
	int m_DelayTurretType;
	vec2 m_InitialPosition;

	int m_ExplosionsOnDeath;
	int m_ExplosionsLeft;
	void HandleExplosions();

	CPickupNode m_aHeartsIndicator[MAX_HEARTS];
	int m_NumHeartsIndicator;
	CPickupNode m_aArmorIndicator[MAX_ARMOR];
	int m_NumArmorIndicator;
	void SetNumIndicator(CPickupNode *aPickups, int& NumPickups, int NewNumPickups, int MaxPickups, int PowerupType);

	int64 m_ShowHealthbarUntil;
	int64 m_LastDamage;
	int64 m_LastEnvironmentalDamage;
	int m_LastKnownOwner;
	void UpdateHealthbarIndicator();
	void RegenerateHelicopterArmor();
	void UpdateVisualDamage();
	void DamageInFreeze();
	void DamageInWall();

	int64 m_BroadcastingTick;
	void SendBroadcastIndicator();

	struct SBuild
	{
		int m_StartTick;
		int m_Duration;
		bool m_Building;
		float m_CachedHeight;
		int m_aBuildIDs[NUM_BUILD_IDS];
	} m_Build;
	void InitBuildAnimation();
	void BuildHelicopter();

	// Getting
	bool IsPassenger(CCharacter *pChar);
	bool ShouldShowHealthbar();
	bool ShouldFlashDamagedHearts();

	// Manipulating
	void SortBones();

public:
	CHelicopter(
		CGameWorld *pGameWorld,
		int HelicopterType,
		int Spawner,
		int Team,
		vec2 Pos,
		float Scale = 1.f,
		int BuildTime = 0,
		// 0 for no build animation
		int Number = -1,
		int DelayTurretType = -1
	);
	virtual ~CHelicopter();

	// Getting
	int GetHelicopterType() { return m_HelicopterType; }
	float GetScale() { return m_Scale; }
	bool IsFlipped() { return m_Flipped; }
	float Angle() { return m_Angle; }
	int LastKnownOwnerCID() { return m_LastKnownOwner; }
	bool IsExploding() { return m_ExplosionsLeft; }
	bool IsBuilding() { return m_Build.m_Building; }
	bool IsInvincible() { return IsBuilding() || IsExploding(); }
	bool IsFullHealthAndArmor() { return m_Health == m_MaxHealth && m_Armor == m_MaxArmor; }
	bool CanRegenerateArmor();
	CCharacter *GetDriver();
	CCharacter *GetGunner();
	int GetNextAvailableSeat(int AfterIndex = 0);
	IHelicopterModel *Model() { return m_pModel; }

	bool PlacedByTile() { return m_Number >= 0; }

	// Manipulating
	void SetNumHeartsIndicator(int NumHearts);
	void SetNumArmorIndicator(int NumArmor);
	void SetClassType(int HelicopterType, bool HealFullyToo);
	bool TryAttachTurret(CVehicleTurret *pNewTurret);
	void DestroyTurret();
	void FlingTee(CCharacter *pChar);
	void ApplyScale(float HelicopterScale);
	void Explode();
	void TakeDamage(float Damage, vec2 HitPos, int FromID);
	void ExplosionDamage(float Strength, vec2 Pos, int FromID);
	void Heal(float Health);
	void HealArmor(float Armor);

	// Ticking & Events
	void Tick() override;
	void Snap(int SnappingClient) override;
	void Reset() override;

	bool Mount(int ClientID, int WantedSeat = -1); // -1 next available
	void Dismount(int ClientID, bool ForceDismountAtHelicopter = true); // -1 for all
	void OnInput(CNetObj_PlayerInput *pNewInput, CCharacter *pController);
};

#endif // GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_H
