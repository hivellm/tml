	.def	@feat.00;
	.scl	3;
	.type	0;
	.endef
	.globl	@feat.00
@feat.00 = 0
	.file	"bench_arithmetic.ll"
	.def	tml_main;
	.scl	2;
	.type	32;
	.endef
	.text
	.globl	tml_main                        # -- Begin function tml_main
	.p2align	4
tml_main:                               # @tml_main
# %bb.0:                                # %entry
	pushq	%r14
	pushq	%rsi
	pushq	%rdi
	pushq	%rbp
	pushq	%rbx
	subq	$32, %rsp
	leaq	.L.str.0(%rip), %rcx
	callq	puts
	movl	$10, %ecx
	callq	putchar
	callq	tml_instant_now
	movq	%rax, %rsi
	movl	$1783293664, %ecx               # imm = 0x6A4AE6E0
	callq	tml_black_box_i32
	movl	$1783293664, %ecx               # imm = 0x6A4AE6E0
	callq	tml_black_box_i32
	movl	$1783293664, %ecx               # imm = 0x6A4AE6E0
	callq	tml_black_box_i32
	movq	%rsi, %rcx
	callq	tml_instant_elapsed
	movq	%rax, %rcx
	callq	tml_duration_format_ms
	movq	%rax, %rsi
	leaq	.L.fmt.str.no_nl(%rip), %rdi
	leaq	.L.str.2(%rip), %rdx
	movq	%rdi, %rcx
	callq	printf
	movq	%rdi, %rcx
	movq	%rsi, %rdx
	callq	printf
	leaq	.L.str.3(%rip), %rdx
	movq	%rdi, %rcx
	callq	printf
	movl	$10, %ecx
	callq	putchar
	movl	$1, %edi
	movl	$4, %ebx
	callq	tml_instant_now
	movq	%rax, %rsi
	.p2align	4
.LBB0_1:                                # %if.end11.i
                                        # =>This Inner Loop Header: Depth=1
	leal	-3(%rbx), %eax
	imull	%edi, %eax
	cltq
	imulq	$1152921497, %rax, %rcx         # imm = 0x44B82F99
	movq	%rcx, %rdx
	shrq	$63, %rdx
	sarq	$60, %rcx
	addl	%edx, %ecx
	imull	$1000000007, %ecx, %ecx         # imm = 0x3B9ACA07
	subl	%ecx, %eax
	leal	-2(%rbx), %ecx
	imull	%eax, %ecx
	movslq	%ecx, %rax
	imulq	$1152921497, %rax, %rcx         # imm = 0x44B82F99
	movq	%rcx, %rdx
	shrq	$63, %rdx
	sarq	$60, %rcx
	addl	%edx, %ecx
	imull	$1000000007, %ecx, %ecx         # imm = 0x3B9ACA07
	subl	%ecx, %eax
	leal	-1(%rbx), %ecx
	imull	%eax, %ecx
	movslq	%ecx, %rax
	imulq	$1152921497, %rax, %rcx         # imm = 0x44B82F99
	movq	%rcx, %rdx
	shrq	$63, %rdx
	sarq	$60, %rcx
	addl	%edx, %ecx
	imull	$1000000007, %ecx, %ecx         # imm = 0x3B9ACA07
	subl	%ecx, %eax
	imull	%ebx, %eax
	movslq	%eax, %rdi
	imulq	$1152921497, %rdi, %rax         # imm = 0x44B82F99
	movq	%rax, %rcx
	shrq	$63, %rcx
	sarq	$60, %rax
	addl	%ecx, %eax
	imull	$1000000007, %eax, %eax         # imm = 0x3B9ACA07
	subl	%eax, %edi
	addl	$4, %ebx
	cmpl	$100004, %ebx                   # imm = 0x186A4
	jne	.LBB0_1
# %bb.2:                                # %tml_bench_int_mul.exit
	movl	%edi, %ecx
	callq	tml_black_box_i32
	movl	$1, %ecx
	movl	$4, %eax
	.p2align	4
