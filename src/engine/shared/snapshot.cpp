/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/tl/base.h>
#include <base/tl/algorithm.h>
#include "snapshot.h"
#include "compression.h"
#include "uuid_manager.h"

#include <generated/protocol.h>

static int ObjTypeToSevendown(int Type)
{
	int Seven;
	if(Type >= NETOBJTYPE_PLAYERINPUT && Type <= NETOBJTYPE_FLAG)
		Seven = Type;
	else if (Type == NETOBJTYPE_GAMEDATA) // NETOBJTYPE_GAMEINFO in 0.6
		Seven = Type;
	else if(Type >= NETOBJTYPE_CHARACTERCORE && Type <= NETOBJTYPE_PLAYERINFO)
		Seven = Type - 1;
	else if (Type == NETOBJTYPE_SPECTATORINFO)
		Seven = Type;
	else if(Type >= NETEVENTTYPE_COMMON && Type <= NETEVENTTYPE_DEATH)
		Seven = Type - 3;
	else if(Type == NETEVENTTYPE_SOUNDWORLD)
		Seven = Type - 2;
	else if(Type > 24)
		Seven = Type - 24;
	else
		return -1;
	return Seven;
}

// CSnapshot

const CSnapshotItem *CSnapshot::GetItem(int Index) const
{
	return (const CSnapshotItem *)(DataStart() + Offsets()[Index]);
}

int CSnapshot::GetItemSize(int Index) const
{
	if(Index == m_NumItems-1)
		return (m_DataSize - Offsets()[Index]) - sizeof(CSnapshotItem);
	return (Offsets()[Index+1] - Offsets()[Index]) - sizeof(CSnapshotItem);
}

int CSnapshot::GetItemIndex(int Type, int ID) const
{
	int InternalType = -1;
	if(Type < OFFSET_UUID)
	{
		InternalType = Type;
	}
	else
	{
		CUuid Uuid = g_UuidManager.GetUuid(Type);
		int aUuid[sizeof(CUuid) / 4];
		for(int i = 0; i < (int)sizeof(CUuid) / 4; i++)
		{
			aUuid[i] =
				(Uuid.m_aData[i * 4 + 0] << 24) |
				(Uuid.m_aData[i * 4 + 1] << 16) |
				(Uuid.m_aData[i * 4 + 2] << 8) |
				(Uuid.m_aData[i * 4 + 3]);
		}
		for(int i = 0; i < m_NumItems && SortedKeys()[i] < ((1<<16)|0); i++)
		{
			if(GetItemSize(i) >= (int)sizeof(aUuid) &&
				mem_comp(aUuid, GetItem(i)->Data(), sizeof(aUuid)) == 0)
			{
				InternalType = SortedKeys()[i]&0xffff;
				break;
			}
		}
	}
	if(InternalType == -1)
	{
		return -1;
	}

	int Key = (InternalType<<16)|ID;
	plain_range_sorted<int> Keys(SortedKeys(), SortedKeys() + m_NumItems);
	plain_range_sorted<int> r = ::find_binary(Keys, Key);

	if(r.empty())
		return -1;

	int Index = &r.front() - SortedKeys();
	if(GetItem(Index)->Key() == -1)
		return -1; // deleted
	return Index;
}

int CSnapshot::GetItemType(int Index) const
{
	int InternalType = GetItem(Index)->Type();
	if(InternalType < OFFSET_UUID_TYPE)
	{
		return InternalType;
	}

	int TypeItemIndex = GetItemIndex(0, InternalType); // NETOBJTYPE_EX
	if(TypeItemIndex == -1 || GetItemSize(TypeItemIndex) < (int)sizeof(CUuid))
	{
		return InternalType;
	}

	const CSnapshotItem *pTypeItem = GetItem(TypeItemIndex);
	CUuid Uuid;
	for(int i = 0; i < (int)sizeof(CUuid) / 4; i++)
	{
		Uuid.m_aData[i * 4 + 0] = pTypeItem->Data()[i] >> 24;
		Uuid.m_aData[i * 4 + 1] = pTypeItem->Data()[i] >> 16;
		Uuid.m_aData[i * 4 + 2] = pTypeItem->Data()[i] >> 8;
		Uuid.m_aData[i * 4 + 3] = pTypeItem->Data()[i];
	}

	return g_UuidManager.LookupUuid(Uuid);
}

