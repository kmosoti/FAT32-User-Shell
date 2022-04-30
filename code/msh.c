/*
	Name: Kennedy Mosoti
	ID: 1001596311

  Name: Viraj Sabhaya
  ID: 1001828871
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>

/*************************************************************************************************/

#define MAX_COMMAND_SIZE 255
#define MAX_NUM_ARGUMENTS 5
// #define MAX_HISTORY 15
// #define MAX_PID_HISTORY 15
/*************************************************************************************************/
FILE *fp = NULL;

char BS_OEMName[8]; //Offset 3 size 8
int16_t BPB_BytesPerSec; //Offset 11 size 2
int8_t BPB_SecPerClus; //Offset 13 size 1
int16_t BPB_RsvdSecCnt; //Offset 14 size 2
int8_t BPB_NumFATs; //Offset 16 size 1
int16_t BPB_RootEntCnt; //Offset 17 size 2
char BS_VolLab[11];
int32_t BPB_FATSz32; //Offset 36 size 4
int32_t BPB_RootClus; //Offset 44 size 4

int32_t RootDirSectors = 0; //RootDirSectors = 0 on FAT32 volumes
int32_t FirstDataSector = 0; //= BPB_ResvdSecCnt + (BPB_NumFATs * FATSz) + RootDirSectors; PAGE 14
int32_t FirstSectorCluster = 0; //=((N – 2) * BPB_SecPerClus) + FirstDataSector; PAGE 14

int root_offset;
char* current_file;
int current_directory;
int cluster_size;

/*
· If DIR_Name[0] == 0xE5, then the directory entry is free 
· If DIR_Name[0] == 0x00, then the directory entry is free (same as for 0xE5)
· If DIR_Name[0] == 0x05, then the actual file name character for this byte is 0xE5. Special Kanji
*/
typedef struct __attribute__((__packed__)) DirectoryEntry {
  char  DIR_Name[11]; //Offset 0 size 11
  uint8_t DIR_Attr; //Offset 11 size 1
  uint8_t Unused1[8];
  uint16_t DIR_FirstClusterHigh; //Offset 20 size 2 *High Word
  uint8_t Unused2[4];
  uint16_t DIR_FirstClusterLow; //Offset 26 size 2
  uint32_t DIR_FileSize;
} DirectoryEntry;

DirectoryEntry dir[16];
/**************************************************************************************************/
char* toLower(char* s);
void openImageFile(char* file);
void closeImageFile();
int LBATToOffset(int32_t sector);
int16_t NextLB(uint32_t sector);
void printImgInfo();
void printDirectoryInfo();
char* toFatFilename(char* file_name);
int compare_files(char* dir_entry, char* input);
void getDirectory(uint32_t sector_num);
/*************************************************************************************************/

