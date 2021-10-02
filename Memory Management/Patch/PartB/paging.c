#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "stat.h"
#include "fcntl.h"
#include "proc.h"
#include "ptable.h"
#include "paging.h"

extern struct ptable_t ptable;
extern void wakeup1(void *chan);

struct swapqueue soq, siq;  // Queue for swapout and swapin resp.
int flimit = 2; 
int swapoutcount, swapincount;

// Inbuilt function to allocate fd
int
fdalloc(struct file *f)   
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

// Inbuilt function to create a file with given name
struct inode*
create(char *path, short type, short major, short minor)  
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

// Inbuilt function to open a file
int open_file(char *path, int omode) {  
  int fd;
  struct file *f;
  struct inode *ip;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  strncpy(f->name, path, 14);
  return fd;
}


// Create name string from
// PID and VA[32:13].
// Will return PID_VA[32:13] as the name
void
get_name(int pid, uint addr, char *name) { 
  int i = 0;
  while (pid) {
    name[i++] = '0' + (pid%10);
    pid = pid / 10;
  } 
  name[i++] = '_';
  if(addr==0){
    name[i++]='0';
  }
  while (addr) {
    name[i++] = '0' + (addr%10);
    addr = addr / 10;
  }
  name[i] = 0;
  int mi = -1;
  for(int j=0;j<i;j++){
    if(name[j]=='_') {
      mi=j;break;
    }
  }
  char temp;
  for(int j=0;j<mi/2;j++){
    temp = name[j];
    name[j] = name[mi-j-1];
    name[mi-j-1] = temp;
  }
  for(int j=mi+1, k=i-1;j<k;j++,k--){
    temp = name[j];
    name[j] = name[k];
    name[k] = temp;
  }
}

// Writes a page into the swapout file
int write_page(int pid, uint addr, char *buf){
  flimit++;
  char name[14];

  get_name(pid, addr, name);
  
  int fd = open_file(name, O_CREATE|O_WRONLY);  // Open + create file 
  struct file *f;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  
  cprintf("Creating page file: %s\n", name);
  int noc = filewrite(f, buf, 4096);          // Write the page in the file
  if(noc < 0){
    cprintf("Unable to write. Exiting (paging.c::write_page)!!");
  }
  swapoutcount++;
  return noc;
}

// Deletes swapout file with the given filename
int
delete_page(char* path)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ];
  uint off;

  begin_op();
  dp = nameiparent(path, name);

  ilock(dp);

  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

// Reads the swapout file into the buffer 
int read_page(int pid, uint addr, char *buf){
  char name[14];

  get_name(pid, addr, name);
  int fd = open_file(name, O_RDONLY);   // Open swapout page file
  struct file *f;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  int noc = fileread(f, buf, 4096);     // Read the page into the buffer
  if(noc < 0){
    cprintf("Unable to write. Exiting (paging.c::read_page)!!");
  }
  swapincount++;
  delete_page(name);
  myproc()->ofile[fd] = 0;
  fileclose(f);

  return noc;
}

// Enqueue function for the queues
void enqueue(struct swapqueue* sq, struct proc* np){
  if(sq->size == NPROC) return; 
  sq->rear = (sq->rear + 1) % NPROC;
  sq->queue[sq->rear] = np; 
  sq->size++;  
}

// Dequeue function for the queues
struct proc* dequeue(struct swapqueue* sq){
  if (sq->size == 0) return 0; 
  struct proc* next = sq->queue[sq->front]; 
  sq->front = (sq->front + 1) % NPROC; 
  sq->size = sq->size - 1; 

  if(sq->size == 0){
    sq->front = 0;
    sq->rear = NPROC - 1;
  }

  return next; 
}

// Chooses a victim frame using LRU and evicts it 
int chooseVictimAndEvict(int pid){
  
  struct proc* p;
  struct victim victims[4]={{0,0,0},{0,0,0},{0,0,0},{0,0,0}};
  pde_t *pte;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state == UNUSED|| p->state == EMBRYO || p->state == RUNNING || p->pid < 5|| p->pid == pid)
        continue;
      
      for(uint i = PGSIZE; i< p->sz; i += PGSIZE){
        pte = (pte_t*)getpte(p->pgdir, (void *) i);
        if(!((*pte) & PTE_U)||!((*pte) & PTE_P))
          continue;
        int idx =(((*pte)&(uint)96)>>5);
        if(idx>0&&idx<3)
          idx=3-idx;
        victims[idx].pte = pte;
        victims[idx].va = i;
        victims[idx].pr = p;
      }
  }
  for(int i=0;i<4;i++)
  {
    if(victims[i].pte != 0)
    {
      pte = victims[i].pte;
      int origstate = victims[i].pr->state;
      char* origchan = victims[i].pr->chan;
      victims[i].pr->state = SLEEPING;
      victims[i].pr->chan = 0;
      uint reqpte = *pte;
      *pte = ((*pte)&(~PTE_P));
      *pte = *pte | ((uint)1<<7);
      
      if(victims[i].pr->state != ZOMBIE){
        release(&soq.lock);
        release(&ptable.lock);
        write_page(victims[i].pr->pid, (victims[i].va)>>12, (void *)P2V(PTE_ADDR(reqpte)));   
        acquire(&soq.lock);
        acquire(&ptable.lock);
      }
      kfree((char *)P2V(PTE_ADDR(reqpte)));
      lcr3(V2P(victims[i].pr->pgdir)); 
      victims[i].pr->state = origstate;
      victims[i].pr->chan = origchan;
      return 1;
    }
  }
  return 0;
}