void CSnapshot::InvalidateItem(int Index)
{
	CSnapshotItem *pItem = (CSnapshotItem *)(DataStart() + Offsets()[Index]);
	if(pItem->Type() != 0) // NETOBJTYPE_EX
	{
		pItem->Invalidate();
	}
}

int CSnapshot::Serialize(char *pDstData)
{
	int *pData = (int*)pDstData;
	pData[0] = m_DataSize;
	pData[1] = m_NumItems;

	mem_copy(pData+2, Offsets(), sizeof(int)*m_NumItems);
	mem_copy(pData+2+m_NumItems, DataStart(), m_DataSize);

	return sizeof(int) * (2 + m_NumItems) + m_DataSize;
}

int CSnapshot::Crc() const
{
	int Crc = 0;

	for(int i = 0; i < m_NumItems; i++)
	{
		const CSnapshotItem *pItem = GetItem(i);
		int Size = GetItemSize(i);

		for(int b = 0; b < Size/4; b++)
			Crc += pItem->Data()[b];
	}
	return Crc;
}

void CSnapshot::DebugDump() const
{
	dbg_msg("snapshot", "data_size=%d num_items=%d", m_DataSize, m_NumItems);
	for(int i = 0; i < m_NumItems; i++)
	{
		const CSnapshotItem *pItem = GetItem(i);
		int Size = GetItemSize(i);
		dbg_msg("snapshot", "\ttype=%d id=%d", pItem->Type(), pItem->ID());
		for(int b = 0; b < Size/4; b++)
			dbg_msg("snapshot", "\t\t%3d %12d\t%08x", b, pItem->Data()[b], pItem->Data()[b]);
	}
}


// CSnapshotDelta

struct CItemList
{
	int m_Num;
	int m_aKeys[64];
	int m_aIndex[64];
};

enum
{
	HASHLIST_SIZE = 256,
};

static void GenerateHash(CItemList *pHashlist, const CSnapshot *pSnapshot)
{
	for(int i = 0; i < HASHLIST_SIZE; i++)
		pHashlist[i].m_Num = 0;

	for(int i = 0; i < pSnapshot->NumItems(); i++)
	{
		int Key = pSnapshot->GetItem(i)->Key();
		int HashID = ((Key>>12)&0xf0) | (Key&0xf);
		if(pHashlist[HashID].m_Num != 64)
		{
			pHashlist[HashID].m_aIndex[pHashlist[HashID].m_Num] = i;
			pHashlist[HashID].m_aKeys[pHashlist[HashID].m_Num] = Key;
			pHashlist[HashID].m_Num++;
		}
	}
}

static int GetItemIndexHashed(int Key, const CItemList *pHashlist)
{
		int HashID = ((Key>>12)&0xf0) | (Key&0xf);
		for(int i = 0; i < pHashlist[HashID].m_Num; i++)
		{
			if(pHashlist[HashID].m_aKeys[i] == Key)
				return pHashlist[HashID].m_aIndex[i];
	}

	return -1;
}

int CSnapshotDelta::DiffItem(const int *pPast, const int *pCurrent, int *pOut, int Size)
{
	int Needed = 0;
	while(Size)
	{
		*pOut = *pCurrent-*pPast;
		Needed |= *pOut;
		pOut++;
		pPast++;
		pCurrent++;
		Size--;
	}

	return Needed;
}

