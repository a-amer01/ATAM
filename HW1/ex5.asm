.global _start

.section .text
_start:
    cmp  $0 , (root)
    je   EMPTYLIST_HW1
    movq (new_node) , %rax
    movq (root) , %rbx 
    jmp CMP_HW1

EMPTYLIST_HW1:
    lea new_node , %rdx
    movq %rdx , root
    jmp END_HW1

CMP_HW1:
    cmp %rax , (%rbx)
    je END_HW1
    jg GOLEFT_HW1
    jl GORIGHT_HW1

# new_node is smaller than the current node
 GOLEFT_HW1: 
    addq  $8 , %rbx
    cmp   $0 ,  (%rbx) 
    je ADDLEFTNODE_HW1
    movq (%rbx) , %rbx
    jmp  CMP_HW1

 GORIGHT_HW1:
    addq  $16 , %rbx
    cmp   $0 ,  (%rbx) 
    je ADDRIGHTNODE_HW1
    movq (%rbx) , %rbx
    jmp  CMP_HW1

 ADDLEFTNODE_HW1:
    lea new_node , %rdx
    movq %rdx , (%rbx)
    jmp END_HW1
 
 ADDRIGHTNODE_HW1:
   lea new_node , %rdx
   movq %rdx , (%rbx)
    
 END_HW1:
    

 