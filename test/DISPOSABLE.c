//* _DISPOSABLE_h : All the simulation for Disposable FTL are defined in this file
//* dynamic wear leveling: using cost-benefit function for its GC


#ifndef _DISPOSABLE_C
#define _DISPOSABLE_C
#endif


#include "main.h"
#include "DISPOSABLE.h"


//# Debug
static cnt=0;

//* *************** global variabl *******************

static VRTElement_t *VirtualRegionTable;	//# the virtual region table
static VBTElement_t **VirtualBlcokTables; //# the virtual block tables
static VPTElement_t ***VirtualPageTables;	//# the virtual page tables

static flash_size_t *SpareBlockCnt;	//# the number of remaining spare blocks in each flash chip (only for DISPOSABLE management scheme)

static LRUList_t VBTLRUList = {0};		//# The LRU list for caching virtual block tables
LRUList_t VPTLRUList = {0};		//# The LRU list for caching virtual page tables
static LRUList_t ParityCheckLRUList = {0};		//# The LRU list for caching parity-check information

static FreeBlockList_t *DisposableFreeBlockLists;		//# The free block lists, where each physical region has its own

RegionCnt_t *RegionCnt;	//#the total block erase count and total new developed bad blocks in a physical region

static flash_size_t MaxRegion;	//# keep the number of regions
static flash_size_t BlocksInTheLastVirtualRegion;	//# keep the number of virtual blocks in the last virtual region
static flash_size_t BlocksInTheLastPhysicalRegion;	//# keep the number of physical blocks in the last physical region
static flash_size_t PagesPerVirtualBlock;	//# the number of virtual pages in a virtual block

static flash_size_t VirtualPageSize;	//# the size of a virtual page
static flash_size_t VirtualBlockSize;	//# the size of a virtual block
static flash_size_t VirtualRegionSize;	//# the size of a virtual region, except the last region
static flash_size_t VirtualFlashSize;	//# the size of the flash-memory storage system in the virtual view
static flash_size_t ClustersPerVirtualPage;		//# the number of clusters in a virtual page
static flash_size_t ClustersPerVirtualBlock;	//# the number of clusters in a virtual block
static flash_size_t ClustersPerVirtualRegion;	//# the number of clusters in a virtual region, except the last region


//# ******************************** Functions ****************************************

//# ****************************************************************
//# Initialize the settings for flash memory in DISPOSABLE management schele
//# ****************************************************************
static void InitializeDisposableFlashMemory(void)
{
	//# set the size of virtual page, virtual block, virtual region, and the virtual view of flash memory
	ClustersPerVirtualPage = ChipNumber;
	ClustersPerVirtualBlock = ClustersPerVirtualPage * (MaxPage-1);
	ClustersPerVirtualRegion = ClustersPerVirtualBlock * BlocksPerVirtualRegion;
	
	//# set the number of virtual regions and the number of virtual blocks in the last virtual region
	MaxRegion = (BlocksPerChip-SpareBlocksPerChip)/BlocksPerPhysicalRegion;
	if((BlocksPerChip-SpareBlocksPerChip)%BlocksPerPhysicalRegion !=0 ) 
	{
		MaxRegion ++;
		BlocksInTheLastPhysicalRegion = (BlocksPerChip-SpareBlocksPerChip)%BlocksPerPhysicalRegion;
		BlocksInTheLastVirtualRegion = (int)((float)BlocksInTheLastPhysicalRegion*BlocksPerVirtualRegion/BlocksPerPhysicalRegion);
	}
	else
	{
		BlocksInTheLastPhysicalRegion = BlocksPerPhysicalRegion;
		BlocksInTheLastVirtualRegion = BlocksPerVirtualRegion;
	}
	
	//# logical address settings
	SetMaxLBA(((MaxRegion-1)*BlocksPerVirtualRegion+BlocksInTheLastVirtualRegion)*ClustersPerVirtualBlock*PageToLBARatio);	//* the number of sectors in the flash memory
	
	//# initialize the number of virtual pages in a virtual block
	PagesPerVirtualBlock = MaxPage -1;

}


//# ****************************************************************
//# Initialize the translation tables and related lists
//# ****************************************************************
static void InitializeTranslationTables(void)
{
	int i,j;

	//# allocate memory space and initialize virtual region table
	VirtualRegionTable = (VRTElement_t *)malloc(sizeof(VRTElement_t)*MaxRegion);
	memset(VirtualRegionTable, DISPOSABLE_POINT_TO_NULL, sizeof(VRTElement_t)*MaxRegion);
	//# allocate memory space and initialize virtual block tables
	VirtualBlcokTables = (VBTElement_t **)malloc(sizeof(VBTElement_t *)*MaxRegion);
	for(i=0;i<MaxRegion;i++)
	{
		VirtualBlcokTables[i] = (VBTElement_t *)malloc(sizeof(VBTElement_t)*BlocksPerVirtualRegion);
		memset(VirtualBlcokTables[i], DISPOSABLE_POINT_TO_NULL, sizeof(VBTElement_t)*BlocksPerVirtualRegion);
	}
	//# allocate memory space and initialize virtual page tables
	VirtualPageTables = (VPTElement_t ***)malloc(sizeof(VPTElement_t **)*MaxRegion);
	for(i=0;i<MaxRegion;i++) 
	{
		VirtualPageTables[i] = (VPTElement_t **)malloc(sizeof(VPTElement_t *)*BlocksPerVirtualRegion);
		for(j=0;j<BlocksPerVirtualRegion;j++)
		{
			VirtualPageTables[i][j] =  (VPTElement_t *)malloc(sizeof(VPTElement_t)*PagesPerVirtualBlock);
			memset(VirtualPageTables[i][j], DISPOSABLE_POINT_TO_NULL, sizeof(VPTElement_t)*PagesPerVirtualBlock);
		}
	}

	//# allocate LRU list for translation tables and parity check
	CreateLRUList(&VBTLRUList, VBTCacheLength); //# create and initialize the LRU lsit for virtual block tables
	CreateLRUList(&VPTLRUList, VPTCacheLength); //# create and initialize the LRU lsit for virtual page tables
	CreateLRUList(&ParityCheckLRUList, ParityCheckCacheLength); //# create and initialize the LRU lsit for parity check 
}


