#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdint.h>
#include <ctype.h>

#define MAX_ARGS 11
#define MAX_TOKENS 12
#define MAX_COMMAND_LEN 255
#define WHITESPACE " \t\n"
#define MAX_HISTORY 50

typedef struct cmd{
	char buffer[MAX_COMMAND_LEN];
	char *tokens[MAX_TOKENS];
	pid_t pid;
} cmd;



static struct fs_info{
	uint16_t bps;
	uint8_t spc;
	uint16_t rsc;
	uint8_t num_fats;
	uint32_t fatsz_32;
	int cluster_start;
	int cluster_size;
} info;


static struct __attribute__((__packed__)) DirectoryEntry {
	char DIR_Name[11];
	uint8_t DIR_Attr;
	uint8_t Unused1[8];
	uint16_t DIR_FirstClusterHigh;
	uint8_t Unused2[4];
	uint16_t DIR_FirstClusterLow;
	uint32_t DIR_FileSize;
} dir[16];


static char working_dir[12];
int32_t dir_sector;

static FILE *fs;

//changes from a logical block address to a physical offset
int LBAToOffset(int32_t sector){
	return ( (sector-2)*info.bps + (info.bps*info.rsc) + (info.num_fats*info.fatsz_32*info.bps) );
}

//follows FAT linked list to next sector
uint16_t NextLB( uint32_t sector){
	uint32_t FATAddress = info.bps*info.rsc + sector*4;
	uint16_t val;
	fseek(fs, FATAddress, SEEK_SET);
	fread(&val, 2, 1, fs);
	return val;
}

//returns whether the fat32 filename is equal to the human filename
int compare_filenames(char *IMG_Name, char *input){
	int i, final_dot;
	char processed_input[13];
	char processed_image[12];
	char input_pre[13];
	char input_suf[13];
	char image_pre[9];
	char image_suf[4];


	memcpy(processed_image, IMG_Name, 11);
	strncpy(processed_input, input, 12);
	processed_image[11] = '\0';
	processed_input[12] = '\0';

	//some special cases
	if(!strcmp(processed_image, ".          ")){
		if(!strcmp(processed_input, ".")){
			return 0;
		} 
		return -1;
	}
	if(!strcmp(processed_image, "..         ")){
		if(!strcmp(processed_input, "..")){
			return 0;
		} 
		return -1;
	}

	i=0; final_dot=-1;
	while(processed_input[i] != '\0'){
		if(processed_input[i] == '.')
			final_dot=i;
		i++;
	}

	for(i=0; i<12; i++){
		processed_input[i] = toupper(processed_input[i]);
	}

	if(final_dot!=-1){
		strncpy(input_pre, processed_input, final_dot);
		input_pre[final_dot] = '\0';
		strcpy(input_suf, processed_input+final_dot+1);
	}else{
		strcpy(input_pre, processed_input);
		
		input_suf[0] = '\0';
	}
	memcpy(image_pre, processed_image, 8);
	image_pre[8]='\0';
	i=7;
	while(image_pre[i] == ' ' && i>0){
		image_pre[i] = '\0';
		i--;
	}
	memcpy(image_suf, processed_image+8, 4);
	i=0;
	while(image_suf[i] == ' ' && i<4){
		image_suf[i] = '\0';
		i++;
	}

	return strcmp(input_pre, image_pre)||strcmp(input_suf, image_suf);
}

//load directory into DirectoryEntry array
void read_directory(uint32_t sector){
	int offset = LBAToOffset(sector);
	fseek(fs, offset, SEEK_SET);
	fread(&dir, sizeof(struct DirectoryEntry), 16, fs);
	return;
}

//search for file in the working dir
int find_file(char *filename){
	int i;
	int32_t dirpointer;
	dirpointer=dir_sector;
	while(dirpointer!=-1 && dirpointer!=0){
		read_directory(dirpointer);
		for(i=0; i<16; i++){
			if(dir[i].DIR_Name[0] == (char)0x00){ dirpointer=-1; break;}//0x00 means last directory entry
			if(!(compare_filenames(dir[i].DIR_Name, filename))){
				return i;
			}
		}
		dirpointer=NextLB(dirpointer);
	}
	printf("Error: File not found in working directory.\n");
	return -1;
}

