// made by fokkonaut

#ifndef GAME_SERVER_ENTITIES_DUMMY_HOUSE_H
#define GAME_SERVER_ENTITIES_DUMMY_HOUSE_H

#include "dummybase.h"

class CDummyHouse : public CDummyBase
{
public:
	CDummyHouse(CCharacter *pChr, int Mode);
	virtual ~CDummyHouse() {}
	virtual void OnTick();
};

#endif
