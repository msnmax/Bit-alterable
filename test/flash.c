//* All the flash related opeartion are defined in this file

#ifndef _FLASH_C
#define _FLASH_C
#endif


#include "main.h"
#include "flash.h"


//* *********************** Global variable *************************
flash_size_t FlashSize;			//# the size of the flash-memory storage system
flash_size_t ChipNumber;		//# the number of flash-memory chips
flash_size_t ChipSize;		//# the size of one flash-memory chip(unit:MB)
flash_size_t BlockSize;		//# the size of a block(unit:KB)
flash_size_t BlocksPerChip;	//# the number of block per chip
flash_size_t PageSize;		//# the size of the data area of a page (unit:B)
flash_size_t PageSpareSize;	//# the size of the spare area of a page (unit:B)
flash_size_t PageToLBARatio;	//# the number of sectors that can be stored in one page
flash_size_t MaxLBA;			//# the maximum LBA of the flash memory
flash_size_t MaxCluster;		//# Cluster size = Page size, so that MaxCluster = MaxLBA / PageToLBARatio
flash_size_t MaxBlock;		//# the maximum number of Block
flash_size_t MaxPage;		//# the number of pages in one block
flash_size_t BadBlocksPerChip;	//# the number of bad blocks in a chip
flash_size_t EraseBlockTime;	//# the time to erase a block (unit: ns)
flash_size_t WritePageTime;		//# the time to write a page (unit: ns)
flash_size_t ReadPageTime;		//# the time to read a page (unit: ns)
flash_size_t ErasePageTime;		//# the time to erase a page (unit: ns)
flash_size_t SerialAccessTime;	//# the time to transfer one-byte data between RAM and flash chip (unit: ns)
flash_size_t AccessSRAM;		//# the time to access one instruction from SRAM (unit: ns)
flash_size_t VBTCacheLength;	//# the length of the LRU list to cache Virtual Block Tables, block remapping list, block remapping table, and list of free block sets: only for DISPOSABLE
flash_size_t VPTCacheLength;	//# the length of the LRU list to cache Virtual Page Tables: only for DISPOSABLE
flash_size_t ParityCheckCacheLength;	//# the length of the LRU list to cache parity-check information: only for DISPOSABLE

flash_size_t *FreeBlockList;	//* the free block list array
//static flash_size_t FreeBlockListHead;	//* The index of the first free block
flash_size_t FreeBlockListHead;	//* The index of the first free block
static flash_size_t FreeBlockListTail;	//* The index of the last free block
//static flash_size_t BlocksInFreeBlockList;	//* The number of free blocks in the free block list
flash_size_t BlocksInFreeBlockList;	//* The number of free blocks in the free block list
//static flash_size_t *BlocksInFreeBlockListArray;	//# Keep track of the number of free block (i.e., the number of blocks in the free block list) of each chip
flash_size_t *BlocksInFreeBlockListArray;	//# Keep track of the number of free block (i.e., the number of blocks in the free block list) of each chip
static flash_size_t LeastBlocksInFreeBlockList;	//* The minimum number of free blocks that should be reserved in the free block list

flash_size_t **FlashMemory;	//* FlashMemory[Block_ID][Page_ID]
flash_size_t ***FlashMemoryArray;	//# FlashMemory[Chip_ID][Block_ID][Page_ID] (only for DISPOSABLE management scheme) where Block_ID is the offset inside its own chip
flash_size_t BadBlockCnt=0;	//# the counter to count the number of bad blocks in the flash memory
flash_size_t *BadBlockCntArray;	//# the counters to count the number of bad blocks in each flash-memory chip (only for DISPOSABLE management scheme)
flash_size_t MaxBlockEraseCnt=0;	//# The max block erase count among blocks in the flash memory
flash_size_t MaxPageEraseCnt;
flash_size_t *BlockEraseCnt;	//* BlockEraseCnt[Block_ID] restores the erase count of each block
flash_size_t *ChipEraseCnt;		//# Keep the total number of block erase in each chip
flash_size_t *InvalidPageCnt;	//* store the number of invalid pages in each block
flash_size_t *ValidPageCnt;	//* store the number of valid pages in each block
flash_size_t *FreePageCnt;
flash_size_t *WatchPageCnt;
flash_size_t AccessedLogicalPages;	//# Accumulate the total logical pages being accessed
flash_size_t GCTriggerSpace;
flash_size_t **PageEraseCnt; // PageEraseCnt[Page_ID] restores the erase count of each page
flash_size_t **MSB; //Most Significant Bit for erase count
flash_size_t *BlockMSB; //Block MSB qty.
flash_size_t *BlockGCTime; //each block last GC time
flash_size_t *BlockType;
flash_size_t AvgTime; //avgtime for find datamove block

