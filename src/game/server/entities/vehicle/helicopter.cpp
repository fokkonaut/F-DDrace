// made by fokkonaut and Matq, somewhere around 2021 and in 2025

#include "helicopter.h"
#include "game/server/entities/bone/helicopter_models.h"
#include "game/server/gamecontext.h"
#include "engine/server.h"

namespace Helicopters
{
static IVehicleModel *CreateDefaultModel(IVehicle *pHeli) { return new CHelicopterModel(pHeli); }
static IVehicleModel *CreateApacheModel(IVehicle *pHeli) { return new CHelicopterApacheModel(pHeli); }
static IVehicleModel *CreateChinookModel(IVehicle *pHeli) { return new CHelicopterChinookModel(pHeli); }

SVehicleMeta aHelicopterMetadata[NUM_HELICOPTER_TYPES] = {
	{
		"Helicopter", // m_pName
		20, // m_BaseHealth
		40, // m_BaseArmorHealth
		1, // m_NumHeartsIndicator
		3, // m_NumArmorIndicator
		false, // m_VerticalHealthbar
		6, // m_NumExplosions
		vec2(80, 128), // m_BaseSize
		vec2(0.75f, 0.6f), // m_BaseAccel
		CreateDefaultModel, // m_pModelFactory
	},
	{
		"Attack Helicopter", // m_pName
		30, // m_BaseHealth
		90, // m_BaseArmorHealth
		2, // m_NumHeartsIndicator
		4, // m_NumArmorIndicator
		false, // m_VerticalHealthbar
		12, // m_NumExplosions
		vec2(170, 128), // m_BaseSize
		vec2(1.0f, 1.0f), // m_BaseAccel
		CreateApacheModel, // m_pModelFactory
	},
	{
		"Transport Helicopter", // m_pName
		20, // m_BaseHealth
		60, // m_BaseArmorHealth
		1, // m_NumHeartsIndicator
		3, // m_NumArmorIndicator
		false, // m_VerticalHealthbar
		12, // m_NumExplosions
		vec2(230, 128), // m_BaseSize
		vec2(0.65f, 0.65f), // m_BaseAccel
		CreateChinookModel, // m_pModelFactory
	},
};
}

CHelicopter::CHelicopter(
	CGameWorld *pGameWorld,
	int HelicopterType,
	int Spawner,
	int Team,
	vec2 Pos,
	float HelicopterScale,
	int BuildTime,
	int Number,
	int DelayTurretType
)
	: IVehicle(pGameWorld, VEHICLETYPE_HELICOPTER, CGameWorld::ENTTYPE_HELICOPTER, Pos, HELICOPTER_PHYSSIZE, Spawner, Team, Number, BuildTime, 1)
{
	m_Elasticity = vec2(0.f, 0.f);
	m_DDTeam = Team;

	m_HelicopterType = clamp(HelicopterType, 0, (int)NUM_HELICOPTER_TYPES - 1);
	m_Strafing = false;
	m_SwitchDelay = DelayTurretType;

	if (PlacedByTile())
	{
		m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * Config()->m_SvHeliRespawnTime;
		m_Layer = LAYER_SWITCH; // unused rn, but for completeness
	}


	if (HelicopterType >= 0 && HelicopterType < NUM_HELICOPTER_TYPES)
		SetVehicleMetadata(Helicopters::aHelicopterMetadata[HelicopterType], true);

	// Order matters
	InitBuildAnimation();
	SortBones();

	HelicopterScale = clamp(HelicopterScale, 0.8f, 5.f);
	ApplyScale(HELICOPTER_DEFAULT_SCALE * HelicopterScale);

	GameWorld()->InsertEntity(this);
}

CHelicopter::~CHelicopter()
{

}

void CHelicopter::Reset()
{
	IVehicle::Reset();
	TryRespawnNewVehicle();
}

void CHelicopter::Tick()
{
	IVehicle::Tick();

	if (!IsInvincible())
	{
		HandleDropped();

		HandleSeats();
		if (m_DriverFreezeTime > 0)
		{
			DriversFrozen();
		}
		else if (GetDriver())
		{
			SetFlags(EFlags::APPLY_GRAVITY, false);
			SetFlags(EFlags::APPLY_GROUND_VEL, IsGrounded()); //
		}

		FlingTeesInPropellersPath();
		TickRopes();
		HandleHookedPassengers();
		ApplyAcceleration();
		HandleFlipping();
		HandleRotationBasedOnVelocity();
		TickPropellers();

		if (m_EngineOn && !GetDriver() && IsGrounded()) // Reset top propellers when grounded without driver
			ResetAndTurnOff();

		DamageInWall();
		DamageInFreeze();

		RegenerateArmor();
		m_HealthBar.UpdateIndicator();
		TickVisualBoneDamage();

		TickAttachments();

		if (m_Health <= 0.f)
			Explode();

		SendBroadcastIndicator();
	}

	// Entity destroyed at m_ExplosionsLeft == 0
	HandleExplosions();

	m_PrevPos = m_Pos;
	m_pModel->UpdateLastPropellerPositions();
}

