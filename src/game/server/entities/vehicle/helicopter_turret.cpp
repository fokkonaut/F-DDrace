//
// Created by Matq on 11/04/2025.
//

#include "helicopter_turret.h"
#include "game/server/entities/vehicle/helicopter.h"
#include "game/server/entities/projectile.h"
#include "game/server/entities/missile.h"

void IVehicleTurret::UpdateVisual()
{
}

void IVehicleTurret::SetRotation(float PivotRotation, float TurretRotation) //
{
	m_Angle = PivotRotation;
	m_TurretAngle = TurretRotation;

	// Rotate turret
	m_TurretBone.m_To = m_TurretBone.m_InitTo - m_Pivot;
	m_TurretBone.m_From = m_TurretBone.m_InitFrom - m_Pivot;
	m_TurretBone.Rotate(TurretRotation);
	if (m_pVehicle->IsFlipped())
		m_TurretBone.Flip();

	m_TurretBone.m_To += m_Pivot;
	m_TurretBone.m_From += m_Pivot;
	m_TurretBone.Rotate(PivotRotation);

	for (int i = 0; i < m_NumBones; i++)
	{
		m_apBones[i].m_To = m_apBones[i].m_InitTo - m_Pivot;
		m_apBones[i].m_From = m_apBones[i].m_InitFrom - m_Pivot;
		m_apBones[i].Rotate(TurretRotation);
		if (m_pVehicle->IsFlipped())
			m_apBones[i].Flip();

		m_apBones[i].m_To += m_Pivot;
		m_apBones[i].m_From += m_Pivot;
		m_apBones[i].Rotate(PivotRotation);
	}
}

void IVehicleTurret::CountControllers()
{
	if (!m_pVehicle || !m_pVehicle->Model())
		return;

	IVehicleModel *pModel = m_pVehicle->Model();

	m_Controllers = 0;
	for (int i = 0; i < pModel->NumSeats(); i++)
	{
		SSeat& Seat = pModel->Seats()[i];
		if (Seat.m_SeatedCID == -1 || // Empty seat
			Seat.m_AttachmentID < 0 || // Undefined idx
			Seat.m_AttachmentID >= m_pVehicle->NumAttachments() || // Invalid idx
			!m_pVehicle->Attachments()[Seat.m_AttachmentID]) // Nonexistent attachment
			continue;

		// This turret is the referenced attachment by an occupied seat
		if (this == m_pVehicle->Attachments()[Seat.m_AttachmentID])
			m_Controllers++;
	}
}

void IVehicleTurret::AimTurret()
{
	float flip = (m_pVehicle->IsFlipped() ? -1.0f : 1.0f);

	vec2 aimFromTurret;
	if (m_Controllers > 0)
	{
		vec2 gunOrigin = m_TurretBone.m_To;
		aimFromTurret = (m_TargetPositionWorld - m_pVehicle->GetPos() - gunOrigin) * flip;
	}
	else
	{
		aimFromTurret = vec2(1000, 0);
	}

	float targetAngle = (atan2f(aimFromTurret.y, aimFromTurret.x) / pi * 180.f) * flip;
	float targetAngleClamped = clamp(targetAngle, m_Angle * flip - m_AimingRange, m_Angle * flip + m_AimingRange) - m_Angle * flip;
	SetRotation(m_Angle, m_TurretAngle + (targetAngleClamped - m_TurretAngle) * 0.15f);
}

void IVehicleTurret::FireTurret()
{
	if (!m_Shooting || Server()->Tick() - m_LastShot <= m_ShootingCooldown)
		return;

	m_LastShot = Server()->Tick();

	vec2 barrelExit = m_TurretBone.m_From;
	vec2 startingPos = m_pVehicle->GetPos() + barrelExit;

	vec2 Direction = GetTurretDirection();
	vec2 projectileDirection = normalize(Direction + vec2(-Direction.y, Direction.x));

	new CProjectile(
		GameWorld(),
		WEAPON_GUN,
		m_ShooterCID,
		startingPos,
		projectileDirection,
		Server()->TickSpeed() * 2,
		false, false,
		0.f, -1,
		projectileDirection * 100
	);
}

