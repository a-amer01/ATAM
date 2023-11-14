.global _start

.section .text
_start:
    movq $array1, %rdi
    movq $array2, %rsi
    movq $mergedArray, %rax
    movl (%rdi), %edx
    movl (%rsi), %ecx
    

main_loop:
    cmpl $0, %edx
    je   fill2
    cmpl $0, %ecx
    je   fill1
    cmpl %edx, %ecx
    ja   fill2
    jb   fill1
    addq $4, %rdi
    movl (%rdi), %edx
    jmp  main_loop
    
fill1:
    cmpl $0, %edx
    je   end_merged
    movl %edx, (%rax)
    addq $4, %rax
skip1:
    addq $4, %rdi
    movl %edx, %r8d
    movl (%rdi), %edx
    cmpl %r8d, %edx
    je   skip1
    jmp  main_loop
    
fill2:
    cmpl $0, %ecx
    je   end_merged
    movl %ecx, (%rax)
    addq $4, %rax
skip2:
    addq $4, %rsi
    movl %ecx, %r9d
    movl (%rsi), %ecx
    cmpl %r9d, %ecx
    je   skip2
    jmp  main_loop
    
    
end_merged:
    movl $0, (%rax)
    
end:
     