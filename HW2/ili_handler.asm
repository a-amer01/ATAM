.globl my_ili_handler
.extern what_to_do, old_ili_handler

.text
.align 4, 0x90
my_ili_handler:
# Save registers and allocate 8 bytes for rip displacement on stack
  subq $8, %rsp
  movq $0, (%rsp)
  push %r15
  push %rdi
  push %r14
  push %r13
  push %r12
  push %r11
  push %r10
  push %r9
  push %r8
  push %rbp
  push %rsi
  push %rdx
  push %rcx
  push %rbx
  push %rax
  xorq %rax, %rax
  xorq %rdi, %rdi
  
# Check instruction 
  movq 128(%rsp), %r12
  movb (%r12), %al
  addq $1, 120(%rsp) # Instruction is at least 1 byte; add 1 to displacement
  cmpb $0x0f, %al
  jne  handler_setup # Begin setup if instruction is 1 byte
  # Else instruction is 2 bytes, change accordingly
  movb 1(%r12), %al
  addq $1, 120(%rsp)
  
handler_setup:
# Check which handler to use
  movq $0, %rdi
  movb %al, %dil
  call what_to_do
  movq %rax, %rdi
  
# Restore registers
  popq %rax
  popq %rbx
  popq %rcx
  popq %rdx
  popq %rsi
  popq %rbp
  popq %r8
  popq %r9
  popq %r10
  popq %r11
  popq %r12
  popq %r13
  popq %r14
  
  cmpq $0, %rdi # Check if what_to_do returned 0
  je   old_handler_aux # Jump to old handler if yes
  
  movq 16(%rsp), %r15 # Move rip displacement into r15
  addq %r15, 24(%rsp) # Add diplacement to saved rip
  
# Restore Stack and Registers
  popq %r15 # r15 is popped twice because we need the new rdi value
  popq %r15 # So we ignore the rdi value saved on stack
  addq $8, %rsp
  jmp  end
  
old_handler_aux: # Execute old handler
  popq %rdi
  popq %r15
  addq $8, %rsp
  jmp *(old_ili_handler)

	
end:
  iretq