//# ****************************************************************
//# Release the memory space allocated for translation tables
//# ****************************************************************
static void FinalizeTranslationTables(void)
{
	int i,j;
	
	//# free virtual region table
	free(VirtualRegionTable); 
	//# free virtual block tables
	for(i=0;i<MaxRegion;i++) free(VirtualBlcokTables[i]);
	free(VirtualBlcokTables);
	//# free virtual page tables
	for(i=0;i<MaxRegion;i++)
	{
		for(j=0;j<BlocksPerVirtualRegion;j++)
		{
			free(VirtualPageTables[i][j]);
		}
		free(VirtualPageTables[i]);
	}
	free(VirtualPageTables);

	//# free memory space allocated for LRUT list of translation tables and parity check
	FreeLRUList(&VBTLRUList); //# free the LRU lsit for virtual block tables
	FreeLRUList(&VPTLRUList); //# free the LRU lsit for virtual page tables
	FreeLRUList(&ParityCheckLRUList); //#free the LRU lsit for parity check 
}


//# ****************************************************************
//# Allocation the memory space for free block lists and spare blocks
//# ****************************************************************
static void InitializeFreeBlockLists(void)
{
	int i,j;

	//# allocate free space for free block lists
	DisposableFreeBlockLists = (FreeBlockList_t *)malloc(sizeof(FreeBlockList_t)*MaxRegion);
	for(i=0;i<MaxRegion;i++) 
	{
		DisposableFreeBlockLists[i].PhysicalBlockList = (flash_size_t *)malloc(sizeof(flash_size_t)*BlocksPerPhysicalRegion);
		//# initialize the free block list
		for(j=0;j<(BlocksPerPhysicalRegion-1);j++)
		{
			DisposableFreeBlockLists[i].PhysicalBlockList[j] = j+1;
		}
		DisposableFreeBlockLists[i].PhysicalBlockList[j] = FBL_POINT_TO_NULL;	//# the last element in the free block list
		DisposableFreeBlockLists[i].Head = 0;	//# the head of the free block list
		DisposableFreeBlockLists[i].Tail = j;	//# the tail of the free block list
		DisposableFreeBlockLists[i].Len = BlocksPerPhysicalRegion;	//# the number of free blocks in the list
	}
	//# initizliae the free block list of the last region
	DisposableFreeBlockLists[MaxRegion-1].PhysicalBlockList[BlocksInTheLastPhysicalRegion-1] = FBL_POINT_TO_NULL;	//# the last element in the free block list
	DisposableFreeBlockLists[MaxRegion-1].Head = 0;	//# the head of the free block list
	DisposableFreeBlockLists[MaxRegion-1].Tail = BlocksInTheLastPhysicalRegion-1; //# the tail of the free block list
	DisposableFreeBlockLists[MaxRegion-1].Len = BlocksInTheLastPhysicalRegion; //# the number of free blocks in the list
	
	//# Keep the number of remaining spare blocks in each chip
	SpareBlockCnt = (flash_size_t *)malloc(sizeof(flash_size_t)*ChipNumber);
	for(i=0;i<ChipNumber;i++)
	{
		SpareBlockCnt[i] = SpareBlocksPerChip - BadBlockCntArray[i];
		//printf("Chip=%d,SpareBlockCnt=%d,BadBlockCnt=%d\n",i,SpareBlockCnt[i],BadBlockCntArray[i]);
	}

	//# initialize the counter for the block erase count of each region
	RegionCnt = (RegionCnt_t *)malloc(sizeof(RegionCnt_t)*MaxRegion);
	memset(RegionCnt,0,sizeof(RegionCnt_t)*MaxRegion);

//	//# Debug
//	for(i=0;i<MaxRegion;i++)
//	{
//		printf("\ni=%d %d %d %d\n",i,DisposableFreeBlockLists[i].Head,DisposableFreeBlockLists[i].Tail,DisposableFreeBlockLists[i].Len);
//		for(j=0;j<(BlocksPerPhysicalRegion);j++)
//			printf("%4d",DisposableFreeBlockLists[i].PhysicalBlockList[j]);
//		
//	}

}

//# ****************************************************************
//# Release the memory space allocated for free block lists and spare blocks
//# ****************************************************************
static void FinalizeFreeBlockLists(void)
{
	int i;

	for(i=0;i<MaxRegion;i++) free(DisposableFreeBlockLists[i].PhysicalBlockList);
	free(DisposableFreeBlockLists);
	
	//# free the space allocated for the counter of remaining spare blocks
	free(SpareBlockCnt);
	
	//# free the counter for the block erase count of each region
	free(RegionCnt);
	
}

//# ****************************************************************
//# get one free block set from one physical region from the Head of the list
//# There is always some free blocksets in the free block list of the correspoding physical region
//# "region" indicates the ID of the physical region, from which we want to get a free block.
//# return the free block id on suceess. Return -1 when failed to find a free block
//# ****************************************************************
flash_size_t GetFreeBlockSetFromRegion(int region)
{
	flash_size_t blockset; //# to keep track of the temporary block set
	
	if(DisposableFreeBlockLists[region].Len <=1) return -1;	//# to guarantee there is at least one free block set

	blockset = DisposableFreeBlockLists[region].Head; //# get a block set form the head of the list
	DisposableFreeBlockLists[region].Head = DisposableFreeBlockLists[region].PhysicalBlockList[blockset]; //# move the head to the next free block set
	DisposableFreeBlockLists[region].PhysicalBlockList[blockset] = FBL_NOT_IN_LIST; //* Remove this block from the free block list and mark is as a block not in the free block list

	//# error checking
	if(blockset >= BlocksPerPhysicalRegion || blockset < 0)
	{
		printf("ERROR: A [region %d,physcial block set %d] returned from the free blocks list\n",region, blockset);
		exit(1);
	}

	DisposableFreeBlockLists[region].Len --; //#  Decrease the number of free block sets in the free blockset list

	return blockset;
}

//# ****************************************************************
//# Put one free block set to the Tail of the free blockset list
//# "region" indicates the ID of the physical region, to which we want to put a free block set.
//# "blockset" indicate the ID of the physical block set that is going to put into the free block set.
//# ****************************************************************
void PutFreeBlockSetToRegion(int region, int blockset)
{
	//* Add the primary block into the free block list
	DisposableFreeBlockLists[region].PhysicalBlockList[DisposableFreeBlockLists[region].Tail] = blockset;
	DisposableFreeBlockLists[region].Tail  = blockset;
	FreeBlockList[DisposableFreeBlockLists[region].Tail] = FBL_POINT_TO_NULL;
	DisposableFreeBlockLists[region].Len++;	//* advance the counter of the number of freeblocks
}