.LBB0_3:                                # %if.end11.i2
                                        # =>This Inner Loop Header: Depth=1
	leal	-3(%rax), %edx
	imull	%ecx, %edx
	movslq	%edx, %rcx
	imulq	$1152921497, %rcx, %rdx         # imm = 0x44B82F99
	movq	%rdx, %r8
	shrq	$63, %r8
	sarq	$60, %rdx
	addl	%r8d, %edx
	imull	$1000000007, %edx, %edx         # imm = 0x3B9ACA07
	subl	%edx, %ecx
	leal	-2(%rax), %edx
	imull	%ecx, %edx
	movslq	%edx, %rcx
	imulq	$1152921497, %rcx, %rdx         # imm = 0x44B82F99
	movq	%rdx, %r8
	shrq	$63, %r8
	sarq	$60, %rdx
	addl	%r8d, %edx
	imull	$1000000007, %edx, %edx         # imm = 0x3B9ACA07
	subl	%edx, %ecx
	leal	-1(%rax), %edx
	imull	%ecx, %edx
	movslq	%edx, %rcx
	imulq	$1152921497, %rcx, %rdx         # imm = 0x44B82F99
	movq	%rdx, %r8
	shrq	$63, %r8
	sarq	$60, %rdx
	addl	%r8d, %edx
	imull	$1000000007, %edx, %edx         # imm = 0x3B9ACA07
	subl	%edx, %ecx
	imull	%eax, %ecx
	movslq	%ecx, %rcx
	imulq	$1152921497, %rcx, %rdx         # imm = 0x44B82F99
	movq	%rdx, %r8
	shrq	$63, %r8
	sarq	$60, %rdx
	addl	%r8d, %edx
	imull	$1000000007, %edx, %edx         # imm = 0x3B9ACA07
	subl	%edx, %ecx
	addl	$4, %eax
	cmpl	$100004, %eax                   # imm = 0x186A4
	jne	.LBB0_3
# %bb.4:                                # %tml_bench_int_mul.exit9
                                        # kill: def $ecx killed $ecx killed $rcx
	callq	tml_black_box_i32
	movl	$1, %ecx
	movl	$4, %eax
	.p2align	4
.LBB0_5:                                # %if.end11.i10
                                        # =>This Inner Loop Header: Depth=1
	leal	-3(%rax), %edx
	imull	%ecx, %edx
	movslq	%edx, %rcx
	imulq	$1152921497, %rcx, %rdx         # imm = 0x44B82F99
	movq	%rdx, %r8
	shrq	$63, %r8
	sarq	$60, %rdx
	addl	%r8d, %edx
	imull	$1000000007, %edx, %edx         # imm = 0x3B9ACA07
	subl	%edx, %ecx
	leal	-2(%rax), %edx
	imull	%ecx, %edx
	movslq	%edx, %rcx
	imulq	$1152921497, %rcx, %rdx         # imm = 0x44B82F99
	movq	%rdx, %r8
	shrq	$63, %r8
	sarq	$60, %rdx
	addl	%r8d, %edx
	imull	$1000000007, %edx, %edx         # imm = 0x3B9ACA07
	subl	%edx, %ecx
	leal	-1(%rax), %edx
	imull	%ecx, %edx
	movslq	%edx, %rcx
	imulq	$1152921497, %rcx, %rdx         # imm = 0x44B82F99
	movq	%rdx, %r8
	shrq	$63, %r8
	sarq	$60, %rdx
	addl	%r8d, %edx
	imull	$1000000007, %edx, %edx         # imm = 0x3B9ACA07
	subl	%edx, %ecx
	imull	%eax, %ecx
	movslq	%ecx, %rcx
	imulq	$1152921497, %rcx, %rdx         # imm = 0x44B82F99
	movq	%rdx, %r8
	shrq	$63, %r8
	sarq	$60, %rdx
	addl	%r8d, %edx
	imull	$1000000007, %edx, %edx         # imm = 0x3B9ACA07
	subl	%edx, %ecx
	addl	$4, %eax
	cmpl	$100004, %eax                   # imm = 0x186A4
	jne	.LBB0_5
# %bb.6:                                # %tml_bench_int_mul.exit17
                                        # kill: def $ecx killed $ecx killed $rcx
	callq	tml_black_box_i32
	movq	%rsi, %rcx
	callq	tml_instant_elapsed
	movq	%rax, %rcx
	callq	tml_duration_format_ms
	movq	%rax, %rsi
	leaq	.L.fmt.str.no_nl(%rip), %rdi
	leaq	.L.str.4(%rip), %rdx
	movq	%rdi, %rcx
	callq	printf
	movq	%rdi, %rcx
	movq	%rsi, %rdx
	callq	printf
	leaq	.L.str.5(%rip), %rdx
	movq	%rdi, %rcx
	callq	printf
	movl	$10, %ecx
	callq	putchar
	movl	$1, %ebx
	movl	$2, %edi
	movl	$3, %r14d
	movl	$100000, %ebp                   # imm = 0x186A0
	callq	tml_instant_now
	movq	%rax, %rsi
	.p2align	4
