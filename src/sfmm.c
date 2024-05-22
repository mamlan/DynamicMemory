/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include <errno.h>

#define MIN_SIZE 32

void *add_to_ql(size_t size);
void *add_to_main(size_t size);
void *coalesce(sf_block* block, sf_block* next_blk);
void *init(size_t);
int get_list(size_t size);
void set_prologue();
void set_epilogue(size_t size);
int check_free(void *pp);
size_t get_size(int i);
void set_footer( sf_block *block, size_t size, sf_header *header);
void split_block(sf_block *head, size_t new_block_size, size_t size);


int bytes_rem, total_size=0, free_index;
sf_block *cur_block, *first_block;
char *addr_ptr;

void *sf_malloc(size_t size) {

    if(size==0) return NULL;

    size_t rblock_size= (size<32) ? 32 : ((size+15)/16)*16; 
    if((char*)sf_mem_start()-(char*)sf_mem_end()==0){   //heap is empty
        cur_block= init(size);
        return add_to_main(rblock_size);
    }

    //search QL
    void* res;
    res = add_to_ql(rblock_size);
    if(res!=NULL) return res;

    //else search ML
    return add_to_main(rblock_size);

    //last 8 bytes of heap must contain epilogue
}

void sf_free(void *pp) {
    //footer needs to be set in case of free block
    printf("inside sf_free\n");
    int val = check_free(pp);
    if(val==-1) abort();

    //search quick list
    //if ql is full, flush the ql and add to ml (coalesce if possible)
    //add current block to ql after ql is flushed

}

int check_free(void *pp){
    if(pp==NULL) return -1;
    if((size_t)pp % 16!=0) return -1;
    printf("check 1\n");
    sf_block *block= pp-16; //needs to be checked 

    if((((block->header)^ MAGIC)& 0xffffffffffffff00)<32) return -1;
    if((((block->header)^ MAGIC)& 0xffffffffffffff00)%16!=0) return -1;
    if((((block->header)^ MAGIC) & THIS_BLOCK_ALLOCATED)==0) return -1;
    if((((block->header)^ MAGIC) & IN_QUICK_LIST)!=0) return -1;
    if(((((block->header)^ MAGIC) & PREV_BLOCK_ALLOCATED)==0) && (((block->prev_footer)^ MAGIC ) & THIS_BLOCK_ALLOCATED)!=0) return -1;

    if((char*)sf_mem_start()> (char*)(pp+sizeof(sf_header))) return -1;

    if((char*)pp> (char*)sf_mem_end()) return -1;

    return 0;

}

void *sf_realloc(void *pp, size_t rsize) {
    int val = check_free(pp);
    if(val==-1) abort();
    if(rsize==0) return NULL;
    return NULL;

}

double sf_fragmentation() {
    // To be implemented.
    abort();
}

double sf_utilization() {
    if(sf_mem_end()-sf_mem_start()==0) return 0.0;

    return (double)(total_size / (sf_mem_end()-sf_mem_start()));
}
void *add_to_ql(size_t size){
    int list_index=0;
    size_t tmp_size = MIN_SIZE;
    while(tmp_size< size && list_index < NUM_QUICK_LISTS-1){
        tmp_size+=16;
        list_index++;
    }
    //list_index found
    //if size exists and is not empty, use this ql
    if(sf_quick_lists[list_index].length==0) return NULL;
    sf_block *first= sf_quick_lists[list_index].first;
    first->header=(tmp_size + THIS_BLOCK_ALLOCATED-IN_QUICK_LIST)^MAGIC;
    sf_quick_lists[list_index].first=first->body.links.next;
    sf_quick_lists[list_index].length=sf_quick_lists[list_index].length-1;
    return first->body.payload;
}

int get_list(size_t size){
    int list_index=0;
    size_t tmp_size = MIN_SIZE;
    while(tmp_size< size && list_index < NUM_FREE_LISTS-1){
        tmp_size*=2;
        list_index++;
    }
    return list_index;
}

void *init(size_t size){
    printf("inside init\n");
    int temp=(int)size;
    void *start;
    do{
        if((start=sf_mem_grow())==NULL) return NULL;
        temp-=(int)PAGE_SZ;
    }
    while(temp> 0);
    free_index=8;
    bytes_rem= PAGE_SZ-48;

    for(int i=0;i<NUM_FREE_LISTS;i++){
        sf_free_list_heads[i].body.links.prev= &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.next= &sf_free_list_heads[i];
    }
    set_prologue(start, size);
    set_epilogue(size);
    return cur_block;
}


