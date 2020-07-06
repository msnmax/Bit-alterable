//* FTL.c : All the simulation for FTL are defined in this file
//* dynamic wear leveling: using best fit strategy for its GC
//* static wear leveling: activated when the BlockErase_count/flag_count > SWLThreshold "T"

#ifndef _FTL_C
#define _FTL_C
#endif


#include "main.h"
#include "FTL.h"

static FTLElement_t *FTLTable;	//* FTL table
static flash_size_t	CurrentFreeBlockID;	//* Point to current free block ID
static flash_size_t CurrentFreePageID;	//* Point to current free page ID: Initially it points to the page beyond the last page of the block to trigger an action to get a new free block from the FreeBlockList
long long livepagecont=0;

//int switchon=1;
//* ******************************** functions *********************


//* ****************************************************************
//* Read a cluster from the flash memory
//* "ClusterID" is the Cluster that is going to be updated or written
//* Return value: If the "ClusterID" exists, return True. Otherwise, return False.
//* *****************************************************************
static Boolean FTLReadOneCluster(flash_size_t ClusterID)
{
	if(FTLTable[ClusterID].Block == FTL_POINT_TO_NULL) return(False);

	//# Staticstics
	AccessStatistics.TotalReadRequestTime += TIME_READ_PAGE; //# the time to read one page

	return(True);
}



//* ****************************************************************
//* Write a cluster to the flash memory and also update the FTL table
//* "ClusterID" is the Cluster that is going to be updated or written
//* *****************************************************************
static void FTLWriteOneCluster(flash_size_t ClusterID)
{
   // printf("block=%d,page=%d,space=%d\n",CurrentFreeBlockID,CurrentFreePageID,GCTriggerSpace);


    //printf("Block=%d page=%d,space=%d,threshold=%d\n",CurrentFreeBlockID,CurrentFreePageID,GCTriggerSpace,MaxBlock*MaxPage/20);
	//* if this is an update, invalidate this page, and advance the number of invalid pages by 1.
	if(FTLTable[ClusterID].Block != FTL_POINT_TO_NULL)	//* an update
	{
		FlashMemory[FTLTable[ClusterID].Block][FTLTable[ClusterID].Page] = INVALID_PAGE;	//* invalidate this page
		InvalidPageCnt[FTLTable[ClusterID].Block]++;	//* advance the number of invalid pages in the block
		ValidPageCnt[FTLTable[ClusterID].Block]--;	//* decrease the number of valid pages in the block
		if(InvalidPageCnt[FTLTable[ClusterID].Block]>MaxPage) printf("%d=%d\n",ClusterID,InvalidPageCnt[FTLTable[ClusterID].Block]);
	}

	//* Store the data of this LBA in current (Block, Page)
	FTLTable[ClusterID].Block = CurrentFreeBlockID;
	FTLTable[ClusterID].Page = CurrentFreePageID;

	FlashMemory[CurrentFreeBlockID][(int)CurrentFreePageID] = ClusterID;	//* Cluster i is stored at current page

	ValidPageCnt[CurrentFreeBlockID]++;	//* advance the number of valid pages in the block
    GCTriggerSpace--;
	CurrentFreePageID++;	//* move to next free page
  //  printf("%d,%d\n",CurrentFreeBlockID,CurrentFreePageID);
	//# Statistics
	AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE; //# the time to write a page

#ifdef _JOHNSON_DEBUG
	if(CurrentFreePageID>MaxPage)
	{
		printf("ERROR: CurrentFreePageID=%d\n",CurrentFreePageID);
		exit(1);
	}
#endif
}



//* ****************************************************************
//* Erase one block
//* ****************************************************************
void FTLEraseOneBlock(flash_size_t BlockID)
{
	flash_size_t i;

	//* Put the selected free block to the free block list
	PutFreeBlock(BlockID);

	//* reclaim the selected block
	for(i=0; i<MaxPage;i++)
	{
		//* if current free block is filled up, get a free block from the lsit
		if(CurrentFreePageID >= MaxPage)
		{
			CurrentFreeBlockID = GetFreeBlock();
			CurrentFreePageID = 0;
		}

		if( (FlashMemory[BlockID][i] != INVALID_PAGE) &&  (FlashMemory[BlockID][i] != FREE_PAGE))
		{
			//# statistics
			AccessStatistics.TotalLivePageCopyings++;	//* advnace the total number of live-page copyings by 1
			AccessStatistics.TotalWriteRequestTime += TIME_READ_PAGE; //#the time to copy a live page
            livepagecont++;
			FTLWriteOneCluster(FlashMemory[BlockID][i]);	//* move the live-page out, including the update of the time to write a page
		}

	}

	//* if current free block is filled up, get a free block from the lsit
	if(CurrentFreePageID >= MaxPage)
	{
		CurrentFreeBlockID = GetFreeBlock();
		CurrentFreePageID = 0;
	}
	//printf("BlockID=%d\n",BlockID);

	//* Clean up the reclaimed block as a free block
	InvalidPageCnt[BlockID] = 0;
	ValidPageCnt[BlockID] = 0;
	for(i=0; i<MaxPage; i++) FlashMemory[BlockID][i] = FREE_PAGE;
    GCTriggerSpace=GCTriggerSpace+MaxPage;
	BlockEraseCnt[BlockID]++;	//* update the erase count

	//# Update the max block erase count
	if(BlockEraseCnt[BlockID]>MaxBlockEraseCnt) MaxBlockEraseCnt = BlockEraseCnt[BlockID];

	//# statistics
	AccessStatistics.TotalBlockEraseCount++;		//* update total number of block erases
	AccessStatistics.TotalWriteRequestTime += TIME_ERASE_BLOCK; //# the time to erase a block

}