Boolean *AccessedLogicalPageMap;	//# Point to the map array

//# statistics
Statistics_t AccessStatistics={0};



//* ******************************** Functions ******************************


//* ****************************************************************
//* Set up the least free blocks guaranteed to be in the free block list
//# "LeastFreeBlocks" is the least number of blocks in the free block list
//* ****************************************************************
static void SetLeastBlocksInFBL(int LeastFreeBlocks)
{
	//flash_size_t i;

	LeastBlocksInFreeBlockList = LeastFreeBlocks;
	//for(i=0;i<MappingModeK+2;i++) LeastBlocksInFreeBlockList *= 2;

}

//* ****************************************************************
//* The least free blocks guaranteed to be in the free block list
//* return value is the minumum number of blocks that should be reserved in the free block list
//* ****************************************************************
flash_size_t LeastFreeBlocksInFBL(void)
{
	return (LeastBlocksInFreeBlockList);
}




//* ****************************************************************
//* initialize the flash memory related information
//* ****************************************************************
void InitializeFlashMemory(void)
{
	flash_size_t i;
	flash_size_t *RandomFreeBlockOrder;	//* use to store the random order list of blocks
	ui SelectedItem;					//* The selected item to switch during the switching procedure
	flash_size_t RandomValue;			//* The value inside the selected item.
	int BadBlockThreshold;				//# The threshold value to mark a block as invalid
	SetLeastBlocksInFBL(64*ChipNumber);	//* Setup the least number of free blocks in the FBL

	//# physical address settings
	BlocksPerChip = ChipSize * 1024 / BlockSize;	//# the number of blocks in a chip
	MaxBlock = FlashSize *1024 / BlockSize;	//* the number of blocks in the flash memory
	MaxPage = BlockSize * 1024 / PageSize;	//* the nubmer of pages in a block

	//srand(time((time_t *)&i));	//* Set a random starting point
	srand(0);	//* Set a random starting point

	//* Allocate the content of flash memory
	FlashMemory = (flash_size_t **)malloc(sizeof(flash_size_t *)*MaxBlock);
	for(i=0; i<MaxBlock; i++)
	{
		FlashMemory[i] = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxPage);
		memset(FlashMemory[i],FREE_PAGE,sizeof(flash_size_t)*MaxPage);	//* initialize the content of each page: means there is nothing in any page initially.
	}
	//# Allocate the flash memory array (only for DISPOSABLE management scheme)
	FlashMemoryArray = (flash_size_t ***)malloc(sizeof(flash_size_t **)*ChipNumber);
	for(i=0; i<ChipNumber; i++)
	{
		FlashMemoryArray[i] = &FlashMemory[i*BlocksPerChip]; //# point to the starting block of each chip
	}

	RandomFreeBlockOrder = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate the memory space for the random block order
	for(i=0; i<MaxBlock; i++) RandomFreeBlockOrder[i] = i;	//* Assign an integer for each block in an increasing order
	//* Generate random sequence according to the increasing integer list
	for(i=MaxBlock; i>0; i--)
	{
		SelectedItem = ((ui)rand()<<16 | (ui)rand()) % (ui)i;
		//* switch values in the selected two blocks
		RandomValue = RandomFreeBlockOrder[SelectedItem];
		RandomFreeBlockOrder[SelectedItem] = RandomFreeBlockOrder[i-1];
		RandomFreeBlockOrder[i-1] = RandomValue;
	}

	//# (only for DISPOSABLE management scheme)
	BadBlockCntArray = (flash_size_t *)malloc(sizeof(flash_size_t)*ChipNumber);
	memset(BadBlockCntArray, 0, sizeof(flash_size_t)*ChipNumber);


	//# randomly mark some blocks as invalid blicks
	BadBlockThreshold = ((int)BadBlocksPerChip*RAND_MAX) / BlocksPerChip;

	//# Mark some blocks as bad blocks
	for(i=1; i<(MaxBlock-1); i++) //# the first and last block in the RandomFreeBlockOrder[] should be a good block
	{
		//if(rand() < BadBlockThreshold)
		if((RandomFreeBlockOrder[i] % BlocksPerChip)<BadBlocksPerChip)
		{
			FlashMemory[RandomFreeBlockOrder[i]][0] = INVALID_BLOCK; //# mark an invalid block
			BadBlockCntArray[RandomFreeBlockOrder[i] / BlocksPerChip]++;	//# advance the number of bad blocks in the corresponding chip
			BadBlockCnt++;	//# advance the number of bad blocks in total
		}
	}

	//* initialize the free block list
	FreeBlockList = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate the memory space for the free block list
	FreeBlockListHead = RandomFreeBlockOrder[0];	//* Initialize the index of the first free block
	//# set the order of free blocks in the list
	for(i=0; i < (MaxBlock-1) ; i++)
	{
		int current_i;

		current_i = i; //# keep current i
		while(FlashMemory[RandomFreeBlockOrder[i+1]][0] == INVALID_BLOCK && i<(MaxBlock-1)) i++;	//# skip invalid block
		FreeBlockList[RandomFreeBlockOrder[current_i]] = RandomFreeBlockOrder[i+1];	//* the initial free block list: Block 0 points to Block 1, Block 1 points to Block 2, and so on.
	}
	FreeBlockList[RandomFreeBlockOrder[i]] = FBL_POINT_TO_NULL;	//* the last free block points to NULL.
	FreeBlockListTail = RandomFreeBlockOrder[i];		//* the index of the last free block
	//* free the memory space of random block list
	free(RandomFreeBlockOrder);
	//* set the initial number of free blocks in the free block list
	BlocksInFreeBlockList = MaxBlock-BadBlockCnt;

	//# for DISPOSABLE and STRIPE
	BlocksInFreeBlockListArray = (flash_size_t *)malloc(sizeof(flash_size_t)*ChipNumber);
	for(i=0;i<ChipNumber;i++) BlocksInFreeBlockListArray[i] = BlocksPerChip - BadBlockCntArray[i];


	//# logical address settings
	PageToLBARatio = PageSize / LBA_SIZE;	//* the number of sectors can be stored in a page
	SetMaxLBA(FlashSize * 1024 * 2-(LeastFreeBlocksInFBL()+BadBlockCnt)*BlockSize*2);	//* the number of sectors in the flash memory
	//MaxLBA = FlashSize * 1024 * 2-(LeastFreeBlocksInFBL()+BadBlockCnt)*BlockSize*2;
	MaxCluster = MaxLBA / PageToLBARatio;	//* the number of Clusters in the flash memory, 1 Cluster size = PageToLBARatio * 1 cluster size

	//* Allocate memory space for the erase count of each block
	BlockEraseCnt = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate memory space
	memset(BlockEraseCnt,0,sizeof(flash_size_t)*MaxBlock);			//* set initial erase count as 0

	//* Allocate memory space for the erase count of each block (for STRIPE)
	ChipEraseCnt = (flash_size_t *)malloc(sizeof(flash_size_t)*ChipNumber);	//* allocate memory space
	memset(ChipEraseCnt,0,sizeof(flash_size_t)*ChipNumber);			//* set initial erase count of each chip as 0



	//* Allocate memory space to keep the number of invalid pages in each block
	InvalidPageCnt = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate memory space for InvalidPageCnt
	memset(InvalidPageCnt,0, sizeof(flash_size_t)*MaxBlock);			//* clear counters in the InvalidPageCnt
	//* Allocate memory space to keep the number of valid pages in each block
	ValidPageCnt = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate memory space for ValidPageCnt
	memset(ValidPageCnt,0, sizeof(flash_size_t)*MaxBlock);			//* clear counters in the ValidPageCnt

 	FreePageCnt = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate memory space for ValidPageCnt
	memset(FreePageCnt,0, sizeof(flash_size_t)*MaxBlock);			//* clear counters in the ValidPageCnt
    //* Allocate memory space for the erase count of each page
 	WatchPageCnt = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate memory space for ValidPageCnt
	memset(WatchPageCnt,0, sizeof(flash_size_t)*MaxBlock);			//* clear counters in the ValidPageCnt



    MSB = (flash_size_t **)malloc(sizeof(flash_size_t *)*MaxBlock);
	for(i=0; i<MaxBlock; i++)
	{
		MSB[i] = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxPage);
		memset(MSB[i],0,sizeof(flash_size_t)*MaxPage);	//* initialize the content of each page: means there is nothing in any page initially.
	}

    PageEraseCnt = (flash_size_t **)malloc(sizeof(flash_size_t *)*MaxBlock);
	for(i=0; i<MaxBlock; i++)
	{
		PageEraseCnt[i] = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxPage);
		memset(PageEraseCnt[i],0,sizeof(flash_size_t)*MaxPage);	//* initialize the content of each page: means there is nothing in any page initially.
	}



    BlockMSB = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate memory space
    BlockGCTime = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate memory space
    BlockType = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);
	memset(BlockMSB,0,sizeof(flash_size_t)*MaxBlock);

	ResetStatisticVariable(&AccessStatistics);


	//# Initialize total number of accessed pages
	AccessedLogicalPages = 0;
	AccessedLogicalPageMap = (Boolean *)malloc(sizeof(Boolean)*MaxPage*MaxBlock);
	memset(AccessedLogicalPageMap,0,sizeof(Boolean)*MaxPage*MaxBlock);
}

