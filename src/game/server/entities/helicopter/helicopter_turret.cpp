//
// Created by Matq on 11/04/2025.
//

#include "missile.h"
#include "helicopter.h"
#include "game/server/entities/projectile.h"
#include "helicopter_turret.h"

void CVehicleTurret::SetFlipped(bool flipped)
{ // SetFlipped instead of Flip because if turret is attached later to match the helicopter flip, flip |ovo|
	if (flipped == m_Flipped)
		return;

	m_Flipped = !m_Flipped;
	m_Angle *= -1.f;
	m_PivotAngle *= -1.f;
	m_Pivot.x *= -1.f;
	m_TurretBone.Flip();
	for (int i = 0; i < m_NumBones; i++)
		m_apBones[i].Flip();
}

void CVehicleTurret::Rotate(float Angle)
{
	m_Angle += Angle;
	m_PivotAngle += Angle;
	rotate(m_Pivot, Angle);
	m_TurretBone.Rotate(Angle);
	for (int i = 0; i < m_NumBones; i++)
		m_apBones[i].Rotate(Angle);
}

void CVehicleTurret::AimTurret()
{
	if (!m_pHelicopter->GetOwner())
		return;

	vec2 aimFromTurret = (m_pHelicopter->GetOwner()->GetCursorPos() - m_pHelicopter->GetPos() - m_TurretBone.m_To) * (m_Flipped ? -1.f : 1.f);
	float targetAngle = (atan2f(aimFromTurret.y, aimFromTurret.x) / pi * 180.f);

	float targetAngleClamped = clamp(targetAngle, m_Angle - m_AimingRange, m_Angle + m_AimingRange);
	float correctionAngle = (targetAngleClamped - m_PivotAngle) * 0.15f; // Slow rotation

	RotateTurret(correctionAngle);
}

void CVehicleTurret::RotateTurret(float ByAngle)
{
	m_PivotAngle += ByAngle;

	// Rotate turret
	m_TurretBone.m_To -= m_Pivot;
	m_TurretBone.m_From -= m_Pivot;
	m_TurretBone.Rotate(ByAngle);
	m_TurretBone.m_To += m_Pivot;
	m_TurretBone.m_From += m_Pivot;

	for (int i = 0; i < m_NumBones; i++)
	{
		m_apBones[i].m_To -= m_Pivot;
		m_apBones[i].m_From -= m_Pivot;
		m_apBones[i].Rotate(ByAngle);
		m_apBones[i].m_To += m_Pivot;
		m_apBones[i].m_From += m_Pivot;
	}
}

void CVehicleTurret::FireTurret()
{
	if (!m_pHelicopter->GetOwner() || m_pHelicopter->GetOwner()->m_FreezeTime || !m_Shooting ||
		Server()->Tick() - m_LastShot <= m_ShootingCooldown)
		return;

	m_LastShot = Server()->Tick();

	vec2 startingPos = m_pHelicopter->GetPos() + m_TurretBone.m_From;
	vec2 Direction = GetTurretDirection();

	vec2 projectileDirection = normalize(Direction + vec2(-Direction.y, Direction.x));
	new CProjectile(GameWorld(),
		WEAPON_GUN,
		m_pHelicopter->GetOwner()->GetPlayer()->GetCID(),
		startingPos,
		projectileDirection,
		Server()->TickSpeed() * 2,
		false, false,
		0.f, -1);
}

vec2 CVehicleTurret::GetTurretDirection()
{
	return normalize(m_TurretBone.m_From - m_TurretBone.m_To);
}

CVehicleTurret::CVehicleTurret(int TurretType, int NumBones,
							   const SBone& TurretBone, const vec2& Pivot,
							   float AimingRange, int ShootingCooldown)
{
	m_TurretType = TurretType;
	m_pHelicopter = nullptr;
	m_apBones = new SBone[NumBones];
	m_NumBones = NumBones;

	// Unsure
	m_TurretBone = TurretBone;
	m_Pivot = Pivot;
	m_Length = m_TurretBone.GetLength();

	m_Flipped = false;
	m_Angle = 0.f;

	m_AimPosition = vec2(0.f, 0.f);
	m_PivotAngle = 0.f;
	m_AimingRange = AimingRange;

	m_Shooting = false;
	m_LastShot = 0;
	m_ShootingCooldown = ShootingCooldown;

	m_Scale = 1.f;
}