int main(){
	
	char command_string[MAX_COMMAND_SIZE];
	while(1){
		printf("msh> ");
		
		//Waits for user input with a max size of 255
		while(!fgets(command_string, MAX_COMMAND_SIZE, stdin));
		

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
		
    //Open Command Logic
		if(!strcmp(toLower(token[0]),"open")){
      if(token[1] != NULL){
        if(strchr(token[1], ' ') == NULL){
          if(fp==NULL){
            openImageFile(token[1]);
          }
          else{
            printf("Error: System image already opened.\n");
          }
        }
        
      }
      else{
        printf("Error: System image not found.\n");
      }
    }
    //close Command Logic
    else if(!strcmp(toLower(token[0]),"close")){
      closeImageFile();
    }
    //info Command Logic
    else if(!strcmp(toLower(token[0]),"info")){
      if(fp == NULL){
        printf("Error: File system not open\n");
        continue;
      }
      else{
        printImgInfo();
      }
    }
    //stat Command Logic 
    else if (!strcmp(toLower(token[0]), "stat")){
      if (fp == NULL){
        printf("Error: File system not open\n");
      }
      else{
          toFatFilename("num.txt");
          toFatFilename("deadbeef.txt");
      }
    }
    //cd Command Logic
    else if(!strcmp(toLower(token[0]),"cd")){
      if(fp == NULL){
        printf("Error: File system not open\n");
        continue;
      }
    }
    //ls Command Logic
    else if(!strcmp(toLower(token[0]),"ls")){
      if(fp == NULL){
        printf("Error: File system not open\n");
        continue;
      }
      else{
        printDirectoryInfo();
      }
    }
    //get Command Logic
    else if(!strcmp(toLower(token[0]),"get")){
      if(fp == NULL){
        printf("Error: File system not open\n");
        continue;
      }
    }
    //read Command Logic
    else if(!strcmp(toLower(token[0]),"read")){
      if(fp == NULL){
        printf("Error: File system not open\n");
        continue;
      }
    }
    //del Command Logic
    else if(!strcmp(toLower(token[0]),"del")){
      if(fp == NULL){
        printf("Error: File system not open\n");
        continue;
      }
    }
    //undel Command Logic
    else if(!strcmp(toLower(token[0]),"undel")){
      if(fp == NULL){
        printf("Error: File system not open\n");
        continue;
      }
    } 
    else{
		 	printf("%s : Command not found\n", token[0]);
		}

		
		free(working_string);
		memset(command_string, 0, sizeof(command_string));
	}
	return 0;
}


//Used to lower the case of the commands to avoid errors in comparisons;


//Opens img file and assigns FAT32 values to global variables
void openImageFile(char* file){
  //Simply open file
  fp = fopen(file, "r");
  if(fp == NULL){
    printf("File doesn't exist in current directory\n");
    return;
  }
  else{
    /*
    char BS_OEMName[8]; //Offset 3 size 8
    int16_t BPB_BytesPerSec; //Offset 11 size 2
    int8_t BPB_SecPerClus; //Offset 13 size 1
    int16_t BPB_RsvdSecCnt; //Offset 14 size 2
    int8_t BPB_NumFATs; //Offset 16 size 1
    int16_t BPB_RootEntCnt; //Offset 17 size 2
    char BS_VolLab[11];
    int32_t BPB_FATSz32; //Offset 36 size 4
    int32_t BPB_RootClus; //Offset 44 size 4
    */
    //Following offset values to load values into variables as indicated at the top of this file
    fseek(fp, 3, SEEK_SET); //Moves three bytes into the file to grab OEMName
    fread(&BS_OEMName, 8, 1, fp);
    fseek(fp, 11, SEEK_SET);
    fread(&BPB_BytesPerSec, 2, 1, fp);
    fread(&BPB_SecPerClus, 1, 1, fp);
    fread(&BPB_RsvdSecCnt, 2, 1, fp);
    fread(&BPB_NumFATs, 1, 1, fp);
    fread(&BPB_RootEntCnt, 2, 1, fp);
    fseek(fp, 36, SEEK_SET);
    fread(&BPB_FATSz32, 4, 1, fp);
    fseek(fp, 44, SEEK_SET);
    fread(&BPB_RootClus, 4, 1, fp);

    current_directory = BPB_RootClus;
    cluster_size = (BPB_NumFATs*BPB_FATSz32*BPB_BytesPerSec)+(BPB_RsvdSecCnt*BPB_BytesPerSec);
    //Get value of data pointing to directory
    root_offset = LBATToOffset(BPB_RootClus);
    fseek(fp, root_offset, SEEK_SET);
    fread(&dir[0], 32, 16, fp);
    
    printf("File Image loaded\n");
  }
}

void closeImageFile(){
  if(fp == NULL){
    printf("Error: File system not open\n");
    return;
  }
  else{
    fclose(fp);
    //always set global variables to NULL if no longer being used
    fp = NULL;
  }
}

