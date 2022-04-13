// Patrick Arzoumanian
// Nghia Lam

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <ctype.h>

#define MAX_NUM_ARGUMENTS 4

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

int16_t BPB_BytsPerSec;
int8_t BPB_SecPerClus;
int16_t BPB_RsvdSecCnt;
int8_t BPB_NumFATs;
int32_t BPB_FATz32;

struct __attribute__((__packed__)) DirectoryEntry
{
  char DIR_Name[11];
  uint8_t DIR_Attr;
  uint8_t unused[8];
  uint16_t ClusterHigh;
  uint8_t unused2[4];
  uint16_t ClusterLow;
  uint32_t size;
};

struct DirectoryEntry dir[16];

FILE *fp = NULL;

/*
  LBAToOffset function returns the value of the address in that block of data
*/
int LBAToOffset(int32_t sector)
{
  //handles root dir quirk
  if(sector == 0)
  {
    sector = 2;
  }
  return ((sector - 2) * BPB_BytsPerSec) + (BPB_BytsPerSec * BPB_RsvdSecCnt) + (BPB_NumFATs * BPB_FATz32 * BPB_BytsPerSec);
}

/*
  NextLB returns unsigned interger for next block address of file
*/
int16_t NextLB(int32_t sector)
{
  int FATAddress = (BPB_BytsPerSec * BPB_RsvdSecCnt) + (sector * 4);
  int16_t value;
  fseek(fp, FATAddress, SEEK_SET);
  fread(&value, 2, 1, fp);
  return value;
}

void int_to_hex(int num);
void open(char **token);
void close_f();
void print_info();
void cd(char **token);
void ls();
void read_f(char **token);
void del();
void undel();
void load_dir();
void load_img();
void get(char **token);

int find_file(char **token);
int find_folder(char **token);

int main()
{
  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

  while( 1 )
  {
    // Print out the mfs prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;                                 
                                                           
    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;                                         
                                                           
    char *working_str  = strdup( cmd_str );                

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input strings with whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    // Now print the tokenized input as a debug check
    // \TODO Remove this code and replace with your FAT32 functionality

    int token_index  = 0;
    for( token_index = 0; token_index < token_count; token_index ++ ) 
    {
      printf("token[%d] = %s\n", token_index, token[token_index] );  
    }

    if (token[0] == NULL)
    {
      continue;
    }

    if (!(strcmp(token[0], "quit")) || !(strcmp(token[0], "exit")))
    {
      free(working_root);
      fclose(fp);
      return 0;
    }
    // Used for debugging, Just type "o", hard coded val so we can debug faster
    else if (!(strcmp(token[0], "o")))
    {
      fp = fopen("fat32.img", "r");

      // Fseek to root dir, now that they've opened the image
      fseek(fp, 0x100400, SEEK_SET);
      fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);

      load_img();
      load_dir();
    }
    // Any command issued after a close, except for open shall result in "Error: File system image must be opened first"
    else if((strcmp(token[0], "open")) && fp == NULL)
    {
      printf("Error: File system image must be opened first.\n");
    }
    else if (!(strcmp(token[0], "open")))
    {
      open(token);
    }
    else if (!(strcmp(token[0], "close")))
    {
      close_f();
    }
    else if (!(strcmp(token[0], "info")))
    {
      print_info();
    }
    else if (!(strcmp(token[0], "stat")))
    {
      // ONLY SUPPORTS FILES AT THE MOMENT
      int i = find_file(token);
      if(i != -1)
      {
        printf("File Attribute\t\tSize\t\tStarting Cluster Number\n%d\t\t\t%d\t\t%d\n", dir[i].DIR_Attr, dir[i].size, dir[i].ClusterLow);
        continue;
      }
      printf("Error: File not found.\n");
    }
    else if (!(strcmp(token[0], "get")))
    {
      get(token);
    }
    else if (!(strcmp(token[0], "cd")))
    {
      cd(token);
    }
    else if (!(strcmp(token[0], "ls")))
    {
      ls();
    }
    else if (!(strcmp(token[0], "read")))
    {
      read_f(token);
    }
    else if (!(strcmp(token[0], "del")))
    {
      del();
    }
    else if (!(strcmp(token[0], "undel")))
    {
      undel();
    }
    else
    {
      printf("Error: Command not found.\n");
    }
    
    free( working_root );
  }

  fclose(fp);
  return 0;
}