//* ****************************************************************
//* release the data structure for flash memory
//* ****************************************************************
void FinalizeFlashMemory(void)
{
	flash_size_t i;

	//* Free the content of flash memory
	for(i=0; i<MaxBlock; i++)
		free(FlashMemory[i]);
	free(FlashMemory);
	//# free the pointers to flash-memory chips (only for DISPOSABLE management scheme)
	free(FlashMemoryArray);

	free(FreeBlockList);	//* free the free block list

	//# free bad block counters(only for DISPOSABLE management scheme)
	free(BadBlockCntArray);
	//# free the array that records the number of free blocks of each chip
	free(BlocksInFreeBlockListArray);

	//* Free the block erase count
	free(BlockEraseCnt);
	//# Free the total block erase count of each chip
	free(ChipEraseCnt);

	//* Free the InvalidPageCnt
	free(InvalidPageCnt);
	//* Free the ValidPageCnt
	free(ValidPageCnt);

	free(AccessedLogicalPageMap);
    	//* Free the block erase count
	free(PageEraseCnt);
    free(MSB);
}


//* ****************************************************************
//* get one free block from the free block list
//* There is always some free blocks in the free block list
//* return the free block id on suceess. Return -1 when failed to find a free block
//* ****************************************************************
flash_size_t GetFreeBlock(void)
{
	flash_size_t block;

	if(BlocksInFreeBlockList > LeastFreeBlocksInFBL())	//* There is always more some free blocks in the free block list
	{
		block = FreeBlockListHead;	//* fetch the first free block in the free block list
		FreeBlockListHead = FreeBlockList[block];	//* move head to next free block
		FreeBlockList[block] = FBL_NOT_IN_LIST;	//* Remove this block from the free block list and mark is as a block not in the free block list

		if(block>MaxBlock || block<0)
		{
			printf("ERROR: A block ID %d returned from the free blocks list\n",block);
			exit(1);
		}

		BlocksInFreeBlockList--;	//* Decrease the number of free blocks in the free block list
		BlocksInFreeBlockListArray[block/BlocksPerChip]--;	//* Decrease the number of free blocks in the chip

		return block;
	}

	return(-1);		//* Failed to find a free block
}


