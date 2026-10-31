// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

//#include "vm.h"
extern pte_t* walkpgdir(pde_t *, const void *, int);
extern void swap_out(struct page *p);

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

//this values already initialized to zero
//already in skeletion code
struct page pages[PHYSTOP/PGSIZE];
struct page *page_lru_head;
int num_free_pages;
int num_lru_pages;

//add my code
struct page* clock_hand;
//struct page *page_lru_tail;//this is nouse
char* swap_bitmap;
int swap_free_blocks;
struct spinlock lru_lock;



void
insert_lru(uint idx, pde_t *pgdir, char *va)
{
  //cprintf("PHYSTOP / PGSIZE=%d", PHYSTOP / PGSIZE);
  acquire(&lru_lock);
  if (idx < 0 || idx >= PHYSTOP / PGSIZE) {
    cprintf("wrong!!! invalid idx=%d (pa=0x%x), va=0x%x\n", idx, idx * 4096, va);
    panic("insert_lru: invalid page index");
  }
  //cprintf("insert: valid=%d (pa=0x%x), va=0x%x\n", idx, idx * 4096, va);
  struct page *p = &pages[idx];
  p->pgdir = pgdir;
  p->vaddr = va;
  num_lru_pages++;
  num_free_pages--;
  if (page_lru_head == 0) {
    page_lru_head = p; //initialize
    clock_hand = p;
    p->next = p;
    p->prev = p;
  } else {
    struct page *tail = page_lru_head->prev;
    p->next = page_lru_head;
    p->prev = tail;
    tail->next = p;
    page_lru_head->prev = p;
  }
  release(&lru_lock);
}

void remove_lru_called_locked(struct page *p)
{ 
  //cprintf("remove: (pa=0x%x)\n", p);
  if (p->next == 0 || p->prev == 0) {
    return;
  }
  num_lru_pages--;
  num_free_pages++;
  if (p->next == p) {
    page_lru_head = 0; // last one
  }
  else {
    p->prev->next = p->next;
    p->next->prev = p->prev;
    if (page_lru_head == p)
      page_lru_head = p->next;
  }
}

void remove_lru(struct page *p) {
  acquire(&lru_lock);
  remove_lru_called_locked(p);
  release(&lru_lock);
}

//find victim
//clock algorithm
struct page* reclaim(void) {
    acquire(&lru_lock);
    if (clock_hand == 0 || num_lru_pages == 0) {
        release(&lru_lock);
        return 0;
    }
    while (1) {
        struct page *p = clock_hand;
        pte_t *pte = walkpgdir(p->pgdir, p->vaddr, 0);
        if (!pte || !p->vaddr) {
            struct page *invalid = clock_hand;
            clock_hand = clock_hand->next;
            remove_lru_called_locked(invalid);
            continue;
        }
        if ((*pte & PTE_P) && (*pte & PTE_U)) {
            if (*pte & PTE_A) {
                *pte &= ~PTE_A; //access bit off
            } else {
                struct page *victim = clock_hand;
                clock_hand = clock_hand->next;
                release(&lru_lock);
                return victim; //victim page
            }
        }
        clock_hand = clock_hand->next;
    }
    release(&lru_lock);
    return 0;
}

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
  //initialize lock
  initlock(&lru_lock, "lru");
  swap_bitmap = kalloc(); //bitmap is oen page size
  memset(swap_bitmap, 0, PGSIZE); //first all zero
  //Bitmap is 4KB, 32768 bit
  swap_free_blocks = (4096 * 8); //this maybe changed
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.

char*
kalloc(void)
{
  struct run *r;
try_again:
  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(!r){
    if (kmem.use_lock){
      release(&kmem.lock); //must release!!!!
    }
    struct page *victim = reclaim();
    if(victim){
      //cprintf("trying swap out\n");
      swap_out(victim);
      goto try_again;
    }
    else {
      cprintf("kalloc is failed\n");
      return 0;
    }
  }
  if(r)
    kmem.freelist = r->next;
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}
