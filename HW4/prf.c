#include "elf64.h"
//define commonly used values
#define NOT_FOUND      (-1)
#define LOCAL_ONLY     (-2)
#define NOT_EXEC       (-3)
#define GLOBAL         (1)
#define STRTAB  ".strtab"
#define SYMTAB  ".symtab"

//a macro to read from the file without moving its pointer,useing becasue we opened the file with fopen(mimic a syscall).
#define READFILE(stream,offset,count,size,buffer) \
    fseek((stream), (offset), SEEK_SET); \
    fread((buffer), (size), (count), (stream))

//necceray libraries for the debugger
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <fcntl.h>

//struct to gather the basic necessary values of the elf
typedef struct
{
    FILE* file;//the file itself,opened with fopen to read only and in binary
    Elf64_Ehdr elfHeader;//the header of the file,used to fetch necessary data to navigate the file
    Elf64_Shdr* secHeaderTBL;//the play center of the code(before the debugging)
} ELFFile;

//struct to store metadata about the function
typedef struct
{
    Elf64_Addr address;//the address in the case if the linking of the function is static(i.e address in known before the run)
    Elf64_Addr got_addr;//the address of the got entry of the function(in the case of dynamic linking)
    bool is_Dynamic;//boolean variable(updated in the look up of the symbol) to indicate if the symbol is of a shared library function
} symbolData;

#define SAVED_RET_ADDR_SIZE (8) 
#define BK_INST (0xFFFFFFFFFFFFFF00) //cancel the last bit of the instruction
#define INSERT_BP (0xCC)// place the breakpoint instruction

//place the breakpoint in the function calling out function
#define PUT_CALLER_TRAP(child_pid, rsp_address, inst_backup) \
    do { \
        unsigned long int callee_address = ptrace(PTRACE_PEEKTEXT, child_pid, rsp_address, NULL); \
        *(inst_backup) = ptrace(PTRACE_PEEKTEXT, child_pid, callee_address, NULL); \
        ptrace(PTRACE_POKETEXT, child_pid, callee_address, (*(inst_backup) & BK_INST) | INSERT_BP); \
    } while(0)

//place the breakpoint in the function we are looking at
#define PUT_FUNC_TRAP(child_pid, function_addr, inst_backup) \
    do { \
        *(inst_backup) = ptrace(PTRACE_PEEKTEXT, child_pid, function_addr, NULL); \
        ptrace(PTRACE_POKETEXT, child_pid, function_addr, (*(inst_backup) & BK_INST) | INSERT_BP); \
    } while(0)
//place the breakpoint in the function GOT entry
#define PUT_GOT_TRAP(child_pid, inst_backup,got_addr) \
    do { \
        unsigned long int function_addr = ptrace(PTRACE_PEEKTEXT, child_pid, got_addr, NULL); \
        *(inst_backup) = ptrace(PTRACE_PEEKTEXT, child_pid, function_addr, NULL); \
        ptrace(PTRACE_POKETEXT,child_pid, function_addr, (*(inst_backup) & BK_INST) | INSERT_BP);  \
    } while (0)



//////////////////////////////////////function declarations////////////////////////////////////////////////////////
void insertElfInfo(FILE* file ,Elf64_Ehdr* elfHeader ,Elf64_Shdr** secHeaderTBL);
int  locateDynamicSymbol(ELFFile* elf_metaData, Elf64_Off dynStrtab_offset, Elf64_Shdr* dyn_sym_hdr, char* symbol_name);
Elf64_Addr  FindRelaSymAddr(ELFFile* elf_metaData, Elf64_Shdr* relHeader, int dyn_symbol_num);
bool checkSectionName(ELFFile* elf_metaData, Elf64_Shdr* sect, const char* name_to_check);
void findSymAdd(ELFFile* elf_metaData,symbolData* symbol_data, char* symbol_name ,FILE* file,Elf64_Ehdr elfHeader, Elf64_Shdr* secHeaderTBL);
int  validateInput(char* symbol_name, char* exe_file_name);
void debugChild(pid_t child_pid, symbolData* symbol_data);
void runTarget(char* argv[], symbolData* symbol_data);
//////////////////////////////////////end of function declarations/////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    symbolData symbol_data;
    char* symbol_name = argv[1]; 
    char* prog_path = argv[2]; 
    int val = validateInput(symbol_name,prog_path);
        switch (val)
        {
        case LOCAL_ONLY:
            printf("PRF:: %s is not a global symbol!\n", symbol_name);
            return 0;
        case NOT_FOUND:
            printf("PRF:: %s not found! :(\n", symbol_name);
            return 0;
        case NOT_EXEC:
            printf("PRF:: %s not an executable!\n", prog_path);
            return 0;
        default:
            break;
        }
    ELFFile* elf_metaData = calloc(1, sizeof(*elf_metaData));
    FILE* elf_file = fopen(prog_path, "r");
    elf_metaData->file = elf_file;
    insertElfInfo(elf_metaData->file,&(elf_metaData->elfHeader),&(elf_metaData->secHeaderTBL));
    
    findSymAdd( elf_metaData,&symbol_data, symbol_name,elf_metaData->file,elf_metaData->elfHeader,elf_metaData->secHeaderTBL);
    fclose(elf_metaData->file);
    free(elf_metaData->secHeaderTBL);
    runTarget(&argv[2], &symbol_data);

    return 0;
}

