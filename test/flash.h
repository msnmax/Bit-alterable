#ifndef _FLASH_H
#define _FLASH_H

#include "typedefine.h"


//# ********************* data structure **************************


//# statistics
typedef struct
{
	flash_huge_size_t TotalBlockEraseCount;		//# calculate total number of block erases
	flash_huge_size_t TotalPageEraseCount;
	flash_huge_size_t TotalLivePageCopyings;	//# calculate total number of live-page copyings
	flash_huge_size_t TotalWriteRequestCount;	//# Keep the total number of write requests from the host
	flash_huge_size_t TotalReadRequestCount;	//# Keep the total number of read requests from the host
	flash_huge_size_t TotalWriteRequestTime;	//# Keep the total elapsed time for write requests form the host (unit: ns)
	flash_huge_size_t TotalReadRequestTime;		//# Keep the total elapsed time for read requests form the host (unit: ns)
	flash_huge_size_t TotalVBTReferenceCnt;		//# (only for DISPOSABLE) Kepp the number of referencing the cache for the virtual block table
	flash_huge_size_t TotalVPTReferenceCnt;		//# (only for DISPOSABLE) Kepp the number of referencing the cache for the virtual page table
	flash_huge_size_t TotalParityCheckReferenceCnt;		//# (only for DISPOSABLE) Kepp the number of referencing the cache for the parity check information
	flash_huge_size_t TotalVBTMissCnt;		//# (only for DISPOSABLE) Kepp the number of the cache miss of the virtual block table
	flash_huge_size_t TotalVPTMissCnt;		//# (only for DISPOSABLE) Kepp the number of the cache miss of the virtual page table
	flash_huge_size_t TotalParityCheckMissCnt;		//# (only for DISPOSABLE) Kepp the number of the cache miss of the parity check information
} Statistics_t;


//* *********************** Global variable *************************
extern flash_size_t FlashSize;		//# the size of the flash-memory storage system
extern flash_size_t ChipNumber;		//* the number of flash-memory chips
extern flash_size_t ChipSize;		//* the size of one flash-memory chip(unit:MB)
extern flash_size_t BlockSize;		//* the size of a block(unit:KB)
extern flash_size_t BlocksPerChip;	//# the number of block per chip
extern flash_size_t PageSize;		//* the size of the data area of a page (unit:B)
extern flash_size_t PageSpareSize;	//# the size of the spare area of a page (unit:B)
extern flash_size_t PageToLBARatio;	//* the number of sectors that can be stored in one page
extern flash_size_t MaxLBA;			//* the maximum LBA of the flash memory
extern flash_size_t MaxCluster;		//* Cluster size = Page size, so that MaxCluster = MaxLBA / PageToLBARatio
extern flash_size_t MaxBlock;		//* the maximum number of Block
extern flash_size_t MaxPage;		//* the number of pages in one block
extern flash_size_t BadBlocksPerChip;	//# the number of bad blocks in a chip
extern flash_size_t EraseBlockTime;	//# the time to erase a block (unit: ns)
extern flash_size_t WritePageTime;		//# the time to write a page (unit: ns)
extern flash_size_t ReadPageTime;		//# the time to read a page (unit: ns)
extern flash_size_t ErasePageTime;      //# the time to erase a page (unit: ns)
extern flash_size_t SerialAccessTime;	//# the time to transfer one-byte data between RAM and flash chip (unit: ns)
extern flash_size_t AccessSRAM;		//# the time to access one instruction from SRAM (unit: ns)
extern flash_size_t VBTCacheLength;	//# the length of the LRU list to cache Virtual Block Tables, block remapping list, block remapping table, and list of free block sets: only for DISPOSABLE
extern flash_size_t VPTCacheLength;	//# the length of the LRU list to cache Virtual Page Tables: only for DISPOSABLE
extern flash_size_t ParityCheckCacheLength;	//# the length of the LRU list to cache parity-check information: only for DISPOSABLE
extern flash_size_t GCTriggerSpace;

extern flash_size_t *FreeBlockList;	//* the free block list array

extern flash_size_t **FlashMemory;	//* FlashMemory[Block_ID][Page_ID]
extern flash_size_t ***FlashMemoryArray;	//# FlashMemory[Chip_ID][Block_ID][Page_ID] (only for DISPOSABLE management scheme) where Block_ID is the offset inside its own chip
extern flash_size_t BadBlockCnt;	//# the counter to count the number of bad blocks in the flash memory
extern flash_size_t *BadBlockCntArray;	//# the counters to count the number of bad blocks in each flash-memory chip (only for DISPOSABLE management scheme)

extern flash_size_t MaxBlockEraseCnt;	//# The max block erase count among blocks in the flash memory
extern flash_size_t MaxPageEraseCnt;
extern flash_size_t *BlockEraseCnt;	//* BlockEraseCnt[Block_ID] restores the erase count of each block
extern flash_size_t *InvalidPageCnt;	//* store the number of invalide pages in each block
extern flash_size_t *ValidPageCnt;	//* store the number of valid pages in each block
extern flash_size_t *FreePageCnt;
extern flash_size_t *WatchPageCnt;
extern flash_size_t **PageEraseCnt;
extern flash_size_t **MSB;
extern flash_size_t *BlockMSB;
extern flash_size_t *BlockGCTime;
extern flash_size_t *BlockType;
extern flash_size_t AvgTime;