CVehicleTurret::~CVehicleTurret()
{
	if (m_pHelicopter) // if helicopter was never set, ids were never set either
		for (int i = 0; i < m_NumBones; i++)
			if (m_apBones[i].m_ID != -1)
				Server()->SnapFreeID(m_apBones[i].m_ID);
	delete[] m_apBones;
}

IServer *CVehicleTurret::Server()
{
	return m_pHelicopter->Server();
}

CGameWorld *CVehicleTurret::GameWorld()
{
	return m_pHelicopter->GameWorld();
}

CGameContext *CVehicleTurret::GameServer()
{
	return m_pHelicopter->GameServer();
}

bool CVehicleTurret::TryBindHelicopter(CHelicopter *helicopter)
{ // returns true on passed ownership, on failure clean up yourself
	if (helicopter == nullptr)
		return false;

	if (m_pHelicopter == nullptr) // really only assign owner once
	{
		m_pHelicopter = helicopter;
		for (int i = 0; i < m_NumBones; i++)
			m_apBones[i].AssignEntityAndID(m_pHelicopter, Server()->SnapNewID());

		// Match helicopter posture initially
		ApplyScale(m_pHelicopter->GetScale());
		SetFlipped(m_pHelicopter->IsFlipped());
		Rotate(m_pHelicopter->Angle());
		return true;
	}
	return false;
}

void CVehicleTurret::ApplyScale(float TurretScale)
{
	// Experimental
	m_Scale *= TurretScale;
	m_Length *= TurretScale;
	m_Pivot *= TurretScale;
	m_TurretBone.Scale(TurretScale);
	for (int i = 0; i < m_NumBones; i++)
		m_apBones[i].Scale(TurretScale);
	//
}

void CVehicleTurret::Tick()
{
//    if (!m_pHelicopter) don't even try to run this function without a heli
//        return;

	AimTurret();
	FireTurret();
}

void CVehicleTurret::Snap(int SnappingClient)
{
	if (!m_pHelicopter)
		return;

	for (int i = 0; i < m_NumBones; i++)
		m_apBones[i].Snap(SnappingClient);
}

void CVehicleTurret::OnInput(CNetObj_PlayerInput *pNewInput)
{
	if (!m_pHelicopter->GetOwner() || m_pHelicopter->GetOwner()->m_FreezeTime)
		return;

	m_AimPosition = vec2((float)pNewInput->m_TargetX, (float)pNewInput->m_TargetY);
	m_Shooting = pNewInput->m_Fire % 2 == 1;
}

//

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
	vec2 Direction = GetTurretDirection(); // gun direction, length: 1
	vec2 Perpendicular = vec2(-Direction.y, Direction.x); // direction perpendicular of the gun, length: 1

	for (int i = 0; i < NUM_BONES_CLUSTER; i++)
	{
		float currentBoneRotation = m_ClusterRotation + AngleDiff() * (float)i * (m_Flipped ? -1.f : 1.f);
		float verticalOffset = sinf(currentBoneRotation) * m_ClusterVerticalRange;
		float horizontalOffset = cosf(currentBoneRotation) * m_ClusterHorizontalRange;

		// This will move each bone in an elliptical motion, even if the gun is rotated it will look as intended
		vec2 verticalDirection = Perpendicular * verticalOffset;
		vec2 horizontalDirection = Direction * horizontalOffset;

		vec2 Combined = verticalDirection + horizontalDirection;
		vec2 barrelStartPos = m_TurretBone.m_To + Combined;
		vec2 barrelEndPos = m_TurretBone.m_From + Combined;

		// Start and end are flipped to have laserball on our side of the gun
		Cluster()[i].m_To = barrelStartPos;
		Cluster()[i].m_From = barrelEndPos;
	}
}