//find a free dir entry
int free_dir_entry(){
	int i;
	int32_t dirpointer;
	dirpointer=dir_sector;
	while(dirpointer!=-1 && dirpointer!=0){
		read_directory(dirpointer);
		for(i=0; i<16; i++){
			if(dir[i].DIR_Name[0] == (char)0x00){ 
				if(i!=15){
					char blank = '\0';
					//int offset = ((void *)(dir[i+1].DIR_Name+0) - (void *)dir) + LBAToOffset(dirpointer);
					int offset = LBAToOffset(dirpointer) + 32*(i+1);
					fseek(fs, offset, SEEK_SET);
					fwrite(&blank, 1, 1, fs);
				}
				return LBAToOffset(dirpointer) + 32*i; 
			}
		}
		dirpointer=NextLB(dirpointer);
	}
	printf("Sorry, no more space in directory for that file.(use better software than this)\n");
	return -1;
}

//print a direcotry (ls)
void print_directory(){
	int i;
	int32_t dirpointer;
	printf("CWD=%s\n", working_dir);
	dirpointer=dir_sector;
	while(dirpointer!=-1 && dirpointer!=0){
		read_directory(dirpointer);
		for(i=0; i<16; i++){
			uint32_t cluster;
			if(dir[i].DIR_Attr & (uint16_t)0x02) continue;
			if(dir[i].DIR_Attr & (uint16_t)0x04) continue;
			if(dir[i].DIR_Attr & (uint16_t)0x08) continue;

			cluster = (((uint32_t)dir[i].DIR_FirstClusterHigh) << 16) | ((uint32_t)dir[i].DIR_FirstClusterLow);
			if(dir[i].DIR_Name[0] == (char)0xe5) continue;
			if(dir[i].DIR_Name[0] == (char)0x00){ dirpointer=-1; break;}//0x00 means last directory entry
			printf("\t\"%.*s\" cluster:%-10d size:%-10d", 11, dir[i].DIR_Name, (int)cluster, dir[i].DIR_FileSize);
			if(dir[i].DIR_Attr & (uint16_t)0x10) printf("(subdirectory)");
			if(dir[i].DIR_Attr & (uint16_t)0x02) printf("(hidden)");
			if(dir[i].DIR_Attr & (uint16_t)0x04) printf("(system)");
			if(dir[i].DIR_Attr & (uint16_t)0x08) printf("(volume)");
			printf("\n");
		}
		dirpointer=NextLB(dirpointer);
	}
	return;
}

//change directory to the filename
void change_directory(char *filename){
	int i;
	int32_t dirpointer;
	dirpointer=dir_sector;
	while(dirpointer!=65535 && dirpointer!=0){
		read_directory(dirpointer);
		for(i=0; i<16; i++){
			if(dir[i].DIR_Name[0] == (char)0x00){ dirpointer=-1; break;}//0x00 means last directory entry
			if(!(compare_filenames(dir[i].DIR_Name, filename))){
				if(!(dir[i].DIR_Attr & (uint16_t)0x10)){
					printf("That entry doesn't seem to be a directory...\n");
					return;
				}
				uint32_t cluster = (((uint32_t)dir[i].DIR_FirstClusterHigh) << 16) | ((uint32_t)dir[i].DIR_FirstClusterLow);
				if(cluster == 0){
					dir_sector = 2;
					strcpy(working_dir, "ROOT");
					working_dir[11] = '\0';
				}else{
					dir_sector = cluster;
					strcpy(working_dir, dir[i].DIR_Name);
					working_dir[11] = '\0';
				}
				return;
			}
		}
		dirpointer=NextLB(dirpointer);
	}
	printf("Error: File not found in working directory.\n");
}

//splits filepaths for change_directory
void outer_change_directory(char *filepath){
	if(filepath[0] == '/'){
		dir_sector = 2;

		char *tok = strtok(filepath+1, "/");
		while(tok != NULL){
			change_directory(tok);
			tok = strtok(NULL, "/");
		}
	}else{
		char *tok = strtok(filepath, "/");
		while(tok != NULL){
			change_directory(tok);
			tok = strtok(NULL, "/");
		}
	}

}