vec2 IVehicleTurret::GetTurretDirection()
{
	return normalize(m_TurretBone.m_From - m_TurretBone.m_To);
}

IVehicleTurret::IVehicleTurret(
	int TurretType,
	int NumBones,
	const CBone& TurretBone,
	const vec2& Pivot,
	float AimingRange,
	int ShootingCooldown
)
{
	m_TurretType = TurretType;
	m_pVehicle = nullptr;
	m_apBones = new CBone[NumBones];
	m_NumBones = NumBones;

	m_TurretBone = TurretBone;
	m_InitPivot = Pivot;
	m_Pivot = Pivot;
	m_Length = m_TurretBone.GetLength();
	m_Angle = 0.f;

	m_Controllers = 0;
	m_TargetPosition = vec2(0.f, 0.f);
	m_TargetPositionWorld = vec2(0.f, 0.f);
	m_TurretAngle = 0.f;
	m_AimingRange = AimingRange;

	m_Shooting = false;
	m_LastShot = 0;
	m_ShootingCooldown = ShootingCooldown;

	m_Scale = 1.f;
}

IVehicleTurret::~IVehicleTurret()
{
	if (m_pVehicle) // if helicopter was never set, ids were never set either
		for (int i = 0; i < m_NumBones; i++)
			if (m_apBones[i].m_ID != -1)
				Server()->SnapFreeID(m_apBones[i].m_ID);
	delete[] m_apBones;
}

IServer *IVehicleTurret::Server()
{
	return m_pVehicle->Server();
}

CGameWorld *IVehicleTurret::GameWorld()
{
	return m_pVehicle->GameWorld();
}

CGameContext *IVehicleTurret::GameServer()
{
	return m_pVehicle->GameServer();
}

Mask128 IVehicleTurret::GetShooterTeamMask()
{
	CCharacter *pShooter = GameServer()->GetPlayerChar(m_ShooterCID);
	return pShooter ? pShooter->TeamMask() : Mask128();
}

bool IVehicleTurret::TryBindVehicle(IVehicle *pVehicle)
{
	// returns true on passed ownership, on failure clean up yourself
	if (pVehicle == nullptr)
		return false;

	if (m_pVehicle) // really only assign owner once
		return false;

	m_pVehicle = pVehicle;
	for (int i = 0; i < m_NumBones; i++)
		m_apBones[i].AssignEntityAndID(m_pVehicle, Server()->SnapNewID());

	// match posture
	ApplyScale(m_pVehicle->GetScale());
	UpdateVisual();
	SetRotation(m_pVehicle->Angle(), m_TurretAngle);
	return true;
}

void IVehicleTurret::ApplyScale(float TurretScale)
{
	m_Scale *= TurretScale;
	m_Length *= TurretScale;
	m_Pivot *= TurretScale;
	m_TurretBone.Scale(TurretScale);
	for (int i = 0; i < m_NumBones; i++)
		m_apBones[i].Scale(TurretScale);
}

void IVehicleTurret::Dismounted()
{
	m_Shooting = false;
}

void IVehicleTurret::Tick()
{
	AimTurret();
	FireTurret();
	UpdateVisual();
}

void IVehicleTurret::Snap(int SnappingClient, const SBoneModelSnapping& Options)
{
	for (int i = 0; i < m_NumBones; i++)
		m_apBones[i].Snap(SnappingClient, Options);
}

void IVehicleTurret::OnInput(CNetObj_PlayerInput *pNewInput, CCharacter *pController)
{
	if (pController->m_FreezeTime)
		return;

	m_TargetPosition = vec2((float)pNewInput->m_TargetX, (float)pNewInput->m_TargetY);
	m_TargetPositionWorld = pController->GetCursorPos();
	m_Shooting = pNewInput->m_Fire % 2 == 1;
	m_ShooterCID = pController->GetPlayer()->GetCID();
}

