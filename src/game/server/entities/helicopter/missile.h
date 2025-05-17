//
// Created by Matq on 14/04/2025.
//

#ifndef GAME_SERVER_ENTITIES_HELICOPTER_MISSILE_H
#define GAME_SERVER_ENTITIES_HELICOPTER_MISSILE_H

#include "../../gamecontext.h"
#include "../stable_projectile.h"

class CMissile;
class CSpark
{
	CMissile *m_pMissile;
	vec2 m_Pos;
	vec2 m_Vel;
	int m_ResetLifespan;
	int m_Lifespan;

	int m_ID;

public:
	CSpark(CMissile *pMissile, int ResetLifespan, int Lifespan);
	~CSpark();

	// Getting
	IServer *Server();
	bool IsStillVisible() { return m_Lifespan > 0; }

	// Manipulating
	void ResetFromMissile();

	// Ticking
	void Tick();
	void Snap(int SnappingClient);

};

class CMissile : public CEntity
{
	enum
	{
		NUM_SPARKS = 15,
	};

	int m_Owner;
	int m_DDTeam;
	Mask128 m_TeamMask;
	int m_LifeSpan;
	int m_StartTick;

	vec2 m_Vel;
	vec2 m_PrevPos;
	vec2 m_Direction;
	void ApplyAcceleration();
	void HandleCollisions();

	int m_IgnitionTime;

	CStableProjectile *m_pStableRocket;
	CSpark *m_apSparks[NUM_SPARKS];
	void UpdateStableProjectiles();

	int m_ExplosionsLeft;
	void TriggerExplosions();
	void HandleExplosions();

public:
	CMissile(CGameWorld *pGameWorld, int Owner, vec2 Pos, vec2 Vel, vec2 Direction, int Span);
	~CMissile();

	// Getting
	vec2 GetVel() { return m_Vel; }
	bool IsIgnited() { return m_IgnitionTime <= 0; }
	bool IsExploding() { return m_ExplosionsLeft > -1; }

	// Ticking
	void Tick() override;
	void Snap(int SnappingClient) override;

};

#endif // GAME_SERVER_ENTITIES_HELICOPTER_MISSILE_H
