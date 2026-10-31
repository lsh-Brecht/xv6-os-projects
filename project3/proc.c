#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h" //PGSIZE is 4kb(4096)
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h" //for file.h
#include "fs.h" //for file.h
#include "file.h"
//#include "vm.h" //this try is failed
extern pte_t* walkpgdir(pde_t*, const void*,int);
extern int mappages(pde_t*, void *, uint, uint, int);
//-----
#define maxmem 64
//Manage all mmap areas created by each mmap() call in one mmap_area array.
struct mmap_area mmap_areas[64]; //Maximum number of mmap_area array is 64
//in proc.h is extern. this is real!

extern int count_freepages;
//extern int counting_freepages(void);
//-----

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);
extern uint ticks; //trap.c
static void wakeup1(void *chan);

static const int weight_table[40] = {
  88761, 71755, 56483, 46273, 36291, 29154, 23254, 18705, 14949, 11916,
  9548, 7620, 6100, 4904, 3906, 3121, 2501, 1991, 1586, 1277,
  1024, 820, 655, 526, 423, 335, 272, 215, 172, 137,
  110, 87, 70, 56, 45, 36, 29, 23, 18, 15
};


void
print_mmap_pgtable(struct proc *p)
{
  uint start = 0x40000000;
  uint end = start + (PGSIZE * 10);  // 10 pages
  cprintf("Printing Page Table (0x40000000 ~ 10 pages) for pid %d ===\n", p->pid);
  for (uint va = start; va < end; va += PGSIZE) {
    pte_t *pte = walkpgdir(p->pgdir, (void*)va, 0);
    if (pte && (*pte & PTE_P)) {
      uint pa = PTE_ADDR(*pte);
      int flags = *pte & 0xFFF;
      cprintf("VA 0x%08x -> PA 0x%08x | flags: ", va, pa);
      if (flags & PTE_U) cprintf("U ");
      if (flags & PTE_W) cprintf("W ");
      if (flags & PTE_P) cprintf("P ");
      cprintf("(0x%x)\n", flags);
    } else {
      cprintf("VA 0x%08x -> not mapped\n", va);
    }
  }
  cprintf("End of Page Table\n");
}


//-----
uint mmap(uint addr, int lenth, int prot, int flags, int fd, int offset)
{
  struct proc *p = myproc();
  // addr is always page-aligned
  // length is also a multiple of page size
  // prot can be PROT_READ or PROT_READ|PROT_WRITE
  if(addr % PGSIZE != 0)
    return 0; //fail
  if(lenth % PGSIZE != 0)
    return 0; //fail
  if(lenth <= 0 || lenth > PGSIZE*64) // 
    return 0; //fail
  //If it is anonymous, fd must be -1 and offset must be 0
  if ((flags & MAP_ANONYMOUS) && (fd != -1 || offset != 0))
    return 0; //fail
  struct file *f = 0;
  if (!(flags & MAP_ANONYMOUS)) {
    f =p->ofile[fd];
    if (fd < 0 || fd >= NOFILE || f ==0)
      return 0; //fail
    if ((prot & PROT_WRITE) && !f->writable){
      cprintf("%write is %d\n", f->writable);
      cprintf("%read is %d\n", f->readable);
      return 0; //fail
    }
    if ((prot & PROT_READ) && !f->readable)
      return 0; //fail
    filedup(f); //using this file
  }
  // find unused mmap_area entry
  int head = -1; //like head of disk
  for(int i=0;i<64;i++){
    if(mmap_areas[i].addr ==0){
      head = i;
      break;
    }
  }
  if (head == -1 )
    return 0; // no free area, fail
  // MMAPBASE is 0x40000000 //1073741824
  uint mapped_addr =  0x40000000 + addr; //this is return value
  //MAP_ANONYMOUS x01
  //MAP_POPULATE x02
  //MAP_POPULATE is set, allocate physical memory immediately
  if (flags & MAP_POPULATE) {
    for (int i = 0; i < lenth; i += PGSIZE) {
      char *kpage = kalloc();
      if (!kpage){
        return 0; //fail
      }
      memset(kpage, 0, PGSIZE);
      // file mapping, reading data from file
      if (f && !(flags & MAP_ANONYMOUS))
      {
        int fileoff = offset +i;
	ilock(f->ip); //lock
	readi(f->ip, kpage, fileoff, PGSIZE);
	iunlock(f->ip);
      }
      // Set page table permissions
      int perm = PTE_U | PTE_P;
      if (prot & PROT_WRITE)
        perm |= PTE_W;
      // real mapping virtual address to physical page
      if (mappages(p->pgdir, (void *)(mapped_addr + i), PGSIZE, V2P(kpage), perm) < 0)
      {
        kfree(kpage);
        return 0; //fail
      }
    }
  }
  //mmap area also change
  mmap_areas[head].addr = mapped_addr;
  mmap_areas[head].length = lenth;
  mmap_areas[head].offset = offset;
  mmap_areas[head].prot = prot;
  mmap_areas[head].flags = flags;
  mmap_areas[head].f = (flags & MAP_ANONYMOUS) ? 0 : f;
  mmap_areas[head].p = p;
  // return MMAPBASE + addr;
  return mapped_addr; //success
}

