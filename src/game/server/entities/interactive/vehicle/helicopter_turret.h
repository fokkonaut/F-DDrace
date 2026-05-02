//
// Created by Matq on 11/04/2025.
//

#ifndef GAME_SERVER_ENTITIES_INTERACTIVE_VEHICLE_HELICOPTER_TURRET_H
#define GAME_SERVER_ENTITIES_INTERACTIVE_VEHICLE_HELICOPTER_TURRET_H

#include <game/server/entities/misc/bone/base/bone_model.h>

enum
{
	TURRETTYPE_NONE,
	TURRETTYPE_MINIGUN,
	TURRETTYPE_LAUNCHER,
	NUM_TURRET_TYPES
};

class IVehicle;
class IVehicleTurret
{
protected:
	int m_TurretType;
	IVehicle *m_pVehicle;
	CBone *m_apBones;
	int m_NumBones;

	CBone m_TurretBone; // don't snap this one
	vec2 m_InitPivot;
	vec2 m_Pivot;
	float m_Length;
	float m_Angle;

	virtual void UpdateVisual(); // used for Tick() or separately when manipulating bones

	int m_Controllers;
	vec2 m_TargetPosition;
	vec2 m_TargetPositionWorld;
	float m_TurretAngle;
	float m_AimingRange;
	void AimTurret();

	int m_ShooterCID;
	bool m_Shooting;
	int m_LastShot;
	int m_ShootingCooldown;
	virtual void FireTurret();

	float m_Scale;

	// Generating
	vec2 GetTurretDirection();

public:
	IVehicleTurret(int TurretType,
	               int NumBones,
	               const CBone& TurretBone,
	               const vec2& Pivot,
	               float AimingRange,
	               int ShootingCooldown);
	virtual ~IVehicleTurret();

	// Sense
	IServer *Server();
	CGameWorld *GameWorld();
	CGameContext *GameServer();
	int GetType() { return m_TurretType; }
	int NumBones() { return m_NumBones; }
	CBone *Bones() { return m_apBones; } // size: m_NumBones || GetNumBones()
	float GetTurretRotation() { return m_TurretAngle; }
	int NumControllers() { return m_Controllers; }
	Mask128 GetShooterTeamMask();

	// Manipulating
	bool TryBindVehicle(IVehicle *pVehicle);
	virtual void ApplyScale(float TurretScale);
	void Dismounted();
	void SetRotation(float PivotRotation, float TurretRotation);
	void AddController() { m_Controllers++; }
	void RemoveController() { m_Controllers--; }
	void ResetNumControllers() { m_Controllers = 0;}
	void CountControllers();

	// Ticking
	virtual void Tick();
	virtual void Snap(int SnappingClient, const SBoneModelSnapping& Options);

	void OnInput(CNetObj_PlayerInput *pNewInput, CCharacter *pController);
};

class CHelicopter;
class CMinigunTurret : public IVehicleTurret
{
private:
	friend class CHelicopter;
	enum
	{
		NUM_BONES_CLUSTER = 3,
		NUM_BONES_RETAINER = 2,
		NUM_BONES = NUM_BONES_CLUSTER + NUM_BONES_RETAINER,
	};

	void UpdateVisual() override;

	// Cluster - N parts rotating along the direction of the turret
	float m_ClusterRotation;
	float m_ClusterSpeed;
	float m_ClusterVerticalRange;
	float m_ClusterHorizontalRange;
	void InitCluster();
	void UpdateClusterBones();
	void SpinCluster();

	// Retainer - 2 parts `holding` the cluster together
	float m_RetainerPosition;
	float m_RetainerRadius;
	void InitRetainer();

	// void SetFlipped(bool flipped) override;
	// void SetRotation(float PivotRotation, float TurretRotation) override;

	int m_ShootingBarrelIndex;
	void FireTurret() override;

	static float AngleDiff() { return 2.f * pi / NUM_BONES_CLUSTER; } // 360deg / 3 = 120deg per bone
	CBone *Cluster() { return &m_apBones[0]; } // size: NUM_BONES_CLUSTER
	CBone *Retainer() { return &m_apBones[NUM_BONES_CLUSTER]; } // size: NUM_BONES_RETAINER

public:
	CMinigunTurret();
	~CMinigunTurret();

	// Manipulating
	virtual void ApplyScale(float TurretScale);

	// Ticking
	void Tick() override;
	void Snap(int SnappingClient, const SBoneModelSnapping& Options) override;
};

class CLauncherTurret : public IVehicleTurret
{
private:
	friend class CHelicopter;
	enum
	{
		NUM_BONES_EJECTOR = 1,
		NUM_BONES_SHAFT = 3,
		NUM_BONES_RETAINER = 2,
		NUM_BONES = NUM_BONES_EJECTOR + NUM_BONES_SHAFT + NUM_BONES_RETAINER,
	};

	void UpdateVisual() override;

	float m_ShaftRadius;
	float m_RetainerPosition;
	float m_RetainerRadius;
	void InitBones();
	void UpdateBones();

	float m_RecoilAmount;
	float m_RecoilSpan;
	float m_CurrentRecoilFactor;
	void HandleRecoil();
	void FireTurret() override;

	CBone *Ejector() { return &m_apBones[0]; } // size: NUM_BONES_EJECTOR
	CBone *Shaft() { return &m_apBones[NUM_BONES_EJECTOR]; } // size: NUM_BONES_SHAFT
	CBone *Retainer() { return &m_apBones[NUM_BONES_EJECTOR + NUM_BONES_SHAFT]; } // size: NUM_BONES_RETAINER

public:
	CLauncherTurret();
	virtual ~CLauncherTurret();

	// Manipulating
	virtual void ApplyScale(float TurretScale);

	// Ticking
	void Tick() override;
	void Snap(int SnappingClient, const SBoneModelSnapping& Options) override;
};

#endif // GAME_SERVER_ENTITIES_INTERACTIVE_VEHICLE_HELICOPTER_TURRET_H
