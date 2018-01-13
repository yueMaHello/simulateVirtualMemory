
//  CMPUT 379 ass4
//  Created by Yue Ma on 2017-11-23.
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#define SIZE 2560000

int hashCode(int key);
struct DataItem *search(int key);
void insert(int key,int data);
int push(int value,int stack[],int STACK_SIZE,int top_element);
int pop(int stack[],int top_element);
int isEmpty(int stack[],int top_element_0);
int isFull(int stack[],int STACK_SIZE,int top_element_0);
int removeToTop(int value,int stack[],int top_element);
int removeElement(int index,int stack[],int top_element);

//hash table for 'none'
struct DataItem {
    int data;
    int key;
};

struct DataItem* hashArray[SIZE];
struct DataItem* item;
//top elements for different array
static int top_element = -1;
static int top_element_memory = -1;
static int top_element_flush = -1;
//data structure used in 'sec'
typedef struct {
    int val;
    int ref;
}secElement;
//outputs
int pageFaults = 0;
int pageHits = 0;
long int accumulator = 0;
int flush=0;

int main(int argc,char *argv[]){
    uint32_t binaryWord;
    int binaryWordSize;
    long int writeCount=0;
    int pagesize = atoi(argv[1]);
    //length of pagenumber
    int pageNumLength = 32 - log(pagesize)/log(2);
    int memsize = atoi(argv[2]);
    char* strategy = argv[3];
    long int totalRef = 0;
    time_t t0, t1;
    t0 = time(NULL);
    char secstr[128];
    struct tm delta_time;
    
    //memory stack size
    int STACK_SIZE = memsize/pagesize+(memsize % pagesize != 0);
    int flush_stack_size = STACK_SIZE;
    
    int stack[STACK_SIZE];
    int flushStack[flush_stack_size];
    int firstThreePages[3];
    memset(stack,0,sizeof(stack));
    memset(flushStack,0,sizeof(flushStack));
    memset(firstThreePages,0,sizeof(firstThreePages));
    
    secElement secList[STACK_SIZE];
    for(int j=0;j<STACK_SIZE;j++){
        secList[j].val = 0;
        secList[j].ref = 0;
    }
    int secPointer = 0;
    int secTop = -1;
    

    while(1){
        if((binaryWordSize = fread(&binaryWord, sizeof(binaryWord), 1, stdin)) <= 0){
            break;
        }
        totalRef+=1;
        //split 32 bits into several parts
        uint32_t pageNumberBinary = binaryWord>>(32-pageNumLength);
        uint32_t operationBinary = (binaryWord & 0x000000C0)>>6;
        uint32_t valueBinary = (binaryWord & 0x0000003f);
        
        int value =(int)valueBinary;
        int pageNumber = (int)pageNumberBinary;
        int operation = (int)operationBinary;
        //lru strategy
        if(strncmp("lru",strategy,strlen("lru"))==0){
            int found=0;
            int found_flush = 0;
            int poped_element =0;
            int pageNum =pageNumber;
            //deal with accumulator
            if(operation==0){
                accumulator += value;
            }
            else if(operation==1){
                accumulator -= value;
            }

            int i = top_element;
            if (i == -1) {
                i = -1;
            }
            else{
                while (i != -1) {
                    if(stack[i] == pageNum){
                        found = 1;
                        pageHits+=1;
                        //renew the position of the founded elements
                        //move the founded element to the top of the stack
                        top_element = removeToTop(pageNum,stack,top_element);
                        break;
                    }
                    i = i-1;
                }
            }
            //didn't find the element
            if (found == 0){
                pageFaults =pageFaults+1;
                //if not full
                if(!isFull(stack,STACK_SIZE,top_element)){
                    top_element = push(pageNum,stack,STACK_SIZE,top_element);

                }
                //if full
                else{
                    poped_element = stack[0];
                    top_element=pop(stack,top_element);
                    top_element=push(pageNum,stack,STACK_SIZE,top_element);

                }
            }
            //calculate flush
            if(poped_element!=0){
                //check whether it is in the flushStack
                int i = top_element_flush;
                if (i == -1) {
                    i = -1;
                }
                else{
                    while (i != -1) {
                        if(flushStack[i] == poped_element){
                            flush+=1;
                            //remove the element from the flushstack
                            top_element_flush=removeElement(i,flushStack,top_element_flush);
                            break;
                        }
                        i = i-1;
                    }
                }
            }
            if(operation==2){
                writeCount+=1;
                int exit_in_flush =0;
                int i = top_element_flush;
                if (i == -1) {
                    i = -1;
                }
                else{
                    while (i != -1) {
                        if(flushStack[i] == pageNum){
                            exit_in_flush = 1;
                            break;
                        }
                        i = i-1;
                    }
                }
                //the pagenum is not in the current flush stack
                if(exit_in_flush ==0){
                    if(top_element_flush>flush_stack_size){
                        perror("flush wrong");
                        exit(1);
                    }
                    //store in the flush stack
                    top_element_flush=push(pageNum,flushStack,flush_stack_size,top_element_flush);
                }
            }
        }
        else if (strncmp("sec",strategy,strlen("sec"))==0){
            int poped_element =0;
            if(operation==0){
                accumulator += value;
            }
            else if(operation==1){
                accumulator -= value;
            }
            int secPageNum = pageNumber;
            //not full, the pointer will always on the top element
            if(secTop < STACK_SIZE-1){
                int found = 0;
                for(int i=0;i<STACK_SIZE;i++){
                    if(secList[i].val ==secPageNum){
                        secList[i].ref= 1;
                        found = 1;
                        break;
                    }
                }
                //not full, not found
                //extend the stack
                if(found ==0){
                    pageFaults += 1;
                    secTop += 1;
                    secList[secTop].val = secPageNum;
                    secList[secTop].ref = 1;
                    secPointer = secTop;
                    
                }
            }
            //FULL
            else{
                int found = 0;
                for(int i=0;i<STACK_SIZE;i++){
                    if(secList[i].val ==secPageNum){
                        secList[i].ref= 1;
                        found = 1;
                        break;
                    }
                }
                //full, not found
                //find the first element with ref==0, then pointer moves to that position
                if(found ==0){
                    pageFaults +=1;
                    int refAllOnes = 1;
                    for (int i=0;i<STACK_SIZE;i++){
                        if(secList[i].ref == 0){
                            refAllOnes = 0;
                            break;
                        }
                    }
                    /*
                     //if all ref are ones
                     //this method is what I found on the internet
                    if(refAllOnes == 1 ){
                        //move pointer to the next position
                        if(secPointer == STACK_SIZE-1){
                            secPointer = 0;
                        }
                        else{
                            secPointer+=1;
                        }
                        
                        for(int i =0;i<STACK_SIZE;i++){
                            secList[i].ref = 0;
                        }
                        poped_element = secList[secPointer].val;
                        secList[secPointer].val =secPageNum;
                        secList[secPointer].ref = 1;
                    }*/
                
                    // if all ref are ones
                    //This method is what the professor suggests
                    if(refAllOnes == 1 ){
                        //replace the element which is pointed by the secPointer
                        if(secPointer == STACK_SIZE-1){
                            secPointer = STACK_SIZE-1;
                        }
                        
                        
                        for(int i =0;i<STACK_SIZE;i++){
                            secList[i].ref = 0;
                        }
                        poped_element = secList[secPointer].val;
                        secList[secPointer].val =secPageNum;
                        secList[secPointer].ref = 1;
                    }
                    //if there is some ref being zero
                    else{
                        int newPointerPosition= -1;
                        for(int i = secPointer;i<STACK_SIZE;i++){
                            if(secList[i].ref == 1){
                                secList[i].ref =0;
                            }
                            else{
                                newPointerPosition = i;
                                break;
                            }
                        }
                        if(newPointerPosition == -1){
                            for(int i =0;i<secPointer;i++){
                                if(secList[i].ref == 1){
                                    secList[i].ref =0;
                                }
                                else{
                                    newPointerPosition = i;
                                    break;
                                }
                                
                            }
                        }
                        secPointer = newPointerPosition;
                        poped_element = secList[secPointer].val;
                        secList[secPointer].ref = 1;
                        secList[secPointer].val = secPageNum;
                        
                        
                    }
                    
  
                    
                }
            }
            if(poped_element!=0){
                //check whether it is in the flushStack
                int i = top_element_flush;
                if (i == -1) {
                    i = -1;
                }
                else{
                    while (i != -1) {
                        if(flushStack[i] == poped_element){
                            flush+=1;
                            top_element_flush=removeElement(i,flushStack,top_element_flush);
                            break;
                        }
                        i = i-1;
                    }
                }
            }
            if(operation==2){
                writeCount+=1;
                int exit_in_flush =0;
                int i = top_element_flush;
                if (i == -1) {
                    i = -1;
                }
                else{
                    while (i != -1) {
                        if(flushStack[i] == pageNumber){
                            exit_in_flush = 1;
                            break;
                        }
                        i = i-1;
                    }
                }
                if(exit_in_flush ==0){
                    if(top_element_flush>flush_stack_size){
                        perror("flush wrong");
                        exit(1);
                    }
                    top_element_flush=push(pageNumber,flushStack,flush_stack_size,top_element_flush);
                }
            }
        }
        else if(strncmp("mrand",strategy,strlen("mrand"))==0){
           
            int poped_element;
            if(operation==0){
                accumulator += value;
            }
            else if(operation==1){
                accumulator -= value;
            }
            
            if(STACK_SIZE<3){
                perror("physical memory is less than 3 pages!");
                exit(1);
            }
            int found=0;
 
            int pageNum = pageNumber;
            int i = top_element;
            if (i == -1) {
                i = -1;
            }
            else{
                //check whether the page has already in the stack
                while (i != -1) {
                    if(stack[i] == pageNum){
                        found = 1;
                        break;
                    }
                    i = i-1;
                }
            }
            if (found == 0){
                pageFaults =pageFaults+1;
                if(!isFull(stack,STACK_SIZE,top_element)){
                    top_element=push(pageNum,stack,STACK_SIZE,top_element);
                }
                //not found, full stack
                else{
                    int r;
                    //find an element randomly, but the element shouldn't appear in the nearest 3 times
                    while(1){
                        int inMemory =0;
                        r = rand()%(STACK_SIZE-3);
                        if(r>(STACK_SIZE-3)){
                            perror("get random number faults");
                            exit(1);
                        }
                        //check wether it is in the memory
                        for(int i=0;i<3;i++){
                            if (firstThreePages[i]==stack[r]){
                                inMemory =1;
                                break;//while loop continue
                            }
                        }
                        if(inMemory==0){
                            break;//found
                        }
                    }
                    //record the removed element
                    poped_element = stack[r];
                    //push the new element
                    stack[r] = pageNum;
                }
            }
            //store memory (3 pages)
            if(top_element_memory<2){
                top_element_memory=push(pageNum,firstThreePages,3,top_element_memory);
            }
            else{
                top_element_memory=pop(firstThreePages,top_element_memory);
                top_element_memory=push(pageNum,firstThreePages,3,top_element_memory);
            }
            //deal with flushes
            if(poped_element!=0){
                //check whether it is in the flushStack
                int i = top_element_flush;
                if (i == -1) {
                    i = -1;
                }
                else{
                    while (i != -1) {
                        if(flushStack[i] == poped_element){
                            flush+=1;
                            top_element_flush=removeElement(i,flushStack,top_element_flush);
                            break;
                        }
                        i = i-1;
                    }
                }
            }
            //if the page is modified
            if(operation==2){
                writeCount+=1;
                int exit_in_flush =0;
                int i = top_element_flush;
                if (i == -1) {
                    i = -1;
                }
                else{
                    while (i != -1) {
                        if(flushStack[i] == pageNum){
                            exit_in_flush = 1;
                            break;
                        }
                        i = i-1;
                    }
                }
                if(exit_in_flush ==0){
                    if(top_element_flush>flush_stack_size){
                        perror("flush wrong");
                        exit(1);
                    }
                    top_element_flush=push(pageNum,flushStack,flush_stack_size,top_element_flush);
                }
                
            }
        }

        else if(strncmp("none",strategy,strlen("none"))==0){
            if(operation==0){
                accumulator += value;
            }
            else if(operation==1){
                accumulator -= value;
            }
            else if(operation == 2){
                
                writeCount+=1;
               
            }
            item = search(pageNumber);
            if(item == NULL){
                pageFaults+=1;
                insert(pageNumber,0);
            }
            
        }
    }
    t1 = time(NULL);

    time_t delta_secs = t1-t0;
    memset(secstr, 0, sizeof(secstr));
    
    localtime_r(&delta_secs, &delta_time);
    strftime(secstr, sizeof(secstr), "%S.%02d", &delta_time);
    

    printf("[a4vmsim] %ld references processed using '%s' in %s sec.\n",
           totalRef, strategy, secstr);
    printf("[a4vmsim] page faults= %d, write count= %ld, flushes= %d\n",
           pageFaults, writeCount, flush);
    printf("[a4vmsim] Accumulator= %ld\n", accumulator);
    
    exit(EXIT_SUCCESS);
    return 1;
}