//if we got here the symbol exists and is global,we locate it and fill the address if staticly
//linked,otherwise fill the GOT entry
void  findSymAdd(ELFFile* elf_metaData,symbolData* symbol_data, char* symbol_name ,FILE* file,Elf64_Ehdr elfHeader, Elf64_Shdr* secHeaderTBL)
{
    Elf64_Shdr* dyn_tab;
    int dyn_symbol_num;
    Elf64_Off dynStrtab_offset;
    Elf64_Xword numOfSymbols, symtab_es;
    Elf64_Off symtab_offset,strtab_offset;
    size_t symName_len = strlen(symbol_name) + 1;
    Elf64_Sym* symTab = malloc(sizeof(*symTab));
    char* sym_name = malloc(symName_len);
//collect relative sections
    for (unsigned int index = 0; index < elfHeader.e_shnum; index++) {
        Elf64_Shdr tbl_entry = secHeaderTBL[index];
        if (tbl_entry.sh_type == 3 ) {
                if(checkSectionName(elf_metaData, &tbl_entry, ".dynstr")){
                   dynStrtab_offset = tbl_entry.sh_offset;
                }else{
                    if(checkSectionName(elf_metaData, &tbl_entry, ".strtab")){
                        strtab_offset = tbl_entry.sh_offset;
                    }
                }
        }
//the symbol table of the file
        if (tbl_entry.sh_type == 2) {
            symtab_offset = tbl_entry.sh_offset;
            symtab_es = tbl_entry.sh_entsize;
            numOfSymbols = tbl_entry.sh_size / symtab_es;
        }
    }
//check if the symbol is dynamicly linked and find its number if so
    for (unsigned int index = 0; index < elfHeader.e_shnum; index++) {
        if (secHeaderTBL[index].sh_type == 11) {
            dyn_symbol_num = locateDynamicSymbol(elf_metaData,dynStrtab_offset,&secHeaderTBL[index],symbol_name);
        }
    }
//if is dynamicly linked, locate the GOT entry and relocation info
    if (dyn_symbol_num != -1) {
        for (unsigned long int index = 0; index < elfHeader.e_shnum; index++) {
            if (secHeaderTBL[index].sh_type == 4) {
                Elf64_Addr rela_addr = FindRelaSymAddr(elf_metaData,&secHeaderTBL[index],dyn_symbol_num);
                if (rela_addr != 0) {
                    symbol_data->got_addr = rela_addr;
                    break;
                }
            }
        }
    }
//file the data in the symbol struct
    for (int index = 0; index < numOfSymbols; index++) {
        READFILE(file,symtab_offset+index*sizeof(*symTab),1,sizeof(*symTab),symTab);
        READFILE(file,strtab_offset +symTab->st_name,1,symName_len,sym_name);
        if (!strcmp(symbol_name, sym_name)) {
            if ((ELF64_ST_BIND(symTab->st_info) == GLOBAL)) {
                symbol_data->address = symTab->st_value;
                symbol_data->is_Dynamic = symTab->st_shndx == 0 ? 1:0;
            }
        }
    }
    free(symTab);
    free(sym_name);
}

