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

	vec2 m_CurPos;
	vec2 m_PrevPos;
	vec2 GetPos(float Time);
	void SetBouncing(int Value);

	void FillInfo(CNetObj_Projectile *pProj, int SnappingClient);
	void FillExtraInfo(CNetObj_DDNetProjectile *pProj, int SnappingClient);
	bool FillExtraInfoLegacy(CNetObj_DDRaceProjectile *pProj, int SnappingClient);

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

	bool DetermineIfDefaultTuning();
	bool m_DefaultTuning;
	bool m_DDrace;

	bool m_CalculatedVel;
	virtual void TickDeferred();
	void CalculateVel();
	void GetOriginalTunings(float *pCurvature, float *pSpeed);
	void DetermineTuning();

	enum
	{
		SNAPINFO_LOWBANDWIDTH,
		SNAPINFO_HIGHBANDWIDTH,
		NUM_SNAPINFO
	};

	struct SSnapInfo
	{
		vec2 m_LastResetPos;
		int m_LastResetTick;
		ivec2 m_Vel;
	} m_aSnap[NUM_SNAPINFO];
};

#endif