void CMinigunTurret::SpinCluster()
{
	if (m_pHelicopter->GetOwner() && !m_pHelicopter->GetOwner()->m_FreezeTime && m_Shooting)
		m_ClusterSpeed += (float)Server()->TickSpeed() / 5000.f;
	m_ClusterSpeed *= 0.98f;
	m_ClusterRotation += m_ClusterSpeed;

	UpdateClusterBones();
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
		Radius *= -1.f;
	}
}

void CMinigunTurret::SetFlipped(bool flipped)
{ // SetFlipped instead of Flip because if turret is attached later to match the helicopter flip, flip |ovo|
	if (flipped == m_Flipped)
		return;

	m_Flipped = !m_Flipped;
	m_Angle *= -1.f;

	m_PivotAngle *= -1.f;

	m_Pivot.x *= -1.f;
	m_TurretBone.Flip(); // Cluster bones update dynamically
	for (int i = 0; i < NUM_BONES_RETAINER; i++)
		Retainer()[i].Flip();
}

void CMinigunTurret::Rotate(float Angle)
{
	m_Angle += Angle;
	m_PivotAngle += Angle;
	rotate(m_Pivot, Angle);
	m_TurretBone.Rotate(Angle); // Cluster bones update dynamically
	for (int i = 0; i < NUM_BONES_RETAINER; i++)
		Retainer()[i].Rotate(Angle);
}

void CMinigunTurret::RotateTurret(float Angle)
{
	m_PivotAngle += Angle;

	// Rotate turret
	m_TurretBone.m_To -= m_Pivot;
	m_TurretBone.m_From -= m_Pivot;
	m_TurretBone.Rotate(Angle);
	m_TurretBone.m_To += m_Pivot;
	m_TurretBone.m_From += m_Pivot;

	// Rotate retainer
	for (int i = 0; i < NUM_BONES_RETAINER; i++)
	{
		Retainer()[i].m_To -= m_Pivot;
		Retainer()[i].m_From -= m_Pivot;
		Retainer()[i].Rotate(Angle);
		Retainer()[i].m_To += m_Pivot;
		Retainer()[i].m_From += m_Pivot;
	}
}

void CMinigunTurret::FireTurret()
{
	if (!m_pHelicopter->GetOwner() || m_pHelicopter->GetOwner()->m_FreezeTime || !m_Shooting ||
		Server()->Tick() - m_LastShot <= m_ShootingCooldown || m_ClusterSpeed < 0.3f)
		return;

	m_LastShot = Server()->Tick();

	vec2 startingPos = m_pHelicopter->GetPos() + Cluster()[m_ShootingBarrelIndex].m_From;
	vec2 Direction = GetTurretDirection();

	float spreadRandomness = (float)(random_int() % 21 - 10) / 400.f;
	vec2 projectileDirection = normalize(Direction + vec2(-Direction.y, Direction.x) * spreadRandomness);
	new CProjectile(GameWorld(),
		WEAPON_GUN,
		m_pHelicopter->GetOwner()->GetPlayer()->GetCID(),
		startingPos,
		projectileDirection,
		Server()->TickSpeed() * 2,
		false, true,
		0.f, SOUND_GRENADE_EXPLODE);
	GameServer()->CreateSound(startingPos, SOUND_GRENADE_FIRE, m_pHelicopter->GetOwner()->TeamMask());
//	GameServer()->CreateSound(startingPos, SOUND_GUN_FIRE, m_pHelicopter->GetOwner()->TeamMask());

	m_ShootingBarrelIndex = (m_ShootingBarrelIndex + 1) % NUM_BONES_CLUSTER;
	m_pHelicopter->SetVel(m_pHelicopter->GetVel() + -Direction * 0.5f);
}

