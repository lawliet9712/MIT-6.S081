#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
pte_t *
_walk(pagetable_t pagetable, uint64 va)
{
  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      return 0;
    }
  }
  return &pagetable[PX(0, va)];
}

int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  int page_cnt;
  uint64 start_addr;
  uint64 out_result;

  if (argaddr(0, &start_addr) < 0)
    return -1;
  if (argint(1, &page_cnt) < 0)
    return -1;
  if (argaddr(2, &out_result) < 0)
    return -1;

  //printf("start addr %p \n ", start_addr);
  uint32 scan_result = 0;
  pagetable_t pagetable = myproc()->pagetable;
  for (int i = 0; i < page_cnt; i++)
  {
    pte_t* pte = _walk(pagetable, start_addr);
    if (pte == 0)
      continue;
    
    //printf("found pte %p no.%d\n", *pte, i);
    if ((*pte & PTE_A))
    {
      scan_result |= (1 << i);
      *pte &= (~PTE_A); // need to clear bit , becasue if set PTE_A , will forever exists
      //printf("scan %d is valid \n", i);
    }
      
    start_addr += PGSIZE;
  }

  if(copyout(pagetable, out_result, (char *)&scan_result, sizeof(uint32)) < 0)
      return -1;

  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
