//
// Created by Matq on 02/04/2026.
//

#include "spider_model.h"
#include "game/server/gamecontext.h"

// Reaching for stuff
void SolveIK(vec2* aPoints, float* aLengths, int N, vec2 Target, bool ClockwiseBend)
{
	// Calculate total length
	float totalLength = 0;
	for(int i = 0; i < N; i++)
		totalLength += aLengths[i];

	vec2 root = aPoints[0];
	float dx = Target.x - root.x;
	float dy = Target.y - root.y;
	float d = sqrt(dx*dx + dy*dy);

	if(d >= totalLength)
	{
		// Out of range
		float dirX = dx / d;
		float dirY = dy / d;

		for(int i = 1; i <= N; i++)
		{
			float len = 0;
			for(int j = 0; j < i; j++)
				len += aLengths[j];

			aPoints[i].x = root.x + dirX * len;
			aPoints[i].y = root.y + dirY * len;
		}
	}
	else
	{
		// In range
		for(int i = 0; i < N - 1; i++)
		{
			float L1 = aLengths[i];
			float L2 = aLengths[i + 1];

			dx = Target.x - aPoints[i].x;
			dy = Target.y - aPoints[i].y;
			d = sqrt(dx*dx + dy*dy);

			d = clamp(d, abs(L1 - L2), L1 + L2);

			float baseAngle = atan2(dy, dx);
			float cosA = (d*d + L1*L1 - L2*L2) / (2 * d * L1);
			float elbowOffset = acos(clamp(cosA, -1.0f, 1.0f));
			float angle1 = baseAngle + (ClockwiseBend ? -elbowOffset : +elbowOffset);

			aPoints[i + 1].x = aPoints[i].x + cos(angle1) * L1;
			aPoints[i + 1].y = aPoints[i].y + sin(angle1) * L1;
		}

		// Last point snaps to target
		aPoints[N] = Target;
	}
}

// Just keeping legs intact
void SolveFK(vec2* aPoints, vec2* aVels, float* aLengths, int N)
{
	for(int i = 0; i < N; i++)
	{
		vec2 diff = aPoints[i + 1] - aPoints[i];
		float d = length(diff);
		if(d < 0.0001f) continue;

		vec2 corrected = aPoints[i] + diff * (aLengths[i] / d);
		vec2 correction = corrected - aPoints[i + 1];

		aPoints[i + 1] = corrected;
		aVels[i] += correction; // velocity reflects the constraint correction
	}
}

void CSpiderModel::InitBody()
{
	for (int i = 0; i < NUM_BONES; i++)
	{
		int Thickness = (i % 2 == 0) ? 4 : 2; // Inner legs more thinner
		Bones()[i] = CBone(Entity(), Server()->SnapNewID(), vec2(), vec2(), Thickness, LASERTYPE_FREEZE);
	}

	for (int i = 0; i < NUM_TRAILS; i++)
		Trails()[i] = CTrailNode(Entity(), Server()->SnapNewID(), &Bones()[i].m_To, true, false);
}

void CSpiderModel::InitSeats()
{
	SSeat aSeats[NUM_SEATS] = {
		SSeat(vec2(0, 0), SEATTYPE_DRIVER, 0)
	};
	memcpy(Seats(), aSeats, sizeof(SSeat) * NUM_SEATS);
}

void CSpiderModel::InitModel()
{
	InitBody();
	InitSeats();
}

CSpiderModel::CSpiderModel(CEntity *pEntity)
	: IVehicleModel(pEntity, NUM_BONES, NUM_TRAILS, 0, NUM_SEATS, 0)
{
}
