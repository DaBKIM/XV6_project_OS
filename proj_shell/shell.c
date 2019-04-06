#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/wait.h>
#define MAX 10000

int main(int argc,char* argv[]){

    if(argc==0){ // 아무것도 들어오지 않았을 때 에러를 출력

        printf("error : No arguments.\n");
        return 0;
    }
    else if(argc==1){ // Intertactive mode
        
        while(1){
        
            printf("prompt> ");
            char* in=(char*)malloc(sizeof(char)); // 명령어를 받을 변수
            fgets(in,MAX,stdin); // 명령어 받기
  
            char* end="quit\n";
            if((!strcmp(in,end))||in==NULL) exit(EXIT_FAILURE);
            //end 또는 in이 NULL일때 프로세스에서 빠져나온다.

            char** cut=(char**)malloc(sizeof(char));
            // 명령어가 한번에 많이 들어왔을 때 특정 심볼을 중심으로 구분한
            //각 명령어들을 저장할 변수

            int i;
            cut[0]=(char*)malloc(sizeof(char)); 
            cut[0]=strtok(in,";\n");

            for(i=1;;i++){

                cut[i]=(char*)malloc(sizeof(char));
                cut[i]=strtok(NULL,";\n");
                if(cut[i]==NULL) break;
               
            } // 명령어 분류+저장
           
            int j;
            for(j=0;j<i;j++){ // 각 명령어들을 동작시킴

                pid_t pid,waitpid;
                int status;
                pid=fork();// 명령어들을 돌릴 자식 프로세스 생성

                if(pid==-1){ // 프로세스가 제대로 생성 안됐을 경우 에러출력
                    printf("error : Can't fork.\n");
                    exit(0);
                }
                else if(pid==0){ // 자식 프로세스 동작

                   
                   // ls -al 같은 두개의 명령어로 동작하는 경우를 대비한
                   // 명령어 분류
                    char** cut2=(char**)malloc(sizeof(char));
                    cut2[0]=(char*)malloc(sizeof(char));
                    cut2[0]=strtok(cut[j]," ");
                   
                    int z;
                    for(z=1;;z++){
                        cut2[z]=(char*)malloc(sizeof(char));
                        cut2[z]=strtok(NULL," ");
                        if(cut2[z]==NULL) break;
                       
                    }
                   
                    execvp(cut2[0],cut2); // 명령어 실행
                }
                else{
                 //부모 프로세스. 자식 프로세스가 전부 종료될 때때 까지 wait
                    while((waitpid=wait(&status))>0);
                }

            }

        }
    }else{ // Batch mode
        FILE* fi= fopen(argv[1],"r"); // 파일 open

        while(1){
            char* in=(char*)malloc(sizeof(char));
            if(fgets(in,MAX,fi)==NULL){ // 파일의 명령어를 전부 읽어왔을 경우 종료
                fclose(fi);
                exit(EXIT_FAILURE);
            }

            printf("%s",in);

            char** cut=(char**)malloc(sizeof(char)); // 명령어 분류+ 저장 작업
            
            int i;
            cut[0]=(char*)malloc(sizeof(char));
            cut[0]=strtok(in,";\n");
           

            for(i=1;;i++){

                cut[i]=(char*)malloc(sizeof(char));
                cut[i]=strtok(NULL,";\n");
                if(cut[i]==NULL) break;
               
            }
           
            int j;
            for(j=0;j<i;j++){ // 명령어 실행

              
                pid_t pid,waitpid;
                int status;
                pid=fork(); // 자식 프로세스 생성

                if(pid==-1){ // 자식 프로세스 생성 오류
                    printf("error : Can't fork.\n");
                    exit(0);
                }
                else if(pid==0){ // 명령어 정상 작동

                   
                   //명령어가 중간에 공백을 포함한 경우를 대비해서
                   //한번더 명령어 분류+저장
                    char** cut2=(char**)malloc(sizeof(char));
                    cut2[0]=(char*)malloc(sizeof(char));
                    cut2[0]=strtok(cut[j]," ");
                   
                    int z;
                    for(z=1;;z++){
                        cut2[z]=(char*)malloc(sizeof(char));
                        cut2[z]=strtok(NULL," ");
                        if(cut2[z]==NULL) break;
                       
                    }
                   // 명령어 실행
                    execvp(cut2[0],cut2);
                }
                else{
                   // 부모 프로세스. 자식 프로세스들이 전부 종료 될 때 까지 대기
                    while((waitpid=wait(&status))>0);
                }

            }
        }

    }
    exit(EXIT_FAILURE);
}