//initializing mmap area
//called in main.c
void
mmapinit()
{
  for(int i = 0; i < maxmem; i++){
    mmap_areas[i].f = 0;
    mmap_areas[i].addr = 0;
    mmap_areas[i].length = 0;
    mmap_areas[i].offset = 0;
    mmap_areas[i].prot = 0;
    mmap_areas[i].flags = 0;
    mmap_areas[i].p = 0;
  }
}

// addr will be always given with the start address of mapping region, which is page aligned
//munmap() should remove corresponding mmap_area structure
// If there is no mmap_area of process starting with the address, return -1
//If physical page is allocated & page table is constructed, should free physical page & page table
// When freeing the physical page should fill with 1 and put it back to freelist
//If physical page is not allocated (page fault has not been occurred on that address), just remove mmap_area structure.
// Notice) In one mmap_area, situation of some of pages are allocated and some are not can happen.

int munmap(uint addr){
  struct proc *p = myproc();
  int j= -1; //this is target index
  if(addr % PGSIZE !=0)
    return -1;//fail
  for(int i=0;i<64;i++){
    //other processes can not access this area!
    if(mmap_areas[i].p ==p && mmap_areas[i].addr == addr)
    {
      j = i; //target mmap area associative with virtual memory
      break;
    }
  }
  if(j == -1)
    return -1; //no mapping, fail
  ///freeing real page for each mmap_area
  for(int i=0;i<mmap_areas[j].length;i += PGSIZE){
    uint mapped_addr = mmap_areas[j].addr + i;
    // get the PTE for the mapped address
    pte_t *pte = walkpgdir(p->pgdir, (void*)mapped_addr, 0);
    if (pte && (*pte & PTE_P)) {
      //the kernel virtual address
      char *pa = P2V(PTE_ADDR(*pte));
      memset(pa, 1,PGSIZE); //freeing physical page should fill with 1
      kfree(pa);
      *pte = 0;
    }
    //free page table
    pde_t *pde = &p->pgdir[PDX(mapped_addr)];
    if (*pde & PTE_P) {
      pte_t *pt = (pte_t*)P2V(PTE_ADDR(*pde));
      int all_empty = 1;
      for (int t = 0; t < NPTENTRIES; t++) {
        if (pt[t] & PTE_P) {
          all_empty = 0;
          break;
        }
      }
      if (all_empty) {
        kfree((char*)pt);
        *pde = 0;
      }
    }
  }

  if(mmap_areas[j].f && !(mmap_areas[j].flags & MAP_ANONYMOUS)){
    fileclose(mmap_areas[j].f);
  }
  //mmap area also change
  mmap_areas[j].addr = 0;
  mmap_areas[j].length = 0;
  mmap_areas[j].offset = 0;
  mmap_areas[j].prot = 0;
  mmap_areas[j].flags = 0;
  mmap_areas[j].f = 0;
  mmap_areas[j].p = 0;
  return 1; //success
}

int freemem(){
  return count_freepages;
}