void CSnapshotDelta::UndiffItem(const int *pPast, const int *pDiff, int *pOut, int Size)
{
	while(Size)
	{
		*pOut = *pPast+*pDiff;

		if(*pDiff == 0)
			m_aSnapshotDataRate[m_SnapshotCurrent] += 1;
		else
		{
			unsigned char aBuf[16];
			unsigned char *pEnd = CVariableInt::Pack(aBuf, *pDiff);
			m_aSnapshotDataRate[m_SnapshotCurrent] += (int)(pEnd - (unsigned char*)aBuf) * 8;
		}

		pOut++;
		pPast++;
		pDiff++;
		Size--;
	}
}

CSnapshotDelta::CSnapshotDelta()
{
	mem_zero(m_aItemSizes, sizeof(m_aItemSizes));
	mem_zero(m_aSnapshotDataRate, sizeof(m_aSnapshotDataRate));
	mem_zero(m_aSnapshotDataUpdates, sizeof(m_aSnapshotDataUpdates));
	m_SnapshotCurrent = 0;
	mem_zero(&m_Empty, sizeof(m_Empty));
}

void CSnapshotDelta::SetStaticsize(int ItemType, int Size)
{
	m_aItemSizes[ItemType] = Size;
}

CSnapshotDelta::CData *CSnapshotDelta::EmptyDelta()
{
	return &m_Empty;
}

// TODO: OPT: this should be made much faster
int CSnapshotDelta::CreateDelta(const CSnapshot *pFrom, CSnapshot *pTo, void *pDstData)
{
	CData *pDelta = (CData *)pDstData;
	int *pData = (int *)pDelta->m_pData;
	int i, ItemSize, PastIndex;
	const CSnapshotItem *pFromItem;
	const CSnapshotItem *pCurItem;
	const CSnapshotItem *pPastItem;
	int SizeCount = 0;

	pDelta->m_NumDeletedItems = 0;
	pDelta->m_NumUpdateItems = 0;
	pDelta->m_NumTempItems = 0;

	CItemList Hashlist[HASHLIST_SIZE];
	GenerateHash(Hashlist, pTo);

	// pack deleted stuff
	for(i = 0; i < pFrom->NumItems(); i++)
	{
		pFromItem = pFrom->GetItem(i);
		if(GetItemIndexHashed(pFromItem->Key(), Hashlist) == -1)
		{
			// deleted
			pDelta->m_NumDeletedItems++;
			*pData = pFromItem->Key();
			pData++;
		}
	}

	GenerateHash(Hashlist, pFrom);
	int aPastIndecies[1024];

	// fetch previous indices
	// we do this as a separate pass because it helps the cache
	const int NumItems = pTo->NumItems();
	for(i = 0; i < NumItems; i++)
	{
		pCurItem = pTo->GetItem(i); // O(1) .. O(n)
		aPastIndecies[i] = GetItemIndexHashed(pCurItem->Key(), Hashlist); // O(n) .. O(n^n)
	}

	for(i = 0; i < NumItems; i++)
	{
		// do delta
		ItemSize = pTo->GetItemSize(i); // O(1) .. O(n)
		pCurItem = pTo->GetItem(i); // O(1) .. O(n)
		PastIndex = aPastIndecies[i];

		if(PastIndex != -1)
		{
			int *pItemDataDst = pData+3;

			pPastItem = pFrom->GetItem(PastIndex);

			if(m_aItemSizes[pCurItem->Type()])
				pItemDataDst = pData+2;

			if(DiffItem(pPastItem->Data(), (int*)pCurItem->Data(), pItemDataDst, ItemSize/4))
			{

				*pData++ = pCurItem->Type();
				*pData++ = pCurItem->ID();
				if(!m_aItemSizes[pCurItem->Type()])
					*pData++ = ItemSize/4;
				pData += ItemSize/4;
				pDelta->m_NumUpdateItems++;
			}
		}
		else
		{
			*pData++ = pCurItem->Type();
			*pData++ = pCurItem->ID();
			if(!m_aItemSizes[pCurItem->Type()])
				*pData++ = ItemSize/4;

			mem_copy(pData, pCurItem->Data(), ItemSize);
			SizeCount += ItemSize;
			pData += ItemSize/4;
			pDelta->m_NumUpdateItems++;
		}
	}

	if(0)
	{
		dbg_msg("snapshot", "%d %d %d",
			pDelta->m_NumDeletedItems,
			pDelta->m_NumUpdateItems,
			pDelta->m_NumTempItems);
	}

	/*
	// TODO: pack temp stuff

	// finish
	//mem_copy(pDelta->offsets, deleted, pDelta->num_deleted_items*sizeof(int));
	//mem_copy(&(pDelta->offsets[pDelta->num_deleted_items]), update, pDelta->num_update_items*sizeof(int));
	//mem_copy(&(pDelta->offsets[pDelta->num_deleted_items+pDelta->num_update_items]), temp, pDelta->num_temp_items*sizeof(int));
	//mem_copy(pDelta->data_start(), data, data_size);
	//pDelta->data_size = data_size;
	* */

	if(!pDelta->m_NumDeletedItems && !pDelta->m_NumUpdateItems && !pDelta->m_NumTempItems)
		return 0;

	return (int)((char*)pData-(char*)pDstData);
}

