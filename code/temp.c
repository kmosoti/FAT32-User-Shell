/*
	Name: Kennedy Mosoti
	ID: 1001596311
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>

/*************************************************************************************************/

#define MAX_COMMAND_SIZE 255
#define MAX_NUM_ARGUMENTS 10
// #define MAX_HISTORY 15
// #define MAX_PID_HISTORY 15

/**************************************************************************************************/

// void change_directory(char* new_path);
// char* toLower(char* s);


// typedef struct history_queue {
// 	char commands[MAX_NUM_ARGUMENTS][MAX_COMMAND_SIZE];
// 	int front, last, size;
// 	int capacity;
// } history_queue;

//This function allows the shell to create a queue with a set size.
// history_queue* initialize_queue(int max_size){
	
// 	history_queue* queue = (history_queue*)malloc(sizeof(history_queue));
	
// 	queue->capacity = max_size;
// 	queue->front = queue->size = 0;

// 	queue->last = max_size - 1;
// 	//queue-
// 	return queue;

// }
// void print_queue(history_queue* queue);
// void execute_statement(history_queue* pid_history, char* program, char** arguments);

// int isFull(history_queue* queue){
// 	return (queue->size == queue->capacity);
// }
// int isEmpty(history_queue* queue){
// 	return ( queue->size == 0);
// }

// void dequeue(history_queue* queue);

//Addes string data structure to queue from the last position
// void enqueue(history_queue* queue, char* item){
// 	if (isFull(queue)){ dequeue(queue);}
// 	queue->last = (queue->last + 1) % queue->capacity;
// 	strcpy(queue->commands[queue->last], item);
// 	queue->size = queue->size + 1;
// 	printf("\n%s added to history\n", item); 
// }

//Removes the first entry of a queue preserving the FIFO order of queues
// void dequeue(history_queue* queue){
// 	queue->front = (queue->front + 1) % queue->capacity;
// 	queue->size = queue->size - 1;
// 	printf("Front of queue removed");
// }

// //Grabs the first value in a queue
// char* get_front(history_queue* queue){
// 	if(isEmpty(queue)) return "NO RECENT COMMANDS";
// 	return queue->commands[queue->front];
// }
// //Simply gets the last value stored in a queue
// char* get_last(history_queue* queue){
// 	if(isEmpty(queue)) return "NO RECENT COMMANDS";
// 	return queue->commands[queue->last];
// }
/*************************************************************************************************/

int main(){
	
	char command_string[MAX_COMMAND_SIZE];
	char* supported_commands[] = {"cd","pidhistory","history","n!"};
	history_queue* command_history = initialize_queue(MAX_NUM_ARGUMENTS);
	history_queue* pid_history = initialize_queue(MAX_PID_HISTORY);
	while(1){
		//char* arguments[MAX_NUM_ARGUMENTS-1];
		printf("msh> ");
		
		//Waits for user input with a max size of 255
		while(!fgets(command_string, MAX_COMMAND_SIZE, stdin));

		//Remove new line character from in put
		

		//printf("\ncommand_string = '%s'",command_string);
		if(!strncmp(command_string, "quit",4) || !strncmp(command_string, "exit",4)){
			exit(0);
		}
		else if(!strcmp(command_string,"\n")){
			continue;
		}
		//The input will not be tokenized to parse the command
		char* token[MAX_NUM_ARGUMENTS];
		int token_count = 0;
		
		//char* argument_ptr;
		char* working_string = strdup(command_string);


		int token_indexer = 0;
		token[token_indexer] = strtok(working_string," \n");

		//printf("\nCommand: %s\n",token[0]);
		while (token[token_indexer] != NULL && token_indexer < MAX_NUM_ARGUMENTS){
			//printf("Token_indexer == '%d'\n",token_indexer);		
			token[++token_indexer] = strtok(NULL," \n");
		};
		
		/*
		To handle requirments 11 and 12 shell catches the special inputs of 
		cd, pidhistory, n!, and history and handles them uniquely.

		Queues are used to keep track of the history for PIDs and Commands

		*/
		if(!strcmp(toLower(token[0]),"cd")){
			if(token_indexer > 2 || token_indexer <= 1){
				printf("Invalid parameters for cd\n");
			}
			else{
				change_directory(token[1]);
			}	
			continue;
		}
		else if(!strcmp(toLower(token[0]),"pidhistory")){
			printf("Getting most recently spawned processess\n");
			print_queue(pid_history);
			continue;
		}
		else if(!strcmp(toLower(token[0]),"n!")){
			printf("\nMost recent command: %s\n", get_last(command_history));
			continue;
		}
		else if(!strcmp(toLower(token[0]),"history")){
			printf("Getting History of Commands\n");
			print_queue(command_history);
			continue;
		}

		//initialize path variables for execution attempt
		// int file_exists = 0;
		// char curr_dir[] = "./";
		// char bin[] = "/bin/";
		// char usr_bin[] = "/usr/bin/";
		// char usr_local_bin[] = "/usr/local/bin/";
		// char command_execute[100];

		/*Here we are copying the suspected executable command into likely 
		directories to scan*/
		// strcat(curr_dir, token[0]);
		// strcat(bin, token[0]);
		// strcat(usr_bin, token[0]);
		// strcat(usr_local_bin, token[0]);

		//printf("File path: %s\n",bin);

		/*Each if statement utilizes the access function to check 
		if a file exists and is OK to be handled otherwise it will default to 
		"command not found"*/

		// if(!access(curr_dir, F_OK)){
		// 	file_exists = 1;
		// 	strcpy(command_execute, curr_dir);
		// }
		// else if(!access(bin, F_OK)){
		// 	file_exists = 1;
		// 	strcpy(command_execute, bin);
		// }
		// else if(!access(usr_bin, F_OK)){
		// 	file_exists = 1; //Currently doesn't work with files stored in /usr/bin	
		// 	strcpy(command_execute, usr_bin);
		// }
		// else if(!access(usr_local_bin, F_OK)){
		// 	file_exists = 1;
		// 	strcpy(command_execute, usr_local_bin);	
		// }		
		// if(file_exists){
		// 	/*If the program is found and okay to use the shell adds it to 
		// 	command_history and executes the statement*/
		// 	enqueue(command_history, command_execute);
		// 	execute_statement(pid_history, command_execute, token);
		// }
		// else{
		// 	printf("%s : Command not found\n", token[0]);
		// }
		free(working_string);
		memset(command_string, 0, sizeof(command_string));
	}
	return 0;
}
/*************************************************************************************************/
/*
execute_statement takes a queue data type, name of a valid program, and its arguments to run.

It then creates a child process and saves the PID to the PID history queue and then waits for the child function to exit
*/
void execute_statement(history_queue* pid_history, char* program, char** arguments){
	pid_t pid = fork();
	int status;
	char pid_string[15];
	if(pid != 0){
		sprintf(pid_string,"%d", pid);
	}
	if (pid ==0){
		execv(program, arguments);
		exit(0);
	}
	enqueue(pid_history, pid_string);
	waitpid(pid, &status, 0);
}

//char * get_command_line(char* program);

//Simply takes the path of a directory and goes to it only for the running shell script
void change_directory(char* new_path){
	chdir(new_path);
}

//Used to lower the case of the commands to avoid errors in comparisons;
char* toLower(char* s){
	for(char *p = s; *p; p++) *p= tolower(*p);
	return s;
}

//Takes a queue data structure as an object and prints each value in the command array
void print_queue(history_queue* queue){
	for (int i = 0; i < queue->size; i++){
		printf("[%d]: %s\n",i,queue->commands[i]);
	}
}
