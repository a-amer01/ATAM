#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <syscall.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "elf64.h"

#define	ET_NONE	0	//No file type 
#define	ET_REL	1	//Relocatable file 
#define	ET_EXEC	2	//Executable file 
#define	ET_DYN	3	//Shared object file 
#define	ET_CORE	4	//Core file

#define FOUND          (1)
#define NOT_FOUND      (-1)
#define LOCAL_ONLY     (-2)
#define NOT_EXEC       (-3)
#define GLOB_NOT_FOUND (-4)

#define ERROR_RET 0

#define GLOBAL  1
#define ET_EXEC 2
#define STRTAB  ".strtab"
#define SYMTAB  ".symtab"
#define REL_PLT ".rela.plt"
#define DYNSYM  ".dynsym"
#define DYNSTR  ".dynstr"


int fileno(FILE *stream);

/* symbol_name		- The symbol (maybe function) we need to search for.
 * exe_file_name	- The file where we search the symbol in.
 * error_val		- If  1: A global symbol was found, and defined in the given executable.
 * 			- If -1: Symbol not found.
 *			- If -2: Only a local symbol was found.
 * 			- If -3: File is not an executable.
 * 			- If -4: The symbol was found, it is global, but it is not defined in the executable.
 * return value		- The address which the symbol_name will be loaded to, if the symbol was found and is global.
 */
unsigned long find_symbol(char* symbol_name, char* exe_file_name, int* error_val) {

// open the passed file, might need to change reading to binary format
    FILE *file = fopen (exe_file_name, "r");
        
        if (!file) {
            *error_val = NOT_EXEC;
            return ERROR_RET;
        }

    int elf_fd;

    elf_fd = open (exe_file_name, O_RDONLY);
       
        if (elf_fd == -1) {
            *error_val = NOT_EXEC;
            return ERROR_RET;
        }
    
    // off_t offset;
    // offset = lseek (elf_fd, 0, SEEK_END);//offset is the size of the file

// read the header of the elf(size 64 bytes) and check if the file type is exe
    void *elf = NULL;
    elf = mmap (NULL, 64, PROT_READ, MAP_PRIVATE, elf_fd, 0); 

    Elf64_Ehdr* elf_header;
    elf_header = (Elf64_Ehdr*)elf;
        
        if(elf_header->e_type != ET_EXEC) {
            *error_val = NOT_EXEC;
            return ERROR_RET;
        }

//  set the variable elf to be the whole elf file
    off_t offset;
    offset = lseek (elf_fd, 0, SEEK_END);
    elf = mmap (NULL, offset, PROT_READ, MAP_PRIVATE, elf_fd, 0); 

//
    Elf64_Half sections_num;
    Elf64_Shdr sh_str_section, *sh_arr;
    char  *sh_str_tbl;

    sections_num = elf_header->e_shnum;// number of entries in the section header tables
    sh_arr = (Elf64_Shdr*)((char*)elf + elf_header->e_shoff);// the section header table of the elf
    sh_str_section = sh_arr[elf_header->e_shstrndx];// the header of name string table
    sh_str_tbl = (char*)elf + sh_str_section.sh_offset;// the start of the string table


    int address, target_index;
    char *strtab, *dynstr, *section_name, *curr_symbol_name;
    unsigned long symbols_num = 0, rel_num = 0;
    Elf64_Sym *symtab, *dynsym;
    Elf64_Rela *reltab;
    bool found_in_symtab = false;

        for (int i = 0; i < sections_num; i++) {
            section_name = sh_str_tbl + sh_arr[i].sh_name;
            if (!strcmp(section_name, STRTAB)) {
             
              if ((char *) elf + sh_arr[i].sh_offset != sh_str_tbl){
                    strtab = ((char *) elf + sh_arr[i].sh_offset);
                }
            }
            else if (!strcmp(section_name, DYNSTR)) {
                
                if ((char *) elf + sh_arr[i].sh_offset != sh_str_tbl){
                    dynstr = ((char *) elf + sh_arr[i].sh_offset);
                }
            }
            else if (!strcmp(section_name, REL_PLT)) {

                rel_num = sh_arr[i].sh_size / sh_arr[i].sh_entsize;
                reltab = (Elf64_Rela *)((char*)elf + sh_arr[i].sh_offset);
            }
            else if (!strcmp(section_name, DYNSYM)) {

                dynsym = (Elf64_Sym *)((char*)elf + sh_arr[i].sh_offset);
            }
            else if (!strcmp(section_name, SYMTAB)) {

                symbols_num = sh_arr[i].sh_size / sh_arr[i].sh_entsize;
                symtab = (Elf64_Sym *)((char*)elf + sh_arr[i].sh_offset);
            }
        }


        for (int i = 0; i < symbols_num; i++){
           
            curr_symbol_name = strtab + symtab[i].st_name;
                
                if(!strcmp(symbol_name, curr_symbol_name)) {
                   
                    if(ELF64_ST_BIND(symtab[i].st_info) == GLOBAL) {
                        if(symtab[i].st_value){
                            address = (int) symtab[i].st_value;
                            close(elf_fd);
                            *error_val = FOUND;
                            return address;
                        }else{
                             *error_val = GLOB_NOT_FOUND;
                             close(elf_fd);
                             return ERROR_RET;
                        }
                    }
                }
        }

        for (int i = 0; i < symbols_num; i++){
            
            curr_symbol_name = strtab + symtab[i].st_name;
                
                if(!strcmp(symbol_name, curr_symbol_name)) {
                     close (elf_fd);
                    *error_val = LOCAL_ONLY;
                    return ERROR_RET;    
         
                }
        }


    close (elf_fd);
    *error_val = NOT_FOUND;
    return ERROR_RET;
}

int main(int argc, char *const argv[]) {
	int err = 0;
	unsigned long addr = find_symbol(argv[1], argv[2], &err);

	if (addr > 0)
		printf("%s will be loaded to 0x%lx\n", argv[1], addr);
	else if (err == -2)
		printf("%s is not a global symbol! :(\n", argv[1]);
	else if (err == -1)
		printf("%s not found!\n", argv[1]);
	else if (err == -3)
		printf("%s not an executable! :(\n", argv[2]);
	else if (err == -4)
		printf("%s is a global symbol, but will come from a shared library\n", argv[1]);
	return 0;
}