//# ****************************************************************
//# Check whether the target virtual block table, virtual page table, parity-check information is cached. If it is not cached, cache it.
//# The refernced table/parity-check is also moved to the head of the LRU list 
//# "list" is the LRU list to cache virtual block table, virtual page table, or parity-check information
//# "keyvalue" is the refenced table ID, because when a table is cached in the LRU list, the key value of an LRU element is the referenced table ID.
//# "index" is the address of the index pointing to the target virtual block table, virtual page table, or parity-check information
//# "TotalRequestTime" is the statistic variable that calculates the total write request time or the total read request time. Possible parameters: AccessStatistics.TotalWriteRequestTime or AccessStatistics.TotalReadRequestTime   
//# ****************************************************************
void UpdateLRUCache(LRUList_t *list, flash_size_t keyvalue, flash_size_t *index, flash_huge_size_t *TotalRequestTime)
{
	int pos;				//# the position of the element in the LRU list
	int anotherpos;			//# the position of the element in the LRU list
	int key;				//# the key stored in the element of the LRU list
	int num;				//# the number of searched elements in the LRU list
	
	
	if(*index == DISPOSABLE_POINT_TO_NULL || list->Element[*index].key != keyvalue)
	{
		//# advance the number of miss count for the cache
		if(list == &VBTLRUList) AccessStatistics.TotalVBTMissCnt++;
		if(list == &VPTLRUList) AccessStatistics.TotalVPTMissCnt++;
		if(list == &ParityCheckLRUList) AccessStatistics.TotalParityCheckMissCnt++;
		
		if(IsLRUListFull(list)) //# If the cache for the referenced table is full, remove the last one
		{
			RemoveTheTailElemenetFromLRUList(list, &key, &pos);	//# remove the last one
			
			//# Debug
			if(pos<0) { printf("pos of removed element=%d",pos); exit(-1);}
			
			if(IsElementDirty(list, &pos)) //# if the removed virtual table is dirty, we have to write it back to flash memory --> we have to count the time on writing it back
			{
				ClearElementDirty(list, key, &pos); //# set the cahced element as clean

				//# if the removed element is a virtual page table, its corresponding parity-check information should be removed as well
				if(list == &VPTLRUList)
				{
					if(IsElemenetInLRUList(&ParityCheckLRUList, key, &anotherpos, &num)) //# search the corresponding parity-check information
					{
						//# Debug
						if(anotherpos<0) { printf("anotherpos of removed element=%d",anotherpos); exit(-1);}
				
						ClearElementDirty(&ParityCheckLRUList, key, &anotherpos); //# set the cahced element as clean
						RemoveElemenetFromLRUList(&ParityCheckLRUList,key, &anotherpos); //# remove it
					}
				}
				//# if the removed element is a parity-check, its corresponding virtual page table should be removed as well
				if(list == &ParityCheckLRUList)
				{
					if(IsElemenetInLRUList(&VPTLRUList, key, &anotherpos, &num)) //# search the corresponding parity-check information
					{
						//# Debug
						if(anotherpos<0) { printf("anotherpos of removed element=%d",anotherpos); exit(-1);}
							
						if(IsElementDirty(&VPTLRUList, &anotherpos)) //# if the corresponding virtual page table is dirty, remove it
						{
							ClearElementDirty(&VPTLRUList, key, &anotherpos); //# set the cahced element as clean
							RemoveElemenetFromLRUList(&VPTLRUList, key, &anotherpos); //# remove it
						}
					}
				}

				//# Statistics
				*TotalRequestTime += TIME_WRITE_PAGE_NO_TRANSFER; //# increase the time on writes or reads
			}
		}

		PutElemenetToLRUList(list, keyvalue, &pos); //# cache the currently referenced virtual block table

		//# Debug
		if(pos<0) { printf("pos of removed element=%d",pos); exit(-1);}

		*index = pos; //# update the inde pointing to the new cache element
		//# Statistics
		*TotalRequestTime += TIME_READ_PAGE_NO_TRANSFER; //# increase the time on writes or reads
		
	}	
	else //# the referenced table or index is already in the cache, move it to the head of the list
	{
		MoveElemenetToTheHeadofLRUList(list, keyvalue, index); //# Move the referenced element to the head of the LRU list
	}


}