//* ****************************************************************
//* get one free block from one specific chip without checking whether there is enough free blocks in the list
//# "chip" indicates the ID of the chip, from which we want to get a free block.
//* return the free block id
//* ****************************************************************
flash_size_t GetFreeBlockFromChipWithoutChecking(int chip)
{
	flash_size_t block;
	int cnt=0;
	int checklist=0;


	while(1) //# loop until we find a free block in the assigned "chip"
	{
		cnt++;

		block = FreeBlockListHead;	//* fetch the first free block in the free block list
		FreeBlockListHead = FreeBlockList[block];	//* move head to next free block
		FreeBlockList[block] = FBL_NOT_IN_LIST;	//* Remove this block from the free block list and mark is as a block not in the free block list

		if(block>MaxBlock || block<0 || cnt > MaxBlock)
		{
			printf("ERROR: A block ID %d returned from the free blocks list\n",block);
			exit(1);
		}

		BlocksInFreeBlockList--;	//* Decrease the number of free blocks in the free block list
		BlocksInFreeBlockListArray[block/BlocksPerChip]--;	//* Decrease the number of free blocks in the chip

		//# if the allocated free block "block" is not in the designated chip, put it back; othereise, return it.
		if(block/BlocksPerChip != chip)
			PutFreeBlock(block);
		else
			break;
	}

	return block;
}