//

void CMinigunTurret::UpdateVisual()
{
	UpdateClusterBones();
}

void CMinigunTurret::InitCluster()
{
	UpdateClusterBones();

	for (int i = 0; i < NUM_BONES_CLUSTER; i++)
	{
		Cluster()[i].m_Color = LASERTYPE_FREEZE;
		Cluster()[i].m_Thickness = 3;
	}
}

void CMinigunTurret::UpdateClusterBones()
{
	vec2 Direction = GetTurretDirection(); // gun direction
	vec2 Perpendicular = vec2(-Direction.y, Direction.x); // gun perpendicular

	float flip = (m_pVehicle && m_pVehicle->IsFlipped()) ? -1.0f : 1.0f;
	for (int i = 0; i < NUM_BONES_CLUSTER; i++)
	{
		float currentBoneRotation = m_ClusterRotation + AngleDiff() * (float)i;
		float depthOffset = cosf(currentBoneRotation); // also horizontal
		float verticalOffset = sinf(currentBoneRotation) * m_ClusterVerticalRange;
		float horizontalOffset = depthOffset * m_ClusterHorizontalRange * flip;

		// elliptical motion
		vec2 verticalDirection = Perpendicular * verticalOffset;
		vec2 horizontalDirection = Direction * horizontalOffset;

		vec2 Combined = verticalDirection + horizontalDirection;
		vec2 barrelStartPos = m_TurretBone.m_To + Combined;
		vec2 barrelEndPos = m_TurretBone.m_From + Combined;

		Cluster()[i].m_To = barrelStartPos;
		Cluster()[i].m_From = barrelEndPos;
		Cluster()[i].m_Thickness = Cluster()[i].m_InitThickness - round_to_int(depthOffset) - 1;
	}
}

void CMinigunTurret::SpinCluster()
{
	if (m_Shooting)
		m_ClusterSpeed += (float)Server()->TickSpeed() / 5000.f;
	m_ClusterSpeed *= 0.98f;
	m_ClusterRotation += m_ClusterSpeed;
}

void CMinigunTurret::InitRetainer()
{
	float Radius = m_RetainerRadius;
	vec2 initialPosition = m_TurretBone.m_From + vec2(m_RetainerPosition, 0);
	for (int i = 0; i < NUM_BONES_RETAINER; i++)
	{
		vec2 fromPosition = initialPosition;
		vec2 toPosition = initialPosition + vec2(0.f, Radius);

		Retainer()[i].m_To = toPosition;
		Retainer()[i].m_From = fromPosition;
		Retainer()[i].m_Color = LASERTYPE_FREEZE;
		Retainer()[i].m_Thickness = 2;
		Radius = -Radius;
	}
}

void CMinigunTurret::FireTurret()
{
	if (!m_Shooting || m_ClusterSpeed < 0.3f)
		return;

	if (Server()->Tick() - m_LastShot <= m_ShootingCooldown)
		return;

	m_LastShot = Server()->Tick();

	vec2 startingPos = m_pVehicle->GetPos() + Cluster()[m_ShootingBarrelIndex].m_From;
	vec2 Direction = GetTurretDirection();

	float spreadRandomness = (float)(random_int() % 21 - 10) / 400.f;
	vec2 projectileDirection = normalize(Direction + vec2(-Direction.y, Direction.x) * spreadRandomness);
	new CProjectile(
		GameWorld(),
		WEAPON_GUN,
		m_ShooterCID,
		startingPos,
		projectileDirection,
		Server()->TickSpeed() * 2,
		false, true,
		0.f, SOUND_GRENADE_EXPLODE,
		projectileDirection * 1000 // bug
	);
	GameServer()->CreateSound(startingPos, SOUND_GRENADE_FIRE, GetShooterTeamMask());

	m_ShootingBarrelIndex = (m_ShootingBarrelIndex + 1) % NUM_BONES_CLUSTER;
	m_pVehicle->SetVel(m_pVehicle->GetVel() + -Direction * 0.5f);
}