void set_prologue(void *start, size_t size){
    sf_block *prologue_area= sf_mem_start();    //should this be at an offset of 8??
    prologue_area->header= (sizeof(sf_block) | THIS_BLOCK_ALLOCATED |PREV_BLOCK_ALLOCATED)^ MAGIC;

    sf_block *new_block = sf_mem_start()+sizeof(sf_block);
    // new_block->header= ((PAGE_SZ-48)| PREV_BLOCK_ALLOCATED) ^MAGIC; //new block after prologue
    new_block->header= (PAGE_SZ-48+PREV_BLOCK_ALLOCATED)^MAGIC;
    sf_footer* footer = (sf_footer*) (sf_mem_end()-16);
    *footer=(PAGE_SZ-48+PREV_BLOCK_ALLOCATED)^MAGIC;
        // sf_block *add_block = sf_mem_start()+32;   //start of freed block
    sf_block *head= &sf_free_list_heads[8];
    new_block->body.links.prev= head;
    new_block->body.links.next= head;
    head->body.links.next=new_block;
    head->body.links.prev=new_block; 
    // sf_show_block(head);
}

void set_epilogue(size_t size){
    // sf_block epilogue;
    // epilogue.header= (0x0 | THIS_BLOCK_ALLOCATED)^ MAGIC;
    // *(sf_block*)((char*)sf_mem_end())=epilogue;
    sf_header *epi= (sf_header*)(sf_mem_end()-8);
    *epi= THIS_BLOCK_ALLOCATED^MAGIC;
}

void *add_to_main(size_t size){
    int list_index= get_list(size);
    total_size+=size;
    for(int i=list_index;i<NUM_FREE_LISTS;i++){
        sf_block *head = &sf_free_list_heads[i];
        sf_block *dummy = head->body.links.next;    //error
        while(dummy!=head){     //circular list, so if head==dummy, it means we're at beginning
            size_t block_size= ((dummy->header)^MAGIC)&0xfffffffffffffff0;  //make sure to use magic |||| retrieves block size
            if(block_size>= size){
                printf("inside if\n");
                if(block_size-size>=32){
                    //means this block has space
                    size_t new_block_size= block_size-size; //new block size for new sized freed list
                    split_block(head, new_block_size, size);
                }
                else{
                sf_block *new_block= (sf_block*) (char*)(dummy+block_size);
                new_block->header= (((new_block->header)^MAGIC)+ PREV_BLOCK_ALLOCATED+THIS_BLOCK_ALLOCATED)^MAGIC;    //rmm magic

                dummy->body.links.prev->body.links.next= dummy->body.links.next;
                dummy->body.links.next->body.links.prev= dummy->body.links.prev;

                dummy->header= (block_size+THIS_BLOCK_ALLOCATED+(((dummy->header)^MAGIC)&PREV_BLOCK_ALLOCATED))^MAGIC;
                }
                bytes_rem-= size;
                bytes_rem+=32;
                sf_show_heap();
                return dummy->body.payload;
            }
            dummy=dummy->body.links.next;

        }

    }
    void *new_pg;   //for whatever is returned by sf_mem_grow()
    sf_block *block;
    sf_block *dummy; //next element
    while(size>bytes_rem){
        new_pg=sf_mem_grow();   //pointer to new block start
        if(new_pg==NULL) {
            sf_errno= ENOMEM;
            return NULL;
        }
        bytes_rem+=PAGE_SZ;
        // printf("what new_pg returns: %p\n", new_pg);
        sf_block *add_block= new_pg;        //points to block start
        sf_block *head = &sf_free_list_heads[8];    //points to head of new page
        dummy= head->body.links.next;   //dummy is pointed to 2nd el of list
        // add_block->header= (PAGE_SZ) ^MAGIC;        //header of page size
        add_block->body.links.next= dummy;
        add_block->body.links.prev= head;
        head->body.links.next= add_block;
        dummy->body.links.prev= add_block;
        //add_block added as [1] of list

        block=coalesce(add_block, dummy);  //should maintain pointer to beginning of list

    }

    if(bytes_rem-size>=32){
        sf_block *head= &sf_free_list_heads[free_index];
        size_t new_block_size= bytes_rem-size; //new block size for new sized freed list
        split_block(head, new_block_size, size);
    }
    else{
    sf_block *new_block = (sf_block*)((char*)block);
    new_block->body.links.prev->body.links.next= new_block->body.links.next;    //gets block out of current list
    new_block->body.links.next->body.links.prev= new_block->body.links.prev;
    // new_block->body.links.next=NULL;
    // new_block->body.links.prev=NULL;
    
    new_block->header= (bytes_rem | THIS_BLOCK_ALLOCATED|PREV_BLOCK_ALLOCATED) ^ MAGIC;
    sf_show_block(new_block);

    sf_header *header= (sf_header*) &(new_block->header);
    // *header= (bytes_rem+PREV_BLOCK_ALLOCATED+THIS_BLOCK_ALLOCATED)^MAGIC;
    set_footer(new_block, bytes_rem, header);
    }
    //block is the merged memory block that is being returned

    // set_epilogue(bytes_rem);
    set_epilogue(bytes_rem);

    sf_show_heap();
    
    // sf_show_block(tmp->body.links.next);
     return block->body.payload;
}