.LBB0_7:                                # %if.end17.i
                                        # =>This Inner Loop Header: Depth=1
	addl	%edi, %ebx
	imull	%r14d, %ebx
	movslq	%ebx, %rbx
	imulq	$1152921497, %rbx, %rax         # imm = 0x44B82F99
	movq	%rax, %rcx
	shrq	$63, %rcx
	sarq	$60, %rax
	addl	%ecx, %eax
	imull	$1000000007, %eax, %eax         # imm = 0x3B9ACA07
	subl	%eax, %ebx
	imull	%r14d, %edi
	addl	%ebx, %edi
	movslq	%edi, %rdi
	imulq	$1152921497, %rdi, %rax         # imm = 0x44B82F99
	movq	%rax, %rcx
	shrq	$63, %rcx
	sarq	$60, %rax
	addl	%ecx, %eax
	imull	$1000000007, %eax, %eax         # imm = 0x3B9ACA07
	subl	%eax, %edi
	addl	%ebx, %r14d
	subl	%edi, %r14d
	movslq	%r14d, %rax
	imulq	$1152921497, %rax, %rcx         # imm = 0x44B82F99
	movq	%rcx, %rdx
	shrq	$63, %rdx
	sarq	$60, %rcx
	addl	%edx, %ecx
	imull	$1000000007, %ecx, %ecx         # imm = 0x3B9ACA07
	movl	%eax, %edx
	subl	%ecx, %edx
	negl	%ecx
	testl	%edx, %edx
	leal	1000000007(%rax,%rcx), %r14d
	cmovnsl	%edx, %r14d
	decl	%ebp
	jne	.LBB0_7
# %bb.8:                                # %tml_bench_mixed_ops.exit
	addl	%ebx, %edi
	addl	%r14d, %edi
	movl	%edi, %ecx
	callq	tml_black_box_i32
	movl	$1, %eax
	movl	$2, %ecx
	movl	$3, %r8d
	movl	$100000, %edx                   # imm = 0x186A0
	.p2align	4
.LBB0_9:                                # %if.end17.i19
                                        # =>This Inner Loop Header: Depth=1
	addl	%ecx, %eax
	imull	%r8d, %eax
	cltq
	imulq	$1152921497, %rax, %r9          # imm = 0x44B82F99
	movq	%r9, %r10
	shrq	$63, %r10
	sarq	$60, %r9
	addl	%r10d, %r9d
	imull	$1000000007, %r9d, %r9d         # imm = 0x3B9ACA07
	subl	%r9d, %eax
	imull	%r8d, %ecx
	addl	%eax, %ecx
	movslq	%ecx, %rcx
	imulq	$1152921497, %rcx, %r9          # imm = 0x44B82F99
	movq	%r9, %r10
	shrq	$63, %r10
	sarq	$60, %r9
	addl	%r10d, %r9d
	imull	$1000000007, %r9d, %r9d         # imm = 0x3B9ACA07
	subl	%r9d, %ecx
	addl	%eax, %r8d
	subl	%ecx, %r8d
	movslq	%r8d, %r8
	imulq	$1152921497, %r8, %r9           # imm = 0x44B82F99
	movq	%r9, %r10
	shrq	$63, %r10
	sarq	$60, %r9
	addl	%r10d, %r9d
	imull	$1000000007, %r9d, %r9d         # imm = 0x3B9ACA07
	movl	%r8d, %r10d
	subl	%r9d, %r10d
	negl	%r9d
	testl	%r10d, %r10d
	leal	1000000007(%r8,%r9), %r8d
	cmovnsl	%r10d, %r8d
	decl	%edx
	jne	.LBB0_9
# %bb.10:                               # %tml_bench_mixed_ops.exit40
	addl	%eax, %ecx
	addl	%r8d, %ecx
                                        # kill: def $ecx killed $ecx killed $rcx
	callq	tml_black_box_i32
	movl	$1, %eax
	movl	$2, %ecx
	movl	$3, %r8d
	movl	$100000, %edx                   # imm = 0x186A0
	.p2align	4