int page_fault_handler(uint vaddr, int wbool){
  struct proc *p = myproc(); 
  //Check if already mapped // no need handling
  pte_t *pte = walkpgdir(p->pgdir, (void*)vaddr, 0);
  if (pte && (*pte & PTE_P))
    return 0; //success
  // finding mapping address in mmap_areas
  struct mmap_area *found = 0;
  for (int i = 0; i < 64; i++) {
    struct mmap_area *area = &mmap_areas[i];
    if (area->p != p || area->length == 0)
      continue;
    if (vaddr >= area->addr && vaddr < area->addr + area->length) {
      found = area;
      break;
    }
  }
  if (!found)
    return -1; // no mapping, fail
  if (wbool && !(found->prot & PROT_WRITE))
    return -1; // write impossible(fail)
  char *kpage = kalloc();
  if (!kpage)
    return -1; //fail
  memset(kpage, 0, PGSIZE);
  // file mapping
  if (!(found->flags & MAP_ANONYMOUS) && found->f) {
    uint pagedsize = vaddr - (vaddr % PGSIZE);
    int fileoff = found->offset + (pagedsize - found->addr);
    ilock(found->f->ip); //lock
    readi(found->f->ip, kpage, fileoff, PGSIZE);
    iunlock(found->f->ip);
  }
  uint ava = vaddr -(vaddr % PGSIZE);
  // Set page table permissions
  int perm = PTE_U | PTE_P;
  if (found->prot & PROT_WRITE)
    perm |= PTE_W;
  //mapping new page
  if (mappages(p->pgdir, (void*)ava, PGSIZE, V2P(kpage), perm) < 0) {
    kfree(kpage);
    return -1; //fail
  }
  //print_mmap_pgtable(myproc());
  return 0; //success this process survive
}
//-----



int
getnice(int pid)
{
  if(pid <= 0)
    return -1; //invalid pid
  struct proc *p;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->pid == pid){
      int temp = p->nice;
      release(&ptable.lock); //release before return
      return temp;
    }
  }
  release(&ptable.lock);
  return -1; //found fail
}
  

int
setnice(int pid, int value)
{
  if(pid <= 0)
    return -1;//invalid pid
  if(value < 0 || value >39)
    return -1; //invalid nice value
  struct proc *p;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->pid == pid){
      p->nice = value; //pid set
      release(&ptable.lock);
      return 0; //success
    }
  }
  release(&ptable.lock);
  return -1; //found fail
}

void ps(int pid)
{
  if(pid < 0)
  {
    return;//fail
  }
  static char *states[] = {
    [UNUSED] "UNUSED  ", [EMBRYO] "EMBRYO  ",
    [SLEEPING] "SLEEPING", [RUNNABLE] "RUNNABLE",
    [RUNNING] "RUNNING ", [ZOMBIE] "ZOMBIE  "
  };
  struct proc *p;
  acquire(&ptable.lock); //lock
  if(pid == 0)
  {
    cprintf("name         pid    state      priority"
    "    runtime/weight        runtime      vruntime      "
    "tick %d\n", (ticks *1000));
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if(p->state==UNUSED)
        continue;
      if(p->pid) // pid is 0, mean no process
      {
        int nlen = strlen(p->name);
        cprintf("%s", p->name);
        for(int i = 0; i < 13 - nlen; i++)
          cprintf(" ");
        cprintf("%d      %s      %d            "
	"%d            %d            %d\n", p->pid,
	states[p->state], p->nice,((p->runtime*1000)/weight_table[p->nice]),
	(p->runtime*1000), (p->vruntime));
      }
    }
    release(&ptable.lock); // realease before return
    return; //success
  }
  else
  {
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if(p->state==UNUSED)
        continue;
      if(p->pid == pid)
      {
        cprintf("name         pid    state      priority    "
	"runtime/weight        runtime      vruntime      "
	"tick %d\n", (ticks*1000));
	int nlen = strlen(p->name);
        cprintf("%s", p->name);
        for(int i = 0; i < 13 - nlen; i++)
          cprintf(" ");
        cprintf("%d      %s      %d            "
	"%d            %d            %d\n", p->pid,
	states[p->state], p->nice,((p->runtime*1000)/weight_table[p->nice]),
	(p->runtime*1000), (p->vruntime));
        release(&ptable.lock); //realease
        return; //success
      }
    }
  }
  release(&ptable.lock); //release
  return; //found fail print out nothing
}


