//
// Created by Matq on 02/04/2026.
//

#ifndef GAME_SERVER_ENTITIES_BONE_SPIDER_MODEL_H
#define GAME_SERVER_ENTITIES_BONE_SPIDER_MODEL_H

#include "vehicle_model.h"
#include "game/server/entities/advanced_entity.h"

void SolveIK(vec2 *aPoints, float *aLengths, int N, vec2 Target, bool ClockwiseBend);
void SolveFK(vec2* aPoints, vec2* aVels, float* aLengths, int N);

class CLegIK
{
public:
	float m_Length;
	float m_Femur;
	float m_Tibia;
	vec2 m_InitOrigin;
	vec2 m_Origin;
	vec2 m_Joint;
	vec2 m_Toes;

	vec2 m_JointVel;
	vec2 m_ToesVel;

	// for updates
	CBone *m_pFemurBone;
	CBone *m_pTibiaBone;

	// for inverse kinematics
	CAdvancedEntity *m_pParent;
	vec2 m_IkTarget; // World space
	vec2 m_IkTargetSmooth; // World space
	bool m_ClockwiseBend;

public:
	CLegIK() : CLegIK(nullptr, 200.0f, 1.0f, 2.0f, true)
	{
	}
	CLegIK(CAdvancedEntity *pParent, float Length, float FemurRatio, float TibiaRatio, bool ClockwiseBend)
	{
		m_Length = Length;
		float RatioSum = FemurRatio + TibiaRatio;

		m_Femur = FemurRatio / RatioSum * Length;
		m_Tibia = TibiaRatio / RatioSum * Length;

		m_pFemurBone = nullptr;
		m_pTibiaBone = nullptr;

		m_pParent = pParent;
		m_IkTarget = vec2(0.f, 0.f);
		m_IkTargetSmooth = vec2(0.f, 0.f);
		m_ClockwiseBend = ClockwiseBend;
	}

	// Getting
	vec2 ParentPos() { return m_pParent ? m_pParent->GetPos() : vec2(0, 0); }
	vec2 GetRestingPosition() { return normalize(m_Origin) * m_Length; } // missing origin distance from center (looks nice tho)

	// Manipulating
	void SetIkToRestingPosition()
	{
		m_IkTargetSmooth = ParentPos() + GetRestingPosition();
	}
	void SetRestingIk()
	{
		m_IkTarget = ParentPos() + m_Toes;
		m_IkTargetSmooth = m_IkTarget;
	}
	void TickSmoothTarget(float Affect)
	{
		vec2 Diff = m_IkTargetSmooth - m_IkTarget;
		m_IkTarget += Diff * Affect;
	}
	void SolveIk()
	{
		vec2 RelativeTarget = m_IkTarget - ParentPos();
		vec2 aPoints[] = { m_Origin, m_Joint, m_Toes };
		float aLengths[] = { m_Femur, m_Tibia };
		SolveIK(aPoints, aLengths, 2, RelativeTarget, m_ClockwiseBend);

		m_Origin = aPoints[0];
		m_Joint = aPoints[1];
		m_Toes = aPoints[2];
	}
	void SolveFk()
	{
		vec2 aPoints[] = { m_Origin, m_Joint, m_Toes };
		vec2 aVels[] = { m_JointVel, m_ToesVel };
		float aLengths[] = { m_Femur, m_Tibia };
		SolveFK(aPoints, aVels, aLengths, 2);

		m_Origin = aPoints[0];
		m_Joint = aPoints[1];
		m_Toes = aPoints[2];

		m_JointVel = aVels[0];
		m_ToesVel = aVels[1];
	}
	void InitPositions(float Origin, vec2 Direction)
	{
		m_InitOrigin = Direction * Origin;
		m_Origin = m_InitOrigin;
		m_Joint = Direction * m_Femur;
		m_Toes = Direction * m_Length;
		SetRestingIk();
	}
	void InitBones(CBone *pFemurBone, CBone *pTibiaBone)
	{
		m_pFemurBone = pFemurBone;
		m_pTibiaBone = pTibiaBone;

		UpdateBones();
	}
	void UpdateBones()
	{
		m_pFemurBone->m_From = m_Origin;
		m_pFemurBone->m_To = m_Joint;
		m_pTibiaBone->m_From = m_Joint;
		m_pTibiaBone->m_To = m_Toes;
	}
	void ApplyScale(float Scale)
	{
		m_InitOrigin *= Scale;
		m_Origin *= Scale;
		m_Joint *= Scale;
		m_Toes *= Scale;

		m_Femur *= Scale;
		m_Tibia *= Scale;
		m_Length *= Scale;

		vec2 Parent = ParentPos();
		m_IkTarget = (Parent - m_IkTarget) * Scale + Parent;
		m_IkTargetSmooth = (Parent - m_IkTargetSmooth) * Scale + Parent;
	}
};


class CSpiderModel : public IVehicleModel
{
private:
	enum
	{
		NUM_LEGS = 8,
		NUM_LEGS_BONES = NUM_LEGS * 2,

		NUM_BONES = NUM_LEGS_BONES,
		NUM_TRAILS = 2,
		NUM_SEATS = 1,
		NUM_ROPES = 0,
	};

	void InitBody() override;
	void InitPropellers() override
	{
	}
	void InitSeats() override;
	void InitRopes() override
	{
	}
	void InitModel() override;

public:
	CSpiderModel(CEntity *pEntity);
};

#endif
