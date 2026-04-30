//
// Created by Matq on 02/04/2026.
//

#include "spider.h"
#include "game/server/gamecontext.h"

static float EightLegPhasing[8] = { 0.0f, 0.25f, 0.5f, 0.75f, 0.25f, 0.0f, 0.75f, 0.5f };
static IVehicleModel *CreateSpiderModel(IVehicle *pSpider) { return new CSpiderModel(pSpider); }

void CSpider::ApplyAcceleration()
{
	if (!m_EngineOn)
		m_Accel = vec2(0.f, 0.f);

	m_Vel.x += m_BaseAccel.x * m_Accel.x;
	m_Vel.y += m_BaseAccel.y * m_Accel.y;

	if (!IsFlags(EFlags::APPLY_GRAVITY))
		m_Vel *= 0.93f;
}

void CSpider::HandleRotation()
{
	float Diff = m_VisualAngle - m_Angle;
	while (Diff > pi) Diff -= 2 * pi;
	while (Diff < -pi) Diff += 2 * pi;

	float NewAngle = m_Angle + Diff * 0.25f;
	SetRotation(NewAngle);
}

void CSpider::HandleSeat(SSeat& Seat, int PassengerCID, CCharacter *pChar)
{
	// Movement controls
	if (Seat.m_Type == SEATTYPE_DRIVER)
	{
		m_Accel = vec2(0.f, 0.f);
		if (!pChar->m_FreezeTime)
		{
			SSeat::SInputs& Inputs = Seat.m_Inputs;;
			bool Drive = Inputs.m_Fire % 2 == 1;
			bool Boost = Inputs.m_Jump % 2 == 1;
			if (Drive && Inputs.m_MouseX && Inputs.m_MouseY)
			{
				vec2 Direction = normalize(vec2((float)Inputs.m_MouseX, (float)Inputs.m_MouseY));
				m_Accel = Direction * (Boost ? 2.0f : 1.0f);

				float targetAngle = atan2(Direction.y, Direction.x);
				m_VisualAngle = targetAngle;
			}
		}
	}

	IVehicle::HandleSeat(Seat, PassengerCID, pChar);
}

void CSpider::DriversDismounted()
{
	IVehicle::DriversDismounted();
	m_EngineOn = false;
}

void CSpider::DriversFrozen()
{
	IVehicle::DriversFrozen();
	m_Attacking = false;
}

void CSpider::SetRotation(float NewRotation)
{
	m_Angle = NewRotation;
	for (int i = 0; i < m_NumLegs; i++)
	{
		CLegIK& Leg = m_aLegs[i];
		Leg.m_Origin = rotate(Leg.m_InitOrigin, NewRotation / pi * 180.0f);
	}
}

void CSpider::TickLegs()
{
	// Prepare to reset to default positions when unfrozen
	if (m_DriverFreezeTime)
	{
		m_TravelDirection = vec2(0.0f, 0.0f);
		m_PhaseCompensation = 1.0f; // Move legs as if we're walking for once cycle
		for (int i = 0; i < m_NumLegs; i++)
			m_aLegs[i].SetIkToRestingPosition();
	}

	// Phase timer determines which group of legs will step next
	vec2 Diff = m_Pos - m_PrevPos;
	float Speed = length(Diff);
	m_PhaseTimer += Speed * (0.003f / m_Scale);
	if (m_PhaseCompensation > 0.0f) // Allows to move legs without moving entity
	{
		float TransferSpeed = 0.03f;
		float Transfer = minimum(m_PhaseCompensation, TransferSpeed);
		m_PhaseCompensation -= Transfer;
		m_PhaseTimer += Transfer;
	}

	if (Speed > 0.01f)
		m_TravelDirection = Diff / Speed;

	// Stepping algorithm
	float SmoothAffect = minimum(maximum(Speed / (30.0f * m_Scale), 0.2f), 1.0f);
	for (int i = 0; i < m_NumLegs; i++)
	{
		float Phase = fmodf(m_PhaseTimer + EightLegPhasing[i], 1.0f);
		bool GroupCanStep = Phase < 0.25f;

		CLegIK& Leg = m_aLegs[i];
		float LegLength = Leg.m_Length;

		// Calculate bias: front legs extend normally, back legs a little more
		vec2 LegDirection = normalize(Leg.m_Origin);
		float BackLegsBias = (dot(LegDirection, m_TravelDirection) - 1.0f) * -0.75f + 1.0f;

		// Brain that makes legs reach toward travel direction
		vec2 Compensation = m_TravelDirection * (LegLength * 0.32f * BackLegsBias);

		vec2 LegRestingPos = Leg.GetRestingPosition();
		vec2 NewIkTarget = m_Pos + LegRestingPos + Compensation;

		if (GroupCanStep && // Legs step in groups of 2, determined by the current phase
			(length(Leg.m_IkTargetSmooth - NewIkTarget) > LegLength * 0.8f || // Old target is too far from new one
				length(m_Pos + Leg.m_Toes - Leg.m_IkTargetSmooth) > 32.0f)) // or Toes position is too far from current target (avoid dragging effect)
		{
			// Legs grab onto walls if nearby
			vec2 NewIkTargetWall = NewIkTarget;
			GameServer()->Collision()->IntersectLine(m_Pos, NewIkTarget, &NewIkTargetWall, nullptr);
			Leg.m_IkTargetSmooth = NewIkTargetWall;
		}

		Leg.TickSmoothTarget(SmoothAffect);
		if (IsFlags(EFlags::APPLY_GRAVITY)) // experimental limp
		{
			// limp mode
			CTuningParams *pTuning = m_TuneZone ? &GameServer()->TuningList()[m_TuneZone] : GameServer()->Tuning();

			Leg.m_JointVel.y += pTuning->m_Gravity * 0.3f;
			Leg.m_ToesVel.y += pTuning->m_Gravity * 0.3f;

			Leg.m_JointVel -= m_Vel * 0.01f;
			Leg.m_ToesVel -= m_Vel * 0.01f;

			Leg.m_Joint += Leg.m_JointVel;
			Leg.m_Toes += Leg.m_ToesVel;

			Leg.SolveFk();
			Leg.SetRestingIk(); // Waking will cause the targets to shift to the real ones
		}
		else
		{
			// reach mode
			Leg.SolveIk();
		}
		Leg.UpdateBones();
	}
}