bool CHelicopter::OnInput(CNetObj_PlayerInput *pNewInput, CCharacter *pControllerChar)
{
	if (IVehicle::OnInput(pNewInput, pControllerChar))
	{
		int CharSeat = pControllerChar->m_VehicleSeat;
		SSeat& Seat = m_pModel->Seats()[CharSeat];

		// Movement controls
		if (Seat.m_Type == SEATTYPE_DRIVER)
		{
			if (!pControllerChar->m_FreezeTime)
			{
				m_Strafing = pNewInput->m_Fire % 2 == 0;
				m_InputDirection = pNewInput->m_Direction;
				m_Accel.x = (float)pNewInput->m_Direction;

				bool Rise = pNewInput->m_Jump;
				bool Sink = pNewInput->m_Hook;
				if (Rise == Sink)
					m_Accel.y = 0.f;
				else
					m_Accel.y = Rise ? -1 : 1;
			}
			else
			{
				m_Accel = vec2(0.f, 0.f);
			}
		}

		// Weapon controls
		if (Seat.m_AttachmentID >= 0 && Seat.m_AttachmentID < m_NumAttachments && m_apAttachments[Seat.m_AttachmentID])
			m_apAttachments[Seat.m_AttachmentID]->OnInput(pNewInput, pControllerChar);
		// basically how this works is each seat can have an attachment_id, that is how the seat controls a component on the vehicle - like a gun
		return true;
	}

	return false;
}

void CHelicopter::ApplyAcceleration()
{
	if (!m_EngineOn)
		m_Accel = vec2(0.f, 0.f);

	float strafeFactor = (m_Flipped == (m_Vel.x > 0.f)) ? 0.4f : 1.f; // Accelerate slower when moving backwards
	m_Vel.x += m_BaseAccel.x * m_Accel.x * strafeFactor;
	m_Vel.y += m_BaseAccel.y * m_Accel.y;
	m_Vel.y *= 0.95f;
}

void CHelicopter::HandleFlipping()
{
	if (((m_InputDirection == -1 && !m_Flipped && m_Vel.x < 0.f) ||
		(m_InputDirection == 1 && m_Flipped && m_Vel.x > 0.f)) &&
		m_Strafing)
		m_Flipped = !m_Flipped;
}

void CHelicopter::DriversDismounted()
{
	IVehicle::DriversDismounted();
	SetFlags(EFlags::APPLY_GRAVITY, true);
	m_Strafing = false;
	m_Accel.y = 0.0f;
}

void CHelicopter::DriversFrozen()
{
	IVehicle::DriversFrozen();
	m_Accel.y = 0.0f;
}

bool CHelicopter::TryRespawnNewVehicle()
{
	if (!PlacedByTile())
		return false;

	return GameServer()->SpawnHelicopter(-1, 0, m_InitialPosition, m_HelicopterType, m_SwitchDelay, 1.f, false, m_Number);
}

void CHelicopter::HandleRotationBasedOnVelocity()
{
	float targetAngle = (m_Vel.x != 0.0f) ? m_Vel.x / (abs(m_Vel.x) + 60) * 90 : 0.0f;
	m_VisualAngle += (targetAngle - m_VisualAngle) * 0.1f;
	SetRotation(m_VisualAngle);
}

void CHelicopter::Snap(int SnappingClient)
{
	if (IsExploding() || IsSpawning())
		return;

	if (NetworkClipped(SnappingClient) || !CmaskIsSet(m_TeamMask, SnappingClient))
		return;

	SnapBuildingParticles(SnappingClient);
	SnapHealthBar(SnappingClient);

	// Draw helicopter
	CCharacter *pDriver = GetDriver();
	SBoneModelSnapping Options = {
		m_EngineOn, // m_SendTrails
		m_Flipped, // m_Flipped
		floor((1.0f - m_Health / m_MaxHealth) * 10), // m_VertexSnapping
		pDriver && (pDriver->m_Rainbow || pDriver->m_IsRainbowHooked) // m_RainbowMode
	};
	SnapModel(SnappingClient, Options);

	// Guns we don't flip.. for now
	Options.m_Flipped = false;

	// Draw guns
	SnapAttachments(SnappingClient, Options);
}

void CHelicopter::ResetAndTurnOff()
{
	// Dismounted & grounded
	m_EngineOn = false;

	m_pModel->ResetPropellers();
}
