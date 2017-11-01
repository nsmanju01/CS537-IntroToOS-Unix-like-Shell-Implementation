#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>

#define INPUT_SIZE 1000
#define BG_PROCESS 40

//Globals
char error_message[30] = "An error has occurred\n";
char *prompt = "mysh ";
int cmdNum = 1;
char line[INPUT_SIZE];
int process_id[BG_PROCESS];
int bg_process_count =0 ;

struct sigaction sigchld_action = {
  .sa_handler = SIG_DFL,
  .sa_flags = SA_NOCLDWAIT
};

//Function to read a line from stding
bool read_line(char *line, FILE *stream){
    if(fgets(line,INPUT_SIZE,stream) == NULL){
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(0);
    }
    if(strcmp(line,"\n") == 0){
        return false;
    }
    else{
        line[strlen(line)-1] = '\0';
        return true;
    }
}
//Freeing Memory
void freeMemory(int argc, char **argv){
    for(int i=0;i<=argc;i++){
        free(argv[i]);
    }
    free(argv);
}

//Freeing Pipe Memory
void freePipeMemory(int argc1, char **argv1, int argc2, char **argv2){
    int i;
    for(i=0;i<=argc1;i++){
        free(argv1[i]);
    }
    free(argv1);
    for(i=0;i<=argc2;i++){
        free(argv2[i]);
    }
    free(argv2);
}

//check if all spaces
bool checkSpaces(char *str){
	while(*str != 0){
		if(!isspace((unsigned char)*str)){
			return false;
		}
		str++;
	}
	return true;
}

void executeCmd(int argc, char **argv, bool bgprocess){
    int pid;
    if (strcmp(argv[0],"exit") == 0){
      	//Pending kill all the background process
        for (int i=0; i< bg_process_count; i++){
            kill(process_id[i], SIGTERM);
        }
        freeMemory(argc,argv);
        exit(0);
    } 
    else if (strcmp(argv[0],"cd") == 0){
        if(argv[1]){ //if directory argument exist
            char *path = argv[1];
            int retcd = chdir(path); // returns 0 on successful execution, else -1
            if ( retcd == -1 )
            {
                write(STDERR_FILENO, error_message, strlen(error_message)); // print the error message and return
            }
            freeMemory(argc,argv);
        }
        else{
            char *path = getenv("HOME"); // to get the path to the home directory.
            int retcd = chdir(path);    // returns 0 on successful execution, else -1
            if (retcd == -1 )
            {
                write(STDERR_FILENO, error_message, strlen(error_message)); //print the error message and return
            }
            freeMemory(argc,argv);
        }
    }
    else if (strcmp(argv[0],"pwd") == 0){
    	if(argc>1){
    		write(STDERR_FILENO, error_message, strlen(error_message)); //print the error message and return
    	}
    	else{
    		char filePath[1024];
        	getcwd(filePath,sizeof(filePath));
        	printf("%s\n",filePath);
    	}
    	freeMemory(argc,argv);
    }
    else{       
       	pid = fork();
        if(pid == 0){
        	execvp(argv[0],argv);
            write(STDERR_FILENO, error_message, strlen(error_message));
            freeMemory(argc,argv);
            exit(0);
        }
       	else{
           	if (bgprocess != true)
            {   
            	waitpid(pid,NULL,0);
            	fflush(stdout);
            }
            else
            {
                process_id[bg_process_count++] = pid; // stores the p_id of bg child process in process_id global variable and increments count.
            }
            freeMemory(argc,argv);
        }
    }
}

