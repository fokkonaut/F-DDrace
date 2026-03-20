//
// Created by Matq on 12/05/2025.
//

#ifndef GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_MODELS_H
#define GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_MODELS_H

#include "helicopter_models.h"
#include "bone/bone_model.h"

enum
{
	PROPELLER_HORIZONTAL,
	PROPELLER_CIRCULAR
};

struct SSeat
{
	vec2 m_InitSeat; // Only scaled, reset pos
	vec2 m_Seat; // Rotated / flipped
	vec2 m_Interpolated; // Smooth transition

	int m_SeatedCID; // -1 empty

public:
	SSeat() : SSeat(vec2(0, 0))
	{
	}
	SSeat(vec2 SeatPos)
	{
		m_InitSeat = SeatPos;
		m_Seat = SeatPos;
		m_Interpolated = SeatPos;

		m_SeatedCID = -1;
	}

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
};

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

class IHelicopterModel : public IBoneModel
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

	virtual void InitBody() = 0;
	virtual void InitPropellers() = 0;
	virtual void InitSeats() = 0;

	void InitModel() override = 0;

public:
	IHelicopterModel(CEntity *pEntity, int NumBones, int NumTrails, int NumPropellers, int NumSeats)
		: IBoneModel(pEntity, NumBones, NumTrails)
	{
		m_apPropellers = NumPropellers ? new SPropeller[NumPropellers] : nullptr;
		m_NumPropellers = NumPropellers;
		m_apSeats = NumSeats ? new SSeat[NumSeats] : nullptr;
		m_NumSeats = NumSeats;
	}
	virtual ~IHelicopterModel()
	{
		delete[] m_apPropellers;
		delete[] m_apSeats;
	}

	// Getting
	SPropeller *Propellers() { return m_apPropellers; } // size: m_NumPropellers
	int NumPropellers() { return m_NumPropellers; }
	SSeat *Seats() { return m_apSeats; } // size: m_NumPropellers
	int NumSeats() { return m_NumSeats; }
	int NumSeated() { return m_NumSeated; }

	// Manipulating
	void ApplyScale(float Scale) override
	{
		ApplyScaleBones(Scale);
		ApplyScalePropellers(Scale);
		ApplyScaleSeats(Scale);
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
};

// Default helicopter model

class CHelicopterModel : public IHelicopterModel
{
private:
	enum
	{
		NUM_TRAILS = 2,
		NUM_PROPELLERS = 2,
		NUM_SEATS = 1,

		NUM_BONES_BODY = 14,
		NUM_BONES_PROPELLERS = NUM_PROPELLERS * 2,
		NUM_BONES = NUM_BONES_BODY + NUM_BONES_PROPELLERS,
	};

	void InitBody() override;
	void InitPropellers() override;
	void InitSeats() override;

	void InitModel() override;

	CBone *Body() { return &m_aBones[0]; } // size: NUM_BONES_BODY
	CBone *Blades() { return &m_aBones[NUM_BONES_BODY]; } // size: NUM_BONES_PROPELLERS

public:
	CHelicopterModel(CEntity *pEntity);
};

// Attack helicopter model

class CHelicopterApacheModel : public IHelicopterModel
{
private:
	enum
	{
		NUM_TRAILS = 2,
		NUM_PROPELLERS = 2,
		NUM_SEATS = 2,

		NUM_BONES_BODY = 14,
		NUM_BONES_PROPELLERS = NUM_PROPELLERS * 2,
		NUM_BONES = NUM_BONES_BODY + NUM_BONES_PROPELLERS,
	};

	void InitBody() override;
	void InitPropellers() override;
	void InitSeats() override;

	void InitModel() override;

	CBone *Body() { return &m_aBones[0]; } // size: NUM_BONES_BODY
	CBone *Blades() { return &m_aBones[NUM_BONES_BODY]; } // size: NUM_BONES_PROPELLERS

public:
	CHelicopterApacheModel(CEntity *pEntity);
};

// Transport helicopter model

class CHelicopterChinookModel : public IHelicopterModel
{
private:
	enum
	{
		NUM_PROPELLERS = 2,
		NUM_TRAILS = 0,
		NUM_SEATS = 4,

		NUM_BONES_BODY = 15,
		NUM_BONES_PROPELLERS = NUM_PROPELLERS * 2,
		NUM_BONES = NUM_BONES_BODY + NUM_BONES_PROPELLERS,
	};

	void InitBody() override;
	void InitPropellers() override;
	void InitSeats() override;

	void InitModel() override;

	CBone *Body() { return &m_aBones[0]; } // size: NUM_BONES_BODY
	CBone *Blades() { return &m_aBones[NUM_BONES_BODY]; } // size: NUM_BONES_PROPELLERS

public:
	CHelicopterChinookModel(CEntity *pEntity);
};

#endif // GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_MODELS_H
