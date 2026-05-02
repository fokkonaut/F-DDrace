// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_INTERACTIVE_GROG_H
#define GAME_SERVER_ENTITIES_INTERACTIVE_GROG_H

#include <game/server/entities/interactive/advanced_entity.h>

class CGrog : public CAdvancedEntity
{
	enum
	{
		GROG_LINE_LIQUID,
		GROG_LINE_HANDLE,
		GROG_LINE_BOTTOM,
		GROG_LINE_LEFT,
		GROG_LINE_RIGHT,
		NUM_GROG_LINES,

		NUM_GROG_SIPS = 5,
	};

	int m_Direction;
	int64 m_LastDirChange;
	bool m_ProcessedNudge;
	int m_NumSips;
	int m_Lifetime;

	struct SGrogLine
	{
		int m_ID;
		vec2 m_From;
		vec2 m_To;
	} m_aLines[NUM_GROG_LINES];

	int m_PickupDelay;
	void Pickup();
	void Reset(bool CreateDeath);
	void DecreaseNumGrogsHolding();

public:
	CGrog(CGameWorld *pGameWorld, vec2 Pos, int Owner);
	virtual ~CGrog();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
	virtual void Reset() { Reset(true); }

	void OnSip();
	bool Drop(float Dir = -3, bool OnDeath = false);

	// For snapping own hammerhit effect in silentfarm, public so eventhandler can reset it
	vec2 m_LastNudgePos;
};

#endif // GAME_SERVER_ENTITIES_INTERACTIVE_GROG_H
