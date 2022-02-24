// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
void reset_page_ref();

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

// record each page reference count
struct {
  struct spinlock lock;
  int ref[(PHYSTOP - KERNBASE) / PGSIZE];
} page_ref;

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  reset_page_ref();
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

void
reset_page_ref()
{
  int cnt = sizeof(page_ref.ref) / sizeof(int);
  printf("cnt = %d\n", cnt);
  for (int i = 0; i < cnt; i++)
  {
    page_ref.ref[i] = 1;
  }
}

int
get_pa_index(uint64 pa)
{
  return ((pa & ~(PGSIZE - 1)) - KERNBASE) / PGSIZE;
}

void
inc_page_ref(uint64 pa)
{
  acquire(&page_ref.lock);
  int idx = get_pa_index(pa);
  page_ref.ref[idx] += 1;
  release(&page_ref.lock);
}

void 
dec_page_ref(uint64 pa)
{
  acquire(&page_ref.lock);
  int idx = get_pa_index(pa);
  page_ref.ref[idx] -= 1;
  release(&page_ref.lock);
}

int 
get_ref_cnt(uint64 pa)
{
  int idx = get_pa_index(pa);
  return page_ref.ref[idx];
}

// Free the page of physical memory pointed at by v,
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
  int ref_cnt = get_ref_cnt((uint64)pa);
  if (ref_cnt == 0){
    release(&kmem.lock);
    panic("ref cnt == 0"); // release page double times
  }

  if(ref_cnt == 1){
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    r->next = kmem.freelist;
    kmem.freelist = r;
  }
  
  dec_page_ref((uint64)pa);
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
    kmem.freelist = r->next;
    inc_page_ref((uint64)r);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