.LBB0_11:                               # %if.end17.i41
                                        # =>This Inner Loop Header: Depth=1
	addl	%ecx, %eax
	imull	%r8d, %eax
	cltq
	imulq	$1152921497, %rax, %r9          # imm = 0x44B82F99
	movq	%r9, %r10
	shrq	$63, %r10
	sarq	$60, %r9
	addl	%r10d, %r9d
	imull	$1000000007, %r9d, %r9d         # imm = 0x3B9ACA07
	subl	%r9d, %eax
	imull	%r8d, %ecx
	addl	%eax, %ecx
	movslq	%ecx, %rcx
	imulq	$1152921497, %rcx, %r9          # imm = 0x44B82F99
	movq	%r9, %r10
	shrq	$63, %r10
	sarq	$60, %r9
	addl	%r10d, %r9d
	imull	$1000000007, %r9d, %r9d         # imm = 0x3B9ACA07
	subl	%r9d, %ecx
	addl	%eax, %r8d
	subl	%ecx, %r8d
	movslq	%r8d, %r8
	imulq	$1152921497, %r8, %r9           # imm = 0x44B82F99
	movq	%r9, %r10
	shrq	$63, %r10
	sarq	$60, %r9
	addl	%r10d, %r9d
	imull	$1000000007, %r9d, %r9d         # imm = 0x3B9ACA07
	movl	%r8d, %r10d
	subl	%r9d, %r10d
	negl	%r9d
	testl	%r10d, %r10d
	leal	1000000007(%r8,%r9), %r8d
	cmovnsl	%r10d, %r8d
	decl	%edx
	jne	.LBB0_11
# %bb.12:                               # %tml_bench_mixed_ops.exit62
	addl	%eax, %ecx
	addl	%r8d, %ecx
                                        # kill: def $ecx killed $ecx killed $rcx
	callq	tml_black_box_i32
	movq	%rsi, %rcx
	callq	tml_instant_elapsed
	movq	%rax, %rcx
	callq	tml_duration_format_ms
	movq	%rax, %rsi
	leaq	.L.fmt.str.no_nl(%rip), %rdi
	leaq	.L.str.6(%rip), %rdx
	movq	%rdi, %rcx
	callq	printf
	movq	%rdi, %rcx
	movq	%rsi, %rdx
	callq	printf
	leaq	.L.str.7(%rip), %rdx
	movq	%rdi, %rcx
	callq	printf
	movl	$10, %ecx
	callq	putchar
	movl	$1, %edi
	xorl	%ebp, %ebp
	movl	$9999, %ebx                     # imm = 0x270F
	callq	tml_instant_now
	movq	%rax, %rsi
	.p2align	4
.LBB0_13:                               # %if.end29.i
                                        # =>This Inner Loop Header: Depth=1
	addl	%edi, %ebp
	addl	%ebp, %edi
	addl	%edi, %ebp
	addl	%ebp, %edi
	addl	%edi, %ebp
	addl	%ebp, %edi
	addl	%edi, %ebp
	addl	%ebp, %edi
	addl	%edi, %ebp
	movl	%edi, %eax
	addl	%ebp, %eax
	movl	%ebp, %edi
	addl	%eax, %edi
	movl	%eax, %ebp
	addl	$-11, %ebx
	jne	.LBB0_13
# %bb.14:                               # %tml_bench_fibonacci.exit
	movl	%edi, %ecx
	callq	tml_black_box_i32
	movl	$1, %ecx
	xorl	%edx, %edx
	movl	$9999, %eax                     # imm = 0x270F
	.p2align	4
.LBB0_15:                               # %if.end29.i64
                                        # =>This Inner Loop Header: Depth=1
	addl	%ecx, %edx
	addl	%edx, %ecx
	addl	%ecx, %edx
	addl	%edx, %ecx
	addl	%ecx, %edx
	addl	%edx, %ecx
	addl	%ecx, %edx
	addl	%edx, %ecx
	addl	%ecx, %edx
	movl	%ecx, %r8d
	addl	%edx, %r8d
	movl	%edx, %ecx
	addl	%r8d, %ecx
	movl	%r8d, %edx
	addl	$-11, %eax
	jne	.LBB0_15
# %bb.16:                               # %tml_bench_fibonacci.exit71
	callq	tml_black_box_i32
	movl	$1, %ecx
	xorl	%edx, %edx
	movl	$9999, %eax                     # imm = 0x270F
	.p2align	4
.LBB0_17:                               # %if.end29.i72
                                        # =>This Inner Loop Header: Depth=1
	addl	%ecx, %edx
	addl	%edx, %ecx
	addl	%ecx, %edx
	addl	%edx, %ecx
	addl	%ecx, %edx
	addl	%edx, %ecx
	addl	%ecx, %edx
	addl	%edx, %ecx
	addl	%ecx, %edx
	movl	%ecx, %r8d
	addl	%edx, %r8d
	movl	%edx, %ecx
	addl	%r8d, %ecx
	movl	%r8d, %edx
	addl	$-11, %eax
	jne	.LBB0_17