//execute Redirection Command
void executeRedCmd(int argc, char **argv, bool bgprocess, bool rdir_tofile, bool rdir_fromfile, char* outfile_name, char* infile_name){
	int outfile; // defining the file descriptors 
   	int infile;
   	int pid;
	if(rdir_tofile){
		outfile = open(outfile_name,O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU); 
      	if (outfile == -1)
       	{
       		write(STDERR_FILENO, error_message, strlen(error_message));
         	if(rdir_tofile)
           		free(outfile_name);
          	else
             	free(infile_name);
          	freeMemory(argc,argv);
         	return;        
       	}
	}
	if(rdir_fromfile){
		infile = open(infile_name, O_RDONLY); 
       	if (infile == -1)
       	{
         	write(STDERR_FILENO, error_message, strlen(error_message));
           	if(rdir_tofile)
           		free(outfile_name);
          	else
             	free(infile_name);
          	freeMemory(argc,argv);
          	return;
     	}
	}
	pid = fork();
   	if(pid == 0){
	   	if (rdir_tofile) // if both the redirection operators are present ,outfile and infile filedescriptors are updated.
	  	{
	   		dup2(outfile, 1);
	   	}
	   	if (rdir_fromfile) // if both the redirection operators are present ,outfile and infile filedescriptors are updated.
	  	{
	   		dup2(infile, 0);
	   	} 
	   	execvp(argv[0],argv);
	   	write(STDERR_FILENO, error_message, strlen(error_message));
	   	if(rdir_tofile)
          	free(outfile_name);
       	else
          	free(infile_name);
	  	freeMemory(argc,argv);
	  	exit(0);
    }
    else{
	   	if (bgprocess != true)
	  	{   
	    	waitpid(pid,NULL,0);
	       	fflush(stdout);
	   	}
    	else
     	{
        	process_id[bg_process_count++] = pid; // stores the p_id of bg child process in process_id global variable and increments count.
     	}
     	if(rdir_tofile)
          	free(outfile_name);
       	else
          	free(infile_name);
	  	freeMemory(argc,argv);
 	}
}

//function to execute Pipes
void executePipeCmd(int argc1,char **argv1,int argc2,char **argv2){
	int pid1,pid2;
    int pipes[2];
    pipe(pipes);
    pid1 = fork();
    if(pid1 == 0){
    	close(pipes[0]);
       	dup2(pipes[1],1);
        execvp(argv1[0],argv1);
        write(STDERR_FILENO, error_message, strlen(error_message));
        freePipeMemory(argc1,argv1,argc2,argv2);
        exit(0);
    }
    pid2 = fork();
    if(pid2 == 0){
    	close(pipes[1]);
       	dup2(pipes[0],0);
       	setvbuf(stdout, NULL, _IONBF,0);
        execvp(argv2[0],argv2);
        write(STDERR_FILENO, error_message, strlen(error_message));
        freePipeMemory(argc1,argv1,argc2,argv2);
        exit(0);
    }
    if(pid2 != 0){
        close(pipes[0]);
        close(pipes[1]);
        waitpid(pid2,NULL,0);
        kill(pid1, SIGTERM);
        freePipeMemory(argc1,argv1,argc2,argv2);
    }
}

void freeCmd(char **cmds,int numCmds){
	for(int i=0;i<numCmds;i++){
        free(cmds[i]);
    }
    free(cmds);
}

void processPipeCmd(char *line){
	char **cmds = NULL;
    char *rest;
    char *token;
    char **argv1 = NULL,**argv2 = NULL;
    int argc1=0,argc2=0;
    int numCmds=0;
    int i=0;
    //below loop is used to get the two cmds separated by pipe
  	while ((token = strtok_r(line, "|", &line))){
  		if(!checkSpaces(token)){
  			i++;
  			cmds = (char **)realloc(cmds, sizeof(char *) * i);
  			cmds[numCmds++] = strdup(token);
  		}	
  	}
  	//check if after pipe command is missing
  	if(numCmds==1){
  		write(STDERR_FILENO, error_message, strlen(error_message));
  		freeCmd(cmds,numCmds);
  		return;
  	}
  	if(numCmds == 2){
  		i=0;
  		rest = cmds[0];
	  	while ((token = strtok_r(rest, " \t", &rest))){
	  		argv1 = (char **)realloc(argv1, sizeof(char *) * (++i));
	  		argv1[argc1++] = strdup(token);	
	  	}
	  	i++;
	  	argv1 = (char **)realloc(argv1, sizeof(char *) * i); // assigning the space for NULL variable.
	    argv1[argc1]=NULL;
	    i=0;
	    rest = cmds[1];
	  	while ((token = strtok_r(rest, " \t", &rest))){
	  		argv2 = (char **)realloc(argv2, sizeof(char *) * (++i));
	  		argv2[argc2++] = strdup(token);	
	  	}
	  	i++;
	  	argv2 = (char **)realloc(argv2, sizeof(char *) * i); // assigning the space for NULL variable.
	    argv2[argc2]=NULL; 
	    freeCmd(cmds,numCmds);
		executePipeCmd(argc1,argv1,argc2,argv2);
	}
}