//# ****************************************************************
//# Do garbage collection to reclaim one more free physical block set if there is no free physical block set in the free block set list
//# dynamic wear leveling: using cost-benefit strategy for its GC-> 
//# (free page=0, live page=-1, dead page = 1) while the sum over every page is larger than 0, erase the block.
//# "VirtualRegion" is the virtual region in which we want to reclaim new physical block set
//# "NotToReclaimVirtualBlock" is the block set that can not reclaim because  we are currently accessing it
//# Return value: if there is no physical block set to reclaim, return False. Otherwise, return True.
//# ****************************************************************
Boolean DisposableGarbageCollection(flash_size_t VirtualRegion, flash_size_t NotToReclaimVirtualBlock)
{
	int i;
	int offset;
	flash_size_t NextVictimVirtualBlock; //# the victim virtual block to erase
	flash_size_t FreeBlockSet, CurrentFreePageSet;

	for(i=0;i<BlocksPerVirtualRegion;i++)
	{
		NextVictimVirtualBlock = (++VirtualRegionTable[VirtualRegion].CurrentScannedVirtualBlock) % BlocksPerVirtualRegion; //# get the virtual block to scan
		if(	NextVictimVirtualBlock != NotToReclaimVirtualBlock && \
			VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].NewPhysicalBlockSet != DISPOSABLE_POINT_TO_NULL && \
			VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].OldPhysicalBlockSet != DISPOSABLE_POINT_TO_NULL)
		{
			break; //# the vimtim virtual block has found
		}
	}

	if(i==BlocksPerVirtualRegion) return(False); //# can't find any victim block


	//# ****************** the victim virtual block is found. Start to merge the physical block sets *************************
	//# put the reclaimed physical block sets to the free block list
	PutFreeBlockSetToRegion(VirtualRegion, VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].NewPhysicalBlockSet);
	PutFreeBlockSetToRegion(VirtualRegion, VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].OldPhysicalBlockSet);

	//# get free block set
	FreeBlockSet = GetFreeBlockSetFromRegion(VirtualRegion);
	CurrentFreePageSet = 0;
	
	//# merge the new and old physical block sets of the virtual block
	for(i=0;i<PagesPerVirtualBlock;i++)
	{
		if(	VirtualPageTables[VirtualRegion][NextVictimVirtualBlock][i].BlockSet == VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].OldPhysicalBlockSet ||
			VirtualPageTables[VirtualRegion][NextVictimVirtualBlock][i].BlockSet == VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].NewPhysicalBlockSet)
		{
			//# copy one page set
			FlashMemoryArray[0][FreeBlockSet][CurrentFreePageSet] = FlashMemoryArray[0][VirtualPageTables[VirtualRegion][NextVictimVirtualBlock][i].BlockSet][VirtualPageTables[VirtualRegion][NextVictimVirtualBlock][i].PageSet];
			//# update the virtual page table
			VirtualPageTables[VirtualRegion][NextVictimVirtualBlock][i].BlockSet = FreeBlockSet;
			VirtualPageTables[VirtualRegion][NextVictimVirtualBlock][i].PageSet = CurrentFreePageSet;
			CurrentFreePageSet++; //# move to the next free physical page set

			//# Statistics - read and write (copy) one page set
			AccessStatistics.TotalWriteRequestTime += TIME_TRANSFER_PAGE * ChipNumber + TIME_READ_PAGE_NO_TRANSFER; //# read one page set
			AccessStatistics.TotalWriteRequestTime += TIME_TRANSFER_PAGE * ChipNumber + TIME_WRITE_PAGE_NO_TRANSFER; //# write one page set
			//# Statistics
			AccessStatistics.TotalLivePageCopyings += ChipNumber;	//* Adance the total live-page copyings by ChipNumber.
		}
	}

	//# Erase the physical block sets
	for(i=0;i<PagesPerVirtualBlock;i++)
	{
		FlashMemoryArray[0][VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].NewPhysicalBlockSet][i] = DISPOSABLE_POINT_TO_NULL;
		FlashMemoryArray[0][VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].OldPhysicalBlockSet][i] = DISPOSABLE_POINT_TO_NULL;
	}

	//# Statistics -- the erase count of each block
	BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion+VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].NewPhysicalBlockSet]++;	//* update the erase count
	if(BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion+VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].NewPhysicalBlockSet]>MaxBlockEraseCnt) 
	{
		MaxBlockEraseCnt = BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion+VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].NewPhysicalBlockSet];
		offset = rand() % BlocksPerPhysicalRegion;
		BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion+VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].NewPhysicalBlockSet] = BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion + offset];
		BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion + offset] = MaxBlockEraseCnt;
	}
	BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion+VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].OldPhysicalBlockSet]++;	//* update the erase count
	if(BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion+VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].OldPhysicalBlockSet]>MaxBlockEraseCnt) 
	{
		MaxBlockEraseCnt = BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion+VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].OldPhysicalBlockSet];
		offset = rand() % BlocksPerPhysicalRegion;
		BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion+VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].OldPhysicalBlockSet] = BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion + offset];
		BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion + offset] = MaxBlockEraseCnt;
	}
	//# update the number of block erase count in this region
	RegionCnt[VirtualRegion].TotalBlockEraseCnt += ChipNumber*2;

	//# Update virtual block table
	VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].OldPhysicalBlockSet = DISPOSABLE_POINT_TO_NULL;
	VirtualBlcokTables[VirtualRegion][NextVictimVirtualBlock].NewPhysicalBlockSet = FreeBlockSet;	

	//# Statistics
	AccessStatistics.TotalWriteRequestTime += TIME_ERASE_BLOCK; //# Erase the physical block set
	AccessStatistics.TotalBlockEraseCount += ChipNumber;	//* Adance the total live-page copyings by ChipNumber.

	return(True);
}


