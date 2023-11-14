.global _start

.section .text
_start:
    movq %rsp, %rbp #for correct debuggingd
    xor %rax,%rax
    movq num, %rax
    xor %rbx ,%rbx

loop:
    cmpq $0 , %rax
    je end
    mov %rax , %rcx
    and $1 , %rcx
    jz CONT
    inc %rbx
CONT:
    shr %rax
    jmp loop

end:
 movb %bl , Bool