static int RangeCheck(const void *pEnd, const void *pPtr, int Size)
{
	if((const char *)pPtr + Size > (const char *)pEnd)
		return -1;
	return 0;
}

int CSnapshotDelta::UnpackDelta(const CSnapshot *pFrom, CSnapshot *pTo, const void *pSrcData, int DataSize)
{
	CSnapshotBuilder Builder;
	const CData *pDelta = (const CData *)pSrcData;
	const int *pData = (const int *)pDelta->m_pData;
	const int *pEnd = (const int *)(((const char *)pSrcData + DataSize));

	const CSnapshotItem *pFromItem;
	int Keep, ItemSize;
	const int *pDeleted;
	int ID, Type, Key;
	int FromIndex;
	int *pNewData;

	Builder.Init();

	// unpack deleted stuff
	pDeleted = pData;
	pData += pDelta->m_NumDeletedItems;
	if(pData > pEnd)
		return -1;

	// copy all non deleted stuff
	for(int i = 0; i < pFrom->NumItems(); i++)
	{
		// dbg_assert(0, "fail!");
		pFromItem = pFrom->GetItem(i);
		ItemSize = pFrom->GetItemSize(i);
		Keep = 1;
		for(int d = 0; d < pDelta->m_NumDeletedItems; d++)
		{
			if(pDeleted[d] == pFromItem->Key())
			{
				Keep = 0;
				break;
			}
		}

		if(Keep)
		{
			// keep it
			mem_copy(
				Builder.NewItem(pFromItem->Type(), pFromItem->ID(), ItemSize),
				pFromItem->Data(), ItemSize);
		}
	}

	// unpack updated stuff
	for(int i = 0; i < pDelta->m_NumUpdateItems; i++)
	{
		if(pData+2 > pEnd)
			return -1;

		Type = *pData++;
		if(Type < 0)
			return -1;
		ID = *pData++;
		if(m_aItemSizes[Type])
			ItemSize = m_aItemSizes[Type];
		else
		{
			if(pData+1 > pEnd)
				return -2;
			ItemSize = (*pData++) * 4;
		}
		m_SnapshotCurrent = Type;

		if(RangeCheck(pEnd, pData, ItemSize) || ItemSize < 0) return -3;

		Key = (Type<<16)|(ID&0xffff);

		// create the item if needed
		pNewData = Builder.GetItemData(Key);
		if(!pNewData)
			pNewData = (int *)Builder.NewItem(Key>>16, Key&0xffff, ItemSize);

		//if(range_check(pEnd, pNewData, ItemSize)) return -4;

		FromIndex = pFrom->GetItemIndex(Type, ID);
		if(FromIndex != -1)
		{
			// we got an update so we need to apply the diff
			UndiffItem(pFrom->GetItem(FromIndex)->Data(), pData, pNewData, ItemSize/4);
			m_aSnapshotDataUpdates[m_SnapshotCurrent]++;
		}
		else // no previous, just copy the pData
		{
			mem_copy(pNewData, pData, ItemSize);
			m_aSnapshotDataRate[m_SnapshotCurrent] += ItemSize*8;
			m_aSnapshotDataUpdates[m_SnapshotCurrent]++;
		}

		pData += ItemSize/4;
	}

	// finish up
	return Builder.Finish(pTo);
}