//read the fsinfo from the loaded image
void populate_fs_info(){
	fseek(fs, 11, SEEK_SET);
	fread(&(info.bps), 2, 1, fs);

	fseek(fs, 13, SEEK_SET);
	fread(&(info.spc), 1, 1, fs);

	fseek(fs, 14, SEEK_SET);
	fread(&(info.rsc), 2, 1, fs);

	fseek(fs, 16, SEEK_SET);
	fread(&(info.num_fats), 1, 1, fs);

	fseek(fs, 36, SEEK_SET);
	fread(&(info.fatsz_32), 4, 1, fs);

	info.cluster_start = (info.num_fats*info.fatsz_32*info.bps)+(info.rsc*info.bps);
	info.cluster_size = (info.spc*info.bps);
}

//info
void print_fs_info(){
	printf("BPB_BytesPerSec: %10x%20d\n", info.bps, info.bps);
	printf("BPB_SecPerClus:  %10x%20d\n", info.spc, info.spc);
	printf("BPB_RsvdSecCnt:  %10x%20d\n", info.rsc, info.rsc);
	printf("BPB_NumFATS:     %10x%20d\n", info.num_fats, info.num_fats);
	printf("BPB_FATSz32:     %10x%20d\n", info.fatsz_32, info.fatsz_32);
}

//stat
void print_file_info(char *filename){
	int fileposition = find_file(filename);
		uint32_t cluster = (((uint32_t)dir[fileposition].DIR_FirstClusterHigh) << 16) | ((uint32_t)dir[fileposition].DIR_FirstClusterLow);
			printf("\"%.*s\" cluster:%-10d size:%-10d", 11, dir[fileposition].DIR_Name, cluster, dir[fileposition].DIR_FileSize);
			if(dir[fileposition].DIR_Attr & (uint16_t)0x10) printf("(subdirectory)");
			if(dir[fileposition].DIR_Attr & (uint16_t)0x02) printf("(hidden)");
			if(dir[fileposition].DIR_Attr & (uint16_t)0x04) printf("(system)");
			if(dir[fileposition].DIR_Attr & (uint16_t)0x08) printf("(volume)");
			printf("\n");
			return;
	printf("Error: File not found in working directory.\n");
}

//read file to screen
void read_file(char *filename, int position, int num_bytes){
	int fileposition, offset, working_cluster, left_to_read, working_ptr;
	uint8_t *page;
	page = (uint8_t *) malloc(info.bps);
	fileposition = find_file(filename);
	working_cluster = (int)(((uint32_t)dir[fileposition].DIR_FirstClusterHigh) << 16) | ((uint32_t)dir[fileposition].DIR_FirstClusterLow);
	offset = LBAToOffset(working_cluster);

	fseek(fs, offset, SEEK_SET);
	fread(page, info.bps, 1, fs);

	//nobody said it had to be efficient
	left_to_read = num_bytes;
	working_ptr = position;
	while(left_to_read>0){
		if(working_ptr >= info.bps){
			//load new page
			working_ptr = 0;
			offset = LBAToOffset(working_cluster = NextLB(working_cluster));
			fseek(fs, offset, SEEK_SET);
			fread(page, info.bps, 1, fs);
			continue;
		}
		printf("%.2x ",page[working_ptr]);
		left_to_read--;
		working_ptr++;
	}
	printf("\n");
	
	fflush(stdout);
	free(page);
}

//get file from image
void get(char *filename){
	int fileposition, offset, working_cluster, left_to_read, working_ptr;
	uint8_t *page;
	FILE *outfile;
	outfile = fopen(filename, "w");
	page = (uint8_t *) malloc(info.bps);
	fileposition = find_file(filename);
	left_to_read = dir[fileposition].DIR_FileSize;
	working_cluster = (int)(((uint32_t)dir[fileposition].DIR_FirstClusterHigh) << 16) | ((uint32_t)dir[fileposition].DIR_FirstClusterLow);
	offset = LBAToOffset(working_cluster);
	fseek(fs, offset, SEEK_SET);
	fread(page, info.bps, 1, fs);

	//nobody said it had to be efficient
	working_ptr = 0;
	while(left_to_read>0){
		if(working_ptr >= info.bps){
			//load new page
			working_ptr = 0;
			offset = LBAToOffset(working_cluster = NextLB(working_cluster));
			fseek(fs, offset, SEEK_SET);
			fread(page, info.bps, 1, fs);
			continue;
		}
		fprintf(outfile, "%c",page[working_ptr]);
		left_to_read--;
		working_ptr++;
	}

	fflush(outfile);
	fclose(outfile);
	free(page);
}