void printImgInfo(){
  printf("\n --Image Information (Hex | Dec)--\n");
  /*
    Printing:
    • BPB_BytesPerSec
    • BPB_SecPerClus
    • BPB_RsvdSecCnt
    • BPB_NumFATs
    • BPB_FATSz32
  */
  printf("BPB_BytesPerSec: 0x%-6X | %d\n", BPB_BytesPerSec, BPB_BytesPerSec);
  printf("BPB_SecPerClus:  0x%-6X | %d\n", BPB_SecPerClus, BPB_SecPerClus);
  printf("BPB_RsvdSecCnt:  0x%-6X | %d\n", BPB_RsvdSecCnt, BPB_RsvdSecCnt);
  printf("BPB_NumFATs:     0x%-6X | %d\n", BPB_NumFATs, BPB_NumFATs);
  printf("BPB_FATSz32:     0x%-6X | %d\n\n", BPB_FATSz32, BPB_FATSz32);
  printDirectoryInfo();
}




/*
  Auxilliary Functions
*/

//Finds starting address of a block of data given the sector number, VIA Professor Baker
int LBATToOffset(int32_t sector){
  return ((sector-2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_RsvdSecCnt) + (BPB_NumFATs * BPB_FATSz32 * BPB_BytesPerSec);
}

int16_t NextLB(uint32_t sector){
  uint32_t FATAddress = (BPB_BytesPerSec * BPB_RsvdSecCnt)+(sector * 4);
  int16_t val;
  fseek(fp, FATAddress, SEEK_SET);
  fread(&val, 2, 1, fp);
  return val;
}

//Changes string to lowercase
char* toLower(char* s){
	for(char *p = s; *p; p++) *p= tolower(*p);
	return s;
}

/*
  char  DIR_Name[11]; //Offset 0 size 11
  uint8_t DIR_Attr; //Offset 11 size 1
  uint8_t Unused1[8];
  uint8_t Unused2[4];
  uint16_t DIR_FirstClusterHigh; //Offset 20 size 2 *High Word
  uint16_t DIR_FirstClusterLow; //Offset 26 size 2
  uint32_t DIR_FileSize;
*/
void getDirectory(uint32_t sector_num){
  int offset = LBATToOffset(sector_num);
  fseek(fp, offset, SEEK_SET);
  fread(&dir[0], 32, 16, fp);
}
void printDirectoryInfo(){
  int current_directory_ptr = current_directory;
  while(current_directory_ptr!=-1 && current_directory_ptr!=0){
    getDirectory(current_directory_ptr);
    for(int i = 0; i<16; i++){
      if(dir[i].DIR_Attr == (uint16_t)0x02 || dir[i].DIR_Attr & (uint16_t)0x04
      || dir[i].DIR_Attr == (uint16_t)0x08)
      {
          continue;
      }
      if(dir[i].DIR_Name[0] == (char)0xe5) continue;
			if(dir[i].DIR_Name[0] == (char)0x00)
      { 
        break;
      }
      printf("%.11s\n", dir[i].DIR_Name);
    }
    current_directory_ptr=NextLB(current_directory_ptr);
  }

}


char* toFatFilename(char* file_name){
  int file_size = strlen(file_name);
  char temp[12];
  char* output_filename = (char*)malloc(sizeof(char)*12);
  char* delim = ".";
  strcpy(temp, file_name);
  char* token_file = strtok(temp, delim);
  char* token_ext = strtok(NULL, delim);

  if(token_ext){
    int space_count = 8;
    int token_file_length = strlen(token_file);
    space_count = space_count - token_file_length+1;
    char* space_string = (char*)malloc(sizeof(char)*space_count);;
    memset(space_string, ' ', space_count-1);

    strcpy(output_filename, token_file);
    strcat(output_filename, space_string);
    strcat(output_filename, token_ext);
  }
  for(int i = 0; i < 11; i++){
    output_filename[i] = toupper(output_filename[i]);
  }
  printf("%s\n",output_filename);
  return "Tested";
}

int compare_files(char* dir_entry, char* input){
  char compare_string[11];
  strcpy(compare_string, toFatFilename(input));
  if(strcmp(compare_string, dir_entry) == 0){
    return 1;
  }
  else{
    return 0;
  }
}