// CSnapshotStorage

void CSnapshotStorage::Init()
{
	m_pFirst = 0;
	m_pLast = 0;
}

void CSnapshotStorage::PurgeAll()
{
	CHolder *pHolder = m_pFirst;
	CHolder *pNext;

	while(pHolder)
	{
		pNext = pHolder->m_pNext;
		mem_free(pHolder);
		pHolder = pNext;
	}

	// no more snapshots in storage
	m_pFirst = 0;
	m_pLast = 0;
}

void CSnapshotStorage::PurgeUntil(int Tick)
{
	CHolder *pHolder = m_pFirst;
	CHolder *pNext;

	while(pHolder)
	{
		pNext = pHolder->m_pNext;
		if(pHolder->m_Tick >= Tick)
			return; // no more to remove
		mem_free(pHolder);

		// did we come to the end of the list?
		if (!pNext)
			break;

		m_pFirst = pNext;
		pNext->m_pPrev = 0x0;

		pHolder = pNext;
	}

	// no more snapshots in storage
	m_pFirst = 0;
	m_pLast = 0;
}

void CSnapshotStorage::Add(int Tick, int64 Tagtime, int DataSize, void *pData, int CreateAlt)
{
	// allocate memory for holder + snapshot_data
	int TotalSize = sizeof(CHolder)+DataSize;

	if(CreateAlt)
		TotalSize += DataSize;

	CHolder *pHolder = (CHolder *)mem_alloc(TotalSize, 1);

	// set data
	pHolder->m_Tick = Tick;
	pHolder->m_Tagtime = Tagtime;
	pHolder->m_SnapSize = DataSize;
	pHolder->m_pSnap = (CSnapshot*)(pHolder+1);
	mem_copy(pHolder->m_pSnap, pData, DataSize);

	if(CreateAlt) // create alternative if wanted
	{
		pHolder->m_pAltSnap = (CSnapshot*)(((char *)pHolder->m_pSnap) + DataSize);
		mem_copy(pHolder->m_pAltSnap, pData, DataSize);
	}
	else
		pHolder->m_pAltSnap = 0;


	// link
	pHolder->m_pNext = 0;
	pHolder->m_pPrev = m_pLast;
	if(m_pLast)
		m_pLast->m_pNext = pHolder;
	else
		m_pFirst = pHolder;
	m_pLast = pHolder;
}

int CSnapshotStorage::Get(int Tick, int64 *pTagtime, CSnapshot **ppData, CSnapshot **ppAltData)
{
	CHolder *pHolder = m_pFirst;

	while(pHolder)
	{
		if(pHolder->m_Tick == Tick)
		{
			if(pTagtime)
				*pTagtime = pHolder->m_Tagtime;
			if(ppData)
				*ppData = pHolder->m_pSnap;
			if(ppAltData)
				*ppAltData = pHolder->m_pAltSnap;
			return pHolder->m_SnapSize;
		}

		pHolder = pHolder->m_pNext;
	}

	return -1;
}

// CSnapshotBuilder
CSnapshotBuilder::CSnapshotBuilder()
{
	m_NumExtendedItemTypes = 0;
}

void CSnapshotBuilder::Init(bool Sevendown)
{
	m_DataSize = 0;
	m_NumItems = 0;
	m_Sevendown = Sevendown;

	for(int i = 0; i < m_NumExtendedItemTypes; i++)
	{
		AddExtendedItemType(i);
	}
}