# %bb.18:                               # %tml_bench_fibonacci.exit79
	callq	tml_black_box_i32
	movq	%rsi, %rcx
	callq	tml_instant_elapsed
	movq	%rax, %rcx
	callq	tml_duration_format_ms
	movq	%rax, %rsi
	leaq	.L.fmt.str.no_nl(%rip), %rdi
	leaq	.L.str.8(%rip), %rdx
	movq	%rdi, %rcx
	callq	printf
	movq	%rdi, %rcx
	movq	%rsi, %rdx
	callq	printf
	leaq	.L.str.9(%rip), %rdx
	movq	%rdi, %rcx
	callq	printf
	movl	$10, %ecx
	callq	putchar
	xorl	%edi, %edi
	movl	$2, %ebx
	callq	tml_instant_now
	movq	%rax, %rsi
	jmp	.LBB0_21
	.p2align	4
.LBB0_19:                               #   in Loop: Header=BB0_21 Depth=1
	movl	$1, %r8d
.LBB0_20:                               # %loop.end38.i
                                        #   in Loop: Header=BB0_21 Depth=1
	addl	%r8d, %edi
	incl	%ebx
	cmpl	$1001, %ebx                     # imm = 0x3E9
	je	.LBB0_26
.LBB0_21:                               # %if.end35.i
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB0_23 Depth 2
	cmpl	$4, %ebx
	jb	.LBB0_19
# %bb.22:                               # %if.end41.i.preheader
                                        #   in Loop: Header=BB0_21 Depth=1
	movl	$3, %ecx
	.p2align	4
.LBB0_23:                               # %if.end41.i
                                        #   Parent Loop BB0_21 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	leal	-1(%rcx), %r9d
	xorl	%r8d, %r8d
	movl	%ebx, %eax
	xorl	%edx, %edx
	divl	%r9d
	testl	%edx, %edx
	je	.LBB0_20
# %bb.24:                               # %loop.start36.i
                                        #   in Loop: Header=BB0_23 Depth=2
	movl	%ecx, %eax
	imull	%ecx, %eax
	incl	%ecx
	cmpl	%ebx, %eax
	jbe	.LBB0_23
	jmp	.LBB0_19
.LBB0_26:                               # %tml_bench_count_primes.exit
	movl	%edi, %ecx
	callq	tml_black_box_i32
	xorl	%ecx, %ecx
	movl	$2, %r8d
	jmp	.LBB0_29
	.p2align	4
.LBB0_27:                               #   in Loop: Header=BB0_29 Depth=1
	movl	$1, %r10d
.LBB0_28:                               # %loop.end38.i95
                                        #   in Loop: Header=BB0_29 Depth=1
	addl	%r10d, %ecx
	incl	%r8d
	cmpl	$1001, %r8d                     # imm = 0x3E9
	je	.LBB0_34
.LBB0_29:                               # %if.end35.i83
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB0_31 Depth 2
	cmpl	$4, %r8d
	jb	.LBB0_27
# %bb.30:                               # %if.end41.i87.preheader
                                        #   in Loop: Header=BB0_29 Depth=1
	movl	$3, %r9d
	.p2align	4
.LBB0_31:                               # %if.end41.i87
                                        #   Parent Loop BB0_29 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	leal	-1(%r9), %r11d
	xorl	%r10d, %r10d
	movl	%r8d, %eax
	xorl	%edx, %edx
	divl	%r11d
	testl	%edx, %edx
	je	.LBB0_28
# %bb.32:                               # %loop.start36.i91
                                        #   in Loop: Header=BB0_31 Depth=2
	movl	%r9d, %eax
	imull	%r9d, %eax
	incl	%r9d
	cmpl	%r8d, %eax
	jbe	.LBB0_31
	jmp	.LBB0_27
.LBB0_34:                               # %tml_bench_count_primes.exit100
	callq	tml_black_box_i32
	xorl	%ecx, %ecx
	movl	$2, %r8d
	jmp	.LBB0_37
	.p2align	4
.LBB0_35:                               #   in Loop: Header=BB0_37 Depth=1
	movl	$1, %r10d
