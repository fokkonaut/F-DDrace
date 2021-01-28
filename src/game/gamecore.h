/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_GAMECORE_H
#define GAME_GAMECORE_H

#include <base/system.h>
#include <base/math.h>

#include <map>
#include <vector>

#include <math.h>
#include "collision.h"
#include <engine/shared/protocol.h>
#include <generated/protocol.h>

#include "teamscore.h"
#include "mapitems.h"

// F-DDrace
enum
{
	HOOK_FLAG_RED = MAX_CLIENTS,
	HOOK_FLAG_BLUE
};

#define UPDATE_ANGLE_TIME 20


class CTuneParam
{
	int m_Value;
public:
	void Set(int v) { m_Value = v; }
	int Get() const { return m_Value; }
	CTuneParam &operator = (int v) { m_Value = (int)(v*100.0f); return *this; }
	CTuneParam &operator = (float v) { m_Value = (int)(v*100.0f); return *this; }
	operator float() const { return m_Value/100.0f; }
};

class CTuningParams
{
public:
	CTuningParams()
	{
		const float TicksPerSecond = 50.0f;
		#define MACRO_TUNING_PARAM(Name,ScriptName,Value,Description) m_##Name.Set((int)(Value*100.0f));
		#include "tuning.h"
		#undef MACRO_TUNING_PARAM
	}

	static const char *ms_apNames[];

	#define MACRO_TUNING_PARAM(Name,ScriptName,Value,Description) CTuneParam m_##Name;
	#include "tuning.h"
	#undef MACRO_TUNING_PARAM

	static int Num() { return sizeof(CTuningParams)/sizeof(int); }
	bool Set(int Index, float Value);
	bool Set(const char *pName, float Value);
	bool Get(int Index, float *pValue) const;
	bool Get(const char *pName, float *pValue) const;
};

inline float GetAngle(vec2 Dir)
{
	if (Dir.x == 0 && Dir.y == 0)
		return 0.0f;
	float a = atanf(Dir.y / Dir.x);
	if (Dir.x < 0)
		a = a + pi;
	return a;
} 

inline void StrToInts(int *pInts, int Num, const char *pStr)
{
	int Index = 0;
	while(Num)
	{
		char aBuf[4] = {0,0,0,0};
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds" // false positive
#endif
		for(int c = 0; c < 4 && pStr[Index]; c++, Index++)
			aBuf[c] = pStr[Index];
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
		*pInts = ((aBuf[0]+128)<<24)|((aBuf[1]+128)<<16)|((aBuf[2]+128)<<8)|(aBuf[3]+128);
		pInts++;
		Num--;
	}

	// null terminate
	pInts[-1] &= 0xffffff00;
}

inline void IntsToStr(const int *pInts, int Num, char *pStr)
{
	while(Num)
	{
		pStr[0] = (((*pInts)>>24)&0xff)-128;
		pStr[1] = (((*pInts)>>16)&0xff)-128;
		pStr[2] = (((*pInts)>>8)&0xff)-128;
		pStr[3] = ((*pInts)&0xff)-128;
		pStr += 4;
		pInts++;
		Num--;
	}

	// null terminate
	pStr[-1] = 0;
}



inline vec2 CalcPos(vec2 Pos, vec2 Velocity, float Curvature, float Speed, float Time)
{
	vec2 n;
	Time *= Speed;
	n.x = Pos.x + Velocity.x*Time;
	n.y = Pos.y + Velocity.y*Time + Curvature/10000*(Time*Time);
	return n;
}


template<typename T>
inline T SaturatedAdd(T Min, T Max, T Current, T Modifier)
{
	if(Modifier < 0)
	{
		if(Current < Min)
			return Current;
		Current += Modifier;
		if(Current < Min)
			Current = Min;
		return Current;
	}
	else
	{
		if(Current > Max)
			return Current;
		Current += Modifier;
		if(Current > Max)
			Current = Max;
		return Current;
	}
}


float VelocityRamp(float Value, float Start, float Range, float Curvature);

// hooking stuff
enum
{
	HOOK_RETRACTED=-1,
	HOOK_IDLE=0,
	HOOK_RETRACT_START=1,
	HOOK_RETRACT_END=3,
	HOOK_FLYING,
	HOOK_GRABBED,
};

class CWorldCore
{
public:
	CWorldCore()
	{
		mem_zero(m_apCharacters, sizeof(m_apCharacters));
	}

	CTuningParams m_Tuning;
	class CCharacterCore *m_apCharacters[MAX_CLIENTS];
};

class CCharacterCore
{
	friend class CCharacter;
	CWorldCore *m_pWorld;
	CCollision *m_pCollision;
	std::map<int, std::vector<vec2> >* m_pTeleOuts;
public:
	static const float PHYS_SIZE;
	vec2 m_Pos;
	vec2 m_Vel;
	bool m_Hook;
	bool m_Collision;

	vec2 m_HookDragVel;

	vec2 m_HookPos;
	vec2 m_HookDir;
	vec2 m_HookTeleBase;
	int m_HookTick;
	int m_HookState;
	int m_HookedPlayer;

	bool m_NewHook;

	int m_Jumped;
	int m_JumpedTotal;
	int m_Jumps;

	int m_Direction;
	int m_Angle;

	CNetObj_PlayerInput m_Input;

	int m_TriggeredEvents;

	void Init(CWorldCore* pWorld, CCollision* pCollision, CTeamsCore* pTeams);
	void Init(CWorldCore* pWorld, CCollision* pCollision, CTeamsCore* pTeams, std::map<int, std::vector<vec2> >* pTeleOuts);
	void Reset();
	void Tick(bool UseInput);
	void Move();

	void AddDragVelocity();
	void ResetDragVelocity();

	void Read(const CNetObj_CharacterCore *pObjCore);
	void Write(CNetObj_CharacterCore *pObjCore) const;
	void Quantize();

	// F-DDrace

	void SetFlagInfo(int Team, vec2 Pos, int Stand, vec2 Vel, bool Carried);

	vec2 m_FlagPos[2];
	vec2 m_FlagVel[2];
	bool m_AtStand[2];
	bool m_Carried[2];

	int m_UpdateFlagAtStand;
	int m_UpdateFlagVel;
	vec2 m_UFlagVel;

	struct KillerInfo
	{
		int m_ClientID;
		int m_Weapon;
	} m_Killer;

	CCollision::MoveRestrictionExtra m_MoveRestrictionExtra;

	bool m_SpinBot;
	int m_SpinBotSpeed;
	float m_SpinBotAngle;
	bool m_AimClosest;
	vec2 m_AimClosestPos;
	int m_UpdateAngle;

	int m_Id;
	class CCollision* Collision() { return m_pCollision; }

	vec2 m_LastVel;
	int m_Colliding;
	bool m_LeftWall;

	// Caps the given velocity according to the current set of stoppers
	// that the character is affected by.
	vec2 LimitVel(vec2 Vel);
	void ApplyForce(vec2 Force);

private:

	CTeamsCore* m_pTeams;
	int m_MoveRestrictions;
	static bool IsSwitchActiveCb(int Number, void *pUser);
};

#endif
