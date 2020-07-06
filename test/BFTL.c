//* FTL.c : All the simulation for FTL are defined in this file
//* dynamic wear leveling: using best fit strategy for its GC
//* static wear leveling: activated when the BlockErase_count/flag_count > SWLThreshold "T"

#ifndef _BFTL_C
#define _BFTL_C
#endif


#include "main.h"
#include "BFTL.h"
#include <time.h>

static BFTLElement_t *BFTLTable;	//* BFTL table
static flash_size_t	CurrentOldBlockID;	//* Point to current free block ID
static flash_size_t CurrentOldPageID;	//* Point to current free page ID: Initially it points to the page beyond the last page of the block to trigger an action to get a new free block from the FreeBlockList
static flash_size_t	CurrentNewBlockID;	//* Point to current free block ID
static flash_size_t CurrentNewPageID;	//* Point to current free page ID: Initially it points to the page beyond the last page of the block to trigger an action to get a new free block from the FreeBlockList

int switchoff=1;
int reset=0;
int checkblock;
int AvgErase;
int sum=0,sum_sub=0;
int debug=0;
long long clustercnt=0;
long long newtimes,oldtimes=0;
long long requesttime=0;
BFTLElement_t *watchinglist=NULL;
BFTLElement_t *newblock=NULL;
BFTLElement_t *oldblock=NULL;
BFTLElement_t *HPE1=NULL;
BFTLElement_t *HPE01=NULL;
BFTLElement_t *HPE0=NULL;
BFTLElement_t *LPE1=NULL;
BFTLElement_t *LPE01=NULL;
BFTLElement_t *LPE0=NULL;
int overflow=0;
int watchbug;

//查表
float PageWriteTime[1024];
float PageEraseTime[1024];
float BlockEraseTime[1024];
float Normalization[1024];

void *Insertlist(BFTLElement_t *list,flash_size_t Block,flash_size_t Page);
void *CheckList(BFTLElement_t *list,flash_size_t Block,flash_size_t Page);
void *DelList(BFTLElement_t *list,flash_size_t Block,flash_size_t Page);
void *InsertBlock(BFTLElement_t *list,flash_size_t Block);
void *FindBlock(BFTLElement_t *list,flash_size_t Block);
void *DelBlock(BFTLElement_t *list,flash_size_t Block);
void *BlockLentgth(BFTLElement_t *list);
void *FindFirst(BFTLElement_t *list);
void *FindEnd(BFTLElement_t *list);
//void *DelFirst(BFTLElement_t *list);

//* ******************************** functions *********************


//min function
int min(int a, int b)
{
    return a>b?b:a;
}

int minmum(int a, int b,int c,int d)
{
    if(min(a,b)<min(c,d)){
        return min(a,b);
    }
    else{
        return min(c,d);
    }
}

//according block priority to get block
int getblock(int datatype){
    int block;
    if(datatype==1){
        if(HPE1!=NULL){
            block=FindEnd(HPE1);
            return block;
        }
        else if(HPE01!=NULL){
            block=FindEnd(HPE01);
            return block;
        }
        else if(HPE0!=NULL){
            block=FindEnd(HPE0);
            return block;
        }
        else if(LPE1!=NULL){
            block=FindEnd(LPE1);
            return block;
        }
        else if(LPE01!=NULL){
            block=FindEnd(LPE01);
            return block;
        }
        else{
            block=FindEnd(LPE0);
            return block;
        }
    }
    else{
        if(LPE0!=NULL){
            block=FindFirst(LPE0);
            return block;
        }
        else if(LPE01!=NULL){
            block=FindFirst(LPE01);
            return block;
        }
        else if(LPE1!=NULL){
            block=FindFirst(LPE1);
            return block;
        }
        else if(HPE0!=NULL){
            block=FindFirst(HPE0);
            return block;
        }
        else if(HPE01!=NULL){
            block=FindFirst(HPE01);
            return block;
        }
        else{
            block=FindFirst(HPE1);
            return block;
        }
    }
}



