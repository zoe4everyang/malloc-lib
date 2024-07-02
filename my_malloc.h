#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

typedef struct free_block{
    struct free_block * next;
    struct free_block * prev;
    size_t size;
} fb;



/* malloc */

/*
malloc with first fit approach / best fit apprach
@param: malloc size 
@return: starting address of the allocated memory (end of the header part)
*/
void * ff_malloc(size_t size, int isLock);
void * bf_malloc(size_t size, int isLock);

void * malloc_with_type(size_t size, int isLock, char type);
void * reuse_free(size_t size, fb * match, int isLock);
void * allocate_new(size_t size, int isLock);
void remove_free(fb * match, int isLock);
void add_remaining_free(fb * ptr, int isLock);


/* free */

/*
both free the indicated block
@param: pointer of the block to free (end of the header part)
*/
void ff_free(void * ptr, int isLock);
void bf_free(void * ptr, int isLock);

void free_both_type(fb *ptr, int isLock);
void add_free(fb * ptr, int isLock);



/* performance */

unsigned long get_total_free_size();           
unsigned long get_largest_free_data_segment_size();
  

////void print_all_free();




/* Add Thread safe to malloc */

//Thread Safe malloc/free: locking version
void *ts_malloc_lock(size_t size);
void ts_free_lock(void *ptr);

//Thread Safe malloc/free: non-locking version
void *ts_malloc_nolock(size_t size);
void ts_free_nolock(void *ptr);