CMinigunTurret::CMinigunTurret()
	: CVehicleTurret(TURRETTYPE_MINIGUN, NUM_BONES,
		SBone(nullptr, -1, vec2(70.f, 50.f), vec2(-34.f, 50.f)),
		vec2(4.f, 50.f), 35.f, 7)
{
	m_pHelicopter = nullptr;

	m_ClusterRotation = 0.f;
	m_ClusterSpeed = 0.f;
	m_ClusterVerticalRange = 8.f;
	m_ClusterHorizontalRange = 5.f;

	m_RetainerPosition = -14.f; // from the tip of the turret
	m_RetainerRadius = 9.f;

	m_ShootingBarrelIndex = 0;

	InitCluster();
	InitRetainer();
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
}

void CMinigunTurret::Snap(int SnappingClient)
{
	if (!m_pHelicopter)
		return;

	for (int i = 0; i < m_NumBones; i++)
		m_apBones[i].Snap(SnappingClient);
}

void CMinigunTurret::OnInput(CNetObj_PlayerInput *pNewInput)
{
	if (!m_pHelicopter->GetOwner() || m_pHelicopter->GetOwner()->m_FreezeTime)
		return;

	m_AimPosition = vec2((float)pNewInput->m_TargetX, (float)pNewInput->m_TargetY);
	m_Shooting = pNewInput->m_Fire % 2 == 1;
}

//

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
	if (!m_pHelicopter->GetOwner() || m_pHelicopter->GetOwner()->m_FreezeTime || !m_Shooting ||
		Server()->Tick() - m_LastShot <= m_ShootingCooldown)
		return;

	m_LastShot = Server()->Tick();

	vec2 startingPos = m_pHelicopter->GetPos() + m_TurretBone.m_From;
	vec2 Direction = GetTurretDirection();
	float missileStartingSpeed = 8.f;
	new CMissile(GameWorld(),
		m_pHelicopter->GetOwner()->GetPlayer()->GetCID(),
		startingPos,
		m_pHelicopter->GetVel() * 0.5f + Direction * missileStartingSpeed,
		Direction,
		Server()->TickSpeed() * 3);
	GameServer()->CreateSound(startingPos, SOUND_GRENADE_FIRE, m_pHelicopter->GetOwner()->TeamMask());
	GameServer()->CreateSound(startingPos, SOUND_GRENADE_EXPLODE, m_pHelicopter->GetOwner()->TeamMask());

	m_pHelicopter->SetVel(m_pHelicopter->GetVel() + -Direction * 3.f);
}

CLauncherTurret::CLauncherTurret()
	: CVehicleTurret(TURRETTYPE_LAUNCHER, NUM_BONES,
					 SBone(nullptr, -1, vec2(70.f, 50.f), vec2(-44.f, 50.f)),
					 vec2(-10.f, 50.f), 35.f, 50)
{
	m_pHelicopter = nullptr;

	m_ShaftRadius = 7.f;
	m_RetainerPosition = 16.f; // From the tip of the launcher
	m_RetainerRadius = 11.f;

	m_RecoilAmount = 20.f;
	m_RecoilSpan = 30.f; // Ticks until normal position
	m_CurrentRecoilFactor = 0.f;

	InitBones();
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
//    if (!m_pHelicopter) don't even try to run this function without a heli
//        return;
	AimTurret();
	FireTurret();

	HandleRecoil();
	UpdateBones();
}

void CLauncherTurret::Snap(int SnappingClient)
{
	if (!m_pHelicopter)
		return;

	for (int i = 0; i < m_NumBones; i++)
		m_apBones[i].Snap(SnappingClient);
}

void CLauncherTurret::OnInput(CNetObj_PlayerInput *pNewInput)
{
	if (!m_pHelicopter->GetOwner() || m_pHelicopter->GetOwner()->m_FreezeTime)
		return;

	m_AimPosition = vec2((float)pNewInput->m_TargetX, (float)pNewInput->m_TargetY);
	m_Shooting = pNewInput->m_Fire % 2 == 1;
}