//* ****************************************************************
//* Read a cluster from the flash memory
//* "ClusterID" is the Cluster that is going to be updated or written
//* Return value: If the "ClusterID" exists, return True. Otherwise, return False.
//* *****************************************************************
static Boolean BFTLReadOneCluster(flash_size_t ClusterID)
{
	if(BFTLTable[ClusterID].Block == BFTL_POINT_TO_NULL) return(False);

	//# Staticstics
	AccessStatistics.TotalReadRequestTime += TIME_READ_PAGE; //# the time to read one page

	return(True);
}
static void NewDataWriteOneCluster(flash_size_t ClusterID)
{   flash_size_t i,j;
    clock_t CurrentTime;
    CurrentTime=clock();
    CurrentTime=CurrentTime/1000; //recording time
    BFTLElement_t *temp;


    if(FreePageCnt[CurrentNewBlockID]==0){
    printf("block%d type=%d,freepage=%d,valid=%d,invalid=%d\n",CurrentNewBlockID,BlockType[CurrentNewBlockID],
            FreePageCnt[CurrentNewBlockID],ValidPageCnt[CurrentNewBlockID],InvalidPageCnt[CurrentNewBlockID]);
        deleteblock(0,BlockType[CurrentNewBlockID],CurrentNewBlockID);
        if(CurrentNewBlockID==CurrentOldBlockID){
            CurrentOldBlockID=getblock(1);
            CurrentOldPageID=0;
        }
       // BlockType[CurrentNewBlockID]=0;
        CurrentNewBlockID=getblock(0);
        CurrentNewPageID=0;
    }
    if(WatchPageCnt[CurrentNewBlockID]>0){
        while(CheckList(watchinglist,CurrentNewBlockID,CurrentNewPageID)!=NULL){
       // printf("Block=%d page=%d watchlist state=%d\n",CurrentNewBlockID,i,FlashMemory[CurrentNewBlockID][CurrentNewPageID]);
        CurrentNewPageID++;
        }
    }


    //block=2or5 select msb 0 page
    if(BlockType[CurrentNewBlockID]==2||BlockType[CurrentNewBlockID]==5){
        while(MSB[CurrentNewBlockID][CurrentNewPageID]==1&&CurrentOldPageID<MaxPage){
            CurrentNewPageID++;

        }
    }


    if(CurrentNewPageID>=MaxPage){
            deleteblock(0,BlockType[CurrentNewBlockID],CurrentNewBlockID);

            if(CurrentNewBlockID==CurrentOldBlockID){
                CurrentOldBlockID=getblock(1);
                CurrentOldPageID=0;
            }
            CurrentNewBlockID=getblock(0);
            CurrentNewPageID=0;
        }
    while(FlashMemory[CurrentNewBlockID][CurrentNewPageID]!=FREE_PAGE){

        CurrentNewPageID++;
        //checking watching list
        if(CheckList(watchinglist,CurrentNewBlockID,CurrentNewPageID)!=NULL){
            CurrentNewPageID++;
        }
        if(BlockType[CurrentNewBlockID]==2||BlockType[CurrentNewBlockID]==5){
            while(MSB[CurrentNewBlockID][CurrentNewPageID]==1&&CurrentOldPageID<MaxPage){
                CurrentNewPageID++;
            }
        }
        if(CurrentNewPageID>=MaxPage){
            deleteblock(0,BlockType[CurrentNewBlockID],CurrentNewBlockID);

            if(CurrentNewBlockID==CurrentOldBlockID){
                CurrentOldBlockID=getblock(1);
                CurrentOldPageID=0;
            }
            CurrentNewBlockID=getblock(0);
            CurrentNewPageID=0;
        }
    }

    //block=2or5 select msb 0 page

    if(BlockType[CurrentNewBlockID]==2||BlockType[CurrentNewBlockID]==5){
        while(MSB[CurrentNewBlockID][CurrentNewPageID]==1||FlashMemory[CurrentNewBlockID][CurrentNewPageID]!=FREE_PAGE){
            CurrentNewPageID++;
            if(CurrentNewPageID>=MaxPage){

             printf("new block%d type=%d,page=%d,state=%d\n",CurrentNewBlockID,BlockType[CurrentNewBlockID],
                    CurrentNewPageID,FlashMemory[CurrentNewBlockID][CurrentNewPageID]);
 //    system("pause");
    }
        }
    }


if(CurrentNewPageID>=MaxPage){

     printf("new block%d type=%d,page=%d,state=%d\n",CurrentNewBlockID,BlockType[CurrentNewBlockID],
            CurrentNewPageID,FlashMemory[CurrentNewBlockID][CurrentNewPageID]);
 //    system("pause");
    }



	//* if this is an update, invalidate this page, and advance the number of invalid pages by 1.
	if(BFTLTable[ClusterID].Block != BFTL_POINT_TO_NULL)	//* an update
	{

		FlashMemory[BFTLTable[ClusterID].Block][BFTLTable[ClusterID].Page] = INVALID_PAGE;	//* invalidate this page
		InvalidPageCnt[BFTLTable[ClusterID].Block]++;	//* advance the number of invalid pages in the block
		ValidPageCnt[BFTLTable[ClusterID].Block]--;	//* decrease the number of valid pages in the block
		if(InvalidPageCnt[BFTLTable[ClusterID].Block]>MaxPage)
        {
        printf("%d=%d\n",ClusterID,InvalidPageCnt[BFTLTable[ClusterID].Block]);
        printf("new block%d,type=%d ,msbqty=%d,free=%d,valid=%d\n",BFTLTable[ClusterID].Block,BlockType[BFTLTable[ClusterID].Block],
                BlockMSB[BFTLTable[ClusterID].Block],FreePageCnt[BFTLTable[ClusterID].Block]
                ,ValidPageCnt[BFTLTable[ClusterID].Block]);
        Display(watchinglist);
        }

	}

	//* Store the data of this LBA in current (Block, Page)
	BFTLTable[ClusterID].Block = CurrentNewBlockID;
	BFTLTable[ClusterID].Page = CurrentNewPageID;
    BFTLTable[ClusterID].Time = CurrentTime; //added Time

  //  printf("TIME %d Newdatawrite block=%d,page=%d,blocktype=%d,MSB=%d\n"
 //           ,CurrentTime,CurrentNewBlockID,CurrentNewPageID,BlockType[CurrentNewBlockID],MSB[CurrentNewBlockID][CurrentNewPageID]);
	FlashMemory[CurrentNewBlockID][(int)CurrentNewPageID] = ClusterID;	//* Cluster i is stored at current page
	ValidPageCnt[CurrentNewBlockID]++;	//* advance the number of valid pages in the block
    FreePageCnt[CurrentNewBlockID]--;

	CurrentNewPageID++;	//* move to next free page
	GCTriggerSpace--;

    if(BlockType[CurrentNewBlockID]==2||BlockType[CurrentNewBlockID]==5){
            while(MSB[CurrentNewBlockID][CurrentNewPageID]==1||FlashMemory[CurrentNewBlockID][CurrentNewPageID]!=FREE_PAGE){
                CurrentNewPageID++;
                if(WatchPageCnt[CurrentNewBlockID]>0){
                    while(CheckList(watchinglist,CurrentNewBlockID,CurrentNewPageID)!=NULL){
                   // printf("Block=%d page=%d watchlist state=%d\n",CurrentNewBlockID,i,FlashMemory[CurrentNewBlockID][CurrentNewPageID]);
                    CurrentNewPageID++;
                    }
                }
                if(CurrentNewPageID>=MaxPage){
                    break;
                }
            }
    }
    else{
         while(FlashMemory[CurrentNewBlockID][CurrentNewPageID]!=FREE_PAGE){
                CurrentNewPageID++;
                if(WatchPageCnt[CurrentNewBlockID]>0){
                    while(CheckList(watchinglist,CurrentNewBlockID,CurrentNewPageID)!=NULL){
                   // printf("Block=%d page=%d watchlist state=%d\n",CurrentNewBlockID,i,FlashMemory[CurrentNewBlockID][CurrentNewPageID]);
                    CurrentNewPageID++;
                    }
                }
                if(CurrentNewPageID>=MaxPage){
                    break;
                }
        }

    }
   if(FreePageCnt[CurrentNewBlockID]==0){

        deleteblock(2,BlockType[CurrentNewBlockID],CurrentNewBlockID);
        if(CurrentNewBlockID==CurrentOldBlockID){
            CurrentOldBlockID=getblock(1);
            CurrentOldPageID=0;
        }
           // BlockType[CurrentNewBlockID]=0;
        CurrentNewBlockID=getblock(0);
        CurrentNewPageID=0;
    }
    else if(CurrentNewPageID>=MaxPage){
        deleteblock(0,BlockType[CurrentNewBlockID],CurrentNewBlockID);
        if(CurrentNewBlockID==CurrentOldBlockID){
            CurrentOldBlockID=getblock(1);
            CurrentOldPageID=0;
        }
           // BlockType[CurrentNewBlockID]=0;
        CurrentNewBlockID=getblock(0);
        CurrentNewPageID=0;


    }



	//# Statistics
	AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE/1000000.0; //# the time to write a page
	requesttime+=TIME_WRITE_PAGE/1000;


#ifdef _JOHNSON_DEBUG
	if(CurrentFreePageID>MaxPage)
	{
		printf("ERROR: CurrentFreePageID=%d\n",CurrentFreePageID);
		exit(1);
	}
#endif

if(LPE0==NULL&&LPE01==NULL&&LPE1==NULL&&HPE0==NULL&&HPE01==NULL&&HPE1==NULL){
        printf("new noblock\n");
        printf("watchlist cnt %d,space=%d,trigger=%d\n",BlockLentgth(watchinglist),GCTriggerSpace,MaxBlock*MaxPage/20);

}


}

