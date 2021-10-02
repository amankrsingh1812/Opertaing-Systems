struct swapqueue{
  struct spinlock lock;
  char* qchan;
  char* reqchan;
  int front;
  int rear; 
  int size;  
  struct proc* queue[NPROC+1];
};

extern struct swapqueue soq, siq;

struct victim
{
  pte_t* pte;
  struct proc* pr;
  uint va; 
};

extern int flimit;

int fdalloc(struct file *f);

struct inode* create(char *path, short type, short major, short minor);
int open_file(char *path, int omode);

// Create name string from
// PID and VA[32:13]
void get_name(int pid, uint addr, char *name);

int write_page(int pid, uint addr, char *buf);
int delete_page(char* path);
int read_page(int pid, uint addr, char *buf);

void enqueue(struct swapqueue* sq, struct proc* np);
struct proc* dequeue(struct swapqueue* sq);

int chooseVictimAndEvict(int pid);

void swapoutprocess();
void swapinprocess();

void submitToSwapOut();
void submitToSwapIn();

void deletePageFiles();
