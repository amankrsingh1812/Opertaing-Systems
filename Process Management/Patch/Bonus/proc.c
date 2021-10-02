#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "processInfo.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct {
  struct proc* array[NPROC];             
  int front;
  int rear; 
  int size;  
} rqueue; // Running Queue

struct proc* base_process = 0;
int base_process_pid = 0;

void enqueue(struct proc* np){
  if(rqueue.size == NPROC) return; 
  rqueue.rear = (rqueue.rear + 1) % NPROC;
  rqueue.array[rqueue.rear] = np; 
  rqueue.size = rqueue.size + 1;  
}

struct proc* dequeue(){
  if (rqueue.size == 0) return 0; 
  struct proc* next = rqueue.array[rqueue.front]; 
  rqueue.front = (rqueue.front + 1) % NPROC; 
  rqueue.size = rqueue.size - 1; 

  if(rqueue.size == 0){
    rqueue.front = 0;
    rqueue.rear = NPROC - 1;
  }

  return next; 
}

// This is used to insert a new process with default burst time (0)
void insert_rqueue(struct proc* np){
  // int proc_inserted = 0;
  const int size = rqueue.size;


  // [0, ]

  if(size == 0 || rqueue.array[rqueue.front]->burstTime == 0){
    enqueue(np);
    return;
  }

  struct proc* min_proc = 0;
  for(int i = 0; i < size; ++i){
    struct proc* cur = dequeue();
    if(min_proc == 0 || min_proc->burstTime >= cur->burstTime){
      min_proc = cur;
    }
    enqueue(cur);
  }

  for(int i = 0; i < size; ++i){
    struct proc* cur = dequeue();
    if(cur == min_proc){
      enqueue(np);
    }
    enqueue(cur);
  }
}

void insert_rqueue_sorted(struct proc* cur){
  const int size = rqueue.size;

  // at this instant rqueue looks like this
  // [0] + [0, 0, 0, non-zero, non-zero, ....., non-zero, 0, 0, 0]
  struct proc* first_proc = rqueue.array[rqueue.front];

  // Step 1: place all 0 burstTime process at end
  for(int i = 0; i < size; ++i){
    if(rqueue.array[rqueue.front]->burstTime > 0) break;
    enqueue(dequeue());
  }

  if(rqueue.array[rqueue.front]->burstTime == 0){
    // all processes have zero burst time
    enqueue(cur);
    return;
  }

  // Step 2: Rotate array and place our process at correct spot
  int proc_inserted = 0;
  for(int i = 0; i < size; ++i){
    struct proc* p = dequeue();
    if(p->burstTime == 0 || p->burstTime > cur->burstTime){
      enqueue(cur);
      enqueue(p);
      proc_inserted = 1;
      break;
    }
    enqueue(p);
  }

  if(proc_inserted == 0){
    // all process have non-zero burst time and current process has max burst time;
    enqueue(cur);
    return;
  }

  // Step 3: Return to original state with first_proc in front;
  while(rqueue.array[rqueue.front] != first_proc) enqueue(dequeue());
}


// helper function called when a process is added to ready queue(priority queue)
void makeProcRunnable(struct proc* proc){
  proc->state = RUNNABLE;
  enqueue(proc);
}


static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

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

  // Initialise number of context switches with 0
  p->numcs = 0;
  p->burstTime = 0;

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

  rqueue.front = rqueue.size = 0; 
  rqueue.rear = NPROC - 1;

  makeProcRunnable(p);

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

  np->state = RUNNABLE;
  insert_rqueue(np);


  if(np->parent->pid == 2){
    base_process = np;
    base_process_pid = np->pid;
  }

  // if(rqueue.size == 1 && np->pid >= 3){
  //   // this is the only process in queue
  //   if(base_process_pid == 0){
  //       base_process = np;
  //       base_process_pid = np->pid;
  //   }
  // }



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

  // cprintf("Exiting PID: %d\n", curproc->pid);

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
  cprintf("Waking up parent\n");
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
//  - choose a process to run using Priority Queue 
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *reqp=0;
  // struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    reqp = 0;    

    // Choose a process from ready queue to run
    acquire(&ptable.lock);
    reqp = dequeue(); 

    if(reqp == 0) {
      // if(base_process != 0){
      //   // cprintf("No running processes");
      //   // debug_queue();
      //   base_process = 0;
      //   base_process_pid = 0;
      // }
      release(&ptable.lock); // No process is curently runnable
      continue;
    }

    // debug_queue();

    // if(rqueue.size == 0 && reqp->pid >= 3){
    //   // this is the only process in queue
    //   if(base_process_pid == 0){
    //     base_process = reqp;
    //     base_process_pid = reqp->pid;
    //   }
    // }

    // int pid = base_process == 0 ? 0 : base_process->pid;
    if(reqp->pid>=3)    //donot print for shell and userinit
      cprintf("SCHEDULING - pid: %d  burstTime: %d baseprocess: %d\n", reqp->pid, reqp->burstTime, base_process_pid);
    // debug_queue();

    c->proc = reqp;
    switchuvm(reqp);
    reqp->state = RUNNING;
    reqp->numcs++; // Number of Context Switch Increment
    swtch(&(c->scheduler), reqp->context);
    switchkvm();
    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;

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
  makeProcRunnable(myproc());
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

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
      insert_rqueue_sorted(p);
      // makeProcRunnable(p);
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
      if(p->state == SLEEPING){
        makeProcRunnable(p);
      }
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

int 
getProcNum(void)
{
	int used = 0;
	struct proc *p;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state != UNUSED)
      used++;

  release(&ptable.lock);
	return used;
}

int 
getMaxPid(void)
{
	int maxPid = 0;
	struct proc *p;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state != UNUSED)
      maxPid = (maxPid < p->pid) ? p->pid : maxPid;

  release(&ptable.lock);
	return maxPid;
}

int 
getProcInfo()
{
  int ret = -1;
  struct proc *p = myproc();
  
  acquire(&ptable.lock);

  cprintf(" PID: %d   NCS: %d    BurstTime: %d \n", p->pid, p->numcs, p->burstTime);
  

  release(&ptable.lock);
  return ret;
}

int
get_burst_time()
{
  struct proc *p = myproc();
  return p->burstTime;
}


int
set_burst_time(int n)
{
  struct proc* cur = myproc();

  // can't use this system call more than once for one process
  if(cur->burstTime != 0) return -1;

  cprintf("Setting burst time\n");

  cur->burstTime = n;
  acquire(&ptable.lock);
  
  // Reposition this process in rqueue
  insert_rqueue_sorted(cur);

  // Check if burst time of all processes have been set 
  const int size = rqueue.size;
  int should_rotate = 1;
  struct proc* minBurstproc = 0;
  for(int i = 0; i < size; ++i){
    struct proc* p = dequeue();
    if(p->burstTime == 0){
      should_rotate = 0;
    }
    if(minBurstproc == 0 || minBurstproc->burstTime > p->burstTime){
      minBurstproc = p;
    }
    enqueue(p);
  }

  // Choose base process if burst time of all processes have been set 
  if(should_rotate){
    while(rqueue.array[rqueue.front] != minBurstproc) enqueue(dequeue());
    base_process = minBurstproc;
    base_process_pid = minBurstproc->pid;
  }

  cur->state = RUNNABLE;
  sched();
  release(&ptable.lock);
  return 0;
}


