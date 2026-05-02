//
// Created by Matq on 14/04/2026.
//

#ifndef GAME_SERVER_ENTITIES_MISC_BONE_VEHICLE_MODEL_H
#define GAME_SERVER_ENTITIES_MISC_BONE_VEHICLE_MODEL_H

#include "base/bone_model.h"

enum
{
	PROPELLER_HORIZONTAL,
	PROPELLER_CIRCULAR
};

enum
{
	SEATTYPE_DRIVER,
	SEATTYPE_PASSENGER,
};

struct SSeat
{
	vec2 m_InitSeat; // Only scaled
	vec2 m_Seat; // Rotated / flipped
	vec2 m_Interpolated; // Resulting smooth transition

	int m_SeatedCID; // -1 empty
	int m_Type;
	int m_AttachmentID; // Controls which components, like turrets

	struct SInputs
	{
		int m_WalkDirection;
		int m_Fire;
		int m_Hook;
		int m_Jump;
		int m_MouseX;
		int m_MouseY;

		// For example dismount while holding A/D, shooting or hooking
		int m_HeldWalkDirection;
		bool m_HeldFire;
		bool m_HeldHook;
		bool m_HeldJump;

		void Dismounted()
		{
			m_WalkDirection = 0;
			m_Fire = 0;
			m_Hook = 0;
			m_Jump = 0;
		}
	} m_Inputs;

public:
	SSeat() : SSeat(vec2(0, 0))
	{
	}
	SSeat(vec2 SeatPos, int SeatType = SEATTYPE_PASSENGER, int ControllingAttachmentIdx = -1)
	{
		m_InitSeat = SeatPos;
		m_Seat = SeatPos;
		m_Interpolated = SeatPos;

		m_SeatedCID = -1;
		m_Type = SeatType;
		memset(&m_Inputs, 0, sizeof(SInputs));
		m_AttachmentID = ControllingAttachmentIdx;
	}

	// Manipulating
	void Flip();
	void Rotate(float Angle)
	{
		m_Seat = rotate(m_Seat, Angle);
	}
	void Scale(float Scale)
	{
		m_InitSeat *= Scale;
		m_Seat *= Scale;
		m_Interpolated *= Scale;
	}
	void Dismounted()
	{
		m_SeatedCID = -1;
		m_Inputs.Dismounted();
	}
};

class IVehicle;
struct SPropeller
{
	int m_PropellerType;
	CBone *m_pBoneA;
	CBone *m_pBoneB;
	vec2 m_Pivot;
	float m_Radius;
	float m_Rotation;
	float m_Speed;

	vec2 m_LastA;
	vec2 m_LastB;

public:
	SPropeller() : SPropeller(PROPELLER_HORIZONTAL, nullptr, nullptr, vec2(0, 0), 25.0f, pi / 50)
	{
	}
	SPropeller(int PropellerType, CBone *pBoneA, CBone *pBoneB, vec2 Pivot, float Radius, float Speed)
	{
		m_PropellerType = PropellerType;
		m_pBoneA = pBoneA;
		m_pBoneB = pBoneB;
		m_Pivot = Pivot;
		m_Radius = Radius;
		m_Speed = Speed;

		m_Rotation = -Speed;
		m_LastA = vec2(0.0f, 0.0f);
		m_LastB = vec2(0.0f, 0.0f);

		if (pBoneA)
		{
			pBoneA->m_InitTo = Pivot;
			pBoneA->m_InitFrom = Pivot + vec2(-Radius, 0);
			pBoneA->LoadPositions();
		}
		if (pBoneB)
		{
			pBoneB->m_InitTo = Pivot;
			pBoneB->m_InitFrom = Pivot + vec2(Radius, 0);
			pBoneB->LoadPositions();
		}
	}

	// Getting
	int PropellerType() { return m_PropellerType; }
	vec2 GetCenter() { return m_pBoneA->m_To; }