//# ****************************************************************
//# Write data to a virtual page ( or a physical page set)
//# "ClusterID" is the start Cluster that is going to be updated or written
//# "StartChip" is the first chip to be written with data
//# "EndChip" is the last chip to be written with data
//# Return value: if the write operation is correct, return True. Otherwise, return False
//# ****************************************************************
Boolean DisposableWriteOnePageSet(flash_size_t ClusterID, flash_size_t StartChip, flash_size_t EndChip)
{
	int i;
	int offset;
	int ValidPagesInOldBlockSet,ValidPagesInNewBlockSet; //# the counters to count the number of valid page sets in the new/old block sets

	flash_size_t FreeBlockSet;	//# A free physical block set
	flash_size_t ReplacedBlockSet;	//# a physical block set that is going to be replaced
	flash_size_t VirtualRegion;	//# The target virtual region
	flash_size_t VirtualBlock;	//# The target virtual block
	flash_size_t AbsoluteVirtualBlock;	//# the target absolute virtual block
	flash_size_t VirtualPage;	//# The target virtual page
	flash_size_t AbsoluteVirtualPage; //# the target absolute virtual page


	
	VirtualRegion = ClusterID / ClustersPerVirtualRegion; //# get the virtual region
	VirtualBlock = (ClusterID % ClustersPerVirtualRegion) / ClustersPerVirtualBlock; //# get the relative virtual block
	VirtualPage = ((ClusterID % ClustersPerVirtualRegion) % ClustersPerVirtualBlock) / ClustersPerVirtualPage; //# get the virtual page
	AbsoluteVirtualBlock = ClusterID / ClustersPerVirtualBlock;	//# get the absolute virtual block
	AbsoluteVirtualPage = ClusterID / ClustersPerVirtualPage; //# get the virtual page
	
	//# check whether the target virtual block table, virtual page table, parity-check information is cached. If it is not cached, cache it.
	UpdateLRUCache(&VBTLRUList, VirtualRegion, &VirtualRegionTable[VirtualRegion].VBTIndex, &AccessStatistics.TotalWriteRequestTime); //# update caches of virtual block tables	
	UpdateLRUCache(&VPTLRUList, AbsoluteVirtualBlock, &VirtualBlcokTables[VirtualRegion][VirtualBlock].VPTIndex, &AccessStatistics.TotalWriteRequestTime); //# update caches of virtual page tables
	UpdateLRUCache(&ParityCheckLRUList, AbsoluteVirtualBlock, &VirtualBlcokTables[VirtualRegion][VirtualBlock].ParityCheckIndex, &AccessStatistics.TotalWriteRequestTime); //# udpate caches of parity-check information
	//# advance the number of referenced count for the cache
	AccessStatistics.TotalVBTReferenceCnt++;
	AccessStatistics.TotalVPTReferenceCnt++;
	AccessStatistics.TotalParityCheckReferenceCnt++;

	//# ************************* Make sure there is free space for new writes to a vritual page **************************
	//# Check whether the virtual block has its own new physical block set
	if(VirtualBlcokTables[VirtualRegion][VirtualBlock].NewPhysicalBlockSet == DISPOSABLE_POINT_TO_NULL) 
	{ //# CASE 1: the virtual block doesn't have its corresponding physical block set.
		//# This virtual block doesn't has its corresponding new physical block set. Allocate a physical block set for it.
		while((FreeBlockSet=GetFreeBlockSetFromRegion(VirtualRegion))== -1)
		{
			if(DisposableGarbageCollection(VirtualRegion, VirtualBlock)==False)	//# Do garbage collection to reclaim one more free physical block set if there is no free physical block set in the free block set list
				return False;
		}
		VirtualBlcokTables[VirtualRegion][VirtualBlock].NewPhysicalBlockSet = FreeBlockSet; //# Assign the allocated physical block set to the virtual block
		VirtualBlcokTables[VirtualRegion][VirtualBlock].CurrentFreePageSet = 0; //# The current free page set of a new allocated physical block set is 0
	}
	else if(VirtualBlcokTables[VirtualRegion][VirtualBlock].CurrentFreePageSet >= PagesPerVirtualBlock) //# The new physical block set is full
	{ 
		//# the new physical block is full, write the summary page to commit
		ClearElementDirty(&VPTLRUList, AbsoluteVirtualBlock, &VirtualBlcokTables[VirtualRegion][VirtualBlock].VPTIndex); //# set the cahced element as clean
		ClearElementDirty(&ParityCheckLRUList, AbsoluteVirtualBlock, &VirtualBlcokTables[VirtualRegion][VirtualBlock].ParityCheckIndex); //# set the cahced element as clean
		//# Statistics
		AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE_NO_TRANSFER; //# increase the time on writes

		//# the virtual block doesn't have its corresponding old physical block set, allocate a free physical block set for it
		if(VirtualBlcokTables[VirtualRegion][VirtualBlock].OldPhysicalBlockSet == DISPOSABLE_POINT_TO_NULL) 
		{ //# CASE 2: the virtual block has its corresponding new physical block set, but doesn't have its corresponding old block set.
			//# This virtual block doesn't has its corresponding new physical block set. Allocate a physical block set for it.
			while((FreeBlockSet=GetFreeBlockSetFromRegion(VirtualRegion))== -1)
			{
				if(DisposableGarbageCollection(VirtualRegion, VirtualBlock)==False)	//# Do garbage collection to reclaim one more free physical block set if there is no free physical block set in the free block set list
					return False;
			}			
			//# Set the new allocated block set as "new" and the existing block set as "old"
			VirtualBlcokTables[VirtualRegion][VirtualBlock].OldPhysicalBlockSet = VirtualBlcokTables[VirtualRegion][VirtualBlock].NewPhysicalBlockSet;
			VirtualBlcokTables[VirtualRegion][VirtualBlock].NewPhysicalBlockSet = FreeBlockSet; //# Assign the allocated physical block set to the virtual block
			VirtualBlcokTables[VirtualRegion][VirtualBlock].CurrentFreePageSet = 0; //# The current free page set of a new allocated physical block set is 0
		}
		else //# the virtual block has its corresponding old physical block set, allocate a free physical block set to replace the existing block set that has fewer valid physical page sets
		{  //# CASE 3: the virtual block has both its corresponding old and physical block set.
			//# This virtual block doesn't has its corresponding new physical block set. Allocate a physical block set for it.
			while((FreeBlockSet=GetFreeBlockSetFromRegion(VirtualRegion))== -1)
			{
				if(DisposableGarbageCollection(VirtualRegion, VirtualBlock)==False)	//# Do garbage collection to reclaim one more free physical block set if there is no free physical block set in the free block set list
					return False;
			}
			//# the physical block set that has fewer valid physical page sets is replaced. 
			ValidPagesInOldBlockSet = 0;
			ValidPagesInNewBlockSet = 0;
			//# count the number of valid physical page sets in the new/old physical block sets
			for(i=0;i<PagesPerVirtualBlock;i++)
			{
				if(VirtualPageTables[VirtualRegion][VirtualBlock][i].BlockSet == VirtualBlcokTables[VirtualRegion][VirtualBlock].OldPhysicalBlockSet)
					ValidPagesInOldBlockSet++;
				if(VirtualPageTables[VirtualRegion][VirtualBlock][i].BlockSet == VirtualBlcokTables[VirtualRegion][VirtualBlock].NewPhysicalBlockSet)
					ValidPagesInNewBlockSet++;
				//# error checking
				if(	VirtualPageTables[VirtualRegion][VirtualBlock][i].BlockSet != VirtualBlcokTables[VirtualRegion][VirtualBlock].OldPhysicalBlockSet &&
					VirtualPageTables[VirtualRegion][VirtualBlock][i].BlockSet != VirtualBlcokTables[VirtualRegion][VirtualBlock].NewPhysicalBlockSet &&
					VirtualPageTables[VirtualRegion][VirtualBlock][i].BlockSet != DISPOSABLE_POINT_TO_NULL)
					printf("A virtual page is not stored in anyone of its correspodning physical block set\n");
			}
			
			//# update the corresponding new/old physical block set
			//# update virtual block table
			if(ValidPagesInNewBlockSet >= ValidPagesInOldBlockSet) //# replace the old physical block set
			{
				ReplacedBlockSet = VirtualBlcokTables[VirtualRegion][VirtualBlock].OldPhysicalBlockSet; //# old block set is the one to be replaced
				VirtualBlcokTables[VirtualRegion][VirtualBlock].OldPhysicalBlockSet = VirtualBlcokTables[VirtualRegion][VirtualBlock].NewPhysicalBlockSet; //# the new block set becomes the old block set
				VirtualBlcokTables[VirtualRegion][VirtualBlock].NewPhysicalBlockSet = FreeBlockSet; //# The new block set is the allocated free block set
			}
			else //# replace the new physical block set
			{
				ReplacedBlockSet = VirtualBlcokTables[VirtualRegion][VirtualBlock].NewPhysicalBlockSet; //# new block set is the one to be replaced
				VirtualBlcokTables[VirtualRegion][VirtualBlock].NewPhysicalBlockSet = FreeBlockSet; //# The new block set is the allocated free block set
			}
			VirtualBlcokTables[VirtualRegion][VirtualBlock].CurrentFreePageSet = 0; //# The current free page set of a new allocated physical block set is 0

			//# copy valid pages to the new allocated free block set
			for(i=0;i<PagesPerVirtualBlock;i++)
			{
				if(VirtualPageTables[VirtualRegion][VirtualBlock][i].BlockSet==ReplacedBlockSet)
				{
					//# store the logical address in the first chip
					//# copy data - only store the virtual page ID in the first chip
					FlashMemoryArray[0][VirtualRegion*BlocksPerPhysicalRegion+FreeBlockSet][VirtualBlcokTables[VirtualRegion][VirtualBlock].CurrentFreePageSet] = 
					FlashMemoryArray[0][VirtualRegion*BlocksPerPhysicalRegion+ReplacedBlockSet][VirtualPageTables[VirtualRegion][VirtualBlock][i].PageSet];
					//# updata the virtual page table
					VirtualPageTables[VirtualRegion][VirtualBlock][i].BlockSet = VirtualBlcokTables[VirtualRegion][VirtualBlock].NewPhysicalBlockSet;
					VirtualPageTables[VirtualRegion][VirtualBlock][i].PageSet = VirtualBlcokTables[VirtualRegion][VirtualBlock].CurrentFreePageSet++;
					//# Statistics - read and write (copy) one page set
					AccessStatistics.TotalWriteRequestTime += TIME_TRANSFER_PAGE * ChipNumber + TIME_READ_PAGE_NO_TRANSFER; //# read one page set
					AccessStatistics.TotalWriteRequestTime += TIME_TRANSFER_PAGE * ChipNumber + TIME_WRITE_PAGE_NO_TRANSFER; //# write one page set
					//# Statistics
					AccessStatistics.TotalLivePageCopyings += ChipNumber;	//* Adance the total live-page copyings by ChipNumber.
				}			
			}
			
			//# erase the block set - clear the virtual page ID stored in the first chip of the corresponding block
			for(i=0;i<PagesPerVirtualBlock;i++)
			{
				FlashMemoryArray[0][VirtualRegion*BlocksPerPhysicalRegion+ReplacedBlockSet][i] = DISPOSABLE_POINT_TO_NULL;
			}
			//# Erase the replaced block set and put it back to the free block set
			PutFreeBlockSetToRegion(VirtualRegion, ReplacedBlockSet);
			
			//# Statistics
			AccessStatistics.TotalWriteRequestTime += TIME_ERASE_BLOCK; //# Erase the physical block set
			AccessStatistics.TotalBlockEraseCount += ChipNumber;	//* Adance the total live-page copyings by ChipNumber.
			//# update the number of block erase count in this region
			RegionCnt[VirtualRegion].TotalBlockEraseCnt += ChipNumber;
			//# Statistics -- the erase count of each block
			BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion+ReplacedBlockSet]++;	//* update the erase count
			if(BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion+ReplacedBlockSet]>MaxBlockEraseCnt) 
			{
				MaxBlockEraseCnt = BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion+ReplacedBlockSet];
				offset = rand() % BlocksPerPhysicalRegion;
				BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion+ReplacedBlockSet] = BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion + offset];
				BlockEraseCnt[VirtualRegion*BlocksPerPhysicalRegion + offset] = MaxBlockEraseCnt;				
			}
			
		}
	}
	//# ************************* end of Make sure there is free space for new writes to a vritual page **************************

	//# write the new data to the physical page set
	//# copy data - only store the virtual page ID in the first chip
	FlashMemoryArray[0][VirtualRegion*BlocksPerPhysicalRegion+VirtualBlcokTables[VirtualRegion][VirtualBlock].NewPhysicalBlockSet][VirtualBlcokTables[VirtualRegion][VirtualBlock].CurrentFreePageSet] = AbsoluteVirtualPage;
	//# updata the virtual page table
	VirtualPageTables[VirtualRegion][VirtualBlock][VirtualPage].BlockSet = VirtualBlcokTables[VirtualRegion][VirtualBlock].NewPhysicalBlockSet;
	VirtualPageTables[VirtualRegion][VirtualBlock][VirtualPage].PageSet = VirtualBlcokTables[VirtualRegion][VirtualBlock].CurrentFreePageSet++;
	 
	//# Set the LRU element dirty because the element is modified
	SetElementDirty(&VPTLRUList, AbsoluteVirtualBlock, &VirtualBlcokTables[VirtualRegion][VirtualBlock].VPTIndex); //# set the cahced element as clean
	SetElementDirty(&ParityCheckLRUList, AbsoluteVirtualBlock, &VirtualBlcokTables[VirtualRegion][VirtualBlock].ParityCheckIndex); //# set the cahced element as clean


	//# Statistics
	AccessStatistics.TotalWriteRequestTime += TIME_TRANSFER_PAGE * (EndChip-StartChip+1) + TIME_WRITE_PAGE_NO_TRANSFER; //# write one page set

	return (True);
}


