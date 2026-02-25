/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_PROJECTILE_H
#define GAME_SERVER_ENTITIES_PROJECTILE_H

class CProjectile : public CEntity
{
public:
	CProjectile
	(
		CGameWorld* pGameWorld,
		int Type,
		int Owner,
		vec2 Pos,
		vec2 Dir,
		int Span,
		bool Freeeze,
		bool Explosive,
		float Force,
		int SoundImpact,
		vec2 InitDir,
		int Layer = 0,
		int Number = 0,
		bool Spooky = false
	);

	vec2 GetPos(float Time);
	void FillInfo(CNetObj_Projectile *pProj);

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);

private:
	vec2 m_Direction;
	int m_LifeSpan;
	Mask128 m_TeamMask;
	int m_Owner;
	int m_Type;
	int m_SoundImpact;
	float m_Force;
	int m_StartTick;
	bool m_Explosive;

	// F-DDrace

	int m_Bouncing;
	bool m_Freeze;
	int m_TuneZone;
	float m_Curvature;
	float m_Speed;
	vec2 m_InitDir;

	bool m_Spooky;

	bool IsDefaultTuning();
	bool m_DefaultTuning;
	bool m_DDrace;
	bool m_IsSpreadWeapon;
	vec2 m_LastResetPos;
	int m_LastResetTick;
	bool m_CalculatedVel;
	vec2 m_Vel;

	virtual void TickDeferred();
	void CalculateVel();
	void GetOriginalTunings(float *pCurvature, float *pSpeed);
	void DetermineTuning();

public:

	void SetBouncing(int Value);
	bool FillExtraInfoLegacy(CNetObj_DDRaceProjectile *pProj, int SnappingClient);
	void FillExtraInfo(CNetObj_DDNetProjectile *pProj, int SnappingClient);
	vec2 m_CurPos;
};

#endif
