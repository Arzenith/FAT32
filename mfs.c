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

#define MAX_NUM_ARGUMENTS 3

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
int LBAToOffset(int sector)
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
unsigned NextLB(int sector)
{
  int FATAddress = (BPB_BytsPerSec * BPB_RsvdSecCnt) + (sector * 4);
  unsigned value;
  // fseek(file, FATAddress, SEEK_SET);
  // fread(&value, 2, 1, file);
  return value;
}

void info();

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

    // Tokenize the input stringswith whitespace used as the delimiter
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

    // int token_index  = 0;
    // for( token_index = 0; token_index < token_count; token_index ++ ) 
    // {
    //   printf("token[%d] = %s\n", token_index, token[token_index] );  
    // }

    if (token[0] == NULL)
    {
      continue;
    }

    if (!(strcmp(token[0], "quit")) || !(strcmp(token[0], "exit")))
    {
      free(working_root);
      exit(0);
      return 0;
    }
    // REQUIREMENT 10: Shell supports cd comma  nd
    else if (!(strcmp(token[0], "cd")))
    {
      // Change directory function using token[1] as to grab string FOLLOWING "cd"
      chdir(token[1]);
    }
    else if (!(strcmp(token[0], "open")))
    {
      if (fp = fopen(token[1], "r")) 
      {
        printf("Opening file.\n");
      }
      else
      {
        printf("Error: File system image not found.\n");
      }
    
    }
    else if (!(strcmp(token[0], "info")))
    {
      info();
    }


    free( working_root );
  }
  return 0;
}

void info()
{
  fp = fopen("fat32.img", "r");
  fseek(fp, 11, SEEK_SET);
  fread(&BPB_BytsPerSec, 2, 1, fp);
  printf("BPB_BytsPerSec = %d\n", BPB_BytsPerSec);

  fseek(fp, 13, SEEK_SET);
  fread(&BPB_SecPerClus, 1, 1, fp);
  printf("BPB_SecPerClus = %d\n", BPB_SecPerClus);

  fseek(fp, 14, SEEK_SET);
  fread(&BPB_RsvdSecCnt, 2, 1, fp);
  printf("BPB_RsvdSecCnt = %d\n", BPB_RsvdSecCnt);

  fseek(fp, 16, SEEK_SET);
  fread(&BPB_NumFATs, 1, 1, fp);
  printf("BPB_NumFATs = %d\n", BPB_NumFATs);

  fseek(fp, 36, SEEK_SET);
  fread(&BPB_FATz32, 4, 1, fp);
  printf("BPB_FATz32 = %d\n", BPB_FATz32);
  
  //LS
  fseek(fp, 0x100400, SEEK_SET);
  fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);

  for (int i = 0; i < 16; i++)
  {
    printf("DIR[%d] = %s LOWCLUSTERNUM = %d\n", i, dir[i].DIR_Name, dir[i].ClusterLow);
  }

  //get 
  int offset = LBAToOffset(17);
  printf("Offset = %d\n", offset);
  fseek(fp, offset, SEEK_SET);

  FILE *ofp = fopen("bar.txt", "w");

  uint8_t buffer[512];

  fread(&buffer, 512, 1, fp);
  fwrite(&buffer, dir[0].size, 1, ofp);

  //cd 
  // anytime you cd if lowcluster num is 0, set to 2
  offset = LBAToOffset(6099);
  fseek(fp, offset, SEEK_SET);
  fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
  dir[0].ClusterLow = 2;

  for (int i = 0; i < 16; i++)
  {
    printf("DIR[%d] = %s LOWCLUSTERNUM = %d\n", i, dir[i].DIR_Name, dir[i].ClusterLow);
  }
  
  //read loop until nextLB = -1
  fclose(ofp);
}