static void OldDataWriteOneCluster(flash_size_t ClusterID)
{   flash_size_t i,j;
    clock_t CurrentTime;
    CurrentTime=clock();
    CurrentTime=CurrentTime/1000;
    BFTLElement_t *temp;

    int wathchpage=NULL;
    int watchblock=NULL;

    //checking watching list
    if(watchinglist!=NULL){

        temp=watchinglist;
        while(FlashMemory[temp->Block][temp->Page]!=FREE_PAGE){
            if(temp->next==NULL){
                break;
            }
            temp=temp->next;

        }
        if(FlashMemory[temp->Block][temp->Page]==FREE_PAGE){
            watchblock=temp->Block;
            wathchpage=temp->Page;
            if(watchblock==watchbug){
                watchblock=NULL;
            }
        }

    }


    if(watchblock==NULL){

        if(FreePageCnt[CurrentOldBlockID]==0){
            printf("block%d type=%d,freepage=%d,valid=%d,invalid=%d\n",CurrentOldBlockID,BlockType[CurrentOldBlockID],
            FreePageCnt[CurrentOldBlockID],ValidPageCnt[CurrentOldBlockID],InvalidPageCnt[CurrentOldBlockID]);

            deleteblock(1,BlockType[CurrentOldBlockID],CurrentOldBlockID);
            if(CurrentOldBlockID==CurrentNewBlockID){
                CurrentNewBlockID=getblock(0);
                CurrentNewPageID=0;
            }
            //BlockType[CurrentOldBlockID]=0;
            CurrentOldBlockID=getblock(1);
            CurrentOldPageID=0;
        }

        //block=2or5 select msb 1 page
        if(BlockType[CurrentOldBlockID]==2||BlockType[CurrentOldBlockID]==5){
            while(MSB[CurrentOldBlockID][CurrentOldPageID]==0&&CurrentOldPageID<MaxPage){
                CurrentOldPageID++;

            }
        }
        if(CurrentOldPageID>=MaxPage){
                deleteblock(1,BlockType[CurrentOldBlockID],CurrentOldBlockID);
                if(CurrentOldBlockID==CurrentNewBlockID){
                    CurrentNewBlockID=getblock(0);
                    CurrentNewPageID=0;
                }
                CurrentOldBlockID=getblock(1);
                CurrentOldPageID=0;
            }

        //block=2or5 select msb 1 page
        while(FlashMemory[CurrentOldBlockID][CurrentOldPageID]!=FREE_PAGE){

            CurrentOldPageID++;
            if(BlockType[CurrentOldBlockID]==2||BlockType[CurrentOldBlockID]==5){
                while(MSB[CurrentOldBlockID][CurrentOldPageID]==0&&CurrentOldPageID<MaxPage){
                    CurrentOldPageID++;
                }
            }

            if(CurrentOldPageID>=MaxPage){
                deleteblock(1,BlockType[CurrentOldBlockID],CurrentOldBlockID);
                if(CurrentOldBlockID==CurrentNewBlockID){
                    CurrentNewBlockID=getblock(0);
                    CurrentNewPageID=0;
                }
                CurrentOldBlockID=getblock(1);
                CurrentOldPageID=0;
            }


        }


        if(BlockType[CurrentOldBlockID]==2||BlockType[CurrentOldBlockID]==5){
            while(MSB[CurrentOldBlockID][CurrentOldPageID]==0||FlashMemory[CurrentOldBlockID][CurrentOldPageID]!=FREE_PAGE){
                CurrentOldPageID++;

            }
            if(CurrentOldPageID>=MaxPage){
                printf("old22 block=%d page=%d\n");
            }
        }



    }


    //printf("OLDEN block %d ,page %d,type=%d,erase %d\n",CurrentNewBlockID,CurrentNewPageID,BlockType[CurrentNewBlockID],MaxBlockEraseCnt);

    else{
        CurrentOldBlockID=watchblock;
        CurrentOldPageID=wathchpage;
    }


if(CurrentOldPageID>=MaxPage){

     printf("old block%d,type=%d page=%d,state=%d,msbqty=%d,free=%d,valid=%d\n",CurrentOldBlockID,BlockType[CurrentOldBlockID],
            CurrentOldPageID,FlashMemory[CurrentOldBlockID][CurrentOldPageID],BlockMSB[CurrentOldBlockID],FreePageCnt[CurrentOldBlockID]
            ,ValidPageCnt[CurrentOldBlockID]);
      //  for(i=0;i<MaxPage;i++){
      //      printf("page=%d,state=%d,msb=%d\n",i,FlashMemory[CurrentOldBlockID][i],MSB[CurrentOldBlockID][i]);
      //  }

     //   printf("watchlist=%d,ERASE=%d\n",WatchPageCnt[CurrentOldBlockID],MaxBlockEraseCnt);
  //   system("pause");
    }

	//* if this is an update, invalidate this page, and advance the number of invalid pages by 1.
	if(BFTLTable[ClusterID].Block != BFTL_POINT_TO_NULL)	//* an update
	{

		FlashMemory[BFTLTable[ClusterID].Block][BFTLTable[ClusterID].Page] = INVALID_PAGE;	//* invalidate this page
		InvalidPageCnt[BFTLTable[ClusterID].Block]++;	//* advance the number of invalid pages in the block
		ValidPageCnt[BFTLTable[ClusterID].Block]--;	//* decrease the number of valid pages in the block
		if(InvalidPageCnt[BFTLTable[ClusterID].Block]>MaxPage){
        printf("%d=%d\n",ClusterID,InvalidPageCnt[BFTLTable[ClusterID].Block]);
        printf("block%d,type=%d ,msbqty=%d,free=%d,valid=%d\n",BFTLTable[ClusterID].Block,BlockType[BFTLTable[ClusterID].Block],
                BlockMSB[BFTLTable[ClusterID].Block],FreePageCnt[BFTLTable[ClusterID].Block]
                ,ValidPageCnt[BFTLTable[ClusterID].Block]);

        }
    }

	//* Store the data of this LBA in current (Block, Page)
	BFTLTable[ClusterID].Block = CurrentOldBlockID;
	BFTLTable[ClusterID].Page = CurrentOldPageID;
    BFTLTable[ClusterID].Time = CurrentTime; //added Time

    FlashMemory[CurrentOldBlockID][(int)CurrentOldPageID] = ClusterID;
    	//* Cluster i is stored at current page

	ValidPageCnt[CurrentOldBlockID]++;	//* advance the number of valid pages in the block
    FreePageCnt[CurrentOldBlockID]--;

	GCTriggerSpace--;


    if(FreePageCnt[CurrentOldBlockID]==0){
        deleteblock(2,BlockType[CurrentOldBlockID],CurrentOldBlockID);
        if(CurrentOldBlockID==CurrentNewBlockID){
            CurrentNewBlockID=getblock(0);
            CurrentNewPageID=0;
        }
            //BlockType[CurrentOldBlockID]=0;
        CurrentOldBlockID=getblock(1);
        CurrentOldPageID=0;

    }
    else{
        if(watchblock==NULL){
            CurrentOldPageID++;	//* move to next free page
            if(BlockType[CurrentOldBlockID]==2||BlockType[CurrentOldBlockID]==5){
                while(MSB[CurrentOldBlockID][CurrentOldPageID]==0||FlashMemory[CurrentOldBlockID][CurrentOldPageID]!=FREE_PAGE){
                    CurrentOldPageID++;
                    if(CurrentOldPageID>=MaxPage){
                        break;
                    }
                }
            }

            if(CurrentOldPageID>=MaxPage){
                deleteblock(1,BlockType[CurrentOldBlockID],CurrentOldBlockID);
                if(CurrentOldBlockID==CurrentNewBlockID){
                    CurrentNewBlockID=getblock(0);
                    CurrentNewPageID=0;
                }
            //BlockType[CurrentOldBlockID]=0;
                CurrentOldBlockID=getblock(1);
                CurrentOldPageID=0;
            }
        }
        else{
                CurrentOldBlockID=getblock(1);
                CurrentOldPageID=0;
        }
    }



if(LPE0==NULL&&LPE01==NULL&&LPE1==NULL&&HPE0==NULL&&HPE01==NULL&&HPE1==NULL){
        printf("noblock\n");
        printf("watchlist cnt %d,space=%d,trigger=%d\n",BlockLentgth(watchinglist),GCTriggerSpace,MaxBlock*MaxPage/20);

       // Display(watchinglist);
}



	//# Statistics
	AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE/1000000.0; //# the time to write a page
    requesttime+=TIME_WRITE_PAGE/1000;
#ifdef _JOHNSON_DEBUG
	if(CurrentFreePageID>MaxPage)
	{
		printf("ERROR: CurrentFreePageID=%d\n",CurrentFreePageID);
		exit(1);
	}
#endif

}