	// Generating
	void GetHorizontalPositions(vec2& outPosA, vec2& outPosB)
	{
		vec2 bladeSpan = normalize(m_pBoneA->m_To - m_pBoneA->m_From) * m_Radius;
		outPosA = m_pBoneA->m_To + bladeSpan;
		outPosB = m_pBoneA->m_To - bladeSpan;
	}

	// Manipulating
	void ApplyScale(float Scale)
	{
		m_Radius *= Scale;
		m_Pivot *= Scale;
	}
	void ResetExtended()
	{
		vec2 oneDim = vec2(1, 0) * m_Radius;
		vec2 twoDim = normalize(m_pBoneA->m_From - m_pBoneA->m_To) * m_Radius;

		m_pBoneA->m_InitFrom = m_Pivot + oneDim;
		m_pBoneB->m_InitFrom = m_Pivot - oneDim;
		m_pBoneA->m_From = m_pBoneA->m_To + twoDim;
		m_pBoneB->m_From = m_pBoneB->m_To - twoDim;
	}
	void FlingTee(CCharacter *pChar, IVehicle *pVehicle);

	// Ticking
	void Tick()
	{
		m_Rotation += m_Speed;
		if (m_PropellerType == PROPELLER_HORIZONTAL)
		{
			float curLen = clamp(sinf(m_Rotation) * m_Radius, -m_Radius, m_Radius);
			// vec2 oneDim = vec2(curLen, 0);
			vec2 twoDim = normalize(m_pBoneA->m_From - m_pBoneA->m_To) * curLen;

			m_pBoneA->m_From = m_pBoneA->m_To + twoDim;
			m_pBoneB->m_From = m_pBoneA->m_To - twoDim;
		}
		if (m_PropellerType == PROPELLER_CIRCULAR)
		{
			vec2 CosSin = vec2(cosf(m_Rotation), sinf(m_Rotation)) * m_Radius;

			m_pBoneA->m_InitFrom = m_Pivot + CosSin;
			m_pBoneB->m_InitFrom = m_Pivot - CosSin;
			m_pBoneA->m_From = m_pBoneA->m_To + CosSin;
			m_pBoneB->m_From = m_pBoneB->m_To - CosSin;
		}
	}
	void UpdateLastPositionsHorizontal()
	{
		GetHorizontalPositions(m_LastA, m_LastB);
	}
};

struct SRope
{
public:
	struct SSegment // Fk instead
	{
		vec2 m_Pos;
		vec2 m_Vel;
	};

	CEntity *m_pEntity;
	vec2 m_ResetPos;
	vec2 m_Connection;
	float m_SegLength;
	SSegment *m_apSegments;
	CTrailNode *m_apTrails;
	int m_NumSegments;
	float m_Slack;

	float m_FullLength;

public:
	SRope() : SRope(nullptr, vec2(0, 0), 32.f, 10, 40.f)
	{
	}
	SRope(CEntity *pEntity, vec2 Connection, float SegmentLength, int NumSegments, float MaxSlack)
	{
		m_pEntity = pEntity;
		m_ResetPos = Connection;
		m_Connection = Connection;
		m_apSegments = NumSegments ? new SSegment[NumSegments] : nullptr;
		m_apTrails = NumSegments ? new CTrailNode[NumSegments] : nullptr;
		m_SegLength = SegmentLength;
		m_NumSegments = NumSegments;
		m_Slack = MaxSlack;

		m_FullLength = SegmentLength * (float)NumSegments;
	}
	void Destroy()
	{
		delete[] m_apSegments;
		delete[] m_apTrails;
	}

	// Getting
	vec2 ConnectionWorldPos() { return m_pEntity->GetPos() + m_Connection; }

	// Manipulating
	void InitRope();
	void ApplyScale(float Scale)
	{
		m_ResetPos *= Scale;
		m_Connection *= Scale;
	}

	// Ticking
	void Tick();
	void Snap(int SnappingClient);
};

