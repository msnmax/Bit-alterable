//* _DISPOSABLE_h : All the simulation for Disposable FTL are defined in this file
//* dynamic wear leveling: using cost-benefit function for its GC


#ifndef _DISPOSABLE_H
#define _DISPOSABLE_H


#include "typedefine.h"

//* ************************* MACROs ***************

//* Used in NFTLTable
#define DISPOSABLE_POINT_TO_NULL	(-1)		//* point to NULL 


//* define the data structure of the translation tables of DIPOSABLE management scheme

typedef enum
{
	NewPhysicalBlockSet,
	OldPhysicalBlockSet
} PhysicalBlockSet_t;

typedef struct
{
	flash_size_t VBTIndex; //# Index which element of the LRU list caches the corresponding VBT 
	flash_size_t CurrentScannedVirtualBlock;	//# index the curretn virutla block that is going to be scanned during the garbage collection
} VRTElement_t;

typedef struct
{
	flash_size_t NewPhysicalBlockSet;		//# Point to the new physical block set of the virtual block
	flash_size_t CurrentFreePageSet;		//# The current free page set in the NewPhysicalBlockSet
	flash_size_t OldPhysicalBlockSet;		//# Point to the old physical block set of the virtual block
	flash_size_t VPTIndex;					//# Index which element of the LRU list caches the corresponding VPT 
	flash_size_t ParityCheckIndex;			//# Index which element of the LRU list caches the corresponding parity-check information
} VBTElement_t;

typedef struct
{
	flash_size_t BlockSet;		//# To indicate whcih physical physical block set stores the data
	flash_size_t PageSet;					//# To indicate which physical page set stores the data
} VPTElement_t;

typedef struct	//# the data structure for the free block lists, where each physical region has its own free block list
{
	flash_size_t *PhysicalBlockList;	//# the physical block array
	flash_size_t Head;			//# head of the list
	flash_size_t Tail;			//# tail of the list
	flash_size_t Len;			//# number of elements in the list
} FreeBlockList_t;

typedef struct //# the data structure for the total block erase count and total new developed bad blocks in a physical region
{
	flash_huge_size_t TotalBlockEraseCnt;
	flash_size_t TotalNewBadBlockCnt;
	Boolean SwappedFlag;
} RegionCnt_t;


extern LRUList_t VPTLRUList;		//# The LRU list for caching virtual page tables

//* ****************************************************************
//* Simulate the Disposable method with DWL
//* "fp" is the input file descriptor
//* ****************************************************************
void DisposableSimulation(FILE *fp);
















#endif