//locate a free sector in the FAT
int find_free_sector(){
	int i=2; 
	while(1){
		uint32_t FATAddress = (info.bps * info.rsc) + (i*4);
		int16_t val;
		fseek(fs, FATAddress, SEEK_SET);
		fread(&val, 2, 1, fs);
		if(val == 0){
			return i;
		}
		i++;
	}
}

//convert human filename to fat
void human_to_fat(char *into, char *input){
	char processed_input[12];
	char input_pre[12];
	char input_suf[12];
	int i, final_dot;


	strncpy(processed_input, input, 11);
	processed_input[11] = '\0';

	i=0; final_dot=-1;
	while(processed_input[i] != '\0'){
		if(processed_input[i] == '.')
			final_dot=i;
		i++;
	}

	for(i=0; i<11; i++){
		processed_input[i] = toupper(processed_input[i]);
	}

	if(final_dot!=-1){
		strncpy(input_pre, processed_input, final_dot);
		input_pre[final_dot] = '\0';
		strcpy(input_suf, processed_input+final_dot+1);
	}else{
		strcpy(input_pre, processed_input);
		input_suf[0] = '\0';
	}

	memset(processed_input, ' ', 12);
	strncpy(processed_input, input_pre, 8);
	
	strncpy(processed_input+11-strlen(input_suf), input_suf, strlen(input_suf));
	for(i=0; i<12; i++){
		if(processed_input[i] == '\0'){
			processed_input[i] = ' ';
		}
	}
	processed_input[11] = '\0';

	strncpy(into, processed_input,12);

}

//add a directory entry of some size to cwd
uint32_t add_directory_entry(char *human, int size){
	char fat[12] = {'\0'};
	int free_sec = find_free_sector();
	uint32_t free_sec_32 = (uint32_t) free_sec;
	int free_dir_offset = free_dir_entry();
	int i;
	uint32_t val;
	uint32_t FATAddress;

	i=0; val=0; FATAddress=0;
	while(i<=size/info.bps){
		if(i==size/info.bps){//last one
			val = 0x0fffffff;
			FATAddress = (info.bps * info.rsc) + (free_sec*4);
			fseek(fs, FATAddress, SEEK_SET);
			fwrite(&val, 4, 1, fs);
		}else{ //not last one
			val = 0x00000001;
			FATAddress = (info.bps * info.rsc) + (free_sec*4);
			fseek(fs, FATAddress, SEEK_SET);
			fwrite(&val, 4, 1, fs);
			
			free_sec = find_free_sector();

			val = (uint32_t)free_sec;
			fseek(fs, FATAddress, SEEK_SET);
			fwrite(&val, 4, 1, fs);
		}
		i++;
	}


	human_to_fat(fat, human);


	//strcpy(dir[free_dir].DIR_Name, fat);
	fseek(fs, free_dir_offset, SEEK_SET);
	fwrite(fat, 11, 1, fs);

	//dir[free_dir].DIR_Attr = (uint8_t)0x20;
	uint8_t used_val = (uint8_t)0x20;
	fseek(fs, free_dir_offset+11, SEEK_SET);
	fwrite(&used_val, 1, 1, fs);
	
	//dir[free_dir].DIR_FirstClusterHigh = (uint16_t)free_sec_32 >> 16;
	//dir[free_dir].DIR_FirstClusterLow = (uint16_t)(free_sec_32 & (uint32_t)0x0000ffff);
	uint16_t high, low;
	high = (uint16_t)free_sec_32 >> 16;
	low = (uint16_t)(free_sec_32 & (uint32_t)0x0000ffff);
	fseek(fs, free_dir_offset+20, SEEK_SET);
	fwrite(&high, 2, 1, fs);
	fseek(fs, free_dir_offset+26, SEEK_SET);
	fwrite(&low, 2, 1, fs);

	//dir[free_dir].DIR_FileSize = (uint32_t)size;
	uint32_t size_val = (uint32_t)size;
	fseek(fs, free_dir_offset+28, SEEK_SET);
	fwrite(&size_val, 4, 1, fs);

	read_directory(dir_sector);
	return free_sec_32;
}