extern flash_size_t AccessedLogicalPages;	//# Accumulate the total logical pages being accessed
extern Boolean *AccessedLogicalPageMap;	//# Point to the map array


//# statistics
extern Statistics_t AccessStatistics;


extern flash_size_t FreeBlockListHead;	//* The index of the first free block
extern flash_size_t BlocksInFreeBlockList;	//* The minimum number of free blocks that should be reserved in the free block list
extern flash_size_t *BlocksInFreeBlockListArray;	//# Keep track of the number of free block (i.e., the number of blocks in the free block list) of each chip

//* ************************* MACROs ***************

//* Used in FreeBlockList
#define FBL_POINT_TO_NULL		(-1)		//* POINT_TO_NULL means NULL or points to nothing, or there is nothing assigned.
											//* only the last block in the free block list would be filled with this value
#define FBL_NOT_IN_LIST			(-2)		//* mean this block is not in the free block list



//* page status for FlashMemory[BlockID][PageID]
#define FREE_PAGE		(-1)		//* Mark as a free page

#define INVALID_PAGE	(-2)		//* Mark as an invalid page
#define INVALID_BLOCK	(-3)		//# Mark as an invalid block, in which the page resides. A block is marked as invalid whenever its first page marks the block as invalid

//# access time calculation
#define TIME_TRANSFER_PAGE			(SerialAccessTime * (PageSpareSize+PageSize))	//# the time to click a page data in or out
#define TIME_TRANSFER_SPARE_PAGE	(SerialAccessTime * PageSpareSize)				//# the time to click the spare area of a page in or out
#define TIME_READ_PAGE				(ReadPageTime +  TIME_TRANSFER_PAGE)			//# the time to read a page, including the data transfer time
#define TIME_READ_PAGE_NO_TRANSFER	(ReadPageTime)									//# the time to read a page, excluding the data transfer time
#define TIME_READ_SPARE_PAGE		(ReadPageTime +  TIME_TRANSFER_SPARE_PAGE)		//# the time to read the spare area of a page, including the data transfer time
#define TIME_WRITE_PAGE				(WritePageTime +  TIME_TRANSFER_PAGE)			//# the time to write a page, including the data transfer time
#define TIME_WRITE_PAGE_NO_TRANSFER	(WritePageTime)									//# the time to write a page, excluding the data transfer time
#define TIME_ERASE_BLOCK			(EraseBlockTime)								//# the time to erase a block
#define TIME_ERASE_PAGE             (ErasePageTime)
#define TOTALPAGE                   (MaxPage)
#define THRESHOLD                   (TOTALPAGE * TIME_ERASE_PAGE - TIME_ERASE_BLOCK)/(TIME_ERASE_PAGE + TIME_WRITE_PAGE_NO_TRANSFER)
#define TIME_ACCESS_SRAM			(AccessSRAM)				//# the time to access one bye of SRAM


//* ****************************************************************
//* initialize the flash memory related information
//* ****************************************************************
void InitializeFlashMemory(void);

//* ****************************************************************
//* release the data structure for flash memory
//* ****************************************************************
void FinalizeFlashMemory(void);

//* ****************************************************************
//* get one free block from the free block list
//* return the free block id on suceess. Return -1 when failed to find a free block
//* ****************************************************************
flash_size_t GetFreeBlock(void);


//* ****************************************************************
//* get one free block from one specific chip without checking whether there is enough free blocks in the list
//# "chip" indicates the ID of the chip, from which we want to get a free block.
//* return the free block id
//* ****************************************************************
flash_size_t GetFreeBlockFromChipWithoutChecking(int chip);


//* ****************************************************************
//* get one free block from one specific chip
//* There is always some free blocks in the free block list
//# "chip" indicates the ID of the chip, from which we want to get a free block.
//* return the free block id on suceess. Return -1 when failed to find a free block
//* ****************************************************************
flash_size_t GetFreeBlockFromChip(int chip);

//* ****************************************************************
//* Check the status of the free block list
//* There is some free blocks in the free block list
//* When there is more than the minimum number of free blocks, return the ID of the first block in the list.
//* Otherwise return -1.
//* ****************************************************************
flash_size_t CheckFreeBlock(void);

//* ****************************************************************
//* Check the status of the free block list
//* There is some free blocks in the free block list
//# "chip" indicates the ID of the chip, from which we want to get a free block.
//* When there is more than the minimum number of free blocks, return the ID of the first block in the list.
//* Otherwise return -1.
//* ****************************************************************
flash_size_t CheckFreeBlockFromChip(flash_size_t chip);


//* ****************************************************************
//* Put one block into the free block list
//* "BlockID" is block ID of the physical block that is going to be insert into the free block list
//* ****************************************************************
void PutFreeBlock(flash_size_t BlockID);

//* ****************************************************************
//* The least free blocks guaranteed to be in the free block list
//* return value is the minumum number of blocks that should be reserved in the free block list
//* ****************************************************************
flash_size_t LeastFreeBlocksInFBL(void);

//# ****************************************************************
//# Set the maximal value of LBA
//# "max" is the value of MaxLBA
//# Return: no return value
//# ****************************************************************
void SetMaxLBA(int max);

//# ****************************************************************
//# return the maximal value of LBA
//# Return: no return value
//# ****************************************************************
flash_size_t GetMaxLBA(void);

//# ****************************************************************
//# reset statistic variables
//# "statistic" is the variable to store the statistic information
//# Return: no return value
//# ****************************************************************
void ResetStatisticVariable(Statistics_t *statistic);


#endif