/////////////////////////////////////////////////helper functions and validations/////////////////////////////////////
int  locateDynamicSymbol(ELFFile* elf_metaData, Elf64_Off dynStrtab_offset, Elf64_Shdr* dyn_sym_hdr, char* symbol_name)
{
    Elf64_Xword numOfSymbols = (dyn_sym_hdr->sh_size) ? dyn_sym_hdr->sh_size / dyn_sym_hdr->sh_entsize : 0;
        if(numOfSymbols==0){
            return -1;
        }
    Elf64_Off dyn_sym_tab_offset = dyn_sym_hdr->sh_offset;
    size_t symName_len = strlen(symbol_name) + 1;
    Elf64_Sym* dymSymTab = malloc(sizeof(*dymSymTab));
    char* sym_name = malloc(symName_len);
        
        if (sym_name == NULL) {
           free(dymSymTab);
        }
    for (int index = 0; index < numOfSymbols; index++) {
        READFILE(elf_metaData->file,dyn_sym_tab_offset +index * sizeof(*dymSymTab),1,sizeof(*dymSymTab),dymSymTab);
        READFILE(elf_metaData->file, dynStrtab_offset +dymSymTab->st_name,1,symName_len,sym_name);
        if ((strcmp(symbol_name,sym_name) == 0) && (ELF64_ST_BIND(dymSymTab->st_info) == GLOBAL) && (ELF64_ST_TYPE(dymSymTab->st_info) == 2)) 
        { 
            free(dymSymTab);
            free(sym_name);
            return (int)index;
        }
    }
    free(dymSymTab);
    free(sym_name);
    return -1;
}

Elf64_Addr  FindRelaSymAddr(ELFFile* elf_metaData, Elf64_Shdr* relHeader, int dyn_symbol_num)
{
    Elf64_Xword relNum;
    relNum =relHeader->sh_size / relHeader->sh_entsize;
            if(relNum == 0){
                return 0;
            }
    
    Elf64_Off rel_tab_offset = relHeader->sh_offset;
    Elf64_Rel* rel_entry = malloc(sizeof(*rel_entry)); 

    for (int index = 0; index < relNum; index++) {
        READFILE(elf_metaData->file,rel_tab_offset+index*relHeader->sh_entsize,1, sizeof(*rel_entry),rel_entry);
        if (ELF64_R_SYM(rel_entry->r_info) == dyn_symbol_num) {
            Elf64_Addr rela_addr = rel_entry->r_offset;
            free(rel_entry);
            return rela_addr;
        }
    }
    free(rel_entry);
    return 0;
}

bool  checkSectionName(ELFFile* elf_metaData, Elf64_Shdr* sect, const char* name_to_check)
{
    char* section_name = malloc(strlen(name_to_check) + 1);
    Elf64_Off secHeader_offset = elf_metaData->secHeaderTBL[elf_metaData->elfHeader.e_shstrndx].sh_offset;
    READFILE(elf_metaData->file,secHeader_offset+sect->sh_name,1,strlen(name_to_check)+1,section_name);
    bool equal = strcmp(name_to_check, section_name) == 0;
    free(section_name);
    return equal;
}

void  insertElfInfo(FILE* file ,Elf64_Ehdr* elfHeader ,Elf64_Shdr** secHeaderTBL)
{
    fread(elfHeader, sizeof(*elfHeader), 1, file);
    unsigned int shEntries_num = elfHeader->e_shnum;
    *secHeaderTBL = (Elf64_Shdr*)calloc(shEntries_num, sizeof(**secHeaderTBL));
    READFILE(file,elfHeader->e_shoff,shEntries_num,sizeof(**secHeaderTBL),*secHeaderTBL);
}


