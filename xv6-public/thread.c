//thread func

#include "types.h"
#include "user.h"
#include "mmu.h"
#include "x86.h"
#include "param.h"
#include "proc.h"

#define NULL 0


int 
thread_create(thread_t *thread,void(*func) (void*),void *arg){
//(*start_routine) -> (*func)
    
    void *stack=malloc((uint)PGSIZE*2);
    int ret_val;
    
    if((uint)stack<=0){
        printf(1,"thread_create : Allocate error.\n");
        ret_val=-1;
        return ret_val;
    }// if failed to allocate stack

    if((uint)stack%PGSIZE==1) stack+= PGSIZE-((uint)stack%PGSIZE);
    //for argument make stack space

    *thread=create_t(func,arg,stack);//create in "proc.c"
    ret_val=*thread;

    if(ret_val>=0) ret_val = -1;
    else ret_val=0;
    
    return ret_val;// if some error is occured ret_val will be non zero
}

int
thread_join(thread_t thread,void **retval){

    void *stack;
    int ret_val=0;

    if(join((uint)thread,&stack)<0) ret_val=-1;
    //join in "proc.c"

    free(stack);

    return ret_val;// if some error is occured ret_val will be non zore

}

void 
thread_exit(void *retval){
    retval=(int*)(proc->pid);
    exit();
}
