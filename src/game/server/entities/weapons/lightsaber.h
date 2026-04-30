// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_LIGHTSABER_H
#define GAME_SERVER_ENTITIES_LIGHTSABER_H

#include <game/server/entity.h>

#define SPEED 15
#define RETRACTED_LENGTH 0
#define EXTENDED_LENGTH 200

class CLightsaber : public CEntity
{
	int m_Length;
	vec2 m_To;
	bool m_Extending;
	bool m_Retracting;

	int m_Owner;
	CCharacter *m_pOwner;
	int m_EvalTick;
	int m_SoundTick;
	Mask128 m_TeamMask;

	int m_LastHit[MAX_CLIENTS];

	void PlaySound();
	bool HitCharacter();
	void Step();
public:
	CLightsaber(CGameWorld *pGameWorld, vec2 Pos, int Owner);

	void Extend();
	void Retract();
	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
};

#endif // GAME_SERVER_ENTITIES_LIGHTSABER_H
