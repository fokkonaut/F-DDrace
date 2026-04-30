//
// Created by Matq on 14/04/2026.
//

#include "vehicle_model.h"
#include <game/server/entities/interactive/vehicle/base/vehicle.h>
#include <game/server/gamecontext.h>

void SPropeller::FlingTee(CCharacter *pChar, IVehicle *pVehicle)
{
	if (!pChar->IsAlive() || !pVehicle)
		return;

	pChar->GameServer()->CreateSound(pChar->GetPos(), SOUND_PLAYER_PAIN_SHORT, pChar->TeamMask());
	pChar->GameServer()->CreateDeath(pChar->GetPos(), pChar->GetPlayer()->GetCID(), pChar->TeamMask());
	pChar->SetEmote(EMOTE_PAIN, pChar->Server()->Tick() + 500 * pChar->Server()->TickSpeed() / 1000);

	float helicopterVelocity = length(pVehicle->GetVel());
	float teeVelocity = length(pChar->GetCore().m_Vel);

	vec2 directionAwayFromBlades = normalize(pChar->m_PrevPos - pVehicle->GetPos());
	// Known at compile time
	constexpr float teeMass = 10.f;
	constexpr float vehicleMass = 18.f;
	constexpr float transferForceTee = vehicleMass / teeMass; // POOR THING :SKULL: 💀
	constexpr float transferForceVehicle = teeMass / vehicleMass;
	//
	float totalVelocity = clamp((helicopterVelocity + teeVelocity) * 0.75f, 5.f, 25.f);
	vec2 teeAcceleration = directionAwayFromBlades * transferForceTee * totalVelocity;
	vec2 vehicleAcceleration = -directionAwayFromBlades * transferForceVehicle * totalVelocity;

	CCharacter *pOwner = pVehicle->GetOwner();
	int OwnerCID = pOwner ? pOwner->GetPlayer()->GetCID() : 0;
	pChar->TakeDamage(pVehicle->GetVel() * maximum(0.001f, 0.f), pVehicle->GetVel() * -1, 1.f, OwnerCID, WEAPON_PLAYER);

	pChar->SetCoreVel(pChar->GetCore().m_Vel + teeAcceleration);
	pVehicle->SetVel(pVehicle->GetVel() + vehicleAcceleration);
}

void SRope::InitRope()
{
	if (!m_NumSegments || !m_pEntity)
		return;

	vec2 worldPos = ConnectionWorldPos();
	for (int i = 0; i < m_NumSegments; i++)
	{
		m_apSegments[i].m_Pos = worldPos;
		m_apSegments[i].m_Vel = vec2(0, 0);
		m_apTrails[i] = CTrailNode(m_pEntity, m_pEntity->Server()->SnapNewID(), &m_apSegments[i].m_Pos, false);
	}
}

void SRope::Tick()
{
	if (!m_NumSegments)
		return;

	vec2 prevPos = ConnectionWorldPos();
	for (int i = 0; i < m_NumSegments; i++)
	{
		SSegment& Segment = m_apSegments[i];
		vec2& segmentPos = Segment.m_Pos;
		vec2& segmentVel = Segment.m_Vel;

		int currentIndex = m_pEntity->GameServer()->Collision()->GetMapIndex(segmentPos);
		int tuneZone = m_pEntity->GameServer()->Collision()->IsTune(currentIndex);
		CTuningParams *pTuning = tuneZone ? &m_pEntity->GameServer()->TuningList()[tuneZone] : m_pEntity->GameServer()->Tuning();
		segmentVel.y += pTuning->m_Gravity;
		segmentPos += segmentVel;

		vec2 Diff = segmentPos - prevPos;
		float Distance = length(Diff);

		if (Distance > m_SegLength)
		{
			vec2 Dir = normalize(Diff);
			segmentPos = prevPos + Dir * m_SegLength;
			float directionAlike = dot(segmentVel, Dir);
			if (directionAlike > 0)
				segmentVel -= Dir * directionAlike * 0.1f;
		}

		prevPos = segmentPos;
	}
}

void SRope::Snap(int SnappingClient)
{
	for (int i = 0; i < m_NumSegments; i++)
	{
		if ((m_pEntity->Server()->Tick() + i) % m_NumSegments != 0)
			continue;

		m_apTrails[i].Snap(SnappingClient);
		break;
	}
}

IVehicleModel::~IVehicleModel()
{
	for (int i = 0; i < NumRopes(); i++)
	{
		SRope& Rope = Ropes()[i];
		for (int j = 0; j < Rope.m_NumSegments; j++)
		{
			CTrailNode& Trail = Rope.m_apTrails[j];
			if (Trail.m_ID == -1)
				continue;

			Server()->SnapFreeID(Trail.m_ID);
			Trail.m_ID = -1;
		}

		Rope.Destroy();
	}

	delete[] m_apPropellers;
	delete[] m_apSeats;
	delete[] m_apRopes;
}

void IVehicleModel::Snap(int SnappingClient, const SBoneModelSnapping& Options)
{
	IBoneModel::Snap(SnappingClient, Options);

	for (int i = 0; i < NumRopes(); i++)
		Ropes()[i].Snap(SnappingClient);
}

bool MovingCircleHitsMovingSegment_Analytical(
	vec2 circleLast,
	vec2 circleNow,
	float radius,
	vec2 lineLastA,
	vec2 lineNowA,
	vec2 lineLastB,
	vec2 lineNowB
)
{
	// Step 1: Relative motion
	vec2 circleVel = circleNow - circleLast;
	vec2 lineVelA = lineNowA - lineLastA;
	vec2 lineVelB = lineNowB - lineLastB;
	vec2 lineVel = (lineVelA + lineVelB) * 0.5f;
	vec2 relVel = circleVel - lineVel;

	vec2 C0 = circleLast;
	//	vec2 C1 = C0 + relVel;

	vec2 L0 = lineLastA;
	vec2 L1 = lineLastB;
	vec2 d = L1 - L0;
	vec2 m = C0 - L0;

	float a = dot(relVel, relVel);
	float b = dot(relVel, d);
	float c = dot(d, d);
	float d1 = dot(relVel, m);
	float e = dot(d, m);

	float denom = a * c - b * b;

	float t, s;

	if (denom != 0.0f)
	{
		t = (b * e - c * d1) / denom;
	}
	else
	{
		t = 0.0f; // Parallel
	}

	t = clamp(t, 0.0f, 1.0f);

	// Get point on circle path at time t
	vec2 circlePosAtT = C0 + relVel * t;

	// Find the closest point on segment
	float segmentLenSq = dot(d, d);
	if (segmentLenSq != 0.0f)
	{
		s = clamp(dot(circlePosAtT - L0, d) / segmentLenSq, 0.0f, 1.0f);
	}
	else
	{
		s = 0.0f;
	}

	vec2 closestOnSegment = L0 + d * s;

	vec2 diff = circlePosAtT - closestOnSegment;
	float distSq = dot(diff, diff);

	return distSq <= radius * radius;
}