//put a file into the working dir of fat32 image
void put(char *filename){
	uint8_t *page;
	FILE *infile;
	char ch;
	int size;
	uint32_t writing_sector;
	int working_ptr;
	page = (uint8_t *) malloc(info.bps);

	if(!(infile = fopen(filename, "r"))){
		printf("Error: I can't open that file in your working directory\n");
		return;
	}

	fseek(infile, 0, SEEK_END);
	size = ftell(infile);
	fseek(infile, 0, SEEK_SET);

	writing_sector = add_directory_entry(filename, size);

	working_ptr = 0;
	while((ch = fgetc(infile)) != EOF ){
		if(working_ptr == 512){
			working_ptr = 0;
			fseek(fs, LBAToOffset(writing_sector), SEEK_SET);
			fwrite(page, info.bps, 1, fs);
			writing_sector = NextLB(writing_sector);
			memset(page, '\0', info.bps);
		}
		page[working_ptr] = ch;
		working_ptr++;
	}
	fseek(fs, LBAToOffset(writing_sector), SEEK_SET);
	fwrite(page, info.bps, 1, fs);

	fflush(fs);
	free(page);
}

/*deep copies command so that tokens point to self contained strings*/
void copycmd(cmd *to, cmd *from){
	int i;
	memcpy(to->buffer, from->buffer, MAX_COMMAND_LEN);
	for(i=0; i<MAX_TOKENS; i++){
		if(from->tokens[i] == NULL){
			to->tokens[i] = NULL;
		}else{
			to->tokens[i] = 
				from->tokens[i] - from->buffer + to->buffer;//position relative
		}
	}
	to->pid = from->pid;
}

/*function used for debugging*/
void print_tokens(cmd *command){
	int i;
	for(i=0; i<MAX_TOKENS ; i++){
		printf("tokens[%d] = %s\n", i, command->tokens[i]);
	}
}

/*
	Takes in a command string and uses it to populate tokens[]
	Note: destroys buffer string
*/
void tokenize_cmd(cmd *command){
	char *save_ptr;
	int token_count = 0;

	command->tokens[0] = 
		strtok_r(command->buffer, WHITESPACE, &save_ptr);//first token must exist
	token_count = 1;
	while(
		(token_count < MAX_TOKENS)/*note short circuiting prevents array out of bounds*/
		&&
		(command->tokens[token_count] = strtok_r(NULL, WHITESPACE, &save_ptr))
	){token_count++;}
	command->tokens[MAX_TOKENS-1] = (char *)NULL;//null terminate tokens array

	return;
}