CMinigunTurret::CMinigunTurret()
	: IVehicleTurret(TURRETTYPE_MINIGUN, NUM_BONES,
	                 CBone(nullptr, -1, vec2(70.f, 50.f), vec2(-34.f, 50.f)),
	                 vec2(4.f, 50.f), 35.f, 7)
{
	m_pVehicle = nullptr;

	m_ClusterRotation = 0.f;
	m_ClusterSpeed = 0.f;
	m_ClusterVerticalRange = 8.f;
	m_ClusterHorizontalRange = 5.f;

	m_RetainerPosition = -14.f; // from the tip of the turret
	m_RetainerRadius = 9.f;

	m_ShootingBarrelIndex = 0;

	InitCluster();
	InitRetainer();
	for (int i = 0; i < NUM_BONES; i++)
		Bones()[i].Save();
}

CMinigunTurret::~CMinigunTurret()
{
}

void CMinigunTurret::ApplyScale(float TurretScale)
{
	// Experimental
	m_Length *= TurretScale;
	m_Pivot *= TurretScale;
	m_RetainerPosition *= TurretScale;
	m_RetainerRadius *= TurretScale;
	m_ClusterHorizontalRange *= TurretScale;
	m_ClusterVerticalRange *= TurretScale;
	m_TurretBone.Scale(TurretScale);
	for (int i = 0; i < m_NumBones; i++) // Cluster will scale from m_TurretBone automatically
		m_apBones[i].Scale(TurretScale);
	//
}

void CMinigunTurret::Tick()
{
	//    if (!m_pHelicopter) don't even try to run this function without a heli
	//        return;

	AimTurret();
	FireTurret();

	SpinCluster();
	UpdateVisual();
}

void CMinigunTurret::Snap(int SnappingClient, const SBoneModelSnapping& Options)
{
	for (int i = 0; i < m_NumBones; i++)
		m_apBones[i].Snap(SnappingClient, Options);
}

//

void CLauncherTurret::UpdateVisual()
{
	UpdateBones();
}

void CLauncherTurret::InitBones()
{
	UpdateBones();

	Shaft()[0].m_Thickness = 2;
	Shaft()[1].m_Thickness = 2;
	Shaft()[2].m_Thickness = 2;

	Retainer()[0].m_Thickness = 2;
	Retainer()[1].m_Thickness = 2;

	for (int i = 0; i < NUM_BONES; i++)
		m_apBones[i].m_Color = LASERTYPE_FREEZE;
	Ejector()[0].m_Color = LASERTYPE_SHOTGUN;
}

void CLauncherTurret::UpdateBones()
{
	vec2 Direction = GetTurretDirection(); // gun direction, length: 1
	vec2 Perpendicular = vec2(-Direction.y, Direction.x); // direction perpendicular of the gun, length: 1

	vec2 perpendicularShaft = Perpendicular * m_ShaftRadius;
	vec2 perpendicularRetainer = Perpendicular * m_RetainerRadius;
	vec2 retainerPosDiff = Direction * m_RetainerPosition;
	vec2 recoilPosDiff = Direction * m_CurrentRecoilFactor * m_RecoilAmount;

	// Retraction animation
	m_TurretBone.m_From = m_TurretBone.m_To + Direction * (m_Length - m_CurrentRecoilFactor * m_RecoilAmount);

	// Middle
	Ejector()[0].m_From = m_TurretBone.m_From - retainerPosDiff;
	Ejector()[0].m_To = m_TurretBone.m_To - Direction * 12.f + Direction * (m_Length - m_RetainerPosition) * m_CurrentRecoilFactor - recoilPosDiff;

	// Base
	Shaft()[0].m_To = m_TurretBone.m_To + perpendicularShaft - recoilPosDiff;
	Shaft()[0].m_From = m_TurretBone.m_To - perpendicularShaft - recoilPosDiff;
	// Sides (startpoint): same as base
	Shaft()[1].m_To = Shaft()[0].m_To;
	Shaft()[2].m_To = Shaft()[0].m_From;
	// Sides (endpoint)
	Shaft()[1].m_From = m_TurretBone.m_From + perpendicularShaft;
	Shaft()[2].m_From = m_TurretBone.m_From - perpendicularShaft;

	// Retainer (midpoint)
	Retainer()[0].m_From = m_TurretBone.m_From - retainerPosDiff;
	Retainer()[1].m_From = Retainer()[0].m_From;
	// Retainer (sides)
	Retainer()[0].m_To = Retainer()[0].m_From + perpendicularRetainer;
	Retainer()[1].m_To = Retainer()[1].m_From - perpendicularRetainer;
}

