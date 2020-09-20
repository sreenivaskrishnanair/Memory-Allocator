/******************************************************************************
 * FILENAME: mem.c
 * AUTHOR:   cherin@cs.wisc.edu <Cherin Joseph>
 * DATE:     20 Nov 2013
 * PROVIDES: Contains a set of library functions for memory allocation
 * *****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "mem.h"
#include <assert.h>
#include <stdlib.h>

int fit;

/* this structure serves as the header for each block */
typedef struct block_hd
{
    /* The blocks are maintained as a linked list */
    /* The blocks are ordered in the increasing order of addresses */
    struct block_hd *next;

    /* size of the block is always a multiple of 4 */
    /* ie, last two bits are always zero - can be used to store other information*/
    /* LSB = 0 => free block */
    /* LSB = 1 => allocated/busy block */

    /* So for a free block, the value stored in size_status will be the same as the block size*/
    /* And for an allocated block, the value stored in size_status will be one more than the block size*/

    /* The value stored here does not include the space required to store the header */

    /* Example: */
    /* For a block with a payload of 24 bytes (ie, 24 bytes data + an additional 8 bytes for header) */
    /* If the block is allocated, size_status should be set to 25, not 24!, not 23! not 32! not 33!, not 31! */
    /* If the block is free, size_status should be set to 24, not 25!, not 23! not 32! not 33!, not 31! */
    int size_status;

} block_header;

/* Global variable - This will always point to the first block */
/* ie, the block with the lowest address */
block_header *list_head = NULL;

/**
 * Variable to keep track of heap size
 * */
 //int allocsize = 0;

 /* Function used to Initialize the memory allocator */
 /* Not intended to be called more than once by a program */
 /* Argument - sizeOfRegion: Specifies the size of the chunk which needs to be allocated
            policy: indicates the policy to use eg: best fit is 0*/
            /* Returns 0 on success and -1 on failure */
int Mem_Init(int sizeOfRegion, int policy)
{
    int pagesize;
    int padsize;
    int fd;
    int alloc_size;
    void *space_ptr;
    static int allocated_once = 0;

    if (0 != allocated_once)
    {
        fprintf(stderr, "Error:mem.c: Mem_Init has allocated space during a previous call\n");
        return -1;
    }
    if (sizeOfRegion <= 0)
    {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    /* Get the pagesize */
    pagesize = getpagesize();

    /* Calculate padsize as the padding required to round up sizeOfRegion to a multiple of pagesize */
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    alloc_size = sizeOfRegion + padsize;

    /* Using mmap to allocate memory */
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd)
    {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    space_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == space_ptr)
    {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }

    allocated_once = 1;

    /* To begin with, there is only one big, free block */
    list_head = (block_header *)space_ptr;
    list_head->next = NULL;
    /* Remember that the 'size' stored in block size excludes the space for the header */
    list_head->size_status = alloc_size - (int)sizeof(block_header);
    fit = policy;
    // allocsize = list_head->size_status;
    return 0;
}