int push(int value,int stack[],int STACK_SIZE,int top_element_0){
    if (!isFull(stack,STACK_SIZE,top_element_0)) {
        stack[++top_element_0] = value;
    }
    return top_element_0;
}


int pop(int stack[],int top_element_0){
    if (!isEmpty(stack,top_element_0)) {
        for (int i = 0; i < top_element_0; ++i){
            stack[i] = stack[i + 1];}
        top_element_0--;
    }
    return top_element_0;
}

int isEmpty(int stack[],int top_element_0){
    return top_element_0 == -1;
}

int isFull(int stack[],int STACK_SIZE,int top_element_0){
    return top_element_0 == STACK_SIZE - 1;
}

int removeToTop(int value,int stack[],int top_element_0){
    int found = 0;
    int index;
    int i = top_element_0;
    if (i == -1) {
        i = -1;
    }else{
        while (i != -1) {
            if(stack[i] == value){
                found = 1;
                index = i;
                break;
            }
            i = i-1;
        }
    }
    if(found == 1){
        
        for (int i = index; i < top_element_0; ++i){
            stack[i] = stack[i + 1];}
        stack[top_element_0] = value;
    }
    return top_element_0;
}

int removeElement(int index,int stack[],int top_element_0){
    for (int i = index; i < top_element_0; ++i){
        stack[i] = stack[i + 1];
    }
    stack[top_element_0] = 0;
    top_element_0-=1;
    return top_element_0;
}

/*hash table functions*/
int hashCode(int key) {
    return key % SIZE;
}
//search in hash table
struct DataItem *search(int key) {
    //get the hash
    int hashIndex = hashCode(key);
    
    //move in array until an empty
    while(hashArray[hashIndex] != NULL) {
        
        if(hashArray[hashIndex]->key == key)
            return hashArray[hashIndex];
        
        //go to next cell
        ++hashIndex;
        
        //wrap around the table
        hashIndex %= SIZE;
    }
    
    return NULL;
}
//insert into hash table
void insert(int key,int data) {
    
    struct DataItem *item = (struct DataItem*) malloc(sizeof(struct DataItem));
    item->data = data;
    item->key = key;
    
    //get the hash
    int hashIndex = hashCode(key);
    
    //move in array until an empty or deleted cell
    while(hashArray[hashIndex] != NULL && hashArray[hashIndex]->key != -1) {
        //go to next cell
        ++hashIndex;
        
        //wrap around the table
        hashIndex %= SIZE;
    }
    
    hashArray[hashIndex] = item;
}




