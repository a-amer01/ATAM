.global _start

.section .text

_start:
# zero all the required reg's
       xor %rax , %rax
       xor %rbx , %rbx
       xor %rcx , %rcx
       xor %rdx , %rdx
       xor %r9  , %r9
       xor %r10 , %r10 
       xor %r11 , %r11 
# but all relative values
       movl (num) , %eax
       lea (source) , %rbx
       lea destination , %rcx
# checking the relevant case to handle
       cmpl $0 , %eax
       je END_HW1
       jg CHECKCOND1_HW1
       js NEG_NUM_HW1


CHECKCOND1_HW1:
        cmp %rcx , %rbx # 
        jg LOOP3_HW1
LOOP1_HW1:
       cmp $0 , %ax
       je END_HW1
       dec %ax
       movb (%rax,%rbx) , %r9b
       movb %r9b , (%rax,%rcx)
       jmp LOOP1_HW1



LOOP3_HW1:
       cmp %dx , %ax
       je END_HW1
       movb (%rdx,%rbx) , %r9b
       movb %r9b , (%rdx,%rcx)
       inc %dx
       jmp LOOP3_HW1


NEG_NUM_HW1:
     
       movl %eax ,destination
  
  cmpl $-211, destination
  jne END_HW1

    dec %r11

END_HW1:

    dec %r11





