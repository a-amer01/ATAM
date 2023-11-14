.global	keyMixing, byteSubstitution, shiftRows, cipher

.section	.data
sbox:	.byte	99,124,119,123,-14,107,111,-59,48,1,103,43,-2,-41,-85,118
		.byte	-54,-126,-55,125,-6,89,71,-16,-83,-44,-94,-81,-100,-92,114,-64
		.byte	-73,-3,-109,38,54,63,-9,-52,52,-91,-27,-15,113,-40,49,21
		.byte	4,-57,35,-61,24,-106,5,-102,7,18,-128,-30,-21,39,-78,117
		.byte	9,-125,44,26,27,110,90,-96,82,59,-42,-77,41,-29,47,-124
		.byte	83,-47,0,-19,32,-4,-79,91,106,-53,-66,57,74,76,88,-49
		.byte	-48,-17,-86,-5,67,77,51,-123,69,-7,2,127,80,60,-97,-88
		.byte	81,-93,64,-113,-110,-99,56,-11,-68,-74,-38,33,16,-1,-13,-46
		.byte	-51,12,19,-20,95,-105,68,23,-60,-89,126,61,100,93,25,115
		.byte	96,-127,79,-36,34,42,-112,-120,70,-18,-72,20,-34,94,11,-37
		.byte	-32,50,58,10,73,6,36,92,-62,-45,-84,98,-111,-107,-28,121
		.byte	-25,-56,55,109,-115,-43,78,-87,108,86,-12,-22,101,122,-82,8
		.byte	-70,120,37,46,28,-90,-76,-58,-24,-35,116,31,75,-67,-117,-118
		.byte	112,62,-75,102,72,3,-10,14,97,53,87,-71,-122,-63,29,-98
		.byte	-31,-8,-104,17,105,-39,-114,-108,-101,30,-121,-23,-50,85,40,-33
		.byte	-116,-95,-119,13,-65,-26,66,104,65,-103,45,15,-80,84,-69,22

.section	.text
keyMixing:
	# rdi contains the input , rsi the key
    movq (%rsi) , %r9
    xor  %r9 , (%rdi)

    addq $8 , %rsi
    addq $8 , %rdi

    movq (%rsi) , %r9
    xor  %r9 , (%rdi)

KMend:
	ret


byteSubstitution:
	    
    
        xor %r8,%r8
        xor %r9,%r9
        xor %rax,%rax
        leaq sbox(%rip) , %r8

    BS_LOOP:
        cmp $16 , %r9
        je end1
        inc %r9
        xor %rax,%rax
        movb (%rdi),%al
        movb (%r8,%rax,1),%al
        movb %al,(%rdi)
        addq $1 ,%rdi
        jmp BS_LOOP
        end1:
      
	ret 


shiftRows:
	xor %rsi , %rsi #  used as index for outer loop to do to 4 time(actually 3, first row will remain unchanged)
	xor %rdx , %rdx #  used in the inner loop as the amonut to shift the row
	xor %rcx , %rcx	#  will hold the row we want to shift, r is to init to 0, actual size is 32bits(for each row)
	xor %r9 , %r9
	xor %r10 , %r10
	
SR_OUTERLOOP:
	cmpq $4 , %rsi
	je SR_END
	movl (%rdi) , %ecx # move the current row to reg ecx ,rdi is the start of input , r10 is the row number
		
				SR_SHIFTBYTE:
					cmpq $0 , %r9
					je  PREP_NEXT_ITER
					rorl %ecx
					decq %r9
					jmp SR_SHIFTBYTE

	PREP_NEXT_ITER:
		movl %ecx , (%rdi)
		addq $4 , %rdi

		incq %rsi
		movq %rsi , %rdx 
		imul $8 , %rsi , %r9
		jmp SR_OUTERLOOP

SR_END:
	ret


cipher:
    pushq %r15
    movq %rdx, %r15
    
cipher_loop:
   
    pushq %rdi
    pushq %rsi
    pushq %rdx
    
    call keyMixing
    
    popq %rdx
    popq %rsi
    popq %rdi
    
    xorq %rax, %rax
    
loop9:

    pushq %rax    
    pushq %rdi
    pushq %rsi
    pushq %rdx
    
    call byteSubstitution
    
    popq %rdx
    popq %rsi
    popq %rdi
    
    pushq %rdi
    pushq %rsi
    pushq %rdx
    
    call shiftRows
    
    popq %rdx
    popq %rsi
    popq %rdi
    
    pushq %rdi
    pushq %rsi
    pushq %rdx
    
    call mixColumns
    
    popq %rdx
    popq %rsi
    popq %rdi
    
    pushq %rdi
    pushq %rsi
    pushq %rdx
    
    call keyMixing
    
    popq %rdx
    popq %rsi
    popq %rdi
    
    popq %rax
    incq %rax
    cmpq $9, %rax
    jne loop9
    
    
    pushq %rdi
    pushq %rsi
    pushq %rdx
    
    call byteSubstitution
    
    popq %rdx
    popq %rsi
    popq %rdi
    
    pushq %rdi
    pushq %rsi
    pushq %rdx
    
    call shiftRows
    
    popq %rdx
    popq %rsi
    popq %rdi
    
    pushq %rdi
    pushq %rsi
    pushq %rdx
    
    call keyMixing
    
    popq %rdx
    popq %rsi
    popq %rdi
    
    addq $16, %rdi
    decq %r15
    cmpq $0, %r15
    jne cipher_loop
    
    popq %r15
    ret
