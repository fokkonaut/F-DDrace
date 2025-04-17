// made by fokkonaut
//  AND MATQ 2021

#ifndef GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_H
#define GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_H

#include "helicopter_turret.h"
#include "game/server/entities/advanced_entity.h"

class CHelicopter : public CAdvancedEntity
{
private:
	enum
	{
		NUM_BONES_BODY = 14,
		NUM_BONES_PROPELLERS_TOP = 2,
		NUM_BONES_PROPELLERS_BACK = 2,
		NUM_BONES_PROPELLERS = NUM_BONES_PROPELLERS_TOP + NUM_BONES_PROPELLERS_BACK,
		NUM_BONES = NUM_BONES_BODY + NUM_BONES_PROPELLERS,
		NUM_TRAILS = 2,
		NUM_HELICOPTER_IDS = NUM_BONES + NUM_TRAILS,
	};

	int m_InputDirection;
	float m_Health;
	bool m_EngineOn;

	float m_Scale;
	void InitBody();
	void InitPropellers();
	void SpinPropellers();
	void ResetAndTurnOff();

	bool m_Flipped;
	void Flip();

	float m_Angle;
	void Rotate(float Angle);
	void SetAngle(float Angle);

	void ApplyAcceleration();
	vec2 m_Accel;

	int m_aFlungCharacters[MAX_CLIENTS];
	void FlingTeesInPropellersPath();

	float m_BackPropellerRadius;
	float m_TopPropellerRadius;
	vec2 m_LastTopPropellerA, m_LastTopPropellerB;
	void GetFullPropellerPositions(vec2& outPosA, vec2& outPosB);

	STrail m_aTrails[NUM_TRAILS];
	SBone m_aBones[NUM_BONES];
	CVehicleTurret *m_pTurret;

	int m_ExplosionsLeft;
	void HandleExplosions();

	SBone *Body() { return &m_aBones[0]; } // size: NUM_BONES_BODY
	SBone *TopPropeller() { return &m_aBones[NUM_BONES_BODY]; } // size: NUM_BONES_PROPELLERS_TOP
	SBone *BackPropeller() { return &m_aBones[NUM_BONES_BODY + NUM_BONES_PROPELLERS_TOP]; } // size: NUM_BONES_PROPELLERS_BACK
	void SortBones();

public:
	CHelicopter(CGameWorld *pGameWorld, int Team, vec2 Pos, float Scale = 1.f);
	virtual ~CHelicopter();

	// Sense
	float GetScale() { return m_Scale; }
	bool IsFlipped() { return m_Flipped; }
	float Angle() { return m_Angle; }
	bool IsExploding() { return m_ExplosionsLeft > -1; }

	// Manipulating
	bool AttachTurret(CVehicleTurret *helicopterTurret);
	void DestroyTurret();
	void FlingTee(CCharacter *pChar);
	void ApplyScale(float HelicopterScale);
	void Explode();

	// Ticking
	void Tick() override;
	void Snap(int SnappingClient) override;
	void Reset() override;

	bool Mount(int ClientID);
	void Dismount();
	void OnInput(CNetObj_PlayerInput *pNewInput);
};

#endif // GAME_SERVER_ENTITIES_HELICOPTER_HELICOPTER_H
