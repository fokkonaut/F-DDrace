// made by ChillerDragon, improved by fokkonaut

#ifndef GAME_SERVER_ENTITIES_DUMMY_V3_BLOCKER_H
#define GAME_SERVER_ENTITIES_DUMMY_V3_BLOCKER_H

#include "dummybase.h"

class CDummyV3Blocker : public CDummyBase
{
public:
	CDummyV3Blocker(CCharacter *pChr);
	virtual ~CDummyV3Blocker() {}
	virtual void OnTick();
};

#endif
