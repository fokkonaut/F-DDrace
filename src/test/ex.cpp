#include <gtest/gtest.h>

#include <engine/message.h>
#include <engine/shared/protocol_ex.h>
#include <engine/shared/snapshot.h>
#include <generated/protocol.h>

static void TestMessage(bool WantedSys, int WantedID, const void *pData, int Size, CUnpacker *pUnpacker)
{
	pUnpacker->Reset(pData, Size);

	int ID;
	bool Sys;
	CUuid Uuid;
	CMsgPacker Answer(0);
	EXPECT_EQ(UnpackMessageID(&ID, &Sys, &Uuid, pUnpacker, &Answer, false), (int)UNPACKMESSAGE_OK);
	EXPECT_EQ(ID, WantedID);
	EXPECT_EQ(Sys, WantedSys);
}

TEST(Ex, MessageSys)
{
	CMsgPacker Packer(NETMSG_MYOWNMESSAGE, true);
	Packer.AddString("canary", 0);
	CUnpacker Unpacker;
	TestMessage(true, NETMSG_MYOWNMESSAGE, Packer.Data(), Packer.Size(), &Unpacker);
	EXPECT_STREQ(Unpacker.GetString(), "canary");
}

TEST(Ex, MessageGame)
{
	CNetMsg_Sv_MyOwnMessage Msg;
	Msg.m_Test = 1234567890;
	CMsgPacker Packer(NETMSGTYPE_SV_MYOWNMESSAGE);
	Msg.Pack(&Packer);

	CUnpacker Unpacker;
	CNetObjHandler Handler;
	TestMessage(false, NETMSGTYPE_SV_MYOWNMESSAGE, Packer.Data(), Packer.Size(), &Unpacker);
	CNetMsg_Sv_MyOwnMessage *pMsg = (CNetMsg_Sv_MyOwnMessage *)Handler.SecureUnpackMsg(NETMSGTYPE_SV_MYOWNMESSAGE, &Unpacker);
	ASSERT_NE(pMsg, nullptr);
	EXPECT_EQ(pMsg->m_Test, 1234567890);
}

TEST(Ex, SnapshotObject)
{
	CSnapshotBuilder Builder;
	Builder.Init();
	CNetObj_MyOwnObject *pObj = (CNetObj_MyOwnObject *)Builder.NewItem(NETOBJTYPE_MYOWNOBJECT, 0, sizeof(*pObj));
	CNetObj_MyOwnEvent *pEvent = (CNetObj_MyOwnEvent *)Builder.NewItem(NETOBJTYPE_MYOWNEVENT, 1, sizeof(*pEvent));
	ASSERT_NE(pObj, nullptr);
	ASSERT_NE(pEvent, nullptr);
	pObj->m_Test = 1234567890;
	pEvent->m_Test = 1357924680;

	unsigned char aData[CSnapshot::MAX_SIZE];
	Builder.Finish(aData);
	CSnapshot *pSnap = (CSnapshot *)aData;

	pSnap->DebugDump();

	int IndexObj = -1;
	int IndexEvent = -1;
	for(int i = 0; i < pSnap->NumItems(); i++)
	{
		int Type = pSnap->GetItemType(i);
		if(Type == NETOBJTYPE_MYOWNOBJECT)
		{
			EXPECT_EQ(IndexObj, -1);
			EXPECT_EQ(((const CNetObj_MyOwnObject *)pSnap->GetItem(i)->Data())->m_Test, 1234567890);
			IndexObj = i;
		}
		else if(Type == NETOBJTYPE_MYOWNEVENT)
		{
			EXPECT_EQ(IndexEvent, -1);
			EXPECT_EQ(((const CNetObj_MyOwnEvent *)pSnap->GetItem(i)->Data())->m_Test, 1357924680);
			IndexEvent = i;
		}
		else
		{
			pSnap->InvalidateItem(i);
		}
	}
	EXPECT_NE(IndexObj, -1);
	EXPECT_NE(IndexEvent, -1);

	EXPECT_EQ(pSnap->GetItemIndex(NETOBJTYPE_MYOWNOBJECT, 0), IndexObj);
	EXPECT_EQ(pSnap->GetItemIndex(NETOBJTYPE_MYOWNEVENT, 1), IndexEvent);
}

static void GetWhatIsAnswer(int Uuid, CMsgPacker *pPacker)
{
	CMsgPacker Packer(NETMSG_WHATIS, true);
	g_UuidManager.PackUuid(Uuid, &Packer);
	CUnpacker Unpacker;
	Unpacker.Reset(Packer.Data(), Packer.Size());
	int ID;
	bool Sys;
	CUuid TmpUuid;
	ASSERT_EQ(UnpackMessageID(&ID, &Sys, &TmpUuid, &Unpacker, pPacker, false), (int)UNPACKMESSAGE_ANSWER);
	EXPECT_EQ(Sys, true);
	EXPECT_EQ(ID, (int)NETMSG_WHATIS);
}

TEST(Ex, WhatIsKnown)
{
	CMsgPacker Buffer(0);
	CUnpacker Unpacker;
	GetWhatIsAnswer(NETMSG_MYOWNMESSAGE, &Buffer);
	TestMessage(true, NETMSG_ITIS, Buffer.Data(), Buffer.Size(), &Unpacker);
	CUuid Uuid;
	EXPECT_EQ(g_UuidManager.UnpackUuid(&Unpacker, &Uuid), (int)NETMSG_MYOWNMESSAGE);
	EXPECT_STREQ(Unpacker.GetString(CUnpacker::SANITIZE_CC), "system-message-my-own-message@heinrich5991.de");
	EXPECT_FALSE(Unpacker.Error());
}
