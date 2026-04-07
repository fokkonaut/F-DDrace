/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_EVENTHANDLER_H
#define GAME_SERVER_EVENTHANDLER_H

#if !defined(_MSC_VER) || _MSC_VER >= 1600
#include <stdint.h>
#else
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#endif

#include "mask128.h"

//
class CEventHandler
{
	static const int MAX_EVENTS = 1024;
	static const int MAX_DATASIZE = 256*64;

	// Required for lowbandwidth players, so that they dont skip events
	static const int NUM_BUFFERS = 2;
	int m_CurrentBuffer;

	int m_aTypes[NUM_BUFFERS][MAX_EVENTS]; // TODO: remove some of these arrays
	int m_aOffsets[NUM_BUFFERS][MAX_EVENTS];
	int m_aSizes[NUM_BUFFERS][MAX_EVENTS];
	Mask128 m_aClientMasks[NUM_BUFFERS][MAX_EVENTS];
	char m_aData[NUM_BUFFERS][MAX_DATASIZE];

	int m_CurrentOffset[NUM_BUFFERS];
	int m_NumEvents[NUM_BUFFERS];

	void SnapBuffer(int SnappingClient, int Buf);

	class CGameContext *m_pGameServer;
public:
	CGameContext *GameServer() const { return m_pGameServer; }
	void SetGameServer(CGameContext *pGameServer);

	CEventHandler();
	void *Create(int Type, int Size, Mask128 Mask = Mask128());
	void Clear();
	void Snap(int SnappingClient);
};

#endif
