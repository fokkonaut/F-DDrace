// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_HELICOPTER_H
#define GAME_SERVER_ENTITIES_HELICOPTER_H

#include "advanced_entity.h"

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
		NUM_HELICOPTER_IDS = NUM_BONES + NUM_TRAILS
	};

	struct SBone
	{
		vec2 m_From;
		vec2 m_To;
		int m_ID;

		SBone() { *this = SBone(0, 0, 0, 0); }
		SBone(float FromX, float FromY, float ToX, float ToY) { *this = SBone(vec2(FromX, FromY), vec2(ToX, ToY)); }
		SBone(vec2 From, vec2 To)
		{
			m_From = From;
			m_To = To;
		}
		void Flip()
		{
			m_From.x *= -1;
			m_To.x *= -1;
		}
		void Rotate(float Angle)
		{
			m_From = rotate(m_From, Angle);
			m_To = rotate(m_To, Angle);
		}
	};

	struct STrail
	{
		vec2 *m_pPos;
		int m_ID;
	};

	void InitBody();
	void InitPropellers();
	void SpinPropellers();

	bool m_Flipped;
	void Flip();

	float m_Angle;
	void Rotate(float Angle);
	void SetAngle(float Angle);

	void ApplyAcceleration();
	vec2 m_Accel;
	
	STrail m_aTrails[NUM_TRAILS];
	SBone m_aBones[NUM_BONES];
	SBone *Body() { return &m_aBones[0]; } // size: NUM_BONES_BODY
	SBone *TopPropeller() { return &m_aBones[NUM_BONES_BODY]; } // size: NUM_BONES_PROPELLERS_TOP
	SBone *BackPropeller() { return &m_aBones[NUM_BONES_BODY+NUM_BONES_PROPELLERS_TOP]; } // size: NUM_BONES_PROPELLERS_BACK
	void SnapBone(SBone Bone);
	void SnapTrail(STrail Trail);

public:
	CHelicopter(CGameWorld *pGameWorld, vec2 Pos);
	virtual ~CHelicopter();

	virtual void Tick();
	virtual void Snap(int SnappingClient);
	virtual void Reset();

	bool Mount(int ClientID);
	void Dismount();
	void OnInput(CNetObj_PlayerInput *pNewInput);
};

#endif // GAME_SERVER_ENTITIES_HELICOPTER_H