//# ****************************************************************
//# Read data from a virtual page ( or a physical page set)
//# "ClusterID" is the start Cluster that is going to be read from
//# "StartChip" is the first chip to be read with data
//# "EndChip" is the last chip to be read with data
//# "TotalRequestTime" is the statistic variable that calculates the total write request time or the total read request time. Possible parameters: AccessStatistics.TotalWriteRequestTime or AccessStatistics.TotalReadRequestTime   
//# Return value: If the "VirtualPage" exists, return True. Otherwise, return False.
//# ****************************************************************
Boolean DisposableReadOnePageSet(flash_size_t ClusterID, flash_size_t StartChip, flash_size_t EndChip,  flash_huge_size_t *TotalRequestTime)
{
	flash_size_t VirtualRegion;	//# The target virtual region
	flash_size_t VirtualBlock;	//# The target virtual block
	flash_size_t VirtualPage;	//# The target virtual page
	flash_size_t AbsoluteVirtualBlock;	//# the target absolute virtual block
	flash_size_t AbsoluteVirtualPage; //# the target absolute virtual page


	
	VirtualRegion = ClusterID / ClustersPerVirtualRegion; //# get the virtual region
	VirtualBlock = (ClusterID % ClustersPerVirtualRegion) / ClustersPerVirtualBlock; //# get the relative virtual block
	VirtualPage = ((ClusterID % ClustersPerVirtualRegion) % ClustersPerVirtualBlock) / ClustersPerVirtualPage; //# get the virtual page
	AbsoluteVirtualBlock = ClusterID / ClustersPerVirtualBlock;	//# get the absolute virtual block
	AbsoluteVirtualPage = ClusterID / ClustersPerVirtualPage; //# get the virtual page

	
	//# checked whether the accessed cluster exists or not
	if(	VirtualPageTables[VirtualRegion][VirtualBlock][VirtualPage].BlockSet == DISPOSABLE_POINT_TO_NULL ||
		VirtualPageTables[VirtualRegion][VirtualBlock][VirtualPage].PageSet == DISPOSABLE_POINT_TO_NULL)
	{
		return(False); //# the accessed clsuter doesn't exist
	}

	//# check whether the target virtual block table, virtual page table, parity-check information is cached. If it is not cached, cache it.
	//# check whether the target virtual block table, virtual page table, parity-check information is cached. If it is not cached, cache it.
	UpdateLRUCache(&VBTLRUList, VirtualRegion, &VirtualRegionTable[VirtualRegion].VBTIndex, TotalRequestTime); //# update caches of virtual block tables	
	UpdateLRUCache(&VPTLRUList, AbsoluteVirtualBlock, &VirtualBlcokTables[VirtualRegion][VirtualBlock].VPTIndex, TotalRequestTime); //# update caches of virtual page tables
	//UpdateLRUCache(&ParityCheckLRUList, AbsoluteVirtualBlock, &VirtualBlcokTables[VirtualRegion][VirtualBlock].ParityCheckIndex, TotalRequestTime); //# udpate caches of parity-check information
	//# advance the number of referenced count for the cache
	AccessStatistics.TotalVBTReferenceCnt++;
	AccessStatistics.TotalVPTReferenceCnt++;
	AccessStatistics.TotalParityCheckReferenceCnt++;


	//# read the cluster: we only calcualte the time on read the page set
	//# Statistics
	*TotalRequestTime += TIME_TRANSFER_PAGE * (EndChip-StartChip+1) + TIME_READ_PAGE_NO_TRANSFER; //# read one page set


	return True;
}