//delete block function for block priority
void deleteblock(int datatype,int blocktype,flash_size_t BlockID){
        switch(blocktype){
        case 1:
            LPE0=DelBlock(LPE0,BlockID);
            break;
        case 2:

            LPE01=DelBlock(LPE01,BlockID);
            if(datatype==0){
                LPE1=InsertBlock(LPE1,BlockID);
                BlockType[BlockID]=3;
            }
            else if(datatype==1){
                LPE0=InsertBlock(LPE0,BlockID);
                BlockType[BlockID]=1;
            }


            break;
        case 3:
            LPE1=DelBlock(LPE1,BlockID);

            break;
        case 4:
            HPE0=DelBlock(HPE0,BlockID);
            break;
        case 5:
            HPE01=DelBlock(HPE01,BlockID);
            if(datatype==0){
                HPE1=InsertBlock(HPE1,BlockID);
                BlockType[BlockID]=6;
            }
            else if(datatype==1){
                HPE0=InsertBlock(HPE0,BlockID);
                BlockType[BlockID]=4;
            }


            break;
        case 6:
            HPE1=DelBlock(HPE1,BlockID);
            break;
        }

}

//after page-gc internal data moving
void InternalDataMoving(flash_size_t BlockID){
	flash_size_t i,j;
//	printf("internal block=%d,clustercnt=%d\n",BlockID,clustercnt);
   // printf("space=%d,invalid=%d\n",GCTriggerSpace,InvalidPageCnt[BlockID]);
    AccessStatistics.TotalWriteRequestTime += (TIME_READ_SPARE_PAGE/1000000.0) * MaxPage;
    requesttime+=(TIME_READ_SPARE_PAGE/1000) * MaxPage;
    //find min value to change
    if(min(ValidPageCnt[BlockID],(MaxPage-BlockMSB[BlockID]))<=min(FreePageCnt[BlockID],BlockMSB[BlockID])){
            //compare msb0data and msb1 page,choice less one
        for(i=0;i<MaxPage;i++){
            if(MSB[BlockID][i]==0&&FlashMemory[BlockID][i]>=0){ //MSB 0 data
                if(CheckList(watchinglist,BlockID,i)==NULL){
                    for(j=0;j<MaxPage;j++){
                        if((MSB[BlockID][j]==1)&&(FlashMemory[BlockID][j]==FREE_PAGE)){ //MSB 1 page
                            FlashMemory[BlockID][j]=FlashMemory[BlockID][i]; //swap
                            BFTLTable[FlashMemory[BlockID][i]].Page=j;
                            FlashMemory[BlockID][i]=INVALID_PAGE;
                            InvalidPageCnt[BlockID]++;
                            FreePageCnt[BlockID]--;
                            GCTriggerSpace--;
                            requesttime+=TIME_WRITE_PAGE/1000;
                            AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE/1000000.0;
                            AccessStatistics.TotalLivePageCopyings++;
                            break;
                        }
                    }
                }
            }
        }

    }
    else{
        for(i=0;i<MaxPage;i++){
            if(MSB[BlockID][i]==1&&FlashMemory[BlockID][i]==FREE_PAGE){
                for(j=0;j<MaxPage;j++){
                    if(MSB[BlockID][j]==0&&FlashMemory[BlockID][j]>=0){
                        if(CheckList(watchinglist,BlockID,j)!=NULL){
                            break;
                        }
                        FlashMemory[BlockID][i]=FlashMemory[BlockID][j];
                        BFTLTable[FlashMemory[BlockID][j]].Page=i;
                        FlashMemory[BlockID][j]=INVALID_PAGE;
                        InvalidPageCnt[BlockID]++;
                        FreePageCnt[BlockID]--;
                        GCTriggerSpace--;
                        requesttime+=TIME_WRITE_PAGE/1000;
                        AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE/1000000.0;
                        AccessStatistics.TotalLivePageCopyings++;
                        break;
                    }
                }
            }
        }

    }



}

//* ****************************************************************
//* Erase one block
//* ****************************************************************