void CSnapshotBuilder::Init(const CSnapshot *pSnapshot)
{
	if(pSnapshot->m_DataSize + sizeof(CSnapshot) + pSnapshot->m_NumItems * sizeof(int)*2 > CSnapshot::MAX_SIZE || pSnapshot->m_NumItems > MAX_ITEMS)
	{
		// key and offset per item
		dbg_assert(m_DataSize + sizeof(CSnapshot) + m_NumItems * sizeof(int)*2 < CSnapshot::MAX_SIZE, "too much data");
		dbg_assert(m_NumItems < MAX_ITEMS, "too many items");
		dbg_msg("snapshot", "invalid snapshot"); // remove me
		m_DataSize = 0;
		m_NumItems = 0;
		return;
	}

	m_DataSize = pSnapshot->m_DataSize;
	m_NumItems = pSnapshot->m_NumItems;
	mem_copy(m_aOffsets, pSnapshot->Offsets(), sizeof(int)*m_NumItems);
	mem_copy(m_aData, pSnapshot->DataStart(), m_DataSize);
}

bool CSnapshotBuilder::UnserializeSnap(const char *pSrcData, int SrcSize)
{
	m_DataSize = 0;
	m_NumItems = 0;

	const int *pData = (const int*)pSrcData;
	if(SrcSize < (int)sizeof(int)*2)
		return false;

	int DataSize = pData[0];
	int NumItems = pData[1];
	int CompleteSize = DataSize + sizeof(int) * (2 + NumItems);
	int NewSnapSize = DataSize + sizeof(CSnapshot) + NumItems * sizeof(int)*2;
	if(NewSnapSize > CSnapshot::MAX_SIZE || NumItems > MAX_ITEMS || CompleteSize != SrcSize)
		return false;

	// check offsets
	const int *pOffsets = pData+2;
	int LastOffset = DataSize;
	for(int i = NumItems-1; i >= 0; i--)
	{
		int ItemSize = LastOffset - pOffsets[i];
		LastOffset = pOffsets[i];
		if(pOffsets[i] < 0 || ItemSize < (int)sizeof(CSnapshotItem))
			return false;
	}

	m_DataSize = DataSize;
	m_NumItems = NumItems;
	mem_copy(m_aOffsets, pOffsets, sizeof(int)*m_NumItems);
	mem_copy(m_aData, pOffsets+m_NumItems, m_DataSize);
	return true;
}

CSnapshotItem *CSnapshotBuilder::GetItem(int Index)
{
	return (CSnapshotItem *)&(m_aData[m_aOffsets[Index]]);
}

int *CSnapshotBuilder::GetItemData(int Key)
{
	int i;
	for(i = 0; i < m_NumItems; i++)
	{
		if(GetItem(i)->Key() == Key)
			return GetItem(i)->Data();
	}
	return 0;
}

