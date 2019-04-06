#include "syscall.h"
#include "types.h"
#include "user.h"
#include "stat.h"

int main(int argc,char *argv[]){
    
    int tmp=fork(); // 프로세스 생성

    if(tmp<0){ // fork 실패
        
        write(1,"ERROR : FORK FAIL!\n",19);
        exit();
    }
    else if(tmp==0){ // 자식 프로세스
        
        while(1){
            write(1,"Child\n",6);
            yield();
        }
    }
    else{ // 부모 프로세스
        
        while(1){
            write(1,"Parent\n",7);
            yield();
        }
    }

    return 0;

}