void BFTLEraseOneBlock(flash_size_t BlockID)
{

	flash_size_t i;

	//* Put the selected free block to the free block list
	//PutFreeBlock(BlockID);
    clock_t gctime;
    gctime=clock();
    gctime=gctime/1000;
    sum_sub=sum_sub+gctime-BlockGCTime[BlockID];
    BlockGCTime[BlockID]=gctime;
    MaxPageEraseCnt=0;

    if(FreePageCnt[BlockID]>0){
        deleteblock(2,BlockType[BlockID],BlockID);
    }
    watchbug=BlockID;
 //   printf("%d %d\n",gctime,AvgTime);
	//* reclaim the selected block
	for(i=0; i<MaxPage;i++)
	{
		if(FlashMemory[BlockID][i]>=0)
		{
			//# statistics

			AccessStatistics.TotalLivePageCopyings++;	//* advnace the total number of live-page copyings by 1
			AccessStatistics.TotalWriteRequestTime += TIME_READ_PAGE/1000000.0; //#the time to copy a live page
            requesttime+=TIME_READ_PAGE/1000;
      //      printf("data moving %d,%d\n",BlockID,i);
            AvgTime=(gctime*MaxBlock-sum_sub)/MaxBlock;
        //    printf("written time=%d,AvgTime=%d\n",BFTLTable[FlashMemory[BlockID][i]].Time,AvgTime);
            if(BFTLTable[FlashMemory[BlockID][i]].Time>=gctime/2){

                    NewDataWriteOneCluster(FlashMemory[BlockID][i]);    //old=small time
                   // printf("cold data time=%d,avgtime=%d\n",BFTLTable[FlashMemory[BlockID][i]].Time,AvgTime);
                    newtimes++;
                   // AccessStatistics.TotalLivePageCopyings++;
            }
			else{



                    OldDataWriteOneCluster(FlashMemory[BlockID][i]);	//* move the live-page out, including the update of the time to write a page
                    oldtimes++;
            }

        }
        if(FlashMemory[BlockID][i]==FREE_PAGE){
            GCTriggerSpace--;
        }

        GCTriggerSpace++;
        if(PageEraseCnt[BlockID][i]>=MaxPageEraseCnt){
            MaxPageEraseCnt=PageEraseCnt[BlockID][i];
	    }


	}
	if(ValidPageCnt[BlockID]>0){
        printf("Not correct block=%d,free=%d,invalid=%d,valid=%d\n",BlockID,FreePageCnt[BlockID],InvalidPageCnt[BlockID],ValidPageCnt[BlockID]);
        for(i=0;i<MaxPage;i++){
            printf("page=%d,state=%d\n",i,FlashMemory[BlockID][i]);
        }
        printf("--------------------\n");
        Display(watchinglist);
        system("pause");
	}

	InvalidPageCnt[BlockID] = 0;
	ValidPageCnt[BlockID] = 0;
	FreePageCnt[BlockID] = MaxPage;
	for(i=0; i<MaxPage; i++) {
            FlashMemory[BlockID][i] = FREE_PAGE;
	}
	BlockEraseCnt[BlockID]++;	//* update the erase count

	//# Update the max block erase count
	if(BlockEraseCnt[BlockID]+MaxPageEraseCnt>MaxBlockEraseCnt) MaxBlockEraseCnt = BlockEraseCnt[BlockID]+MaxPageEraseCnt;

	//# statistics

	AccessStatistics.TotalBlockEraseCount++;		//* update total number of block erases
	AccessStatistics.TotalWriteRequestTime += TIME_ERASE_BLOCK/1000000.0; //# the time to erase a block
    requesttime+=TIME_ERASE_BLOCK/1000;
	AvgErase=AccessStatistics.TotalBlockEraseCount/MaxBlock;

    //can change block erase count to detect block type
	if(BlockEraseCnt[BlockID]<AvgErase){
        if(BlockMSB[BlockID]==0){
            LPE0=InsertBlock(LPE0,BlockID);
            BlockType[BlockID]=1;
        }
        else{
            LPE01=InsertBlock(LPE01,BlockID);
            BlockType[BlockID]=2;


        }
	}
	else{
        if(BlockMSB[BlockID]==0){
            HPE0=InsertBlock(HPE0,BlockID);
            BlockType[BlockID]=4;
        }
        else{
            HPE01=InsertBlock(HPE01,BlockID);
            BlockType[BlockID]=5;

        }
	}



  //  printf("Erasing %d,%d,space=%d\n",CurrentFreeBlockID,CurrentFreePageID,GCTriggerSpace);
   // printf("Erasing %d,%d,space=%d\n",CurrentFreeBlockID,CurrentFreePageID,GCTriggerSpace);

   // printf("block erase end\n");

}


//* ****************************************************************
//* Erase one page
//* ****************************************************************

void BFTLEraseOnePage(flash_size_t BlockID)
{
	flash_size_t i;
	int putblock0=0;
	int putblock1=0;
	int WastePage;
	clock_t gctime;
    gctime=clock();
    gctime=gctime/1000;
    sum_sub=sum_sub+gctime-BlockGCTime[BlockID];
    BlockGCTime[BlockID]=gctime;
    MaxPageEraseCnt=0;

    if(FreePageCnt[BlockID]>0){
        deleteblock(2,BlockType[BlockID],BlockID);
    }
   /* if(MaxBlockEraseCnt>=870){
        printf("cluste=%lld\n",clustercnt);
        printf("Pageerase0 block=%d,type=%d,freepage=%d,valid=%d,invalid=%d\n",BlockID,BlockType[BlockID],FreePageCnt[BlockID]
               ,ValidPageCnt[BlockID],InvalidPageCnt[BlockID]);
    }*/

    for(i=0; i<MaxPage;i++)
	{


	    if(FlashMemory[BlockID][i]==INVALID_PAGE){
            FlashMemory[BlockID][i]=FREE_PAGE; //select page convert to freepage
            InvalidPageCnt[BlockID]--;
            FreePageCnt[BlockID]++;
            PageEraseCnt[BlockID][i]++;         //page erase count

            if(PageEraseCnt[BlockID][i]==Bit/2){   //trigger for MSB
                MSB[BlockID][i]=1;
                BlockMSB[BlockID]++;            //Block MSB qty++

                                        }
            if(PageEraseCnt[BlockID][i]==Bit){
                //PutWatchingList
                if(CheckList(watchinglist,BlockID,i)!=NULL){
                    overflow++;
                }
                watchinglist=Insertlist(watchinglist,BlockID,i);
                WatchPageCnt[BlockID]++;
                BlockMSB[BlockID]--;
                MSB[BlockID][i]=0;
                PageEraseCnt[BlockID][i]-=Bit;

              //  Display(watchinglist);
            }

            GCTriggerSpace++;
            AccessStatistics.TotalWriteRequestTime += TIME_ERASE_PAGE/1000000.0; //# the time to erase a page
            AccessStatistics.TotalPageEraseCount++;
            requesttime+=TIME_ERASE_PAGE/1000;
        }

        if(PageEraseCnt[BlockID][i]>=MaxPageEraseCnt){
            MaxPageEraseCnt=PageEraseCnt[BlockID][i];
	    }

    }


    if(BlockMSB[BlockID]+WatchPageCnt[BlockID]>=MaxPage){
        MaxPageEraseCnt=0;
        for(i=0; i<MaxPage;i++){
            if(CheckList(watchinglist,BlockID,i)!=NULL){
                if(MSB[BlockID][i]==0){
                    PageEraseCnt[BlockID][i]+=Bit/2;
                    MSB[BlockID][i]=1;
                    watchinglist=DelList(watchinglist,BlockID,i);
                    WatchPageCnt[BlockID]--;
                    BlockMSB[BlockID]++;
                    reset++;
                }
                else{
                    MSB[BlockID][i]=0;
                    PageEraseCnt[BlockID][i]-=Bit/2;
                    BlockMSB[BlockID]--;
                }

            }
            else{
                MSB[BlockID][i]=0;
                PageEraseCnt[BlockID][i]-=Bit/2;
                BlockMSB[BlockID]--;
            }
            if(PageEraseCnt[BlockID][i]>=MaxPageEraseCnt){
                MaxPageEraseCnt=PageEraseCnt[BlockID][i];
            }

        }

  //      BlockMSB[BlockID]=WatchPageCnt[BlockID];
        BlockEraseCnt[BlockID]=BlockEraseCnt[BlockID]+Bit/2;	// reset for MSB ,Block count=+(2^erase bit)/2
       // AccessStatistics.TotalBlockEraseCount+=32;		//* update total number of block erases
    }
    if(InvalidPageCnt[BlockID]>0){
        printf("Not clear block=%d,free=%d,invalid=%d,valid=%d\n",BlockID,FreePageCnt[BlockID],InvalidPageCnt[BlockID],ValidPageCnt[BlockID]);
        for(i=0;i<MaxPage;i++){
            printf("page=%d,state=%d\n",i,FlashMemory[BlockID][i]);
        }
        system("pause");
    }

    WastePage=minmum(ValidPageCnt[BlockID],FreePageCnt[BlockID],BlockMSB[BlockID],MaxPage-BlockMSB[BlockID]);
     if(WastePage!=0){
        InternalDataMoving(BlockID);
    }
    for(i=0;i<MaxPage;i++){
        if(FlashMemory[BlockID][i]==FREE_PAGE&&MSB[BlockID][i]==0){
            putblock0++;
        }
        else if(FlashMemory[BlockID][i]==FREE_PAGE&&MSB[BlockID][i]==1){
            putblock1++;
        }
    }
    if(BlockEraseCnt[BlockID]+MaxPageEraseCnt>MaxBlockEraseCnt) MaxBlockEraseCnt = BlockEraseCnt[BlockID]+MaxPageEraseCnt;

    AvgErase=AccessStatistics.TotalBlockEraseCount/MaxBlock;

    if(FreePageCnt[BlockID]!=0){
        if(BlockEraseCnt[BlockID]<=AvgErase){
            if(FreePageCnt[BlockID]==putblock0){
                LPE0=InsertBlock(LPE0,BlockID);
                BlockType[BlockID]=1;
            }
            else if(FreePageCnt[BlockID]==putblock1){
                LPE1=InsertBlock(LPE1,BlockID);
                BlockType[BlockID]=3;
            }
            else{
                LPE01=InsertBlock(LPE01,BlockID);
                BlockType[BlockID]=2;
            }
        }
        else{
            if(FreePageCnt[BlockID]==putblock0){
                HPE0=InsertBlock(HPE0,BlockID);
                BlockType[BlockID]=4;
            }
            else if(FreePageCnt[BlockID]==putblock1){
                HPE1=InsertBlock(HPE1,BlockID);
                BlockType[BlockID]=6;
            }
            else{
                HPE01=InsertBlock(HPE01,BlockID);
                BlockType[BlockID]=5;
            }
        }
    }


	//# Update the max block erase count

  /*  for(i=0;i<MaxPage;i++){
    printf("after erase page%d page status=%d,msb status=%d,page erase count=%d\n"
           ,i,FlashMemory[BlockID][i],MSB[BlockID][i],PageEraseCnt[BlockID][i]);
    }*/
	//# statistics
//	AccessStatistics.TotalBlockEraseCount++;		//* update total number of block erases
	//AccessStatistics.TotalWriteRequestTime += TIME_ERASE_BLOCK; //# the time to erase a block
	//InternalDataMoving(BlockID);
  //  printf("page erase end\n");

}



