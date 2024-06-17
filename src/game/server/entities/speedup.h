// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_SPEEDUP_H
#define GAME_SERVER_ENTITIES_SPEEDUP_H

#include <game/server/entity.h>

class CSpeedup : public CEntity
{
	enum
	{
		DOT_CENTER = 0,
		DOT_END_BOTTOM = 5,
		DOT_END_TOP = 6,
		NUM_DOTS = 7
	};

	struct SDot
	{
		int m_ID;
		vec2 m_Pos;
		void Rotate(int Angle)
		{
			m_Pos = rotate(m_Pos, Angle);
		}
	} m_aDots[NUM_DOTS];

	int m_Angle;
	int m_Force;
	int m_MaxSpeed;

	float m_Distance;
	float m_CurrentDist;

	void Rotate(int Angle);

public:
	CSpeedup(CGameWorld *pGameWorld, vec2 Pos, float Angle, int Force, int MaxSpeed, bool Collision = true);
	virtual ~CSpeedup();
	virtual void ResetCollision(bool Remove = false);
	virtual void Tick();
	virtual void Snap(int SnappingClient);

	void SetAngle(int Angle);
	int GetAngle() { return m_Angle; }
	int GetForce() { return m_Force; }
	int GetMaxSpeed() { return m_MaxSpeed; }
};

#endif // GAME_SERVER_ENTITIES_SPEEDUP_H
