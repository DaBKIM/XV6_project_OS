#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_getppid(void){

    return proc->parent->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_yield(void){
    
    yield();
    return 0;
}

int
sys_getlev(void){

    return proc->priority;
}

int
sys_create_t(void){

    void *arg;
    void *(*func)(void*);

    if((argptr(0,(void*)&func,sizeof(func))<0) || (argptr(1,(void*)&arg,sizeof(arg))<0)) return -1;
    //argptr : fetch the nth word-sized system call argument as a pointer

    return create_t(func,arg);

}

int 
sys_join(void){

    int threadID;

    if((argint(0,&threadID)<0))return -1;
    //argint : Fetch the nth 32-bit system call argument

    int res=join(threadID);

    return res;

}

int
sys_thread_create(void){
    
    void *arg;
    void *(*start_routine)(void *);
    int *thread;

    if((argint(0,(int*)&thread)<0) || (argptr(1,(void*)&(start_routine),sizeof(start_routine))<0) || (argptr(2,(void*)&arg,sizeof(arg))<0)) return -1;
    return thread_create(thread,start_routine,arg);    
}

int
sys_thread_join(void){

    int thread;
    void **retval;

    if((argint(0,&thread)<0) || (argptr(1,(void*)&retval,sizeof(retval))<0))    return -1;

    return thread_join(thread,retval);
}

int
sys_thread_exit(void){
    void *retval;
    if(argptr(0,(void*)&retval,sizeof(retval)<0)<0) return -1;

    thread_exit(retval);

    return 0;
}