void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}







// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->nice = 20; //0~39, default is 20
  p->vruntime = 0;
  p->runtime=0;
  
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
  //processed in alloc
  //p->vruntime=0;
  //p->runtime=0;
  //p->nice=20;
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);
  //-----
  for(int i=0;i<64;i++){
    if(mmap_areas[i].p != curproc)
      continue;
    int j = 0; //no OOM contorl
    for(int l=0;l< 64;l++){
      if(!(mmap_areas[l].addr)) {
        j=l; //found available space for child
	break;
      }
    }
    //copying mmap_area, i is parent & j is child
    if (mmap_areas[i].f)
      mmap_areas[j].f = filedup(mmap_areas[i].f);
    else
      mmap_areas[j].f = 0;
    mmap_areas[j].addr = mmap_areas[i].addr;
    mmap_areas[j].length = mmap_areas[i].length;
    mmap_areas[j].offset = mmap_areas[i].offset;
    mmap_areas[j].prot = mmap_areas[i].prot;
    mmap_areas[j].flags = mmap_areas[i].flags;
    mmap_areas[j].p = np; //np is the pid of child 
    //no COW
    //we should copy parent’s page table and map physical page same as parent
    //pde_t(page dictionary entry), pte_t(page table entry)
    for(int k=0;k<mmap_areas[i].length; k += PGSIZE) {
      int temp_addr = mmap_areas[i].addr + k;
      //get PTE(temp_addr) of parent in page directory
      pte_t *pte = walkpgdir(curproc->pgdir, (void*)temp_addr, 0);
      if(!pte || !(*pte & PTE_P))
        continue;
      pte_t *pte2 = walkpgdir(np->pgdir, (void*)temp_addr, 0);
      if (pte2 && (*pte2 & PTE_P))
        continue;
      char *kpage = kalloc();
      if(!kpage)
        return -1; //fail allocating physical memory
      memmove(kpage, (char*)P2V(PTE_ADDR(*pte)), PGSIZE);
      int permissi = PTE_U | PTE_P;
      if(mmap_areas[i].prot & PROT_WRITE)
        permissi |= PTE_W;
      if (mappages(np->pgdir, (void *)temp_addr, PGSIZE, V2P(kpage), permissi) < 0) {
        kfree(kpage);
        mmap_areas[j].addr = 0;
        return -1;
      }
    }
  }
  //-----
  np->state = RUNNABLE;
  //copy from parent to child
  np->nice=curproc->nice;
  np->runtime=curproc->runtime;
  np->vruntime=curproc->vruntime;
  
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    struct proc *minimum = 0;
    int total_weights = 0; //for calculating time slice in CFS
    // (weight / total_weights)
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue; //only in ready queue
      total_weights += weight_table[p->nice];
      if(minimum == 0){
        minimum = p; //initialize
      }
      if (p->vruntime < minimum->vruntime){
        minimum = p;
      }
    }
    if(minimum != 0){
      minimum->slice = (1000*10*weight_table[minimum->nice]) / total_weights;
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      if (minimum->slice == 0)
        minimum->slice = 1000; //min granularity
      c->proc = minimum;
      switchuvm(minimum);
      minimum->state = RUNNING;

      swtch(&(c->scheduler), minimum->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;
  int RBTmin = 2000000000;
  //already in acquire()
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == RUNNABLE)
      if(RBTmin == 2000000000 || (p->vruntime) < RBTmin)
        RBTmin = p->vruntime; //smallest vruntime in running processes
  }
  //no running process
  if(RBTmin == 2000000000){
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if(p->state == SLEEPING && p->chan == chan){
        p->state = RUNNABLE;
        p->vruntime = 0;
      }
    }
  }
  else{
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if(p->state == SLEEPING && p->chan == chan){
        p->state = RUNNABLE;
        p->vruntime = RBTmin - ((1000*1024) / weight_table[p->nice]);
        //(minimum vruntime of processes in the ready queue – vruntime(1tick))
      }
    }
  }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
