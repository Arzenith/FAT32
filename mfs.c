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
void open(char **);       // Function opens fat32 image file (open <filename>)
void close_f();           // Function closes fat32 image file (close)
void print_info();        // Function prints info about the loaded image file (info)
void stat(char **);       // Function prints info about specific entry in directory 
                          // (stat <filename> or <foldername>)
void get(char **);        // Function gets file from fat32 image file and saves it
                          // in users current directory (get <filename>)
void cd(char **);         // Function allows cd command in fat32 image file (cd <foldername>)
void ls();                // Function allows ls command in fat32 image file (ls)
void read_f(char **);     // Function reads entry and prints to screen 
                          // (read <filename> <starting_position> <bytes_to_read>)
void del(char **);        // Function deletes file in fat32 image file (del <filename>)
void undel();             // Function undeletes *last* file delted in fat32 image file (undel)

// Utility Functions
void load_img();          // Function loads data of fat32 image 
void load_dir();          // Function formats directory names and hides previously deleted files
int find(char **);        // Function finds file/folder in directory (returns location in directory)
int find_folder(char *);  // Function finds folder in directory (returns location in directory)
void int_to_hex(int);     // Function takes integer and prints hex value to screen
int LBAToOffset(int32_t); // Function returns the value of the address in that block of data
int16_t NextLB(int32_t);  // Function returns the value for next block address of file

Node *head = NULL;

struct DirectoryEntry dir[16];
FILE *fp = NULL;

int last_deleted_file = -1;
int last_deleted_file_loc = -1;
int last_cd_folder = -1;
int last_offset = 0x100400;

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

    // If user types in nothing
    if (token[0] == NULL)
    {
      continue;
    }

    if (!(strcmp(token[0], "quit")) || !(strcmp(token[0], "q")))
    {
      free(working_root);
      // Check if file has been opened
      if(fp != NULL)
      {
        fclose(fp);
      }
      return 0;
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
  if(token[1] == NULL)
  {
    printf("Error: Not enough arguments.\n");
    return;
  }

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
  fseek(fp, (BPB_NumFATs * BPB_FATz32 * BPB_BytsPerSec) + (BPB_RsvdSecCnt * BPB_BytsPerSec), SEEK_SET);
  fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);

  last_cd_folder = (BPB_NumFATs * BPB_FATz32 * BPB_BytsPerSec) + (BPB_RsvdSecCnt * BPB_BytsPerSec);
  printf("%x\n", LBAToOffset(dir[0].ClusterLow));

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
  if(token[1] == NULL)
  {
    printf("Error: Not enough arguments.\n");
    return;
  }

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
  if(token[1] == NULL)
  {
    printf("Error: Not enough arguments.\n");
    return;
  }

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
    // Loop and write a sector at a time
    while(size >= BPB_BytsPerSec)
    {
      fread(&buffer, BPB_BytsPerSec, 1, fp);
      fwrite(&buffer, BPB_BytsPerSec, 1, ofp);
      // NextLB returns -1 if file doesn't have a next block
      cluster = NextLB(cluster);
      // Only seek if file has a next block
      if(cluster != -1)
      {
        offset = LBAToOffset(cluster);
        fseek(fp, offset, SEEK_SET);
      }

      size -= BPB_BytsPerSec;
    }
    // Read remaining data 
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
  int offset;
  if(token[1] == NULL)
  {
    printf("Error: Not enough arguments.\n");
    return;
  }

  // Tokenize inputted folderpath and save into a linked list
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
 
  Node *temp = head;
  // last_cd_folder = LBAToOffset(dir[0].ClusterLow);
  printf("0x100400 = %X\n", LBAToOffset(dir[0].ClusterLow));
  while(temp)
  {
    i = find_folder(temp->folder_name);

    if(i == -1)
    {
      fseek(fp, last_offset, SEEK_SET);
      fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
      load_dir();
      
      printf("Error: Could not find folder.\n");
      break;
    }

    // If folderpath exists, seek to it
    offset = LBAToOffset(dir[i].ClusterLow);
    fseek(fp, offset, SEEK_SET);
    fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
    load_dir();
    
    temp = temp->next;
    
  }
  last_offset = offset;
  // Free linked list
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
    // Loop and write a sector at a time
    while(size >= BPB_BytsPerSec)
    {
      fread(&buffer, BPB_BytsPerSec, 1, fp);
      printf("%s", buffer);
      // NextLB returns -1 if file doesn't have a next block
      cluster = NextLB(cluster);
      // Only seek if file has a next block
      if(cluster != -1)
      {
        offset = LBAToOffset(cluster);
        fseek(fp, offset, SEEK_SET);
      }

      size -= BPB_BytsPerSec;
    }
    // Read remaining data 
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

void del(char **token)
{
  if(token[1] == NULL)
  {
    printf("Error: Not enough arguments.\n");
    return;
  }

  int i = find(token);
  if(i != -1)
  {
    // Save file data so we can undelete later
    last_deleted_file_loc = i;
    last_deleted_file = dir[i].DIR_Name[0];
    // 0xe5 means file is delted in FAT32 
    dir[i].DIR_Name[0] = 0xe5;
    // Hide the file
    dir[i].DIR_Attr = 4;
    return;
  }
  printf("Error: File not found.\n");
}

void undel()
{
  if(last_deleted_file != -1 || last_deleted_file_loc != -1)
  {
    // Set attribute to read only (unhide)
    dir[last_deleted_file_loc].DIR_Attr = 1;
    // Reset letter
    dir[last_deleted_file_loc].DIR_Name[0] = last_deleted_file;
    return;
  }
  printf("Error: No file to undelete.\n");
}

void load_img()
{
  // Hardcoded values of data can be found in fatspec
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
  char expanded_name[12];
  memset(expanded_name, ' ', 12);
  strncpy(expanded_name, token, strlen(token));

  // Loop through directory
  for(int i = 0; i < 16; i++)
  {
    // Format the dir folder name to lowecase so we can compare
    for(int j = 0; j < 11; j++)
    {
      expanded_name[j] = toupper(expanded_name[j]);
    }
    expanded_name[12] = '\0';

    if(!strncmp(dir[i].DIR_Name, expanded_name, 11))
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

int LBAToOffset(int32_t sector)
{
  //handles root dir quirk
  if(sector == 0)
  {
    sector = 2;
  }
  return ((sector - 2) * BPB_BytsPerSec) + (BPB_BytsPerSec * BPB_RsvdSecCnt) + (BPB_NumFATs * BPB_FATz32 * BPB_BytsPerSec);
}

int16_t NextLB(int32_t sector)
{
  int FATAddress = (BPB_BytsPerSec * BPB_RsvdSecCnt) + (sector * 4);
  int16_t value;
  fseek(fp, FATAddress, SEEK_SET);
  fread(&value, 2, 1, fp);
  return value;
}