int isFree(block_header *ptr)
{
    //If size status is even then it is free
    if (ptr->size_status % 2 == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void setFree(block_header *ptr)
{
    if (!isFree(ptr))
    {
        ptr->size_status = ptr->size_status - 1;
    }
}

void setAllocated(block_header *ptr)
{
    if (isFree(ptr))
    {
        ptr->size_status = ptr->size_status + 1;
    }
}

/* Function for allocating 'size' bytes. */
/* Returns address of allocated block on success */
/* Returns NULL on failure */
/* Here is what this function should accomplish */
/* - Check for sanity of size - Return NULL when appropriate */
/* - Round up size to a multiple of 4 */
/* - Traverse the list of blocks and allocate the best free block which can accommodate the requested size */
/* -- Also, when allocating a block - split it into two blocks when possible */
/* Tips: Be careful with pointer arithmetic */
void *Mem_Alloc(int size)
{
    /** Checking for Sanity
     * 1. Cannot allocate memory if size requested is non positive
     * 2. Cannot alloctae memory if no memory is left
    */
    if (size <= 0)// || size > list_head->size_status)
    {
        return NULL;
    }


    //Round up size to be divisible by 4
    if (size % 4 != 0)
    {
        size = (size | 0x03) + 1;
    }

    //Regardless of policy head initialization is gonna be the same
    //Allocate head
    if (list_head->next == NULL)
    {

        void *temp_pointer = (void *)list_head;
        int size_head = list_head->size_status;
        block_header *new_block = (block_header *)(temp_pointer +
            size + sizeof(block_header));
        size += 0x1; // indicate block is in use
        list_head->size_status = size;

        //Link the blocks
        list_head->next = new_block;
        new_block->next = NULL;
        new_block->size_status = size_head - sizeof(block_header) - size + 1;
        // void *math_pointer1 = (void *)list_head;
        // void *math_pointer2 = (void *)list_head->next;
        // printf("----||%d|||-----\n", math_pointer2 - math_pointer1);
        return list_head + 1;
    }

    switch (fit)
    {
    case 0: { //When a best fit Policy is followed
        /**
         * In this policy:
         * 1. Start at head and traverse the list to see size
         * 2. make array of that size
         * 3. input sizes into that array.
        */
        int array_size = 0;
        block_header *counter = list_head;
        while (counter != NULL)
        {
            array_size++;
            counter = counter->next;
        }
        int array_list[array_size];
        //fill the array
        counter = list_head;
        int j =0;
        while (counter != NULL)
        {
            array_list[j] = counter->size_status;
            counter = counter->next;
            j++;
        }

        //Of the array Elements find the min even number
        int min = -1;
        //Assign min a number
        for (int i = 0; i < array_size; i++) {
            if ((array_list[i] % 2 ==0)) {
                min = array_list[i];
                break;
            }
        }
        if (min == -1) {//No space in array
            return NULL;
        }

        //Search for smallest min

        for (int i = 0; i < array_size; i++) {
            if ((array_list[i] % 2 ==0) && array_list[i] < min
                && min >= size) {
                min = array_list[i];
            }
        }


        //Placement obtained based on min & size;
        //Placement obtained - Decide splitting 
        int demand = size;
        int available = min;
        if (demand == available) {//The requirement is an exact fit
        //filter through list for that position and allocate
            counter = list_head;
            while (counter != NULL) {
                if (counter->size_status == size) {
                    //Required position found
                    //Allocate space
                    size += 0x1; // indicate block is in use
                    counter->size_status = size;
                    return counter + 1;
                }
                counter = counter->next;
            }

        }
        else if (available < (demand + sizeof(list_head) + sizeof(list_head)/2)) {
            //Not enough space to spill, enough to allocate
            //size += 0x1; // indicate block is in use


            counter = list_head;
            while (counter != NULL) {

                if (counter->size_status == size) {
                    //Required position found
                    //Allocate space
                    size += 0x1; // indicate block is in use
                    counter->size_status = counter->size_status + 1;
                    return counter + 1;
                }
                counter = counter->next;
            }


        }
        else {

            //Enough space for splitting
            //Process of splitting
            counter = list_head;
            while (counter != NULL) {
                if (counter->size_status == min) {
                    // printf("--------mEEEEEEEEEE123------\n");
                    // printf("--------min:%d------\n", size);
                    //If block found do the process of splitting
                    void *temp_pointer = (void *)counter;
                    int size_counter = counter->size_status;
                    block_header *new_block = (block_header *)(temp_pointer +
                        size + sizeof(block_header));
                    size += 0x1; // indicate block is in use
                    counter->size_status = size;
                    //Link the blocks
                    block_header *next_block = counter->next;
                    counter->next = new_block;
                    //Two conditions-may be at end of list
                    //1. new block is last block
                    //2. new block in middle
                    if (next_block == NULL) {//End of list
                        // printf("-------------MEE---------\n");
                        // printf("----------%d------------\n", size_counter);
                        new_block->next=NULL;
                        new_block->size_status = size_counter -
                            sizeof(block_header) - (size-1);
                        return counter + 1;
                    }
                    else { //Not end of list
                    //Check after remove()!!!!!
                       // printf("-------------MEE!!!---------\n");
                        new_block->next = counter->next;
                        void *start_p = (void *)counter;
                        void *end_p = (void *)counter->next;
                        new_block->size_status = (end_p - start_p) -
                            2*sizeof(block_header) - (size-1);
                        return counter + 1;
                    }

                }

                counter = counter->next;
            }

        }






        break;}
    case 1: {
        //When a policy of First Fit is followed
            /**
             * In this policy:
             * 1. Start at head and traverse the list
             * 2. if a good spot is found see if it can be split
             * 3. if yes split (space for head and atleast 4bytes)-return
             * 4. if not alloc whole space-retun
            */
            //Step1:
        block_header *counter = list_head;
        while (counter != NULL)
        {

            //Step2-check for spot
            // if ()
            // {
            //     /* code */
            // }
            if (isFree(counter) && (counter->size_status == size)) {
               // printf("-----i was called----\n");
                size += 0x1; // indicate block is in use
                counter->size_status = size;
                return counter + 1;

            }


            if (isFree(counter) && (counter->size_status > size)) {
                //Step3-Check if it can be split
                //it can be split if it can accomodate atleast
                //size + one head + 0.5 head min size 4
                int demand = size;
                int available = counter->size_status;
                int pinch = available - demand;

                if (counter->size_status >=  size + sizeof(block_header)
                    + sizeof(block_header)/2) {
                    //Continue spliting 
                    void *temp_pointer = (void *)counter;
                    int size_counter = counter->size_status;
                    block_header *new_block = (block_header *)(temp_pointer +
                        size + sizeof(block_header));
                    size += 0x1; // indicate block is in use
                    counter->size_status = size;
                    //Link the blocks
                    block_header *next_block = counter->next;
                    counter->next = new_block;
                    //Two conditions-may be at end of list
                    //1. new block is last block
                    //2. new block in middle
                    if (next_block == NULL) {//End of list
                        // printf("-------------MEE---------\n");
                        // printf("----------%d------------\n", size_counter);
                        new_block->next=NULL;
                        new_block->size_status = size_counter -
                            sizeof(block_header) - (size-1);
                        return counter + 1;
                    }
                    else { //Not end of list
                    //Check after remove()!!!!!
                        // printf("-------------MEE!!!---------\n");
                        new_block->next = counter->next;
                        void *start_p = (void *)counter;
                        void *end_p = (void *)counter->next;
                        new_block->size_status = (end_p - start_p) -
                            2*sizeof(block_header) - (size-1);
                        return counter + 1;
                    }

                }
                //Just allocate without splitting
                //Two conditions when thats possible
                //demand== available
                // else if (isFree(counter) && (counter->size_status >= size) &&
                //     (counter->size_status ==  size + sizeof(block_header)
                //         + sizeof(block_header)/2)) {
                //     printf("-------------MEE-yoo--------\n");


                //     size += 0x1; // indicate block is in use
                //     counter->size_status = size;
                //     return counter + 1;
                // }
                //pinch = avialable - demand
                //pinch<12 && pinch > 0
                // available = counter->size_status
                //This cannnot be allowed to happen on the last block

                else if (isFree(counter) && available > demand && pinch >0
                    && pinch < 12 && counter->next != NULL) {
                    // printf("-------------MEE-yoo-pinch=%d-------\n", pinch);
                    // printf("-------------MEE-yoo-available=%d-------\n", available);


                    size += 0x1; // indicate block is in use
                    counter->size_status = counter->size_status + 1;
                    return counter + 1;
                }




            }
            // else{
            //     printf("here");

            // }

            counter = counter->next;
        }
        //If it traversed through the entire list and couldnt find a place
        //Then size > any free slot. In which case return NULL
        //Sanity check Part 2
        return NULL;
        break;}
    case 2: { //When a worst fit Policy is followed
        int array_size = 0;
        block_header *counter = list_head;
        while (counter != NULL)
        {
            array_size++;
            counter = counter->next;
        }
        int array_list[array_size];
        //fill the array
        counter = list_head;
        int j =0;
        while (counter != NULL)
        {
            array_list[j] = counter->size_status;
            counter = counter->next;
            j++;
        }

        int max = -1;
        //Assign min a number
        for (int i = 0; i < array_size; i++) {
            if ((array_list[i] % 2 ==0)) {
                max = array_list[i];
                break;
            }
        }
        if (max == -1) {//No space in array
            return NULL;
        }

        //Search for largest max

        for (int i = 0; i < array_size; i++) {
            if ((array_list[i] % 2 ==0) && array_list[i] > max
                && max >= size) {
                max = array_list[i];
            }
        }

        //Placement obtained based on max & size;
        //Placement obtained - Decide splitting 
        int demand = size;
        int available = max;
        if (demand == available) {//The requirement is an exact fit
        //filter through list for that position and allocate
            counter = list_head;
            while (counter != NULL) {
                if (counter->size_status == size) {
                    //Required position found
                    //Allocate space
                    size += 0x1; // indicate block is in use
                    counter->size_status = size;
                    return counter + 1;
                }
                counter = counter->next;
            }

        }
        else if (available < (demand + sizeof(list_head) + sizeof(list_head)/2)) {
            //Not enough space to spill, enough to allocate
            //size += 0x1; // indicate block is in use


            counter = list_head;
            while (counter != NULL) {

                if (counter->size_status == size) {
                    //Required position found
                    //Allocate space
                    size += 0x1; // indicate block is in use
                    counter->size_status = counter->size_status + 1;
                    return counter + 1;
                }
                counter = counter->next;
            }


        }
        else {

            //Enough space for splitting
            //Process of splitting
            counter = list_head;
            while (counter != NULL) {
                if (counter->size_status == max) {
                    // printf("--------mEEEEEEEEEE123------\n");
                    // printf("--------max:%d------\n", size);
                    //If block found do the process of splitting
                    void *temp_pointer = (void *)counter;
                    int size_counter = counter->size_status;
                    block_header *new_block = (block_header *)(temp_pointer +
                        size + sizeof(block_header));
                    size += 0x1; // indicate block is in use
                    counter->size_status = size;
                    //Link the blocks
                    block_header *next_block = counter->next;
                    counter->next = new_block;
                    //Two conditions-may be at end of list
                    //1. new block is last block
                    //2. new block in middle
                    if (next_block == NULL) {//End of list
                        // printf("-------------MEE---------\n");
                        // printf("----------%d------------\n", size_counter);
                        new_block->next=NULL;
                        new_block->size_status = size_counter -
                            sizeof(block_header) - (size-1);
                        return counter + 1;
                    }
                    else { //Not end of list
                    //Check after remove()!!!!!
                        // printf("-------------MEE!!!---------\n");
                        new_block->next = counter->next;
                        void *start_p = (void *)counter;
                        void *end_p = (void *)counter->next;
                        new_block->size_status = (end_p - start_p) -
                            2*sizeof(block_header) - (size-1);
                        return counter + 1;
                    }

                }

                counter = counter->next;
            }

        }




        break;
    }
    default:
        break;
    }

    return 0;
    /* Your code should go in here */
}

block_header * combine(block_header *p1, block_header *p2) {
    //By-pass p2- Combine
    p1->next = p2->next;
    p1->size_status = p1->size_status + p2->size_status + sizeof(block_header);
    return p1;
}

int inList(void *ptr) {
    void *math_pointer_start = (void *)list_head;

    //To get last element in list
    block_header *counter = list_head;
    while (counter->next != NULL)
    {
        counter = counter->next;
    }
    int size_final = 0;
    if (counter->size_status %2 != 0) {
        size_final = counter->size_status - 1;
    }
    else
    {
        size_final = counter->size_status;
    }
    void *math_pointer_end = (void *)(counter + 1) + size_final;
    if (ptr > math_pointer_end || ptr < math_pointer_start) {
        return 0;
    }
    return 1;
}

block_header * getPrevious(block_header * node) {
    if (node == list_head)
    {
        return NULL;
    }
    block_header *previous = NULL;
    block_header *find = list_head;
    while (find->next != NULL)
    {
        if (find->next == node) {
            previous = find;
            break;
        }
        find = find->next;
    }
    return previous;
}

block_header * getNext(block_header * node) {
    return node->next;
}




/* Function for freeing up a previously allocated block */
/* Argument - ptr: Address of the block to be freed up */
/* Returns 0 on success */
/* Returns -1 on failure */
/* Here is what this function should accomplish */
/* - Return -1 if ptr is NULL */
/* - Return -1 if ptr is not pointing to the first byte of a busy block */
/* - Mark the block as free */
/* - Coalesce if one or both of the immediate neighbours are free */
int Mem_Free(void *ptr)
{
    block_header *req_pointer = (block_header*)(ptr - sizeof(block_header));
    block_header *next_pointer = req_pointer;
    block_header *prev_pointer = req_pointer;

    //Null ptr return -1
    if (ptr == NULL) {
        return -1;
    }
    //Not in the list return -1
    if (!inList(ptr)) {
        return -1;
    }
    //Already free return -1;


    //if no such node return -1
    if (req_pointer == NULL) {
        return -1;
    }

    //if already pointing to free block return -1
    if (req_pointer->size_status %2 == 0) {
        return -1;
    }
    req_pointer->size_status = req_pointer->size_status -1;


    ///////free here

    block_header *next_pointer_tracker = req_pointer;
    while (next_pointer_tracker != NULL) //Keep searchinf until 
    {


        //&& next_pointer->size_status %2 == 0
        if (next_pointer_tracker->size_status %2 == 0) {
            next_pointer = next_pointer_tracker;
        }
        else
        {
            break;
        }

        next_pointer_tracker = getNext(next_pointer_tracker);
    }

    block_header *prev_pointer_tracker = req_pointer;
    while (prev_pointer_tracker != NULL) //Keep searchinf until 
    {
        if (prev_pointer_tracker->size_status %2 == 0) {
            prev_pointer = prev_pointer_tracker;
        }
        else
        {
            break;
        }
        //&& prev_pointer->size_status %2 == 0
        prev_pointer_tracker = getPrevious(prev_pointer_tracker);
    }

    //Merge all blocks from prev to next
    if (prev_pointer != next_pointer) {
        void *math_pointer_start = (void *)(prev_pointer+1);
        void *math_pointer_end = (void *)(next_pointer+1) + next_pointer->size_status;
        int new_size = math_pointer_end - math_pointer_start;
        prev_pointer->next = next_pointer->next;
        prev_pointer->size_status = new_size;

        return 0;
    }

    //If prev===next = required pointer
    //Implies only one block to free
    //no coalescing
    //prev_pointer->size_status = prev_pointer->size_status -1;
    return 0;

    //Forward direction 

}

/* Function to be used for debug */
/* Prints out a list of all the blocks along with the following information for each block */
/* No.      : Serial number of the block */
/* Status   : free/busy */
/* Begin    : Address of the first useful byte in the block */
/* End      : Address of the last byte in the block */
/* Size     : Size of the block (excluding the header) */
/* t_Size   : Size of the block (including the header) */
/* t_Begin  : Address of the first byte in the block (this is where the header starts) */
void Mem_Dump()
{
    int counter;
    block_header *current = NULL;
    char *t_Begin = NULL;
    char *Begin = NULL;
    int Size;
    int t_Size;
    char *End = NULL;
    int free_size;
    int busy_size;
    int total_size;
    char status[5];

    free_size = 0;
    busy_size = 0;
    total_size = 0;
    current = list_head;
    counter = 1;
    fprintf(stdout, "************************************Block list***********************************\n");
    fprintf(stdout, "No.\tStatus\tBegin\t\tEnd\t\tSize\tt_Size\tt_Begin\n");
    fprintf(stdout, "---------------------------------------------------------------------------------\n");
    while (NULL != current)
    {
        t_Begin = (char *)current;
        Begin = t_Begin + (int)sizeof(block_header);
        Size = current->size_status;
        strcpy(status, "Free");
        if (Size & 1) /*LSB = 1 => busy block*/
        {
            strcpy(status, "Busy");
            Size = Size - 1; /*Minus one for ignoring status in busy block*/
            t_Size = Size + (int)sizeof(block_header);
            busy_size = busy_size + t_Size;
        }
        else
        {
            t_Size = Size + (int)sizeof(block_header);
            free_size = free_size + t_Size;
        }
        End = Begin + Size;
        fprintf(stdout, "%d\t%s\t0x%08lx\t0x%08lx\t%d\t%d\t0x%08lx\n", counter, status, (unsigned long int)Begin,
            (unsigned long int)End, Size, t_Size, (unsigned long int)t_Begin);
        total_size = total_size + t_Size;
        current = current->next;
        counter = counter + 1;
    }
    fprintf(stdout, "---------------------------------------------------------------------------------\n");
    fprintf(stdout, "*********************************************************************************\n");

    fprintf(stdout, "Total busy size = %d\n", busy_size);
    fprintf(stdout, "Total free size = %d\n", free_size);
    fprintf(stdout, "Total size = %d\n", busy_size + free_size);
    fprintf(stdout, "*********************************************************************************\n");
    fflush(stdout);
    return;
}

int main()
{
    assert(Mem_Init(4096, 1) == 0);
    void* ptr[9];
    void* test;

    ptr[0] = Mem_Alloc(300);
    assert(ptr[0] != NULL);

    ptr[1] = Mem_Alloc(200);
    assert(ptr[1] != NULL);

    ptr[2] = Mem_Alloc(200);
    assert(ptr[2] != NULL);

    ptr[3] = Mem_Alloc(100);
    assert(ptr[3] != NULL);

    ptr[4] = Mem_Alloc(200);
    assert(ptr[4] != NULL);

    ptr[5] = Mem_Alloc(800);
    assert(ptr[5] != NULL);

    ptr[6] = Mem_Alloc(500);
    assert(ptr[6] != NULL);

    ptr[7] = Mem_Alloc(700);
    assert(ptr[7] != NULL);

    ptr[8] = Mem_Alloc(300);
    assert(ptr[8] != NULL);


    assert(Mem_Free(ptr[1]) == 0);


    assert(Mem_Free(ptr[3]) == 0);


    assert(Mem_Free(ptr[5]) == 0);


    assert(Mem_Free(ptr[7]) == 0);



    test = Mem_Alloc(50);

    assert(
        (
            ((unsigned long int)test >= (unsigned long int)ptr[1])
            &&
            ((unsigned long int)test < (unsigned long int)ptr[2])
            )
        ||
        (
            ((unsigned long int)test <= (unsigned long int)ptr[8])
            )
    );
    exit(0);
}
