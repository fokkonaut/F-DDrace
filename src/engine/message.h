/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_MESSAGE_H
#define ENGINE_MESSAGE_H

#include <engine/shared/packer.h>
#include <engine/shared/uuid_manager.h>

class CMsgPacker : public CPacker
{
public:
	CMsgPacker(int Type, bool System=false)
	{
		Reset();
		// NETMSG_EX, NETMSGTYPE_EX for UUID messages
		int NetType = Type < OFFSET_UUID ? Type : 0;
		AddInt((NetType<<1)|(System?1:0));
		if(Type >= OFFSET_UUID)
		{
			g_UuidManager.PackUuid(Type, this);
		}
	}
};

#endif
