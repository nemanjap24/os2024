// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define PG_INDEX(pa) (((uint64)(pa) - (uint64)kmem.refcount) / PGSIZE)
void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist; 
  uint *refcount;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  kmem.refcount = (uint *)p;
  // celkovy pocet stranok spracovavabycg alokatorom
  uint64 pages = ((uint64) pa_end - (uint64) kmem.refcount) / PGSIZE;
  // pocet stranok potrebnych pre refcount
  uint64 refcount_pages = PGROUNDUP(pages * sizeof(uint)) / PGSIZE;
  // uint64 refcount_pages = PGROUNDUP(pages * sizeof(*kmem.refcount)) / PGSIZE;
  // TODO: inicializovat pole referencii
  for(uint64 i = 0; i < refcount_pages; i++){
    kmem.refcount[i] = 2;
  }
  for(uint64 i = refcount_pages; i < pages; i++){
    kmem.refcount[i] = 1;
  }

  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);
  kmem.refcount[PG_INDEX(pa)]--;
  if(kmem.refcount[PG_INDEX(pa)]){
    release(&kmem.lock);
    return;
  }
  release(&kmem.lock);


  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    // nastav pocet referencii pre r na 1
    kmem.refcount[PG_INDEX(r)] = 1;
    kmem.freelist = r->next;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void
get_page(uint64 pa)
{
  acquire(&kmem.lock);
  kmem.refcount[PG_INDEX(pa)]++;
  release(&kmem.lock);
}