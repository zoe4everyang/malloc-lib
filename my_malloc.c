#include "my_malloc.h"

fb * first_free_block = NULL;  // head of the free block list
__thread fb * first_free_block_nolock = NULL;  // head of the free block list for no lock version
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


/* Add Thread safe to malloc */

//Thread Safe malloc/free: locking version
void *ts_malloc_lock(size_t size){
    int isLock = 1;
    pthread_mutex_lock(&lock);
    void * block = bf_malloc(size, isLock);
    pthread_mutex_unlock(&lock);
    return block;

}

void ts_free_lock(void *ptr){
    int isLock = 1;
    pthread_mutex_lock(&lock);
    bf_free(ptr, isLock);
    pthread_mutex_unlock(&lock);
    
}

//Thread Safe malloc/free: non-locking version
void *ts_malloc_nolock(size_t size){
    int isLock = 0;
    void * block = bf_malloc(size, isLock);
    return block;
}
void ts_free_nolock(void *ptr){
    int isLock = 0;
    bf_free(ptr, isLock);
}


/******************
    Malloc
******************/


void * ff_malloc(size_t size, int isLock){
    return malloc_with_type(size, isLock, 'f');  // type 'f' indicate first fit approach for malloc
}

void * bf_malloc(size_t size, int isLock){
    return malloc_with_type(size, isLock, 'b');  // type 'b' indicate best fit approach for malloc
}

void * malloc_with_type(size_t size, int isLock, char type){
    fb * p = NULL;
    if(isLock == 1){
        p = first_free_block;
    }else{
        p = first_free_block_nolock;
    }
    
    // first fit: find the first free block that's big enough
    if(type == 'f'){
        while(p != NULL){
            if(p->size >= size){
               return reuse_free(size, p, isLock);
            }
            p = p->next;
        }
    // best fit
    }else if(type == 'b'){
        fb * min_match = NULL;
        // loop free list to find the best fit free block (least waste)
        while(p != NULL){
            if(p->size >= size && (min_match == NULL || p->size < min_match->size)){
                min_match = p;
                // stop looping if the best fit is found
                if(p->size == size) break;
            }
            p = p->next;
        }
        // best fit found, reuse this free block
        if (min_match != NULL){
            return reuse_free(size, min_match, isLock);
        }
    }
    // otherwise, sbrk a new block 
    return allocate_new(size, isLock);
}


void * reuse_free(size_t size, fb * match, int isLock){
    // calculate if the remaining size of the free block are enough for another malloc (> sizeof(fb))
    int remain_size = match->size - size - sizeof(fb);
    // if so, split, remove used part, and add remaining free block to the free list
    if(remain_size > 0){
        fb * remain_free = (fb *)((char *)match + sizeof(fb) + size); //(match + 1)
        match->size = size;
        remain_free->size = remain_size;
        remain_free->prev = match->prev;
        remain_free->next = match->next;
        remove_free(match, isLock);
        add_remaining_free(remain_free, isLock);
    }
    // otherwise, remove the whole free block from the free list
    else{
        remove_free(match, isLock);
    }
    return match + 1;    
}

// add remaining free block back to free list after reusing part of the free block
void add_remaining_free(fb * ptr, int isLock){ 
    fb * first = NULL;
    if(isLock == 1){
        first = first_free_block;
    }else{
        first = first_free_block_nolock;
    }
    // insert to the middle (not the first)
    if(ptr->prev != NULL){
        ptr->prev->next = ptr;
    }
    // insert to the front
    else if(ptr->prev == NULL || ptr < first){
        first = ptr;
    }

    // insert middle (not last)
    if(ptr->next != NULL){
        ptr->next->prev = ptr;
    }
}

