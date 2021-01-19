// made by ChillerDragon

#ifndef GAME_SERVER_ENTITIES_DUMMY_CHILLBLOCK5_POLICE_H
#define GAME_SERVER_ENTITIES_DUMMY_CHILLBLOCK5_POLICE_H

#include "dummybase.h"

class CDummyChillBlock5Police : public CDummyBase
{
public:
	CDummyChillBlock5Police(CCharacter *pChr);
	virtual ~CDummyChillBlock5Police() {}
	virtual void OnTick();

private:
	bool m_GetSpeed;
	bool m_GotStuck;
	int m_PoliceMode;
	int m_AttackMode;
};

#endif