class IVehicleModel : public IBoneModel
{
protected:
	SPropeller *m_apPropellers;
	int m_NumPropellers;
	void ApplyScalePropellers(float Scale)
	{
		for (int i = 0; i < m_NumPropellers; i++)
			m_apPropellers[i].ApplyScale(Scale);
	}

	SSeat *m_apSeats;
	int m_NumSeats;
	int m_NumSeated;
	void ApplyScaleSeats(float Scale)
	{
		for (int i = 0; i < m_NumSeats; i++)
			m_apSeats[i].Scale(Scale);
	}
	void SetSeatsRotation(float NewRotation)
	{
		for (int i = 0; i < m_NumSeats; i++)
			m_apSeats[i].m_Seat = rotate(m_apSeats[i].m_InitSeat, NewRotation);
	}

	SRope *m_apRopes;
	int m_NumRopes;
	void ApplyScaleRopes(float Scale)
	{
		for (int i = 0; i < m_NumRopes; i++)
			m_apRopes[i].ApplyScale(Scale);
	}

	virtual void InitBody() = 0;
	virtual void InitPropellers() = 0;
	virtual void InitSeats() = 0;
	virtual void InitRopes() = 0;

	void InitModel() override = 0;

public:
	IVehicleModel(CEntity *pEntity, int NumBones, int NumTrails, int NumPropellers, int NumSeats, int NumRopes)
		: IBoneModel(pEntity, NumBones, NumTrails)
	{
		m_apPropellers = NumPropellers ? new SPropeller[NumPropellers] : nullptr;
		m_NumPropellers = NumPropellers;
		m_apSeats = NumSeats ? new SSeat[NumSeats] : nullptr;
		m_NumSeats = NumSeats;
		m_NumSeated = 0;
		m_apRopes = NumRopes ? new SRope[NumRopes] : nullptr;
		m_NumRopes = NumRopes;
	}
	virtual ~IVehicleModel();

	// Getting
	SPropeller *Propellers() { return m_apPropellers; } // size: m_NumPropellers
	int NumPropellers() { return m_NumPropellers; }
	SSeat *Seats() { return m_apSeats; } // size: m_NumPropellers
	int NumSeats() { return m_NumSeats; }
	int NumSeated() { return m_NumSeated; }
	SRope *Ropes() { return m_apRopes; }
	int NumRopes() { return m_NumRopes; }

	// Manipulating
	void ApplyScale(float Scale) override
	{
		ApplyScaleBones(Scale);
		ApplyScalePropellers(Scale);
		ApplyScaleSeats(Scale);
		ApplyScaleRopes(Scale);
	}
	void SetRotation(float NewRotation) override
	{
		SetBonesRotation(NewRotation);
		SetSeatsRotation(NewRotation);
	}
	void UpdateLastPropellerPositions()
	{
		for (int i = 0; i < m_NumPropellers; i++)
		{
			if (m_apPropellers[i].PropellerType() == PROPELLER_CIRCULAR)
				continue;

			m_apPropellers[i].UpdateLastPositionsHorizontal();
		}
	}
	void ResetPropellers()
	{
		for (int i = 0; i < m_NumPropellers; i++)
		{
			if (m_apPropellers[i].PropellerType() == PROPELLER_CIRCULAR)
				continue;

			m_apPropellers[i].ResetExtended();
		}
	}
	void SetNumSeated(int NewNumSeated)
	{
		m_NumSeated = NewNumSeated;
	}

	// Ticking
	void SpinPropellers()
	{
		for (int i = 0; i < m_NumPropellers; i++)
			m_apPropellers[i].Tick();
	}
	void Snap(int SnappingClient, const SBoneModelSnapping& Options) override;
};

// Propeller function

bool MovingCircleHitsMovingSegment_Analytical(
	vec2 circleLast,
	vec2 circleNow,
	float radius,
	vec2 lineLastA,
	vec2 lineNowA,
	vec2 lineLastB,
	vec2 lineNowB
);

#endif // GAME_SERVER_ENTITIES_MISC_BONE_VEHICLE_MODEL_H