//# ****************************************************************
//# GC預留空間為7.5%
//# 因與傳統GC不同不會出現所有Block的所有page已用完的狀況

//# ****************************************************************
static void BFTLGarbageCollection(void)
{
	static flash_size_t CurrentScanningBlockID=-1;		//* Record the Id of the block that is going to be scanned
	static flash_size_t InvalidPage2ValidPageDivider=1;	//# The ratio of invalid pages to valid pages
	flash_size_t i;
	flash_size_t MaxPERBlock;
	//int Threshold;
	float PER;
	float MaxPER=0;
	int WastePage;
	int CurrentSpace;
    int tempblock;
	//* Keep reclaim blocks until there is one more free block in the free block list


	while(GCTriggerSpace<=MaxPage*MaxBlock*3/40)
	{
	    CurrentSpace=GCTriggerSpace;
		//* find out the block with the largest number of invalid pages
		for(i=0; i<MaxBlock; i++)
		{
            //	printf("gc start,%d\n",CurrentScanningBlockID);
            //# statistics
        //   requesttime+=(TIME_READ_SPARE_PAGE/1000) * MaxPage;
       //     AccessStatistics.TotalWriteRequestTime += (TIME_READ_SPARE_PAGE/1000000.0) * MaxPage; //# the time to check the spare area of pages in one block
			CurrentScanningBlockID = i;	//# advance to the next block
            if(InvalidPageCnt[CurrentScanningBlockID]!=0&&CurrentNewBlockID!=CurrentScanningBlockID&&CurrentOldBlockID!=CurrentScanningBlockID){
                if(InvalidPageCnt[CurrentScanningBlockID]<(MaxPage-THRESHOLD)){
                    WastePage=minmum(ValidPageCnt[CurrentScanningBlockID],InvalidPageCnt[CurrentScanningBlockID]+FreePageCnt[CurrentScanningBlockID],BlockMSB[CurrentScanningBlockID],MaxPage-BlockMSB[CurrentScanningBlockID]);
                    if(WastePage>=InvalidPageCnt[CurrentScanningBlockID]){
                        PER=0.0;
                    }
                    else{
                        PER=PageEraseTime[InvalidPageCnt[CurrentScanningBlockID]]+PageWriteTime[WastePage];
                   //     PER=PER/1000000.0;
                        PER=(InvalidPageCnt[CurrentScanningBlockID]-WastePage)/PER;
                        PER=Normalization[InvalidPageCnt[CurrentScanningBlockID]]*PER;
                     //   printf("per=%f\n",PER);
                    }

                 //   }
                }
                else{
                    PER=BlockEraseTime[ValidPageCnt[CurrentScanningBlockID]]+PageWriteTime[ValidPageCnt[CurrentScanningBlockID]];
                //    PER=PER/1000000.0;
                    PER=InvalidPageCnt[CurrentScanningBlockID]/(float)PER;
                   // printf("BLOCK per=%f\n",PER);
                }

                if(PER>=MaxPER){

                   // printf("seltet block=%d,maxerasecnt=%d,PER=%f,MaxPER=%f\n",CurrentScanningBlockID,MaxBlockEraseCnt,PER,MaxPER);
                    MaxPER=PER;
                    MaxPERBlock=CurrentScanningBlockID;

                }


            }

		}


        if(MaxPER>=0){
            //Page-level GC
            if(InvalidPageCnt[MaxPERBlock]<=(MaxPage-THRESHOLD)){

           //     printf("Pblock=%d,Type=%d,valid=%d,invalid=%d,free=%d,PER=%f\n",MaxPERBlock,BlockType[MaxPERBlock],ValidPageCnt[MaxPERBlock],InvalidPageCnt[MaxPERBlock],
               //        FreePageCnt[MaxPERBlock],MaxPER);
            //    WastePage=minmum(ValidPageCnt[MaxPERBlock],InvalidPageCnt[MaxPERBlock]+FreePageCnt[MaxPERBlock],BlockMSB[MaxPERBlock],MaxPage-BlockMSB[CurrentScanningBlockID]);
                BFTLEraseOnePage(MaxPERBlock);

                //BFTLEraseOneBlock(MaxPERBlock);
                MaxPER=0;

            }
            //Block-level GC
            else{
               // printf("page block=%d,PER=%f\n",MaxPERBlock,MaxPER);
          //      printf("Bblock=%d,Type=%d,valid=%d,invalid=%d,free=%d,PER=%f\n",MaxPERBlock,BlockType[MaxPERBlock],ValidPageCnt[MaxPERBlock],InvalidPageCnt[MaxPERBlock],
              //         FreePageCnt[MaxPERBlock],MaxPER);

                BFTLEraseOneBlock(MaxPERBlock);
                MaxPER=0;
            //    printf("space=%d,cluster=%d\n",GCTriggerSpace,clustercnt);


            }
        }
        else{
            return 0;
        }

	}

	//if(clustercnt>=340000000){
  //          printf("MaxErase=%d,space=%d,cluster=%d\n",MaxBlockEraseCnt,GCTriggerSpace,clustercnt);
   //     } //* end of while()
//if(MaxBlockEraseCnt>300){printf("gc End\n");}
//printf("gc End max=%d\n",MaxBlockEraseCnt);
}