/*executes a command without any ";" tokens
called after command is split by split_command()*/
void execute_command(cmd *command/*, pid_hist *pids, cmd_hist *cmds*/){
	//char *path[] = PATH;
	//char program_path[MAX_PATH_LEN];
	pid_t successful_pid = -1;//assume pid isnt' success until it is

	
	//SPECIAL CASES
	if(command->tokens[0] == NULL){
		return;
	}
	if(     (strcmp(command->tokens[0], "exit") == 0) 
		|| 
		(strcmp(command->tokens[0], "quit") == 0)   ){
		exit(0);
	}
	if(strcmp(command->tokens[0], "help") == 0){
		printf("Supported Commands:\n");
		printf("\topen <image_name>\n");
		printf("\tclose <image_name>\n");
		printf("\tinfo (show information about open fs)\n");
		printf("\tstat <filename>  (show information about file)\n");
		printf("\tcd <directory_name>\n");
		printf("\tls\n");
		printf("\tget <filename> (transfer file from image to host)\n");
		printf("\tput <image_name> (transfer file from host to image)\n");
		printf("\tread <filename> <fileoffset> <num_bytes>\n");
		return;
	}
	if((strcmp(command->tokens[0], "open") == 0)){
		if(!command->tokens[1]){
			printf("Which file do you want to open?\n");
			return;
		}
		if(fs){
			printf("Error: Please close the current image first.\n");
			return;
		}
		fs = fopen(command->tokens[1], "r+");
		if(!fs){
			printf("Error: Unable to open that file.\n");
			return;
		}
		populate_fs_info();
		dir_sector=2;
		strcpy(working_dir, "ROOT");
		return;
	}
	if((strcmp(command->tokens[0], "close") == 0)){
		if(fs == NULL){
			printf("Error: No file system open...\n");
			return;
		}
		fclose(fs);
		memset(&dir,'\0',sizeof(struct DirectoryEntry)*16);
		return;
	}
	if((strcmp(command->tokens[0], "info") == 0)){
		if(!fs){
			printf("Error: Please open a filesystem image first.\n");
			return;
		}
		print_fs_info();
		return;
	}
	if((strcmp(command->tokens[0], "stat") == 0)){
		if(!fs){
			printf("Error: Please open a filesystem image first.\n");
			return;
		}
		if(command->tokens[1] == NULL){
			printf("Which file do you want to know about?\n");
			return;
		}
		print_file_info(command->tokens[1]);
		return;
	}
	if((strcmp(command->tokens[0], "get") == 0)){
		if(!fs){
			printf("Error: Please open a filesystem image first.\n");
			return;
		}
		if(command->tokens[1] == NULL){
			printf("Please specify a directory name.\n");
			return;
		}
		get(command->tokens[1]);
		return;
	}
	if((strcmp(command->tokens[0], "put") == 0)){
		if(!fs){
			printf("Error: Please open a filesystem image first.\n");
			return;
		}
		if(command->tokens[1] == NULL){
			printf("Please specify a file name.\n");
			return;
		}
		put(command->tokens[1]);
		return;

	}
	if((strcmp(command->tokens[0], "cd") == 0)){
		if(!fs){
			printf("Error: Please open a filesystem image first.\n");
			return;
		}
		if(command->tokens[1] == NULL){
			printf("Please specify a directory name.\n");
			return;
		}
		outer_change_directory(command->tokens[1]);
		return;
	}
	if((strcmp(command->tokens[0], "ls") == 0)){
		if(!fs){
			printf("Error: Please open a filesystem image first.\n");
			return;
		}
		print_directory();
		return;
	}
	if((strcmp(command->tokens[0], "read") == 0)){
		if(!fs){
			printf("Error: Please open a filesystem image first.\n");
			return;
		}
		if(!command->tokens[1] || !command->tokens[2] || !command->tokens[3]){
			printf("3 args required\n");
			return;
		}
		read_file(command->tokens[1], atoi(command->tokens[2]), atoi(command->tokens[3]));
		return;
	}
	if(successful_pid == -1){
		printf("%s: Command not found.\n", command->tokens[0]);
	}

	//insertpid(pids, successful_pid);
	return;
}

/*splits or otherwise processes command before passing into execute_command()*/
void split_command(cmd *command/*, pid_hist *pids, cmd_hist *cmds*/){
	int i;
	if(command->tokens[0] == NULL){
		return;//null command
	}

	//split
	int runner = 0;
	int split_pointer = 0;
	cmd splits[MAX_TOKENS];
	int tok_ptr = 0;
	for(i=0; i<MAX_TOKENS; i++){
		copycmd(&splits[i], command);
	}
	while(1){
		splits[split_pointer].tokens[tok_ptr] = splits[split_pointer].tokens[runner];
		if(command->tokens[runner] == NULL){
			splits[split_pointer].tokens[tok_ptr] = NULL;
			break;
		}else if(strcmp(command->tokens[runner],";")==0){
			splits[split_pointer].tokens[tok_ptr] = NULL;
			split_pointer++;
			runner++;
			tok_ptr = 0;
		}else{
			tok_ptr++;
			runner++;
		}
	}
	for(i=0; i<=split_pointer; i++){
		execute_command(&splits[i]/*, pids, cmds*/);
	}
}

int main(){
	//vars
	cmd command;
	memset(&command, 0, sizeof(cmd));
	fs = NULL;
	memset(&info, 0, sizeof(info));

	//read_info loop
	while(1){
		printf("mfs> ");
		while(NULL == fgets(command.buffer, MAX_COMMAND_LEN, stdin));
		tokenize_cmd(&command);
		split_command(&command/*, &pids, &cmds*/);
	}
}