void int_to_hex(int num)
{
  char reversedDigits[100];
	int i = 0;
	
	while(num > 0) {
		
		int remain = num % 16;
		
		if(remain < 10)
			reversedDigits[i] = '0' + remain;
		else
			reversedDigits[i] = 'A' + (remain - 10);
		
		num = num / 16;
		i++;
	}
	
  printf("0x");
	while(i--) {
		printf("%c", reversedDigits[i]);
	}
}

void open(char **token)
{
  if(fp != NULL)
  {
    printf("Error: File system image already open.\n");
  }
  else if (fp = fopen(token[1], "r")) 
  {
    printf("Opening file.\n");
  }
  else
  {
    printf("Error: File system image not found.\n");
  }

  load_img();

  // Fseek to root dir, now that they've opened the image
  fseek(fp, 0x100400, SEEK_SET);
  fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
  load_dir();

}

void close_f()
{
  if(fp == NULL)
  {
    printf("Error: File system not open.\n");
  }
  else
  {
    fclose(fp);
    fp = NULL;
  }
}

void load_img()
{
  fseek(fp, 11, SEEK_SET);
  fread(&BPB_BytsPerSec, 2, 1, fp);
  fseek(fp, 13, SEEK_SET);
  fread(&BPB_SecPerClus, 1, 1, fp);
  fseek(fp, 14, SEEK_SET);
  fread(&BPB_RsvdSecCnt, 2, 1, fp);
  fseek(fp, 16, SEEK_SET);
  fread(&BPB_NumFATs, 1, 1, fp);
  fseek(fp, 36, SEEK_SET);
  fread(&BPB_FATz32, 4, 1, fp);
}

// Function loads in global data of .img file
void load_dir()
{
  for (int i = 0; i < 16; i++)
  {
    // NULL terminate the string to remove the garbage
    dir[i].DIR_Name[12] = '\0';
  }
}

void print_info()
{
  printf("BPB_BytsPerSec = %-5d\t", BPB_BytsPerSec);
  int_to_hex(BPB_BytsPerSec);

  printf("\nBPB_SecPerClus = %-5d\t", BPB_SecPerClus);
  int_to_hex(BPB_SecPerClus);

  printf("\nBPB_RsvdSecCnt = %-5d\t", BPB_RsvdSecCnt);
  int_to_hex(BPB_RsvdSecCnt);

  printf("\nBPB_NumFATs    = %-5d\t", BPB_NumFATs);
  int_to_hex(BPB_NumFATs);

  printf("\nBPB_FATz32     = %-5d\t", BPB_FATz32);
  int_to_hex(BPB_FATz32);

  printf("\n");
}

void cd(char **token)
{
  // only supports cding back a directorie at the moment, not forward
  int i = find_folder(token);
  if(i != -1)
  {
    int offset = LBAToOffset(dir[i].ClusterLow);
    fseek(fp, offset, SEEK_SET);
    fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
    load_dir();
    return;
  }
  printf("Error: Folder not found.\n");
}

void ls()
{
  char temp[10];
  // Loop through directory...
  for (int i = 0; i < 16; i++)
  {
    // Save the file's attribute as a hex value into a string
    sprintf(temp, "0x%02X", dir[i].DIR_Attr);

    // Only print files with attributes "0x01", "0x10", "0x20"
    if(!(strcmp(temp, "0x01")) || !(strcmp(temp, "0x10")) || !(strcmp(temp, "0x20")))
    {
      printf("%s\n", dir[i].DIR_Name);
    }
  }
}

