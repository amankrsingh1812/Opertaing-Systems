struct ptable_t {
  struct spinlock lock;
  struct proc proc[NPROC];
};