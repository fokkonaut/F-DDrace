//
// Created by Matq on 02/04/2026.
//

#pragma once

#include "healthbar.h"
#include "../helicopter_turret.h"
#include "game/server/entities/advanced_entity.h"
#include "game/server/entities/bone/vehicle_model.h"

enum
{
	VEHICLETYPE_HELICOPTER,
	VEHICLETYPE_SPIDER,
};

struct SVehicleMeta
{
	const char *m_pName;
	float m_BaseHealth;
	float m_BaseArmorHealth;
	int m_NumHeartsIndicator;
	int m_NumArmorIndicator;
	bool m_VerticalHealthbar;
	int m_NumExplosions;
	vec2 m_BaseSize;
	vec2 m_BaseAccel;
	IVehicleModel *(*m_pModelFactory)(IVehicle *pVehicle);
};

class IVehicle : public CAdvancedEntity
{
protected:
	enum
	{
		// Snap IDs
		MAX_HEARTS = 4,
		MAX_ARMOR = 4,
		NUM_BUILD_IDS = 3, // sparkles/particles

		// Other
		MAX_BONES_SORT = 128,
	};

	int m_VehicleType;
	vec2 m_BaseSize;
	float m_Scale;
	int m_InputDirection;
	float m_MaxHealth;
	float m_Health;
	float m_MaxArmor;
	float m_Armor;
	const char *m_pName;
	bool m_EngineOn;
	IVehicleModel *m_pModel;

	IVehicleTurret **m_apAttachments;
	int m_AttachmentsCap;
	int m_NumAttachments;
	void TickAttachments();

	bool m_Flipped;
	float m_Angle;
	float m_VisualAngle;
	virtual void SetRotation(float NewRotation);

	vec2 m_BaseAccel;
	vec2 m_Accel;
	void HandleHookedPassengers();
	virtual void ApplyAcceleration();

	int m_aFlungCharacters[MAX_CLIENTS];
	void FlingTeesInPropellersPath();

	// Switch layer | with `int m_Number`
	// int m_DelayTurretType;
	int64 m_NextSpawnTick;
	vec2 m_InitialPosition;
	int m_SpawnTick;
	int m_SwitchDelay;
	bool HandleSpawning();
	virtual bool TryRespawnNewVehicle();

	int m_ExplosionsOnDeath;
	int m_ExplosionsLeft;
	void HandleExplosions();

	int m_DriverFreezeTime;
	virtual void HandleSeat(SSeat& Seat, int PassengerCID, CCharacter* pChar);
	void HandleSeats();
	virtual void DriversDismounted();
	virtual void DriversFrozen();

	void TickRopes();
	void TickPropellers();

	CHealthBar m_HealthBar;
	int64 m_ShowHealthbarUntil;
	int64 m_LastDamage;
	int64 m_LastEnvironmentalDamage;
	int m_LastKnownOwner;
	void RegenerateArmor();
	void TickVisualBoneDamage();
	void DamageInFreeze();
	void DamageInWall();

	int64 m_BroadcastingTick;
	bool CanSendBroadcastIndicator();
	virtual void SendBroadcastIndicator();
	void SendBroadcastToPassengers(const char *pMsg);

	struct SBuild
	{
		int m_StartTick;
		int m_Duration;
		bool m_Building;
		float m_CachedHeight;
		int m_aBuildIDs[NUM_BUILD_IDS];
	} m_Build;
	void InitBuildAnimation();
	bool HandleBuilding();

	// Getting
	bool IsPassenger(CCharacter *pChar);
	bool ShouldShowHealthbar();
	bool ShouldFlashDamagedHearts();

	// Manipulating
	void SortBones();

	// Ticking
	void SnapBuildingParticles(int SnappingClient);
	void SnapHealthBar(int SnappingClient);
	void SnapModel(int SnappingClient, const SBoneModelSnapping& Options);
	void SnapAttachments(int SnappingClient, const SBoneModelSnapping& Options);
	void UpdateAttachmentControllerCounts();

public:
	static vec2 MinimumVehicleHitbox(vec2 Hitbox);

public:
	IVehicle(CGameWorld *pGameWorld, int VehicleType, int Objtype, vec2 Pos, vec2 BaseSize, int Owner, int Team, int Number, int BuildTime);
	~IVehicle();

	// Getting
	float GetScale() { return m_Scale; }
	int LastKnownOwnerCID() { return m_LastKnownOwner; }
	bool IsExploding() { return m_ExplosionsLeft; }
	bool IsBuilding() { return m_Build.m_Building; }
	bool IsSpawning() { return m_SpawnTick > -1; }
	bool PlacedByTile() { return m_Number >= 0 && m_SwitchDelay >= 0; }
	bool IsInvincible() { return IsBuilding() || IsSpawning() || IsExploding(); }
	float Health() { return m_Health; }
	float MaxHealth() { return m_MaxHealth; }
	float MaxArmor() { return m_MaxArmor; }
	float Armor() { return m_Armor; }
	bool IsFullHealthAndArmor() { return m_Health == m_MaxHealth && m_Armor == m_MaxArmor; }
	bool CanRegenerateArmor();
	CCharacter *GetDriver();
	int GetNextAvailableSeat(int CheckFromIndex = 0);
	IVehicleModel *Model() { return m_pModel; }
	float Angle() { return m_Angle; }
	bool IsFlipped() { return m_Flipped; }
	IVehicleTurret** Attachments() { return m_apAttachments; }
	int NumAttachments() { return m_NumAttachments; }

	// Manipulating
	bool TryAttach(IVehicleTurret *pNewTurret);
	void AllocateNumAttachments(int NumAttachments);
	void SetVehicleMetadata(const SVehicleMeta& metadata, bool HealFullyToo);
	virtual void ApplyScale(float VehicleScale);
	void Explode();
	void TakeDamage(float Damage, vec2 HitPos, int FromID);
	void ExplosionDamage(float Strength, vec2 Pos, int FromID);
	void HealHealth(float Health);
	void HealArmor(float Armor);

	// Ticking & Events
	void Tick() override;
	void Snap(int SnappingClient) override;
	void Reset() override;

	bool Mount(int ClientID, int WantedSeat = -1); // -1 next available
	void Dismount(int ClientID, bool ForceDismountAtHelicopter = true); // -1 for all
	virtual bool OnInput(CNetObj_PlayerInput *pNewInput, CCharacter *pControllerChar);
};