// use sbrk() to claim a new block from the heap
void * allocate_new(size_t size, int isLock){
    fb * new_block = NULL;
    if(isLock == 1){
        new_block = sbrk(sizeof(fb) + size);
    }else{ // isLock == 0
        pthread_mutex_lock(&lock);
        new_block = sbrk(sizeof(fb) + size);
        pthread_mutex_unlock(&lock);
    }
    
    if(new_block == (void *) -1){
        perror("sbrk failed\n");
    }
    // update metadatta
    new_block->size = size;
    new_block->prev = NULL;
    new_block->next = NULL;
    return new_block + 1;
}



/* 
Usages:
1. to reuse an existing free block 
2. when merging adjacent free blocks
*/
void remove_free(fb * match, int isLock){
    fb * first = NULL;
    if(isLock == 1){
        first = first_free_block;
    }else{
        first = first_free_block_nolock;
    }

    // only 1 block in freelist and the first one is to be removed
    if(match == first && match->next == NULL){
        first = NULL;
    }
    // more than 1 in freelist and the first one is to be removed
    else if(match == first){
        first = match->next;
        first->prev = NULL;
    }
    // to remove the last block in freelist
    else if(match->next == NULL){
        match->prev->next = NULL;
    }
    // remove a block in the middle of the freelist
    else{
        match->prev->next = match->next;
        match->next->prev = match->prev;
    }
    // unlink the block from the freelist
    match->prev = match->next = NULL;
}


/************
    Free
*************/

/* ff_free and bf_free has the same implementation */
void ff_free(void * ptr, int isLock){
    free_both_type(ptr, isLock);
}

void bf_free(void * ptr, int isLock){
    free_both_type(ptr, isLock);
}

void free_both_type(fb *ptr, int isLock){
    fb * free_ptr = ptr - 1; 
    add_free(free_ptr, isLock);  // add the new free block to free list

    // merge with previous free block if adjacent
    if(free_ptr->prev && (char *)(free_ptr->prev + 1) + free_ptr->prev->size == (char *)free_ptr){
        free_ptr->prev->size += free_ptr->size + sizeof(fb);
        free_ptr = free_ptr->prev;  // update the pointer to point at the start of the merged block
        remove_free(free_ptr->next, isLock);
    }
     // merge with next free block if adjacent
    if(free_ptr->next && (char *)(free_ptr + 1) + free_ptr->size == (char *)free_ptr->next){
        free_ptr->size += free_ptr->next->size + sizeof(fb);
        remove_free(free_ptr->next, isLock);
    }
}

void add_free(fb * ptr, int isLock){
    fb * first = NULL;
    if(isLock == 1){
        first = first_free_block;
    }else{
        first = first_free_block_nolock;
    }
    
    //add to the first
    if(first == NULL || ptr < first){
        ptr->prev = NULL;
        ptr->next = first;
        first = ptr;
        if(ptr->next != NULL){
            ptr->next->prev = ptr;
        }
    }
    // not the first, iterate the free list
    else{
        while(first != NULL){
            // found right position, insert in the middle
            if(ptr < first){ 
                ptr->next = first;
                ptr->prev = first->prev;
                first->prev = ptr;
                ptr->prev->next = ptr;
                break;
            }
            // not found, add to the last
            else if(first->next == NULL){ 
                ptr->prev = first;
                ptr->next = NULL;
                first->next = ptr;
                break;
            }
            first = first->next;
        }
    }
}


unsigned long get_total_free_size(){
    int total = 0;
    fb * p = first_free_block;
    while(p != NULL){
        total += p->size;
        p = p->next;
    }
    return total;
}

unsigned long get_largest_free_data_segment_size(){
    int largest = 0;
    fb * p = first_free_block;
    while(p != NULL){
        if(p->size > largest){
            largest = p->size;
        }
        p = p->next;
    }
    return largest;
}

/* for testing */
// void print_all_free(){
//     fb * p = first_free_block;
//     int counter = 1;
//     printf("=============\n");
//     while(p != NULL){
//         printf("%d: start at %p, size is %zu\n", counter, p, p->size);
//         counter++;
//         p = p->next;
//     }

// }