CSpider::CSpider(CGameWorld *pGameWorld, int Spawner, int Team, vec2 Pos, float Scale, int BuildTime, int Number)
	: IVehicle(pGameWorld, VEHICLETYPE_SPIDER, CGameWorld::ENTTYPE_SPIDER, Pos, SPIDER_PHYSSIZE, Spawner, Team, Number, BuildTime)
{
	SetFlags(EFlags::APPLY_GRAVITY, false);

	m_DDTeam = Team;
	m_Number = Number;

	m_NumLegs = 8;
	m_PhaseTimer = 0.0f;
	m_PhaseCompensation = 1.0f;
	m_TravelDirection = vec2(0.0f, 0.0f);

	m_Attacking = false;
	m_Attack = vec2(0.0f, 0.0f);

	SVehicleMeta SpiderMetadata = {
		"Spider", // m_pName
		50, // m_BaseHealth
		100, // m_BaseArmorHealth
		3, // m_NumHeartsIndicator
		5, // m_NumArmorIndicator
		true, // m_VerticalHealthbar
		8, // m_NumExplosions
		vec2(46, 46), // m_BaseSize
		vec2(0.4f, 0.4f), // m_BaseAccel
		CreateSpiderModel, // m_pModelFactory
	};
	SetVehicleMetadata(SpiderMetadata, true);

	float AnglePerLeg = pi * 2.0f / (float)m_NumLegs;
	float CurrentAngle = AnglePerLeg * 0.5f; // angle = 0 between front legs
	for (int i = 0; i < m_NumLegs; i++)
	{
		int BonesIndex = i * 2;
		vec2 CosSin = vec2(cos(CurrentAngle), sin(CurrentAngle));

		CBone& FemurBone = m_pModel->Bones()[BonesIndex + 1];
		CBone& TibiaBone = m_pModel->Bones()[BonesIndex];

		// Back legs idx 3 & 4 push away
		//  while others pull towards
		bool ClockwiseLeg = (i == 3 || i == 4) ^ (i > 3);

		CLegIK& Leg = m_aLegs[i];
		Leg = CLegIK(this, 200.0f, 4, 6, ClockwiseLeg);
		Leg.InitPositions(16.0f, CosSin);
		Leg.InitBones(&FemurBone, &TibiaBone);

		FemurBone.SavePositions();
		TibiaBone.SavePositions();

		CurrentAngle += AnglePerLeg;
	} // atp the model looks like a star

	m_pModel->UpdateAndCacheBounds();
	InitBuildAnimation();
	SortBones();

	Scale = clamp(Scale, 0.5f, 5.f);
	ApplyScale(SPIDER_DEFAULT_SCALE * Scale);

	GameWorld()->InsertEntity(this);
}

CSpider::~CSpider()
{
}

void CSpider::Reset()
{
	IVehicle::Reset();
}

void CSpider::ApplyScale(float VehicleScale)
{
	IVehicle::ApplyScale(VehicleScale);

	for (int i = 0; i < m_NumLegs; i++)
	{
		CLegIK& Leg = m_aLegs[i];
		Leg.ApplyScale(VehicleScale);
	}
}

void CSpider::Tick()
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
		else
		{
			SetFlags(EFlags::APPLY_GRAVITY, false);
			SetFlags(EFlags::APPLY_GROUND_VEL, false);
		}

		FlingTeesInPropellersPath();
		TickRopes();
		HandleHookedPassengers();
		ApplyAcceleration();
		HandleRotation();
		TickPropellers();

		DamageInWall();
		DamageInFreeze();

		m_HealthBar.UpdateIndicator();
		RegenerateArmor();
		TickVisualBoneDamage();

		TickAttachments();
		TickLegs();

		if (m_Health <= 0.f)
			Explode();

		SendBroadcastIndicator();
	}

	// Entity destroyed at m_ExplosionsLeft == 0
	HandleExplosions();

	m_PrevPos = m_Pos;
	if (m_DriverFreezeTime > 0)
		m_DriverFreezeTime--;
}

// void CSpider::Snap(int SnappingClient)
// {
//
// }

bool CSpider::OnInput(CNetObj_PlayerInput *pNewInput, CCharacter *pController)
{
	if (!IVehicle::OnInput(pNewInput, pController))
		return false;

	// int CharSeat = pController->m_VehicleSeat;
	// SSeat& Seat = m_pModel->Seats()[CharSeat];

	return true;
}
