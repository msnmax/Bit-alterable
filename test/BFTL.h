//* FTL.c : All the simulation for FTL are defined in this file
//* dynamic wear leveling: using best fit strategy for its GC
//* static wear leveling: activated when the BlockErase_count/flag_count > SWLThreshold "T"

#ifndef _BFTL_H
#define _BFTL_H

#include "typedefine.h"

//* ************************* MACROs ***************

//* Used in BFTLTable
#define BFTL_POINT_TO_NULL	(-1)		//* point to NULL
#define NULL	(-1)

//* **************** structure definitions ****************

//* define the elements of each entry of the BFTL table: Each LBA is stored in a (Block, Page) location
typedef struct
{
	flash_size_t Block;
	flash_size_t Page;
	flash_size_t Time;
	struct WatchingList *next;
} BFTLElement_t;



//* ****************************************************************
//* Simulate the FTL method with DWL
//* "fp" is the input file descriptor
//* ****************************************************************
void BFTLSimulation(FILE *fp);














#endif