int CSnapshotBuilder::Finish(void *pSnapdata)
{
	// flattern and make the snapshot
	CSnapshot *pSnap = (CSnapshot *)pSnapdata;
	int OffsetSize = sizeof(int)*m_NumItems;
	int KeySize = sizeof(int)*m_NumItems;
	pSnap->m_DataSize = m_DataSize;
	pSnap->m_NumItems = m_NumItems;

	const int NumItems = m_NumItems;
	for(int i = 0; i < NumItems; i++)
	{
		pSnap->SortedKeys()[i] = GetItem(i)->Key();
	}

	// get full item sizes
	int aItemSizes[CSnapshotBuilder::MAX_ITEMS];

	for(int i = 0; i < NumItems-1; i++)
	{
		aItemSizes[i] = m_aOffsets[i+1] - m_aOffsets[i];
	}
	aItemSizes[NumItems-1] = m_DataSize - m_aOffsets[NumItems-1];

	// bubble sort by keys
	bool Sorting = true;
	while(Sorting)
	{
		Sorting = false;

		for(int i = 1; i < NumItems; i++)
		{
			if(pSnap->SortedKeys()[i-1] > pSnap->SortedKeys()[i])
			{
				Sorting = true;
				tl_swap(pSnap->SortedKeys()[i], pSnap->SortedKeys()[i-1]);
				tl_swap(m_aOffsets[i], m_aOffsets[i-1]);
				tl_swap(aItemSizes[i], aItemSizes[i-1]);
			}
		}
	}

	// copy sorted items
	int OffsetCur = 0;
	for(int i = 0; i < NumItems; i++)
	{
		pSnap->Offsets()[i] = OffsetCur;
		mem_copy(pSnap->DataStart()+OffsetCur, m_aData + m_aOffsets[i], aItemSizes[i]);
		OffsetCur += aItemSizes[i];
	}

	return sizeof(CSnapshot) + KeySize + OffsetSize + m_DataSize;
}

static int GetTypeFromIndex(int Index)
{
	return CSnapshot::MAX_TYPE - Index;
}

void CSnapshotBuilder::AddExtendedItemType(int Index)
{
	dbg_assert(0 <= Index && Index < m_NumExtendedItemTypes, "index out of range");
	int TypeID = m_aExtendedItemTypes[Index];
	CUuid Uuid = g_UuidManager.GetUuid(TypeID);
	int *pUuidItem = (int *)NewItem(0, GetTypeFromIndex(Index), sizeof(Uuid)); // NETOBJTYPE_EX
	for(int i = 0; i < (int)sizeof(CUuid) / 4; i++)
	{
		pUuidItem[i] =
			(Uuid.m_aData[i * 4 + 0] << 24) |
			(Uuid.m_aData[i * 4 + 1] << 16) |
			(Uuid.m_aData[i * 4 + 2] << 8) |
			(Uuid.m_aData[i * 4 + 3]);
	}
}

int CSnapshotBuilder::GetExtendedItemTypeIndex(int TypeID)
{
	for(int i = 0; i < m_NumExtendedItemTypes; i++)
	{
		if(m_aExtendedItemTypes[i] == TypeID)
		{
			return i;
		}
	}
	dbg_assert(m_NumExtendedItemTypes < MAX_EXTENDED_ITEM_TYPES, "too many extended item types");
	int Index = m_NumExtendedItemTypes;
	m_aExtendedItemTypes[Index] = TypeID;
	m_NumExtendedItemTypes++;
	AddExtendedItemType(Index);
	return Index;
}

void *CSnapshotBuilder::NewItem(int Type, int ID, int Size)
{
	if(m_DataSize + sizeof(CSnapshot) + sizeof(CSnapshotItem) + Size + (m_NumItems+1) * sizeof(int)*2 >= CSnapshot::MAX_SIZE ||
		m_NumItems+1 >= MAX_ITEMS)
	{
		// key and offset per item
		dbg_assert(m_DataSize + sizeof(CSnapshot) + m_NumItems * sizeof(int)*2 < CSnapshot::MAX_SIZE, "too much data");
		dbg_assert(m_NumItems < MAX_ITEMS, "too many items");
		return 0;
	}

	if(Type >= OFFSET_UUID)
	{
		Type = GetTypeFromIndex(GetExtendedItemTypeIndex(Type));
	}

	CSnapshotItem *pObj = (CSnapshotItem *)(m_aData + m_DataSize);

	if(m_Sevendown)
	{
		Type = ObjTypeToSevendown(Type);
		if(Type < 0) return pObj;
	}

	mem_zero(pObj, sizeof(CSnapshotItem) + Size);
	pObj->SetKey(Type, ID);
	m_aOffsets[m_NumItems] = m_DataSize;
	m_DataSize += sizeof(CSnapshotItem) + Size;
	m_NumItems++;

	return pObj->Data();
}
