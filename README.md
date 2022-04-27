## Project Overview

In this project, we study concepts of file allocation methods by diving into one of the three methods, linked allocation. To apply our knowledge, we constructed a user-space FAT32 file system that loads in a standard FAT32 disk image, allowing the user to run common file-system commands. 

## To run

Clone the repo, and type `./mfs` into your terminal.

## Commands to try

| Command | Description |
| ----------- | ----------- |
| open &lt;FAT32 image name&gt; | Opens the inputted FAT32 disk image |
| close | Closes the currently open FAT32 disk image |
| info | Prints out information about the loaded file system in both decimal and hexadecimal |
| stat &lt;file name / directory name&gt; | Prints out attributes and starting cluster number of inputted folder/directory |
| get &lt;file name&gt; | Retrieves file from FAT32 image and places it in current working directory  |
| cd &lt;directory path&gt; | Change directory  |
| ls | Lists files in current directory |
| read &lt;file name&gt; &lt;position&gt; &lt;# of bytes&gt; | Prints out n bytes of the inputted file to console from the inputted starting position |
| del &lt;file name&gt; | Deletes inputted file name from current directory |
| undel | Undeletes last deleted file |

