.global _start

.section .text
_start:
    xor  %rdi, %rdi
    movl (Value), %edi
    movq (head), %rsi
    movq (Source) , %r10
    xor  %r8, %r8
    xor  %r9, %r9
    
    
find_node:
    cmpq $0, %rsi
    je   end
    cmpl (%rsi), %edi
    je   node
    movq %rsi, %r8
    movq 4(%rsi), %rsi
    jmp  find_node
node:
    movq %rsi, %rax


movq (head), %rsi

find_src:
    cmpq $0, %rsi
    je   end
    cmpq %rsi, %r10
    je   node_src
    movq %rsi, %r9
    movq 4(%rsi), %rsi
    jmp  find_src
node_src:
    movq %rsi, %rbx

is_val_first:
       cmp $0 , %r8
       je is_src_first
       movq %rbx , 4(%r8) # val prev next's

is_src_first: 
       cmp $0 , %r9
       je swap
       movq %rax , 4(%r9) # src prev next's
swap:
       movq 4(%rbx) , %r11 
       movq 4(%rax) , %r12
       movq %r11  , 4(%rax) # val next's
       movq %r12  , 4(%rbx) # src next's

check_src_first:
       cmp $0 , %r9
       jne check_val_first
       movq %rax , head

check_val_first:
       cmp $0 , %r8
       jne end
       movq %rbx , head

end:
       
