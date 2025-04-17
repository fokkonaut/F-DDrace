/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_LASER_H
#define GAME_SERVER_ENTITIES_LASER_H

#include <game/server/entity.h>

class CLaser : public CEntity
{
public:
	CLaser(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, int Type, int TaserStrength = 0);

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);

protected:
	bool HitEntity(vec2 From, vec2 To);
	void DoBounce();

private:
	vec2 m_From;
	vec2 m_Dir;
	vec2 m_TelePos;
	bool m_WasTele;
	float m_Energy;
	int m_Bounces;
	int m_EvalTick;
	int m_Owner;
	Mask128 m_TeamMask;

	// DDRace

	int m_TaserStrength;

	vec2 m_PrevPos;
	int m_Type;
	bool m_TeleportCancelled;
	bool m_IsBlueTeleport;

	int m_TuneZone;
	float m_ShotgunStrength;
	float m_BounceNum;
	float m_BounceCost;
	float m_BounceDelay;
};

#endif
