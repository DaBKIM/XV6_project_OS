#include "types.h" 
#include "defs.h" 
#include "param.h" 
#include "memlayout.h" 
#include "mmu.h" 
#include "x86.h" 
#include "proc.h" 
#include "spinlock.h"
#define NULL 0

struct stat;
struct rtcdate;
struct proc* queue[3][65]; //queue level 0 is lowest
int q[3]={-1,-1,-1}; // location of procese in queue

struct {   
   struct spinlock lock;   
   struct proc proc[NPROC]; 
} ptable;

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

//PAGEBREAK: 32 
// Look in the process table for an UNUSED proc. 
// If found, change state to EMBRYO and initialize 
// state required to run in the kernel. 
// Otherwise return 0. 
static struct proc* 
allocproc(void) 
{   
   // cprintf("process in\n");
    struct proc *p;   
    char *sp;   
    
    acquire(&ptable.lock);   
    

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)     
        if(p->state == UNUSED)       
            goto found;   

    release(&ptable.lock);   
    return 0; 

found:
    //cprintf("initialize in found\n");
    p->state = EMBRYO;   
    p->pid = nextpid++;   
    
    p->priority=2; //initialize queue
    p->ticks_ck=0;
    p->boosting=-1;
    q[2]++;
    queue[2][q[2]]=p;
    
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

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;

  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }

  proc->sz = sz;
  /*
  struct proc *p;
  
  acquire(&ptable.lock);

  for(p=ptable.proc;p<&ptable.proc[NPROC];p++){
    if(p->parent!=proc||1==p->thread_ck)
        p->sz=sz;
  }
  release(&ptable.lock);*/
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{

 // cprintf("in fork\n");  
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
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

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
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
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

void
priority_boosting(void){ //boosting all process in all level queues
                         //to highest level queue
    int i,j;

    for(i=0;i<2;i++){
        for(j=0;j<=q[i];j++){

            q[2]++;
            queue[i][j]->priority=2;
            queue[2][q[2]]=queue[i][j];
        }
        q[j]=-1;
    }
  //  cprintf("boost : %d, %d\n",ticks,proc->priority);
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  //cprintf("in scheduler\n");  
  struct proc *p;
  int i,j;

  //cprintf("s : for\n");  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    
   // cprintf("S : q[2]\n");
   if(q[2]!=-1){ // highest level queue process
                  // if queue is not empty, loop table


        for(i=0;i<=q[2];i++){

            if(queue[2][i]->state!=RUNNABLE)
                continue;
            
            // Switch to chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
           // cprintf("S : q[2].p->proc\n");
            p=queue[2][i];
            proc=p;

            switchuvm(p);
            p->state=RUNNING;
           // cprintf("S : q[2]->swtch\n");
            swtch(&cpu->scheduler,proc->context);
            switchkvm();
            
           // cprintf("S : q[2]->out switchkvm\n");
           
            if(proc->boosting == 1){ //priority boosing
                proc->boosting =-1;
                proc->ticks_ck=0;

                q[1]++;
                (proc->priority)--;
                queue[1][q[1]]=proc;

                for(j=i;j<=q[2]-1;j++){

                    queue[2][j]=queue[2][j+1];
                }
                q[2]--;
                priority_boosting();
                proc=0;
           
                break;

            }

            if(proc->ticks_ck==5){ //if time quantum is 5 give interrupt
                proc->ticks_ck=0;
                
                q[1]++;
                (proc->priority)--;
                queue[1][q[1]]=proc;

                for(j=i;j<=q[2]-1;j++){
                   queue[2][j]=queue[2][j+1];
                }
                q[2]--;
                

            }
         //   cprintf("queue : 2\n");
            // Process is done running for now.
            // It should have changed its p->state before coming back.
            proc=0;
        }
    } 

    if(q[1]!=-1){

                  // middle level queue process
                  // if queue is not empty, loop table
       // cprintf("queue : 1\n");

        for(i=0;i<=q[1];i++){

            if(queue[1][i]->state!=RUNNABLE)
                continue;
            
            // Switch to chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
           // cprintf("S : q[2].p->proc\n");
            p=queue[1][i];
            proc=p;

            switchuvm(p);
            p->state=RUNNING;
           // cprintf("S : q[2]->swtch\n");
            swtch(&cpu->scheduler,proc->context);
            switchkvm();
            
           // cprintf("S : q[2]->out switchkvm\n");
           
            if(proc->boosting == 1){ //priority boosing
                proc->boosting =-1;
                proc->ticks_ck=0;

                q[0]++;
                (proc->priority)--;
                queue[0][q[0]]=proc;

                for(j=i;j<=q[1]-1;j++){
                    queue[1][j]=queue[1][j+1];
                }
                q[1]--;
                priority_boosting();
                proc=0;
               
                break;

            }

            if(proc->ticks_ck==10){ //if time quantum is 5 give interrupt
                proc->ticks_ck=0;
                
                q[0]++;
                (proc->priority)--;
                queue[0][q[0]]=proc;

                for(j=i;j<=q[1]-1;j++){
                   queue[1][j]=queue[1][j+1];
                }
                q[1]--;
               

            }

            // Process is done running for now.
            // It should have changed its p->state before coming back.
            proc=0;
        }
    }

    
    
    if(q[0]!=-1){

                  // lowest level queue process
                  // if queue is not empty, loop table
       // cprintf("queue : 0\n");
      
        for(i=0;i<=q[0];i++){

            if(queue[0][i]->state!=RUNNABLE)
                continue;
            
            // Switch to chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
           // cprintf("S : q[2].p->proc\n");
            p=queue[0][i];
            proc=p;

            switchuvm(p);
            p->state=RUNNING;
           // cprintf("S : q[2]->swtch\n");
            swtch(&cpu->scheduler,proc->context);
            switchkvm();
            
           // cprintf("S : q[2]->out switchkvm\n");
           
            if(proc->boosting == 1){ //priority boosing
                proc->boosting =-1;
                proc->ticks_ck=0;

                q[0]++;
                queue[0][q[0]]=proc;

                for(j=i;j<=q[0]-1;j++){
                    queue[0][j]=queue[0][j+1];
                }
                q[0]--;
                priority_boosting();
                proc=0;
                
                break;

            }

            if(proc->ticks_ck==20){ //if time quantum is 5 give interrupt
                proc->ticks_ck=0;

                q[0]++;
                queue[0][q[0]]=proc;

                for(j=i;j<=q[0]-1;j++){
                    queue[0][j]=queue[0][j+1];
                }

                q[0]--;

            }
            // Process is done running for now.
            // It should have changed its p->state before coming back.
            proc=0;
        }
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

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
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
  if(proc == 0)
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
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

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
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
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

//create : provide a method to create threads within the process.
//return : pid of new thread
int 
create_t(void *(*func)(void*),void* arg){

    //cprintf("in create_t satrt\n");
    int i,pid;
    struct proc *process;


   // if(((uint)stack+PGSIZE>proc->sz) || ((uint)stack%PGSIZE!=0)) return -1;
    
    process=allocproc();
    if(process==0) return -1; // allocate thread
    //cprintf("alloc success\n");

    //copy parent process(proc)
    process->pgdir =proc->pgdir;//process state
    process->sz=proc->sz;
    process->parent=proc;
    *process->tf=*proc->tf;
    process->thread_ck=1;//this is thread
    process->tf->eax=0; //clear %eax->fork return 0
     //cprintf("copy parent\n");
                                
    
    growproc(2*PGSIZE);//allocate page
    clearpteu(process->pgdir,(char *)(process->sz));//guard page
    process->sz=(process->sz)+(2*PGSIZE);
    int tmp_sz=process->sz;

    uint stack[2]; //set the stack, top fo stack.
    stack[0]=0xffffffff;// a fake return PC
    stack[1]=(uint)arg; //put argument in the stack
    tmp_sz=tmp_sz-8;
    //cprintf("page size\n");

    if(copyout(process->pgdir,tmp_sz,stack,8)<0) return -1;

    //set stack pointer
    process->tf->eip=(uint)func;
    process->tf->esp=(uint)tmp_sz;
    //cprintf("stack pointer\n");

    for(i=0;i<NOFILE;i++){

        if(proc->ofile[i]) process->ofile[i]=filedup(proc->ofile[i]);
    }
    // cprintf("offile\n");

    safestrcpy(process->name,proc->name,sizeof(proc->name));
    pid=process->pid;
    process->cwd=idup(proc->cwd);//increment reference count for ip
    
    acquire(&ptable.lock);
    process->state=RUNNABLE;
    release(&ptable.lock);
    //like strncpy but guaranteed to NUL-terminate
     //cprintf("dup+runnable\n");

    // cprintf("in create_t : %d\n",pid);
    
    return pid;

}

//join : wait for child thread end
//return : pid of child which is waited or -1(error)
int
join(int threadID){

    //cprintf("in JOIN!\n");
    struct proc *process;
    int child_ck,tmp=0;

   // cprintf("lock\n");
    acquire(&ptable.lock);
    for(;;){
        
        //cprintf("start lock, in for\n");
        child_ck=0;
        for(process=ptable.proc;process<&ptable.proc[NPROC];process++){

            if((process->thread_ck==0)||(process->parent!=proc))//||(process->pgdir!=proc->pgdir)||(process->pid==proc->pid) || (process->parent->pid!=proc->pid)) 
            {
               //cprintf("i am child %d, %d\n",proc->parent->pid,proc->pid);
                continue;
            }
            //not a child or not a child thread->skip
            child_ck=1;
            
           // cprintf("%d %d %d\n",process->thread_ck,process->pid,threadID);
            if((process->state==ZOMBIE)&&(process->pid==threadID)){

               // child_ck=1;
               // cprintf("i am Zombie\n");
               // pid=process->pid;
                kfree(process->kstack);
                process->kstack=0;
                process->state=UNUSED;
                process->parent=0;
                process->pid=0;
                process->name[0]=0;
                process->killed=0;
                process->thread_ck=0;
                
                //cprintf("Zombie end\n");

                tmp=(int)(process->retval);
                process->retval=0;

                deallocuvm(process->pgdir,process->sz,(process->sz)-2*PGSIZE);
                process->pgdir=0;

                //cprintf("go back to MAIN\n");
               // cprintf("unlock\n");
                release(&ptable.lock);
                return tmp;
            }
        }

        //cprintf("i have no child\n");
        if((child_ck==0) || (process->killed==1)){ // no child
           // cprintf("no child unlock\n");
            release(&ptable.lock);
            return -1;
        }

        //cprintf("sleep for a while\n");
        sleep(proc,&ptable.lock); // wait for child end
    }

    return tmp;
}

int 
thread_create(thread_t *thread,void *(*start_routine)(void*),void *arg){
//(*start_routine) <-  (*func)
    
  //  cprintf("in\n");
    int ret_val=-1;

   // cprintf("alloc success\n");
    if((*thread=(int)create_t(start_routine,arg))>0){
        
        ret_val=0;
    }
   // cprintf("create success %d\n",thread);
 
    return ret_val;// if some error is occured ret_val will be non zero
}

int
thread_join(thread_t thread,void **retval){

  //  int cnt=1;
    int ret_val=0;
   // cprintf("retval : %d\n",(int)retval);

    if(retval!=NULL){
        if((*retval=(int*)join((uint)thread))<0) ret_val=-1;
    }
    else{
        if(join((uint)thread)<0) ret_val=-1;
    }
    
    //join in "proc.c"
   // if(nextpid==58) nextpid=1;
    return ret_val;// if some error is occured ret_val will be non zore

}

void 
thread_exit(void *retval){
  
  struct proc *p;
  int fd;

  if(proc == initproc)
      panic("init exiting");
  proc->retval=retval;
  
  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
      if(proc->ofile[fd]){
          fileclose(proc->ofile[fd]);
          proc->ofile[fd] = 0;
          }
  }

   begin_op();
   iput(proc->cwd);
   end_op();
   proc->cwd = 0;
   
   acquire(&ptable.lock);
   // Parent might be sleeping in wait();
   
   wakeup1(proc->parent);
   // Pass abandoned children to init.
   
   for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
       if(p->parent == proc){
           p->parent = initproc;
           if(p->state == ZOMBIE)
               wakeup1(initproc);
       }
   }

// Jump into the scheduler, never to return.
    proc->state = ZOMBIE;

    sched();
    panic("zombie exit");
}    