size_t get_size(int i){
    if(i==0) return 32;
    if(i==1) return 64;
    if(i==2) return 128;
    if(i==3) return 256;  
    if(i==4) return 512;
    if(i==5) return 1024;
    if(i==6) return 2048;
    if(i==7) return 4096;
    if(i==8) return 8192;
    if(i==9) return -2;
    return 0;
}

void *coalesce(sf_block *block, sf_block *next_blk){
    if(((((next_blk->header)^MAGIC)&THIS_BLOCK_ALLOCATED)==0)&& (next_blk!=block)){
        block->header= (bytes_rem | PREV_BLOCK_ALLOCATED) ^MAGIC;
        next_blk->header= (0) ^MAGIC;
        sf_header* header= (sf_header*) &(next_blk->header);
        // *header= (0)^MAGIC;
        set_footer(next_blk, 0, header);
        sf_block *head= &sf_free_list_heads[free_index];
        head->body.links.prev= block;
        block->body.links.next=head;
        // next_blk->body.links.next->body.links.prev=NULL;
        // next_blk->body.links.prev->body.links.next=NULL;
        next_blk->body.links.next=NULL;
        next_blk->body.links.prev= NULL;
        next_blk=block;
    }
    return block;
}
//new)block_size is for free_block; size is for new_block
//dummy is current node from free_list
void split_block(sf_block *head, size_t new_block_size, size_t size){
    sf_block *dummy= head->body.links.next;
    sf_block *new_block= (sf_block *)((char*)dummy);       //new unused block after split    
    sf_block *free_block= (sf_block*) ((char*)dummy+ size);    
    // free_block->prev_footer= &
    sf_header* header= (sf_header*) &(free_block->header);
    *header= (new_block_size+PREV_BLOCK_ALLOCATED)^MAGIC;
    set_footer(free_block, new_block_size, header);
    dummy->body.links.prev->body.links.next= dummy->body.links.next;
    dummy->body.links.next->body.links.prev= dummy->body.links.prev;

    int new_idx=get_list(new_block_size);
    free_index=new_idx; //where the new free block is being kept
    sf_block *new_head= &sf_free_list_heads[new_idx];   //insert new block to desginated list
    free_block->body.links.next= new_head->body.links.next;
    free_block->body.links.prev= new_head;
    new_head->body.links.next->body.links.prev=free_block;  //make head node point to free_block
    new_head->body.links.next=free_block;
    dummy->header= (size+THIS_BLOCK_ALLOCATED+ (((dummy->header)^MAGIC)+PREV_BLOCK_ALLOCATED))^MAGIC;
    // sf_block *tmp = (sf_block *)((char*)dummy+size);

    new_block->header= (size+ THIS_BLOCK_ALLOCATED+PREV_BLOCK_ALLOCATED) ^ MAGIC;
}

void set_footer(sf_block *block, size_t size, sf_header *header){
    sf_footer* footer = (sf_footer*) ((char*)block+size-8);
    *footer=*header^ MAGIC;

}