//******************************************************************************
//watching list function
//expection case for page erase count
//******************************************************************************


void *Insertlist(BFTLElement_t *list,flash_size_t Block,flash_size_t Page)
{
   BFTLElement_t *watchinglist;

   watchinglist=(BFTLElement_t *) malloc(sizeof(BFTLElement_t)); //memory space allocation

   if(watchinglist == NULL){ return NULL;}

   watchinglist->Page=Page;
   watchinglist->Block=Block;
   watchinglist->next=NULL;


   if(list==NULL)
    {
       list=watchinglist;
    }
   else
    {
       BFTLElement_t *temp=list;
       while(temp->next!=NULL)
       {temp=temp->next;}
       temp->next=watchinglist;
    }
   return list;
}


//find page&block
void *CheckList(BFTLElement_t *list,flash_size_t Block,flash_size_t Page)
{
   BFTLElement_t *temp=list;
   if(WatchPageCnt[Block]==0){
    return NULL;
   }
   while(temp!=NULL)
    {
    if(temp->Page==Page&&temp->Block==Block)
    {
            return temp;

    }
    else{
    temp=temp->next;
    }
    }

   return NULL ;
}



//del list data
void *DelList(BFTLElement_t *list,flash_size_t Block,flash_size_t Page)
{
    BFTLElement_t *temp=list;
    BFTLElement_t *target=CheckList(list,Block,Page);

    if(target!=NULL){
        if(target==temp){
            list=list->next;
        }
        else{
            while(temp->next!=target){
                temp=temp->next;
            }
                if(target->next==NULL){
                    temp->next=NULL;
                }
                else{
                    temp->next=target->next;
                }
        }
    }
    return list;

}


void Display(BFTLElement_t *list){
   BFTLElement_t *temp=list;
   while(temp!=NULL){
       printf("block=%d,page=%d,state=%d,MSB=%d,EraseCnt=%d\n",temp->Block,temp->Page,
              FlashMemory[temp->Block][temp->Page],MSB[temp->Block][temp->Page],PageEraseCnt[temp->Block][temp->Page]);
       temp=temp->next;
   }
   return 1;
}

void DisplayBlock(BFTLElement_t *list){
   BFTLElement_t *temp=list;
   while(temp!=NULL){
       printf("block=%d\n",temp->Block);
       temp=temp->next;
   }
   return 1;
}

void *InsertBlock(BFTLElement_t *list,flash_size_t Block)
{
   BFTLElement_t *watchinglist;

   watchinglist=(BFTLElement_t *) malloc(sizeof(BFTLElement_t)); //memory space allocation

   if(watchinglist == NULL){ return NULL;}

   watchinglist->Block=Block;
   watchinglist->next=NULL;


   if(list==NULL)
    {
       list=watchinglist;
    }
   else
    {
       BFTLElement_t *temp=list;
       while(temp->next!=NULL)
       {temp=temp->next;}
       temp->next=watchinglist;
    }
   return list;
}
void *FindBlock(BFTLElement_t *list,flash_size_t Block)
{
   BFTLElement_t *temp=list;
   while(temp!=NULL)
    {
    if(temp->Block==Block)
    {
            checkblock=1;
            break;
    }
    else{
    temp=temp->next;
    checkblock=0;
    }
    }
    if(checkblock==0){
        return NULL;
    }

   return temp ;
}


void *DelBlock(BFTLElement_t *list,flash_size_t Block)
{
    BFTLElement_t *temp=list;
    BFTLElement_t *target=FindBlock(list,Block);

    if(target!=NULL){
        if(target==temp){
            list=list->next;
        }
        else{
            while(temp->next!=target){
                temp=temp->next;
            }
                if(target->next==NULL){
                    temp->next=NULL;
                }
                else{
                    temp->next=target->next;
                }
        }
    }
    return list;

}

void *FindFirst(BFTLElement_t *list)
{
    int first;
    BFTLElement_t *temp=list;
    if(list==NULL){
        return NULL;
    }
    else{
    first=list->Block;
    return first;
    }
}

void *FindEnd(BFTLElement_t *list)
{
    int end;
    BFTLElement_t *temp=list;
    if(list==NULL){
        return NULL;
    }
    else{
    end=temp->Block;
        while(temp->next!=NULL){
            temp=temp->next;
            end=temp->Block;
        }
    return end;
    }
}


/*

void *DelFirst(BFTLElement_t *list)
{
    BFTLElement_t *temp=list;
    temp=temp->next;

    return temp;

}
*/
void *BlockLentgth(BFTLElement_t *list)
{
   BFTLElement_t *temp=list;
   int num=0;
   while(temp!=NULL)
    {
        num++;
        temp=temp->next;
    }

   return num ;

}





