//
// Created by Matq on 11/04/2025.
//

#ifndef GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_TURRET_H
#define GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_TURRET_H

#include "bone.h"

enum
{
	TURRETTYPE_NONE,
	TURRETTYPE_MINIGUN,
	TURRETTYPE_LAUNCHER,
};

class CHelicopter;
class CVehicleTurret
{
protected:
	friend class CHelicopter;

	int m_TurretType;
	CHelicopter *m_pHelicopter;
	SBone *m_apBones;
	int m_NumBones;

	SBone m_TurretBone; // don't snap this one
	vec2 m_Pivot;
	float m_Length;

	bool m_Flipped;
	virtual void SetFlipped(bool flipped);

	float m_Angle;
	virtual void Rotate(float Angle);

	vec2 m_AimPosition;
	float m_PivotAngle;
	float m_AimingRange;
	void AimTurret();
	virtual void RotateTurret(float ByAngle);

	bool m_Shooting;
	int m_LastShot;
	int m_ShootingCooldown;
	virtual void FireTurret();

	float m_Scale;

	// Generating
	vec2 GetTurretDirection();

public:
	CVehicleTurret(int TurretType, int NumBones,
				   const SBone& TurretBone, const vec2& Pivot,
				   float AimingRange, int ShootingCooldown);
	virtual ~CVehicleTurret();

	// Sense
	IServer *Server();
	CGameWorld *GameWorld();
	CGameContext *GameServer();
	int GetType() { return m_TurretType; }
	int GetNumBones() { return m_NumBones; }
	SBone *Bones() { return m_apBones; } // size: m_NumBones || GetNumBones()

	// Manipulating
	bool TryBindHelicopter(CHelicopter *helicopter);
	virtual void ApplyScale(float TurretScale);

	// Ticking
	virtual void Tick();
	virtual void Snap(int SnappingClient);

	virtual void OnInput(CNetObj_PlayerInput *pNewInput);
};

class CHelicopter;
class CMinigunTurret : public CVehicleTurret
{
private:
	friend class CHelicopter;
	enum
	{
		NUM_BONES_CLUSTER = 3,
		NUM_BONES_RETAINER = 2,
		NUM_BONES = NUM_BONES_CLUSTER + NUM_BONES_RETAINER,
	};

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

	void SetFlipped(bool flipped) override;
	void Rotate(float Angle) override;
	void RotateTurret(float Angle) override;

	int m_ShootingBarrelIndex;
	void FireTurret() override;

	static float AngleDiff() { return 2.f * pi / NUM_BONES_CLUSTER; } // 360deg / 3 = 120deg per bone
	SBone *Cluster() { return &m_apBones[0]; } // size: NUM_BONES_CLUSTER
	SBone *Retainer() { return &m_apBones[NUM_BONES_CLUSTER]; } // size: NUM_BONES_RETAINER

public:
	CMinigunTurret();
	~CMinigunTurret();

	// Manipulating
	virtual void ApplyScale(float TurretScale);

	// Ticking
	void Tick() override;
	void Snap(int SnappingClient) override;
	void OnInput(CNetObj_PlayerInput *pNewInput) override;
};

class CLauncherTurret : public CVehicleTurret
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

	float m_ShaftRadius;
	float m_RetainerPosition;
	float m_RetainerRadius;
	void InitBones();
	void UpdateBones();

//    void SetFlipped(bool flipped) override;
//    void Rotate(float Angle) override;
//    void RotateTurret(float Angle) override;

	float m_RecoilAmount;
	float m_RecoilSpan;
	float m_CurrentRecoilFactor;
	void HandleRecoil();
	void FireTurret() override;

	SBone *Ejector() { return &m_apBones[0]; } // size: NUM_BONES_EJECTOR
	SBone *Shaft() { return &m_apBones[NUM_BONES_EJECTOR]; } // size: NUM_BONES_SHAFT
	SBone *Retainer() { return &m_apBones[NUM_BONES_EJECTOR + NUM_BONES_SHAFT]; } // size: NUM_BONES_RETAINER

public:
	CLauncherTurret();
	virtual ~CLauncherTurret();

	// Manipulating
	virtual void ApplyScale(float TurretScale);

	// Ticking
	void Tick() override;
	void Snap(int SnappingClient) override;
	void OnInput(CNetObj_PlayerInput *pNewInput) override;
};

#endif // GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_TURRET_H
