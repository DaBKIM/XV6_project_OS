#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/wait.h>
#define MAX 10000
int main(int argc,char* argv[]){
    //No arguments
    if(argc == 0){
        printf("Error : No Arguments!\n");
        exit(0);
    }
    // Interactive mode
    else if(argc == 1){
        
        while(1){
            printf("prompt> ");
            fflush(NULL);
            // Input
            char* input=(char*)malloc(sizeof(char));
            fgets(input,MAX,stdin);
            // End of the program
            // If input contains 'quit', terminate the shell.
            // If shell reachs End of file(ctrl+D), terminate the shell.
            char* tr = strstr(input,"quit");
            if(tr!=NULL) exit(EXIT_FAILURE);
            if(feof(stdin)){
                fflush(stdout);
                exit(0);
            }
            // Save the command + cut the command by the tokens
            char* order[100] = {NULL,};
            char* tmp = strtok(input,";\n");
            int i =0;
            while(tmp != NULL){
                order[i] = tmp;
                i++;
                tmp = strtok(NULL,";\n");
            }
 
     
            // Start the process
            int j;
            for(j=0;j<i;j++){
                
                int pid,wait_p,status;
                pid = fork();
                if(pid == -1){
                    printf("Error : Fork Error!\n");
                    exit(0);
                }
                else if(pid == 0){
                    // For commands which has more than one word.
                    // Delete blank.
                    char* token[100]={NULL,};
                    char* tmp_2=strtok(order[j]," ");
                   
                    int k=0;
                    while(tmp_2 != NULL){
                        token[k]=tmp_2;
                        k++;
                        tmp_2=strtok(NULL," ");
                    }
                   
                    // Execute the commands
                    execvp(token[0],token);
                 
                }
                // Wait for the end of the command.
                else{
                    while((wait_p=wait(&status))>0);
                }
                
            }
        }
    }
    //Batch mode
    else{
        
        // Open the file
        FILE* fi = fopen(argv[1],"r");
        while(1){
            
            char* input=(char*)malloc(sizeof(char));
            if(fgets(input,MAX,fi) == NULL){
                fclose(fi);
                exit(EXIT_FAILURE);
            }
            printf("%s",input);
            // Save the command + cut the command by the tokens
            char* order[100] = {NULL,};
            char* tmp = strtok(input,";\n");
            int i =0;
            while(tmp != NULL){
                order[i] = tmp;
                i++;
                tmp = strtok(NULL,";\n");
            }
 
     
            // Start the process
            int j;
            for(j=0;j<i;j++){
                
                int pid,wait_p,status;
                pid = fork();
                if(pid == -1){
                    printf("Error : Fork Error!\n");
                    exit(0);
                }
                else if(pid == 0){
                    // For commands which has more than one word.
                    // Delete blank.
                    char* token[100]={NULL,};
                    char* tmp_2=strtok(order[j]," ");
                   
                    int k=0;
                    while(tmp_2 != NULL){
                        token[k]=tmp_2;
                        k++;
                        tmp_2=strtok(NULL," ");
                    }
                   
                    // Execute the commands
                    execvp(token[0],token);
                 
                }  
                
                // Wait for the end of the command.
                else{
                    while((wait_p=wait(&status))>0);
                }
                
            }
        }
    }
    exit(EXIT_FAILURE);
}