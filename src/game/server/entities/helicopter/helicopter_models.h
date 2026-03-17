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

struct SPropeller
{
	int m_PropellerType;
	SBone *m_pBoneA;
	SBone *m_pBoneB;
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
	SPropeller(int PropellerType, SBone *pBoneA, SBone *pBoneB, vec2 Pivot, float Radius, float Speed)
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
			pBoneA->ResetPositions();
		}
		if (pBoneB)
		{
			pBoneB->m_InitTo = Pivot;
			pBoneB->m_InitFrom = Pivot + vec2(Radius, 0);
			pBoneB->ResetPositions();
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
	SPropeller *m_aPropellers;
	int m_NumPropellers;

	// float m_BackPropellerRadius;

	void ApplyScalePropellers(float Scale)
	{
		for (int i = 0; i < m_NumPropellers; i++)
			m_aPropellers[i].ApplyScale(Scale);
	}

	virtual void InitBody() = 0;
	virtual void InitPropellers() = 0;

public:
	IHelicopterModel(CEntity *pEntity, int NumBones, int NumTrails, int NumPropellers)
		: IBoneModel(pEntity, NumBones, NumTrails)
	{
		m_aPropellers = new SPropeller[NumPropellers];
		m_NumPropellers = NumPropellers;
	}
	virtual ~IHelicopterModel()
	{
		delete[] m_aPropellers;
	}

	// Getting
	SPropeller *Propellers() { return m_aPropellers; } // size: m_NumPropellers
	int NumPropellers() { return m_NumPropellers; }

	// Manipulating
	void ApplyScale(float Scale)
	{
		ApplyScaleBones(Scale);
		ApplyScalePropellers(Scale);
	}
	void UpdateLastPropellerPositions()
	{
		for (int i = 0; i < m_NumPropellers; i++)
		{
			if (m_aPropellers[i].PropellerType() == PROPELLER_CIRCULAR)
				continue;

			m_aPropellers[i].UpdateLastPositionsHorizontal();
		}
	}
	void ResetPropellers()
	{
		for (int i = 0; i < m_NumPropellers; i++)
		{
			if (m_aPropellers[i].PropellerType() == PROPELLER_CIRCULAR)
				continue;

			m_aPropellers[i].ResetExtended();
		}
	}
	void SpinPropellers()
	{
		for (int i = 0; i < m_NumPropellers; i++)
			m_aPropellers[i].Tick();
	}
};

class SHelicopterModel : public IHelicopterModel
{
private:
	enum
	{
		NUM_BONES_BODY = 14,
		NUM_BONES_PROPELLERS = 4,
		NUM_BONES = NUM_BONES_BODY + NUM_BONES_PROPELLERS,
		NUM_TRAILS = 2,
		NUM_PROPELLERS = 2,
	};

	void InitBody() override;
	void InitPropellers() override;
	void InitModel() override;

	SBone *Body() { return &m_aBones[0]; } // size: NUM_BONES_BODY
	SBone *Blades() { return &m_aBones[NUM_BONES_BODY]; } // size: NUM_BONES_PROPELLERS

public:
	SHelicopterModel(CEntity *pEntity);

	// Manipulating
	void ApplyScale(float Scale) override;
};

class SHelicopterApacheModel : public IHelicopterModel
{
private:
	enum
	{
		NUM_BONES_BODY = 14,
		NUM_BONES_PROPELLERS = 4,
		NUM_BONES = NUM_BONES_BODY + NUM_BONES_PROPELLERS,
		NUM_TRAILS = 2,
		NUM_PROPELLERS = 2,
	};

	void InitBody() override;
	void InitPropellers() override;
	void InitModel() override;

	SBone *Body() { return &m_aBones[0]; } // size: NUM_BONES_BODY
	SBone *Blades() { return &m_aBones[NUM_BONES_BODY]; } // size: NUM_BONES_PROPELLERS

public:
	SHelicopterApacheModel(CEntity *pEntity);

	// Manipulating
	void ApplyScale(float Scale) override;
};

#endif // GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_MODELS_H