//Process Redirection
void processRedCmd(char *line){
	char *token;
    char **argv = NULL;
    char *rest = line;
    int i=0,k=0;
    int argc=0;
    bool bgprocess = false;
    bool rdir_tofile = false;
    bool rdir_fromfile = false;
    int index_to_file ;
    int index_from_file ;
    char * outfile_name;
    char * infile_name;
    bool to_file_check = false;
    bool from_file_check = false;

	while ((token = strtok_r(rest, " \t", &rest))){
       	if(rdir_tofile && (!to_file_check))
       	{
        	outfile_name = strdup(token);
           	to_file_check = true;   
       	}
      	if(rdir_fromfile && (!from_file_check))
      	{
        	infile_name = strdup(token);
        	from_file_check = true;
       	}
        if ((strcmp(token,">") == 0) && (!rdir_tofile)) { // if the input argument contains ">" , store its index in index_to_file;
        	rdir_tofile = true;
          	index_to_file = k;
      	}
      	if ((strcmp(token,"<") == 0 ) && !(rdir_fromfile))
      	{
        	rdir_fromfile = true;
        	index_from_file = k; 
       	}
      	if(!(rdir_tofile || rdir_fromfile))
      	{
      		i++;
        	argv = (char **)realloc(argv, sizeof(char *) * i);
          	argv[argc++] =  strdup(token);
      	}
      	if((strcmp(token,"&") == 0 )){
      		bgprocess = true;
      	}
      	else
      		k++;
  	}
  	i++;
    argv = (char **)realloc(argv, sizeof(char *) * i); // assigning the space for NULL variable.
    argv[argc]=NULL;

 	if (rdir_tofile && rdir_fromfile){
 		if(!from_file_check || !to_file_check){
 			write(STDERR_FILENO, error_message, strlen(error_message));
 			if(outfile_name)
           		free(outfile_name);
          	else
             	free(infile_name);
 			freeMemory(argc,argv);
 			return;
 		}
    	int index = (index_from_file > index_to_file)?index_from_file:index_to_file;
     	if ((k-2) > index)
       	{
        	write(STDERR_FILENO, error_message, strlen(error_message));
         	free(outfile_name);
          	free(infile_name);
          	freeMemory(argc,argv);
           	return;
       	}
    }
   	else{
   		if(!from_file_check && !to_file_check){
 			write(STDERR_FILENO, error_message, strlen(error_message));
 			freeMemory(argc,argv);
 			return;
 		}
      	if( (rdir_tofile && (k-2 > index_to_file )) || (rdir_fromfile && (k-2 > index_from_file) )) //if there are more arguments than required after redirection ,throw error
       	{
          	write(STDERR_FILENO, error_message, strlen(error_message));
          	if(rdir_tofile)
           		free(outfile_name);
          	else
             	free(infile_name);
           	freeMemory(argc,argv);
            return;
        }
   	}
   	executeRedCmd(argc,argv,bgprocess,rdir_tofile,rdir_fromfile,outfile_name,infile_name);
}

void processCmd(char *line){
    char *token;
    char **argv = NULL;
    char *rest = line;
    int i=0;
    int argc=0;
    bool bgprocess = false;
    //Handle Pipeline - preparing command line argumenrts for pipe
    if(strchr(line,'|') != NULL){
    	processPipeCmd(line);
    }   
    else if( (strchr(line,'<') != NULL) || (strchr(line,'>') != NULL) ){
    	processRedCmd(line);
    }
  	else{
	    while ((token = strtok_r(rest, " \t", &rest))){
	        i++;
	        argv = (char **)realloc(argv, sizeof(char *) * i);
	        argv[argc] =  strdup(token);
	        argc++;
	    }
	    i++;
	    if (strcmp(argv[argc-1],"&") == 0){ // checks for &, if present & is replaced by NULL.
	        bgprocess = true;
	        free(argv[argc-1]);
	        argv[--argc] = NULL;
	    }
	    else{
	        argv = (char **)realloc(argv, sizeof(char *) * i); // assigning the space for NULL variable.
	        argv[argc]=NULL;
	    }
	    executeCmd(argc,argv,bgprocess);
	}
}

int main(int argc, char **argv){
	if(argc>1){
		write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
	}
	sigaction(SIGCHLD, &sigchld_action, NULL);
    while(1){
        printf("%s(%d)> ",prompt,cmdNum);
        fflush(stdout);
        bool cont = read_line(line,stdin); //cont to decide whether we need to continue
        bool incCount = true; //decide to increase the count or not.
        if(strlen(line) > 128){
            write(STDERR_FILENO, error_message, strlen(error_message));     
            cont=false;          
        }
        if(checkSpaces(line)){
   			cont = false;
        	incCount = false;
        }
        if(cont){
            processCmd(line);
        }
        if(incCount)
        	cmdNum++;
    }
}