//# ****************************************************************
//# Do garbage collection to reclaim one more free block if there is no free block in the free block list
//# dynamic wear leveling: using cost-benefit strategy for its GC->
//# (free page=0, live page=-1, dead page = 1) while the sum over every page is larger than 0, erase the block.
//# Always reclaim the first block that has the maximal number of invalid pages
//# ****************************************************************
static void FTLGarbageCollection(void)
{
	static flash_size_t CurrentScanningBlockID=-1;		//* Record the Id of the block that is going to be scanned
	static flash_size_t InvalidPage2ValidPageDivider=1;	//# The ratio of invalid pages to valid pages
	flash_size_t i;
    int maxinvalid=0;
	//* Keep reclaim blocks until there is one more free block in the free block list
	while(GCTriggerSpace<=MaxPage*MaxBlock*3/40)
	{
		//* find out the block with the largest number of invalid pages
		for(i=0; i<MaxBlock; i++)
		{
			CurrentScanningBlockID = (CurrentScanningBlockID + 1) % MaxBlock;	//# advance to the next block

			//# statistics
			//AccessStatistics.TotalWriteRequestTime += TIME_READ_SPARE_PAGE * MaxPage; //# the time to check the spare area of pages in one block

			//# a victim is found
			if( (InvalidPageCnt[CurrentScanningBlockID] > ValidPageCnt[CurrentScanningBlockID]/InvalidPage2ValidPageDivider) && \
				(FreeBlockList[CurrentScanningBlockID] == FBL_NOT_IN_LIST) && \
				(CurrentScanningBlockID != CurrentFreeBlockID) )	//* encounter a block that has more invalid pages than valid pages
			{

				break;	//# break when the victim block is found
			}
			/*
			if(InvalidPageCnt[CurrentScanningBlockID]>maxinvalid){
                maxinvalid=InvalidPageCnt[CurrentScanningBlockID];
			}*/

		}

		if(i==MaxBlock) //# the cost-benefit function can't find the victim block set, so that we use the block set that has the larget weight
		{
			InvalidPage2ValidPageDivider *= 2;	//# lower the ratio whenever we can't find a victim (due to the cost-benefit function)
			printf("InvalidPage2ValidPageDivider=%d\n", InvalidPage2ValidPageDivider);

		}
		else
		{
			FTLEraseOneBlock(CurrentScanningBlockID);	//* Erase one block and also update the BET and counters
		}


	} //* end of while()

}

//* ****************************************************************
//* Write a cluster to the flash memory and also update the FTL table. We suggest this function to write a cluster to the flash memory and also update the FTL table
//* "ClusterID" is the Cluster that is going to be updated or written
//* ****************************************************************
static void SecureFTLWriteOneCluster(flash_size_t ClusterID)
{

	//* Curret block is filled up. Therefore, we have to get another free block from the free block list.
	while(CurrentFreePageID >= MaxPage)
	{

		if(CheckFreeBlock()==-1)	//* get one free block, if the block is not free, do garbage collection to reclaim one more free block
		{
			FTLGarbageCollection();	//* Do garbage collection to reclaim one more free block if there is no free block in the free block list

		}
		else
		{
		    CurrentFreeBlockID=GetFreeBlock();
			CurrentFreePageID =  0;	//* the free block should start from page 0 to store data
		}
	}

	FTLWriteOneCluster(ClusterID);	//* update the FTL information for the write or update of this Cluster

	//* Curret block is filled up. Therefore, we have to get another free block from the free block list.
	while(CurrentFreePageID >= MaxPage)
	{
		if(CheckFreeBlock()==-1)	//* get one free block, if the block is not free, do garbage collection to reclaim one more free block
		{
			FTLGarbageCollection();	//* Do garbage collection to reclaim one more free block if there is no free block in the free block list

		}
		else
		{
		    CurrentFreeBlockID=GetFreeBlock();
			CurrentFreePageID =  0;	//* the free block should start from page 0 to store data
		}
	}

}