//* ****************************************************************
//* Simulate the Disposable method with DWL
//* "fp" is the input file descriptor
//* ****************************************************************
void DisposableSimulation(FILE *fp)
{
	int i,j,k;
	int BlockMaxRandomValue;	//* The maximal random value, which allows us to select a block storing initial data
	int run, length, start, end;	//# please refer to the Algorithm 1 in the paper
	Boolean status;
	Boolean WriteStatus=True;	//# keep the result of the write request

	InitializeDisposableFlashMemory(); //# initialize the disposable flash memory
	InitializeTranslationTables();	//# initialize the translation tables and related lists
	InitializeFreeBlockLists();	//# initialize the free block lists for each virtual region
	

	//# *************** Set up the initial data ***************
	//# according the parameter "InitialPercentage" to determine what's the percentage of data in ths flash memory initially.
	BlockMaxRandomValue = (int)((float)InitialPercentage * (RAND_MAX) / 100);	//* The max random number equals "InitialPercentage" percentage of data.
	for(i=0; i<(BlocksPerVirtualRegion* (MaxRegion-1) + BlocksInTheLastVirtualRegion); i++)
	{
		if( rand() <= BlockMaxRandomValue)
		{
			//PageMaxRandomValue = rand();	//* The maximal number of pages storing initial data is from 0 to MaxPagd
			for(j=0;j<PagesPerVirtualBlock;j++)
			{
				//if(rand() <= PageMaxRandomValue)
				{
					DisposableWriteOnePageSet((i*PagesPerVirtualBlock+j)*ChipNumber, 0, ChipNumber-1);

					//# Update the number of accessed logical pages
					AccessedLogicalPages += ChipNumber;	//# advanced the number of set flags in the map
					for(k=0;k<ChipNumber;k++) AccessedLogicalPageMap[(i*PagesPerVirtualBlock+j)*ChipNumber+k] = True; //# Set the flag on
				}
			}
		}
	}
	//# Log information
	printf("Initial Percentage of data: %.02f%%\n",(float)AccessedLogicalPages/(MaxPage*MaxBlock)*100);

	//# reset statictic variables
	ResetStatisticVariable(&AccessStatistics);


	//* ******************************* Start simulation ***************************
	//* dynamic wear leveling: cost-benefit function
	while(GetOneOperation(fp, &CurrentRequest.AccessType, &CurrentRequest.StartCluster, &CurrentRequest.Length))	//* fetch one write operation
	{
		//# Debug
		{

			cnt++;
			//printf("%8d",cnt);
			if(cnt==-1)
			{
				printf("cnt=%d, VPTLRUList.FreeTail=%d VPTLRUList.FreeLen=%d\n",cnt, VPTLRUList.FreeTail,VPTLRUList.FreeLen);
			}
			if(VPTLRUList.FreeTail<0 && VPTLRUList.FreeLen>0)
			{
				printf("cnt=%d, VPTLRUList.FreeTail=%d VPTLRUList.FreeLen=%d\n",cnt, VPTLRUList.FreeTail,VPTLRUList.FreeLen);
			}
		}

		//# each loop handles one virtual page
		while(CurrentRequest.Length > 0)
		{
			run = CurrentRequest.StartCluster % ClustersPerVirtualPage; //# start chip

			//# calculate the number of physical pages to write
			if((ClustersPerVirtualPage-run) < CurrentRequest.Length)
			{
				length = ClustersPerVirtualPage-run;
			}
			else
			{
				length = CurrentRequest.Length;
			}
			start = run;	//# start chip
			end = run + length - 1; //# end chip

			if(CurrentRequest.AccessType == ReadType) //# a read command
			{
				status = DisposableReadOnePageSet(CurrentRequest.StartCluster, start, end,  &AccessStatistics.TotalReadRequestTime);
				if(status = False) break;
			}
			else //# a write command
			{
				status = DisposableReadOnePageSet(CurrentRequest.StartCluster, start, end,  &AccessStatistics.TotalWriteRequestTime);
				if(status == False)
					WriteStatus = DisposableWriteOnePageSet(CurrentRequest.StartCluster, start, end);
				else
					WriteStatus = DisposableWriteOnePageSet(CurrentRequest.StartCluster, 0, ChipNumber-1);

				if(WriteStatus==False) break; //# the simulation ends whenever a write request fails
			}

			//# update StartCluster and Length
			CurrentRequest.StartCluster += length;
			CurrentRequest.Length -= length;

			//# start *******************************************************************************
			//# If any block is going to be worn out, replace it with a spare block
			{
				
				int SourceRegion;
				//static int SpareBlock=-2;
				//if(SpareBlock == -2) SpareBlock = SpareBlocksPerChip-BadBlocksPerChip; //# the least number of free blocks among all chips
				static int SpareBlock=10000000; //# the least number of free blocks among all chips
				static int MaxNewBadBlockRegion=0;
				if(SpareBlock==10000000)
					for(i=0;i<ChipNumber;i++) if(SpareBlockCnt[i]<SpareBlock) SpareBlock=SpareBlockCnt[i];
				
				if(MaxBlockEraseCnt >= (BlockEndurance-2) && SpareBlock>0)
				{
					for(i=0;i<BlocksPerChip;i++)
					{
						if(MaxNewBadBlockRegion<RegionCnt[i/BlocksPerPhysicalRegion].TotalNewBadBlockCnt) MaxNewBadBlockRegion=RegionCnt[i/BlocksPerPhysicalRegion].TotalNewBadBlockCnt;//Get the one with the largest number of bad blocks

						//# a to-be worn-out block is found -> replace it
						if(BlockEraseCnt[i] >= (BlockEndurance-2) && SpareBlockCnt[0]>0 )
						{
							for(j=0;j<ChipNumber;j++) SpareBlockCnt[j]--;
							SpareBlock--;
							BlockEraseCnt[i]=0;
							//printf(" block %d in region %d, spareblock=%d\n", i,i/BlocksPerPhysicalRegion,SpareBlock);
							//# check whether we need to swap physical regions
							SourceRegion = i/BlocksPerPhysicalRegion;
							RegionCnt[SourceRegion].TotalNewBadBlockCnt++;

							//if(RegionCnt[SourceRegion].TotalNewBadBlockCnt % RegionSwappingThreshold == 0 || (RegionCnt[SourceRegion].TotalNewBadBlockCnt>=MaxNewBadBlockRegion && RegionCnt[SourceRegion].TotalNewBadBlockCnt>=RegionSwappingThreshold))
							if(RegionCnt[SourceRegion].TotalNewBadBlockCnt % RegionSwappingThreshold == 0)
							{
								int MinNewBadBlock = 1000000;
								flash_huge_size_t MinBlockErases;
								int MinNewBadBlockRegion;
								Boolean SwapFlag;

								SwapFlag = False; 
								//printf("\n");
								for(j=0;j<MaxRegion;j++) //# search the region that has the least bad block cnt
								{
									//printf("%3d:%4d%8d",j,RegionCnt[j].TotalNewBadBlockCnt,RegionCnt[j].TotalBlockEraseCnt);
									//# search the region that has the least number of bad blocks and least number of block erases
									if(j != (SourceRegion) && MinNewBadBlock >= RegionCnt[j].TotalNewBadBlockCnt)
									{
										if(MinNewBadBlock == RegionCnt[j].TotalNewBadBlockCnt) 
										{	//# equal number of bad blocks so that we compare the block erase count
											if(MinBlockErases > RegionCnt[j].TotalBlockEraseCnt)
											{
												MinNewBadBlock = RegionCnt[j].TotalNewBadBlockCnt;
												MinBlockErases = RegionCnt[j].TotalBlockEraseCnt;
												MinNewBadBlockRegion = j;
												//RegionCnt[SourceRegion].SwappedFlag = True;
												SwapFlag = True;
											}
										}
										else
										{
											MinNewBadBlock = RegionCnt[j].TotalNewBadBlockCnt;
											MinBlockErases = RegionCnt[j].TotalBlockEraseCnt;
											MinNewBadBlockRegion = j;
											//RegionCnt[SourceRegion].SwappedFlag = True;
											SwapFlag = True;
										}
									}
								}
								//printf("\n");
								//# found 
								{
									int tmp;
									RegionCnt_t regiontmp;
									static SwapCounter=1;
									if(SwapFlag == True)
									{
										//# swap the physical region by swapping their erase count and 
										for(j=0;j<BlocksPerPhysicalRegion;j++)
										{
											tmp = BlockEraseCnt[SourceRegion*BlocksPerPhysicalRegion+BlocksPerPhysicalRegion-j-1];
											BlockEraseCnt[SourceRegion*BlocksPerPhysicalRegion+BlocksPerPhysicalRegion-j-1] = BlockEraseCnt[MinNewBadBlockRegion*BlocksPerPhysicalRegion+j];
											BlockEraseCnt[MinNewBadBlockRegion*BlocksPerPhysicalRegion+j] = tmp;
										}
										regiontmp = RegionCnt[SourceRegion];
										RegionCnt[SourceRegion] = RegionCnt[MinNewBadBlockRegion];
										RegionCnt[MinNewBadBlockRegion] = regiontmp;
										printf("\n%d: Region %d and %d are swapped (SpareBlock=%d)\n", SwapCounter++,SourceRegion, MinNewBadBlockRegion,SpareBlock);
									}
									
								}

							} // end of if(RegionCnt[SourceRegion].TotalNewBadBlockCnt % RegionSwappingThreshold == 0)
						} //# end of for i
					}
					//# find the maximual block erase count
					MaxBlockEraseCnt = 0;
					for(i=0;i<BlocksPerChip;i++)
					{
						if(BlockEraseCnt[i] > MaxBlockEraseCnt) MaxBlockEraseCnt = BlockEraseCnt[i];
					}
				} //# end of if
			}
			//# end *******************************************************************************

		}


		//# Statistics
		if(CurrentRequest.Length <= 0) //# if the request is a legal command
		{
			if(CurrentRequest.AccessType == WriteType) AccessStatistics.TotalWriteRequestCount ++;	//# advance the number of total write requests
			if(CurrentRequest.AccessType == ReadType) AccessStatistics.TotalReadRequestCount ++;	//# advance the number of total read requests
		}
		
		if(WriteStatus==False) break; //# the simulation ends whenever a write request fails

	}	//* end of while
	//* ******************************* End of simulation *****************************


	FinalizeTranslationTables();	//# release the memory space allocated for translation tables
	FinalizeFreeBlockLists();	//# release the memory space allocated for free block lists

}

