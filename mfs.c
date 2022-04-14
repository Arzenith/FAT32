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

typedef struct node Node;
struct node 
{
  char *folder_name;
  Node* next;
};

// Main Functions
void open(char **token);
void close_f();
void print_info();
void stat(char **token);
void get(char **token);
void cd(char **token);
void ls();
void del();
void undel();

// Utility Functions
void load_img();
void load_dir();
void read_f(char **token);
int find(char **token);
int find_folder(char *token);
void int_to_hex(int num);
int LBAToOffset(int32_t sector);
int16_t NextLB(int32_t sector);

Node *head = NULL;

struct DirectoryEntry dir[16];
FILE *fp = NULL;

int last_deleted_file = -1;
int last_deleted_file_loc = -1;

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
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    if (token[0] == NULL)
    {
      continue;
    }

    if (!(strcmp(token[0], "quit")) || !(strcmp(token[0], "q")))
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
      stat(token);
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
      del(token);
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

void stat(char **token)
{
  int i = find(token);
  if(i != -1)
  {
    printf("File Attribute\t\tSize\t\tStarting Cluster Number\n%d\t\t\t%d\t\t%d\n", dir[i].DIR_Attr, dir[i].size, dir[i].ClusterLow);
    return;
  }
  printf("Error: File not found.\n");
}

void get(char **token)
{
  // Save input becuase find_file messes with token
  char saved_input[strlen(token[1])];
  strcpy(saved_input, token[1]);

  int i = find(token);
  if(i != -1)
  {
    FILE *ofp = fopen(saved_input, "w");
    uint8_t buffer[BPB_BytsPerSec];

    int offset = LBAToOffset(dir[i].ClusterLow);
    fseek(fp, offset, SEEK_SET);

    int cluster = dir[i].ClusterLow;
    int size = dir[i].size;
    while(size >= BPB_BytsPerSec)
    {
      fread(&buffer, BPB_BytsPerSec, 1, fp);
      fwrite(&buffer, BPB_BytsPerSec, 1, ofp);
      cluster = NextLB(cluster);
      if(cluster != -1)
      {
        offset = LBAToOffset(cluster);
        fseek(fp, offset, SEEK_SET);
      }
      size -= BPB_BytsPerSec;
    }
    if(size > 0)
    {
      fread(&buffer, size, 1, fp);
      fwrite(&buffer, size, 1, ofp);
    }

    fclose(ofp);
    return;
  }
  printf("Error: File not found.\n");
}

void cd(char **token)
{
  // Tokenize inputted folderpath
  char *array[100];
  int token_count = 0;

  array[token_count] = strtok(token[1], "/");

  while(array[token_count] != NULL)
  {
    // Make a new node with each folder name
    Node *new_node = (Node *)malloc(sizeof(Node));
    new_node->folder_name = malloc(sizeof(char) * sizeof(strlen(array[token_count])));
    strcpy(new_node->folder_name, array[token_count]);
    new_node->next = NULL;

    // If linked list doesn't exist yet, set head to new_node
    if(head == NULL)
    {
      head = new_node;
    }
    // Add to end of list
    else
    {
      Node *temp = head;
      while (temp->next != NULL)
      {
        temp = temp->next;
      }
      temp->next = new_node;
    }

    token_count++;
    array[token_count] = strtok(NULL, "/");
  }

  int i;
  int offset;
  Node *temp = head;
  while(temp)
  {
    i = find_folder(temp->folder_name);

    if(i == -1)
    {
      printf("Error: Could not find folder.\n");
      break;
    }

    offset = LBAToOffset(dir[i].ClusterLow);
    fseek(fp, offset, SEEK_SET);
    fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
    load_dir();

    temp = temp->next;
  }

  temp = head;
  while (head != NULL)
  {
    temp = head;
    head = head->next;
    free(temp->folder_name);
    free(temp);
  }
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
    if(!(strcmp(temp, "0x01")) || !(strcmp(temp, "0x10")) || !(strcmp(temp, "0x20") && dir[i].DIR_Name[0] != 229) && dir[i].DIR_Name[0] != -27)
    {
      printf("%s\n", dir[i].DIR_Name);
    } 
  }
}

void del(char **token)
{
  int i = find(token);
  if(i != -1)
  {
    last_deleted_file_loc = i;
    last_deleted_file = dir[i].DIR_Name[0];
    dir[i].DIR_Name[0] = 0xe5;
    dir[i].DIR_Attr = 4;
    return;
  }
  printf("Error: File not found.\n");
}

void undel()
{
  if(last_deleted_file != -1 || last_deleted_file_loc != -1)
  {
    dir[last_deleted_file_loc].DIR_Attr = 1;
    dir[last_deleted_file_loc].DIR_Name[0] = last_deleted_file;
    return;
  }
  printf("Error: No file to undelete.\n");
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

    // If file is deleted, hide it
    if(dir[i].DIR_Name[0] == -27)
    {
      dir[i].DIR_Attr = 4;
    }
  }
}

void read_f(char **token)
{
  if(token[1] == NULL || token[2] == NULL || token[3] == NULL)
  {
    printf("Error: Not enough arguments.\n");
    return;
  }
  
  int position = atoi(token[2]);
  int num_bytes = atoi(token[3]);

  int i = find(token);
  if(i != -1)
  {
    uint8_t buffer[BPB_BytsPerSec];

    int offset = LBAToOffset(dir[i].ClusterLow);
    fseek(fp, offset + position, SEEK_SET);

    int cluster = dir[i].ClusterLow;
    int size = num_bytes;
    while(size >= BPB_BytsPerSec)
    {
      fread(&buffer, BPB_BytsPerSec, 1, fp);
      printf("%s", buffer);
      cluster = NextLB(cluster);
      if(cluster != -1)
      {
        offset = LBAToOffset(cluster);
        fseek(fp, offset, SEEK_SET);
      }

      size -= BPB_BytsPerSec;
    }
    if(size > 0)
    {
      fread(&buffer, size, 1, fp);
      printf("%s", buffer);
    }
    printf("\n");
    return;
  }
  printf("Error: File not found.\n");
}

int find(char **token)
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
    if(!strcmp(trimmed_input, trimmed_file) && (!(strcmp(temp, "0x01")) || !(strcmp(temp, "0x10")) || !(strcmp(temp, "0x20"))))
    {
      return i;
    }
  }
  return -1;
}

int find_folder(char *token)
{
  // Make two temp strings, format them similarly, and compare to find folder
  // Make temp string with everything up to file extention
  char *trimmed_input = strtok(token, "/");
  // Make another string with the length of the input (+1 for '\0')
  char trimmed_folder[strlen(trimmed_input)];

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