//* ****************************************************************
//* Simulate the FTL method with DWL
//* "fp" is the input file descriptor
//* ****************************************************************
void BFTLSimulation(FILE *fp)
{
    //printf("PageEraseTime=%d\nBlockEraseTime=%d\nPageWriteTime=%d\nThreshold=%d\n",TIME_ERASE_PAGE,TIME_ERASE_BLOCK,TIME_WRITE_PAGE_NO_TRANSFER,THRESHOLD);
    BFTLElement_t *temp;
	flash_size_t CurrentCluster;
	flash_size_t i,j;
//	flash_size_t StartCluster;	//* the starting Cluster to be written
//	flash_size_t Length;		//* the number of Clusters to be written
//	AccessType_t AccessType;	//# the type of access (i.e., read or write)

	int BlockMaxRandomValue;	//* The maximal random value, which allows us to select a block storing initial data
	//int PageMaxRandomValue;		//* The maximal random value, which allows us to select a page of thte selected block

	//* Initialize FTL table
	BFTLTable = (BFTLElement_t *)malloc(sizeof(BFTLElement_t)*MaxCluster);	//* allocate memory space for the FTL table
	memset(BFTLTable,BFTL_POINT_TO_NULL,sizeof(BFTLElement_t)*MaxCluster);	//* initialize the content of this table

    AvgTime=0;

    GCTriggerSpace=MaxPage*MaxBlock;

    for(i=0; i<MaxBlock;i++){
        BlockGCTime[i]=0;
        LPE0=InsertBlock(LPE0,i);
      //  oldblock=DelFirst(oldblock);
        FreePageCnt[i]=MaxPage;
        ValidPageCnt[i]=0;
        InvalidPageCnt[i]=0;
        BlockType[i]=1;
        WatchPageCnt[i]=0;
    }

    for(i=0;i<MaxPage-THRESHOLD;i++){
        PageWriteTime[i]=i*TIME_ERASE_PAGE/1000000.0;
        PageEraseTime[i]=i*TIME_WRITE_PAGE_NO_TRANSFER/1000000.0;
        Normalization[i]=i/(float)(MaxPage-THRESHOLD);
       // printf("time %d,Write=%f,erase=%f,nor=%f\n",i,PageWriteTime[i],PageEraseTime[i],Normalization[i]);
    }

    for(i=0;i<=THRESHOLD;i++){
        BlockEraseTime[i]=(TIME_ERASE_BLOCK+i*TIME_WRITE_PAGE_NO_TRANSFER)/1000000.0;

     //  printf("time %d,Write=%f\n",i,BlockEraseTime[i]);
    }

//    printf("block%d page%d\n",temp->Block,temp->Page);
  //  watchinglist=Insertlist(watchinglist,100,200);
 //   Display(watchinglist);
	//* Get the first free block
	CurrentNewBlockID = getblock(0);	// Get a free block
	CurrentNewPageID = 0 ;
	CurrentOldBlockID = getblock(1);	// Get a free block
	CurrentOldPageID = 0 ;


	printf("Threshol=%d\n",THRESHOLD);
    //GC time set 0
//    printf("block %d\n",FlashMemory[0][128]);
	//* Randomly select where are the data stored in the flash memory initially
	//* according the parameter "InitialPercentage" to determine what's the percentage of data in ths flash memory initially.
	BlockMaxRandomValue = (int)((float)InitialPercentage * (RAND_MAX) / 100);	//* The max random number equals "InitialPercentage" percentage of data.
	for(i=0; i<(MaxBlock-LeastFreeBlocksInFBL()-BadBlockCnt); i++)
	{
		if( rand() <= BlockMaxRandomValue)
		{
			//PageMaxRandomValue = rand();	//* The maximal number of pages storing initial data is from 0 to MaxPage
			for(j=0;j<MaxPage;j++)
			{
			//	if(rand() <= PageMaxRandomValue)
				{
					NewDataWriteOneCluster(i*MaxPage+j);

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
	   // for(i=0;i<6;i++){
		//* Check each Write request is a new write or an update and then update the mapping information
		for(CurrentCluster=CurrentRequest.StartCluster; CurrentCluster<(CurrentRequest.StartCluster+CurrentRequest.Length); CurrentCluster++)
		{
			if(CurrentRequest.AccessType == WriteType)
			{
				NewDataWriteOneCluster(CurrentCluster);//* Write a cluster to the flash memory and also update the FTL table. We suggest this function to write a cluster to the flash memory and also update the FTL table
                clustercnt++;
                if(clustercnt%100000000==0){
                    clock_t time;
                    time=clock();
                    time=time/1000;
                    printf("MaxErase=%d,time=%d,resetime=%d,watchlist=%d\nBlock cnt=%lld,page cnt=%lld,live page copy=%lld,cluster=%lld\n",
                           MaxBlockEraseCnt,time,reset,BlockLentgth(watchinglist),AccessStatistics.TotalBlockEraseCount,
                           AccessStatistics.TotalPageEraseCount,AccessStatistics.TotalLivePageCopyings,clustercnt);


                    reset=0;
                }
          //      printf("cluster %d\n",CurrentCluster);
			}
			else if(CurrentRequest.AccessType == ReadType)
			{
				//if(FTLTable[CurrentCluster].Block == FTL_POINT_TO_NULL) break;	//# the accessed ClusterID, i.e., the accessed logical address, doesn't exist
				if(BFTLReadOneCluster(CurrentCluster) == False) break;	//* Read a cluster to the flash memory and also update the FTL table. We suggest this function to write a cluster to the flash memory and also update the FTL table
			}

		}	//* end of for

		//# Statistics
		if(CurrentCluster == (CurrentRequest.StartCluster+CurrentRequest.Length)) //# if the request is a legal command
		{
			if(CurrentRequest.AccessType == WriteType) AccessStatistics.TotalWriteRequestCount ++;	//# advance the number of total write requests
			if(CurrentRequest.AccessType == ReadType) AccessStatistics.TotalReadRequestCount ++;	//# advance the number of total read requests
		}
		//printf("Threshold=%d,Space=%d\n",MaxPage*MaxBlock/20,GCTriggerSpace);
        if(GCTriggerSpace<=MaxPage*MaxBlock*3/40){

            BFTLGarbageCollection();	//* Do garbage collection to reclaim one more free block if there is no free block in the free block list

      //      }
        }
   /*     if(MaxBlockEraseCnt%250==0&&switchoff==MaxBlockEraseCnt/250){
            printf("MaxEraseCnt=%d,cluster=%lld\n",MaxBlockEraseCnt,clustercnt);
            printf("total page cnt=%lld\n",AccessStatistics.TotalPageEraseCount);
            printf("total block cnt=%lld\n",AccessStatistics.TotalBlockEraseCount);
            printf("total live page copy=%lld\n",AccessStatistics.TotalLivePageCopyings);
            requesttime=requesttime/1000000;
            printf("Total request time=%lld sec\n",requesttime);
            float responsetime=((float)requesttime/(float)clustercnt);
            responsetime=responsetime*1000;
            printf("Avg response time=%f ms\n",responsetime);
            requesttime=requesttime*1000000;
            switchoff++;
        }*/




	}	//* end of while

	//* ******************************* End of simulation *****************************
 /*   for(i=0;i<MaxBlock;i++){
        for(j=0;j<MaxPage;j++){

            if(PageEraseCnt[i][j]>=16){
            printf("Block=%d,Page=%d,PageEraseCnt=%d\n",i,j,PageEraseCnt[i][j]);
            }
        }
    }
*/



    printf("MaxEraseCnt=%d,cluster=%lld\n",MaxBlockEraseCnt,clustercnt);
    printf("Newdatawrite cnt=%lld,Olddatawrite cnt=%lld\n",newtimes,oldtimes);
    printf("watchlist cnt=%d,overflow=%d,reset time=%d\n",BlockLentgth(watchinglist),overflow,reset);

    printf("total page cnt=%lld\n",AccessStatistics.TotalPageEraseCount);
    printf("total block cnt=%lld\n",AccessStatistics.TotalBlockEraseCount);
    printf("total live page copy=%lld\n",AccessStatistics.TotalLivePageCopyings);
    requesttime=requesttime/1000000;
    printf("Total request time=%lld sec\n",requesttime);
    float responsetime=((float)requesttime/(float)clustercnt);
    responsetime=responsetime*1000;
    printf("Avg response time=%f ms\n",responsetime);
 //   printf("Avg response time=%f ms\n",responsetime);
  //  printf("time=%d\n",AvgTime);
	free(BFTLTable);	//* release memory space
    free(BlockType);
}