//* ****************************************************************
//* get one free block from one specific chip
//* There is always some free blocks in the free block list
//# "chip" indicates the ID of the chip, from which we want to get a free block.
//* return the free block id on suceess. Return -1 when failed to find a free block
//* ****************************************************************
flash_size_t GetFreeBlockFromChip(int chip)
{


	if(BlocksInFreeBlockListArray[chip] > LeastFreeBlocksInFBL()/ChipNumber)	//* There is always more some free blocks in the free block list in each chip
	{
		return(GetFreeBlockFromChipWithoutChecking(chip));
	}

	return(-1);		//* Failed to find a free block
}


//* ****************************************************************
//* Check the status of the free block list
//* There is some free blocks in the free block list
//* When there is more than the minimum number of free blocks, return the ID of the first block in the list.
//* Otherwise return -1.
//* ****************************************************************
flash_size_t CheckFreeBlock(void)
{
	if(BlocksInFreeBlockList > LeastFreeBlocksInFBL())	//* There is always more than 2^k blocks in the free block list
		return FreeBlockListHead;
	else
		return -1;
}


//* ****************************************************************
//* Check the status of the free block list
//* There is some free blocks in the free block list
//# "chip" indicates the ID of the chip, from which we want to get a free block.
//* When there is more than the minimum number of free blocks, return the ID of the first block in the list.
//* Otherwise return -1.
//* ****************************************************************
flash_size_t CheckFreeBlockFromChip(flash_size_t chip)
{
	if(BlocksInFreeBlockListArray[chip] > LeastFreeBlocksInFBL()/ChipNumber)	//* There is always more than the minimum number of free blocks
	{
		return FreeBlockListHead;
	}
	else
	{
		return -1;
	}
}


//* ****************************************************************
//* Put one block into the free block list
//* "BlockID" is block ID of the physical block that is going to be insert into the free block list
//* ****************************************************************
void PutFreeBlock(flash_size_t BlockID)
{

	//* Add the primary block into the free block list
	FreeBlockList[FreeBlockListTail] = BlockID;
	FreeBlockListTail = BlockID;
	FreeBlockList[FreeBlockListTail] = FBL_POINT_TO_NULL;
	BlocksInFreeBlockList++;	//* advance the counter of the number of freeblocks
	BlocksInFreeBlockListArray[BlockID/BlocksPerChip]++;	//* Increase the number of free blocks in the chip

}

//# ****************************************************************
//# Set the maximal value of LBA
//# "max" is the value of MaxLBA
//# Return: no return value
//# ****************************************************************
void SetMaxLBA(int max)
{
	MaxLBA = max;
	MaxCluster = MaxLBA / PageToLBARatio;

}

//# ****************************************************************
//# return the maximal value of LBA
//# Return: no return value
//# ****************************************************************
flash_size_t GetMaxLBA(void)
{
	return MaxLBA;
}

//# ****************************************************************
//# reset statistic variables
//# "statistic" is the variable to store the statistic information
//# Return: no return value
//# ****************************************************************
void ResetStatisticVariable(Statistics_t *statistic)
{
	memset(statistic,0,sizeof(Statistics_t));
}