// Entry point of the swapout process
void swapoutprocess(){
  sleep(soq.qchan, &ptable.lock);

  while(1){
    cprintf("\n\nEntering swapout\n");
    acquire(&soq.lock);
    while(soq.size){
      while (flimit >= NOFILE)    // Edge case handling
      {
        cprintf("flimit \n");
        wakeup1(soq.reqchan);
        release(&soq.lock);
        release(&ptable.lock);
        yield();
        acquire(&soq.lock);
        acquire(&ptable.lock);
      }
      
      struct proc *p = dequeue(&soq); // Dequeue process from queue
      
      if(!chooseVictimAndEvict(p->pid)) // Edge case handling
      {
        wakeup1(soq.reqchan);
        release(&soq.lock);
        release(&ptable.lock);
        yield();
        acquire(&soq.lock);
        acquire(&ptable.lock);
      }
      p->satisfied = 1;     // When frame found set satified to true
    }

    wakeup1(soq.reqchan);   // The the corresponding process
    release(&soq.lock);
    sleep(soq.qchan, &ptable.lock);
  }

}

// Entry point of the swapin process
void swapinprocess(){
  sleep(siq.qchan, &ptable.lock);
  while(1){
    cprintf("\n\nEntering swapin\n");
    acquire(&siq.lock);
    while(siq.size){
      struct proc *p = dequeue(&siq);
      flimit--;
      release(&siq.lock);
      release(&ptable.lock);
      
      char* mem = kalloc();
      read_page(p->pid,((p->trapva)>>12),mem);
      
      acquire(&siq.lock);
      acquire(&ptable.lock);
      swapInMap(p->pgdir, (void *)PGROUNDDOWN(p->trapva), PGSIZE, V2P(mem));
      wakeup1(p->chan);
    }
    // cprintf("\n\n");
    release(&siq.lock);
    sleep(siq.qchan, &ptable.lock);
  }

}


// Submits a request for a free page to the swapout process
void submitToSwapOut(){
  struct proc* p = myproc();
  cprintf("submitToSwapOut %d\n",p->pid);

  acquire(&ptable.lock);
  acquire(&soq.lock);
  p->satisfied = 0;   
  enqueue(&soq, p);   // Enqueues the process in the Swapout queue
  wakeup1(soq.qchan); // Wakes up the Swapout process
  release(&soq.lock);

  while(p->satisfied==0)  // Sleep process till not satisfied 
    sleep(soq.reqchan, &ptable.lock);
  release(&ptable.lock);
  return;

}

// Submits a request to the swapin process
void submitToSwapIn(){
  struct proc* p = myproc();
  cprintf("submitToSwapIn %d\n",p->pid);

  acquire(&ptable.lock);
  acquire(&siq.lock); 
    enqueue(&siq, p);   // Enqueues the process in the Swapin queue
    wakeup1(siq.qchan); // Wake up the Swapin process
  release(&siq.lock);
  
  sleep((char *)p->pid, &ptable.lock);  // Suspend the process
  release(&ptable.lock);
  return;
}

// On exit delete the swapout page-files created 
void deletePageFiles()
{
  acquire(&ptable.lock);
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)  
  {
    if(p->state == UNUSED) continue;
    if(p->pid==2||p->pid==3)
    {
      for(int fd = 0; fd < NOFILE; fd++){
        if(p->ofile[fd]){
          struct file* f;
          f = p->ofile[fd];

          if(f->ref < 1) {
            p->ofile[fd] = 0;
            continue;
          }
          release(&ptable.lock);
          if(f->ref == 1) cprintf("Deleting page file: %s\n", f->name);
          delete_page(p->ofile[fd]->name);
          fileclose(f);
          flimit--;
          p->ofile[fd] = 0;

          acquire(&ptable.lock);
        }
      }
    }
  }
  cprintf("\nTotal no. of Swap in: %d\nTotal no. of Swap out: %d\n\n", swapincount, swapoutcount);
  swapincount = swapoutcount = 0;
  release(&ptable.lock);
}