int validateInput(char* symbol_name, char* exe_file_name){
    int elf_fd;
    elf_fd = open (exe_file_name, O_RDONLY);      
    void *elf = NULL;
    elf = mmap (NULL, 64, PROT_READ, MAP_PRIVATE, elf_fd, 0); 

    Elf64_Ehdr* elf_header;
    elf_header = (Elf64_Ehdr*)elf; 
        if(elf_header->e_type != 2) {
            return NOT_EXEC;
        }

    off_t offset;
    offset = lseek (elf_fd, 0, SEEK_END);
    elf = mmap (NULL, offset, PROT_READ, MAP_PRIVATE, elf_fd, 0); 

    unsigned int sections_num;
    Elf64_Shdr sh_str_section, *sh_arr;
    char  *sh_str_tbl;

    sections_num = elf_header->e_shnum;// number of entries in the section header tables
    sh_arr = (Elf64_Shdr*)((char*)elf + elf_header->e_shoff);// the section header table of the elf
    sh_str_section = sh_arr[elf_header->e_shstrndx];// the header of name string table
    sh_str_tbl = (char*)elf + sh_str_section.sh_offset;// the start of the string table


    int address, target_index;
    char *strtab, *section_name, *curr_symbol_name;
    unsigned long symbols_num = 0, rel_num = 0;
    Elf64_Sym *symtab, *dynsym;
    Elf64_Rela *reltab;
    bool found_in_symtab = false;

        for (int index = 0; index < sections_num; index++) {
            section_name = sh_str_tbl + sh_arr[index].sh_name;
            if (!strcmp(section_name, STRTAB)) {
             
              if ((char *) elf + sh_arr[index].sh_offset != sh_str_tbl){
                    strtab = ((char *) elf + sh_arr[index].sh_offset);
                }
            }else if (!strcmp(section_name, SYMTAB)) {
                symbols_num = sh_arr[index].sh_size / sh_arr[index].sh_entsize;
                symtab = (Elf64_Sym *)((char*)elf + sh_arr[index].sh_offset);
            }
        }

        for (int index = 0; index < symbols_num; index++){           
            curr_symbol_name = strtab + symtab[index].st_name;
                if(!strcmp(symbol_name, curr_symbol_name)) {
                    if(ELF64_ST_BIND(symtab[index].st_info) == GLOBAL) {
                       return 1;
                    }
                }
        }

        for (int index = 0; index < symbols_num; index++){  
            curr_symbol_name = strtab + symtab[index].st_name;
                if(!strcmp(symbol_name, curr_symbol_name)) {
                     close (elf_fd);
                    return LOCAL_ONLY;    
         
                }
        }
    close (elf_fd);
    return NOT_FOUND;
}

////////////////////////////////////////////end helper functions and validations/////////////////////////////////

void  debugChild(pid_t child_pid, symbolData* symbol_data)
{   
    int wait_status;
    unsigned long int  saved_caller_stack_frame,caller_backup, callee_backup, call_counter = 1;
    struct user_regs_struct regs;
    //FILE* file = fopen("output1.txt", "a");

    wait(&wait_status);
    while (!WIFEXITED(wait_status)) {
//put a breakpoint bfore the first call of the function(if dynamic put at the GOT entry) 
        if (symbol_data->is_Dynamic == false) {
            PUT_FUNC_TRAP(child_pid, symbol_data->address, &callee_backup);
        } else {
            PUT_GOT_TRAP(child_pid,&callee_backup,symbol_data->got_addr);
        }
//allow the child to continue and wait for it to reach the breakpoint
        ptrace(PTRACE_CONT, child_pid, NULL, NULL);
        wait(&wait_status);
        if (WIFEXITED(wait_status)) {
            break;
        }
// rechead calle breakpoint
        ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
        regs.rip--;
        saved_caller_stack_frame = regs.rsp + SAVED_RET_ADDR_SIZE;
        ptrace(PTRACE_POKETEXT, child_pid, regs.rip, callee_backup);
        ptrace(PTRACE_SETREGS, child_pid, NULL, &regs);
// trap the caller it right after the call
        PUT_CALLER_TRAP(child_pid, regs.rsp, &caller_backup);
        //fprintf(file,"%d\n", (int)regs.rdi);
        printf("PRF:: run #%lu first parameter is %d\n",call_counter, (int)regs.rdi);
        ptrace(PTRACE_CONT, child_pid, NULL, NULL);
        wait(&wait_status);
// wait till the recurtion end
        do {
            ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
            regs.rip--;
            ptrace(PTRACE_POKETEXT, child_pid, regs.rip, caller_backup);
            ptrace(PTRACE_SETREGS, child_pid, NULL, &regs);
// validate the we are on the first call(upon return)
            if (saved_caller_stack_frame != regs.rsp) {
                ptrace(PTRACE_SINGLESTEP, child_pid, NULL, NULL);
                wait(&wait_status);
                PUT_FUNC_TRAP(child_pid, regs.rip, &caller_backup);
                ptrace(PTRACE_CONT, child_pid, NULL, NULL);
                wait(&wait_status);
            }
        } while (saved_caller_stack_frame != regs.rsp);
        printf("PRF:: run #%lu returned with %d\n", call_counter++, (int)regs.rax);
    }
       // fclose(file);

}

void runTarget(char* argv[], symbolData* symbol_data)
{
    pid_t child_pid;
    child_pid = fork();
    if (child_pid > 0) {//the father proccess
        debugChild(child_pid, symbol_data);
    } else if (child_pid == 0) {//child proccess
          
            if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
                exit(1);
            }
            
        execv(argv[0], argv);
    }else{
        exit(1);
    }
}
