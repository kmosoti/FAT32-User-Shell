# FAT32-User-Shell
A user space shell application that can interpret a FAT32 system image.

# Compile
To compile you will simply run `g++ msh.c -o msh` or `gcc msh.c -o msh`

# Starting the shell
* simply type `./msh` to start the program and run the command `open <filename>` where filename is the FAT32 image

# Supported Commands (Some WIP)
#### open "filename" EXAMPLE: `open fat32.img`
>This command shall open a fat32 image. Filenames of fat32 images shall not contain spaces and shall be limited to 100 characters. If the file is not found your program shall output: “Error: File system image not found.”. If a file
system is already opened then your program shall output: “Error: File system image already
open.”.
#### close
> This command shall close the fat32 image. If the file system is not currently open your program shall output: “Error: File system not open.” Any command issued after a close, except for open, shall result in “Error: File system image must be opened first.”
#### info
> This command shall print out information about the file system in both hexadecimal and base 10
#### stat "filename" or "direcory name" EXAMPLE: `stat FOO.txt`
> This command shall print the attributes and starting cluster number of the file or directory name.
If the parameter is a directory name then the size shall be 0. If the file or directory does not exist
then your program shall output “Error: File not found”.
#### cd "filename"
> This command shall change the current working directory to the given directory. Your program
shall support relative paths, e.g cd ../name and absolute paths.
#### ls
>Lists the directory contents. Your program shall support listing “.” and “..” . Your program shall
not list deleted files or system volume names.
#### get "filename"
>This command shall retrieve the file from the FAT 32 image and place it in your current working
directory. If the file or directory does not exist then your program shall output “Error: File not
found”.
#### read "filename" "position" "number of bytes" EXAMPLE: `read FOO.txt 10 4`
> Reads from the given file at the position, in bytes, specified by the position parameter and output
the number of bytes specified.
#### del "filename"
> Deletes the file from the file system
#### undel "filename"
> Un-deletes the file from the file system