.LBB0_36:                               # %loop.end38.i113
                                        #   in Loop: Header=BB0_37 Depth=1
	addl	%r10d, %ecx
	incl	%r8d
	cmpl	$1001, %r8d                     # imm = 0x3E9
	je	.LBB0_42
.LBB0_37:                               # %if.end35.i101
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB0_39 Depth 2
	cmpl	$4, %r8d
	jb	.LBB0_35
# %bb.38:                               # %if.end41.i105.preheader
                                        #   in Loop: Header=BB0_37 Depth=1
	movl	$3, %r9d
	.p2align	4
.LBB0_39:                               # %if.end41.i105
                                        #   Parent Loop BB0_37 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	leal	-1(%r9), %r11d
	xorl	%r10d, %r10d
	movl	%r8d, %eax
	xorl	%edx, %edx
	divl	%r11d
	testl	%edx, %edx
	je	.LBB0_36
# %bb.40:                               # %loop.start36.i109
                                        #   in Loop: Header=BB0_39 Depth=2
	movl	%r9d, %eax
	imull	%r9d, %eax
	incl	%r9d
	cmpl	%r8d, %eax
	jbe	.LBB0_39
	jmp	.LBB0_35
.LBB0_42:                               # %tml_bench_count_primes.exit118
	callq	tml_black_box_i32
	movq	%rsi, %rcx
	callq	tml_instant_elapsed
	movq	%rax, %rcx
	callq	tml_duration_format_ms
	movq	%rax, %rsi
	leaq	.L.fmt.str.no_nl(%rip), %rdi
	leaq	.L.str.10(%rip), %rdx
	movq	%rdi, %rcx
	callq	printf
	movq	%rdi, %rcx
	movq	%rsi, %rdx
	callq	printf
	leaq	.L.str.11(%rip), %rdx
	movq	%rdi, %rcx
	callq	printf
	movl	$10, %ecx
	callq	putchar
	movl	$10, %ecx
	callq	putchar
	leaq	.L.str.13(%rip), %rcx
	callq	puts
	xorl	%eax, %eax
	addq	$32, %rsp
	popq	%rbx
	popq	%rbp
	popq	%rdi
	popq	%rsi
	popq	%r14
	retq
                                        # -- End function
	.def	main;
	.scl	2;
	.type	32;
	.endef
	.globl	main                            # -- Begin function main
	.p2align	4
main:                                   # @main
# %bb.0:                                # %entry
	subq	$40, %rsp
	callq	tml_main
	xorl	%eax, %eax
	addq	$40, %rsp
	retq
                                        # -- End function
	.section	.rdata,"dr"
.L.fmt.str.no_nl:                       # @.fmt.str.no_nl
	.asciz	"%s"

	.p2align	4, 0x0                          # @.str.0
.L.str.0:
	.asciz	"=== TML Arithmetic Benchmarks ==="

.L.str.2:                               # @.str.2
	.asciz	"int_add_1M: "

	.p2align	4, 0x0                          # @.str.3
.L.str.3:
	.asciz	" ms (avg of 3 runs)"

.L.str.4:                               # @.str.4
	.asciz	"int_mul_100K: "

	.p2align	4, 0x0                          # @.str.5
.L.str.5:
	.asciz	" ms (avg of 3 runs)"

	.p2align	4, 0x0                          # @.str.6
.L.str.6:
	.asciz	"mixed_ops_100K: "

	.p2align	4, 0x0                          # @.str.7
.L.str.7:
	.asciz	" ms (avg of 3 runs)"

.L.str.8:                               # @.str.8
	.asciz	"fibonacci_10K: "

	.p2align	4, 0x0                          # @.str.9
.L.str.9:
	.asciz	" ms (avg of 3 runs)"

	.p2align	4, 0x0                          # @.str.10
.L.str.10:
	.asciz	"count_primes_1K: "

	.p2align	4, 0x0                          # @.str.11
.L.str.11:
	.asciz	" ms (avg of 3 runs)"

.L.str.13:                              # @.str.13
	.asciz	"Done."

	.addrsig
	.addrsig_sym .L.fmt.str.no_nl
	.addrsig_sym .L.str.0
	.addrsig_sym .L.str.2
	.addrsig_sym .L.str.3
	.addrsig_sym .L.str.4
	.addrsig_sym .L.str.5
	.addrsig_sym .L.str.6
	.addrsig_sym .L.str.7
	.addrsig_sym .L.str.8
	.addrsig_sym .L.str.9
	.addrsig_sym .L.str.10
	.addrsig_sym .L.str.11
	.addrsig_sym .L.str.13