//* ****************************************************************
//* Simulate the FTL method with DWL
//* "fp" is the input file descriptor
//* ****************************************************************
void FTLSimulation(FILE *fp)
{
	flash_size_t CurrentCluster;
	flash_size_t i,j;
	long long clustercnt=0;
//	flash_size_t StartCluster;	//* the starting Cluster to be written
//	flash_size_t Length;		//* the number of Clusters to be written
//	AccessType_t AccessType;	//# the type of access (i.e., read or write)

	int BlockMaxRandomValue;	//* The maximal random value, which allows us to select a block storing initial data
	//int PageMaxRandomValue;		//* The maximal random value, which allows us to select a page of thte selected block

	//* Initialize FTL table
	FTLTable = (FTLElement_t *)malloc(sizeof(FTLElement_t)*MaxCluster);	//* allocate memory space for the FTL table
	memset(FTLTable,FTL_POINT_TO_NULL,sizeof(FTLElement_t)*MaxCluster);	//* initialize the content of this table


	//* Get the first free block
	CurrentFreeBlockID = GetFreeBlock();	// Get a free block
	CurrentFreePageID = 0 ;
    GCTriggerSpace=MaxBlock*MaxPage;

	//* Randomly select where are the data stored in the flash memory initially
	//* according the parameter "InitialPercentage" to determine what's the percentage of data in ths flash memory initially.
	BlockMaxRandomValue = (int)((float)InitialPercentage * (RAND_MAX) / 100);	//* The max random number equals "InitialPercentage" percentage of data.
	for(i=0; i<(MaxBlock-LeastFreeBlocksInFBL()-BadBlockCnt); i++)
	{
		if( rand() <= BlockMaxRandomValue)
		{
			//PageMaxRandomValue = rand();	//* The maximal number of pages storing initial data is from 0 to MaxPagd
			for(j=0;j<MaxPage;j++)
			{
			//	if(rand() <= PageMaxRandomValue)
				{
					SecureFTLWriteOneCluster(i*MaxPage+j);

					//# Update the number of accessed logical pages
					AccessedLogicalPages++;	//# advanced the number of set flags in the map
					AccessedLogicalPageMap[i*MaxPage+j] = True; //# Set the flag on
				}
			}
		}
	}
	//# Log information
	printf("Initial Percentage of data: %.02f%%\n",(float)AccessedLogicalPages/(MaxPage*(MaxBlock))*100);


	//# reset statictic variables
	ResetStatisticVariable(&AccessStatistics);

	//* ******************************* Start simulation ***************************
	while(GetOneOperation(fp, &CurrentRequest.AccessType, &CurrentRequest.StartCluster, &CurrentRequest.Length))	//* fetch one write operation
	{

		//* Check each Write request is a new write or an update and then update the mapping information
		for(CurrentCluster=CurrentRequest.StartCluster; CurrentCluster<(CurrentRequest.StartCluster+CurrentRequest.Length); CurrentCluster++)
		{

          //  printf("cluster=%d\n",CurrentCluster);
			if(CurrentRequest.AccessType == WriteType)
			{
				SecureFTLWriteOneCluster(CurrentCluster);	//* Write a cluster to the flash memory and also update the FTL table. We suggest this function to write a cluster to the flash memory and also update the FTL table
                clustercnt++;
              //  printf("%lld\n",clustercnt);

                if(clustercnt%1000000000==0){
                    printf("MaxErase=%d,Blockcnt=%lld,cluster=%lld\n",MaxBlockEraseCnt,AccessStatistics.TotalBlockEraseCount,clustercnt);
                    printf("total live page copy=%lld\n",livepagecont);
                }
             //   }
			}
			else if(CurrentRequest.AccessType == ReadType)
			{
				//if(FTLTable[CurrentCluster].Block == FTL_POINT_TO_NULL) break;	//# the accessed ClusterID, i.e., the accessed logical address, doesn't exist
				if(FTLReadOneCluster(CurrentCluster) == False) break;	//* Read a cluster to the flash memory and also update the FTL table. We suggest this function to write a cluster to the flash memory and also update the FTL table
			}
		}	//* end of for

		//# Statistics
		if(CurrentCluster == (CurrentRequest.StartCluster+CurrentRequest.Length)) //# if the request is a legal command
		{
			if(CurrentRequest.AccessType == WriteType) AccessStatistics.TotalWriteRequestCount ++;	//# advance the number of total write requests
			if(CurrentRequest.AccessType == ReadType) AccessStatistics.TotalReadRequestCount ++;	//# advance the number of total read requests
		}
        if(GCTriggerSpace<=MaxPage*MaxBlock*3/40){
            FTLGarbageCollection();

      //      }
        }

	}	//* end of while
	//* ******************************* End of simulation *****************************


    printf("MaxEraseCnt=%d,cluster=%lld\n",MaxBlockEraseCnt,clustercnt);
    printf("Space=%d,Threshold=%d\n",GCTriggerSpace,MaxBlock*MaxPage*3/40);
    printf("total block cnt=%d\n",AccessStatistics.TotalBlockEraseCount);
  //  printf("total live page copy=%lld\n",AccessStatistics.TotalLivePageCopyings);
    printf("total live page copy=%lld\n",livepagecont);
    printf("Total request time=%d sec\n",AccessStatistics.TotalWriteRequestTime/1000000000);
    float responsetime=(float)AccessStatistics.TotalWriteRequestTime/1000000.0;
    responsetime=responsetime/(float)clustercnt;
    printf("Avg response time=%f ms\n",responsetime);

	free(FTLTable);	//* release memory space

}