void get(char **token)
{
  // Save input becuase find_file messes with token
  char saved_input[strlen(token[1])];
  strcpy(saved_input, token[1]);

  int i = find_file(token);
  if(i != -1)
  {
    int offset = LBAToOffset(dir[i].ClusterLow);

    FILE *ofp = fopen(saved_input, "w");

    uint8_t buffer[dir[i].size];

    fseek(fp, offset, SEEK_SET);
    fread(&buffer, dir[i].size + 1, 1, fp);
    fwrite(&buffer, dir[i].size + 1, 1, ofp);

    fclose(ofp);
    return;
  }
  printf("Error: File not found.\n");
}

void read_f(char **token)
{
  if(token[1] == NULL || token[2] == NULL || token[3] == NULL)
  {
    printf("Error: Not enough arguments.\n");
    return;
  }
  
  char saved_input[strlen(token[1])];
  strcpy(saved_input, token[1]);
  int position = atoi(token[2]);
  int num_bytes = atoi(token[3]);

  int i = find_file(token);
  if(i != -1)
  {
    int offset = LBAToOffset(dir[i].ClusterLow);
    int num_sectors = dir[i].size / 512;
    uint8_t buffer[num_bytes];
    fseek(fp, offset + position, SEEK_SET);
    fread(&buffer, num_bytes + 1, 1, fp);

    printf("%s\n", buffer);
    return;
  }
  printf("Error: File not found.\n");
}

int find_file(char **token)
{
  // Make two temp strings, format them similarly, and compare to find file
  // Make temp string with everything up to file extention
  char *trimmed_input = strtok(token[1], ".");
  // Make another string with the length of the input (+1 for '\0')
  char trimmed_file[strlen(trimmed_input) + 1];

  char temp[10];
  // Loop through directory
  for(int i = 0; i < 16; i++)
  {
    // Format the dir file name to lowecase so we can compare
    for(int j = 0; j < strlen(trimmed_input); j++)
    {
      trimmed_file[j] = tolower(dir[i].DIR_Name[j]);
    }
    trimmed_file[strlen(trimmed_input)] = '\0';
    
    // Save the file's attribute as a hex value into a string
    sprintf(temp, "0x%02X", dir[i].DIR_Attr);

    // If the it's a file (0x01 and 0x20) and the file names match 
    if(!strcmp(trimmed_input, trimmed_file) && (!(strcmp(temp, "0x01")) || !(strcmp(temp, "0x20"))))
    {
      return i;
    }
  }
  return -1;
}

int find_folder(char **token)
{
  // Make two temp strings, format them similarly, and compare to find folder
  // Make temp string with everything up to file extention
  char *trimmed_input = strtok(token[1], "/");
  // Make another string with the length of the input (+1 for '\0')
  char trimmed_folder[strlen(trimmed_input) + 1];

  char temp[10];
  // Loop through directory
  for(int i = 0; i < 16; i++)
  {
    // Format the dir folder name to lowecase so we can compare
    for(int j = 0; j < strlen(trimmed_input); j++)
    {
      trimmed_folder[j] = tolower(dir[i].DIR_Name[j]);
    }
    trimmed_folder[strlen(trimmed_input)] = '\0';
    
    // Save the folder's attribute as a hex value into a string
    sprintf(temp, "0x%02X", dir[i].DIR_Attr);

    // If the it's a folder (0x10 and the folder names match 
    if(!strcmp(trimmed_input, trimmed_folder) && !(strcmp(temp, "0x10")))
    {
      return i;
    }
  }
  return -1;
}


void del(){}
void undel(){}

//get 
// int offset = LBAToOffset(17);
// printf("Offset = %d\n", offset);
// fseek(fp, offset, SEEK_SET);

// FILE *ofp = fopen("bar.txt", "w");

// uint8_t buffer[512];

// fread(&buffer, 512, 1, fp);
// fwrite(&buffer, dir[0].size, 1, ofp);

//cd 
// anytime you cd if lowcluster num is 0, set to 2
// offset = LBAToOffset(6099);
// fseek(fp, offset, SEEK_SET);
// fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
// dir[0].ClusterLow = 2;

// for (int i = 0; i < 16; i++)
// {
//   printf("DIR[%d] = %s LOWCLUSTERNUM = %d\n", i, dir[i].DIR_Name, dir[i].ClusterLow);
// }

//read loop until nextLB = -1
// fclose(ofp);