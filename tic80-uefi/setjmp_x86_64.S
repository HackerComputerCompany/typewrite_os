/* x86_64 setjmp/longjmp — MIT licensed (Nicholas J. Kain, musl). For EFI link without libc. */
.text
.global setjmp
.global _setjmp
.global longjmp
.type setjmp, @function
.type _setjmp, @function
.type longjmp, @function

setjmp:
_setjmp:
	movq %rbx, (%rdi)
	movq %rbp, 8(%rdi)
	movq %r12, 16(%rdi)
	movq %r13, 24(%rdi)
	movq %r14, 32(%rdi)
	movq %r15, 40(%rdi)
	leaq 8(%rsp), %rdx
	movq %rdx, 48(%rdi)
	movq (%rsp), %rdx
	movq %rdx, 56(%rdi)
	xorl %eax, %eax
	ret

longjmp:
	xorl %eax, %eax
	cmpl $1, %esi
	adcl %esi, %eax
	movq (%rdi), %rbx
	movq 8(%rdi), %rbp
	movq 16(%rdi), %r12
	movq 24(%rdi), %r13
	movq 32(%rdi), %r14
	movq 40(%rdi), %r15
	movq 48(%rdi), %rsp
	jmp *56(%rdi)

.size setjmp, .-setjmp
.size _setjmp, .-_setjmp
.size longjmp, .-longjmp