void CLauncherTurret::HandleRecoil()
{
	m_CurrentRecoilFactor = 1.f - clamp((float)(Server()->Tick() - m_LastShot) / m_RecoilSpan, 0.f, 1.f);
}

void CLauncherTurret::FireTurret()
{
	if (!m_Shooting)
		return;

	if (Server()->Tick() - m_LastShot <= m_ShootingCooldown)
		return;

	m_LastShot = Server()->Tick();

	vec2 startingPos = m_pVehicle->GetPos() + m_TurretBone.m_From;
	vec2 Direction = GetTurretDirection();
	new CMissile(
		GameWorld(),
		m_ShooterCID,
		startingPos,
		m_pVehicle->GetVel() * 0.5f + Direction * 8.0f,
		Direction,
		Server()->TickSpeed() * 3
	);
	Mask128 TeamMask = GetShooterTeamMask();
	GameServer()->CreateSound(startingPos, SOUND_GRENADE_FIRE, TeamMask);
	GameServer()->CreateSound(startingPos, SOUND_GRENADE_EXPLODE, TeamMask);

	m_pVehicle->SetVel(m_pVehicle->GetVel() + -Direction * 3.f);
}

CLauncherTurret::CLauncherTurret()
	: IVehicleTurret(TURRETTYPE_LAUNCHER, NUM_BONES,
	                 CBone(nullptr, -1, vec2(70.f, 50.f), vec2(-44.f, 50.f)),
	                 vec2(-10.f, 50.f), 35.f, 50)
{
	m_pVehicle = nullptr;

	m_ShaftRadius = 7.f;
	m_RetainerPosition = 16.f; // From the tip of the launcher
	m_RetainerRadius = 11.f;

	m_RecoilAmount = 20.f;
	m_RecoilSpan = 30.f; // Ticks until normal position
	m_CurrentRecoilFactor = 0.f;

	InitBones();
	for (int i = 0; i < NUM_BONES; i++)
		Bones()[i].Save();
}

CLauncherTurret::~CLauncherTurret()
{
}

void CLauncherTurret::ApplyScale(float TurretScale)
{
	// Experimental
	m_Length *= TurretScale;
	m_Pivot *= TurretScale;
	m_ShaftRadius *= TurretScale;
	m_RetainerPosition *= TurretScale;
	m_RetainerRadius *= TurretScale;
	m_RecoilAmount *= TurretScale;
	m_TurretBone.Scale(TurretScale);
	for (int i = 0; i < m_NumBones; i++)
		m_apBones[i].Scale(TurretScale);
	//
}

void CLauncherTurret::Tick()
{
	AimTurret();
	FireTurret();

	HandleRecoil();
	UpdateVisual();
}

void CLauncherTurret::Snap(int SnappingClient, const SBoneModelSnapping& Options)
{
	for (int i = 0; i < m_NumBones; i++)
		m_apBones[i].Snap(SnappingClient, Options);
}
