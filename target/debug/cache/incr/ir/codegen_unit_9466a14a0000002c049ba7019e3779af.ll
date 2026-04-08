target datalayout = "e-m:w-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-i128:128:128-f32:32:32-f64:64:64-v64:64:64-v128:128:128-v256:256:256-a:0:64-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc"

$__tml_ident = comdat any
@__tml_ident = linkonce_odr constant [19 x i8] c"tml version 0.2.14\00", comdat, align 1
@llvm.used = appending global [1 x ptr] [ptr @__tml_ident], section "llvm.metadata"

; Runtime type declarations
%struct.tml_str = type { ptr, i64 }
%struct.Ordering = type { i32 }

; Runtime declarations (on-demand)
declare dso_local void @tml_str_free(ptr)
declare dso_local i32 @strcmp(ptr, ptr)
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)
declare dso_local void @panic(ptr) noreturn
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) nounwind
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) nounwind
declare dso_local void @println(ptr)
declare dso_local ptr @f64_to_string(double)
declare dso_local ptr @f32_to_string(float)
declare dso_local ptr @f64_to_exp_string(double, i32)
declare dso_local ptr @f32_to_exp_string(float, i32)
declare dso_local ptr @mem_alloc(i64)
declare dso_local ptr @mem_realloc(ptr, i64)
declare dso_local void @mem_free(ptr)


; Lowlevel functions from imported modules

; Generic types from imported modules
%struct.Mask2 = type { i1, i1 }
%struct.Mask4 = type { i1, i1, i1, i1 }
%struct.Mask16 = type { i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1 }
%struct.Mask8 = type { i1, i1, i1, i1, i1, i1, i1, i1 }
%struct.Mask32 = type { i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1, i1 }
%struct.IrParam = type { ptr, ptr }
%struct.List__Str = type { ptr }
%struct.IrInstr = type { ptr, ptr, %struct.List__Str, ptr }
%struct.List__IrInstr = type { ptr }
%struct.IrBlock = type { ptr, %struct.List__IrInstr }
%struct.List__IrFunction = type { ptr }
%struct.List__IrGlobal = type { ptr }
%struct.IrModule = type { %struct.List__IrFunction, %struct.List__IrGlobal, %struct.List__Str }
%struct.List__IrParam = type { ptr }
%struct.List__IrBlock = type { ptr }
%struct.IrFunction = type { ptr, ptr, %struct.List__IrParam, %struct.List__IrBlock, %struct.List__Str }
%struct.IrGlobal = type { ptr, ptr, ptr }
%struct.ParseError = type { i64, i64, ptr }
%struct.I8x16 = type <16 x i8>
%struct.FnHeader = type { ptr, ptr, %struct.List__IrParam, %struct.List__Str }
%struct.FunctionDiff = type { ptr, i64, ptr, ptr, %struct.List__Str }
%struct.List__FunctionDiff = type { ptr }
%struct.DiffResult = type { %struct.List__Str, %struct.List__Str, %struct.List__FunctionDiff }
%struct.DiffError = type { ptr }
%struct.Buffer = type { ptr }
%struct.Interval = type { i64, i64 }
%struct.I64x2 = type <2 x i64>
%struct.F32x4 = type <4 x float>
%struct.F32x8 = type <8 x float>
%struct.I32x4 = type <4 x i32>
%struct.F64x2 = type <2 x double>
%struct.U8x32 = type <32 x i8>
%struct.F64x4 = type <4 x double>
%struct.I16x16 = type <16 x i16>
%struct.I32x8 = type <8 x i32>
%struct.I64x4 = type <4 x i64>
%struct.I8x32 = type <32 x i8>
%struct.U8x16 = type <16 x i8>
%struct.Maybe__tuple_Str_Str = type { i32, [2 x i64] }
%struct.Maybe__I32 = type { i32, i32 }
%struct.Maybe__I64 = type { i32, i64 }
%struct.List__I64 = type { ptr }
%struct.List__I32 = type { ptr }
%struct.List__U8 = type { ptr }
%struct.Maybe__FnHeader = type { i32, [4 x i64] }
%struct.Maybe__IrGlobal = type { i32, [3 x i64] }
%struct.Outcome__IrModule__ParseError = type { i32, [3 x i64] }
%struct.Maybe__FunctionDiff = type { i32, [5 x i64] }
%struct.Outcome__DiffResult__DiffError = type { i32, [3 x i64] }
%struct.Text = type { ptr }
%struct.Maybe__U16 = type { i32, i32 }
%struct.Maybe__F64 = type { i32, i64 }
%struct.Maybe__Bool = type { i32, i32 }
; Pure TML functions from imported modules
; Module: std::collections::list
; Module: core::runtime::intrinsics

; @extern("c") tml_cpuid_eax
declare dso_local i32 @tml_cpuid_eax(i32, i32)

; @extern("c") tml_cpuid_ebx
declare dso_local i32 @tml_cpuid_ebx(i32, i32)

; @extern("c") tml_cpuid_ecx
declare dso_local i32 @tml_cpuid_ecx(i32, i32)

; @extern("c") tml_cpuid_edx
declare dso_local i32 @tml_cpuid_edx(i32, i32)

; @extern("c") tml_xgetbv
declare dso_local i64 @tml_xgetbv(i32)
; Module: core::traits::default
; Module: core::str::split

; @extern("c") c_strlen
declare dso_local i64 @strlen(ptr)

; @extern("c") c_memcmp
declare dso_local i32 @memcmp(ptr, ptr, i64)

; @extern("c") c_memchr
declare dso_local ptr @memchr(ptr, i32, i64)








; Module: core::simd::mask
; Module: core::runtime::mem
; Module: core::simd::i8x16

; Module: std::collections::buffer

; @extern("c") c_buf_last_index_of_simd
declare dso_local i64 @buf_last_index_of_simd(ptr, i64, i32, i64)

; @extern("c") c_buf_bswap16
declare dso_local void @buf_bswap16(ptr, i64)

; @extern("c") c_buf_bswap32
declare dso_local void @buf_bswap32(ptr, i64)

; @extern("c") c_buf_bswap64
declare dso_local void @buf_bswap64(ptr, i64)


















; Module: core::str::basic








; Module: core::simd::detect







; Module: core::str::simd



























; Module: core::str::search







; Module: core::str::transform








; Module: core::str::convert










; Module: ir_diff::parser











; Module: ir_diff::differ




















; Module: std::collections::behaviors
; Module: core::str
; Module: std::text























; Module: core::fmt::impls
; Module: core::fmt::helpers


































; Dynamic dispatch types
%dyn.Default = type { ptr, ptr }
%dyn.Drop = type { ptr, ptr }
%dyn.Iterator = type { ptr, ptr }
%dyn.LowerHex = type { ptr, ptr }
%dyn.Display = type { ptr, ptr }
%dyn.Debug = type { ptr, ptr }
%dyn.Duplicate = type { ptr, ptr }
%dyn.PartialEq = type { ptr, ptr }
%dyn.Eq = type { ptr, ptr }
%dyn.PartialOrd = type { ptr, ptr }
%dyn.Ord = type { ptr, ptr }
%dyn.UpperHex = type { ptr, ptr }
%dyn.Hash = type { ptr, ptr }
%dyn.Index = type { ptr, ptr }
%dyn.IndexMut = type { ptr, ptr }
%dyn.FromIterator = type { ptr, ptr }
%dyn.IntoIterator = type { ptr, ptr }
%dyn.Extend = type { ptr, ptr }
%dyn.UpperExp = type { ptr, ptr }
%dyn.Binary = type { ptr, ptr }
%dyn.Octal = type { ptr, ptr }
%dyn.LowerExp = type { ptr, ptr }


; @extern("c") c_puts
declare dso_local i32 @puts(ptr)

; @extern("c") c_printf
declare dso_local i32 @printf(ptr)

; @extern("c") tml_os_args_count
declare dso_local i32 @tml_os_args_count()

; @extern("c") tml_os_args_get
declare dso_local ptr @tml_os_args_get(i32)

; @extern("c") fopen
declare dso_local i64 @fopen(ptr, ptr)

; @extern("c") fclose
declare dso_local i32 @fclose(i64)

; @extern("c") fseek
declare dso_local i32 @fseek(i64, i64, i32)

; @extern("c") ftell
declare dso_local i64 @ftell(i64)

; @extern("c") fread
declare dso_local i64 @fread(i64, i64, i64, i64)

; @extern("c") malloc
declare dso_local i64 @malloc(i64)

; @extern("c") memset
declare dso_local i64 @memset(i64, i32, i64)

define internal ptr @tml_read_file(ptr %path) #0 {
entry:
  %t0 = alloca ptr
  store ptr %path, ptr %t0
  %t3 = alloca i64
  %t12 = alloca i64
  %t27 = alloca i64
  %t38 = alloca i64
  %t1 = load ptr, ptr %t0
  %t2 = call i64 @fopen(ptr %t1, ptr @.str.0)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3)
  store i64 %t2, ptr %t3
  %t4 = load i64, ptr %t3
  %t6 = sext i32 0 to i64
  %t5 = icmp eq i64 %t4, %t6
  br i1 %t5, label %if.then0, label %if.end2
if.then0:
  ret ptr @.str.1
if.end2:
  %t7 = load i64, ptr %t3
  %t8 = sext i32 0 to i64
  %t9 = call i32 @fseek(i64 %t7, i64 %t8, i32 2)
  %t10 = load i64, ptr %t3
  %t11 = call i64 @ftell(i64 %t10)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t12)
  store i64 %t11, ptr %t12
  %t13 = load i64, ptr %t3
  %t14 = sext i32 0 to i64
  %t15 = call i32 @fseek(i64 %t13, i64 %t14, i32 0)
  %t16 = load i64, ptr %t12
  %t18 = sext i32 0 to i64
  %t17 = icmp sle i64 %t16, %t18
  br i1 %t17, label %if.then3, label %if.end5
if.then3:
  %t19 = load i64, ptr %t3
  %t20 = call i32 @fclose(i64 %t19)
  ret ptr @.str.1
if.end5:
  %t21 = load i64, ptr %t12
  %t23 = sext i32 1 to i64
  %t24 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t21, i64 %t23)
  %t22 = extractvalue { i64, i1 } %t24, 0
  %t25 = extractvalue { i64, i1 } %t24, 1
  br i1 %t25, label %add_overflow7, label %add_ok6
add_overflow7:
  call void @panic(ptr @.str.2)
  unreachable
add_ok6:
  %t26 = call i64 @malloc(i64 %t22)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t27)
  store i64 %t26, ptr %t27
  %t28 = load i64, ptr %t27
  %t30 = sext i32 0 to i64
  %t29 = icmp eq i64 %t28, %t30
  br i1 %t29, label %if.then8, label %if.end10
if.then8:
  %t31 = load i64, ptr %t3
  %t32 = call i32 @fclose(i64 %t31)
  ret ptr @.str.1
if.end10:
  %t33 = load i64, ptr %t27
  %t34 = sext i32 1 to i64
  %t35 = load i64, ptr %t12
  %t36 = load i64, ptr %t3
  %t37 = call i64 @fread(i64 %t33, i64 %t34, i64 %t35, i64 %t36)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t38)
  store i64 %t37, ptr %t38
  %t39 = load i64, ptr %t3
  %t40 = call i32 @fclose(i64 %t39)
  %t41 = load i64, ptr %t27
  %t42 = load i64, ptr %t38
  %t44 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t41, i64 %t42)
  %t43 = extractvalue { i64, i1 } %t44, 0
  %t45 = extractvalue { i64, i1 } %t44, 1
  br i1 %t45, label %add_overflow12, label %add_ok11
add_overflow12:
  call void @panic(ptr @.str.3)
  unreachable
add_ok11:
  %t46 = inttoptr i64 %t43 to ptr
  %t47 = trunc i32 0 to i8
  %t48 = sext i32 1 to i64
  call void @llvm.memset.p0.i64(ptr %t46, i8 %t47, i64 %t48, i1 false)
  %t49 = load i64, ptr %t27
  %t50 = inttoptr i64 %t49 to ptr
  ret ptr %t50
}

define internal void @tml_print_str(ptr %s) #0 {
entry:
  %t51 = alloca ptr
  store ptr %s, ptr %t51
  %t52 = load ptr, ptr %t51
  %t53 = call i32 @printf(ptr %t52)
  ret void
}

define internal void @tml_println(ptr %s) #0 {
entry:
  %t54 = alloca ptr
  store ptr %s, ptr %t54
  %t55 = load ptr, ptr %t54
  %t56 = call i32 @puts(ptr %t55)
  ret void
}

define internal void @tml_print_usage() #0 {
entry:
  call void @println(ptr @.str.4)
  call void @println(ptr @.str.1)
  call void @println(ptr @.str.5)
  call void @println(ptr @.str.1)
  call void @println(ptr @.str.6)
  call void @println(ptr @.str.7)
  call void @println(ptr @.str.8)
  call void @println(ptr @.str.1)
  call void @println(ptr @.str.9)
  call void @println(ptr @.str.10)
  call void @println(ptr @.str.11)
  call void @println(ptr @.str.12)
  ret void
}

define i32 @tml_main() #0 {
entry:
  %t58 = alloca i32
  %t61 = alloca i1
  %t63 = alloca i32
  %t64 = alloca i1
  %t65 = alloca i1
  %t66 = alloca i32
  %t127 = alloca %struct.Outcome__IrModule__ParseError
  %t137 = alloca %struct.IrModule
  %t140 = alloca %struct.Outcome__IrModule__ParseError
  %t150 = alloca %struct.IrModule
  %t152 = alloca %struct.Outcome__DiffResult__DiffError
  %t161 = alloca %struct.DiffResult
  %t166 = alloca i64
  %t168 = alloca i64
  %t170 = alloca i64
  %t172 = alloca i64
  %t209 = alloca i64
  %t57 = call i32 @tml_os_args_count()
  call void @llvm.lifetime.start.p0(i64 4, ptr %t58)
  store i32 %t57, ptr %t58
  %t59 = alloca ptr
  store ptr @.str.1, ptr %t59
  %t60 = alloca ptr
  store ptr @.str.1, ptr %t60
  call void @llvm.lifetime.start.p0(i64 1, ptr %t61)
  store i1 0, ptr %t61
  %t62 = alloca ptr
  store ptr @.str.1, ptr %t62
  call void @llvm.lifetime.start.p0(i64 4, ptr %t63)
  store i32 0, ptr %t63
  call void @llvm.lifetime.start.p0(i64 1, ptr %t64)
  store i1 0, ptr %t64
  call void @llvm.lifetime.start.p0(i64 1, ptr %t65)
  store i1 0, ptr %t65
  call void @llvm.lifetime.start.p0(i64 4, ptr %t66)
  store i32 1, ptr %t66
  br label %loop.preheader13
loop.preheader13:
  br label %loop.header14
loop.header14:
  %t67 = load i32, ptr %t66
  %t68 = load i32, ptr %t58
  %t69 = icmp slt i32 %t67, %t68
  br i1 %t69, label %loop.body15, label %loop.exit17
loop.body15:
  %t70 = load i32, ptr %t66
  %t71 = call ptr @tml_os_args_get(i32 %t70)
  %t72 = alloca ptr
  store ptr %t71, ptr %t72
  %t73 = load i1, ptr %t64
  br i1 %t73, label %if.then18, label %if.end20
if.then18:
  %t74 = load ptr, ptr %t72
  store ptr %t74, ptr %t62
  store i1 0, ptr %t64
  %t75 = load i32, ptr %t66
  %t77 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %t75, i32 1)
  %t76 = extractvalue { i32, i1 } %t77, 0
  %t78 = extractvalue { i32, i1 } %t77, 1
  br i1 %t78, label %add_overflow22, label %add_ok21
add_overflow22:
  call void @panic(ptr @.str.13)
  unreachable
add_ok21:
  store i32 %t76, ptr %t66
  br label %loop.latch16
if.end20:
  %t79 = load ptr, ptr %t72
  %t80 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t79, ptr @.str.14)
  br i1 %t80, label %if.then23, label %if.else24
if.then23:
  %t81 = load ptr, ptr %t72
  %t82 = call i64 @strlen(ptr %t81)
  %t84 = sext i32 6 to i64
  %t83 = icmp eq i64 %t82, %t84
  %t85 = load ptr, ptr %t72
  %t86 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t85, ptr @.str.15)
  %t87 = and i1 %t83, %t86
  br i1 %t87, label %if.then26, label %if.end28
if.then26:
  store i1 1, ptr %t65
  br label %if.end28
if.end28:
  %t88 = load ptr, ptr %t72
  %t89 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t88, ptr @.str.16)
  br i1 %t89, label %if.then29, label %if.end31
if.then29:
  store i1 1, ptr %t61
  br label %if.end31
if.end31:
  %t90 = load ptr, ptr %t72
  %t91 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t90, ptr @.str.17)
  br i1 %t91, label %if.then32, label %if.end34
if.then32:
  store i1 1, ptr %t64
  br label %if.end34
if.end34:
  br label %if.end25
if.else24:
  %t92 = load i32, ptr %t63
  %t93 = icmp eq i32 %t92, 0
  br i1 %t93, label %if.then35, label %if.end37
if.then35:
  %t94 = load ptr, ptr %t72
  store ptr %t94, ptr %t59
  br label %if.end37
if.end37:
  %t95 = load i32, ptr %t63
  %t96 = icmp eq i32 %t95, 1
  br i1 %t96, label %if.then38, label %if.end40
if.then38:
  %t97 = load ptr, ptr %t72
  store ptr %t97, ptr %t60
  br label %if.end40
if.end40:
  %t98 = load i32, ptr %t63
  %t100 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %t98, i32 1)
  %t99 = extractvalue { i32, i1 } %t100, 0
  %t101 = extractvalue { i32, i1 } %t100, 1
  br i1 %t101, label %add_overflow42, label %add_ok41
add_overflow42:
  call void @panic(ptr @.str.18)
  unreachable
add_ok41:
  store i32 %t99, ptr %t63
  br label %if.end25
if.end25:
  %t102 = load i32, ptr %t66
  %t104 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %t102, i32 1)
  %t103 = extractvalue { i32, i1 } %t104, 0
  %t105 = extractvalue { i32, i1 } %t104, 1
  br i1 %t105, label %add_overflow44, label %add_ok43
add_overflow44:
  call void @panic(ptr @.str.19)
  unreachable
add_ok43:
  store i32 %t103, ptr %t66
  br label %loop.latch16
loop.latch16:
  br label %loop.header14, !llvm.loop !1000
loop.exit17:
  %t106 = load i1, ptr %t65
  br i1 %t106, label %if.then45, label %if.end47
if.then45:
  call void @tml_print_usage()
  ret i32 0
if.end47:
  %t107 = load i32, ptr %t63
  %t108 = icmp slt i32 %t107, 2
  br i1 %t108, label %if.then48, label %if.end50
if.then48:
  call void @tml_print_usage()
  ret i32 2
if.end50:
  %t109 = load ptr, ptr %t59
  %t110 = call ptr @tml_read_file(ptr %t109)
  %t111 = alloca ptr
  store ptr %t110, ptr %t111
  %t112 = load ptr, ptr %t60
  %t113 = call ptr @tml_read_file(ptr %t112)
  %t114 = alloca ptr
  store ptr %t113, ptr %t114
  %t115 = load ptr, ptr %t111
  %t116 = call i64 @strlen(ptr %t115)
  %t118 = sext i32 0 to i64
  %t117 = icmp eq i64 %t116, %t118
  br i1 %t117, label %if.then51, label %if.end53
if.then51:
  call void @tml_print_str(ptr @.str.20)
  %t119 = load ptr, ptr %t59
  call void @println(ptr %t119)
  ret i32 2
if.end53:
  %t120 = load ptr, ptr %t114
  %t121 = call i64 @strlen(ptr %t120)
  %t123 = sext i32 0 to i64
  %t122 = icmp eq i64 %t121, %t123
  br i1 %t122, label %if.then54, label %if.end56
if.then54:
  call void @tml_print_str(ptr @.str.20)
  %t124 = load ptr, ptr %t60
  call void @println(ptr %t124)
  ret i32 2
if.end56:
  %t125 = load ptr, ptr %t111
  %t126 = call %struct.Outcome__IrModule__ParseError @tml_N7ir_diff6parser5parseE_S(ptr %t125)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t127)
  store %struct.Outcome__IrModule__ParseError %t126, ptr %t127
  %t128 = load %struct.Outcome__IrModule__ParseError, ptr %t127
  %t129 = extractvalue %struct.Outcome__IrModule__ParseError %t128, 0
  %t130 = icmp eq i32 %t129, 1
  br i1 %t130, label %if.then57, label %if.end59
if.then57:
  call void @tml_print_str(ptr @.str.21)
  %t131 = load ptr, ptr %t59
  call void @println(ptr %t131)
  ret i32 2
if.end59:
  %t132 = load %struct.Outcome__IrModule__ParseError, ptr %t127
  %t133 = extractvalue %struct.Outcome__IrModule__ParseError %t132, 0
  %t134 = alloca %struct.Outcome__IrModule__ParseError
  store %struct.Outcome__IrModule__ParseError %t132, ptr %t134
  %t135 = getelementptr inbounds %struct.Outcome__IrModule__ParseError, ptr %t134, i32 0, i32 1
  %t136 = load %struct.IrModule, ptr %t135
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t137)
  store %struct.IrModule %t136, ptr %t137
  %t138 = load ptr, ptr %t114
  %t139 = call %struct.Outcome__IrModule__ParseError @tml_N7ir_diff6parser5parseE_S(ptr %t138)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t140)
  store %struct.Outcome__IrModule__ParseError %t139, ptr %t140
  %t141 = load %struct.Outcome__IrModule__ParseError, ptr %t140
  %t142 = extractvalue %struct.Outcome__IrModule__ParseError %t141, 0
  %t143 = icmp eq i32 %t142, 1
  br i1 %t143, label %if.then60, label %if.end62
if.then60:
  call void @tml_print_str(ptr @.str.21)
  %t144 = load ptr, ptr %t60
  call void @println(ptr %t144)
  ret i32 2
if.end62:
  %t145 = load %struct.Outcome__IrModule__ParseError, ptr %t140
  %t146 = extractvalue %struct.Outcome__IrModule__ParseError %t145, 0
  %t147 = alloca %struct.Outcome__IrModule__ParseError
  store %struct.Outcome__IrModule__ParseError %t145, ptr %t147
  %t148 = getelementptr inbounds %struct.Outcome__IrModule__ParseError, ptr %t147, i32 0, i32 1
  %t149 = load %struct.IrModule, ptr %t148
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t150)
  store %struct.IrModule %t149, ptr %t150
  %t151 = call %struct.Outcome__DiffResult__DiffError @tml_N7ir_diff6differ4diffE_R8IrModuleR8IrModule(ptr %t137, ptr %t150)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t152)
  store %struct.Outcome__DiffResult__DiffError %t151, ptr %t152
  %t153 = load %struct.Outcome__DiffResult__DiffError, ptr %t152
  %t154 = extractvalue %struct.Outcome__DiffResult__DiffError %t153, 0
  %t155 = icmp eq i32 %t154, 1
  br i1 %t155, label %if.then63, label %if.end65
if.then63:
  call void @println(ptr @.str.22)
  ret i32 2
if.end65:
  %t156 = load %struct.Outcome__DiffResult__DiffError, ptr %t152
  %t157 = extractvalue %struct.Outcome__DiffResult__DiffError %t156, 0
  %t158 = alloca %struct.Outcome__DiffResult__DiffError
  store %struct.Outcome__DiffResult__DiffError %t156, ptr %t158
  %t159 = getelementptr inbounds %struct.Outcome__DiffResult__DiffError, ptr %t158, i32 0, i32 1
  %t160 = load %struct.DiffResult, ptr %t159
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t161)
  store %struct.DiffResult %t160, ptr %t161
  %t162 = call i1 @tml_N7ir_diff6differ12is_identicalE_R10DiffResult(ptr %t161)
  br i1 %t162, label %if.then66, label %if.end68
if.then66:
  %t163 = load i1, ptr %t61
  %t164 = xor i1 %t163, 1
  br i1 %t164, label %if.then69, label %if.end71
if.then69:
  call void @println(ptr @.str.23)
  br label %if.end71
if.end71:
  ret i32 0
if.end68:
  %t165 = call i64 @tml_N7ir_diff6differ15only_in_a_countE_R10DiffResult(ptr %t161)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t166)
  store i64 %t165, ptr %t166
  %t167 = call i64 @tml_N7ir_diff6differ15only_in_b_countE_R10DiffResult(ptr %t161)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t168)
  store i64 %t167, ptr %t168
  %t169 = call i64 @tml_N7ir_diff6differ11diffs_countE_R10DiffResult(ptr %t161)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t170)
  store i64 %t169, ptr %t170
  %t171 = load i1, ptr %t61
  br i1 %t171, label %if.then72, label %if.else73
if.then72:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t172)
  store i64 0, ptr %t172
  %t173 = load i64, ptr %t166
  %t175 = sext i32 0 to i64
  %t174 = icmp sgt i64 %t173, %t175
  br i1 %t174, label %if.then75, label %if.end77
if.then75:
  call void @println(ptr @.str.24)
  br label %loop.preheader78
loop.preheader78:
  br label %loop.header79
loop.header79:
  %t176 = load i64, ptr %t172
  %t177 = load i64, ptr %t166
  %t178 = icmp slt i64 %t176, %t177
  br i1 %t178, label %loop.body80, label %loop.exit82
loop.body80:
  call void @tml_print_str(ptr @.str.25)
  %t179 = load i64, ptr %t172
  %t180 = call ptr @tml_N7ir_diff6differ14only_in_a_nameE_R10DiffResultl(ptr %t161, i64 %t179)
  call void @println(ptr %t180)
  %t181 = load i64, ptr %t172
  %t183 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t181, i64 1)
  %t182 = extractvalue { i64, i1 } %t183, 0
  %t184 = extractvalue { i64, i1 } %t183, 1
  br i1 %t184, label %add_overflow84, label %add_ok83
add_overflow84:
  call void @panic(ptr @.str.26)
  unreachable
add_ok83:
  store i64 %t182, ptr %t172
  br label %loop.latch81
loop.latch81:
  br label %loop.header79, !llvm.loop !1001
loop.exit82:
  br label %if.end77
if.end77:
  store i64 0, ptr %t172
  %t185 = load i64, ptr %t168
  %t187 = sext i32 0 to i64
  %t186 = icmp sgt i64 %t185, %t187
  br i1 %t186, label %if.then85, label %if.end87
if.then85:
  call void @println(ptr @.str.27)
  br label %loop.preheader88
loop.preheader88:
  br label %loop.header89
loop.header89:
  %t188 = load i64, ptr %t172
  %t189 = load i64, ptr %t168
  %t190 = icmp slt i64 %t188, %t189
  br i1 %t190, label %loop.body90, label %loop.exit92
loop.body90:
  call void @tml_print_str(ptr @.str.25)
  %t191 = load i64, ptr %t172
  %t192 = call ptr @tml_N7ir_diff6differ14only_in_b_nameE_R10DiffResultl(ptr %t161, i64 %t191)
  call void @println(ptr %t192)
  %t193 = load i64, ptr %t172
  %t195 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t193, i64 1)
  %t194 = extractvalue { i64, i1 } %t195, 0
  %t196 = extractvalue { i64, i1 } %t195, 1
  br i1 %t196, label %add_overflow94, label %add_ok93
add_overflow94:
  call void @panic(ptr @.str.28)
  unreachable
add_ok93:
  store i64 %t194, ptr %t172
  br label %loop.latch91
loop.latch91:
  br label %loop.header89, !llvm.loop !1002
loop.exit92:
  br label %if.end87
if.end87:
  store i64 0, ptr %t172
  %t197 = load i64, ptr %t170
  %t199 = sext i32 0 to i64
  %t198 = icmp sgt i64 %t197, %t199
  br i1 %t198, label %if.then95, label %if.end97
if.then95:
  call void @println(ptr @.str.29)
  br label %loop.preheader98
loop.preheader98:
  br label %loop.header99
loop.header99:
  %t200 = load i64, ptr %t172
  %t201 = load i64, ptr %t170
  %t202 = icmp slt i64 %t200, %t201
  br i1 %t202, label %loop.body100, label %loop.exit102
loop.body100:
  call void @tml_print_str(ptr @.str.25)
  %t203 = load i64, ptr %t172
  %t204 = call ptr @tml_N7ir_diff6differ14diff_func_nameE_R10DiffResultl(ptr %t161, i64 %t203)
  call void @println(ptr %t204)
  %t205 = load i64, ptr %t172
  %t207 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t205, i64 1)
  %t206 = extractvalue { i64, i1 } %t207, 0
  %t208 = extractvalue { i64, i1 } %t207, 1
  br i1 %t208, label %add_overflow104, label %add_ok103
add_overflow104:
  call void @panic(ptr @.str.30)
  unreachable
add_ok103:
  store i64 %t206, ptr %t172
  br label %loop.latch101
loop.latch101:
  br label %loop.header99, !llvm.loop !1003
loop.exit102:
  br label %if.end97
if.end97:
  call void @llvm.lifetime.end.p0(i64 8, ptr %t172)
  br label %if.end74
if.else73:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t209)
  store i64 0, ptr %t209
  br label %loop.preheader105
loop.preheader105:
  br label %loop.header106
loop.header106:
  %t210 = load i64, ptr %t209
  %t211 = load i64, ptr %t166
  %t212 = icmp slt i64 %t210, %t211
  br i1 %t212, label %loop.body107, label %loop.exit109
loop.body107:
  call void @tml_print_str(ptr @.str.31)
  %t213 = load i64, ptr %t209
  %t214 = call ptr @tml_N7ir_diff6differ14only_in_a_nameE_R10DiffResultl(ptr %t161, i64 %t213)
  call void @println(ptr %t214)
  call void @println(ptr @.str.32)
  %t215 = load i64, ptr %t209
  %t217 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t215, i64 1)
  %t216 = extractvalue { i64, i1 } %t217, 0
  %t218 = extractvalue { i64, i1 } %t217, 1
  br i1 %t218, label %add_overflow111, label %add_ok110
add_overflow111:
  call void @panic(ptr @.str.33)
  unreachable
add_ok110:
  store i64 %t216, ptr %t209
  br label %loop.latch108
loop.latch108:
  br label %loop.header106, !llvm.loop !1004
loop.exit109:
  store i64 0, ptr %t209
  br label %loop.preheader112
loop.preheader112:
  br label %loop.header113
loop.header113:
  %t219 = load i64, ptr %t209
  %t220 = load i64, ptr %t168
  %t221 = icmp slt i64 %t219, %t220
  br i1 %t221, label %loop.body114, label %loop.exit116
loop.body114:
  call void @tml_print_str(ptr @.str.34)
  %t222 = load i64, ptr %t209
  %t223 = call ptr @tml_N7ir_diff6differ14only_in_b_nameE_R10DiffResultl(ptr %t161, i64 %t222)
  call void @println(ptr %t223)
  call void @println(ptr @.str.35)
  %t224 = load i64, ptr %t209
  %t226 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t224, i64 1)
  %t225 = extractvalue { i64, i1 } %t226, 0
  %t227 = extractvalue { i64, i1 } %t226, 1
  br i1 %t227, label %add_overflow118, label %add_ok117
add_overflow118:
  call void @panic(ptr @.str.36)
  unreachable
add_ok117:
  store i64 %t225, ptr %t209
  br label %loop.latch115
loop.latch115:
  br label %loop.header113, !llvm.loop !1005
loop.exit116:
  store i64 0, ptr %t209
  br label %loop.preheader119
loop.preheader119:
  br label %loop.header120
loop.header120:
  %t228 = load i64, ptr %t209
  %t229 = load i64, ptr %t170
  %t230 = icmp slt i64 %t228, %t229
  br i1 %t230, label %loop.body121, label %loop.exit123
loop.body121:
  call void @tml_print_str(ptr @.str.37)
  %t231 = load i64, ptr %t209
  %t232 = call ptr @tml_N7ir_diff6differ14diff_func_nameE_R10DiffResultl(ptr %t161, i64 %t231)
  call void @tml_print_str(ptr %t232)
  call void @println(ptr @.str.38)
  call void @tml_print_str(ptr @.str.39)
  %t233 = load i64, ptr %t209
  %t234 = call ptr @tml_N7ir_diff6differ11diff_a_lineE_R10DiffResultl(ptr %t161, i64 %t233)
  call void @println(ptr %t234)
  call void @tml_print_str(ptr @.str.40)
  %t235 = load i64, ptr %t209
  %t236 = call ptr @tml_N7ir_diff6differ11diff_b_lineE_R10DiffResultl(ptr %t161, i64 %t235)
  call void @println(ptr %t236)
  %t237 = load i64, ptr %t209
  %t239 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t237, i64 1)
  %t238 = extractvalue { i64, i1 } %t239, 0
  %t240 = extractvalue { i64, i1 } %t239, 1
  br i1 %t240, label %add_overflow125, label %add_ok124
add_overflow125:
  call void @panic(ptr @.str.41)
  unreachable
add_ok124:
  store i64 %t238, ptr %t209
  br label %loop.latch122
loop.latch122:
  br label %loop.header120, !llvm.loop !1006
loop.exit123:
  call void @llvm.lifetime.end.p0(i64 8, ptr %t209)
  br label %if.end74
if.end74:
  ret i32 1
}
@vtable.I8.Default = internal constant { ptr } { ptr @tml_N4core2I87defaultE }
@vtable.I16.Default = internal constant { ptr } { ptr @tml_N4core3I167defaultE }
@vtable.I32.Default = internal constant { ptr } { ptr @tml_N4core3I327defaultE }
@vtable.I64.Default = internal constant { ptr } { ptr @tml_N4core3I647defaultE }
@vtable.U8.Default = internal constant { ptr } { ptr @tml_N4core2U87defaultE }
@vtable.U16.Default = internal constant { ptr } { ptr @tml_N4core3U167defaultE }
@vtable.U32.Default = internal constant { ptr } { ptr @tml_N4core3U327defaultE }
@vtable.U64.Default = internal constant { ptr } { ptr @tml_N4core3U647defaultE }
@vtable.F32.Default = internal constant { ptr } { ptr @tml_N4core3F327defaultE }
@vtable.F64.Default = internal constant { ptr } { ptr @tml_N4core3F647defaultE }
@vtable.Bool.Default = internal constant { ptr } { ptr @tml_N4core4Bool7defaultE }
@vtable.Str.Default = internal constant { ptr } { ptr @tml_N4core3Str7defaultE }
@vtable.Buffer.Drop = internal constant { ptr } { ptr @tml_N3std11collections6buffer6Buffer4dropE }
@vtable.List.Eq = internal constant {  } {  }
@vtable.List.FromIterator = internal constant {  } {  }
@vtable.List.Extend = internal constant {  } {  }
@vtable.HashSet.FromIterator = internal constant {  } {  }
@vtable.I8.Display = internal constant { ptr } { ptr @tml_N4core2I89to_stringE }
@vtable.I16.Display = internal constant { ptr } { ptr @tml_N4core3I169to_stringE }
@vtable.I32.Display = internal constant { ptr } { ptr @tml_N4core3I329to_stringE }
@vtable.I64.Display = internal constant { ptr } { ptr @tml_N4core3I649to_stringE }
@vtable.U8.Display = internal constant { ptr } { ptr @tml_N4core2U89to_stringE }
@vtable.U16.Display = internal constant { ptr } { ptr @tml_N4core3U169to_stringE }
@vtable.U32.Display = internal constant { ptr } { ptr @tml_N4core3U329to_stringE }
@vtable.U64.Display = internal constant { ptr } { ptr @tml_N4core3U649to_stringE }
@vtable.F32.Display = internal constant { ptr } { ptr @tml_N4core3F329to_stringE }
@vtable.F64.Display = internal constant { ptr } { ptr @tml_N4core3F649to_stringE }
@vtable.Bool.Display = internal constant { ptr } { ptr @tml_N4core4Bool9to_stringE }
@vtable.Str.Display = internal constant { ptr } { ptr @tml_N4core3Str9to_stringE }
@vtable.Char.Display = internal constant { ptr } { ptr @tml_N4core4Char9to_stringE }
@vtable.I8.Debug = internal constant { ptr } { ptr @tml_N4core2I812debug_stringE }
@vtable.I16.Debug = internal constant { ptr } { ptr @tml_N4core3I1612debug_stringE }
@vtable.I32.Debug = internal constant { ptr } { ptr @tml_N4core3I3212debug_stringE }
@vtable.I64.Debug = internal constant { ptr } { ptr @tml_N4core3I6412debug_stringE }
@vtable.U8.Debug = internal constant { ptr } { ptr @tml_N4core2U812debug_stringE }
@vtable.U16.Debug = internal constant { ptr } { ptr @tml_N4core3U1612debug_stringE }
@vtable.U32.Debug = internal constant { ptr } { ptr @tml_N4core3U3212debug_stringE }
@vtable.U64.Debug = internal constant { ptr } { ptr @tml_N4core3U6412debug_stringE }
@vtable.F32.Debug = internal constant { ptr } { ptr @tml_N4core3F3212debug_stringE }
@vtable.F64.Debug = internal constant { ptr } { ptr @tml_N4core3F6412debug_stringE }
@vtable.Bool.Debug = internal constant { ptr } { ptr @tml_N4core4Bool12debug_stringE }
@vtable.Str.Debug = internal constant { ptr } { ptr @tml_N4core3Str12debug_stringE }
@vtable.Char.Debug = internal constant { ptr } { ptr @tml_N4core4Char12debug_stringE }
@vtable.U8.Binary = internal constant { ptr } { ptr @tml_N4core2U810fmt_binaryE }
@vtable.U16.Binary = internal constant { ptr } { ptr @tml_N4core3U1610fmt_binaryE }
@vtable.U32.Binary = internal constant { ptr } { ptr @tml_N4core3U3210fmt_binaryE }
@vtable.U64.Binary = internal constant { ptr } { ptr @tml_N4core3U6410fmt_binaryE }
@vtable.I8.Binary = internal constant { ptr } { ptr @tml_N4core2I810fmt_binaryE }
@vtable.I16.Binary = internal constant { ptr } { ptr @tml_N4core3I1610fmt_binaryE }
@vtable.I32.Binary = internal constant { ptr } { ptr @tml_N4core3I3210fmt_binaryE }
@vtable.I64.Binary = internal constant { ptr } { ptr @tml_N4core3I6410fmt_binaryE }
@vtable.U8.Octal = internal constant { ptr } { ptr @tml_N4core2U89fmt_octalE }
@vtable.U16.Octal = internal constant { ptr } { ptr @tml_N4core3U169fmt_octalE }
@vtable.U32.Octal = internal constant { ptr } { ptr @tml_N4core3U329fmt_octalE }
@vtable.U64.Octal = internal constant { ptr } { ptr @tml_N4core3U649fmt_octalE }
@vtable.I8.Octal = internal constant { ptr } { ptr @tml_N4core2I89fmt_octalE }
@vtable.I16.Octal = internal constant { ptr } { ptr @tml_N4core3I169fmt_octalE }
@vtable.I32.Octal = internal constant { ptr } { ptr @tml_N4core3I329fmt_octalE }
@vtable.I64.Octal = internal constant { ptr } { ptr @tml_N4core3I649fmt_octalE }
@vtable.U8.LowerHex = internal constant { ptr } { ptr @tml_N4core2U813fmt_lower_hexE }
@vtable.U16.LowerHex = internal constant { ptr } { ptr @tml_N4core3U1613fmt_lower_hexE }
@vtable.U32.LowerHex = internal constant { ptr } { ptr @tml_N4core3U3213fmt_lower_hexE }
@vtable.U64.LowerHex = internal constant { ptr } { ptr @tml_N4core3U6413fmt_lower_hexE }
@vtable.I8.LowerHex = internal constant { ptr } { ptr @tml_N4core2I813fmt_lower_hexE }
@vtable.I16.LowerHex = internal constant { ptr } { ptr @tml_N4core3I1613fmt_lower_hexE }
@vtable.I32.LowerHex = internal constant { ptr } { ptr @tml_N4core3I3213fmt_lower_hexE }
@vtable.I64.LowerHex = internal constant { ptr } { ptr @tml_N4core3I6413fmt_lower_hexE }
@vtable.U8.UpperHex = internal constant { ptr } { ptr @tml_N4core2U813fmt_upper_hexE }
@vtable.U16.UpperHex = internal constant { ptr } { ptr @tml_N4core3U1613fmt_upper_hexE }
@vtable.U32.UpperHex = internal constant { ptr } { ptr @tml_N4core3U3213fmt_upper_hexE }
@vtable.U64.UpperHex = internal constant { ptr } { ptr @tml_N4core3U6413fmt_upper_hexE }
@vtable.I8.UpperHex = internal constant { ptr } { ptr @tml_N4core2I813fmt_upper_hexE }
@vtable.I16.UpperHex = internal constant { ptr } { ptr @tml_N4core3I1613fmt_upper_hexE }
@vtable.I32.UpperHex = internal constant { ptr } { ptr @tml_N4core3I3213fmt_upper_hexE }
@vtable.I64.UpperHex = internal constant { ptr } { ptr @tml_N4core3I6413fmt_upper_hexE }
@vtable.F32.LowerExp = internal constant { ptr } { ptr @tml_N4core3F3213fmt_lower_expE }
@vtable.F64.LowerExp = internal constant { ptr } { ptr @tml_N4core3F6413fmt_lower_expE }
@vtable.F32.UpperExp = internal constant { ptr } { ptr @tml_N4core3F3213fmt_upper_expE }
@vtable.F64.UpperExp = internal constant { ptr } { ptr @tml_N4core3F6413fmt_upper_expE }

; Lazy library definitions (only functions actually used)
; DEBUG LAZY type_name=Char method=to_string

define internal ptr @tml_N4core4Char9to_stringE(i32 %this) #0 {
entry:
  %t241 = call ptr @tml_N4core3fmt7helpers11char_to_strE_c(i32 %this)
  ret ptr %t241
}
; DEBUG LAZY type_name=U8 method=fmt_lower_hex

define internal ptr @tml_N4core2U813fmt_lower_hexE(i8 %this) #0 {
entry:
  %t242 = zext i8 %this to i64
  %t243 = call ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %t242, i1 0)
  %t244 = add i64 0, 2
  %t245 = call i64 @strlen(ptr %t243)
  %t246 = add i64 %t244, %t245
  %t247 = add i64 %t246, 1
  %t248 = call ptr @malloc(i64 %t247)
  call void @llvm.memcpy.p0.p0.i64(ptr %t248, ptr @.str.42, i64 2, i1 false)
  %t249 = getelementptr i8, ptr %t248, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t249, ptr %t243, i64 %t245, i1 false)
  %t250 = getelementptr i8, ptr %t248, i64 %t246
  store i8 0, ptr %t250
  call void @tml_str_free(ptr %t243)
  ret ptr %t248
}
; DEBUG LAZY type_name=F32 method=default

define internal float @tml_N4core3F327defaultE() #0 {
entry:
  ret float 0.0
}

; ir_diff::parser::parse
define %struct.Outcome__IrModule__ParseError @tml_N7ir_diff6parser5parseE_S(ptr %input) #0 {
entry:
  %t251 = alloca ptr
  store ptr %input, ptr %t251
  %t254 = alloca %struct.List__Str
  %t256 = alloca %struct.List__IrFunction
  %t258 = alloca %struct.List__IrGlobal
  %t260 = alloca %struct.List__Str
  %t263 = alloca i64
  %t264 = alloca i64
  %t279 = alloca i64
  %t290 = alloca %struct.Maybe__tuple_Str_Str
  %t322 = alloca %struct.Maybe__IrGlobal
  %t347 = alloca %struct.Maybe__FnHeader
  %t369 = alloca %struct.FnHeader
  %t378 = alloca %struct.List__IrParam
  %t381 = alloca %struct.List__Str
  %t383 = alloca %struct.List__IrBlock
  %t386 = alloca %struct.List__IrInstr
  %t387 = alloca i1
  %t392 = alloca i1
  %t252 = load ptr, ptr %t251
  %t253 = call %struct.List__Str @tml_N4core3str5split5linesE_S(ptr %t252)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t254)
  store %struct.List__Str %t253, ptr %t254
  %t255 = call %struct.List__IrFunction @tml_N3std11collections4list16List__IrFunction3newE(i64 8)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t256)
  store %struct.List__IrFunction %t255, ptr %t256
  %t257 = call %struct.List__IrGlobal @tml_N3std11collections4list14List__IrGlobal3newE(i64 4)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t258)
  store %struct.List__IrGlobal %t257, ptr %t258
  %t259 = call %struct.List__Str @tml_N3std11collections4list9List__Str3newE(i64 4)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t260)
  store %struct.List__Str %t259, ptr %t260
  %t261 = load %struct.List__Str, ptr %t254
  %t262 = call i64 @tml_N3std11collections4list9List__Str3lenE(ptr %t254)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t263)
  store i64 %t262, ptr %t263
  call void @llvm.lifetime.start.p0(i64 8, ptr %t264)
  store i64 0, ptr %t264
  br label %loop.preheader126
loop.preheader126:
  br label %loop.header127
loop.header127:
  %t265 = load i64, ptr %t264
  %t266 = load i64, ptr %t263
  %t267 = icmp slt i64 %t265, %t266
  br i1 %t267, label %loop.body128, label %loop.exit130
loop.body128:
  %t268 = load %struct.List__Str, ptr %t254
  %t269 = load i64, ptr %t264
  %t270 = call ptr @tml_N3std11collections4list9List__Str3getE(ptr %t254, i64 %t269)
  %t271 = alloca ptr
  store ptr %t270, ptr %t271
  %t272 = load ptr, ptr %t271
  %t273 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t272)
  %t274 = alloca ptr
  store ptr %t273, ptr %t274
  %t275 = load i64, ptr %t264
  %t277 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t275, i64 1)
  %t276 = extractvalue { i64, i1 } %t277, 0
  %t278 = extractvalue { i64, i1 } %t277, 1
  br i1 %t278, label %add_overflow132, label %add_ok131
add_overflow132:
  call void @panic(ptr @.str.43)
  unreachable
add_ok131:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t279)
  store i64 %t276, ptr %t279
  %t280 = load ptr, ptr %t274
  %t281 = call i1 @tml_N7ir_diff6parser21is_skippable_preambleE_S(ptr %t280)
  br i1 %t281, label %if.then133, label %if.end135
if.then133:
  %t282 = load i64, ptr %t264
  %t284 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t282, i64 1)
  %t283 = extractvalue { i64, i1 } %t284, 0
  %t285 = extractvalue { i64, i1 } %t284, 1
  br i1 %t285, label %add_overflow137, label %add_ok136
add_overflow137:
  call void @panic(ptr @.str.44)
  unreachable
add_ok136:
  store i64 %t283, ptr %t264
  br label %loop.latch129
if.end135:
  %t286 = load ptr, ptr %t274
  %t287 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t286, ptr @.str.45)
  br i1 %t287, label %if.then138, label %if.end140
if.then138:
  %t288 = load ptr, ptr %t274
  %t289 = call %struct.Maybe__tuple_Str_Str @tml_N4core3str5split10split_onceE_SS(ptr %t288, ptr @.str.46)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t290)
  store %struct.Maybe__tuple_Str_Str %t289, ptr %t290
  %t291 = load %struct.Maybe__tuple_Str_Str, ptr %t290
  %t292 = extractvalue %struct.Maybe__tuple_Str_Str %t291, 0
  %t293 = icmp eq i32 %t292, 0
  br i1 %t293, label %if.then141, label %if.else142
if.then141:
  %t294 = load %struct.Maybe__tuple_Str_Str, ptr %t290
  %t295 = extractvalue %struct.Maybe__tuple_Str_Str %t294, 0
  %t296 = alloca %struct.Maybe__tuple_Str_Str
  store %struct.Maybe__tuple_Str_Str %t294, ptr %t296
  %t297 = getelementptr inbounds %struct.Maybe__tuple_Str_Str, ptr %t296, i32 0, i32 1
  %t298 = load { ptr, ptr }, ptr %t297
  %t299 = alloca { ptr, ptr }
  store { ptr, ptr } %t298, ptr %t299
  %t300 = getelementptr inbounds { ptr, ptr }, ptr %t299, i32 0, i32 0
  %t301 = load ptr, ptr %t300
  %t302 = alloca ptr
  store ptr %t301, ptr %t302
  %t303 = getelementptr inbounds { ptr, ptr }, ptr %t299, i32 0, i32 1
  %t304 = load ptr, ptr %t303
  %t305 = alloca ptr
  store ptr %t304, ptr %t305
  %t306 = load %struct.List__Str, ptr %t260
  %t307 = load ptr, ptr %t302
  %t308 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t307)
  %t309 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t260, ptr %t308)
  br label %if.end143
if.else142:
  %t310 = load %struct.List__Str, ptr %t260
  %t311 = load ptr, ptr %t274
  %t312 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t260, ptr %t311)
  br label %if.end143
if.end143:
  %t313 = phi {} [ %t309, %if.then141 ], [ %t312, %if.else142 ]
  %t314 = load i64, ptr %t264
  %t316 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t314, i64 1)
  %t315 = extractvalue { i64, i1 } %t316, 0
  %t317 = extractvalue { i64, i1 } %t316, 1
  br i1 %t317, label %add_overflow145, label %add_ok144
add_overflow145:
  call void @panic(ptr @.str.47)
  unreachable
add_ok144:
  store i64 %t315, ptr %t264
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t290)
  br label %loop.latch129
if.end140:
  %t318 = load ptr, ptr %t274
  %t319 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t318, ptr @.str.48)
  br i1 %t319, label %if.then146, label %if.end148
if.then146:
  %t320 = load ptr, ptr %t274
  %t321 = call %struct.Maybe__IrGlobal @tml_N7ir_diff6parser12parse_globalE_S(ptr %t320)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t322)
  store %struct.Maybe__IrGlobal %t321, ptr %t322
  %t323 = load %struct.Maybe__IrGlobal, ptr %t322
  %t324 = extractvalue %struct.Maybe__IrGlobal %t323, 0
  %t325 = icmp eq i32 %t324, 0
  br i1 %t325, label %if.then149, label %if.end151
if.then149:
  %t326 = load %struct.List__IrGlobal, ptr %t258
  %t327 = load %struct.Maybe__IrGlobal, ptr %t322
  %t328 = extractvalue %struct.Maybe__IrGlobal %t327, 0
  %t329 = alloca %struct.Maybe__IrGlobal
  store %struct.Maybe__IrGlobal %t327, ptr %t329
  %t330 = getelementptr inbounds %struct.Maybe__IrGlobal, ptr %t329, i32 0, i32 1
  %t331 = load %struct.IrGlobal, ptr %t330
  %t332 = call {} @tml_N3std11collections4list14List__IrGlobal14push__IrGlobalE(ptr %t258, %struct.IrGlobal %t331)
  br label %if.end151
if.end151:
  %t333 = load i64, ptr %t264
  %t335 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t333, i64 1)
  %t334 = extractvalue { i64, i1 } %t335, 0
  %t336 = extractvalue { i64, i1 } %t335, 1
  br i1 %t336, label %add_overflow153, label %add_ok152
add_overflow153:
  call void @panic(ptr @.str.49)
  unreachable
add_ok152:
  store i64 %t334, ptr %t264
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t322)
  br label %loop.latch129
if.end148:
  %t337 = load ptr, ptr %t274
  %t338 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t337, ptr @.str.50)
  br i1 %t338, label %if.then154, label %if.end156
if.then154:
  %t339 = load i64, ptr %t264
  %t341 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t339, i64 1)
  %t340 = extractvalue { i64, i1 } %t341, 0
  %t342 = extractvalue { i64, i1 } %t341, 1
  br i1 %t342, label %add_overflow158, label %add_ok157
add_overflow158:
  call void @panic(ptr @.str.51)
  unreachable
add_ok157:
  store i64 %t340, ptr %t264
  br label %loop.latch129
if.end156:
  %t343 = load ptr, ptr %t274
  %t344 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t343, ptr @.str.52)
  br i1 %t344, label %if.then159, label %if.end161
if.then159:
  %t345 = load ptr, ptr %t274
  %t346 = call %struct.Maybe__FnHeader @tml_N7ir_diff6parser21parse_function_headerE_S(ptr %t345)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t347)
  store %struct.Maybe__FnHeader %t346, ptr %t347
  %t348 = load %struct.Maybe__FnHeader, ptr %t347
  %t349 = extractvalue %struct.Maybe__FnHeader %t348, 0
  %t350 = icmp eq i32 %t349, 1
  br i1 %t350, label %if.then162, label %if.end164
if.then162:
  %t352 = alloca %struct.Outcome__IrModule__ParseError, align 8
  %t353 = getelementptr inbounds %struct.Outcome__IrModule__ParseError, ptr %t352, i32 0, i32 0
  store i32 1, ptr %t353
  %t354 = load i64, ptr %t279
  %t355 = insertvalue %struct.ParseError undef, i64 %t354, 0
  %t356 = insertvalue %struct.ParseError %t355, i64 0, 1
  %t357 = insertvalue %struct.ParseError %t356, ptr @.str.53, 2
  %t358 = getelementptr inbounds %struct.Outcome__IrModule__ParseError, ptr %t352, i32 0, i32 1
  %t359 = bitcast ptr %t358 to ptr
  store %struct.ParseError %t357, ptr %t359
  %t351 = load %struct.Outcome__IrModule__ParseError, ptr %t352
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t347)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t279)
  %t360 = load %struct.List__Str, ptr %t260
  call void @tml_N3std11collections4list9List__Str4dropE(ptr %t260)
  %t361 = load %struct.List__IrGlobal, ptr %t258
  call void @tml_N3std11collections4list14List__IrGlobal4dropE(ptr %t258)
  %t362 = load %struct.List__IrFunction, ptr %t256
  call void @tml_N3std11collections4list16List__IrFunction4dropE(ptr %t256)
  %t363 = load %struct.List__Str, ptr %t254
  call void @tml_N3std11collections4list9List__Str4dropE(ptr %t254)
  ret %struct.Outcome__IrModule__ParseError %t351
if.end164:
  %t364 = load %struct.Maybe__FnHeader, ptr %t347
  %t365 = extractvalue %struct.Maybe__FnHeader %t364, 0
  %t366 = alloca %struct.Maybe__FnHeader
  store %struct.Maybe__FnHeader %t364, ptr %t366
  %t367 = getelementptr inbounds %struct.Maybe__FnHeader, ptr %t366, i32 0, i32 1
  %t368 = load %struct.FnHeader, ptr %t367
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t369)
  store %struct.FnHeader %t368, ptr %t369
  %t370 = getelementptr inbounds %struct.FnHeader, ptr %t369, i32 0, i32 0
  %t371 = load ptr, ptr %t370
  %t372 = alloca ptr
  store ptr %t371, ptr %t372
  %t373 = getelementptr inbounds %struct.FnHeader, ptr %t369, i32 0, i32 1
  %t374 = load ptr, ptr %t373
  %t375 = alloca ptr
  store ptr %t374, ptr %t375
  %t376 = getelementptr inbounds %struct.FnHeader, ptr %t369, i32 0, i32 2
  %t377 = load %struct.List__IrParam, ptr %t376
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t378)
  store %struct.List__IrParam %t377, ptr %t378
  %t379 = getelementptr inbounds %struct.FnHeader, ptr %t369, i32 0, i32 3
  %t380 = load %struct.List__Str, ptr %t379
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t381)
  store %struct.List__Str %t380, ptr %t381
  %t382 = call %struct.List__IrBlock @tml_N3std11collections4list13List__IrBlock3newE(i64 4)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t383)
  store %struct.List__IrBlock %t382, ptr %t383
  %t384 = alloca ptr
  store ptr @.str.1, ptr %t384
  %t385 = call %struct.List__IrInstr @tml_N3std11collections4list13List__IrInstr3newE(i64 8)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t386)
  store %struct.List__IrInstr %t385, ptr %t386
  call void @llvm.lifetime.start.p0(i64 1, ptr %t387)
  store i1 0, ptr %t387
  %t388 = load i64, ptr %t264
  %t390 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t388, i64 1)
  %t389 = extractvalue { i64, i1 } %t390, 0
  %t391 = extractvalue { i64, i1 } %t390, 1
  br i1 %t391, label %add_overflow166, label %add_ok165
add_overflow166:
  call void @panic(ptr @.str.54)
  unreachable
add_ok165:
  store i64 %t389, ptr %t264
  call void @llvm.lifetime.start.p0(i64 1, ptr %t392)
  store i1 0, ptr %t392
  br label %loop.preheader167
loop.preheader167:
  br label %loop.header168
loop.header168:
  %t393 = load i64, ptr %t264
  %t394 = load i64, ptr %t263
  %t395 = icmp slt i64 %t393, %t394
  br i1 %t395, label %loop.body169, label %loop.exit171
loop.body169:
  %t396 = load %struct.List__Str, ptr %t254
  %t397 = load i64, ptr %t264
  %t398 = call ptr @tml_N3std11collections4list9List__Str3getE(ptr %t254, i64 %t397)
  %t399 = alloca ptr
  store ptr %t398, ptr %t399
  %t400 = load ptr, ptr %t399
  %t401 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t400)
  %t402 = alloca ptr
  store ptr %t401, ptr %t402
  %t403 = load ptr, ptr %t402
  %t405 = call i32 @strcmp(ptr %t403, ptr @.str.55)
  %t404 = icmp eq i32 %t405, 0
  br i1 %t404, label %if.then172, label %if.end174
if.then172:
  store i1 1, ptr %t392
  %t406 = load i64, ptr %t264
  %t408 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t406, i64 1)
  %t407 = extractvalue { i64, i1 } %t408, 0
  %t409 = extractvalue { i64, i1 } %t408, 1
  br i1 %t409, label %add_overflow176, label %add_ok175
add_overflow176:
  call void @panic(ptr @.str.56)
  unreachable
add_ok175:
  store i64 %t407, ptr %t264
  br label %loop.exit171
if.end174:
  %t410 = load ptr, ptr %t402
  %t411 = call i64 @strlen(ptr %t410)
  %t413 = sext i32 0 to i64
  %t412 = icmp eq i64 %t411, %t413
  br i1 %t412, label %if.then177, label %if.end179
if.then177:
  %t414 = load i64, ptr %t264
  %t416 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t414, i64 1)
  %t415 = extractvalue { i64, i1 } %t416, 0
  %t417 = extractvalue { i64, i1 } %t416, 1
  br i1 %t417, label %add_overflow181, label %add_ok180
add_overflow181:
  call void @panic(ptr @.str.57)
  unreachable
add_ok180:
  store i64 %t415, ptr %t264
  br label %loop.latch170
if.end179:
  %t418 = load ptr, ptr %t402
  %t419 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t418, ptr @.str.58)
  br i1 %t419, label %if.then182, label %if.end184
if.then182:
  %t420 = load i64, ptr %t264
  %t422 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t420, i64 1)
  %t421 = extractvalue { i64, i1 } %t422, 0
  %t423 = extractvalue { i64, i1 } %t422, 1
  br i1 %t423, label %add_overflow186, label %add_ok185
add_overflow186:
  call void @panic(ptr @.str.59)
  unreachable
add_ok185:
  store i64 %t421, ptr %t264
  br label %loop.latch170
if.end184:
  %t424 = load ptr, ptr %t402
  %t425 = call i1 @tml_N7ir_diff6parser13is_label_lineE_S(ptr %t424)
  br i1 %t425, label %if.then187, label %if.end189
if.then187:
  %t426 = load i1, ptr %t387
  br i1 %t426, label %if.then190, label %if.end192
if.then190:
  %t427 = load %struct.List__IrBlock, ptr %t383
  %t428 = load ptr, ptr %t384
  %t429 = insertvalue %struct.IrBlock undef, ptr %t428, 0
  %t430 = load %struct.List__IrInstr, ptr %t386
  %t431 = insertvalue %struct.IrBlock %t429, %struct.List__IrInstr %t430, 1
  %t432 = call {} @tml_N3std11collections4list13List__IrBlock13push__IrBlockE(ptr %t383, %struct.IrBlock %t431)
  br label %if.end192
if.end192:
  %t433 = load ptr, ptr %t402
  %t434 = sext i32 0 to i64
  %t435 = load ptr, ptr %t402
  %t436 = call i64 @strlen(ptr %t435)
  %t438 = sext i32 1 to i64
  %t439 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t436, i64 %t438)
  %t437 = extractvalue { i64, i1 } %t439, 0
  %t440 = extractvalue { i64, i1 } %t439, 1
  br i1 %t440, label %sub_overflow194, label %sub_ok193
sub_overflow194:
  call void @panic(ptr @.str.60)
  unreachable
sub_ok193:
  %t441 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t433, i64 %t434, i64 %t437)
  store ptr %t441, ptr %t384
  %t442 = call %struct.List__IrInstr @tml_N3std11collections4list13List__IrInstr3newE(i64 8)
  store %struct.List__IrInstr %t442, ptr %t386
  store i1 1, ptr %t387
  %t443 = load i64, ptr %t264
  %t445 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t443, i64 1)
  %t444 = extractvalue { i64, i1 } %t445, 0
  %t446 = extractvalue { i64, i1 } %t445, 1
  br i1 %t446, label %add_overflow196, label %add_ok195
add_overflow196:
  call void @panic(ptr @.str.61)
  unreachable
add_ok195:
  store i64 %t444, ptr %t264
  br label %loop.latch170
if.end189:
  %t447 = load i1, ptr %t387
  %t448 = xor i1 %t447, 1
  br i1 %t448, label %if.then197, label %if.end199
if.then197:
  store ptr @.str.62, ptr %t384
  %t449 = call %struct.List__IrInstr @tml_N3std11collections4list13List__IrInstr3newE(i64 8)
  store %struct.List__IrInstr %t449, ptr %t386
  store i1 1, ptr %t387
  br label %if.end199
if.end199:
  %t450 = load ptr, ptr %t402
  %t451 = call ptr @tml_N7ir_diff6parser23strip_trailing_metadataE_S(ptr %t450)
  %t452 = alloca ptr
  store ptr %t451, ptr %t452
  %t453 = load %struct.List__IrInstr, ptr %t386
  %t454 = load ptr, ptr %t452
  %t455 = call %struct.IrInstr @tml_N7ir_diff6parser17parse_instructionE_S(ptr %t454)
  %t456 = call {} @tml_N3std11collections4list13List__IrInstr13push__IrInstrE(ptr %t386, %struct.IrInstr %t455)
  %t457 = load i64, ptr %t264
  %t459 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t457, i64 1)
  %t458 = extractvalue { i64, i1 } %t459, 0
  %t460 = extractvalue { i64, i1 } %t459, 1
  br i1 %t460, label %add_overflow201, label %add_ok200
add_overflow201:
  call void @panic(ptr @.str.63)
  unreachable
add_ok200:
  store i64 %t458, ptr %t264
  %t461 = load ptr, ptr %t402
  call void @tml_str_free(ptr %t461)
  br label %loop.latch170
loop.latch170:
  br label %loop.header168, !llvm.loop !1008
loop.exit171:
  %t462 = load i1, ptr %t392
  %t463 = xor i1 %t462, 1
  br i1 %t463, label %if.then202, label %if.end204
if.then202:
  %t465 = alloca %struct.Outcome__IrModule__ParseError, align 8
  %t466 = getelementptr inbounds %struct.Outcome__IrModule__ParseError, ptr %t465, i32 0, i32 0
  store i32 1, ptr %t466
  %t467 = load i64, ptr %t279
  %t468 = insertvalue %struct.ParseError undef, i64 %t467, 0
  %t469 = insertvalue %struct.ParseError %t468, i64 0, 1
  %t470 = insertvalue %struct.ParseError %t469, ptr @.str.64, 2
  %t471 = getelementptr inbounds %struct.Outcome__IrModule__ParseError, ptr %t465, i32 0, i32 1
  %t472 = bitcast ptr %t471 to ptr
  store %struct.ParseError %t470, ptr %t472
  %t464 = load %struct.Outcome__IrModule__ParseError, ptr %t465
  call void @llvm.lifetime.end.p0(i64 1, ptr %t392)
  call void @llvm.lifetime.end.p0(i64 1, ptr %t387)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t386)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t383)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t381)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t378)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t369)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t347)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t279)
  %t473 = load %struct.List__IrBlock, ptr %t383
  call void @tml_N3std11collections4list13List__IrBlock4dropE(ptr %t383)
  %t474 = load %struct.List__Str, ptr %t381
  call void @tml_N3std11collections4list9List__Str4dropE(ptr %t381)
  %t475 = load %struct.List__IrParam, ptr %t378
  call void @tml_N3std11collections4list13List__IrParam4dropE(ptr %t378)
  %t476 = load %struct.List__Str, ptr %t260
  call void @tml_N3std11collections4list9List__Str4dropE(ptr %t260)
  %t477 = load %struct.List__IrGlobal, ptr %t258
  call void @tml_N3std11collections4list14List__IrGlobal4dropE(ptr %t258)
  %t478 = load %struct.List__IrFunction, ptr %t256
  call void @tml_N3std11collections4list16List__IrFunction4dropE(ptr %t256)
  %t479 = load %struct.List__Str, ptr %t254
  call void @tml_N3std11collections4list9List__Str4dropE(ptr %t254)
  ret %struct.Outcome__IrModule__ParseError %t464
if.end204:
  %t480 = load i1, ptr %t387
  br i1 %t480, label %if.then205, label %if.end207
if.then205:
  %t481 = load %struct.List__IrBlock, ptr %t383
  %t482 = load ptr, ptr %t384
  %t483 = insertvalue %struct.IrBlock undef, ptr %t482, 0
  %t484 = load %struct.List__IrInstr, ptr %t386
  %t485 = insertvalue %struct.IrBlock %t483, %struct.List__IrInstr %t484, 1
  %t486 = call {} @tml_N3std11collections4list13List__IrBlock13push__IrBlockE(ptr %t383, %struct.IrBlock %t485)
  br label %if.end207
if.end207:
  %t487 = load %struct.List__IrFunction, ptr %t256
  %t488 = load ptr, ptr %t372
  %t489 = insertvalue %struct.IrFunction undef, ptr %t488, 0
  %t490 = load ptr, ptr %t375
  %t491 = insertvalue %struct.IrFunction %t489, ptr %t490, 1
  %t492 = load %struct.List__IrParam, ptr %t378
  %t493 = insertvalue %struct.IrFunction %t491, %struct.List__IrParam %t492, 2
  %t494 = load %struct.List__IrBlock, ptr %t383
  %t495 = insertvalue %struct.IrFunction %t493, %struct.List__IrBlock %t494, 3
  %t496 = load %struct.List__Str, ptr %t381
  %t497 = insertvalue %struct.IrFunction %t495, %struct.List__Str %t496, 4
  %t498 = call {} @tml_N3std11collections4list16List__IrFunction16push__IrFunctionE(ptr %t256, %struct.IrFunction %t497)
  call void @llvm.lifetime.end.p0(i64 1, ptr %t392)
  call void @llvm.lifetime.end.p0(i64 1, ptr %t387)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t386)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t383)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t381)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t378)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t369)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t347)
  br label %loop.latch129
if.end161:
  %t499 = load i64, ptr %t264
  %t501 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t499, i64 1)
  %t500 = extractvalue { i64, i1 } %t501, 0
  %t502 = extractvalue { i64, i1 } %t501, 1
  br i1 %t502, label %add_overflow209, label %add_ok208
add_overflow209:
  call void @panic(ptr @.str.65)
  unreachable
add_ok208:
  store i64 %t500, ptr %t264
  call void @llvm.lifetime.end.p0(i64 8, ptr %t279)
  br label %loop.latch129
loop.latch129:
  br label %loop.header127, !llvm.loop !1007
loop.exit130:
  %t504 = alloca %struct.Outcome__IrModule__ParseError, align 8
  %t505 = getelementptr inbounds %struct.Outcome__IrModule__ParseError, ptr %t504, i32 0, i32 0
  store i32 0, ptr %t505
  %t506 = load %struct.List__IrFunction, ptr %t256
  %t507 = insertvalue %struct.IrModule undef, %struct.List__IrFunction %t506, 0
  %t508 = load %struct.List__IrGlobal, ptr %t258
  %t509 = insertvalue %struct.IrModule %t507, %struct.List__IrGlobal %t508, 1
  %t510 = load %struct.List__Str, ptr %t260
  %t511 = insertvalue %struct.IrModule %t509, %struct.List__Str %t510, 2
  %t512 = getelementptr inbounds %struct.Outcome__IrModule__ParseError, ptr %t504, i32 0, i32 1
  %t513 = bitcast ptr %t512 to ptr
  store %struct.IrModule %t511, ptr %t513
  %t503 = load %struct.Outcome__IrModule__ParseError, ptr %t504
  %t514 = load %struct.List__Str, ptr %t254
  call void @tml_N3std11collections4list9List__Str4dropE(ptr %t254)
  ret %struct.Outcome__IrModule__ParseError %t503
}
; DEBUG LAZY type_name=U32 method=default

define internal i32 @tml_N4core3U327defaultE() #0 {
entry:
  ret i32 0
}
; DEBUG LAZY type_name=I16 method=to_string

define internal ptr @tml_N4core3I169to_stringE(i16 %this) #0 {
entry:
  %t515 = call ptr @tml_N4core3fmt7helpers10i16_to_strE_s(i16 %this)
  ret ptr %t515
}
; DEBUG LAZY type_name=I8 method=to_string

define internal ptr @tml_N4core2I89to_stringE(i8 %this) #0 {
entry:
  %t516 = call ptr @tml_N4core3fmt7helpers9i8_to_strE_a(i8 %this)
  ret ptr %t516
}
; DEBUG LAZY type_name=U8 method=default

define internal i8 @tml_N4core2U87defaultE() #0 {
entry:
  %t517 = trunc i32 0 to i8
  ret i8 %t517
}
; DEBUG LAZY type_name=I32 method=default

define internal i32 @tml_N4core3I327defaultE() #0 {
entry:
  ret i32 0
}
; DEBUG LAZY type_name=I8 method=debug_string

define internal ptr @tml_N4core2I812debug_stringE(i8 %this) #0 {
entry:
  %t518 = call ptr @tml_N4core3fmt7helpers9i8_to_strE_a(i8 %this)
  ret ptr %t518
}
; DEBUG LAZY type_name=I16 method=debug_string

define internal ptr @tml_N4core3I1612debug_stringE(i16 %this) #0 {
entry:
  %t519 = call ptr @tml_N4core3fmt7helpers10i16_to_strE_s(i16 %this)
  ret ptr %t519
}
; DEBUG LAZY type_name=I32 method=debug_string

define internal ptr @tml_N4core3I3212debug_stringE(i32 %this) #0 {
entry:
  %t520 = call ptr @tml_N4core3fmt7helpers10i32_to_strE_i(i32 %this)
  ret ptr %t520
}
; DEBUG LAZY type_name=F64 method=to_string

define internal ptr @tml_N4core3F649to_stringE(double %this) #0 {
entry:
  %t521 = call ptr @f64_to_string(double %this)
  ret ptr %t521
}
; DEBUG LAZY type_name=U16 method=fmt_octal

define internal ptr @tml_N4core3U169fmt_octalE(i16 %this) #0 {
entry:
  %t522 = zext i16 %this to i64
  %t523 = call ptr @tml_N4core3fmt7helpers16u64_to_octal_strE_m(i64 %t522)
  %t524 = add i64 0, 2
  %t525 = call i64 @strlen(ptr %t523)
  %t526 = add i64 %t524, %t525
  %t527 = add i64 %t526, 1
  %t528 = call ptr @malloc(i64 %t527)
  call void @llvm.memcpy.p0.p0.i64(ptr %t528, ptr @.str.66, i64 2, i1 false)
  %t529 = getelementptr i8, ptr %t528, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t529, ptr %t523, i64 %t525, i1 false)
  %t530 = getelementptr i8, ptr %t528, i64 %t526
  store i8 0, ptr %t530
  call void @tml_str_free(ptr %t523)
  ret ptr %t528
}

; core::str::search::starts_with
define i1 @tml_N4core3str6search11starts_withE_SS(ptr %s, ptr %prefix) #0 {
entry:
  %t531 = alloca ptr
  store ptr %s, ptr %t531
  %t532 = alloca ptr
  store ptr %prefix, ptr %t532
  %t535 = alloca i64
  %t538 = alloca i64
  %t533 = load ptr, ptr %t531
  %t534 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t533)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t535)
  store i64 %t534, ptr %t535
  %t536 = load ptr, ptr %t532
  %t537 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t536)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t538)
  store i64 %t537, ptr %t538
  %t539 = load i64, ptr %t538
  %t540 = load i64, ptr %t535
  %t541 = icmp sgt i64 %t539, %t540
  br i1 %t541, label %if.then210, label %if.end212
if.then210:
  ret i1 0
if.end212:
  %t542 = load i64, ptr %t538
  %t544 = sext i32 0 to i64
  %t543 = icmp eq i64 %t542, %t544
  br i1 %t543, label %if.then213, label %if.end215
if.then213:
  ret i1 1
if.end215:
  %t545 = load ptr, ptr %t531
  %t546 = load ptr, ptr %t532
  %t547 = load i64, ptr %t538
  %t548 = call i32 @memcmp(ptr %t545, ptr %t546, i64 %t547)
  %t549 = icmp eq i32 %t548, 0
  ret i1 %t549
}
; DEBUG LAZY type_name=U16 method=to_string

define internal ptr @tml_N4core3U169to_stringE(i16 %this) #0 {
entry:
  %t550 = call ptr @tml_N4core3fmt7helpers10u16_to_strE_t(i16 %this)
  ret ptr %t550
}

; ir_diff::differ::diff
define %struct.Outcome__DiffResult__DiffError @tml_N7ir_diff6differ4diffE_R8IrModuleR8IrModule(ptr %a, ptr %b) #0 {
entry:
  %t551 = alloca ptr
  store ptr %a, ptr %t551
  %t552 = alloca ptr
  store ptr %b, ptr %t552
  %t554 = alloca %struct.List__Str
  %t556 = alloca %struct.List__Str
  %t558 = alloca %struct.List__FunctionDiff
  %t565 = alloca %struct.List__I64
  %t566 = alloca i64
  %t572 = alloca i64
  %t582 = alloca %struct.IrFunction
  %t587 = alloca i64
  %t614 = alloca %struct.IrFunction
  %t616 = alloca %struct.List__Str
  %t618 = alloca %struct.List__Str
  %t622 = alloca %struct.Maybe__FunctionDiff
  %t639 = alloca i64
  %t645 = alloca i64
  %t553 = call %struct.List__Str @tml_N3std11collections4list9List__Str3newE(i64 4)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t554)
  store %struct.List__Str %t553, ptr %t554
  %t555 = call %struct.List__Str @tml_N3std11collections4list9List__Str3newE(i64 4)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t556)
  store %struct.List__Str %t555, ptr %t556
  %t557 = call %struct.List__FunctionDiff @tml_N3std11collections4list18List__FunctionDiff3newE(i64 4)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t558)
  store %struct.List__FunctionDiff %t557, ptr %t558
  %t559 = load ptr, ptr %t552
  %t560 = getelementptr inbounds %struct.IrModule, ptr %t559, i32 0, i32 0
  %t561 = load %struct.List__IrFunction, ptr %t560
  %t562 = alloca %struct.List__IrFunction
  store %struct.List__IrFunction %t561, ptr %t562
  %t563 = call i64 @tml_N3std11collections4list16List__IrFunction3lenE(ptr %t562)
  %t564 = call %struct.List__I64 @tml_N3std11collections4list9List__I643newE(i64 %t563)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t565)
  store %struct.List__I64 %t564, ptr %t565
  call void @llvm.lifetime.start.p0(i64 8, ptr %t566)
  store i64 0, ptr %t566
  %t567 = load ptr, ptr %t551
  %t568 = getelementptr inbounds %struct.IrModule, ptr %t567, i32 0, i32 0
  %t569 = load %struct.List__IrFunction, ptr %t568
  %t570 = alloca %struct.List__IrFunction
  store %struct.List__IrFunction %t569, ptr %t570
  %t571 = call i64 @tml_N3std11collections4list16List__IrFunction3lenE(ptr %t570)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t572)
  store i64 %t571, ptr %t572
  br label %loop.preheader216
loop.preheader216:
  br label %loop.header217
loop.header217:
  %t573 = load i64, ptr %t566
  %t574 = load i64, ptr %t572
  %t575 = icmp slt i64 %t573, %t574
  br i1 %t575, label %loop.body218, label %loop.exit220
loop.body218:
  %t576 = load ptr, ptr %t551
  %t577 = getelementptr inbounds %struct.IrModule, ptr %t576, i32 0, i32 0
  %t578 = load %struct.List__IrFunction, ptr %t577
  %t579 = alloca %struct.List__IrFunction
  store %struct.List__IrFunction %t578, ptr %t579
  %t580 = load i64, ptr %t566
  %t581 = call %struct.IrFunction @tml_N3std11collections4list16List__IrFunction3getE(ptr %t579, i64 %t580)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t582)
  store %struct.IrFunction %t581, ptr %t582
  %t583 = load ptr, ptr %t552
  %t584 = getelementptr inbounds %struct.IrFunction, ptr %t582, i32 0, i32 0
  %t585 = load ptr, ptr %t584
  %t586 = call i64 @tml_N7ir_diff6differ10find_exactE_R8IrModuleS(ptr %t583, ptr %t585)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t587)
  store i64 %t586, ptr %t587
  %t588 = load i64, ptr %t587
  %t590 = sext i32 0 to i64
  %t589 = icmp slt i64 %t588, %t590
  br i1 %t589, label %if.then221, label %if.end223
if.then221:
  %t591 = getelementptr inbounds %struct.IrFunction, ptr %t582, i32 0, i32 0
  %t592 = load ptr, ptr %t591
  %t593 = call ptr @tml_N7ir_diff6differ13demangle_nameE_S(ptr %t592)
  %t594 = alloca ptr
  store ptr %t593, ptr %t594
  %t595 = load ptr, ptr %t552
  %t596 = load ptr, ptr %t594
  %t597 = call i64 @tml_N7ir_diff6differ14find_demangledE_R8IrModuleS(ptr %t595, ptr %t596)
  store i64 %t597, ptr %t587
  br label %if.end223
if.end223:
  %t598 = load i64, ptr %t587
  %t600 = sext i32 0 to i64
  %t599 = icmp slt i64 %t598, %t600
  br i1 %t599, label %if.then224, label %if.else225
if.then224:
  %t601 = load %struct.List__Str, ptr %t554
  %t602 = getelementptr inbounds %struct.IrFunction, ptr %t582, i32 0, i32 0
  %t603 = load ptr, ptr %t602
  %t604 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t554, ptr %t603)
  br label %if.end226
if.else225:
  %t605 = load %struct.List__I64, ptr %t565
  %t606 = load i64, ptr %t587
  %t607 = call {} @tml_N3std11collections4list9List__I649push__I64E(ptr %t565, i64 %t606)
  %t608 = load ptr, ptr %t552
  %t609 = getelementptr inbounds %struct.IrModule, ptr %t608, i32 0, i32 0
  %t610 = load %struct.List__IrFunction, ptr %t609
  %t611 = alloca %struct.List__IrFunction
  store %struct.List__IrFunction %t610, ptr %t611
  %t612 = load i64, ptr %t587
  %t613 = call %struct.IrFunction @tml_N3std11collections4list16List__IrFunction3getE(ptr %t611, i64 %t612)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t614)
  store %struct.IrFunction %t613, ptr %t614
  %t615 = call %struct.List__Str @tml_N7ir_diff6differ20flatten_instructionsE_R10IrFunction(ptr %t582)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t616)
  store %struct.List__Str %t615, ptr %t616
  %t617 = call %struct.List__Str @tml_N7ir_diff6differ20flatten_instructionsE_R10IrFunction(ptr %t614)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t618)
  store %struct.List__Str %t617, ptr %t618
  %t619 = getelementptr inbounds %struct.IrFunction, ptr %t582, i32 0, i32 0
  %t620 = load ptr, ptr %t619
  %t621 = call %struct.Maybe__FunctionDiff @tml_N7ir_diff6differ22diff_instruction_listsE_SR4ListISER4ListISE(ptr %t620, ptr %t616, ptr %t618)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t622)
  store %struct.Maybe__FunctionDiff %t621, ptr %t622
  %t623 = load %struct.Maybe__FunctionDiff, ptr %t622
  %t624 = extractvalue %struct.Maybe__FunctionDiff %t623, 0
  %t625 = icmp eq i32 %t624, 0
  br i1 %t625, label %if.then227, label %if.end229
if.then227:
  %t626 = load %struct.List__FunctionDiff, ptr %t558
  %t627 = load %struct.Maybe__FunctionDiff, ptr %t622
  %t628 = extractvalue %struct.Maybe__FunctionDiff %t627, 0
  %t629 = alloca %struct.Maybe__FunctionDiff
  store %struct.Maybe__FunctionDiff %t627, ptr %t629
  %t630 = getelementptr inbounds %struct.Maybe__FunctionDiff, ptr %t629, i32 0, i32 1
  %t631 = load %struct.FunctionDiff, ptr %t630
  %t632 = call {} @tml_N3std11collections4list18List__FunctionDiff18push__FunctionDiffE(ptr %t558, %struct.FunctionDiff %t631)
  br label %if.end229
if.end229:
  %t633 = load %struct.List__Str, ptr %t618
  call void @tml_N3std11collections4list9List__Str4dropE(ptr %t618)
  %t634 = load %struct.List__Str, ptr %t616
  call void @tml_N3std11collections4list9List__Str4dropE(ptr %t616)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t622)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t618)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t616)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t614)
  br label %if.end226
if.end226:
  %t635 = load i64, ptr %t566
  %t637 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t635, i64 1)
  %t636 = extractvalue { i64, i1 } %t637, 0
  %t638 = extractvalue { i64, i1 } %t637, 1
  br i1 %t638, label %add_overflow231, label %add_ok230
add_overflow231:
  call void @panic(ptr @.str.67)
  unreachable
add_ok230:
  store i64 %t636, ptr %t566
  call void @llvm.lifetime.end.p0(i64 8, ptr %t587)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t582)
  br label %loop.latch219
loop.latch219:
  br label %loop.header217, !llvm.loop !1009
loop.exit220:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t639)
  store i64 0, ptr %t639
  %t640 = load ptr, ptr %t552
  %t641 = getelementptr inbounds %struct.IrModule, ptr %t640, i32 0, i32 0
  %t642 = load %struct.List__IrFunction, ptr %t641
  %t643 = alloca %struct.List__IrFunction
  store %struct.List__IrFunction %t642, ptr %t643
  %t644 = call i64 @tml_N3std11collections4list16List__IrFunction3lenE(ptr %t643)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t645)
  store i64 %t644, ptr %t645
  br label %loop.preheader232
loop.preheader232:
  br label %loop.header233
loop.header233:
  %t646 = load i64, ptr %t639
  %t647 = load i64, ptr %t645
  %t648 = icmp slt i64 %t646, %t647
  br i1 %t648, label %loop.body234, label %loop.exit236
loop.body234:
  %t649 = load i64, ptr %t639
  %t650 = call i1 @tml_N7ir_diff6differ17list_contains_i64E_R4ListIlEl(ptr %t565, i64 %t649)
  %t651 = xor i1 %t650, 1
  br i1 %t651, label %if.then237, label %if.end239
if.then237:
  %t652 = load %struct.List__Str, ptr %t556
  %t653 = load ptr, ptr %t552
  %t654 = getelementptr inbounds %struct.IrModule, ptr %t653, i32 0, i32 0
  %t655 = load %struct.List__IrFunction, ptr %t654
  %t656 = alloca %struct.List__IrFunction
  store %struct.List__IrFunction %t655, ptr %t656
  %t657 = load i64, ptr %t639
  %t658 = call %struct.IrFunction @tml_N3std11collections4list16List__IrFunction3getE(ptr %t656, i64 %t657)
  %t659 = alloca %struct.IrFunction
  store %struct.IrFunction %t658, ptr %t659
  %t660 = getelementptr inbounds %struct.IrFunction, ptr %t659, i32 0, i32 0
  %t661 = load ptr, ptr %t660
  %t662 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t556, ptr %t661)
  br label %if.end239
if.end239:
  %t663 = load i64, ptr %t639
  %t665 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t663, i64 1)
  %t664 = extractvalue { i64, i1 } %t665, 0
  %t666 = extractvalue { i64, i1 } %t665, 1
  br i1 %t666, label %add_overflow241, label %add_ok240
add_overflow241:
  call void @panic(ptr @.str.68)
  unreachable
add_ok240:
  store i64 %t664, ptr %t639
  br label %loop.latch235
loop.latch235:
  br label %loop.header233, !llvm.loop !1010
loop.exit236:
  %t668 = alloca %struct.Outcome__DiffResult__DiffError, align 8
  %t669 = getelementptr inbounds %struct.Outcome__DiffResult__DiffError, ptr %t668, i32 0, i32 0
  store i32 0, ptr %t669
  %t670 = load %struct.List__Str, ptr %t554
  %t671 = insertvalue %struct.DiffResult undef, %struct.List__Str %t670, 0
  %t672 = load %struct.List__Str, ptr %t556
  %t673 = insertvalue %struct.DiffResult %t671, %struct.List__Str %t672, 1
  %t674 = load %struct.List__FunctionDiff, ptr %t558
  %t675 = insertvalue %struct.DiffResult %t673, %struct.List__FunctionDiff %t674, 2
  %t676 = getelementptr inbounds %struct.Outcome__DiffResult__DiffError, ptr %t668, i32 0, i32 1
  %t677 = bitcast ptr %t676 to ptr
  store %struct.DiffResult %t675, ptr %t677
  %t667 = load %struct.Outcome__DiffResult__DiffError, ptr %t668
  %t678 = load %struct.List__I64, ptr %t565
  call void @tml_N3std11collections4list9List__I644dropE(ptr %t565)
  ret %struct.Outcome__DiffResult__DiffError %t667
}

; ir_diff::differ::is_identical
define i1 @tml_N7ir_diff6differ12is_identicalE_R10DiffResult(ptr %r) #0 {
entry:
  %t679 = alloca ptr
  store ptr %r, ptr %t679
  %t680 = load ptr, ptr %t679
  %t681 = getelementptr inbounds %struct.DiffResult, ptr %t680, i32 0, i32 0
  %t682 = load %struct.List__Str, ptr %t681
  %t683 = alloca %struct.List__Str
  store %struct.List__Str %t682, ptr %t683
  %t684 = call i1 @tml_N3std11collections4list9List__Str8is_emptyE(ptr %t683)
  %t685 = load ptr, ptr %t679
  %t686 = getelementptr inbounds %struct.DiffResult, ptr %t685, i32 0, i32 1
  %t687 = load %struct.List__Str, ptr %t686
  %t688 = alloca %struct.List__Str
  store %struct.List__Str %t687, ptr %t688
  %t689 = call i1 @tml_N3std11collections4list9List__Str8is_emptyE(ptr %t688)
  %t690 = and i1 %t684, %t689
  %t691 = load ptr, ptr %t679
  %t692 = getelementptr inbounds %struct.DiffResult, ptr %t691, i32 0, i32 2
  %t693 = load %struct.List__FunctionDiff, ptr %t692
  %t694 = alloca %struct.List__FunctionDiff
  store %struct.List__FunctionDiff %t693, ptr %t694
  %t695 = call i1 @tml_N3std11collections4list18List__FunctionDiff8is_emptyE(ptr %t694)
  %t696 = and i1 %t690, %t695
  ret i1 %t696
}

; ir_diff::differ::diff_b_line
define ptr @tml_N7ir_diff6differ11diff_b_lineE_R10DiffResultl(ptr %r, i64 %i) #0 {
entry:
  %t697 = alloca ptr
  store ptr %r, ptr %t697
  %t698 = alloca i64
  store i64 %i, ptr %t698
  %t699 = load ptr, ptr %t697
  %t700 = getelementptr inbounds %struct.DiffResult, ptr %t699, i32 0, i32 2
  %t701 = load %struct.List__FunctionDiff, ptr %t700
  %t702 = alloca %struct.List__FunctionDiff
  store %struct.List__FunctionDiff %t701, ptr %t702
  %t703 = load i64, ptr %t698
  %t704 = call %struct.FunctionDiff @tml_N3std11collections4list18List__FunctionDiff3getE(ptr %t702, i64 %t703)
  %t705 = alloca %struct.FunctionDiff
  store %struct.FunctionDiff %t704, ptr %t705
  %t706 = getelementptr inbounds %struct.FunctionDiff, ptr %t705, i32 0, i32 3
  %t707 = load ptr, ptr %t706
  ret ptr %t707
}

; ir_diff::differ::only_in_a_count
define i64 @tml_N7ir_diff6differ15only_in_a_countE_R10DiffResult(ptr %r) #0 {
entry:
  %t708 = alloca ptr
  store ptr %r, ptr %t708
  %t709 = load ptr, ptr %t708
  %t710 = getelementptr inbounds %struct.DiffResult, ptr %t709, i32 0, i32 0
  %t711 = load %struct.List__Str, ptr %t710
  %t712 = alloca %struct.List__Str
  store %struct.List__Str %t711, ptr %t712
  %t713 = call i64 @tml_N3std11collections4list9List__Str3lenE(ptr %t712)
  ret i64 %t713
}
; DEBUG LAZY type_name=U64 method=fmt_binary

define internal ptr @tml_N4core3U6410fmt_binaryE(i64 %this) #0 {
entry:
  %t714 = call ptr @tml_N4core3fmt7helpers17u64_to_binary_strE_m(i64 %this)
  %t715 = add i64 0, 2
  %t716 = call i64 @strlen(ptr %t714)
  %t717 = add i64 %t715, %t716
  %t718 = add i64 %t717, 1
  %t719 = call ptr @malloc(i64 %t718)
  call void @llvm.memcpy.p0.p0.i64(ptr %t719, ptr @.str.69, i64 2, i1 false)
  %t720 = getelementptr i8, ptr %t719, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t720, ptr %t714, i64 %t716, i1 false)
  %t721 = getelementptr i8, ptr %t719, i64 %t717
  store i8 0, ptr %t721
  call void @tml_str_free(ptr %t714)
  ret ptr %t719
}

; ir_diff::differ::only_in_b_count
define i64 @tml_N7ir_diff6differ15only_in_b_countE_R10DiffResult(ptr %r) #0 {
entry:
  %t722 = alloca ptr
  store ptr %r, ptr %t722
  %t723 = load ptr, ptr %t722
  %t724 = getelementptr inbounds %struct.DiffResult, ptr %t723, i32 0, i32 1
  %t725 = load %struct.List__Str, ptr %t724
  %t726 = alloca %struct.List__Str
  store %struct.List__Str %t725, ptr %t726
  %t727 = call i64 @tml_N3std11collections4list9List__Str3lenE(ptr %t726)
  ret i64 %t727
}
; DEBUG LAZY type_name=Str method=to_string

define internal ptr @tml_N4core3Str9to_stringE(ptr %this) #0 {
entry:
  ret ptr %this
}

; ir_diff::differ::diffs_count
define i64 @tml_N7ir_diff6differ11diffs_countE_R10DiffResult(ptr %r) #0 {
entry:
  %t728 = alloca ptr
  store ptr %r, ptr %t728
  %t729 = load ptr, ptr %t728
  %t730 = getelementptr inbounds %struct.DiffResult, ptr %t729, i32 0, i32 2
  %t731 = load %struct.List__FunctionDiff, ptr %t730
  %t732 = alloca %struct.List__FunctionDiff
  store %struct.List__FunctionDiff %t731, ptr %t732
  %t733 = call i64 @tml_N3std11collections4list18List__FunctionDiff3lenE(ptr %t732)
  ret i64 %t733
}
; DEBUG LAZY type_name=U8 method=fmt_binary

define internal ptr @tml_N4core2U810fmt_binaryE(i8 %this) #0 {
entry:
  %t734 = call ptr @tml_N4core3fmt7helpers16u8_to_binary_strE_h(i8 %this)
  %t735 = add i64 0, 2
  %t736 = call i64 @strlen(ptr %t734)
  %t737 = add i64 %t735, %t736
  %t738 = add i64 %t737, 1
  %t739 = call ptr @malloc(i64 %t738)
  call void @llvm.memcpy.p0.p0.i64(ptr %t739, ptr @.str.69, i64 2, i1 false)
  %t740 = getelementptr i8, ptr %t739, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t740, ptr %t734, i64 %t736, i1 false)
  %t741 = getelementptr i8, ptr %t739, i64 %t737
  store i8 0, ptr %t741
  call void @tml_str_free(ptr %t734)
  ret ptr %t739
}

; ir_diff::differ::only_in_a_name
define ptr @tml_N7ir_diff6differ14only_in_a_nameE_R10DiffResultl(ptr %r, i64 %i) #0 {
entry:
  %t742 = alloca ptr
  store ptr %r, ptr %t742
  %t743 = alloca i64
  store i64 %i, ptr %t743
  %t744 = load ptr, ptr %t742
  %t745 = getelementptr inbounds %struct.DiffResult, ptr %t744, i32 0, i32 0
  %t746 = load %struct.List__Str, ptr %t745
  %t747 = alloca %struct.List__Str
  store %struct.List__Str %t746, ptr %t747
  %t748 = load i64, ptr %t743
  %t749 = call ptr @tml_N3std11collections4list9List__Str3getE(ptr %t747, i64 %t748)
  ret ptr %t749
}
; DEBUG LAZY type_name=Str method=default

define internal ptr @tml_N4core3Str7defaultE() #0 {
entry:
  ret ptr @.str.1
}
; DEBUG LAZY type_name=I16 method=fmt_binary

define internal ptr @tml_N4core3I1610fmt_binaryE(i16 %this) #0 {
entry:
  %t750 = call ptr @tml_N4core3fmt7helpers17i16_to_binary_strE_s(i16 %this)
  %t751 = add i64 0, 2
  %t752 = call i64 @strlen(ptr %t750)
  %t753 = add i64 %t751, %t752
  %t754 = add i64 %t753, 1
  %t755 = call ptr @malloc(i64 %t754)
  call void @llvm.memcpy.p0.p0.i64(ptr %t755, ptr @.str.69, i64 2, i1 false)
  %t756 = getelementptr i8, ptr %t755, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t756, ptr %t750, i64 %t752, i1 false)
  %t757 = getelementptr i8, ptr %t755, i64 %t753
  store i8 0, ptr %t757
  call void @tml_str_free(ptr %t750)
  ret ptr %t755
}

; ir_diff::differ::only_in_b_name
define ptr @tml_N7ir_diff6differ14only_in_b_nameE_R10DiffResultl(ptr %r, i64 %i) #0 {
entry:
  %t758 = alloca ptr
  store ptr %r, ptr %t758
  %t759 = alloca i64
  store i64 %i, ptr %t759
  %t760 = load ptr, ptr %t758
  %t761 = getelementptr inbounds %struct.DiffResult, ptr %t760, i32 0, i32 1
  %t762 = load %struct.List__Str, ptr %t761
  %t763 = alloca %struct.List__Str
  store %struct.List__Str %t762, ptr %t763
  %t764 = load i64, ptr %t759
  %t765 = call ptr @tml_N3std11collections4list9List__Str3getE(ptr %t763, i64 %t764)
  ret ptr %t765
}
; DEBUG LAZY type_name=U8 method=fmt_upper_hex

define internal ptr @tml_N4core2U813fmt_upper_hexE(i8 %this) #0 {
entry:
  %t766 = zext i8 %this to i64
  %t767 = call ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %t766, i1 1)
  %t768 = add i64 0, 2
  %t769 = call i64 @strlen(ptr %t767)
  %t770 = add i64 %t768, %t769
  %t771 = add i64 %t770, 1
  %t772 = call ptr @malloc(i64 %t771)
  call void @llvm.memcpy.p0.p0.i64(ptr %t772, ptr @.str.42, i64 2, i1 false)
  %t773 = getelementptr i8, ptr %t772, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t773, ptr %t767, i64 %t769, i1 false)
  %t774 = getelementptr i8, ptr %t772, i64 %t770
  store i8 0, ptr %t774
  call void @tml_str_free(ptr %t767)
  ret ptr %t772
}
; DEBUG LAZY type_name=I8 method=fmt_binary

define internal ptr @tml_N4core2I810fmt_binaryE(i8 %this) #0 {
entry:
  %t775 = call ptr @tml_N4core3fmt7helpers16i8_to_binary_strE_a(i8 %this)
  %t776 = add i64 0, 2
  %t777 = call i64 @strlen(ptr %t775)
  %t778 = add i64 %t776, %t777
  %t779 = add i64 %t778, 1
  %t780 = call ptr @malloc(i64 %t779)
  call void @llvm.memcpy.p0.p0.i64(ptr %t780, ptr @.str.69, i64 2, i1 false)
  %t781 = getelementptr i8, ptr %t780, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t781, ptr %t775, i64 %t777, i1 false)
  %t782 = getelementptr i8, ptr %t780, i64 %t778
  store i8 0, ptr %t782
  call void @tml_str_free(ptr %t775)
  ret ptr %t780
}
; DEBUG LAZY type_name=Buffer method=drop

define internal void @tml_N3std11collections6buffer6Buffer4dropE(ptr %this) #0 {
entry:
  %t783 = call {} @tml_N3std11collections6buffer6Buffer7destroyE(ptr %this)
  ret void
}

; ir_diff::differ::diff_func_name
define ptr @tml_N7ir_diff6differ14diff_func_nameE_R10DiffResultl(ptr %r, i64 %i) #0 {
entry:
  %t784 = alloca ptr
  store ptr %r, ptr %t784
  %t785 = alloca i64
  store i64 %i, ptr %t785
  %t786 = load ptr, ptr %t784
  %t787 = getelementptr inbounds %struct.DiffResult, ptr %t786, i32 0, i32 2
  %t788 = load %struct.List__FunctionDiff, ptr %t787
  %t789 = alloca %struct.List__FunctionDiff
  store %struct.List__FunctionDiff %t788, ptr %t789
  %t790 = load i64, ptr %t785
  %t791 = call %struct.FunctionDiff @tml_N3std11collections4list18List__FunctionDiff3getE(ptr %t789, i64 %t790)
  %t792 = alloca %struct.FunctionDiff
  store %struct.FunctionDiff %t791, ptr %t792
  %t793 = getelementptr inbounds %struct.FunctionDiff, ptr %t792, i32 0, i32 0
  %t794 = load ptr, ptr %t793
  ret ptr %t794
}
; DEBUG LAZY type_name=I64 method=to_string

define internal ptr @tml_N4core3I649to_stringE(i64 %this) #0 {
entry:
  %t795 = call ptr @tml_N4core3fmt7helpers10i64_to_strE_l(i64 %this)
  ret ptr %t795
}
; DEBUG LAZY type_name=U8 method=fmt_octal

define internal ptr @tml_N4core2U89fmt_octalE(i8 %this) #0 {
entry:
  %t796 = zext i8 %this to i64
  %t797 = call ptr @tml_N4core3fmt7helpers16u64_to_octal_strE_m(i64 %t796)
  %t798 = add i64 0, 2
  %t799 = call i64 @strlen(ptr %t797)
  %t800 = add i64 %t798, %t799
  %t801 = add i64 %t800, 1
  %t802 = call ptr @malloc(i64 %t801)
  call void @llvm.memcpy.p0.p0.i64(ptr %t802, ptr @.str.66, i64 2, i1 false)
  %t803 = getelementptr i8, ptr %t802, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t803, ptr %t797, i64 %t799, i1 false)
  %t804 = getelementptr i8, ptr %t802, i64 %t800
  store i8 0, ptr %t804
  call void @tml_str_free(ptr %t797)
  ret ptr %t802
}

; ir_diff::differ::diff_a_line
define ptr @tml_N7ir_diff6differ11diff_a_lineE_R10DiffResultl(ptr %r, i64 %i) #0 {
entry:
  %t805 = alloca ptr
  store ptr %r, ptr %t805
  %t806 = alloca i64
  store i64 %i, ptr %t806
  %t807 = load ptr, ptr %t805
  %t808 = getelementptr inbounds %struct.DiffResult, ptr %t807, i32 0, i32 2
  %t809 = load %struct.List__FunctionDiff, ptr %t808
  %t810 = alloca %struct.List__FunctionDiff
  store %struct.List__FunctionDiff %t809, ptr %t810
  %t811 = load i64, ptr %t806
  %t812 = call %struct.FunctionDiff @tml_N3std11collections4list18List__FunctionDiff3getE(ptr %t810, i64 %t811)
  %t813 = alloca %struct.FunctionDiff
  store %struct.FunctionDiff %t812, ptr %t813
  %t814 = getelementptr inbounds %struct.FunctionDiff, ptr %t813, i32 0, i32 2
  %t815 = load ptr, ptr %t814
  ret ptr %t815
}
; DEBUG LAZY type_name=F64 method=default

define internal double @tml_N4core3F647defaultE() #0 {
entry:
  ret double 0.0
}
; DEBUG LAZY type_name=U8 method=to_string

define internal ptr @tml_N4core2U89to_stringE(i8 %this) #0 {
entry:
  %t816 = call ptr @tml_N4core3fmt7helpers9u8_to_strE_h(i8 %this)
  ret ptr %t816
}
; DEBUG LAZY type_name=U16 method=debug_string

define internal ptr @tml_N4core3U1612debug_stringE(i16 %this) #0 {
entry:
  %t817 = call ptr @tml_N4core3fmt7helpers10u16_to_strE_t(i16 %this)
  ret ptr %t817
}
; DEBUG LAZY type_name=U32 method=fmt_binary

define internal ptr @tml_N4core3U3210fmt_binaryE(i32 %this) #0 {
entry:
  %t818 = call ptr @tml_N4core3fmt7helpers17u32_to_binary_strE_j(i32 %this)
  %t819 = add i64 0, 2
  %t820 = call i64 @strlen(ptr %t818)
  %t821 = add i64 %t819, %t820
  %t822 = add i64 %t821, 1
  %t823 = call ptr @malloc(i64 %t822)
  call void @llvm.memcpy.p0.p0.i64(ptr %t823, ptr @.str.69, i64 2, i1 false)
  %t824 = getelementptr i8, ptr %t823, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t824, ptr %t818, i64 %t820, i1 false)
  %t825 = getelementptr i8, ptr %t823, i64 %t821
  store i8 0, ptr %t825
  call void @tml_str_free(ptr %t818)
  ret ptr %t823
}
; DEBUG LAZY type_name=I8 method=default

define internal i8 @tml_N4core2I87defaultE() #0 {
entry:
  %t826 = trunc i32 0 to i8
  ret i8 %t826
}
; DEBUG LAZY type_name=I16 method=default

define internal i16 @tml_N4core3I167defaultE() #0 {
entry:
  %t827 = trunc i32 0 to i16
  ret i16 %t827
}
; DEBUG LAZY type_name=I64 method=default

define internal i64 @tml_N4core3I647defaultE() #0 {
entry:
  %t828 = sext i32 0 to i64
  ret i64 %t828
}
; DEBUG LAZY type_name=U64 method=debug_string

define internal ptr @tml_N4core3U6412debug_stringE(i64 %this) #0 {
entry:
  %t829 = call ptr @tml_N4core3fmt7helpers10u64_to_strE_m(i64 %this)
  ret ptr %t829
}
; DEBUG LAZY type_name=U16 method=default

define internal i16 @tml_N4core3U167defaultE() #0 {
entry:
  %t830 = trunc i32 0 to i16
  ret i16 %t830
}
; DEBUG LAZY type_name=U64 method=default

define internal i64 @tml_N4core3U647defaultE() #0 {
entry:
  %t831 = sext i32 0 to i64
  ret i64 %t831
}
; DEBUG LAZY type_name=Bool method=default

define internal i1 @tml_N4core4Bool7defaultE() #0 {
entry:
  ret i1 0
}
; DEBUG LAZY type_name=Bool method=to_string

define internal ptr @tml_N4core4Bool9to_stringE(i1 %this) #0 {
entry:
  br i1 %this, label %if.then242, label %if.end244
if.then242:
  ret ptr @.str.70
if.end244:
  ret ptr @.str.71
}
; DEBUG LAZY type_name=I32 method=fmt_octal

define internal ptr @tml_N4core3I329fmt_octalE(i32 %this) #0 {
entry:
  %t832 = zext i32 %this to i64
  %t833 = call ptr @tml_N4core3fmt7helpers16u64_to_octal_strE_m(i64 %t832)
  %t834 = add i64 0, 2
  %t835 = call i64 @strlen(ptr %t833)
  %t836 = add i64 %t834, %t835
  %t837 = add i64 %t836, 1
  %t838 = call ptr @malloc(i64 %t837)
  call void @llvm.memcpy.p0.p0.i64(ptr %t838, ptr @.str.66, i64 2, i1 false)
  %t839 = getelementptr i8, ptr %t838, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t839, ptr %t833, i64 %t835, i1 false)
  %t840 = getelementptr i8, ptr %t838, i64 %t836
  store i8 0, ptr %t840
  call void @tml_str_free(ptr %t833)
  ret ptr %t838
}
; DEBUG LAZY type_name=I32 method=to_string

define internal ptr @tml_N4core3I329to_stringE(i32 %this) #0 {
entry:
  %t841 = call ptr @tml_N4core3fmt7helpers10i32_to_strE_i(i32 %this)
  ret ptr %t841
}
; DEBUG LAZY type_name=U32 method=to_string

define internal ptr @tml_N4core3U329to_stringE(i32 %this) #0 {
entry:
  %t842 = call ptr @tml_N4core3fmt7helpers10u32_to_strE_j(i32 %this)
  ret ptr %t842
}
; DEBUG LAZY type_name=U32 method=debug_string

define internal ptr @tml_N4core3U3212debug_stringE(i32 %this) #0 {
entry:
  %t843 = call ptr @tml_N4core3fmt7helpers10u32_to_strE_j(i32 %this)
  ret ptr %t843
}
; DEBUG LAZY type_name=U64 method=to_string

define internal ptr @tml_N4core3U649to_stringE(i64 %this) #0 {
entry:
  %t844 = call ptr @tml_N4core3fmt7helpers10u64_to_strE_m(i64 %this)
  ret ptr %t844
}
; DEBUG LAZY type_name=F32 method=to_string

define internal ptr @tml_N4core3F329to_stringE(float %this) #0 {
entry:
  %t845 = call ptr @f32_to_string(float %this)
  ret ptr %t845
}
; DEBUG LAZY type_name=I64 method=debug_string

define internal ptr @tml_N4core3I6412debug_stringE(i64 %this) #0 {
entry:
  %t846 = call ptr @tml_N4core3fmt7helpers10i64_to_strE_l(i64 %this)
  ret ptr %t846
}
; DEBUG LAZY type_name=U8 method=debug_string

define internal ptr @tml_N4core2U812debug_stringE(i8 %this) #0 {
entry:
  %t847 = call ptr @tml_N4core3fmt7helpers9u8_to_strE_h(i8 %this)
  ret ptr %t847
}
; DEBUG LAZY type_name=F32 method=debug_string

define internal ptr @tml_N4core3F3212debug_stringE(float %this) #0 {
entry:
  %t848 = call ptr @f32_to_string(float %this)
  ret ptr %t848
}
; DEBUG LAZY type_name=I32 method=fmt_binary

define internal ptr @tml_N4core3I3210fmt_binaryE(i32 %this) #0 {
entry:
  %t849 = call ptr @tml_N4core3fmt7helpers17i32_to_binary_strE_i(i32 %this)
  %t850 = add i64 0, 2
  %t851 = call i64 @strlen(ptr %t849)
  %t852 = add i64 %t850, %t851
  %t853 = add i64 %t852, 1
  %t854 = call ptr @malloc(i64 %t853)
  call void @llvm.memcpy.p0.p0.i64(ptr %t854, ptr @.str.69, i64 2, i1 false)
  %t855 = getelementptr i8, ptr %t854, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t855, ptr %t849, i64 %t851, i1 false)
  %t856 = getelementptr i8, ptr %t854, i64 %t852
  store i8 0, ptr %t856
  call void @tml_str_free(ptr %t849)
  ret ptr %t854
}
; DEBUG LAZY type_name=F64 method=debug_string

define internal ptr @tml_N4core3F6412debug_stringE(double %this) #0 {
entry:
  %t857 = call ptr @f64_to_string(double %this)
  ret ptr %t857
}
; DEBUG LAZY type_name=Bool method=debug_string

define internal ptr @tml_N4core4Bool12debug_stringE(i1 %this) #0 {
entry:
  br i1 %this, label %if.then245, label %if.end247
if.then245:
  ret ptr @.str.70
if.end247:
  ret ptr @.str.71
}
; DEBUG LAZY type_name=I64 method=fmt_binary

define internal ptr @tml_N4core3I6410fmt_binaryE(i64 %this) #0 {
entry:
  %t858 = call ptr @tml_N4core3fmt7helpers17i64_to_binary_strE_l(i64 %this)
  %t859 = add i64 0, 2
  %t860 = call i64 @strlen(ptr %t858)
  %t861 = add i64 %t859, %t860
  %t862 = add i64 %t861, 1
  %t863 = call ptr @malloc(i64 %t862)
  call void @llvm.memcpy.p0.p0.i64(ptr %t863, ptr @.str.69, i64 2, i1 false)
  %t864 = getelementptr i8, ptr %t863, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t864, ptr %t858, i64 %t860, i1 false)
  %t865 = getelementptr i8, ptr %t863, i64 %t861
  store i8 0, ptr %t865
  call void @tml_str_free(ptr %t858)
  ret ptr %t863
}
; DEBUG LAZY type_name=Str method=debug_string

define internal ptr @tml_N4core3Str12debug_stringE(ptr %this) #0 {
entry:
  %t866 = add i64 0, 2
  %t867 = call i64 @strlen(ptr %this)
  %t868 = add i64 %t866, %t867
  %t869 = add i64 %t868, 1
  %t870 = call ptr @malloc(i64 %t869)
  call void @llvm.memcpy.p0.p0.i64(ptr %t870, ptr @.str.72, i64 1, i1 false)
  %t871 = getelementptr i8, ptr %t870, i64 1
  call void @llvm.memcpy.p0.p0.i64(ptr %t871, ptr %this, i64 %t867, i1 false)
  %t872 = add i64 1, %t867
  %t873 = getelementptr i8, ptr %t870, i64 %t872
  call void @llvm.memcpy.p0.p0.i64(ptr %t873, ptr @.str.72, i64 1, i1 false)
  %t874 = getelementptr i8, ptr %t870, i64 %t868
  store i8 0, ptr %t874
  ret ptr %t870
}
; DEBUG LAZY type_name=Char method=debug_string

define internal ptr @tml_N4core4Char12debug_stringE(i32 %this) #0 {
entry:
  %t875 = call ptr @tml_N4core3fmt7helpers11char_to_strE_c(i32 %this)
  %t876 = add i64 0, 2
  %t877 = call i64 @strlen(ptr %t875)
  %t878 = add i64 %t876, %t877
  %t879 = add i64 %t878, 1
  %t880 = call ptr @malloc(i64 %t879)
  call void @llvm.memcpy.p0.p0.i64(ptr %t880, ptr @.str.73, i64 1, i1 false)
  %t881 = getelementptr i8, ptr %t880, i64 1
  call void @llvm.memcpy.p0.p0.i64(ptr %t881, ptr %t875, i64 %t877, i1 false)
  %t882 = add i64 1, %t877
  %t883 = getelementptr i8, ptr %t880, i64 %t882
  call void @llvm.memcpy.p0.p0.i64(ptr %t883, ptr @.str.73, i64 1, i1 false)
  %t884 = getelementptr i8, ptr %t880, i64 %t878
  store i8 0, ptr %t884
  call void @tml_str_free(ptr %t875)
  ret ptr %t880
}
; DEBUG LAZY type_name=U16 method=fmt_binary

define internal ptr @tml_N4core3U1610fmt_binaryE(i16 %this) #0 {
entry:
  %t885 = call ptr @tml_N4core3fmt7helpers17u16_to_binary_strE_t(i16 %this)
  %t886 = add i64 0, 2
  %t887 = call i64 @strlen(ptr %t885)
  %t888 = add i64 %t886, %t887
  %t889 = add i64 %t888, 1
  %t890 = call ptr @malloc(i64 %t889)
  call void @llvm.memcpy.p0.p0.i64(ptr %t890, ptr @.str.69, i64 2, i1 false)
  %t891 = getelementptr i8, ptr %t890, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t891, ptr %t885, i64 %t887, i1 false)
  %t892 = getelementptr i8, ptr %t890, i64 %t888
  store i8 0, ptr %t892
  call void @tml_str_free(ptr %t885)
  ret ptr %t890
}
; DEBUG LAZY type_name=U32 method=fmt_octal

define internal ptr @tml_N4core3U329fmt_octalE(i32 %this) #0 {
entry:
  %t893 = zext i32 %this to i64
  %t894 = call ptr @tml_N4core3fmt7helpers16u64_to_octal_strE_m(i64 %t893)
  %t895 = add i64 0, 2
  %t896 = call i64 @strlen(ptr %t894)
  %t897 = add i64 %t895, %t896
  %t898 = add i64 %t897, 1
  %t899 = call ptr @malloc(i64 %t898)
  call void @llvm.memcpy.p0.p0.i64(ptr %t899, ptr @.str.66, i64 2, i1 false)
  %t900 = getelementptr i8, ptr %t899, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t900, ptr %t894, i64 %t896, i1 false)
  %t901 = getelementptr i8, ptr %t899, i64 %t897
  store i8 0, ptr %t901
  call void @tml_str_free(ptr %t894)
  ret ptr %t899
}
; DEBUG LAZY type_name=U64 method=fmt_octal

define internal ptr @tml_N4core3U649fmt_octalE(i64 %this) #0 {
entry:
  %t902 = call ptr @tml_N4core3fmt7helpers16u64_to_octal_strE_m(i64 %this)
  %t903 = add i64 0, 2
  %t904 = call i64 @strlen(ptr %t902)
  %t905 = add i64 %t903, %t904
  %t906 = add i64 %t905, 1
  %t907 = call ptr @malloc(i64 %t906)
  call void @llvm.memcpy.p0.p0.i64(ptr %t907, ptr @.str.66, i64 2, i1 false)
  %t908 = getelementptr i8, ptr %t907, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t908, ptr %t902, i64 %t904, i1 false)
  %t909 = getelementptr i8, ptr %t907, i64 %t905
  store i8 0, ptr %t909
  call void @tml_str_free(ptr %t902)
  ret ptr %t907
}
; DEBUG LAZY type_name=I8 method=fmt_octal

define internal ptr @tml_N4core2I89fmt_octalE(i8 %this) #0 {
entry:
  %t910 = zext i8 %this to i64
  %t911 = call ptr @tml_N4core3fmt7helpers16u64_to_octal_strE_m(i64 %t910)
  %t912 = add i64 0, 2
  %t913 = call i64 @strlen(ptr %t911)
  %t914 = add i64 %t912, %t913
  %t915 = add i64 %t914, 1
  %t916 = call ptr @malloc(i64 %t915)
  call void @llvm.memcpy.p0.p0.i64(ptr %t916, ptr @.str.66, i64 2, i1 false)
  %t917 = getelementptr i8, ptr %t916, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t917, ptr %t911, i64 %t913, i1 false)
  %t918 = getelementptr i8, ptr %t916, i64 %t914
  store i8 0, ptr %t918
  call void @tml_str_free(ptr %t911)
  ret ptr %t916
}
; DEBUG LAZY type_name=I16 method=fmt_octal

define internal ptr @tml_N4core3I169fmt_octalE(i16 %this) #0 {
entry:
  %t919 = zext i16 %this to i64
  %t920 = call ptr @tml_N4core3fmt7helpers16u64_to_octal_strE_m(i64 %t919)
  %t921 = add i64 0, 2
  %t922 = call i64 @strlen(ptr %t920)
  %t923 = add i64 %t921, %t922
  %t924 = add i64 %t923, 1
  %t925 = call ptr @malloc(i64 %t924)
  call void @llvm.memcpy.p0.p0.i64(ptr %t925, ptr @.str.66, i64 2, i1 false)
  %t926 = getelementptr i8, ptr %t925, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t926, ptr %t920, i64 %t922, i1 false)
  %t927 = getelementptr i8, ptr %t925, i64 %t923
  store i8 0, ptr %t927
  call void @tml_str_free(ptr %t920)
  ret ptr %t925
}
; DEBUG LAZY type_name=I64 method=fmt_octal

define internal ptr @tml_N4core3I649fmt_octalE(i64 %this) #0 {
entry:
  %t928 = call ptr @tml_N4core3fmt7helpers16u64_to_octal_strE_m(i64 %this)
  %t929 = add i64 0, 2
  %t930 = call i64 @strlen(ptr %t928)
  %t931 = add i64 %t929, %t930
  %t932 = add i64 %t931, 1
  %t933 = call ptr @malloc(i64 %t932)
  call void @llvm.memcpy.p0.p0.i64(ptr %t933, ptr @.str.66, i64 2, i1 false)
  %t934 = getelementptr i8, ptr %t933, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t934, ptr %t928, i64 %t930, i1 false)
  %t935 = getelementptr i8, ptr %t933, i64 %t931
  store i8 0, ptr %t935
  call void @tml_str_free(ptr %t928)
  ret ptr %t933
}
; DEBUG LAZY type_name=U16 method=fmt_lower_hex

define internal ptr @tml_N4core3U1613fmt_lower_hexE(i16 %this) #0 {
entry:
  %t936 = zext i16 %this to i64
  %t937 = call ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %t936, i1 0)
  %t938 = add i64 0, 2
  %t939 = call i64 @strlen(ptr %t937)
  %t940 = add i64 %t938, %t939
  %t941 = add i64 %t940, 1
  %t942 = call ptr @malloc(i64 %t941)
  call void @llvm.memcpy.p0.p0.i64(ptr %t942, ptr @.str.42, i64 2, i1 false)
  %t943 = getelementptr i8, ptr %t942, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t943, ptr %t937, i64 %t939, i1 false)
  %t944 = getelementptr i8, ptr %t942, i64 %t940
  store i8 0, ptr %t944
  call void @tml_str_free(ptr %t937)
  ret ptr %t942
}
; DEBUG LAZY type_name=U32 method=fmt_lower_hex

define internal ptr @tml_N4core3U3213fmt_lower_hexE(i32 %this) #0 {
entry:
  %t945 = zext i32 %this to i64
  %t946 = call ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %t945, i1 0)
  %t947 = add i64 0, 2
  %t948 = call i64 @strlen(ptr %t946)
  %t949 = add i64 %t947, %t948
  %t950 = add i64 %t949, 1
  %t951 = call ptr @malloc(i64 %t950)
  call void @llvm.memcpy.p0.p0.i64(ptr %t951, ptr @.str.42, i64 2, i1 false)
  %t952 = getelementptr i8, ptr %t951, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t952, ptr %t946, i64 %t948, i1 false)
  %t953 = getelementptr i8, ptr %t951, i64 %t949
  store i8 0, ptr %t953
  call void @tml_str_free(ptr %t946)
  ret ptr %t951
}
; DEBUG LAZY type_name=U64 method=fmt_lower_hex

define internal ptr @tml_N4core3U6413fmt_lower_hexE(i64 %this) #0 {
entry:
  %t954 = call ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %this, i1 0)
  %t955 = add i64 0, 2
  %t956 = call i64 @strlen(ptr %t954)
  %t957 = add i64 %t955, %t956
  %t958 = add i64 %t957, 1
  %t959 = call ptr @malloc(i64 %t958)
  call void @llvm.memcpy.p0.p0.i64(ptr %t959, ptr @.str.42, i64 2, i1 false)
  %t960 = getelementptr i8, ptr %t959, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t960, ptr %t954, i64 %t956, i1 false)
  %t961 = getelementptr i8, ptr %t959, i64 %t957
  store i8 0, ptr %t961
  call void @tml_str_free(ptr %t954)
  ret ptr %t959
}
; DEBUG LAZY type_name=I8 method=fmt_lower_hex

define internal ptr @tml_N4core2I813fmt_lower_hexE(i8 %this) #0 {
entry:
  %t962 = zext i8 %this to i64
  %t963 = call ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %t962, i1 0)
  %t964 = add i64 0, 2
  %t965 = call i64 @strlen(ptr %t963)
  %t966 = add i64 %t964, %t965
  %t967 = add i64 %t966, 1
  %t968 = call ptr @malloc(i64 %t967)
  call void @llvm.memcpy.p0.p0.i64(ptr %t968, ptr @.str.42, i64 2, i1 false)
  %t969 = getelementptr i8, ptr %t968, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t969, ptr %t963, i64 %t965, i1 false)
  %t970 = getelementptr i8, ptr %t968, i64 %t966
  store i8 0, ptr %t970
  call void @tml_str_free(ptr %t963)
  ret ptr %t968
}
; DEBUG LAZY type_name=I16 method=fmt_lower_hex

define internal ptr @tml_N4core3I1613fmt_lower_hexE(i16 %this) #0 {
entry:
  %t971 = zext i16 %this to i64
  %t972 = call ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %t971, i1 0)
  %t973 = add i64 0, 2
  %t974 = call i64 @strlen(ptr %t972)
  %t975 = add i64 %t973, %t974
  %t976 = add i64 %t975, 1
  %t977 = call ptr @malloc(i64 %t976)
  call void @llvm.memcpy.p0.p0.i64(ptr %t977, ptr @.str.42, i64 2, i1 false)
  %t978 = getelementptr i8, ptr %t977, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t978, ptr %t972, i64 %t974, i1 false)
  %t979 = getelementptr i8, ptr %t977, i64 %t975
  store i8 0, ptr %t979
  call void @tml_str_free(ptr %t972)
  ret ptr %t977
}
; DEBUG LAZY type_name=I32 method=fmt_lower_hex

define internal ptr @tml_N4core3I3213fmt_lower_hexE(i32 %this) #0 {
entry:
  %t980 = zext i32 %this to i64
  %t981 = call ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %t980, i1 0)
  %t982 = add i64 0, 2
  %t983 = call i64 @strlen(ptr %t981)
  %t984 = add i64 %t982, %t983
  %t985 = add i64 %t984, 1
  %t986 = call ptr @malloc(i64 %t985)
  call void @llvm.memcpy.p0.p0.i64(ptr %t986, ptr @.str.42, i64 2, i1 false)
  %t987 = getelementptr i8, ptr %t986, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t987, ptr %t981, i64 %t983, i1 false)
  %t988 = getelementptr i8, ptr %t986, i64 %t984
  store i8 0, ptr %t988
  call void @tml_str_free(ptr %t981)
  ret ptr %t986
}
; DEBUG LAZY type_name=I64 method=fmt_lower_hex

define internal ptr @tml_N4core3I6413fmt_lower_hexE(i64 %this) #0 {
entry:
  %t989 = call ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %this, i1 0)
  %t990 = add i64 0, 2
  %t991 = call i64 @strlen(ptr %t989)
  %t992 = add i64 %t990, %t991
  %t993 = add i64 %t992, 1
  %t994 = call ptr @malloc(i64 %t993)
  call void @llvm.memcpy.p0.p0.i64(ptr %t994, ptr @.str.42, i64 2, i1 false)
  %t995 = getelementptr i8, ptr %t994, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t995, ptr %t989, i64 %t991, i1 false)
  %t996 = getelementptr i8, ptr %t994, i64 %t992
  store i8 0, ptr %t996
  call void @tml_str_free(ptr %t989)
  ret ptr %t994
}
; DEBUG LAZY type_name=U16 method=fmt_upper_hex

define internal ptr @tml_N4core3U1613fmt_upper_hexE(i16 %this) #0 {
entry:
  %t997 = zext i16 %this to i64
  %t998 = call ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %t997, i1 1)
  %t999 = add i64 0, 2
  %t1000 = call i64 @strlen(ptr %t998)
  %t1001 = add i64 %t999, %t1000
  %t1002 = add i64 %t1001, 1
  %t1003 = call ptr @malloc(i64 %t1002)
  call void @llvm.memcpy.p0.p0.i64(ptr %t1003, ptr @.str.42, i64 2, i1 false)
  %t1004 = getelementptr i8, ptr %t1003, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t1004, ptr %t998, i64 %t1000, i1 false)
  %t1005 = getelementptr i8, ptr %t1003, i64 %t1001
  store i8 0, ptr %t1005
  call void @tml_str_free(ptr %t998)
  ret ptr %t1003
}
; DEBUG LAZY type_name=U32 method=fmt_upper_hex

define internal ptr @tml_N4core3U3213fmt_upper_hexE(i32 %this) #0 {
entry:
  %t1006 = zext i32 %this to i64
  %t1007 = call ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %t1006, i1 1)
  %t1008 = add i64 0, 2
  %t1009 = call i64 @strlen(ptr %t1007)
  %t1010 = add i64 %t1008, %t1009
  %t1011 = add i64 %t1010, 1
  %t1012 = call ptr @malloc(i64 %t1011)
  call void @llvm.memcpy.p0.p0.i64(ptr %t1012, ptr @.str.42, i64 2, i1 false)
  %t1013 = getelementptr i8, ptr %t1012, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t1013, ptr %t1007, i64 %t1009, i1 false)
  %t1014 = getelementptr i8, ptr %t1012, i64 %t1010
  store i8 0, ptr %t1014
  call void @tml_str_free(ptr %t1007)
  ret ptr %t1012
}
; DEBUG LAZY type_name=U64 method=fmt_upper_hex

define internal ptr @tml_N4core3U6413fmt_upper_hexE(i64 %this) #0 {
entry:
  %t1015 = call ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %this, i1 1)
  %t1016 = add i64 0, 2
  %t1017 = call i64 @strlen(ptr %t1015)
  %t1018 = add i64 %t1016, %t1017
  %t1019 = add i64 %t1018, 1
  %t1020 = call ptr @malloc(i64 %t1019)
  call void @llvm.memcpy.p0.p0.i64(ptr %t1020, ptr @.str.42, i64 2, i1 false)
  %t1021 = getelementptr i8, ptr %t1020, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t1021, ptr %t1015, i64 %t1017, i1 false)
  %t1022 = getelementptr i8, ptr %t1020, i64 %t1018
  store i8 0, ptr %t1022
  call void @tml_str_free(ptr %t1015)
  ret ptr %t1020
}
; DEBUG LAZY type_name=I8 method=fmt_upper_hex

define internal ptr @tml_N4core2I813fmt_upper_hexE(i8 %this) #0 {
entry:
  %t1023 = zext i8 %this to i64
  %t1024 = call ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %t1023, i1 1)
  %t1025 = add i64 0, 2
  %t1026 = call i64 @strlen(ptr %t1024)
  %t1027 = add i64 %t1025, %t1026
  %t1028 = add i64 %t1027, 1
  %t1029 = call ptr @malloc(i64 %t1028)
  call void @llvm.memcpy.p0.p0.i64(ptr %t1029, ptr @.str.42, i64 2, i1 false)
  %t1030 = getelementptr i8, ptr %t1029, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t1030, ptr %t1024, i64 %t1026, i1 false)
  %t1031 = getelementptr i8, ptr %t1029, i64 %t1027
  store i8 0, ptr %t1031
  call void @tml_str_free(ptr %t1024)
  ret ptr %t1029
}
; DEBUG LAZY type_name=I16 method=fmt_upper_hex

define internal ptr @tml_N4core3I1613fmt_upper_hexE(i16 %this) #0 {
entry:
  %t1032 = zext i16 %this to i64
  %t1033 = call ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %t1032, i1 1)
  %t1034 = add i64 0, 2
  %t1035 = call i64 @strlen(ptr %t1033)
  %t1036 = add i64 %t1034, %t1035
  %t1037 = add i64 %t1036, 1
  %t1038 = call ptr @malloc(i64 %t1037)
  call void @llvm.memcpy.p0.p0.i64(ptr %t1038, ptr @.str.42, i64 2, i1 false)
  %t1039 = getelementptr i8, ptr %t1038, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t1039, ptr %t1033, i64 %t1035, i1 false)
  %t1040 = getelementptr i8, ptr %t1038, i64 %t1036
  store i8 0, ptr %t1040
  call void @tml_str_free(ptr %t1033)
  ret ptr %t1038
}
; DEBUG LAZY type_name=I32 method=fmt_upper_hex

define internal ptr @tml_N4core3I3213fmt_upper_hexE(i32 %this) #0 {
entry:
  %t1041 = zext i32 %this to i64
  %t1042 = call ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %t1041, i1 1)
  %t1043 = add i64 0, 2
  %t1044 = call i64 @strlen(ptr %t1042)
  %t1045 = add i64 %t1043, %t1044
  %t1046 = add i64 %t1045, 1
  %t1047 = call ptr @malloc(i64 %t1046)
  call void @llvm.memcpy.p0.p0.i64(ptr %t1047, ptr @.str.42, i64 2, i1 false)
  %t1048 = getelementptr i8, ptr %t1047, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t1048, ptr %t1042, i64 %t1044, i1 false)
  %t1049 = getelementptr i8, ptr %t1047, i64 %t1045
  store i8 0, ptr %t1049
  call void @tml_str_free(ptr %t1042)
  ret ptr %t1047
}
; DEBUG LAZY type_name=I64 method=fmt_upper_hex

define internal ptr @tml_N4core3I6413fmt_upper_hexE(i64 %this) #0 {
entry:
  %t1050 = call ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %this, i1 1)
  %t1051 = add i64 0, 2
  %t1052 = call i64 @strlen(ptr %t1050)
  %t1053 = add i64 %t1051, %t1052
  %t1054 = add i64 %t1053, 1
  %t1055 = call ptr @malloc(i64 %t1054)
  call void @llvm.memcpy.p0.p0.i64(ptr %t1055, ptr @.str.42, i64 2, i1 false)
  %t1056 = getelementptr i8, ptr %t1055, i64 2
  call void @llvm.memcpy.p0.p0.i64(ptr %t1056, ptr %t1050, i64 %t1052, i1 false)
  %t1057 = getelementptr i8, ptr %t1055, i64 %t1053
  store i8 0, ptr %t1057
  call void @tml_str_free(ptr %t1050)
  ret ptr %t1055
}
; DEBUG LAZY type_name=F32 method=fmt_lower_exp

define internal ptr @tml_N4core3F3213fmt_lower_expE(float %this) #0 {
entry:
  %t1058 = zext i1 0 to i32
  %t1059 = call ptr @f32_to_exp_string(float %this, i32 %t1058)
  ret ptr %t1059
}
; DEBUG LAZY type_name=F64 method=fmt_lower_exp

define internal ptr @tml_N4core3F6413fmt_lower_expE(double %this) #0 {
entry:
  %t1060 = zext i1 0 to i32
  %t1061 = call ptr @f64_to_exp_string(double %this, i32 %t1060)
  ret ptr %t1061
}
; DEBUG LAZY type_name=F32 method=fmt_upper_exp

define internal ptr @tml_N4core3F3213fmt_upper_expE(float %this) #0 {
entry:
  %t1062 = zext i1 1 to i32
  %t1063 = call ptr @f32_to_exp_string(float %this, i32 %t1062)
  ret ptr %t1063
}
; DEBUG LAZY type_name=F64 method=fmt_upper_exp

define internal ptr @tml_N4core3F6413fmt_upper_expE(double %this) #0 {
entry:
  %t1064 = zext i1 1 to i32
  %t1065 = call ptr @f64_to_exp_string(double %this, i32 %t1064)
  ret ptr %t1065
}

; core::fmt::helpers::u64_to_octal_str
define ptr @tml_N4core3fmt7helpers16u64_to_octal_strE_m(i64 %n) #0 {
entry:
  %t1066 = alloca i64
  store i64 %n, ptr %t1066
  %t1070 = alloca i64
  %t1072 = alloca i64
  %t1092 = alloca i64
  %t1104 = alloca i64
  %t1106 = alloca i64
  %t1118 = alloca i8
  %t1067 = load i64, ptr %t1066
  %t1069 = sext i32 0 to i64
  %t1068 = icmp eq i64 %t1067, %t1069
  br i1 %t1068, label %if.then248, label %if.end250
if.then248:
  ret ptr @.str.74
if.end250:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1070)
  store i64 0, ptr %t1070
  %t1071 = load i64, ptr %t1066
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1072)
  store i64 %t1071, ptr %t1072
  br label %loop.preheader251
loop.preheader251:
  br label %loop.header252
loop.header252:
  %t1073 = load i64, ptr %t1072
  %t1075 = sext i32 0 to i64
  %t1074 = icmp ne i64 %t1073, %t1075
  br i1 %t1074, label %loop.body253, label %loop.exit255
loop.body253:
  %t1076 = load i64, ptr %t1070
  %t1078 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1076, i64 1)
  %t1077 = extractvalue { i64, i1 } %t1078, 0
  %t1079 = extractvalue { i64, i1 } %t1078, 1
  br i1 %t1079, label %add_overflow257, label %add_ok256
add_overflow257:
  call void @panic(ptr @.str.75)
  unreachable
add_ok256:
  store i64 %t1077, ptr %t1070
  %t1080 = load i64, ptr %t1072
  %t1081 = lshr i64 %t1080, 3
  store i64 %t1081, ptr %t1072
  br label %loop.latch254
loop.latch254:
  br label %loop.header252, !llvm.loop !1011
loop.exit255:
  %t1082 = load i64, ptr %t1070
  %t1084 = sext i32 1 to i64
  %t1085 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1082, i64 %t1084)
  %t1083 = extractvalue { i64, i1 } %t1085, 0
  %t1086 = extractvalue { i64, i1 } %t1085, 1
  br i1 %t1086, label %add_overflow259, label %add_ok258
add_overflow259:
  call void @panic(ptr @.str.76)
  unreachable
add_ok258:
  %t1087 = call ptr @mem_alloc(i64 %t1083)
  %t1088 = alloca ptr
  store ptr %t1087, ptr %t1088
  %t1089 = load ptr, ptr %t1088
  %t1091 = ptrtoint ptr %t1089 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1092)
  store i64 %t1091, ptr %t1092
  %t1093 = load i64, ptr %t1092
  %t1094 = load i64, ptr %t1070
  %t1096 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1093, i64 %t1094)
  %t1095 = extractvalue { i64, i1 } %t1096, 0
  %t1097 = extractvalue { i64, i1 } %t1096, 1
  br i1 %t1097, label %add_overflow261, label %add_ok260
add_overflow261:
  call void @panic(ptr @.str.77)
  unreachable
add_ok260:
  %t1098 = inttoptr i64 %t1095 to ptr
  %t1099 = trunc i32 0 to i8
  store i8 %t1099, ptr %t1098
  %t1100 = load i64, ptr %t1070
  %t1102 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1100, i64 1)
  %t1101 = extractvalue { i64, i1 } %t1102, 0
  %t1103 = extractvalue { i64, i1 } %t1102, 1
  br i1 %t1103, label %sub_overflow263, label %sub_ok262
sub_overflow263:
  call void @panic(ptr @.str.78)
  unreachable
sub_ok262:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1104)
  store i64 %t1101, ptr %t1104
  %t1105 = load i64, ptr %t1066
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1106)
  store i64 %t1105, ptr %t1106
  br label %loop.preheader264
loop.preheader264:
  br label %loop.header265
loop.header265:
  %t1107 = load i64, ptr %t1106
  %t1109 = sext i32 0 to i64
  %t1108 = icmp ne i64 %t1107, %t1109
  br i1 %t1108, label %loop.body266, label %loop.exit268
loop.body266:
  %t1110 = load i64, ptr %t1106
  %t1112 = sext i32 7 to i64
  %t1111 = and i64 %t1110, %t1112
  %t1113 = trunc i64 %t1111 to i8
  %t1114 = trunc i32 48 to i8
  %t1116 = call { i8, i1 } @llvm.uadd.with.overflow.i8(i8 %t1113, i8 %t1114)
  %t1115 = extractvalue { i8, i1 } %t1116, 0
  %t1117 = extractvalue { i8, i1 } %t1116, 1
  br i1 %t1117, label %add_overflow270, label %add_ok269
add_overflow270:
  call void @panic(ptr @.str.79)
  unreachable
add_ok269:
  call void @llvm.lifetime.start.p0(i64 1, ptr %t1118)
  store i8 %t1115, ptr %t1118
  %t1119 = load i64, ptr %t1092
  %t1120 = load i64, ptr %t1104
  %t1122 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1119, i64 %t1120)
  %t1121 = extractvalue { i64, i1 } %t1122, 0
  %t1123 = extractvalue { i64, i1 } %t1122, 1
  br i1 %t1123, label %add_overflow272, label %add_ok271
add_overflow272:
  call void @panic(ptr @.str.80)
  unreachable
add_ok271:
  %t1124 = inttoptr i64 %t1121 to ptr
  %t1125 = load i8, ptr %t1118
  store i8 %t1125, ptr %t1124
  %t1126 = load i64, ptr %t1106
  %t1127 = lshr i64 %t1126, 3
  store i64 %t1127, ptr %t1106
  %t1128 = load i64, ptr %t1104
  %t1130 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1128, i64 1)
  %t1129 = extractvalue { i64, i1 } %t1130, 0
  %t1131 = extractvalue { i64, i1 } %t1130, 1
  br i1 %t1131, label %sub_overflow274, label %sub_ok273
sub_overflow274:
  call void @panic(ptr @.str.81)
  unreachable
sub_ok273:
  store i64 %t1129, ptr %t1104
  call void @llvm.lifetime.end.p0(i64 1, ptr %t1118)
  br label %loop.latch267
loop.latch267:
  br label %loop.header265, !llvm.loop !1012
loop.exit268:
  %t1132 = load ptr, ptr %t1088
  ret ptr %t1132
}

; core::fmt::helpers::u64_to_hex_str
define ptr @tml_N4core3fmt7helpers14u64_to_hex_strE_mb(i64 %n, i1 %upper) #0 {
entry:
  %t1133 = alloca i64
  store i64 %n, ptr %t1133
  %t1134 = alloca i1
  store i1 %upper, ptr %t1134
  %t1144 = alloca i64
  %t1145 = alloca i64
  %t1147 = alloca i64
  %t1167 = alloca i64
  %t1179 = alloca i64
  %t1181 = alloca i64
  %t1194 = alloca i8
  %t1135 = load i64, ptr %t1133
  %t1137 = sext i32 0 to i64
  %t1136 = icmp eq i64 %t1135, %t1137
  br i1 %t1136, label %if.then275, label %if.end277
if.then275:
  ret ptr @.str.74
if.end277:
  %t1138 = load i1, ptr %t1134
  br i1 %t1138, label %if.then278, label %if.else279
if.then278:
  br label %if.end280
if.else279:
  br label %if.end280
if.end280:
  %t1139 = phi ptr [ @.str.82, %if.then278 ], [ @.str.83, %if.else279 ]
  %t1140 = alloca ptr
  store ptr %t1139, ptr %t1140
  %t1141 = load ptr, ptr %t1140
  %t1143 = ptrtoint ptr %t1141 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1144)
  store i64 %t1143, ptr %t1144
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1145)
  store i64 0, ptr %t1145
  %t1146 = load i64, ptr %t1133
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1147)
  store i64 %t1146, ptr %t1147
  br label %loop.preheader281
loop.preheader281:
  br label %loop.header282
loop.header282:
  %t1148 = load i64, ptr %t1147
  %t1150 = sext i32 0 to i64
  %t1149 = icmp ne i64 %t1148, %t1150
  br i1 %t1149, label %loop.body283, label %loop.exit285
loop.body283:
  %t1151 = load i64, ptr %t1145
  %t1153 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1151, i64 1)
  %t1152 = extractvalue { i64, i1 } %t1153, 0
  %t1154 = extractvalue { i64, i1 } %t1153, 1
  br i1 %t1154, label %add_overflow287, label %add_ok286
add_overflow287:
  call void @panic(ptr @.str.84)
  unreachable
add_ok286:
  store i64 %t1152, ptr %t1145
  %t1155 = load i64, ptr %t1147
  %t1156 = lshr i64 %t1155, 4
  store i64 %t1156, ptr %t1147
  br label %loop.latch284
loop.latch284:
  br label %loop.header282, !llvm.loop !1013
loop.exit285:
  %t1157 = load i64, ptr %t1145
  %t1159 = sext i32 1 to i64
  %t1160 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1157, i64 %t1159)
  %t1158 = extractvalue { i64, i1 } %t1160, 0
  %t1161 = extractvalue { i64, i1 } %t1160, 1
  br i1 %t1161, label %add_overflow289, label %add_ok288
add_overflow289:
  call void @panic(ptr @.str.85)
  unreachable
add_ok288:
  %t1162 = call ptr @mem_alloc(i64 %t1158)
  %t1163 = alloca ptr
  store ptr %t1162, ptr %t1163
  %t1164 = load ptr, ptr %t1163
  %t1166 = ptrtoint ptr %t1164 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1167)
  store i64 %t1166, ptr %t1167
  %t1168 = load i64, ptr %t1167
  %t1169 = load i64, ptr %t1145
  %t1171 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1168, i64 %t1169)
  %t1170 = extractvalue { i64, i1 } %t1171, 0
  %t1172 = extractvalue { i64, i1 } %t1171, 1
  br i1 %t1172, label %add_overflow291, label %add_ok290
add_overflow291:
  call void @panic(ptr @.str.86)
  unreachable
add_ok290:
  %t1173 = inttoptr i64 %t1170 to ptr
  %t1174 = trunc i32 0 to i8
  store i8 %t1174, ptr %t1173
  %t1175 = load i64, ptr %t1145
  %t1177 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1175, i64 1)
  %t1176 = extractvalue { i64, i1 } %t1177, 0
  %t1178 = extractvalue { i64, i1 } %t1177, 1
  br i1 %t1178, label %sub_overflow293, label %sub_ok292
sub_overflow293:
  call void @panic(ptr @.str.87)
  unreachable
sub_ok292:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1179)
  store i64 %t1176, ptr %t1179
  %t1180 = load i64, ptr %t1133
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1181)
  store i64 %t1180, ptr %t1181
  br label %loop.preheader294
loop.preheader294:
  br label %loop.header295
loop.header295:
  %t1182 = load i64, ptr %t1181
  %t1184 = sext i32 0 to i64
  %t1183 = icmp ne i64 %t1182, %t1184
  br i1 %t1183, label %loop.body296, label %loop.exit298
loop.body296:
  %t1185 = load i64, ptr %t1144
  %t1186 = load i64, ptr %t1181
  %t1188 = sext i32 15 to i64
  %t1187 = and i64 %t1186, %t1188
  %t1190 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1185, i64 %t1187)
  %t1189 = extractvalue { i64, i1 } %t1190, 0
  %t1191 = extractvalue { i64, i1 } %t1190, 1
  br i1 %t1191, label %add_overflow300, label %add_ok299
add_overflow300:
  call void @panic(ptr @.str.88)
  unreachable
add_ok299:
  %t1192 = inttoptr i64 %t1189 to ptr
  %t1193 = load i8, ptr %t1192
  call void @llvm.lifetime.start.p0(i64 1, ptr %t1194)
  store i8 %t1193, ptr %t1194
  %t1195 = load i64, ptr %t1167
  %t1196 = load i64, ptr %t1179
  %t1198 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1195, i64 %t1196)
  %t1197 = extractvalue { i64, i1 } %t1198, 0
  %t1199 = extractvalue { i64, i1 } %t1198, 1
  br i1 %t1199, label %add_overflow302, label %add_ok301
add_overflow302:
  call void @panic(ptr @.str.89)
  unreachable
add_ok301:
  %t1200 = inttoptr i64 %t1197 to ptr
  %t1201 = load i8, ptr %t1194
  store i8 %t1201, ptr %t1200
  %t1202 = load i64, ptr %t1181
  %t1203 = lshr i64 %t1202, 4
  store i64 %t1203, ptr %t1181
  %t1204 = load i64, ptr %t1179
  %t1206 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1204, i64 1)
  %t1205 = extractvalue { i64, i1 } %t1206, 0
  %t1207 = extractvalue { i64, i1 } %t1206, 1
  br i1 %t1207, label %sub_overflow304, label %sub_ok303
sub_overflow304:
  call void @panic(ptr @.str.90)
  unreachable
sub_ok303:
  store i64 %t1205, ptr %t1179
  call void @llvm.lifetime.end.p0(i64 1, ptr %t1194)
  br label %loop.latch297
loop.latch297:
  br label %loop.header295, !llvm.loop !1014
loop.exit298:
  %t1208 = load ptr, ptr %t1163
  ret ptr %t1208
}

; core::fmt::helpers::char_to_str
define ptr @tml_N4core3fmt7helpers11char_to_strE_c(i32 %c) #0 {
entry:
  %t1209 = alloca i32
  store i32 %c, ptr %t1209
  %t1211 = alloca i32
  %t1214 = alloca i8
  %t1210 = load i32, ptr %t1209
  call void @llvm.lifetime.start.p0(i64 4, ptr %t1211)
  store i32 %t1210, ptr %t1211
  %t1212 = load i32, ptr %t1211
  %t1213 = trunc i32 %t1212 to i8
  call void @llvm.lifetime.start.p0(i64 1, ptr %t1214)
  store i8 %t1213, ptr %t1214
  %t1215 = call ptr @mem_alloc(i64 2)
  %t1216 = alloca ptr
  store ptr %t1215, ptr %t1216
  %t1217 = load ptr, ptr %t1216
  %t1218 = alloca ptr
  store ptr %t1217, ptr %t1218
  %t1219 = load ptr, ptr %t1218
  %t1220 = load i8, ptr %t1214
  store i8 %t1220, ptr %t1219
  %t1221 = load ptr, ptr %t1216
  %t1223 = ptrtoint ptr %t1221 to i64
  %t1225 = sext i32 1 to i64
  %t1226 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1223, i64 %t1225)
  %t1224 = extractvalue { i64, i1 } %t1226, 0
  %t1227 = extractvalue { i64, i1 } %t1226, 1
  br i1 %t1227, label %add_overflow306, label %add_ok305
add_overflow306:
  call void @panic(ptr @.str.91)
  unreachable
add_ok305:
  %t1228 = inttoptr i64 %t1224 to ptr
  %t1229 = alloca ptr
  store ptr %t1228, ptr %t1229
  %t1230 = load ptr, ptr %t1229
  %t1231 = trunc i32 0 to i8
  store i8 %t1231, ptr %t1230
  %t1232 = load ptr, ptr %t1216
  ret ptr %t1232
}

; ir_diff::differ::demangle_name
define internal ptr @tml_N7ir_diff6differ13demangle_nameE_S(ptr %name) #0 {
entry:
  %t1233 = alloca ptr
  store ptr %name, ptr %t1233
  %t1234 = load ptr, ptr %t1233
  %t1235 = alloca ptr
  store ptr %t1234, ptr %t1235
  br label %loop.preheader307
loop.preheader307:
  br label %loop.header308
loop.header308:
  br i1 1, label %loop.body309, label %loop.exit311
loop.body309:
  %t1236 = load ptr, ptr %t1235
  %t1237 = call ptr @tml_N7ir_diff6differ13demangle_onceE_S(ptr %t1236)
  %t1238 = alloca ptr
  store ptr %t1237, ptr %t1238
  %t1239 = load ptr, ptr %t1238
  %t1240 = call i64 @strlen(ptr %t1239)
  %t1241 = load ptr, ptr %t1235
  %t1242 = call i64 @strlen(ptr %t1241)
  %t1243 = icmp eq i64 %t1240, %t1242
  br i1 %t1243, label %if.then312, label %if.end314
if.then312:
  %t1244 = load ptr, ptr %t1235
  ret ptr %t1244
if.end314:
  %t1245 = load ptr, ptr %t1238
  store ptr %t1245, ptr %t1235
  br label %loop.latch310
loop.latch310:
  br label %loop.header308, !llvm.loop !1015
loop.exit311:
  %t1246 = load ptr, ptr %t1235
  ret ptr %t1246
}

; core::str::transform::trim
define ptr @tml_N4core3str9transform4trimE_S(ptr %s) #0 {
entry:
  %t1247 = alloca ptr
  store ptr %s, ptr %t1247
  %t1250 = alloca i64
  %t1259 = alloca i64
  %t1265 = alloca i64
  %t1276 = alloca i64
  %t1277 = alloca i64
  %t1290 = alloca i8
  %t1300 = alloca i64
  %t1317 = alloca i8
  %t1248 = load ptr, ptr %t1247
  %t1249 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t1248)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1250)
  store i64 %t1249, ptr %t1250
  %t1251 = load i64, ptr %t1250
  %t1253 = sext i32 0 to i64
  %t1252 = icmp eq i64 %t1251, %t1253
  br i1 %t1252, label %if.then315, label %if.end317
if.then315:
  ret ptr @.str.1
if.end317:
  %t1254 = load i64, ptr %t1250
  %t1256 = sext i32 32 to i64
  %t1255 = icmp sge i64 %t1254, %t1256
  br i1 %t1255, label %if.then318, label %if.end320
if.then318:
  %t1257 = load ptr, ptr %t1247
  %t1258 = call i64 @tml_N4core3str4simd15trim_start_simdE_S(ptr %t1257)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1259)
  store i64 %t1258, ptr %t1259
  %t1260 = load i64, ptr %t1259
  %t1261 = load i64, ptr %t1250
  %t1262 = icmp sge i64 %t1260, %t1261
  br i1 %t1262, label %if.then321, label %if.end323
if.then321:
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1259)
  ret ptr @.str.1
if.end323:
  %t1263 = load ptr, ptr %t1247
  %t1264 = call i64 @tml_N4core3str4simd13trim_end_simdE_S(ptr %t1263)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1265)
  store i64 %t1264, ptr %t1265
  %t1266 = load i64, ptr %t1259
  %t1267 = load i64, ptr %t1265
  %t1268 = icmp sge i64 %t1266, %t1267
  br i1 %t1268, label %if.then324, label %if.end326
if.then324:
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1265)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1259)
  ret ptr @.str.1
if.end326:
  %t1269 = load ptr, ptr %t1247
  %t1270 = load i64, ptr %t1259
  %t1271 = load i64, ptr %t1265
  %t1272 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t1269, i64 %t1270, i64 %t1271)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1265)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1259)
  ret ptr %t1272
if.end320:
  %t1273 = load ptr, ptr %t1247
  %t1275 = ptrtoint ptr %t1273 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1276)
  store i64 %t1275, ptr %t1276
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1277)
  store i64 0, ptr %t1277
  br label %loop.preheader327
loop.preheader327:
  br label %loop.header328
loop.header328:
  %t1278 = load i64, ptr %t1277
  %t1279 = load i64, ptr %t1250
  %t1280 = icmp slt i64 %t1278, %t1279
  br i1 %t1280, label %loop.body329, label %loop.exit331
loop.body329:
  %t1281 = load i64, ptr %t1276
  %t1282 = load i64, ptr %t1277
  %t1284 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1281, i64 %t1282)
  %t1283 = extractvalue { i64, i1 } %t1284, 0
  %t1285 = extractvalue { i64, i1 } %t1284, 1
  br i1 %t1285, label %add_overflow333, label %add_ok332
add_overflow333:
  call void @panic(ptr @.str.92)
  unreachable
add_ok332:
  %t1286 = inttoptr i64 %t1283 to ptr
  %t1287 = alloca ptr
  store ptr %t1286, ptr %t1287
  %t1288 = load ptr, ptr %t1287
  %t1289 = load i8, ptr %t1288
  call void @llvm.lifetime.start.p0(i64 1, ptr %t1290)
  store i8 %t1289, ptr %t1290
  %t1291 = load i8, ptr %t1290
  %t1292 = zext i8 %t1291 to i32
  %t1293 = call i1 @tml_N4core3str9transform13is_whitespaceE_i(i32 %t1292)
  %t1294 = xor i1 %t1293, 1
  br i1 %t1294, label %if.then334, label %if.end336
if.then334:
  br label %loop.exit331
if.end336:
  %t1295 = load i64, ptr %t1277
  %t1297 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1295, i64 1)
  %t1296 = extractvalue { i64, i1 } %t1297, 0
  %t1298 = extractvalue { i64, i1 } %t1297, 1
  br i1 %t1298, label %add_overflow338, label %add_ok337
add_overflow338:
  call void @panic(ptr @.str.93)
  unreachable
add_ok337:
  store i64 %t1296, ptr %t1277
  call void @llvm.lifetime.end.p0(i64 1, ptr %t1290)
  br label %loop.latch330
loop.latch330:
  br label %loop.header328, !llvm.loop !1016
loop.exit331:
  %t1299 = load i64, ptr %t1250
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1300)
  store i64 %t1299, ptr %t1300
  br label %loop.preheader339
loop.preheader339:
  br label %loop.header340
loop.header340:
  %t1301 = load i64, ptr %t1300
  %t1302 = load i64, ptr %t1277
  %t1303 = icmp sgt i64 %t1301, %t1302
  br i1 %t1303, label %loop.body341, label %loop.exit343
loop.body341:
  %t1304 = load i64, ptr %t1276
  %t1305 = load i64, ptr %t1300
  %t1307 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1304, i64 %t1305)
  %t1306 = extractvalue { i64, i1 } %t1307, 0
  %t1308 = extractvalue { i64, i1 } %t1307, 1
  br i1 %t1308, label %add_overflow345, label %add_ok344
add_overflow345:
  call void @panic(ptr @.str.94)
  unreachable
add_ok344:
  %t1310 = sext i32 1 to i64
  %t1311 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1306, i64 %t1310)
  %t1309 = extractvalue { i64, i1 } %t1311, 0
  %t1312 = extractvalue { i64, i1 } %t1311, 1
  br i1 %t1312, label %sub_overflow347, label %sub_ok346
sub_overflow347:
  call void @panic(ptr @.str.95)
  unreachable
sub_ok346:
  %t1313 = inttoptr i64 %t1309 to ptr
  %t1314 = alloca ptr
  store ptr %t1313, ptr %t1314
  %t1315 = load ptr, ptr %t1314
  %t1316 = load i8, ptr %t1315
  call void @llvm.lifetime.start.p0(i64 1, ptr %t1317)
  store i8 %t1316, ptr %t1317
  %t1318 = load i8, ptr %t1317
  %t1319 = zext i8 %t1318 to i32
  %t1320 = call i1 @tml_N4core3str9transform13is_whitespaceE_i(i32 %t1319)
  %t1321 = xor i1 %t1320, 1
  br i1 %t1321, label %if.then348, label %if.end350
if.then348:
  br label %loop.exit343
if.end350:
  %t1322 = load i64, ptr %t1300
  %t1324 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1322, i64 1)
  %t1323 = extractvalue { i64, i1 } %t1324, 0
  %t1325 = extractvalue { i64, i1 } %t1324, 1
  br i1 %t1325, label %sub_overflow352, label %sub_ok351
sub_overflow352:
  call void @panic(ptr @.str.96)
  unreachable
sub_ok351:
  store i64 %t1323, ptr %t1300
  call void @llvm.lifetime.end.p0(i64 1, ptr %t1317)
  br label %loop.latch342
loop.latch342:
  br label %loop.header340, !llvm.loop !1017
loop.exit343:
  %t1326 = load i64, ptr %t1277
  %t1327 = load i64, ptr %t1300
  %t1328 = icmp sge i64 %t1326, %t1327
  br i1 %t1328, label %if.then353, label %if.end355
if.then353:
  ret ptr @.str.1
if.end355:
  %t1329 = load ptr, ptr %t1247
  %t1330 = load i64, ptr %t1277
  %t1331 = load i64, ptr %t1300
  %t1332 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t1329, i64 %t1330, i64 %t1331)
  ret ptr %t1332
}

; core::fmt::helpers::i64_to_str
define ptr @tml_N4core3fmt7helpers10i64_to_strE_l(i64 %value) #0 {
entry:
  %t1333 = alloca i64
  store i64 %value, ptr %t1333
  %t1341 = alloca i1
  %t1346 = alloca i64
  %t1353 = alloca i64
  %t1361 = alloca i64
  %t1369 = alloca i64
  %t1380 = alloca i64
  %t1384 = alloca i64
  %t1396 = alloca i64
  %t1402 = alloca i64
  %t1409 = alloca i64
  %t1421 = alloca i8
  %t1429 = alloca i8
  %t1459 = alloca i64
  %t1471 = alloca i8
  %t1479 = alloca i8
  %t1507 = alloca i8
  %t1334 = alloca ptr
  store ptr @.str.97, ptr %t1334
  %t1335 = load i64, ptr %t1333
  %t1337 = sext i32 0 to i64
  %t1336 = icmp eq i64 %t1335, %t1337
  br i1 %t1336, label %if.then356, label %if.end358
if.then356:
  ret ptr @.str.74
if.end358:
  %t1338 = load i64, ptr %t1333
  %t1340 = sext i32 0 to i64
  %t1339 = icmp slt i64 %t1338, %t1340
  call void @llvm.lifetime.start.p0(i64 1, ptr %t1341)
  store i1 %t1339, ptr %t1341
  %t1342 = sub i64 0, 9223372036854775807
  %t1344 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1342, i64 1)
  %t1343 = extractvalue { i64, i1 } %t1344, 0
  %t1345 = extractvalue { i64, i1 } %t1344, 1
  br i1 %t1345, label %sub_overflow360, label %sub_ok359
sub_overflow360:
  call void @panic(ptr @.str.98)
  unreachable
sub_ok359:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1346)
  store i64 %t1343, ptr %t1346
  %t1347 = load i1, ptr %t1341
  %t1348 = load i64, ptr %t1333
  %t1349 = load i64, ptr %t1346
  %t1350 = icmp eq i64 %t1348, %t1349
  %t1351 = and i1 %t1347, %t1350
  br i1 %t1351, label %if.then361, label %if.end363
if.then361:
  ret ptr @.str.99
if.end363:
  %t1352 = load i64, ptr %t1333
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1353)
  store i64 %t1352, ptr %t1353
  %t1354 = load i1, ptr %t1341
  br i1 %t1354, label %if.then364, label %if.end366
if.then364:
  %t1355 = load i64, ptr %t1353
  %t1357 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 0, i64 %t1355)
  %t1356 = extractvalue { i64, i1 } %t1357, 0
  %t1358 = extractvalue { i64, i1 } %t1357, 1
  br i1 %t1358, label %sub_overflow368, label %sub_ok367
sub_overflow368:
  call void @panic(ptr @.str.100)
  unreachable
sub_ok367:
  store i64 %t1356, ptr %t1353
  br label %if.end366
if.end366:
  %t1359 = load i64, ptr %t1353
  %t1360 = call i64 @tml_N4core3fmt7helpers16count_digits_u64E_m(i64 %t1359)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1361)
  store i64 %t1360, ptr %t1361
  %t1362 = load i1, ptr %t1341
  br i1 %t1362, label %if.then369, label %if.else370
if.then369:
  %t1363 = load i64, ptr %t1361
  %t1365 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1363, i64 1)
  %t1364 = extractvalue { i64, i1 } %t1365, 0
  %t1366 = extractvalue { i64, i1 } %t1365, 1
  br i1 %t1366, label %add_overflow373, label %add_ok372
add_overflow373:
  call void @panic(ptr @.str.101)
  unreachable
add_ok372:
  br label %if.end371
if.else370:
  %t1367 = load i64, ptr %t1361
  br label %if.end371
if.end371:
  %t1368 = phi i64 [ %t1364, %add_ok372 ], [ %t1367, %if.else370 ]
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1369)
  store i64 %t1368, ptr %t1369
  %t1370 = load i64, ptr %t1369
  %t1372 = sext i32 1 to i64
  %t1373 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1370, i64 %t1372)
  %t1371 = extractvalue { i64, i1 } %t1373, 0
  %t1374 = extractvalue { i64, i1 } %t1373, 1
  br i1 %t1374, label %add_overflow375, label %add_ok374
add_overflow375:
  call void @panic(ptr @.str.102)
  unreachable
add_ok374:
  %t1375 = call ptr @mem_alloc(i64 %t1371)
  %t1376 = alloca ptr
  store ptr %t1375, ptr %t1376
  %t1377 = load ptr, ptr %t1376
  %t1379 = ptrtoint ptr %t1377 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1380)
  store i64 %t1379, ptr %t1380
  %t1381 = load ptr, ptr %t1334
  %t1383 = ptrtoint ptr %t1381 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1384)
  store i64 %t1383, ptr %t1384
  %t1385 = load i64, ptr %t1380
  %t1386 = load i64, ptr %t1369
  %t1388 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1385, i64 %t1386)
  %t1387 = extractvalue { i64, i1 } %t1388, 0
  %t1389 = extractvalue { i64, i1 } %t1388, 1
  br i1 %t1389, label %add_overflow377, label %add_ok376
add_overflow377:
  call void @panic(ptr @.str.103)
  unreachable
add_ok376:
  %t1390 = inttoptr i64 %t1387 to ptr
  %t1391 = trunc i32 0 to i8
  store i8 %t1391, ptr %t1390
  %t1392 = load i64, ptr %t1369
  %t1394 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1392, i64 1)
  %t1393 = extractvalue { i64, i1 } %t1394, 0
  %t1395 = extractvalue { i64, i1 } %t1394, 1
  br i1 %t1395, label %sub_overflow379, label %sub_ok378
sub_overflow379:
  call void @panic(ptr @.str.104)
  unreachable
sub_ok378:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1396)
  store i64 %t1393, ptr %t1396
  br label %loop.preheader380
loop.preheader380:
  br label %loop.header381
loop.header381:
  %t1397 = load i64, ptr %t1353
  %t1399 = sext i32 100 to i64
  %t1398 = icmp sge i64 %t1397, %t1399
  br i1 %t1398, label %loop.body382, label %loop.exit384
loop.body382:
  %t1400 = load i64, ptr %t1353
  %t1401 = srem i64 %t1400, 100
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1402)
  store i64 %t1401, ptr %t1402
  %t1403 = load i64, ptr %t1353
  %t1404 = sdiv i64 %t1403, 100
  store i64 %t1404, ptr %t1353
  %t1405 = load i64, ptr %t1402
  %t1407 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t1405, i64 2)
  %t1406 = extractvalue { i64, i1 } %t1407, 0
  %t1408 = extractvalue { i64, i1 } %t1407, 1
  br i1 %t1408, label %mul_overflow386, label %mul_ok385
mul_overflow386:
  call void @panic(ptr @.str.105)
  unreachable
mul_ok385:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1409)
  store i64 %t1406, ptr %t1409
  %t1410 = load i64, ptr %t1384
  %t1411 = load i64, ptr %t1409
  %t1413 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1410, i64 %t1411)
  %t1412 = extractvalue { i64, i1 } %t1413, 0
  %t1414 = extractvalue { i64, i1 } %t1413, 1
  br i1 %t1414, label %add_overflow388, label %add_ok387
add_overflow388:
  call void @panic(ptr @.str.106)
  unreachable
add_ok387:
  %t1416 = sext i32 1 to i64
  %t1417 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1412, i64 %t1416)
  %t1415 = extractvalue { i64, i1 } %t1417, 0
  %t1418 = extractvalue { i64, i1 } %t1417, 1
  br i1 %t1418, label %add_overflow390, label %add_ok389
add_overflow390:
  call void @panic(ptr @.str.106)
  unreachable
add_ok389:
  %t1419 = inttoptr i64 %t1415 to ptr
  %t1420 = load i8, ptr %t1419
  call void @llvm.lifetime.start.p0(i64 1, ptr %t1421)
  store i8 %t1420, ptr %t1421
  %t1422 = load i64, ptr %t1384
  %t1423 = load i64, ptr %t1409
  %t1425 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1422, i64 %t1423)
  %t1424 = extractvalue { i64, i1 } %t1425, 0
  %t1426 = extractvalue { i64, i1 } %t1425, 1
  br i1 %t1426, label %add_overflow392, label %add_ok391
add_overflow392:
  call void @panic(ptr @.str.107)
  unreachable
add_ok391:
  %t1427 = inttoptr i64 %t1424 to ptr
  %t1428 = load i8, ptr %t1427
  call void @llvm.lifetime.start.p0(i64 1, ptr %t1429)
  store i8 %t1428, ptr %t1429
  %t1430 = load i64, ptr %t1380
  %t1431 = load i64, ptr %t1396
  %t1433 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1430, i64 %t1431)
  %t1432 = extractvalue { i64, i1 } %t1433, 0
  %t1434 = extractvalue { i64, i1 } %t1433, 1
  br i1 %t1434, label %add_overflow394, label %add_ok393
add_overflow394:
  call void @panic(ptr @.str.108)
  unreachable
add_ok393:
  %t1435 = inttoptr i64 %t1432 to ptr
  %t1436 = load i8, ptr %t1421
  store i8 %t1436, ptr %t1435
  %t1437 = load i64, ptr %t1380
  %t1438 = load i64, ptr %t1396
  %t1440 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1437, i64 %t1438)
  %t1439 = extractvalue { i64, i1 } %t1440, 0
  %t1441 = extractvalue { i64, i1 } %t1440, 1
  br i1 %t1441, label %add_overflow396, label %add_ok395
add_overflow396:
  call void @panic(ptr @.str.109)
  unreachable
add_ok395:
  %t1443 = sext i32 1 to i64
  %t1444 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1439, i64 %t1443)
  %t1442 = extractvalue { i64, i1 } %t1444, 0
  %t1445 = extractvalue { i64, i1 } %t1444, 1
  br i1 %t1445, label %sub_overflow398, label %sub_ok397
sub_overflow398:
  call void @panic(ptr @.str.110)
  unreachable
sub_ok397:
  %t1446 = inttoptr i64 %t1442 to ptr
  %t1447 = load i8, ptr %t1429
  store i8 %t1447, ptr %t1446
  %t1448 = load i64, ptr %t1396
  %t1450 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1448, i64 2)
  %t1449 = extractvalue { i64, i1 } %t1450, 0
  %t1451 = extractvalue { i64, i1 } %t1450, 1
  br i1 %t1451, label %sub_overflow400, label %sub_ok399
sub_overflow400:
  call void @panic(ptr @.str.111)
  unreachable
sub_ok399:
  store i64 %t1449, ptr %t1396
  call void @llvm.lifetime.end.p0(i64 1, ptr %t1429)
  call void @llvm.lifetime.end.p0(i64 1, ptr %t1421)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1409)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1402)
  br label %loop.latch383
loop.latch383:
  br label %loop.header381, !llvm.loop !1018
loop.exit384:
  %t1452 = load i64, ptr %t1353
  %t1454 = sext i32 10 to i64
  %t1453 = icmp sge i64 %t1452, %t1454
  br i1 %t1453, label %if.then401, label %if.else402
if.then401:
  %t1455 = load i64, ptr %t1353
  %t1457 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t1455, i64 2)
  %t1456 = extractvalue { i64, i1 } %t1457, 0
  %t1458 = extractvalue { i64, i1 } %t1457, 1
  br i1 %t1458, label %mul_overflow405, label %mul_ok404
mul_overflow405:
  call void @panic(ptr @.str.112)
  unreachable
mul_ok404:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1459)
  store i64 %t1456, ptr %t1459
  %t1460 = load i64, ptr %t1384
  %t1461 = load i64, ptr %t1459
  %t1463 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1460, i64 %t1461)
  %t1462 = extractvalue { i64, i1 } %t1463, 0
  %t1464 = extractvalue { i64, i1 } %t1463, 1
  br i1 %t1464, label %add_overflow407, label %add_ok406
add_overflow407:
  call void @panic(ptr @.str.113)
  unreachable
add_ok406:
  %t1466 = sext i32 1 to i64
  %t1467 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1462, i64 %t1466)
  %t1465 = extractvalue { i64, i1 } %t1467, 0
  %t1468 = extractvalue { i64, i1 } %t1467, 1
  br i1 %t1468, label %add_overflow409, label %add_ok408
add_overflow409:
  call void @panic(ptr @.str.113)
  unreachable
add_ok408:
  %t1469 = inttoptr i64 %t1465 to ptr
  %t1470 = load i8, ptr %t1469
  call void @llvm.lifetime.start.p0(i64 1, ptr %t1471)
  store i8 %t1470, ptr %t1471
  %t1472 = load i64, ptr %t1384
  %t1473 = load i64, ptr %t1459
  %t1475 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1472, i64 %t1473)
  %t1474 = extractvalue { i64, i1 } %t1475, 0
  %t1476 = extractvalue { i64, i1 } %t1475, 1
  br i1 %t1476, label %add_overflow411, label %add_ok410
add_overflow411:
  call void @panic(ptr @.str.114)
  unreachable
add_ok410:
  %t1477 = inttoptr i64 %t1474 to ptr
  %t1478 = load i8, ptr %t1477
  call void @llvm.lifetime.start.p0(i64 1, ptr %t1479)
  store i8 %t1478, ptr %t1479
  %t1480 = load i64, ptr %t1380
  %t1481 = load i64, ptr %t1396
  %t1483 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1480, i64 %t1481)
  %t1482 = extractvalue { i64, i1 } %t1483, 0
  %t1484 = extractvalue { i64, i1 } %t1483, 1
  br i1 %t1484, label %add_overflow413, label %add_ok412
add_overflow413:
  call void @panic(ptr @.str.115)
  unreachable
add_ok412:
  %t1485 = inttoptr i64 %t1482 to ptr
  %t1486 = load i8, ptr %t1471
  store i8 %t1486, ptr %t1485
  %t1487 = load i64, ptr %t1380
  %t1488 = load i64, ptr %t1396
  %t1490 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1487, i64 %t1488)
  %t1489 = extractvalue { i64, i1 } %t1490, 0
  %t1491 = extractvalue { i64, i1 } %t1490, 1
  br i1 %t1491, label %add_overflow415, label %add_ok414
add_overflow415:
  call void @panic(ptr @.str.3)
  unreachable
add_ok414:
  %t1493 = sext i32 1 to i64
  %t1494 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1489, i64 %t1493)
  %t1492 = extractvalue { i64, i1 } %t1494, 0
  %t1495 = extractvalue { i64, i1 } %t1494, 1
  br i1 %t1495, label %sub_overflow417, label %sub_ok416
sub_overflow417:
  call void @panic(ptr @.str.116)
  unreachable
sub_ok416:
  %t1496 = inttoptr i64 %t1492 to ptr
  %t1497 = load i8, ptr %t1479
  store i8 %t1497, ptr %t1496
  call void @llvm.lifetime.end.p0(i64 1, ptr %t1479)
  call void @llvm.lifetime.end.p0(i64 1, ptr %t1471)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1459)
  br label %if.end403
if.else402:
  %t1498 = load i64, ptr %t1353
  %t1500 = sext i32 0 to i64
  %t1499 = icmp sgt i64 %t1498, %t1500
  br i1 %t1499, label %if.then418, label %if.end420
if.then418:
  %t1501 = load i64, ptr %t1353
  %t1502 = trunc i64 %t1501 to i8
  %t1503 = trunc i32 48 to i8
  %t1505 = call { i8, i1 } @llvm.uadd.with.overflow.i8(i8 %t1502, i8 %t1503)
  %t1504 = extractvalue { i8, i1 } %t1505, 0
  %t1506 = extractvalue { i8, i1 } %t1505, 1
  br i1 %t1506, label %add_overflow422, label %add_ok421
add_overflow422:
  call void @panic(ptr @.str.117)
  unreachable
add_ok421:
  call void @llvm.lifetime.start.p0(i64 1, ptr %t1507)
  store i8 %t1504, ptr %t1507
  %t1508 = load i64, ptr %t1380
  %t1509 = load i64, ptr %t1396
  %t1511 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1508, i64 %t1509)
  %t1510 = extractvalue { i64, i1 } %t1511, 0
  %t1512 = extractvalue { i64, i1 } %t1511, 1
  br i1 %t1512, label %add_overflow424, label %add_ok423
add_overflow424:
  call void @panic(ptr @.str.118)
  unreachable
add_ok423:
  %t1513 = inttoptr i64 %t1510 to ptr
  %t1514 = load i8, ptr %t1507
  store i8 %t1514, ptr %t1513
  call void @llvm.lifetime.end.p0(i64 1, ptr %t1507)
  br label %if.end420
if.end420:
  br label %if.end403
if.end403:
  %t1515 = load i1, ptr %t1341
  br i1 %t1515, label %if.then425, label %if.end427
if.then425:
  %t1516 = load ptr, ptr %t1376
  %t1517 = trunc i32 45 to i8
  store i8 %t1517, ptr %t1516
  br label %if.end427
if.end427:
  %t1518 = load ptr, ptr %t1376
  ret ptr %t1518
}

; core::str::split::lines
define %struct.List__Str @tml_N4core3str5split5linesE_S(ptr %s) #0 {
entry:
  %t1519 = alloca ptr
  store ptr %s, ptr %t1519
  %t1521 = alloca %struct.List__Str
  %t1524 = alloca i64
  %t1537 = alloca i64
  %t1538 = alloca i64
  %t1547 = alloca i64
  %t1563 = alloca i64
  %t1568 = alloca i64
  %t1583 = alloca i8
  %t1602 = alloca i64
  %t1604 = alloca i64
  %t1619 = alloca i8
  %t1520 = call %struct.List__Str @tml_N3std11collections4list9List__Str3newE(i64 8)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t1521)
  store %struct.List__Str %t1520, ptr %t1521
  %t1522 = load ptr, ptr %t1519
  %t1523 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t1522)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1524)
  store i64 %t1523, ptr %t1524
  %t1525 = load i64, ptr %t1524
  %t1527 = sext i32 0 to i64
  %t1526 = icmp eq i64 %t1525, %t1527
  br i1 %t1526, label %if.then428, label %if.end430
if.then428:
  %t1528 = load %struct.List__Str, ptr %t1521
  ret %struct.List__Str %t1528
if.end430:
  %t1529 = load i64, ptr %t1524
  %t1531 = sext i32 32 to i64
  %t1530 = icmp sge i64 %t1529, %t1531
  br i1 %t1530, label %if.then431, label %if.end433
if.then431:
  %t1532 = load ptr, ptr %t1519
  %t1533 = call %struct.List__Str @tml_N4core3str4simd16split_lines_simdE_S(ptr %t1532)
  ret %struct.List__Str %t1533
if.end433:
  %t1534 = load ptr, ptr %t1519
  %t1536 = ptrtoint ptr %t1534 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1537)
  store i64 %t1536, ptr %t1537
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1538)
  store i64 0, ptr %t1538
  br label %loop.preheader434
loop.preheader434:
  br label %loop.header435
loop.header435:
  %t1539 = load i64, ptr %t1538
  %t1540 = load i64, ptr %t1524
  %t1541 = icmp sle i64 %t1539, %t1540
  br i1 %t1541, label %loop.body436, label %loop.exit438
loop.body436:
  %t1542 = load i64, ptr %t1524
  %t1543 = load i64, ptr %t1538
  %t1545 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1542, i64 %t1543)
  %t1544 = extractvalue { i64, i1 } %t1545, 0
  %t1546 = extractvalue { i64, i1 } %t1545, 1
  br i1 %t1546, label %sub_overflow440, label %sub_ok439
sub_overflow440:
  call void @panic(ptr @.str.119)
  unreachable
sub_ok439:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1547)
  store i64 %t1544, ptr %t1547
  %t1548 = load i64, ptr %t1547
  %t1550 = sext i32 0 to i64
  %t1549 = icmp sle i64 %t1548, %t1550
  br i1 %t1549, label %if.then441, label %if.end443
if.then441:
  br label %loop.exit438
if.end443:
  %t1551 = load i64, ptr %t1537
  %t1552 = load i64, ptr %t1538
  %t1554 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1551, i64 %t1552)
  %t1553 = extractvalue { i64, i1 } %t1554, 0
  %t1555 = extractvalue { i64, i1 } %t1554, 1
  br i1 %t1555, label %add_overflow445, label %add_ok444
add_overflow445:
  call void @panic(ptr @.str.120)
  unreachable
add_ok444:
  %t1556 = inttoptr i64 %t1553 to ptr
  %t1557 = load i64, ptr %t1547
  %t1558 = call ptr @memchr(ptr %t1556, i32 10, i64 %t1557)
  %t1559 = alloca ptr
  store ptr %t1558, ptr %t1559
  %t1560 = load ptr, ptr %t1559
  %t1562 = ptrtoint ptr %t1560 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1563)
  store i64 %t1562, ptr %t1563
  %t1564 = load i64, ptr %t1563
  %t1566 = sext i32 0 to i64
  %t1565 = icmp eq i64 %t1564, %t1566
  br i1 %t1565, label %if.then446, label %if.end448
if.then446:
  %t1567 = load i64, ptr %t1524
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1568)
  store i64 %t1567, ptr %t1568
  %t1569 = load i64, ptr %t1568
  %t1570 = load i64, ptr %t1538
  %t1571 = icmp sgt i64 %t1569, %t1570
  br i1 %t1571, label %if.then449, label %if.end451
if.then449:
  %t1572 = load i64, ptr %t1537
  %t1573 = load i64, ptr %t1568
  %t1575 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1572, i64 %t1573)
  %t1574 = extractvalue { i64, i1 } %t1575, 0
  %t1576 = extractvalue { i64, i1 } %t1575, 1
  br i1 %t1576, label %add_overflow453, label %add_ok452
add_overflow453:
  call void @panic(ptr @.str.19)
  unreachable
add_ok452:
  %t1578 = sext i32 1 to i64
  %t1579 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1574, i64 %t1578)
  %t1577 = extractvalue { i64, i1 } %t1579, 0
  %t1580 = extractvalue { i64, i1 } %t1579, 1
  br i1 %t1580, label %sub_overflow455, label %sub_ok454
sub_overflow455:
  call void @panic(ptr @.str.121)
  unreachable
sub_ok454:
  %t1581 = inttoptr i64 %t1577 to ptr
  %t1582 = load i8, ptr %t1581
  call void @llvm.lifetime.start.p0(i64 1, ptr %t1583)
  store i8 %t1582, ptr %t1583
  %t1584 = load i8, ptr %t1583
  %t1585 = zext i8 %t1584 to i32
  %t1586 = icmp eq i32 %t1585, 13
  br i1 %t1586, label %if.then456, label %if.end458
if.then456:
  %t1587 = load i64, ptr %t1568
  %t1589 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1587, i64 1)
  %t1588 = extractvalue { i64, i1 } %t1589, 0
  %t1590 = extractvalue { i64, i1 } %t1589, 1
  br i1 %t1590, label %sub_overflow460, label %sub_ok459
sub_overflow460:
  call void @panic(ptr @.str.122)
  unreachable
sub_ok459:
  store i64 %t1588, ptr %t1568
  br label %if.end458
if.end458:
  call void @llvm.lifetime.end.p0(i64 1, ptr %t1583)
  br label %if.end451
if.end451:
  %t1591 = load %struct.List__Str, ptr %t1521
  %t1592 = load ptr, ptr %t1519
  %t1593 = load i64, ptr %t1538
  %t1594 = load i64, ptr %t1568
  %t1595 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t1592, i64 %t1593, i64 %t1594)
  %t1596 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t1521, ptr %t1595)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1568)
  br label %loop.exit438
if.end448:
  %t1597 = load i64, ptr %t1563
  %t1598 = load i64, ptr %t1537
  %t1600 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1597, i64 %t1598)
  %t1599 = extractvalue { i64, i1 } %t1600, 0
  %t1601 = extractvalue { i64, i1 } %t1600, 1
  br i1 %t1601, label %sub_overflow462, label %sub_ok461
sub_overflow462:
  call void @panic(ptr @.str.123)
  unreachable
sub_ok461:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1602)
  store i64 %t1599, ptr %t1602
  %t1603 = load i64, ptr %t1602
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1604)
  store i64 %t1603, ptr %t1604
  %t1605 = load i64, ptr %t1604
  %t1606 = load i64, ptr %t1538
  %t1607 = icmp sgt i64 %t1605, %t1606
  br i1 %t1607, label %if.then463, label %if.end465
if.then463:
  %t1608 = load i64, ptr %t1537
  %t1609 = load i64, ptr %t1604
  %t1611 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1608, i64 %t1609)
  %t1610 = extractvalue { i64, i1 } %t1611, 0
  %t1612 = extractvalue { i64, i1 } %t1611, 1
  br i1 %t1612, label %add_overflow467, label %add_ok466
add_overflow467:
  call void @panic(ptr @.str.124)
  unreachable
add_ok466:
  %t1614 = sext i32 1 to i64
  %t1615 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1610, i64 %t1614)
  %t1613 = extractvalue { i64, i1 } %t1615, 0
  %t1616 = extractvalue { i64, i1 } %t1615, 1
  br i1 %t1616, label %sub_overflow469, label %sub_ok468
sub_overflow469:
  call void @panic(ptr @.str.125)
  unreachable
sub_ok468:
  %t1617 = inttoptr i64 %t1613 to ptr
  %t1618 = load i8, ptr %t1617
  call void @llvm.lifetime.start.p0(i64 1, ptr %t1619)
  store i8 %t1618, ptr %t1619
  %t1620 = load i8, ptr %t1619
  %t1621 = zext i8 %t1620 to i32
  %t1622 = icmp eq i32 %t1621, 13
  br i1 %t1622, label %if.then470, label %if.end472
if.then470:
  %t1623 = load i64, ptr %t1604
  %t1625 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1623, i64 1)
  %t1624 = extractvalue { i64, i1 } %t1625, 0
  %t1626 = extractvalue { i64, i1 } %t1625, 1
  br i1 %t1626, label %sub_overflow474, label %sub_ok473
sub_overflow474:
  call void @panic(ptr @.str.126)
  unreachable
sub_ok473:
  store i64 %t1624, ptr %t1604
  br label %if.end472
if.end472:
  call void @llvm.lifetime.end.p0(i64 1, ptr %t1619)
  br label %if.end465
if.end465:
  %t1627 = load %struct.List__Str, ptr %t1521
  %t1628 = load ptr, ptr %t1519
  %t1629 = load i64, ptr %t1538
  %t1630 = load i64, ptr %t1604
  %t1631 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t1628, i64 %t1629, i64 %t1630)
  %t1632 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t1521, ptr %t1631)
  %t1633 = load i64, ptr %t1602
  %t1635 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1633, i64 1)
  %t1634 = extractvalue { i64, i1 } %t1635, 0
  %t1636 = extractvalue { i64, i1 } %t1635, 1
  br i1 %t1636, label %add_overflow476, label %add_ok475
add_overflow476:
  call void @panic(ptr @.str.127)
  unreachable
add_ok475:
  store i64 %t1634, ptr %t1538
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1604)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1602)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1563)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1547)
  br label %loop.latch437
loop.latch437:
  br label %loop.header435, !llvm.loop !1019
loop.exit438:
  %t1637 = load %struct.List__Str, ptr %t1521
  ret %struct.List__Str %t1637
}

; ir_diff::parser::is_skippable_preamble
define internal i1 @tml_N7ir_diff6parser21is_skippable_preambleE_S(ptr %line) #0 {
entry:
  %t1638 = alloca ptr
  store ptr %line, ptr %t1638
  %t1639 = load ptr, ptr %t1638
  %t1640 = call i64 @strlen(ptr %t1639)
  %t1642 = sext i32 0 to i64
  %t1641 = icmp eq i64 %t1640, %t1642
  br i1 %t1641, label %if.then477, label %if.end479
if.then477:
  ret i1 1
if.end479:
  %t1643 = load ptr, ptr %t1638
  %t1644 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t1643, ptr @.str.58)
  br i1 %t1644, label %if.then480, label %if.end482
if.then480:
  ret i1 1
if.end482:
  %t1645 = load ptr, ptr %t1638
  %t1646 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t1645, ptr @.str.128)
  br i1 %t1646, label %if.then483, label %if.end485
if.then483:
  ret i1 1
if.end485:
  %t1647 = load ptr, ptr %t1638
  %t1648 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t1647, ptr @.str.129)
  br i1 %t1648, label %if.then486, label %if.end488
if.then486:
  ret i1 1
if.end488:
  %t1649 = load ptr, ptr %t1638
  %t1650 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t1649, ptr @.str.130)
  br i1 %t1650, label %if.then489, label %if.end491
if.then489:
  ret i1 1
if.end491:
  %t1651 = load ptr, ptr %t1638
  %t1652 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t1651, ptr @.str.131)
  br i1 %t1652, label %if.then492, label %if.end494
if.then492:
  ret i1 1
if.end494:
  ret i1 0
}

; core::str::split::split_once
define %struct.Maybe__tuple_Str_Str @tml_N4core3str5split10split_onceE_SS(ptr %s, ptr %delim) #0 {
entry:
  %t1653 = alloca ptr
  store ptr %s, ptr %t1653
  %t1654 = alloca ptr
  store ptr %delim, ptr %t1654
  %t1658 = alloca %struct.Maybe__I64
  %t1670 = alloca i64
  %t1673 = alloca i64
  %t1676 = alloca i64
  %t1655 = load ptr, ptr %t1653
  %t1656 = load ptr, ptr %t1654
  %t1657 = call %struct.Maybe__I64 @tml_N4core3str6search4findE_SS(ptr %t1655, ptr %t1656)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t1658)
  store %struct.Maybe__I64 %t1657, ptr %t1658
  %t1659 = load %struct.Maybe__I64, ptr %t1658
  %t1660 = extractvalue %struct.Maybe__I64 %t1659, 0
  %t1661 = icmp eq i32 %t1660, 1
  br i1 %t1661, label %if.then495, label %if.end497
if.then495:
  %t1663 = alloca %struct.Maybe__tuple_Str_Str, align 8
  %t1664 = getelementptr inbounds %struct.Maybe__tuple_Str_Str, ptr %t1663, i32 0, i32 0
  store i32 1, ptr %t1664
  %t1662 = load %struct.Maybe__tuple_Str_Str, ptr %t1663
  ret %struct.Maybe__tuple_Str_Str %t1662
if.end497:
  %t1665 = load %struct.Maybe__I64, ptr %t1658
  %t1666 = extractvalue %struct.Maybe__I64 %t1665, 0
  %t1667 = alloca %struct.Maybe__I64
  store %struct.Maybe__I64 %t1665, ptr %t1667
  %t1668 = getelementptr inbounds %struct.Maybe__I64, ptr %t1667, i32 0, i32 1
  %t1669 = load i64, ptr %t1668
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1670)
  store i64 %t1669, ptr %t1670
  %t1671 = load ptr, ptr %t1654
  %t1672 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t1671)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1673)
  store i64 %t1672, ptr %t1673
  %t1674 = load ptr, ptr %t1653
  %t1675 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t1674)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1676)
  store i64 %t1675, ptr %t1676
  %t1677 = load ptr, ptr %t1653
  %t1678 = sext i32 0 to i64
  %t1679 = load i64, ptr %t1670
  %t1680 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t1677, i64 %t1678, i64 %t1679)
  %t1681 = alloca ptr
  store ptr %t1680, ptr %t1681
  %t1682 = load ptr, ptr %t1653
  %t1683 = load i64, ptr %t1670
  %t1684 = load i64, ptr %t1673
  %t1686 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1683, i64 %t1684)
  %t1685 = extractvalue { i64, i1 } %t1686, 0
  %t1687 = extractvalue { i64, i1 } %t1686, 1
  br i1 %t1687, label %add_overflow499, label %add_ok498
add_overflow499:
  call void @panic(ptr @.str.132)
  unreachable
add_ok498:
  %t1688 = load i64, ptr %t1676
  %t1689 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t1682, i64 %t1685, i64 %t1688)
  %t1690 = alloca ptr
  store ptr %t1689, ptr %t1690
  %t1692 = alloca %struct.Maybe__tuple_Str_Str, align 8
  %t1693 = getelementptr inbounds %struct.Maybe__tuple_Str_Str, ptr %t1692, i32 0, i32 0
  store i32 0, ptr %t1693
  %t1694 = load ptr, ptr %t1681
  %t1695 = load ptr, ptr %t1690
  %t1696 = alloca { ptr, ptr }
  %t1697 = getelementptr inbounds { ptr, ptr }, ptr %t1696, i32 0, i32 0
  store ptr %t1694, ptr %t1697
  %t1698 = getelementptr inbounds { ptr, ptr }, ptr %t1696, i32 0, i32 1
  store ptr %t1695, ptr %t1698
  %t1699 = load { ptr, ptr }, ptr %t1696
  %t1700 = getelementptr inbounds %struct.Maybe__tuple_Str_Str, ptr %t1692, i32 0, i32 1
  %t1701 = bitcast ptr %t1700 to ptr
  store { ptr, ptr } %t1699, ptr %t1701
  %t1691 = load %struct.Maybe__tuple_Str_Str, ptr %t1692
  ret %struct.Maybe__tuple_Str_Str %t1691
}

; ir_diff::parser::parse_function_header
define internal %struct.Maybe__FnHeader @tml_N7ir_diff6parser21parse_function_headerE_S(ptr %line) #0 {
entry:
  %t1702 = alloca ptr
  store ptr %line, ptr %t1702
  %t1711 = alloca %struct.Maybe__I64
  %t1723 = alloca i64
  %t1732 = alloca %struct.List__Str
  %t1756 = alloca %struct.Maybe__I64
  %t1771 = alloca i64
  %t1787 = alloca %struct.List__U8
  %t1790 = alloca i64
  %t1791 = alloca i64
  %t1792 = alloca i64
  %t1794 = alloca i64
  %t1801 = alloca i8
  %t1804 = alloca i64
  %t1855 = alloca %struct.List__IrParam
  %t1871 = alloca %struct.List__Str
  %t1703 = load ptr, ptr %t1702
  %t1704 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t1703, ptr @.str.52)
  %t1705 = xor i1 %t1704, 1
  br i1 %t1705, label %if.then500, label %if.end502
if.then500:
  %t1707 = alloca %struct.Maybe__FnHeader, align 8
  %t1708 = getelementptr inbounds %struct.Maybe__FnHeader, ptr %t1707, i32 0, i32 0
  store i32 1, ptr %t1708
  %t1706 = load %struct.Maybe__FnHeader, ptr %t1707
  ret %struct.Maybe__FnHeader %t1706
if.end502:
  %t1709 = load ptr, ptr %t1702
  %t1710 = call %struct.Maybe__I64 @tml_N4core3str6search4findE_SS(ptr %t1709, ptr @.str.133)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t1711)
  store %struct.Maybe__I64 %t1710, ptr %t1711
  %t1712 = load %struct.Maybe__I64, ptr %t1711
  %t1713 = extractvalue %struct.Maybe__I64 %t1712, 0
  %t1714 = icmp eq i32 %t1713, 1
  br i1 %t1714, label %if.then503, label %if.end505
if.then503:
  %t1716 = alloca %struct.Maybe__FnHeader, align 8
  %t1717 = getelementptr inbounds %struct.Maybe__FnHeader, ptr %t1716, i32 0, i32 0
  store i32 1, ptr %t1717
  %t1715 = load %struct.Maybe__FnHeader, ptr %t1716
  ret %struct.Maybe__FnHeader %t1715
if.end505:
  %t1718 = load %struct.Maybe__I64, ptr %t1711
  %t1719 = extractvalue %struct.Maybe__I64 %t1718, 0
  %t1720 = alloca %struct.Maybe__I64
  store %struct.Maybe__I64 %t1718, ptr %t1720
  %t1721 = getelementptr inbounds %struct.Maybe__I64, ptr %t1720, i32 0, i32 1
  %t1722 = load i64, ptr %t1721
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1723)
  store i64 %t1722, ptr %t1723
  %t1724 = load ptr, ptr %t1702
  %t1725 = sext i32 7 to i64
  %t1726 = load i64, ptr %t1723
  %t1727 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t1724, i64 %t1725, i64 %t1726)
  %t1728 = alloca ptr
  store ptr %t1727, ptr %t1728
  %t1729 = load ptr, ptr %t1728
  %t1730 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t1729)
  %t1731 = call %struct.List__Str @tml_N4core3str5split5splitE_SS(ptr %t1730, ptr @.str.134)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t1732)
  store %struct.List__Str %t1731, ptr %t1732
  %t1733 = alloca ptr
  store ptr @.str.1, ptr %t1733
  %t1734 = load %struct.List__Str, ptr %t1732
  %t1735 = call i64 @tml_N3std11collections4list9List__Str3lenE(ptr %t1732)
  %t1737 = sext i32 0 to i64
  %t1736 = icmp sgt i64 %t1735, %t1737
  br i1 %t1736, label %if.then506, label %if.end508
if.then506:
  %t1738 = load %struct.List__Str, ptr %t1732
  %t1739 = load %struct.List__Str, ptr %t1732
  %t1740 = call i64 @tml_N3std11collections4list9List__Str3lenE(ptr %t1732)
  %t1742 = sext i32 1 to i64
  %t1743 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1740, i64 %t1742)
  %t1741 = extractvalue { i64, i1 } %t1743, 0
  %t1744 = extractvalue { i64, i1 } %t1743, 1
  br i1 %t1744, label %sub_overflow510, label %sub_ok509
sub_overflow510:
  call void @panic(ptr @.str.135)
  unreachable
sub_ok509:
  %t1745 = call ptr @tml_N3std11collections4list9List__Str3getE(ptr %t1732, i64 %t1741)
  store ptr %t1745, ptr %t1733
  br label %if.end508
if.end508:
  %t1746 = load ptr, ptr %t1702
  %t1747 = load i64, ptr %t1723
  %t1749 = sext i32 2 to i64
  %t1750 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1747, i64 %t1749)
  %t1748 = extractvalue { i64, i1 } %t1750, 0
  %t1751 = extractvalue { i64, i1 } %t1750, 1
  br i1 %t1751, label %add_overflow512, label %add_ok511
add_overflow512:
  call void @panic(ptr @.str.136)
  unreachable
add_ok511:
  %t1752 = call ptr @tml_N4core3str5basic14substring_fromE_Sl(ptr %t1746, i64 %t1748)
  %t1753 = alloca ptr
  store ptr %t1752, ptr %t1753
  %t1754 = load ptr, ptr %t1753
  %t1755 = call %struct.Maybe__I64 @tml_N4core3str6search4findE_SS(ptr %t1754, ptr @.str.137)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t1756)
  store %struct.Maybe__I64 %t1755, ptr %t1756
  %t1757 = load %struct.Maybe__I64, ptr %t1756
  %t1758 = extractvalue %struct.Maybe__I64 %t1757, 0
  %t1759 = icmp eq i32 %t1758, 1
  br i1 %t1759, label %if.then513, label %if.end515
if.then513:
  %t1761 = alloca %struct.Maybe__FnHeader, align 8
  %t1762 = getelementptr inbounds %struct.Maybe__FnHeader, ptr %t1761, i32 0, i32 0
  store i32 1, ptr %t1762
  %t1760 = load %struct.Maybe__FnHeader, ptr %t1761
  %t1763 = load ptr, ptr %t1753
  call void @tml_str_free(ptr %t1763)
  %t1764 = load %struct.List__Str, ptr %t1732
  call void @tml_N3std11collections4list9List__Str4dropE(ptr %t1732)
  %t1765 = load ptr, ptr %t1728
  call void @tml_str_free(ptr %t1765)
  ret %struct.Maybe__FnHeader %t1760
if.end515:
  %t1766 = load %struct.Maybe__I64, ptr %t1756
  %t1767 = extractvalue %struct.Maybe__I64 %t1766, 0
  %t1768 = alloca %struct.Maybe__I64
  store %struct.Maybe__I64 %t1766, ptr %t1768
  %t1769 = getelementptr inbounds %struct.Maybe__I64, ptr %t1768, i32 0, i32 1
  %t1770 = load i64, ptr %t1769
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1771)
  store i64 %t1770, ptr %t1771
  %t1772 = load ptr, ptr %t1753
  %t1773 = sext i32 0 to i64
  %t1774 = load i64, ptr %t1771
  %t1775 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t1772, i64 %t1773, i64 %t1774)
  %t1776 = alloca ptr
  store ptr %t1775, ptr %t1776
  %t1777 = load ptr, ptr %t1753
  %t1778 = load i64, ptr %t1771
  %t1780 = sext i32 1 to i64
  %t1781 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1778, i64 %t1780)
  %t1779 = extractvalue { i64, i1 } %t1781, 0
  %t1782 = extractvalue { i64, i1 } %t1781, 1
  br i1 %t1782, label %add_overflow517, label %add_ok516
add_overflow517:
  call void @panic(ptr @.str.138)
  unreachable
add_ok516:
  %t1783 = call ptr @tml_N4core3str5basic14substring_fromE_Sl(ptr %t1777, i64 %t1779)
  %t1784 = alloca ptr
  store ptr %t1783, ptr %t1784
  %t1785 = load ptr, ptr %t1784
  %t1786 = call %struct.List__U8 @tml_N4core3str7convert5bytesE_S(ptr %t1785)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t1787)
  store %struct.List__U8 %t1786, ptr %t1787
  %t1788 = load ptr, ptr %t1784
  %t1789 = call i64 @strlen(ptr %t1788)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1790)
  store i64 %t1789, ptr %t1790
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1791)
  store i64 1, ptr %t1791
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1792)
  store i64 0, ptr %t1792
  %t1793 = sub i64 0, 1
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1794)
  store i64 %t1793, ptr %t1794
  br label %loop.preheader518
loop.preheader518:
  br label %loop.header519
loop.header519:
  %t1795 = load i64, ptr %t1792
  %t1796 = load i64, ptr %t1790
  %t1797 = icmp slt i64 %t1795, %t1796
  br i1 %t1797, label %loop.body520, label %loop.exit522
loop.body520:
  %t1798 = load %struct.List__U8, ptr %t1787
  %t1799 = load i64, ptr %t1792
  %t1800 = call i8 @tml_N3std11collections4list8List__U83getE(ptr %t1787, i64 %t1799)
  call void @llvm.lifetime.start.p0(i64 1, ptr %t1801)
  store i8 %t1800, ptr %t1801
  %t1802 = load i8, ptr %t1801
  %t1803 = zext i8 %t1802 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1804)
  store i64 %t1803, ptr %t1804
  %t1805 = load i64, ptr %t1804
  %t1807 = sext i32 40 to i64
  %t1806 = icmp eq i64 %t1805, %t1807
  br i1 %t1806, label %if.then523, label %if.else524
if.then523:
  %t1808 = load i64, ptr %t1791
  %t1810 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1808, i64 1)
  %t1809 = extractvalue { i64, i1 } %t1810, 0
  %t1811 = extractvalue { i64, i1 } %t1810, 1
  br i1 %t1811, label %add_overflow527, label %add_ok526
add_overflow527:
  call void @panic(ptr @.str.139)
  unreachable
add_ok526:
  store i64 %t1809, ptr %t1791
  br label %if.end525
if.else524:
  %t1812 = load i64, ptr %t1804
  %t1814 = sext i32 41 to i64
  %t1813 = icmp eq i64 %t1812, %t1814
  br i1 %t1813, label %if.then528, label %if.end530
if.then528:
  %t1815 = load i64, ptr %t1791
  %t1817 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1815, i64 1)
  %t1816 = extractvalue { i64, i1 } %t1817, 0
  %t1818 = extractvalue { i64, i1 } %t1817, 1
  br i1 %t1818, label %sub_overflow532, label %sub_ok531
sub_overflow532:
  call void @panic(ptr @.str.140)
  unreachable
sub_ok531:
  store i64 %t1816, ptr %t1791
  %t1819 = load i64, ptr %t1791
  %t1821 = sext i32 0 to i64
  %t1820 = icmp eq i64 %t1819, %t1821
  br i1 %t1820, label %if.then533, label %if.end535
if.then533:
  %t1822 = load i64, ptr %t1792
  store i64 %t1822, ptr %t1794
  br label %loop.exit522
if.end535:
  br label %if.end530
if.end530:
  br label %if.end525
if.end525:
  %t1823 = load i64, ptr %t1792
  %t1825 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1823, i64 1)
  %t1824 = extractvalue { i64, i1 } %t1825, 0
  %t1826 = extractvalue { i64, i1 } %t1825, 1
  br i1 %t1826, label %add_overflow537, label %add_ok536
add_overflow537:
  call void @panic(ptr @.str.33)
  unreachable
add_ok536:
  store i64 %t1824, ptr %t1792
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1804)
  call void @llvm.lifetime.end.p0(i64 1, ptr %t1801)
  br label %loop.latch521
loop.latch521:
  br label %loop.header519, !llvm.loop !1020
loop.exit522:
  %t1827 = load i64, ptr %t1794
  %t1829 = sext i32 0 to i64
  %t1828 = icmp slt i64 %t1827, %t1829
  br i1 %t1828, label %if.then538, label %if.end540
if.then538:
  %t1831 = alloca %struct.Maybe__FnHeader, align 8
  %t1832 = getelementptr inbounds %struct.Maybe__FnHeader, ptr %t1831, i32 0, i32 0
  store i32 1, ptr %t1832
  %t1830 = load %struct.Maybe__FnHeader, ptr %t1831
  %t1833 = load %struct.List__U8, ptr %t1787
  call void @tml_N3std11collections4list8List__U84dropE(ptr %t1787)
  %t1834 = load ptr, ptr %t1784
  call void @tml_str_free(ptr %t1834)
  %t1835 = load ptr, ptr %t1776
  call void @tml_str_free(ptr %t1835)
  %t1836 = load ptr, ptr %t1753
  call void @tml_str_free(ptr %t1836)
  %t1837 = load %struct.List__Str, ptr %t1732
  call void @tml_N3std11collections4list9List__Str4dropE(ptr %t1732)
  %t1838 = load ptr, ptr %t1728
  call void @tml_str_free(ptr %t1838)
  ret %struct.Maybe__FnHeader %t1830
if.end540:
  %t1839 = load ptr, ptr %t1784
  %t1840 = sext i32 0 to i64
  %t1841 = load i64, ptr %t1794
  %t1842 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t1839, i64 %t1840, i64 %t1841)
  %t1843 = alloca ptr
  store ptr %t1842, ptr %t1843
  %t1844 = load ptr, ptr %t1784
  %t1845 = load i64, ptr %t1794
  %t1847 = sext i32 1 to i64
  %t1848 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1845, i64 %t1847)
  %t1846 = extractvalue { i64, i1 } %t1848, 0
  %t1849 = extractvalue { i64, i1 } %t1848, 1
  br i1 %t1849, label %add_overflow542, label %add_ok541
add_overflow542:
  call void @panic(ptr @.str.141)
  unreachable
add_ok541:
  %t1850 = call ptr @tml_N4core3str5basic14substring_fromE_Sl(ptr %t1844, i64 %t1846)
  %t1851 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t1850)
  %t1852 = alloca ptr
  store ptr %t1851, ptr %t1852
  %t1853 = load ptr, ptr %t1843
  %t1854 = call %struct.List__IrParam @tml_N7ir_diff6parser12parse_paramsE_S(ptr %t1853)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t1855)
  store %struct.List__IrParam %t1854, ptr %t1855
  %t1856 = load ptr, ptr %t1852
  %t1857 = alloca ptr
  store ptr %t1856, ptr %t1857
  %t1858 = load ptr, ptr %t1857
  %t1859 = call i1 @tml_N4core3str6search9ends_withE_SS(ptr %t1858, ptr @.str.142)
  br i1 %t1859, label %if.then543, label %if.end545
if.then543:
  %t1860 = load ptr, ptr %t1857
  %t1861 = sext i32 0 to i64
  %t1862 = load ptr, ptr %t1857
  %t1863 = call i64 @strlen(ptr %t1862)
  %t1865 = sext i32 1 to i64
  %t1866 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1863, i64 %t1865)
  %t1864 = extractvalue { i64, i1 } %t1866, 0
  %t1867 = extractvalue { i64, i1 } %t1866, 1
  br i1 %t1867, label %sub_overflow547, label %sub_ok546
sub_overflow547:
  call void @panic(ptr @.str.143)
  unreachable
sub_ok546:
  %t1868 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t1860, i64 %t1861, i64 %t1864)
  %t1869 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t1868)
  store ptr %t1869, ptr %t1857
  br label %if.end545
if.end545:
  %t1870 = call %struct.List__Str @tml_N3std11collections4list9List__Str3newE(i64 0)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t1871)
  store %struct.List__Str %t1870, ptr %t1871
  %t1872 = load ptr, ptr %t1857
  %t1873 = call i64 @strlen(ptr %t1872)
  %t1875 = sext i32 0 to i64
  %t1874 = icmp sgt i64 %t1873, %t1875
  br i1 %t1874, label %if.then548, label %if.end550
if.then548:
  %t1876 = load ptr, ptr %t1857
  %t1877 = call %struct.List__Str @tml_N4core3str5split5splitE_SS(ptr %t1876, ptr @.str.134)
  store %struct.List__Str %t1877, ptr %t1871
  br label %if.end550
if.end550:
  %t1879 = alloca %struct.Maybe__FnHeader, align 8
  %t1880 = getelementptr inbounds %struct.Maybe__FnHeader, ptr %t1879, i32 0, i32 0
  store i32 0, ptr %t1880
  %t1881 = load ptr, ptr %t1776
  %t1882 = insertvalue %struct.FnHeader undef, ptr %t1881, 0
  %t1883 = load ptr, ptr %t1733
  %t1884 = insertvalue %struct.FnHeader %t1882, ptr %t1883, 1
  %t1885 = load %struct.List__IrParam, ptr %t1855
  %t1886 = insertvalue %struct.FnHeader %t1884, %struct.List__IrParam %t1885, 2
  %t1887 = load %struct.List__Str, ptr %t1871
  %t1888 = insertvalue %struct.FnHeader %t1886, %struct.List__Str %t1887, 3
  %t1889 = getelementptr inbounds %struct.Maybe__FnHeader, ptr %t1879, i32 0, i32 1
  %t1890 = bitcast ptr %t1889 to ptr
  store %struct.FnHeader %t1888, ptr %t1890
  %t1878 = load %struct.Maybe__FnHeader, ptr %t1879
  %t1891 = load ptr, ptr %t1852
  call void @tml_str_free(ptr %t1891)
  %t1892 = load ptr, ptr %t1843
  call void @tml_str_free(ptr %t1892)
  %t1893 = load %struct.List__U8, ptr %t1787
  call void @tml_N3std11collections4list8List__U84dropE(ptr %t1787)
  %t1894 = load ptr, ptr %t1784
  call void @tml_str_free(ptr %t1894)
  %t1895 = load ptr, ptr %t1753
  call void @tml_str_free(ptr %t1895)
  %t1896 = load %struct.List__Str, ptr %t1732
  call void @tml_N3std11collections4list9List__Str4dropE(ptr %t1732)
  %t1897 = load ptr, ptr %t1728
  call void @tml_str_free(ptr %t1897)
  ret %struct.Maybe__FnHeader %t1878
}

; core::fmt::helpers::u64_to_str
define ptr @tml_N4core3fmt7helpers10u64_to_strE_m(i64 %value) #0 {
entry:
  %t1898 = alloca i64
  store i64 %value, ptr %t1898
  %t1905 = alloca i64
  %t1916 = alloca i64
  %t1920 = alloca i64
  %t1932 = alloca i64
  %t1934 = alloca i64
  %t1940 = alloca i64
  %t1947 = alloca i64
  %t1959 = alloca i8
  %t1967 = alloca i8
  %t1997 = alloca i64
  %t2009 = alloca i8
  %t2017 = alloca i8
  %t2045 = alloca i8
  %t1899 = alloca ptr
  store ptr @.str.97, ptr %t1899
  %t1900 = load i64, ptr %t1898
  %t1902 = sext i32 0 to i64
  %t1901 = icmp eq i64 %t1900, %t1902
  br i1 %t1901, label %if.then551, label %if.end553
if.then551:
  ret ptr @.str.74
if.end553:
  %t1903 = load i64, ptr %t1898
  %t1904 = call i64 @tml_N4core3fmt7helpers16count_digits_u64E_m(i64 %t1903)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1905)
  store i64 %t1904, ptr %t1905
  %t1906 = load i64, ptr %t1905
  %t1908 = sext i32 1 to i64
  %t1909 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1906, i64 %t1908)
  %t1907 = extractvalue { i64, i1 } %t1909, 0
  %t1910 = extractvalue { i64, i1 } %t1909, 1
  br i1 %t1910, label %add_overflow555, label %add_ok554
add_overflow555:
  call void @panic(ptr @.str.144)
  unreachable
add_ok554:
  %t1911 = call ptr @mem_alloc(i64 %t1907)
  %t1912 = alloca ptr
  store ptr %t1911, ptr %t1912
  %t1913 = load ptr, ptr %t1912
  %t1915 = ptrtoint ptr %t1913 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1916)
  store i64 %t1915, ptr %t1916
  %t1917 = load ptr, ptr %t1899
  %t1919 = ptrtoint ptr %t1917 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1920)
  store i64 %t1919, ptr %t1920
  %t1921 = load i64, ptr %t1916
  %t1922 = load i64, ptr %t1905
  %t1924 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1921, i64 %t1922)
  %t1923 = extractvalue { i64, i1 } %t1924, 0
  %t1925 = extractvalue { i64, i1 } %t1924, 1
  br i1 %t1925, label %add_overflow557, label %add_ok556
add_overflow557:
  call void @panic(ptr @.str.145)
  unreachable
add_ok556:
  %t1926 = inttoptr i64 %t1923 to ptr
  %t1927 = trunc i32 0 to i8
  store i8 %t1927, ptr %t1926
  %t1928 = load i64, ptr %t1905
  %t1930 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1928, i64 1)
  %t1929 = extractvalue { i64, i1 } %t1930, 0
  %t1931 = extractvalue { i64, i1 } %t1930, 1
  br i1 %t1931, label %sub_overflow559, label %sub_ok558
sub_overflow559:
  call void @panic(ptr @.str.146)
  unreachable
sub_ok558:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1932)
  store i64 %t1929, ptr %t1932
  %t1933 = load i64, ptr %t1898
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1934)
  store i64 %t1933, ptr %t1934
  br label %loop.preheader560
loop.preheader560:
  br label %loop.header561
loop.header561:
  %t1935 = load i64, ptr %t1934
  %t1937 = sext i32 100 to i64
  %t1936 = icmp uge i64 %t1935, %t1937
  br i1 %t1936, label %loop.body562, label %loop.exit564
loop.body562:
  %t1938 = load i64, ptr %t1934
  %t1939 = urem i64 %t1938, 100
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1940)
  store i64 %t1939, ptr %t1940
  %t1941 = load i64, ptr %t1934
  %t1942 = udiv i64 %t1941, 100
  store i64 %t1942, ptr %t1934
  %t1943 = load i64, ptr %t1940
  %t1945 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t1943, i64 2)
  %t1944 = extractvalue { i64, i1 } %t1945, 0
  %t1946 = extractvalue { i64, i1 } %t1945, 1
  br i1 %t1946, label %mul_overflow566, label %mul_ok565
mul_overflow566:
  call void @panic(ptr @.str.147)
  unreachable
mul_ok565:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1947)
  store i64 %t1944, ptr %t1947
  %t1948 = load i64, ptr %t1920
  %t1949 = load i64, ptr %t1947
  %t1951 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1948, i64 %t1949)
  %t1950 = extractvalue { i64, i1 } %t1951, 0
  %t1952 = extractvalue { i64, i1 } %t1951, 1
  br i1 %t1952, label %add_overflow568, label %add_ok567
add_overflow568:
  call void @panic(ptr @.str.148)
  unreachable
add_ok567:
  %t1954 = sext i32 1 to i64
  %t1955 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1950, i64 %t1954)
  %t1953 = extractvalue { i64, i1 } %t1955, 0
  %t1956 = extractvalue { i64, i1 } %t1955, 1
  br i1 %t1956, label %add_overflow570, label %add_ok569
add_overflow570:
  call void @panic(ptr @.str.148)
  unreachable
add_ok569:
  %t1957 = inttoptr i64 %t1953 to ptr
  %t1958 = load i8, ptr %t1957
  call void @llvm.lifetime.start.p0(i64 1, ptr %t1959)
  store i8 %t1958, ptr %t1959
  %t1960 = load i64, ptr %t1920
  %t1961 = load i64, ptr %t1947
  %t1963 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1960, i64 %t1961)
  %t1962 = extractvalue { i64, i1 } %t1963, 0
  %t1964 = extractvalue { i64, i1 } %t1963, 1
  br i1 %t1964, label %add_overflow572, label %add_ok571
add_overflow572:
  call void @panic(ptr @.str.149)
  unreachable
add_ok571:
  %t1965 = inttoptr i64 %t1962 to ptr
  %t1966 = load i8, ptr %t1965
  call void @llvm.lifetime.start.p0(i64 1, ptr %t1967)
  store i8 %t1966, ptr %t1967
  %t1968 = load i64, ptr %t1916
  %t1969 = load i64, ptr %t1932
  %t1971 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1968, i64 %t1969)
  %t1970 = extractvalue { i64, i1 } %t1971, 0
  %t1972 = extractvalue { i64, i1 } %t1971, 1
  br i1 %t1972, label %add_overflow574, label %add_ok573
add_overflow574:
  call void @panic(ptr @.str.150)
  unreachable
add_ok573:
  %t1973 = inttoptr i64 %t1970 to ptr
  %t1974 = load i8, ptr %t1959
  store i8 %t1974, ptr %t1973
  %t1975 = load i64, ptr %t1916
  %t1976 = load i64, ptr %t1932
  %t1978 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1975, i64 %t1976)
  %t1977 = extractvalue { i64, i1 } %t1978, 0
  %t1979 = extractvalue { i64, i1 } %t1978, 1
  br i1 %t1979, label %add_overflow576, label %add_ok575
add_overflow576:
  call void @panic(ptr @.str.151)
  unreachable
add_ok575:
  %t1981 = sext i32 1 to i64
  %t1982 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1977, i64 %t1981)
  %t1980 = extractvalue { i64, i1 } %t1982, 0
  %t1983 = extractvalue { i64, i1 } %t1982, 1
  br i1 %t1983, label %sub_overflow578, label %sub_ok577
sub_overflow578:
  call void @panic(ptr @.str.152)
  unreachable
sub_ok577:
  %t1984 = inttoptr i64 %t1980 to ptr
  %t1985 = load i8, ptr %t1967
  store i8 %t1985, ptr %t1984
  %t1986 = load i64, ptr %t1932
  %t1988 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t1986, i64 2)
  %t1987 = extractvalue { i64, i1 } %t1988, 0
  %t1989 = extractvalue { i64, i1 } %t1988, 1
  br i1 %t1989, label %sub_overflow580, label %sub_ok579
sub_overflow580:
  call void @panic(ptr @.str.153)
  unreachable
sub_ok579:
  store i64 %t1987, ptr %t1932
  call void @llvm.lifetime.end.p0(i64 1, ptr %t1967)
  call void @llvm.lifetime.end.p0(i64 1, ptr %t1959)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1947)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1940)
  br label %loop.latch563
loop.latch563:
  br label %loop.header561, !llvm.loop !1021
loop.exit564:
  %t1990 = load i64, ptr %t1934
  %t1992 = sext i32 10 to i64
  %t1991 = icmp uge i64 %t1990, %t1992
  br i1 %t1991, label %if.then581, label %if.else582
if.then581:
  %t1993 = load i64, ptr %t1934
  %t1995 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t1993, i64 2)
  %t1994 = extractvalue { i64, i1 } %t1995, 0
  %t1996 = extractvalue { i64, i1 } %t1995, 1
  br i1 %t1996, label %mul_overflow585, label %mul_ok584
mul_overflow585:
  call void @panic(ptr @.str.154)
  unreachable
mul_ok584:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t1997)
  store i64 %t1994, ptr %t1997
  %t1998 = load i64, ptr %t1920
  %t1999 = load i64, ptr %t1997
  %t2001 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t1998, i64 %t1999)
  %t2000 = extractvalue { i64, i1 } %t2001, 0
  %t2002 = extractvalue { i64, i1 } %t2001, 1
  br i1 %t2002, label %add_overflow587, label %add_ok586
add_overflow587:
  call void @panic(ptr @.str.155)
  unreachable
add_ok586:
  %t2004 = sext i32 1 to i64
  %t2005 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2000, i64 %t2004)
  %t2003 = extractvalue { i64, i1 } %t2005, 0
  %t2006 = extractvalue { i64, i1 } %t2005, 1
  br i1 %t2006, label %add_overflow589, label %add_ok588
add_overflow589:
  call void @panic(ptr @.str.155)
  unreachable
add_ok588:
  %t2007 = inttoptr i64 %t2003 to ptr
  %t2008 = load i8, ptr %t2007
  call void @llvm.lifetime.start.p0(i64 1, ptr %t2009)
  store i8 %t2008, ptr %t2009
  %t2010 = load i64, ptr %t1920
  %t2011 = load i64, ptr %t1997
  %t2013 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2010, i64 %t2011)
  %t2012 = extractvalue { i64, i1 } %t2013, 0
  %t2014 = extractvalue { i64, i1 } %t2013, 1
  br i1 %t2014, label %add_overflow591, label %add_ok590
add_overflow591:
  call void @panic(ptr @.str.156)
  unreachable
add_ok590:
  %t2015 = inttoptr i64 %t2012 to ptr
  %t2016 = load i8, ptr %t2015
  call void @llvm.lifetime.start.p0(i64 1, ptr %t2017)
  store i8 %t2016, ptr %t2017
  %t2018 = load i64, ptr %t1916
  %t2019 = load i64, ptr %t1932
  %t2021 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2018, i64 %t2019)
  %t2020 = extractvalue { i64, i1 } %t2021, 0
  %t2022 = extractvalue { i64, i1 } %t2021, 1
  br i1 %t2022, label %add_overflow593, label %add_ok592
add_overflow593:
  call void @panic(ptr @.str.13)
  unreachable
add_ok592:
  %t2023 = inttoptr i64 %t2020 to ptr
  %t2024 = load i8, ptr %t2009
  store i8 %t2024, ptr %t2023
  %t2025 = load i64, ptr %t1916
  %t2026 = load i64, ptr %t1932
  %t2028 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2025, i64 %t2026)
  %t2027 = extractvalue { i64, i1 } %t2028, 0
  %t2029 = extractvalue { i64, i1 } %t2028, 1
  br i1 %t2029, label %add_overflow595, label %add_ok594
add_overflow595:
  call void @panic(ptr @.str.157)
  unreachable
add_ok594:
  %t2031 = sext i32 1 to i64
  %t2032 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t2027, i64 %t2031)
  %t2030 = extractvalue { i64, i1 } %t2032, 0
  %t2033 = extractvalue { i64, i1 } %t2032, 1
  br i1 %t2033, label %sub_overflow597, label %sub_ok596
sub_overflow597:
  call void @panic(ptr @.str.158)
  unreachable
sub_ok596:
  %t2034 = inttoptr i64 %t2030 to ptr
  %t2035 = load i8, ptr %t2017
  store i8 %t2035, ptr %t2034
  call void @llvm.lifetime.end.p0(i64 1, ptr %t2017)
  call void @llvm.lifetime.end.p0(i64 1, ptr %t2009)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t1997)
  br label %if.end583
if.else582:
  %t2036 = load i64, ptr %t1934
  %t2038 = sext i32 0 to i64
  %t2037 = icmp ugt i64 %t2036, %t2038
  br i1 %t2037, label %if.then598, label %if.end600
if.then598:
  %t2039 = load i64, ptr %t1934
  %t2040 = trunc i64 %t2039 to i8
  %t2041 = trunc i32 48 to i8
  %t2043 = call { i8, i1 } @llvm.uadd.with.overflow.i8(i8 %t2040, i8 %t2041)
  %t2042 = extractvalue { i8, i1 } %t2043, 0
  %t2044 = extractvalue { i8, i1 } %t2043, 1
  br i1 %t2044, label %add_overflow602, label %add_ok601
add_overflow602:
  call void @panic(ptr @.str.159)
  unreachable
add_ok601:
  call void @llvm.lifetime.start.p0(i64 1, ptr %t2045)
  store i8 %t2042, ptr %t2045
  %t2046 = load i64, ptr %t1916
  %t2047 = load i64, ptr %t1932
  %t2049 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2046, i64 %t2047)
  %t2048 = extractvalue { i64, i1 } %t2049, 0
  %t2050 = extractvalue { i64, i1 } %t2049, 1
  br i1 %t2050, label %add_overflow604, label %add_ok603
add_overflow604:
  call void @panic(ptr @.str.160)
  unreachable
add_ok603:
  %t2051 = inttoptr i64 %t2048 to ptr
  %t2052 = load i8, ptr %t2045
  store i8 %t2052, ptr %t2051
  call void @llvm.lifetime.end.p0(i64 1, ptr %t2045)
  br label %if.end600
if.end600:
  br label %if.end583
if.end583:
  %t2053 = load ptr, ptr %t1912
  ret ptr %t2053
}

; ir_diff::parser::parse_global
define internal %struct.Maybe__IrGlobal @tml_N7ir_diff6parser12parse_globalE_S(ptr %line) #0 {
entry:
  %t2054 = alloca ptr
  store ptr %line, ptr %t2054
  %t2063 = alloca %struct.Maybe__tuple_Str_Str
  %t2085 = alloca %struct.List__Str
  %t2086 = alloca i64
  %t2125 = alloca i64
  %t2055 = load ptr, ptr %t2054
  %t2056 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t2055, ptr @.str.48)
  %t2057 = xor i1 %t2056, 1
  br i1 %t2057, label %if.then605, label %if.end607
if.then605:
  %t2059 = alloca %struct.Maybe__IrGlobal, align 8
  %t2060 = getelementptr inbounds %struct.Maybe__IrGlobal, ptr %t2059, i32 0, i32 0
  store i32 1, ptr %t2060
  %t2058 = load %struct.Maybe__IrGlobal, ptr %t2059
  ret %struct.Maybe__IrGlobal %t2058
if.end607:
  %t2061 = load ptr, ptr %t2054
  %t2062 = call %struct.Maybe__tuple_Str_Str @tml_N4core3str5split10split_onceE_SS(ptr %t2061, ptr @.str.46)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2063)
  store %struct.Maybe__tuple_Str_Str %t2062, ptr %t2063
  %t2064 = load %struct.Maybe__tuple_Str_Str, ptr %t2063
  %t2065 = extractvalue %struct.Maybe__tuple_Str_Str %t2064, 0
  %t2066 = icmp eq i32 %t2065, 1
  br i1 %t2066, label %if.then608, label %if.end610
if.then608:
  %t2068 = alloca %struct.Maybe__IrGlobal, align 8
  %t2069 = getelementptr inbounds %struct.Maybe__IrGlobal, ptr %t2068, i32 0, i32 0
  store i32 1, ptr %t2069
  %t2067 = load %struct.Maybe__IrGlobal, ptr %t2068
  ret %struct.Maybe__IrGlobal %t2067
if.end610:
  %t2070 = load %struct.Maybe__tuple_Str_Str, ptr %t2063
  %t2071 = extractvalue %struct.Maybe__tuple_Str_Str %t2070, 0
  %t2072 = alloca %struct.Maybe__tuple_Str_Str
  store %struct.Maybe__tuple_Str_Str %t2070, ptr %t2072
  %t2073 = getelementptr inbounds %struct.Maybe__tuple_Str_Str, ptr %t2072, i32 0, i32 1
  %t2074 = load { ptr, ptr }, ptr %t2073
  %t2075 = alloca { ptr, ptr }
  store { ptr, ptr } %t2074, ptr %t2075
  %t2076 = getelementptr inbounds { ptr, ptr }, ptr %t2075, i32 0, i32 0
  %t2077 = load ptr, ptr %t2076
  %t2078 = alloca ptr
  store ptr %t2077, ptr %t2078
  %t2079 = getelementptr inbounds { ptr, ptr }, ptr %t2075, i32 0, i32 1
  %t2080 = load ptr, ptr %t2079
  %t2081 = alloca ptr
  store ptr %t2080, ptr %t2081
  %t2082 = load ptr, ptr %t2081
  %t2083 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t2082)
  %t2084 = call %struct.List__Str @tml_N4core3str5split5splitE_SS(ptr %t2083, ptr @.str.134)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2085)
  store %struct.List__Str %t2084, ptr %t2085
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2086)
  store i64 0, ptr %t2086
  br label %loop.preheader611
loop.preheader611:
  br label %loop.header612
loop.header612:
  %t2087 = load i64, ptr %t2086
  %t2088 = load %struct.List__Str, ptr %t2085
  %t2089 = call i64 @tml_N3std11collections4list9List__Str3lenE(ptr %t2085)
  %t2090 = icmp slt i64 %t2087, %t2089
  br i1 %t2090, label %loop.body613, label %loop.exit615
loop.body613:
  %t2091 = load %struct.List__Str, ptr %t2085
  %t2092 = load i64, ptr %t2086
  %t2093 = call ptr @tml_N3std11collections4list9List__Str3getE(ptr %t2085, i64 %t2092)
  %t2094 = alloca ptr
  store ptr %t2093, ptr %t2094
  %t2095 = load ptr, ptr %t2094
  %t2096 = call i1 @tml_N7ir_diff6parser18is_linkage_keywordE_S(ptr %t2095)
  br i1 %t2096, label %if.then616, label %if.end618
if.then616:
  %t2097 = load i64, ptr %t2086
  %t2099 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2097, i64 1)
  %t2098 = extractvalue { i64, i1 } %t2099, 0
  %t2100 = extractvalue { i64, i1 } %t2099, 1
  br i1 %t2100, label %add_overflow620, label %add_ok619
add_overflow620:
  call void @panic(ptr @.str.161)
  unreachable
add_ok619:
  store i64 %t2098, ptr %t2086
  br label %loop.latch614
if.end618:
  br label %loop.exit615
loop.latch614:
  br label %loop.header612, !llvm.loop !1022
loop.exit615:
  %t2101 = load i64, ptr %t2086
  %t2102 = load %struct.List__Str, ptr %t2085
  %t2103 = call i64 @tml_N3std11collections4list9List__Str3lenE(ptr %t2085)
  %t2104 = icmp sge i64 %t2101, %t2103
  br i1 %t2104, label %if.then621, label %if.end623
if.then621:
  %t2106 = alloca %struct.Maybe__IrGlobal, align 8
  %t2107 = getelementptr inbounds %struct.Maybe__IrGlobal, ptr %t2106, i32 0, i32 0
  store i32 0, ptr %t2107
  %t2108 = load ptr, ptr %t2078
  %t2109 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t2108)
  %t2110 = insertvalue %struct.IrGlobal undef, ptr %t2109, 0
  %t2111 = insertvalue %struct.IrGlobal %t2110, ptr @.str.1, 1
  %t2112 = insertvalue %struct.IrGlobal %t2111, ptr @.str.1, 2
  %t2113 = getelementptr inbounds %struct.Maybe__IrGlobal, ptr %t2106, i32 0, i32 1
  %t2114 = bitcast ptr %t2113 to ptr
  store %struct.IrGlobal %t2112, ptr %t2114
  %t2105 = load %struct.Maybe__IrGlobal, ptr %t2106
  %t2115 = load %struct.List__Str, ptr %t2085
  call void @tml_N3std11collections4list9List__Str4dropE(ptr %t2085)
  ret %struct.Maybe__IrGlobal %t2105
if.end623:
  %t2116 = load %struct.List__Str, ptr %t2085
  %t2117 = load i64, ptr %t2086
  %t2118 = call ptr @tml_N3std11collections4list9List__Str3getE(ptr %t2085, i64 %t2117)
  %t2119 = alloca ptr
  store ptr %t2118, ptr %t2119
  %t2120 = alloca ptr
  store ptr @.str.1, ptr %t2120
  %t2121 = load i64, ptr %t2086
  %t2123 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2121, i64 1)
  %t2122 = extractvalue { i64, i1 } %t2123, 0
  %t2124 = extractvalue { i64, i1 } %t2123, 1
  br i1 %t2124, label %add_overflow625, label %add_ok624
add_overflow625:
  call void @panic(ptr @.str.162)
  unreachable
add_ok624:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2125)
  store i64 %t2122, ptr %t2125
  br label %loop.preheader626
loop.preheader626:
  br label %loop.header627
loop.header627:
  %t2126 = load i64, ptr %t2125
  %t2127 = load %struct.List__Str, ptr %t2085
  %t2128 = call i64 @tml_N3std11collections4list9List__Str3lenE(ptr %t2085)
  %t2129 = icmp slt i64 %t2126, %t2128
  br i1 %t2129, label %loop.body628, label %loop.exit630
loop.body628:
  %t2130 = load ptr, ptr %t2120
  %t2131 = call i64 @strlen(ptr %t2130)
  %t2133 = sext i32 0 to i64
  %t2132 = icmp sgt i64 %t2131, %t2133
  br i1 %t2132, label %if.then631, label %if.end633
if.then631:
  %t2134 = load ptr, ptr %t2120
  %t2135 = call ptr @tml_N4core3str7convert6concatE_SS(ptr %t2134, ptr @.str.134)
  store ptr %t2135, ptr %t2120
  br label %if.end633
if.end633:
  %t2136 = load ptr, ptr %t2120
  %t2137 = load %struct.List__Str, ptr %t2085
  %t2138 = load i64, ptr %t2125
  %t2139 = call ptr @tml_N3std11collections4list9List__Str3getE(ptr %t2085, i64 %t2138)
  %t2140 = call ptr @tml_N4core3str7convert6concatE_SS(ptr %t2136, ptr %t2139)
  store ptr %t2140, ptr %t2120
  %t2141 = load i64, ptr %t2125
  %t2143 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2141, i64 1)
  %t2142 = extractvalue { i64, i1 } %t2143, 0
  %t2144 = extractvalue { i64, i1 } %t2143, 1
  br i1 %t2144, label %add_overflow635, label %add_ok634
add_overflow635:
  call void @panic(ptr @.str.68)
  unreachable
add_ok634:
  store i64 %t2142, ptr %t2125
  br label %loop.latch629
loop.latch629:
  br label %loop.header627, !llvm.loop !1023
loop.exit630:
  %t2146 = alloca %struct.Maybe__IrGlobal, align 8
  %t2147 = getelementptr inbounds %struct.Maybe__IrGlobal, ptr %t2146, i32 0, i32 0
  store i32 0, ptr %t2147
  %t2148 = load ptr, ptr %t2078
  %t2149 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t2148)
  %t2150 = insertvalue %struct.IrGlobal undef, ptr %t2149, 0
  %t2151 = load ptr, ptr %t2119
  %t2152 = insertvalue %struct.IrGlobal %t2150, ptr %t2151, 1
  %t2153 = load ptr, ptr %t2120
  %t2154 = insertvalue %struct.IrGlobal %t2152, ptr %t2153, 2
  %t2155 = getelementptr inbounds %struct.Maybe__IrGlobal, ptr %t2146, i32 0, i32 1
  %t2156 = bitcast ptr %t2155 to ptr
  store %struct.IrGlobal %t2154, ptr %t2156
  %t2145 = load %struct.Maybe__IrGlobal, ptr %t2146
  %t2157 = load %struct.List__Str, ptr %t2085
  call void @tml_N3std11collections4list9List__Str4dropE(ptr %t2085)
  ret %struct.Maybe__IrGlobal %t2145
}

; core::fmt::helpers::u32_to_str
define ptr @tml_N4core3fmt7helpers10u32_to_strE_j(i32 %value) #0 {
entry:
  %t2158 = alloca i32
  store i32 %value, ptr %t2158
  %t2159 = load i32, ptr %t2158
  %t2160 = zext i32 %t2159 to i64
  %t2161 = call ptr @tml_N4core3fmt7helpers10u64_to_strE_m(i64 %t2160)
  ret ptr %t2161
}

; ir_diff::differ::list_contains_i64
define internal i1 @tml_N7ir_diff6differ17list_contains_i64E_R4ListIlEl(ptr %lst, i64 %v) #0 {
entry:
  %t2162 = alloca ptr
  store ptr %lst, ptr %t2162
  %t2163 = alloca i64
  store i64 %v, ptr %t2163
  %t2164 = alloca i64
  %t2167 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2164)
  store i64 0, ptr %t2164
  %t2165 = load ptr, ptr %t2162
  %t2166 = call i64 @tml_N3std11collections4list9List__I643lenE(ptr %t2165)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2167)
  store i64 %t2166, ptr %t2167
  br label %loop.preheader636
loop.preheader636:
  br label %loop.header637
loop.header637:
  %t2168 = load i64, ptr %t2164
  %t2169 = load i64, ptr %t2167
  %t2170 = icmp slt i64 %t2168, %t2169
  br i1 %t2170, label %loop.body638, label %loop.exit640
loop.body638:
  %t2171 = load ptr, ptr %t2162
  %t2172 = load i64, ptr %t2164
  %t2173 = call i64 @tml_N3std11collections4list9List__I643getE(ptr %t2171, i64 %t2172)
  %t2174 = load i64, ptr %t2163
  %t2175 = icmp eq i64 %t2173, %t2174
  br i1 %t2175, label %if.then641, label %if.end643
if.then641:
  ret i1 1
if.end643:
  %t2176 = load i64, ptr %t2164
  %t2178 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2176, i64 1)
  %t2177 = extractvalue { i64, i1 } %t2178, 0
  %t2179 = extractvalue { i64, i1 } %t2178, 1
  br i1 %t2179, label %add_overflow645, label %add_ok644
add_overflow645:
  call void @panic(ptr @.str.163)
  unreachable
add_ok644:
  store i64 %t2177, ptr %t2164
  br label %loop.latch639
loop.latch639:
  br label %loop.header637, !llvm.loop !1024
loop.exit640:
  ret i1 0
}

; ir_diff::parser::is_label_line
define internal i1 @tml_N7ir_diff6parser13is_label_lineE_S(ptr %line) #0 {
entry:
  %t2180 = alloca ptr
  store ptr %line, ptr %t2180
  %t2183 = alloca i64
  %t2181 = load ptr, ptr %t2180
  %t2182 = call i64 @strlen(ptr %t2181)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2183)
  store i64 %t2182, ptr %t2183
  %t2184 = load i64, ptr %t2183
  %t2186 = sext i32 2 to i64
  %t2185 = icmp slt i64 %t2184, %t2186
  br i1 %t2185, label %if.then646, label %if.end648
if.then646:
  ret i1 0
if.end648:
  %t2187 = load ptr, ptr %t2180
  %t2188 = call i1 @tml_N4core3str6search9ends_withE_SS(ptr %t2187, ptr @.str.164)
  %t2189 = xor i1 %t2188, 1
  br i1 %t2189, label %if.then649, label %if.end651
if.then649:
  ret i1 0
if.end651:
  %t2190 = load ptr, ptr %t2180
  %t2191 = sext i32 0 to i64
  %t2192 = load i64, ptr %t2183
  %t2194 = sext i32 1 to i64
  %t2195 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t2192, i64 %t2194)
  %t2193 = extractvalue { i64, i1 } %t2195, 0
  %t2196 = extractvalue { i64, i1 } %t2195, 1
  br i1 %t2196, label %sub_overflow653, label %sub_ok652
sub_overflow653:
  call void @panic(ptr @.str.165)
  unreachable
sub_ok652:
  %t2197 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t2190, i64 %t2191, i64 %t2193)
  %t2198 = alloca ptr
  store ptr %t2197, ptr %t2198
  %t2199 = load ptr, ptr %t2198
  %t2200 = call i1 @tml_N4core3str6search8containsE_SS(ptr %t2199, ptr @.str.134)
  br i1 %t2200, label %if.then654, label %if.end656
if.then654:
  %t2201 = load ptr, ptr %t2198
  call void @tml_str_free(ptr %t2201)
  ret i1 0
if.end656:
  %t2202 = load ptr, ptr %t2198
  %t2203 = call i1 @tml_N4core3str6search8containsE_SS(ptr %t2202, ptr @.str.166)
  br i1 %t2203, label %if.then657, label %if.end659
if.then657:
  %t2204 = load ptr, ptr %t2198
  call void @tml_str_free(ptr %t2204)
  ret i1 0
if.end659:
  %t2205 = load ptr, ptr %t2198
  call void @tml_str_free(ptr %t2205)
  ret i1 1
}

; core::fmt::helpers::i8_to_str
define ptr @tml_N4core3fmt7helpers9i8_to_strE_a(i8 %value) #0 {
entry:
  %t2206 = alloca i8
  store i8 %value, ptr %t2206
  %t2207 = load i8, ptr %t2206
  %t2208 = sext i8 %t2207 to i64
  %t2209 = call ptr @tml_N4core3fmt7helpers10i64_to_strE_l(i64 %t2208)
  ret ptr %t2209
}

; core::fmt::helpers::u64_to_binary_str
define ptr @tml_N4core3fmt7helpers17u64_to_binary_strE_m(i64 %n) #0 {
entry:
  %t2210 = alloca i64
  store i64 %n, ptr %t2210
  %t2214 = alloca i64
  %t2216 = alloca i64
  %t2236 = alloca i64
  %t2248 = alloca i64
  %t2250 = alloca i64
  %t2262 = alloca i8
  %t2211 = load i64, ptr %t2210
  %t2213 = sext i32 0 to i64
  %t2212 = icmp eq i64 %t2211, %t2213
  br i1 %t2212, label %if.then660, label %if.end662
if.then660:
  ret ptr @.str.74
if.end662:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2214)
  store i64 0, ptr %t2214
  %t2215 = load i64, ptr %t2210
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2216)
  store i64 %t2215, ptr %t2216
  br label %loop.preheader663
loop.preheader663:
  br label %loop.header664
loop.header664:
  %t2217 = load i64, ptr %t2216
  %t2219 = sext i32 0 to i64
  %t2218 = icmp ne i64 %t2217, %t2219
  br i1 %t2218, label %loop.body665, label %loop.exit667
loop.body665:
  %t2220 = load i64, ptr %t2214
  %t2222 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2220, i64 1)
  %t2221 = extractvalue { i64, i1 } %t2222, 0
  %t2223 = extractvalue { i64, i1 } %t2222, 1
  br i1 %t2223, label %add_overflow669, label %add_ok668
add_overflow669:
  call void @panic(ptr @.str.167)
  unreachable
add_ok668:
  store i64 %t2221, ptr %t2214
  %t2224 = load i64, ptr %t2216
  %t2225 = lshr i64 %t2224, 1
  store i64 %t2225, ptr %t2216
  br label %loop.latch666
loop.latch666:
  br label %loop.header664, !llvm.loop !1025
loop.exit667:
  %t2226 = load i64, ptr %t2214
  %t2228 = sext i32 1 to i64
  %t2229 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2226, i64 %t2228)
  %t2227 = extractvalue { i64, i1 } %t2229, 0
  %t2230 = extractvalue { i64, i1 } %t2229, 1
  br i1 %t2230, label %add_overflow671, label %add_ok670
add_overflow671:
  call void @panic(ptr @.str.168)
  unreachable
add_ok670:
  %t2231 = call ptr @mem_alloc(i64 %t2227)
  %t2232 = alloca ptr
  store ptr %t2231, ptr %t2232
  %t2233 = load ptr, ptr %t2232
  %t2235 = ptrtoint ptr %t2233 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2236)
  store i64 %t2235, ptr %t2236
  %t2237 = load i64, ptr %t2236
  %t2238 = load i64, ptr %t2214
  %t2240 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2237, i64 %t2238)
  %t2239 = extractvalue { i64, i1 } %t2240, 0
  %t2241 = extractvalue { i64, i1 } %t2240, 1
  br i1 %t2241, label %add_overflow673, label %add_ok672
add_overflow673:
  call void @panic(ptr @.str.169)
  unreachable
add_ok672:
  %t2242 = inttoptr i64 %t2239 to ptr
  %t2243 = trunc i32 0 to i8
  store i8 %t2243, ptr %t2242
  %t2244 = load i64, ptr %t2214
  %t2246 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t2244, i64 1)
  %t2245 = extractvalue { i64, i1 } %t2246, 0
  %t2247 = extractvalue { i64, i1 } %t2246, 1
  br i1 %t2247, label %sub_overflow675, label %sub_ok674
sub_overflow675:
  call void @panic(ptr @.str.170)
  unreachable
sub_ok674:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2248)
  store i64 %t2245, ptr %t2248
  %t2249 = load i64, ptr %t2210
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2250)
  store i64 %t2249, ptr %t2250
  br label %loop.preheader676
loop.preheader676:
  br label %loop.header677
loop.header677:
  %t2251 = load i64, ptr %t2250
  %t2253 = sext i32 0 to i64
  %t2252 = icmp ne i64 %t2251, %t2253
  br i1 %t2252, label %loop.body678, label %loop.exit680
loop.body678:
  %t2254 = load i64, ptr %t2250
  %t2256 = sext i32 1 to i64
  %t2255 = and i64 %t2254, %t2256
  %t2257 = trunc i64 %t2255 to i8
  %t2258 = trunc i32 48 to i8
  %t2260 = call { i8, i1 } @llvm.uadd.with.overflow.i8(i8 %t2257, i8 %t2258)
  %t2259 = extractvalue { i8, i1 } %t2260, 0
  %t2261 = extractvalue { i8, i1 } %t2260, 1
  br i1 %t2261, label %add_overflow682, label %add_ok681
add_overflow682:
  call void @panic(ptr @.str.171)
  unreachable
add_ok681:
  call void @llvm.lifetime.start.p0(i64 1, ptr %t2262)
  store i8 %t2259, ptr %t2262
  %t2263 = load i64, ptr %t2236
  %t2264 = load i64, ptr %t2248
  %t2266 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2263, i64 %t2264)
  %t2265 = extractvalue { i64, i1 } %t2266, 0
  %t2267 = extractvalue { i64, i1 } %t2266, 1
  br i1 %t2267, label %add_overflow684, label %add_ok683
add_overflow684:
  call void @panic(ptr @.str.136)
  unreachable
add_ok683:
  %t2268 = inttoptr i64 %t2265 to ptr
  %t2269 = load i8, ptr %t2262
  store i8 %t2269, ptr %t2268
  %t2270 = load i64, ptr %t2250
  %t2271 = lshr i64 %t2270, 1
  store i64 %t2271, ptr %t2250
  %t2272 = load i64, ptr %t2248
  %t2274 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t2272, i64 1)
  %t2273 = extractvalue { i64, i1 } %t2274, 0
  %t2275 = extractvalue { i64, i1 } %t2274, 1
  br i1 %t2275, label %sub_overflow686, label %sub_ok685
sub_overflow686:
  call void @panic(ptr @.str.172)
  unreachable
sub_ok685:
  store i64 %t2273, ptr %t2248
  call void @llvm.lifetime.end.p0(i64 1, ptr %t2262)
  br label %loop.latch679
loop.latch679:
  br label %loop.header677, !llvm.loop !1026
loop.exit680:
  %t2276 = load ptr, ptr %t2232
  ret ptr %t2276
}

; core::str::basic::substring
define ptr @tml_N4core3str5basic9substringE_Sll(ptr %s, i64 %start, i64 %p_end) #0 {
entry:
  %t2277 = alloca ptr
  store ptr %s, ptr %t2277
  %t2278 = alloca i64
  store i64 %start, ptr %t2278
  %t2279 = alloca i64
  store i64 %p_end, ptr %t2279
  %t2282 = alloca i64
  %t2284 = alloca i64
  %t2286 = alloca i64
  %t2307 = alloca i64
  %t2280 = load ptr, ptr %t2277
  %t2281 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t2280)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2282)
  store i64 %t2281, ptr %t2282
  %t2283 = load i64, ptr %t2278
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2284)
  store i64 %t2283, ptr %t2284
  %t2285 = load i64, ptr %t2279
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2286)
  store i64 %t2285, ptr %t2286
  %t2287 = load i64, ptr %t2284
  %t2289 = sext i32 0 to i64
  %t2288 = icmp slt i64 %t2287, %t2289
  %t2290 = load i64, ptr %t2284
  %t2291 = load i64, ptr %t2282
  %t2292 = icmp sge i64 %t2290, %t2291
  %t2293 = or i1 %t2288, %t2292
  %t2294 = load i64, ptr %t2286
  %t2295 = load i64, ptr %t2284
  %t2296 = icmp sle i64 %t2294, %t2295
  %t2297 = or i1 %t2293, %t2296
  br i1 %t2297, label %if.then687, label %if.end689
if.then687:
  ret ptr @.str.1
if.end689:
  %t2298 = load i64, ptr %t2286
  %t2299 = load i64, ptr %t2282
  %t2300 = icmp sgt i64 %t2298, %t2299
  br i1 %t2300, label %if.then690, label %if.end692
if.then690:
  %t2301 = load i64, ptr %t2282
  store i64 %t2301, ptr %t2286
  br label %if.end692
if.end692:
  %t2302 = load i64, ptr %t2286
  %t2303 = load i64, ptr %t2284
  %t2305 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t2302, i64 %t2303)
  %t2304 = extractvalue { i64, i1 } %t2305, 0
  %t2306 = extractvalue { i64, i1 } %t2305, 1
  br i1 %t2306, label %sub_overflow694, label %sub_ok693
sub_overflow694:
  call void @panic(ptr @.str.158)
  unreachable
sub_ok693:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2307)
  store i64 %t2304, ptr %t2307
  %t2308 = load i64, ptr %t2307
  %t2310 = sext i32 1 to i64
  %t2311 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2308, i64 %t2310)
  %t2309 = extractvalue { i64, i1 } %t2311, 0
  %t2312 = extractvalue { i64, i1 } %t2311, 1
  br i1 %t2312, label %add_overflow696, label %add_ok695
add_overflow696:
  call void @panic(ptr @.str.173)
  unreachable
add_ok695:
  %t2313 = call ptr @mem_alloc(i64 %t2309)
  %t2314 = alloca ptr
  store ptr %t2313, ptr %t2314
  %t2315 = load ptr, ptr %t2277
  %t2317 = ptrtoint ptr %t2315 to i64
  %t2318 = load i64, ptr %t2284
  %t2320 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2317, i64 %t2318)
  %t2319 = extractvalue { i64, i1 } %t2320, 0
  %t2321 = extractvalue { i64, i1 } %t2320, 1
  br i1 %t2321, label %add_overflow698, label %add_ok697
add_overflow698:
  call void @panic(ptr @.str.159)
  unreachable
add_ok697:
  %t2322 = inttoptr i64 %t2319 to ptr
  %t2323 = load ptr, ptr %t2314
  %t2324 = load i64, ptr %t2307
  call void @llvm.memcpy.p0.p0.i64(ptr %t2323, ptr %t2322, i64 %t2324, i1 false)
  %t2325 = load ptr, ptr %t2314
  %t2327 = ptrtoint ptr %t2325 to i64
  %t2328 = load i64, ptr %t2307
  %t2330 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2327, i64 %t2328)
  %t2329 = extractvalue { i64, i1 } %t2330, 0
  %t2331 = extractvalue { i64, i1 } %t2330, 1
  br i1 %t2331, label %add_overflow700, label %add_ok699
add_overflow700:
  call void @panic(ptr @.str.160)
  unreachable
add_ok699:
  %t2332 = inttoptr i64 %t2329 to ptr
  %t2333 = trunc i32 0 to i8
  store i8 %t2333, ptr %t2332
  %t2334 = load ptr, ptr %t2314
  ret ptr %t2334
}

; ir_diff::parser::strip_trailing_metadata
define internal ptr @tml_N7ir_diff6parser23strip_trailing_metadataE_S(ptr %line) #0 {
entry:
  %t2335 = alloca ptr
  store ptr %line, ptr %t2335
  %t2340 = alloca %struct.Maybe__I64
  %t2350 = alloca i64
  %t2355 = alloca i64
  %t2358 = alloca i64
  %t2370 = alloca %struct.Maybe__I64
  %t2379 = alloca i64
  %t2336 = alloca ptr
  store ptr @.str.174, ptr %t2336
  %t2337 = load ptr, ptr %t2335
  %t2338 = load ptr, ptr %t2336
  %t2339 = call %struct.Maybe__I64 @tml_N4core3str6search4findE_SS(ptr %t2337, ptr %t2338)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2340)
  store %struct.Maybe__I64 %t2339, ptr %t2340
  %t2341 = load %struct.Maybe__I64, ptr %t2340
  %t2342 = extractvalue %struct.Maybe__I64 %t2341, 0
  %t2343 = icmp eq i32 %t2342, 1
  br i1 %t2343, label %if.then701, label %if.end703
if.then701:
  %t2344 = load ptr, ptr %t2335
  ret ptr %t2344
if.end703:
  %t2345 = load %struct.Maybe__I64, ptr %t2340
  %t2346 = extractvalue %struct.Maybe__I64 %t2345, 0
  %t2347 = alloca %struct.Maybe__I64
  store %struct.Maybe__I64 %t2345, ptr %t2347
  %t2348 = getelementptr inbounds %struct.Maybe__I64, ptr %t2347, i32 0, i32 1
  %t2349 = load i64, ptr %t2348
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2350)
  store i64 %t2349, ptr %t2350
  %t2351 = load i64, ptr %t2350
  %t2353 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2351, i64 1)
  %t2352 = extractvalue { i64, i1 } %t2353, 0
  %t2354 = extractvalue { i64, i1 } %t2353, 1
  br i1 %t2354, label %add_overflow705, label %add_ok704
add_overflow705:
  call void @panic(ptr @.str.175)
  unreachable
add_ok704:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2355)
  store i64 %t2352, ptr %t2355
  %t2356 = load ptr, ptr %t2335
  %t2357 = call i64 @strlen(ptr %t2356)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2358)
  store i64 %t2357, ptr %t2358
  br label %loop.preheader706
loop.preheader706:
  br label %loop.header707
loop.header707:
  %t2359 = load i64, ptr %t2355
  %t2360 = load i64, ptr %t2358
  %t2361 = icmp slt i64 %t2359, %t2360
  br i1 %t2361, label %loop.body708, label %loop.exit710
loop.body708:
  %t2362 = load ptr, ptr %t2335
  %t2363 = load i64, ptr %t2355
  %t2364 = load i64, ptr %t2358
  %t2365 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t2362, i64 %t2363, i64 %t2364)
  %t2366 = alloca ptr
  store ptr %t2365, ptr %t2366
  %t2367 = load ptr, ptr %t2366
  %t2368 = load ptr, ptr %t2336
  %t2369 = call %struct.Maybe__I64 @tml_N4core3str6search4findE_SS(ptr %t2367, ptr %t2368)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2370)
  store %struct.Maybe__I64 %t2369, ptr %t2370
  %t2371 = load %struct.Maybe__I64, ptr %t2370
  %t2372 = extractvalue %struct.Maybe__I64 %t2371, 0
  %t2373 = icmp eq i32 %t2372, 1
  br i1 %t2373, label %if.then711, label %if.end713
if.then711:
  br label %loop.exit710
if.end713:
  %t2374 = load %struct.Maybe__I64, ptr %t2370
  %t2375 = extractvalue %struct.Maybe__I64 %t2374, 0
  %t2376 = alloca %struct.Maybe__I64
  store %struct.Maybe__I64 %t2374, ptr %t2376
  %t2377 = getelementptr inbounds %struct.Maybe__I64, ptr %t2376, i32 0, i32 1
  %t2378 = load i64, ptr %t2377
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2379)
  store i64 %t2378, ptr %t2379
  %t2380 = load i64, ptr %t2355
  %t2381 = load i64, ptr %t2379
  %t2383 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2380, i64 %t2381)
  %t2382 = extractvalue { i64, i1 } %t2383, 0
  %t2384 = extractvalue { i64, i1 } %t2383, 1
  br i1 %t2384, label %add_overflow715, label %add_ok714
add_overflow715:
  call void @panic(ptr @.str.103)
  unreachable
add_ok714:
  store i64 %t2382, ptr %t2350
  %t2385 = load i64, ptr %t2350
  %t2387 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2385, i64 1)
  %t2386 = extractvalue { i64, i1 } %t2387, 0
  %t2388 = extractvalue { i64, i1 } %t2387, 1
  br i1 %t2388, label %add_overflow717, label %add_ok716
add_overflow717:
  call void @panic(ptr @.str.176)
  unreachable
add_ok716:
  store i64 %t2386, ptr %t2355
  %t2389 = load ptr, ptr %t2366
  call void @tml_str_free(ptr %t2389)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t2379)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t2370)
  br label %loop.latch709
loop.latch709:
  br label %loop.header707, !llvm.loop !1027
loop.exit710:
  %t2390 = load ptr, ptr %t2335
  %t2391 = sext i32 0 to i64
  %t2392 = load i64, ptr %t2350
  %t2393 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t2390, i64 %t2391, i64 %t2392)
  ret ptr %t2393
}

; core::fmt::helpers::i8_to_binary_str
define ptr @tml_N4core3fmt7helpers16i8_to_binary_strE_a(i8 %n) #0 {
entry:
  %t2394 = alloca i8
  store i8 %n, ptr %t2394
  %t2395 = load i8, ptr %t2394
  %t2396 = call ptr @tml_N4core3fmt7helpers16u8_to_binary_strE_h(i8 %t2395)
  ret ptr %t2396
}

; ir_diff::parser::parse_instruction
define internal %struct.IrInstr @tml_N7ir_diff6parser17parse_instructionE_S(ptr %raw) #0 {
entry:
  %t2397 = alloca ptr
  store ptr %raw, ptr %t2397
  %t2408 = alloca %struct.Maybe__tuple_Str_Str
  %t2433 = alloca %struct.Maybe__tuple_Str_Str
  %t2454 = alloca %struct.List__Str
  %t2398 = load ptr, ptr %t2397
  %t2399 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t2398)
  %t2400 = alloca ptr
  store ptr %t2399, ptr %t2400
  %t2401 = alloca ptr
  store ptr null, ptr %t2401
  %t2402 = load ptr, ptr %t2400
  %t2403 = alloca ptr
  store ptr %t2402, ptr %t2403
  %t2404 = load ptr, ptr %t2400
  %t2405 = call i1 @tml_N4core3str6search11starts_withE_SS(ptr %t2404, ptr @.str.177)
  br i1 %t2405, label %if.then718, label %if.end720
if.then718:
  %t2406 = load ptr, ptr %t2400
  %t2407 = call %struct.Maybe__tuple_Str_Str @tml_N4core3str5split10split_onceE_SS(ptr %t2406, ptr @.str.46)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2408)
  store %struct.Maybe__tuple_Str_Str %t2407, ptr %t2408
  %t2409 = load %struct.Maybe__tuple_Str_Str, ptr %t2408
  %t2410 = extractvalue %struct.Maybe__tuple_Str_Str %t2409, 0
  %t2411 = icmp eq i32 %t2410, 0
  br i1 %t2411, label %if.then721, label %if.end723
if.then721:
  %t2412 = load %struct.Maybe__tuple_Str_Str, ptr %t2408
  %t2413 = extractvalue %struct.Maybe__tuple_Str_Str %t2412, 0
  %t2414 = alloca %struct.Maybe__tuple_Str_Str
  store %struct.Maybe__tuple_Str_Str %t2412, ptr %t2414
  %t2415 = getelementptr inbounds %struct.Maybe__tuple_Str_Str, ptr %t2414, i32 0, i32 1
  %t2416 = load { ptr, ptr }, ptr %t2415
  %t2417 = alloca { ptr, ptr }
  store { ptr, ptr } %t2416, ptr %t2417
  %t2418 = getelementptr inbounds { ptr, ptr }, ptr %t2417, i32 0, i32 0
  %t2419 = load ptr, ptr %t2418
  %t2420 = alloca ptr
  store ptr %t2419, ptr %t2420
  %t2421 = getelementptr inbounds { ptr, ptr }, ptr %t2417, i32 0, i32 1
  %t2422 = load ptr, ptr %t2421
  %t2423 = alloca ptr
  store ptr %t2422, ptr %t2423
  %t2424 = load ptr, ptr %t2420
  %t2425 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t2424)
  store ptr %t2425, ptr %t2401
  %t2426 = load ptr, ptr %t2423
  %t2427 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t2426)
  store ptr %t2427, ptr %t2403
  br label %if.end723
if.end723:
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t2408)
  br label %if.end720
if.end720:
  %t2428 = load ptr, ptr %t2403
  %t2429 = alloca ptr
  store ptr %t2428, ptr %t2429
  %t2430 = alloca ptr
  store ptr @.str.1, ptr %t2430
  %t2431 = load ptr, ptr %t2403
  %t2432 = call %struct.Maybe__tuple_Str_Str @tml_N4core3str5split10split_onceE_SS(ptr %t2431, ptr @.str.134)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2433)
  store %struct.Maybe__tuple_Str_Str %t2432, ptr %t2433
  %t2434 = load %struct.Maybe__tuple_Str_Str, ptr %t2433
  %t2435 = extractvalue %struct.Maybe__tuple_Str_Str %t2434, 0
  %t2436 = icmp eq i32 %t2435, 0
  br i1 %t2436, label %if.then724, label %if.end726
if.then724:
  %t2437 = load %struct.Maybe__tuple_Str_Str, ptr %t2433
  %t2438 = extractvalue %struct.Maybe__tuple_Str_Str %t2437, 0
  %t2439 = alloca %struct.Maybe__tuple_Str_Str
  store %struct.Maybe__tuple_Str_Str %t2437, ptr %t2439
  %t2440 = getelementptr inbounds %struct.Maybe__tuple_Str_Str, ptr %t2439, i32 0, i32 1
  %t2441 = load { ptr, ptr }, ptr %t2440
  %t2442 = alloca { ptr, ptr }
  store { ptr, ptr } %t2441, ptr %t2442
  %t2443 = getelementptr inbounds { ptr, ptr }, ptr %t2442, i32 0, i32 0
  %t2444 = load ptr, ptr %t2443
  %t2445 = alloca ptr
  store ptr %t2444, ptr %t2445
  %t2446 = getelementptr inbounds { ptr, ptr }, ptr %t2442, i32 0, i32 1
  %t2447 = load ptr, ptr %t2446
  %t2448 = alloca ptr
  store ptr %t2447, ptr %t2448
  %t2449 = load ptr, ptr %t2445
  store ptr %t2449, ptr %t2429
  %t2450 = load ptr, ptr %t2448
  %t2451 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t2450)
  store ptr %t2451, ptr %t2430
  br label %if.end726
if.end726:
  %t2452 = load ptr, ptr %t2430
  %t2453 = call %struct.List__Str @tml_N7ir_diff6parser14split_operandsE_S(ptr %t2452)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2454)
  store %struct.List__Str %t2453, ptr %t2454
  %t2455 = load ptr, ptr %t2429
  %t2456 = insertvalue %struct.IrInstr undef, ptr %t2455, 0
  %t2457 = load ptr, ptr %t2401
  %t2458 = insertvalue %struct.IrInstr %t2456, ptr %t2457, 1
  %t2459 = load %struct.List__Str, ptr %t2454
  %t2460 = insertvalue %struct.IrInstr %t2458, %struct.List__Str %t2459, 2
  %t2461 = load ptr, ptr %t2397
  %t2462 = insertvalue %struct.IrInstr %t2460, ptr %t2461, 3
  %t2463 = load ptr, ptr %t2400
  call void @tml_str_free(ptr %t2463)
  ret %struct.IrInstr %t2462
}

; ir_diff::differ::find_demangled
define internal i64 @tml_N7ir_diff6differ14find_demangledE_R8IrModuleS(ptr %b, ptr %demangled) #0 {
entry:
  %t2464 = alloca ptr
  store ptr %b, ptr %t2464
  %t2465 = alloca ptr
  store ptr %demangled, ptr %t2465
  %t2466 = alloca i64
  %t2472 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2466)
  store i64 0, ptr %t2466
  %t2467 = load ptr, ptr %t2464
  %t2468 = getelementptr inbounds %struct.IrModule, ptr %t2467, i32 0, i32 0
  %t2469 = load %struct.List__IrFunction, ptr %t2468
  %t2470 = alloca %struct.List__IrFunction
  store %struct.List__IrFunction %t2469, ptr %t2470
  %t2471 = call i64 @tml_N3std11collections4list16List__IrFunction3lenE(ptr %t2470)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2472)
  store i64 %t2471, ptr %t2472
  br label %loop.preheader727
loop.preheader727:
  br label %loop.header728
loop.header728:
  %t2473 = load i64, ptr %t2466
  %t2474 = load i64, ptr %t2472
  %t2475 = icmp slt i64 %t2473, %t2474
  br i1 %t2475, label %loop.body729, label %loop.exit731
loop.body729:
  %t2476 = load ptr, ptr %t2464
  %t2477 = getelementptr inbounds %struct.IrModule, ptr %t2476, i32 0, i32 0
  %t2478 = load %struct.List__IrFunction, ptr %t2477
  %t2479 = alloca %struct.List__IrFunction
  store %struct.List__IrFunction %t2478, ptr %t2479
  %t2480 = load i64, ptr %t2466
  %t2481 = call %struct.IrFunction @tml_N3std11collections4list16List__IrFunction3getE(ptr %t2479, i64 %t2480)
  %t2482 = alloca %struct.IrFunction
  store %struct.IrFunction %t2481, ptr %t2482
  %t2483 = getelementptr inbounds %struct.IrFunction, ptr %t2482, i32 0, i32 0
  %t2484 = load ptr, ptr %t2483
  %t2485 = call ptr @tml_N7ir_diff6differ13demangle_nameE_S(ptr %t2484)
  %t2486 = load ptr, ptr %t2465
  %t2487 = call i1 @tml_N7ir_diff6differ6str_eqE_SS(ptr %t2485, ptr %t2486)
  br i1 %t2487, label %if.then732, label %if.end734
if.then732:
  %t2488 = load i64, ptr %t2466
  ret i64 %t2488
if.end734:
  %t2489 = load i64, ptr %t2466
  %t2491 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2489, i64 1)
  %t2490 = extractvalue { i64, i1 } %t2491, 0
  %t2492 = extractvalue { i64, i1 } %t2491, 1
  br i1 %t2492, label %add_overflow736, label %add_ok735
add_overflow736:
  call void @panic(ptr @.str.178)
  unreachable
add_ok735:
  store i64 %t2490, ptr %t2466
  br label %loop.latch730
loop.latch730:
  br label %loop.header728, !llvm.loop !1028
loop.exit731:
  %t2493 = sub i32 0, 1
  %t2494 = sext i32 %t2493 to i64
  ret i64 %t2494
}

; core::str::basic::len
define i64 @tml_N4core3str5basic3lenE_S(ptr %s) #0 {
entry:
  %t2495 = alloca ptr
  store ptr %s, ptr %t2495
  %t2499 = alloca i64
  %t2496 = load ptr, ptr %t2495
  %t2498 = ptrtoint ptr %t2496 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2499)
  store i64 %t2498, ptr %t2499
  %t2500 = load i64, ptr %t2499
  %t2502 = sext i32 0 to i64
  %t2501 = icmp eq i64 %t2500, %t2502
  br i1 %t2501, label %if.then737, label %if.end739
if.then737:
  %t2503 = sext i32 0 to i64
  ret i64 %t2503
if.end739:
  %t2504 = load ptr, ptr %t2495
  %t2505 = call i64 @strlen(ptr %t2504)
  ret i64 %t2505
}

; core::fmt::helpers::i16_to_str
define ptr @tml_N4core3fmt7helpers10i16_to_strE_s(i16 %value) #0 {
entry:
  %t2506 = alloca i16
  store i16 %value, ptr %t2506
  %t2507 = load i16, ptr %t2506
  %t2508 = sext i16 %t2507 to i64
  %t2509 = call ptr @tml_N4core3fmt7helpers10i64_to_strE_l(i64 %t2508)
  ret ptr %t2509
}

; core::fmt::helpers::u8_to_str
define ptr @tml_N4core3fmt7helpers9u8_to_strE_h(i8 %value) #0 {
entry:
  %t2510 = alloca i8
  store i8 %value, ptr %t2510
  %t2511 = load i8, ptr %t2510
  %t2512 = zext i8 %t2511 to i64
  %t2513 = call ptr @tml_N4core3fmt7helpers10u64_to_strE_m(i64 %t2512)
  ret ptr %t2513
}

; core::fmt::helpers::u16_to_binary_str
define ptr @tml_N4core3fmt7helpers17u16_to_binary_strE_t(i16 %n) #0 {
entry:
  %t2514 = alloca i16
  store i16 %n, ptr %t2514
  %t2515 = load i16, ptr %t2514
  %t2516 = zext i16 %t2515 to i64
  %t2517 = call ptr @tml_N4core3fmt7helpers17u64_to_binary_strE_m(i64 %t2516)
  ret ptr %t2517
}

; core::fmt::helpers::i32_to_str
define ptr @tml_N4core3fmt7helpers10i32_to_strE_i(i32 %value) #0 {
entry:
  %t2518 = alloca i32
  store i32 %value, ptr %t2518
  %t2519 = load i32, ptr %t2518
  %t2520 = sext i32 %t2519 to i64
  %t2521 = call ptr @tml_N4core3fmt7helpers10i64_to_strE_l(i64 %t2520)
  ret ptr %t2521
}

; core::fmt::helpers::u16_to_str
define ptr @tml_N4core3fmt7helpers10u16_to_strE_t(i16 %value) #0 {
entry:
  %t2522 = alloca i16
  store i16 %value, ptr %t2522
  %t2523 = load i16, ptr %t2522
  %t2524 = zext i16 %t2523 to i64
  %t2525 = call ptr @tml_N4core3fmt7helpers10u64_to_strE_m(i64 %t2524)
  ret ptr %t2525
}

; ir_diff::differ::find_exact
define internal i64 @tml_N7ir_diff6differ10find_exactE_R8IrModuleS(ptr %b, ptr %name) #0 {
entry:
  %t2526 = alloca ptr
  store ptr %b, ptr %t2526
  %t2527 = alloca ptr
  store ptr %name, ptr %t2527
  %t2528 = alloca i64
  %t2534 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2528)
  store i64 0, ptr %t2528
  %t2529 = load ptr, ptr %t2526
  %t2530 = getelementptr inbounds %struct.IrModule, ptr %t2529, i32 0, i32 0
  %t2531 = load %struct.List__IrFunction, ptr %t2530
  %t2532 = alloca %struct.List__IrFunction
  store %struct.List__IrFunction %t2531, ptr %t2532
  %t2533 = call i64 @tml_N3std11collections4list16List__IrFunction3lenE(ptr %t2532)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2534)
  store i64 %t2533, ptr %t2534
  br label %loop.preheader740
loop.preheader740:
  br label %loop.header741
loop.header741:
  %t2535 = load i64, ptr %t2528
  %t2536 = load i64, ptr %t2534
  %t2537 = icmp slt i64 %t2535, %t2536
  br i1 %t2537, label %loop.body742, label %loop.exit744
loop.body742:
  %t2538 = load ptr, ptr %t2526
  %t2539 = getelementptr inbounds %struct.IrModule, ptr %t2538, i32 0, i32 0
  %t2540 = load %struct.List__IrFunction, ptr %t2539
  %t2541 = alloca %struct.List__IrFunction
  store %struct.List__IrFunction %t2540, ptr %t2541
  %t2542 = load i64, ptr %t2528
  %t2543 = call %struct.IrFunction @tml_N3std11collections4list16List__IrFunction3getE(ptr %t2541, i64 %t2542)
  %t2544 = alloca %struct.IrFunction
  store %struct.IrFunction %t2543, ptr %t2544
  %t2545 = getelementptr inbounds %struct.IrFunction, ptr %t2544, i32 0, i32 0
  %t2546 = load ptr, ptr %t2545
  %t2547 = load ptr, ptr %t2527
  %t2548 = call i1 @tml_N7ir_diff6differ6str_eqE_SS(ptr %t2546, ptr %t2547)
  br i1 %t2548, label %if.then745, label %if.end747
if.then745:
  %t2549 = load i64, ptr %t2528
  ret i64 %t2549
if.end747:
  %t2550 = load i64, ptr %t2528
  %t2552 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2550, i64 1)
  %t2551 = extractvalue { i64, i1 } %t2552, 0
  %t2553 = extractvalue { i64, i1 } %t2552, 1
  br i1 %t2553, label %add_overflow749, label %add_ok748
add_overflow749:
  call void @panic(ptr @.str.179)
  unreachable
add_ok748:
  store i64 %t2551, ptr %t2528
  br label %loop.latch743
loop.latch743:
  br label %loop.header741, !llvm.loop !1029
loop.exit744:
  %t2554 = sub i32 0, 1
  %t2555 = sext i32 %t2554 to i64
  ret i64 %t2555
}

; ir_diff::differ::flatten_instructions
define internal %struct.List__Str @tml_N7ir_diff6differ20flatten_instructionsE_R10IrFunction(ptr %f) #0 {
entry:
  %t2556 = alloca ptr
  store ptr %f, ptr %t2556
  %t2558 = alloca %struct.List__Str
  %t2559 = alloca i64
  %t2565 = alloca i64
  %t2575 = alloca %struct.IrBlock
  %t2586 = alloca i64
  %t2590 = alloca i64
  %t2557 = call %struct.List__Str @tml_N3std11collections4list9List__Str3newE(i64 16)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2558)
  store %struct.List__Str %t2557, ptr %t2558
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2559)
  store i64 0, ptr %t2559
  %t2560 = load ptr, ptr %t2556
  %t2561 = getelementptr inbounds %struct.IrFunction, ptr %t2560, i32 0, i32 3
  %t2562 = load %struct.List__IrBlock, ptr %t2561
  %t2563 = alloca %struct.List__IrBlock
  store %struct.List__IrBlock %t2562, ptr %t2563
  %t2564 = call i64 @tml_N3std11collections4list13List__IrBlock3lenE(ptr %t2563)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2565)
  store i64 %t2564, ptr %t2565
  br label %loop.preheader750
loop.preheader750:
  br label %loop.header751
loop.header751:
  %t2566 = load i64, ptr %t2559
  %t2567 = load i64, ptr %t2565
  %t2568 = icmp slt i64 %t2566, %t2567
  br i1 %t2568, label %loop.body752, label %loop.exit754
loop.body752:
  %t2569 = load ptr, ptr %t2556
  %t2570 = getelementptr inbounds %struct.IrFunction, ptr %t2569, i32 0, i32 3
  %t2571 = load %struct.List__IrBlock, ptr %t2570
  %t2572 = alloca %struct.List__IrBlock
  store %struct.List__IrBlock %t2571, ptr %t2572
  %t2573 = load i64, ptr %t2559
  %t2574 = call %struct.IrBlock @tml_N3std11collections4list13List__IrBlock3getE(ptr %t2572, i64 %t2573)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2575)
  store %struct.IrBlock %t2574, ptr %t2575
  %t2576 = getelementptr inbounds %struct.IrBlock, ptr %t2575, i32 0, i32 0
  %t2577 = load ptr, ptr %t2576
  %t2578 = call i64 @strlen(ptr %t2577)
  %t2580 = sext i32 0 to i64
  %t2579 = icmp sgt i64 %t2578, %t2580
  br i1 %t2579, label %if.then755, label %if.end757
if.then755:
  %t2581 = load %struct.List__Str, ptr %t2558
  %t2582 = getelementptr inbounds %struct.IrBlock, ptr %t2575, i32 0, i32 0
  %t2583 = load ptr, ptr %t2582
  %t2584 = call ptr @tml_N4core3str7convert6concatE_SS(ptr %t2583, ptr @.str.164)
  %t2585 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t2558, ptr %t2584)
  br label %if.end757
if.end757:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2586)
  store i64 0, ptr %t2586
  %t2587 = getelementptr inbounds %struct.IrBlock, ptr %t2575, i32 0, i32 1
  %t2588 = load %struct.List__IrInstr, ptr %t2587
  %t2589 = call i64 @tml_N3std11collections4list13List__IrInstr3lenE(ptr %t2587)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2590)
  store i64 %t2589, ptr %t2590
  br label %loop.preheader758
loop.preheader758:
  br label %loop.header759
loop.header759:
  %t2591 = load i64, ptr %t2586
  %t2592 = load i64, ptr %t2590
  %t2593 = icmp slt i64 %t2591, %t2592
  br i1 %t2593, label %loop.body760, label %loop.exit762
loop.body760:
  %t2594 = load %struct.List__Str, ptr %t2558
  %t2595 = getelementptr inbounds %struct.IrBlock, ptr %t2575, i32 0, i32 1
  %t2596 = load %struct.List__IrInstr, ptr %t2595
  %t2597 = load i64, ptr %t2586
  %t2598 = call %struct.IrInstr @tml_N3std11collections4list13List__IrInstr3getE(ptr %t2595, i64 %t2597)
  %t2599 = alloca %struct.IrInstr
  store %struct.IrInstr %t2598, ptr %t2599
  %t2600 = getelementptr inbounds %struct.IrInstr, ptr %t2599, i32 0, i32 3
  %t2601 = load ptr, ptr %t2600
  %t2602 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t2558, ptr %t2601)
  %t2603 = load i64, ptr %t2586
  %t2605 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2603, i64 1)
  %t2604 = extractvalue { i64, i1 } %t2605, 0
  %t2606 = extractvalue { i64, i1 } %t2605, 1
  br i1 %t2606, label %add_overflow764, label %add_ok763
add_overflow764:
  call void @panic(ptr @.str.180)
  unreachable
add_ok763:
  store i64 %t2604, ptr %t2586
  br label %loop.latch761
loop.latch761:
  br label %loop.header759, !llvm.loop !1031
loop.exit762:
  %t2607 = load i64, ptr %t2559
  %t2609 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2607, i64 1)
  %t2608 = extractvalue { i64, i1 } %t2609, 0
  %t2610 = extractvalue { i64, i1 } %t2609, 1
  br i1 %t2610, label %add_overflow766, label %add_ok765
add_overflow766:
  call void @panic(ptr @.str.181)
  unreachable
add_ok765:
  store i64 %t2608, ptr %t2559
  call void @llvm.lifetime.end.p0(i64 8, ptr %t2590)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t2586)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t2575)
  br label %loop.latch753
loop.latch753:
  br label %loop.header751, !llvm.loop !1030
loop.exit754:
  %t2611 = load %struct.List__Str, ptr %t2558
  ret %struct.List__Str %t2611
}
; DEBUG LAZY type_name=Buffer method=destroy

define internal void @tml_N3std11collections6buffer6Buffer7destroyE(ptr %this) #0 {
entry:
  %t2612 = getelementptr inbounds %struct.Buffer, ptr %this, i32 0, i32 0
  %t2613 = load ptr, ptr %t2612
  %t2615 = zext i32 0 to i64
  %t2614 = inttoptr i64 %t2615 to ptr
  %t2616 = icmp eq ptr %t2613, %t2614
  br i1 %t2616, label %if.then767, label %if.end769
if.then767:
  ret void
if.end769:
  %t2617 = getelementptr inbounds %struct.Buffer, ptr %this, i32 0, i32 0
  %t2618 = load ptr, ptr %t2617
  %t2620 = ptrtoint ptr %t2618 to i64
  %t2621 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2621)
  store i64 %t2620, ptr %t2621
  %t2622 = load i64, ptr %t2621
  %t2623 = call ptr @tml_N3std11collections6buffer12buf_get_dataE_l(i64 %t2622)
  %t2624 = alloca ptr
  store ptr %t2623, ptr %t2624
  %t2625 = load ptr, ptr %t2624
  %t2627 = ptrtoint ptr %t2625 to i64
  %t2628 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2628)
  store i64 %t2627, ptr %t2628
  %t2629 = load i64, ptr %t2628
  %t2630 = load i64, ptr %t2621
  %t2632 = sext i32 32 to i64
  %t2633 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2630, i64 %t2632)
  %t2631 = extractvalue { i64, i1 } %t2633, 0
  %t2634 = extractvalue { i64, i1 } %t2633, 1
  br i1 %t2634, label %add_overflow771, label %add_ok770
add_overflow771:
  call void @panic(ptr @.str.182)
  unreachable
add_ok770:
  %t2635 = icmp ne i64 %t2629, %t2631
  br i1 %t2635, label %if.then772, label %if.end774
if.then772:
  %t2636 = load ptr, ptr %t2624
  call void @tml_N3std11collections6buffer12buf_mem_freeE_Pv(ptr %t2636)
  br label %if.end774
if.end774:
  %t2637 = getelementptr inbounds %struct.Buffer, ptr %this, i32 0, i32 0
  %t2638 = load ptr, ptr %t2637
  call void @tml_N3std11collections6buffer12buf_mem_freeE_Pv(ptr %t2638)
  %t2640 = zext i32 0 to i64
  %t2639 = inttoptr i64 %t2640 to ptr
  %t2641 = getelementptr inbounds %struct.Buffer, ptr %this, i32 0, i32 0
  store ptr %t2639, ptr %t2641
  ret void
}

; ir_diff::differ::diff_instruction_lists
define internal %struct.Maybe__FunctionDiff @tml_N7ir_diff6differ22diff_instruction_listsE_SR4ListISER4ListISE(ptr %name, ptr %a_lines, ptr %b_lines) #0 {
entry:
  %t2642 = alloca ptr
  store ptr %name, ptr %t2642
  %t2643 = alloca ptr
  store ptr %a_lines, ptr %t2643
  %t2644 = alloca ptr
  store ptr %b_lines, ptr %t2644
  %t2647 = alloca i64
  %t2650 = alloca i64
  %t2652 = alloca i64
  %t2657 = alloca i64
  %t2645 = load ptr, ptr %t2643
  %t2646 = call i64 @tml_N3std11collections4list9List__Str3lenE(ptr %t2645)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2647)
  store i64 %t2646, ptr %t2647
  %t2648 = load ptr, ptr %t2644
  %t2649 = call i64 @tml_N3std11collections4list9List__Str3lenE(ptr %t2648)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2650)
  store i64 %t2649, ptr %t2650
  %t2651 = load i64, ptr %t2647
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2652)
  store i64 %t2651, ptr %t2652
  %t2653 = load i64, ptr %t2650
  %t2654 = load i64, ptr %t2652
  %t2655 = icmp slt i64 %t2653, %t2654
  br i1 %t2655, label %if.then775, label %if.end777
if.then775:
  %t2656 = load i64, ptr %t2650
  store i64 %t2656, ptr %t2652
  br label %if.end777
if.end777:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2657)
  store i64 0, ptr %t2657
  br label %loop.preheader778
loop.preheader778:
  br label %loop.header779
loop.header779:
  %t2658 = load i64, ptr %t2657
  %t2659 = load i64, ptr %t2652
  %t2660 = icmp slt i64 %t2658, %t2659
  br i1 %t2660, label %loop.body780, label %loop.exit782
loop.body780:
  %t2661 = load ptr, ptr %t2643
  %t2662 = load i64, ptr %t2657
  %t2663 = call ptr @tml_N3std11collections4list9List__Str3getE(ptr %t2661, i64 %t2662)
  %t2664 = alloca ptr
  store ptr %t2663, ptr %t2664
  %t2665 = load ptr, ptr %t2644
  %t2666 = load i64, ptr %t2657
  %t2667 = call ptr @tml_N3std11collections4list9List__Str3getE(ptr %t2665, i64 %t2666)
  %t2668 = alloca ptr
  store ptr %t2667, ptr %t2668
  %t2669 = load ptr, ptr %t2664
  %t2670 = load ptr, ptr %t2668
  %t2671 = call i1 @tml_N7ir_diff6differ6str_eqE_SS(ptr %t2669, ptr %t2670)
  %t2672 = xor i1 %t2671, 1
  br i1 %t2672, label %if.then783, label %if.end785
if.then783:
  %t2674 = alloca %struct.Maybe__FunctionDiff, align 8
  %t2675 = getelementptr inbounds %struct.Maybe__FunctionDiff, ptr %t2674, i32 0, i32 0
  store i32 0, ptr %t2675
  %t2676 = load ptr, ptr %t2642
  %t2677 = insertvalue %struct.FunctionDiff undef, ptr %t2676, 0
  %t2678 = load i64, ptr %t2657
  %t2679 = insertvalue %struct.FunctionDiff %t2677, i64 %t2678, 1
  %t2680 = load ptr, ptr %t2664
  %t2681 = insertvalue %struct.FunctionDiff %t2679, ptr %t2680, 2
  %t2682 = load ptr, ptr %t2668
  %t2683 = insertvalue %struct.FunctionDiff %t2681, ptr %t2682, 3
  %t2684 = load ptr, ptr %t2643
  %t2685 = load i64, ptr %t2657
  %t2686 = call %struct.List__Str @tml_N7ir_diff6differ13build_contextE_R4ListISEl(ptr %t2684, i64 %t2685)
  %t2687 = insertvalue %struct.FunctionDiff %t2683, %struct.List__Str %t2686, 4
  %t2688 = getelementptr inbounds %struct.Maybe__FunctionDiff, ptr %t2674, i32 0, i32 1
  %t2689 = bitcast ptr %t2688 to ptr
  store %struct.FunctionDiff %t2687, ptr %t2689
  %t2673 = load %struct.Maybe__FunctionDiff, ptr %t2674
  ret %struct.Maybe__FunctionDiff %t2673
if.end785:
  %t2690 = load i64, ptr %t2657
  %t2692 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2690, i64 1)
  %t2691 = extractvalue { i64, i1 } %t2692, 0
  %t2693 = extractvalue { i64, i1 } %t2692, 1
  br i1 %t2693, label %add_overflow787, label %add_ok786
add_overflow787:
  call void @panic(ptr @.str.30)
  unreachable
add_ok786:
  store i64 %t2691, ptr %t2657
  br label %loop.latch781
loop.latch781:
  br label %loop.header779, !llvm.loop !1032
loop.exit782:
  %t2694 = load i64, ptr %t2647
  %t2695 = load i64, ptr %t2650
  %t2696 = icmp ne i64 %t2694, %t2695
  br i1 %t2696, label %if.then788, label %if.end790
if.then788:
  %t2697 = alloca ptr
  store ptr @.str.1, ptr %t2697
  %t2698 = alloca ptr
  store ptr @.str.1, ptr %t2698
  %t2699 = load i64, ptr %t2647
  %t2700 = load i64, ptr %t2650
  %t2701 = icmp sgt i64 %t2699, %t2700
  br i1 %t2701, label %if.then791, label %if.end793
if.then791:
  %t2702 = load ptr, ptr %t2643
  %t2703 = load i64, ptr %t2650
  %t2704 = call ptr @tml_N3std11collections4list9List__Str3getE(ptr %t2702, i64 %t2703)
  store ptr %t2704, ptr %t2697
  br label %if.end793
if.end793:
  %t2705 = load i64, ptr %t2650
  %t2706 = load i64, ptr %t2647
  %t2707 = icmp sgt i64 %t2705, %t2706
  br i1 %t2707, label %if.then794, label %if.end796
if.then794:
  %t2708 = load ptr, ptr %t2644
  %t2709 = load i64, ptr %t2647
  %t2710 = call ptr @tml_N3std11collections4list9List__Str3getE(ptr %t2708, i64 %t2709)
  store ptr %t2710, ptr %t2698
  br label %if.end796
if.end796:
  %t2712 = alloca %struct.Maybe__FunctionDiff, align 8
  %t2713 = getelementptr inbounds %struct.Maybe__FunctionDiff, ptr %t2712, i32 0, i32 0
  store i32 0, ptr %t2713
  %t2714 = load ptr, ptr %t2642
  %t2715 = insertvalue %struct.FunctionDiff undef, ptr %t2714, 0
  %t2716 = load i64, ptr %t2652
  %t2717 = insertvalue %struct.FunctionDiff %t2715, i64 %t2716, 1
  %t2718 = load ptr, ptr %t2697
  %t2719 = insertvalue %struct.FunctionDiff %t2717, ptr %t2718, 2
  %t2720 = load ptr, ptr %t2698
  %t2721 = insertvalue %struct.FunctionDiff %t2719, ptr %t2720, 3
  %t2722 = load ptr, ptr %t2643
  %t2723 = load i64, ptr %t2652
  %t2724 = call %struct.List__Str @tml_N7ir_diff6differ13build_contextE_R4ListISEl(ptr %t2722, i64 %t2723)
  %t2725 = insertvalue %struct.FunctionDiff %t2721, %struct.List__Str %t2724, 4
  %t2726 = getelementptr inbounds %struct.Maybe__FunctionDiff, ptr %t2712, i32 0, i32 1
  %t2727 = bitcast ptr %t2726 to ptr
  store %struct.FunctionDiff %t2725, ptr %t2727
  %t2711 = load %struct.Maybe__FunctionDiff, ptr %t2712
  ret %struct.Maybe__FunctionDiff %t2711
if.end790:
  %t2729 = alloca %struct.Maybe__FunctionDiff, align 8
  %t2730 = getelementptr inbounds %struct.Maybe__FunctionDiff, ptr %t2729, i32 0, i32 0
  store i32 1, ptr %t2730
  %t2728 = load %struct.Maybe__FunctionDiff, ptr %t2729
  ret %struct.Maybe__FunctionDiff %t2728
}

; core::fmt::helpers::u8_to_binary_str
define ptr @tml_N4core3fmt7helpers16u8_to_binary_strE_h(i8 %n) #0 {
entry:
  %t2731 = alloca i8
  store i8 %n, ptr %t2731
  %t2732 = load i8, ptr %t2731
  %t2733 = zext i8 %t2732 to i64
  %t2734 = call ptr @tml_N4core3fmt7helpers17u64_to_binary_strE_m(i64 %t2733)
  ret ptr %t2734
}

; core::fmt::helpers::i16_to_binary_str
define ptr @tml_N4core3fmt7helpers17i16_to_binary_strE_s(i16 %n) #0 {
entry:
  %t2735 = alloca i16
  store i16 %n, ptr %t2735
  %t2736 = load i16, ptr %t2735
  %t2737 = call ptr @tml_N4core3fmt7helpers17u16_to_binary_strE_t(i16 %t2736)
  ret ptr %t2737
}

; core::fmt::helpers::u32_to_binary_str
define ptr @tml_N4core3fmt7helpers17u32_to_binary_strE_j(i32 %n) #0 {
entry:
  %t2738 = alloca i32
  store i32 %n, ptr %t2738
  %t2739 = load i32, ptr %t2738
  %t2740 = zext i32 %t2739 to i64
  %t2741 = call ptr @tml_N4core3fmt7helpers17u64_to_binary_strE_m(i64 %t2740)
  ret ptr %t2741
}

; core::fmt::helpers::i32_to_binary_str
define ptr @tml_N4core3fmt7helpers17i32_to_binary_strE_i(i32 %n) #0 {
entry:
  %t2742 = alloca i32
  store i32 %n, ptr %t2742
  %t2743 = load i32, ptr %t2742
  %t2744 = call ptr @tml_N4core3fmt7helpers17u32_to_binary_strE_j(i32 %t2743)
  ret ptr %t2744
}

; core::fmt::helpers::i64_to_binary_str
define ptr @tml_N4core3fmt7helpers17i64_to_binary_strE_l(i64 %n) #0 {
entry:
  %t2745 = alloca i64
  store i64 %n, ptr %t2745
  %t2746 = load i64, ptr %t2745
  %t2747 = call ptr @tml_N4core3fmt7helpers17u64_to_binary_strE_m(i64 %t2746)
  ret ptr %t2747
}

; core::str::search::contains
define i1 @tml_N4core3str6search8containsE_SS(ptr %s, ptr %pattern) #0 {
entry:
  %t2748 = alloca ptr
  store ptr %s, ptr %t2748
  %t2749 = alloca ptr
  store ptr %pattern, ptr %t2749
  %t2752 = alloca i64
  %t2755 = alloca i64
  %t2771 = alloca i64
  %t2779 = alloca i64
  %t2780 = alloca i64
  %t2750 = load ptr, ptr %t2748
  %t2751 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t2750)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2752)
  store i64 %t2751, ptr %t2752
  %t2753 = load ptr, ptr %t2749
  %t2754 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t2753)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2755)
  store i64 %t2754, ptr %t2755
  %t2756 = load i64, ptr %t2755
  %t2758 = sext i32 0 to i64
  %t2757 = icmp eq i64 %t2756, %t2758
  br i1 %t2757, label %if.then797, label %if.end799
if.then797:
  ret i1 1
if.end799:
  %t2759 = load i64, ptr %t2755
  %t2760 = load i64, ptr %t2752
  %t2761 = icmp sgt i64 %t2759, %t2760
  br i1 %t2761, label %if.then800, label %if.end802
if.then800:
  ret i1 0
if.end802:
  %t2762 = load i64, ptr %t2752
  %t2764 = sext i32 32 to i64
  %t2763 = icmp sge i64 %t2762, %t2764
  br i1 %t2763, label %if.then803, label %if.end805
if.then803:
  %t2765 = load ptr, ptr %t2748
  %t2766 = load ptr, ptr %t2749
  %t2767 = call i1 @tml_N4core3str4simd13contains_simdE_SS(ptr %t2765, ptr %t2766)
  ret i1 %t2767
if.end805:
  %t2768 = load ptr, ptr %t2748
  %t2770 = ptrtoint ptr %t2768 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2771)
  store i64 %t2770, ptr %t2771
  %t2772 = load ptr, ptr %t2749
  %t2773 = alloca ptr
  store ptr %t2772, ptr %t2773
  %t2774 = load i64, ptr %t2752
  %t2775 = load i64, ptr %t2755
  %t2777 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t2774, i64 %t2775)
  %t2776 = extractvalue { i64, i1 } %t2777, 0
  %t2778 = extractvalue { i64, i1 } %t2777, 1
  br i1 %t2778, label %sub_overflow807, label %sub_ok806
sub_overflow807:
  call void @panic(ptr @.str.98)
  unreachable
sub_ok806:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2779)
  store i64 %t2776, ptr %t2779
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2780)
  store i64 0, ptr %t2780
  br label %loop.preheader808
loop.preheader808:
  br label %loop.header809
loop.header809:
  %t2781 = load i64, ptr %t2780
  %t2782 = load i64, ptr %t2779
  %t2783 = icmp sle i64 %t2781, %t2782
  br i1 %t2783, label %loop.body810, label %loop.exit812
loop.body810:
  %t2784 = load i64, ptr %t2771
  %t2785 = load i64, ptr %t2780
  %t2787 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2784, i64 %t2785)
  %t2786 = extractvalue { i64, i1 } %t2787, 0
  %t2788 = extractvalue { i64, i1 } %t2787, 1
  br i1 %t2788, label %add_overflow814, label %add_ok813
add_overflow814:
  call void @panic(ptr @.str.92)
  unreachable
add_ok813:
  %t2789 = inttoptr i64 %t2786 to ptr
  %t2790 = load ptr, ptr %t2773
  %t2791 = load i64, ptr %t2755
  %t2792 = call i32 @memcmp(ptr %t2789, ptr %t2790, i64 %t2791)
  %t2793 = icmp eq i32 %t2792, 0
  br i1 %t2793, label %if.then815, label %if.end817
if.then815:
  ret i1 1
if.end817:
  %t2794 = load i64, ptr %t2780
  %t2796 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2794, i64 1)
  %t2795 = extractvalue { i64, i1 } %t2796, 0
  %t2797 = extractvalue { i64, i1 } %t2796, 1
  br i1 %t2797, label %add_overflow819, label %add_ok818
add_overflow819:
  call void @panic(ptr @.str.183)
  unreachable
add_ok818:
  store i64 %t2795, ptr %t2780
  br label %loop.latch811
loop.latch811:
  br label %loop.header809, !llvm.loop !1033
loop.exit812:
  ret i1 0
}

; ir_diff::differ::demangle_once
define internal ptr @tml_N7ir_diff6differ13demangle_onceE_S(ptr %name) #0 {
entry:
  %t2798 = alloca ptr
  store ptr %name, ptr %t2798
  %t2801 = alloca i64
  %t2806 = alloca i64
  %t2813 = alloca i32
  %t2799 = load ptr, ptr %t2798
  %t2800 = call i64 @strlen(ptr %t2799)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2801)
  store i64 %t2800, ptr %t2801
  %t2802 = load i64, ptr %t2801
  %t2804 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t2802, i64 1)
  %t2803 = extractvalue { i64, i1 } %t2804, 0
  %t2805 = extractvalue { i64, i1 } %t2804, 1
  br i1 %t2805, label %sub_overflow821, label %sub_ok820
sub_overflow821:
  call void @panic(ptr @.str.184)
  unreachable
sub_ok820:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2806)
  store i64 %t2803, ptr %t2806
  br label %loop.preheader822
loop.preheader822:
  br label %loop.header823
loop.header823:
  %t2807 = load i64, ptr %t2806
  %t2809 = sext i32 0 to i64
  %t2808 = icmp sge i64 %t2807, %t2809
  br i1 %t2808, label %loop.body824, label %loop.exit826
loop.body824:
  %t2810 = load ptr, ptr %t2798
  %t2811 = load i64, ptr %t2806
  %t2812 = call i32 @tml_N4core3str5basic7char_atE_Sl(ptr %t2810, i64 %t2811)
  call void @llvm.lifetime.start.p0(i64 4, ptr %t2813)
  store i32 %t2812, ptr %t2813
  %t2814 = load i32, ptr %t2813
  %t2815 = call i1 @tml_N7ir_diff6differ16is_type_arg_charE_i(i32 %t2814)
  br i1 %t2815, label %if.then827, label %if.else828
if.then827:
  %t2816 = load i64, ptr %t2806
  %t2818 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t2816, i64 1)
  %t2817 = extractvalue { i64, i1 } %t2818, 0
  %t2819 = extractvalue { i64, i1 } %t2818, 1
  br i1 %t2819, label %sub_overflow831, label %sub_ok830
sub_overflow831:
  call void @panic(ptr @.str.185)
  unreachable
sub_ok830:
  store i64 %t2817, ptr %t2806
  br label %if.end829
if.else828:
  br label %loop.exit826
if.end829:
  call void @llvm.lifetime.end.p0(i64 4, ptr %t2813)
  br label %loop.latch825
loop.latch825:
  br label %loop.header823, !llvm.loop !1034
loop.exit826:
  %t2820 = load i64, ptr %t2806
  %t2822 = sext i32 0 to i64
  %t2821 = icmp sge i64 %t2820, %t2822
  %t2823 = load i64, ptr %t2806
  %t2824 = load i64, ptr %t2801
  %t2826 = sext i32 1 to i64
  %t2827 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t2824, i64 %t2826)
  %t2825 = extractvalue { i64, i1 } %t2827, 0
  %t2828 = extractvalue { i64, i1 } %t2827, 1
  br i1 %t2828, label %sub_overflow833, label %sub_ok832
sub_overflow833:
  call void @panic(ptr @.str.186)
  unreachable
sub_ok832:
  %t2829 = icmp slt i64 %t2823, %t2825
  %t2830 = and i1 %t2821, %t2829
  %t2831 = load ptr, ptr %t2798
  %t2832 = load i64, ptr %t2806
  %t2833 = call i32 @tml_N4core3str5basic7char_atE_Sl(ptr %t2831, i64 %t2832)
  %t2834 = icmp eq i32 %t2833, 95
  %t2835 = and i1 %t2830, %t2834
  br i1 %t2835, label %if.then834, label %if.end836
if.then834:
  %t2836 = load ptr, ptr %t2798
  %t2837 = sext i32 0 to i64
  %t2838 = load i64, ptr %t2806
  %t2839 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t2836, i64 %t2837, i64 %t2838)
  ret ptr %t2839
if.end836:
  %t2840 = load ptr, ptr %t2798
  ret ptr %t2840
}

; ir_diff::parser::split_operands
define internal %struct.List__Str @tml_N7ir_diff6parser14split_operandsE_S(ptr %s) #0 {
entry:
  %t2841 = alloca ptr
  store ptr %s, ptr %t2841
  %t2843 = alloca %struct.List__Str
  %t2846 = alloca i64
  %t2851 = alloca i64
  %t2852 = alloca i64
  %t2853 = alloca i64
  %t2856 = alloca %struct.List__U8
  %t2863 = alloca i8
  %t2866 = alloca i64
  %t2842 = call %struct.List__Str @tml_N3std11collections4list9List__Str3newE(i64 4)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2843)
  store %struct.List__Str %t2842, ptr %t2843
  %t2844 = load ptr, ptr %t2841
  %t2845 = call i64 @strlen(ptr %t2844)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2846)
  store i64 %t2845, ptr %t2846
  %t2847 = load i64, ptr %t2846
  %t2849 = sext i32 0 to i64
  %t2848 = icmp eq i64 %t2847, %t2849
  br i1 %t2848, label %if.then837, label %if.end839
if.then837:
  %t2850 = load %struct.List__Str, ptr %t2843
  ret %struct.List__Str %t2850
if.end839:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2851)
  store i64 0, ptr %t2851
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2852)
  store i64 0, ptr %t2852
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2853)
  store i64 0, ptr %t2853
  %t2854 = load ptr, ptr %t2841
  %t2855 = call %struct.List__U8 @tml_N4core3str7convert5bytesE_S(ptr %t2854)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2856)
  store %struct.List__U8 %t2855, ptr %t2856
  br label %loop.preheader840
loop.preheader840:
  br label %loop.header841
loop.header841:
  %t2857 = load i64, ptr %t2852
  %t2858 = load i64, ptr %t2846
  %t2859 = icmp slt i64 %t2857, %t2858
  br i1 %t2859, label %loop.body842, label %loop.exit844
loop.body842:
  %t2860 = load %struct.List__U8, ptr %t2856
  %t2861 = load i64, ptr %t2852
  %t2862 = call i8 @tml_N3std11collections4list8List__U83getE(ptr %t2856, i64 %t2861)
  call void @llvm.lifetime.start.p0(i64 1, ptr %t2863)
  store i8 %t2862, ptr %t2863
  %t2864 = load i8, ptr %t2863
  %t2865 = zext i8 %t2864 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2866)
  store i64 %t2865, ptr %t2866
  %t2867 = load i64, ptr %t2866
  %t2869 = sext i32 40 to i64
  %t2868 = icmp eq i64 %t2867, %t2869
  %t2870 = load i64, ptr %t2866
  %t2872 = sext i32 91 to i64
  %t2871 = icmp eq i64 %t2870, %t2872
  %t2873 = or i1 %t2868, %t2871
  %t2874 = load i64, ptr %t2866
  %t2876 = sext i32 60 to i64
  %t2875 = icmp eq i64 %t2874, %t2876
  %t2877 = or i1 %t2873, %t2875
  %t2878 = load i64, ptr %t2866
  %t2880 = sext i32 123 to i64
  %t2879 = icmp eq i64 %t2878, %t2880
  %t2881 = or i1 %t2877, %t2879
  br i1 %t2881, label %if.then845, label %if.else846
if.then845:
  %t2882 = load i64, ptr %t2853
  %t2884 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2882, i64 1)
  %t2883 = extractvalue { i64, i1 } %t2884, 0
  %t2885 = extractvalue { i64, i1 } %t2884, 1
  br i1 %t2885, label %add_overflow849, label %add_ok848
add_overflow849:
  call void @panic(ptr @.str.145)
  unreachable
add_ok848:
  store i64 %t2883, ptr %t2853
  br label %if.end847
if.else846:
  %t2886 = load i64, ptr %t2866
  %t2888 = sext i32 41 to i64
  %t2887 = icmp eq i64 %t2886, %t2888
  %t2889 = load i64, ptr %t2866
  %t2891 = sext i32 93 to i64
  %t2890 = icmp eq i64 %t2889, %t2891
  %t2892 = or i1 %t2887, %t2890
  %t2893 = load i64, ptr %t2866
  %t2895 = sext i32 62 to i64
  %t2894 = icmp eq i64 %t2893, %t2895
  %t2896 = or i1 %t2892, %t2894
  %t2897 = load i64, ptr %t2866
  %t2899 = sext i32 125 to i64
  %t2898 = icmp eq i64 %t2897, %t2899
  %t2900 = or i1 %t2896, %t2898
  br i1 %t2900, label %if.then850, label %if.else851
if.then850:
  %t2901 = load i64, ptr %t2853
  %t2903 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t2901, i64 1)
  %t2902 = extractvalue { i64, i1 } %t2903, 0
  %t2904 = extractvalue { i64, i1 } %t2903, 1
  br i1 %t2904, label %sub_overflow854, label %sub_ok853
sub_overflow854:
  call void @panic(ptr @.str.187)
  unreachable
sub_ok853:
  store i64 %t2902, ptr %t2853
  br label %if.end852
if.else851:
  %t2905 = load i64, ptr %t2866
  %t2907 = sext i32 44 to i64
  %t2906 = icmp eq i64 %t2905, %t2907
  %t2908 = load i64, ptr %t2853
  %t2910 = sext i32 0 to i64
  %t2909 = icmp eq i64 %t2908, %t2910
  %t2911 = and i1 %t2906, %t2909
  br i1 %t2911, label %if.then855, label %if.end857
if.then855:
  %t2912 = load ptr, ptr %t2841
  %t2913 = load i64, ptr %t2851
  %t2914 = load i64, ptr %t2852
  %t2915 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t2912, i64 %t2913, i64 %t2914)
  %t2916 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t2915)
  %t2917 = alloca ptr
  store ptr %t2916, ptr %t2917
  %t2918 = load %struct.List__Str, ptr %t2843
  %t2919 = load ptr, ptr %t2917
  %t2920 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t2843, ptr %t2919)
  %t2921 = load i64, ptr %t2852
  %t2923 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2921, i64 1)
  %t2922 = extractvalue { i64, i1 } %t2923, 0
  %t2924 = extractvalue { i64, i1 } %t2923, 1
  br i1 %t2924, label %add_overflow859, label %add_ok858
add_overflow859:
  call void @panic(ptr @.str.188)
  unreachable
add_ok858:
  store i64 %t2922, ptr %t2851
  br label %if.end857
if.end857:
  br label %if.end852
if.end852:
  br label %if.end847
if.end847:
  %t2925 = load i64, ptr %t2852
  %t2927 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2925, i64 1)
  %t2926 = extractvalue { i64, i1 } %t2927, 0
  %t2928 = extractvalue { i64, i1 } %t2927, 1
  br i1 %t2928, label %add_overflow861, label %add_ok860
add_overflow861:
  call void @panic(ptr @.str.148)
  unreachable
add_ok860:
  store i64 %t2926, ptr %t2852
  call void @llvm.lifetime.end.p0(i64 8, ptr %t2866)
  call void @llvm.lifetime.end.p0(i64 1, ptr %t2863)
  br label %loop.latch843
loop.latch843:
  br label %loop.header841, !llvm.loop !1035
loop.exit844:
  %t2929 = load i64, ptr %t2851
  %t2930 = load i64, ptr %t2846
  %t2931 = icmp sle i64 %t2929, %t2930
  br i1 %t2931, label %if.then862, label %if.end864
if.then862:
  %t2932 = load ptr, ptr %t2841
  %t2933 = load i64, ptr %t2851
  %t2934 = load i64, ptr %t2846
  %t2935 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t2932, i64 %t2933, i64 %t2934)
  %t2936 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t2935)
  %t2937 = alloca ptr
  store ptr %t2936, ptr %t2937
  %t2938 = load ptr, ptr %t2937
  %t2939 = call i64 @strlen(ptr %t2938)
  %t2941 = sext i32 0 to i64
  %t2940 = icmp sgt i64 %t2939, %t2941
  br i1 %t2940, label %if.then865, label %if.end867
if.then865:
  %t2942 = load %struct.List__Str, ptr %t2843
  %t2943 = load ptr, ptr %t2937
  %t2944 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t2843, ptr %t2943)
  br label %if.end867
if.end867:
  br label %if.end864
if.end864:
  %t2945 = load %struct.List__Str, ptr %t2843
  %t2946 = load %struct.List__U8, ptr %t2856
  call void @tml_N3std11collections4list8List__U84dropE(ptr %t2856)
  ret %struct.List__Str %t2945
}

; core::str::simd::trim_start_simd
define i64 @tml_N4core3str4simd15trim_start_simdE_S(ptr %s) #0 {
entry:
  %t2947 = alloca ptr
  store ptr %s, ptr %t2947
  %t2950 = alloca i64
  %t2954 = alloca i64
  %t2957 = alloca %struct.I8x16
  %t2960 = alloca %struct.I8x16
  %t2963 = alloca %struct.I8x16
  %t2966 = alloca %struct.I8x16
  %t2967 = alloca i64
  %t2982 = alloca %struct.I8x16
  %t2987 = alloca %struct.I8x16
  %t2992 = alloca %struct.I8x16
  %t2997 = alloca %struct.I8x16
  %t3002 = alloca %struct.I8x16
  %t3006 = alloca %struct.I8x16
  %t3010 = alloca %struct.I8x16
  %t3014 = alloca %struct.I8x16
  %t3017 = alloca i32
  %t3023 = alloca i32
  %t3048 = alloca i32
  %t2948 = load ptr, ptr %t2947
  %t2949 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t2948)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2950)
  store i64 %t2949, ptr %t2950
  %t2951 = load ptr, ptr %t2947
  %t2953 = ptrtoint ptr %t2951 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2954)
  store i64 %t2953, ptr %t2954
  %t2955 = trunc i32 32 to i8
  %t2956 = call %struct.I8x16 @tml_N4core4simd5i8x165I8x165splatE(i8 %t2955)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2957)
  store %struct.I8x16 %t2956, ptr %t2957
  %t2958 = trunc i32 9 to i8
  %t2959 = call %struct.I8x16 @tml_N4core4simd5i8x165I8x165splatE(i8 %t2958)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2960)
  store %struct.I8x16 %t2959, ptr %t2960
  %t2961 = trunc i32 10 to i8
  %t2962 = call %struct.I8x16 @tml_N4core4simd5i8x165I8x165splatE(i8 %t2961)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2963)
  store %struct.I8x16 %t2962, ptr %t2963
  %t2964 = trunc i32 13 to i8
  %t2965 = call %struct.I8x16 @tml_N4core4simd5i8x165I8x165splatE(i8 %t2964)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2966)
  store %struct.I8x16 %t2965, ptr %t2966
  call void @llvm.lifetime.start.p0(i64 8, ptr %t2967)
  store i64 0, ptr %t2967
  br label %loop.preheader868
loop.preheader868:
  br label %loop.header869
loop.header869:
  %t2968 = load i64, ptr %t2967
  %t2970 = sext i32 16 to i64
  %t2971 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2968, i64 %t2970)
  %t2969 = extractvalue { i64, i1 } %t2971, 0
  %t2972 = extractvalue { i64, i1 } %t2971, 1
  br i1 %t2972, label %add_overflow874, label %add_ok873
add_overflow874:
  call void @panic(ptr @.str.189)
  unreachable
add_ok873:
  %t2973 = load i64, ptr %t2950
  %t2974 = icmp sle i64 %t2969, %t2973
  br i1 %t2974, label %loop.body870, label %loop.exit872
loop.body870:
  %t2975 = load i64, ptr %t2954
  %t2976 = load i64, ptr %t2967
  %t2978 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t2975, i64 %t2976)
  %t2977 = extractvalue { i64, i1 } %t2978, 0
  %t2979 = extractvalue { i64, i1 } %t2978, 1
  br i1 %t2979, label %add_overflow876, label %add_ok875
add_overflow876:
  call void @panic(ptr @.str.190)
  unreachable
add_ok875:
  %t2980 = inttoptr i64 %t2977 to ptr
  %t2981 = load <16 x i8>, ptr %t2980, align 1
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2982)
  store %struct.I8x16 %t2981, ptr %t2982
  %t2983 = load %struct.I8x16, ptr %t2982
  %t2984 = load %struct.I8x16, ptr %t2957
  %t2985 = icmp eq %struct.I8x16 %t2983, %t2984
  %t2986 = sext <16 x i1> %t2985 to <16 x i8>
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2987)
  store %struct.I8x16 %t2986, ptr %t2987
  %t2988 = load %struct.I8x16, ptr %t2982
  %t2989 = load %struct.I8x16, ptr %t2960
  %t2990 = icmp eq %struct.I8x16 %t2988, %t2989
  %t2991 = sext <16 x i1> %t2990 to <16 x i8>
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2992)
  store %struct.I8x16 %t2991, ptr %t2992
  %t2993 = load %struct.I8x16, ptr %t2982
  %t2994 = load %struct.I8x16, ptr %t2963
  %t2995 = icmp eq %struct.I8x16 %t2993, %t2994
  %t2996 = sext <16 x i1> %t2995 to <16 x i8>
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t2997)
  store %struct.I8x16 %t2996, ptr %t2997
  %t2998 = load %struct.I8x16, ptr %t2982
  %t2999 = load %struct.I8x16, ptr %t2966
  %t3000 = icmp eq %struct.I8x16 %t2998, %t2999
  %t3001 = sext <16 x i1> %t3000 to <16 x i8>
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3002)
  store %struct.I8x16 %t3001, ptr %t3002
  %t3003 = load %struct.I8x16, ptr %t2987
  %t3004 = load %struct.I8x16, ptr %t2992
  %t3005 = call %struct.I8x16 @tml_N4core4simd5i8x165I8x163borE(ptr %t2987, %struct.I8x16 %t3004)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3006)
  store %struct.I8x16 %t3005, ptr %t3006
  %t3007 = load %struct.I8x16, ptr %t2997
  %t3008 = load %struct.I8x16, ptr %t3002
  %t3009 = call %struct.I8x16 @tml_N4core4simd5i8x165I8x163borE(ptr %t2997, %struct.I8x16 %t3008)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3010)
  store %struct.I8x16 %t3009, ptr %t3010
  %t3011 = load %struct.I8x16, ptr %t3006
  %t3012 = load %struct.I8x16, ptr %t3010
  %t3013 = call %struct.I8x16 @tml_N4core4simd5i8x165I8x163borE(ptr %t3006, %struct.I8x16 %t3012)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3014)
  store %struct.I8x16 %t3013, ptr %t3014
  %t3015 = load %struct.I8x16, ptr %t3014
  %t3016 = call i32 @llvm.x86.sse2.pmovmskb.128(<16 x i8> %t3015)
  call void @llvm.lifetime.start.p0(i64 4, ptr %t3017)
  store i32 %t3016, ptr %t3017
  %t3018 = load i32, ptr %t3017
  %t3019 = icmp ne i32 %t3018, 65535
  br i1 %t3019, label %if.then877, label %if.end879
if.then877:
  %t3020 = load i32, ptr %t3017
  %t3021 = xor i32 %t3020, 65535
  %t3022 = and i32 %t3021, 65535
  call void @llvm.lifetime.start.p0(i64 4, ptr %t3023)
  store i32 %t3022, ptr %t3023
  %t3024 = load i64, ptr %t2967
  %t3025 = load i32, ptr %t3023
  %t3026 = call i32 @tml_N4core3str4simd3ctzE_i(i32 %t3025)
  %t3027 = sext i32 %t3026 to i64
  %t3029 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3024, i64 %t3027)
  %t3028 = extractvalue { i64, i1 } %t3029, 0
  %t3030 = extractvalue { i64, i1 } %t3029, 1
  br i1 %t3030, label %add_overflow881, label %add_ok880
add_overflow881:
  call void @panic(ptr @.str.191)
  unreachable
add_ok880:
  call void @llvm.lifetime.end.p0(i64 4, ptr %t3023)
  call void @llvm.lifetime.end.p0(i64 4, ptr %t3017)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t3014)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t3010)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t3006)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t3002)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t2997)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t2992)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t2987)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t2982)
  ret i64 %t3028
if.end879:
  %t3031 = load i64, ptr %t2967
  %t3033 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3031, i64 16)
  %t3032 = extractvalue { i64, i1 } %t3033, 0
  %t3034 = extractvalue { i64, i1 } %t3033, 1
  br i1 %t3034, label %add_overflow883, label %add_ok882
add_overflow883:
  call void @panic(ptr @.str.192)
  unreachable
add_ok882:
  store i64 %t3032, ptr %t2967
  call void @llvm.lifetime.end.p0(i64 4, ptr %t3017)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t3014)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t3010)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t3006)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t3002)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t2997)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t2992)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t2987)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t2982)
  br label %loop.latch871
loop.latch871:
  br label %loop.header869, !llvm.loop !1036
loop.exit872:
  br label %loop.preheader884
loop.preheader884:
  br label %loop.header885
loop.header885:
  %t3035 = load i64, ptr %t2967
  %t3036 = load i64, ptr %t2950
  %t3037 = icmp slt i64 %t3035, %t3036
  br i1 %t3037, label %loop.body886, label %loop.exit888
loop.body886:
  %t3038 = load i64, ptr %t2954
  %t3039 = load i64, ptr %t2967
  %t3041 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3038, i64 %t3039)
  %t3040 = extractvalue { i64, i1 } %t3041, 0
  %t3042 = extractvalue { i64, i1 } %t3041, 1
  br i1 %t3042, label %add_overflow890, label %add_ok889
add_overflow890:
  call void @panic(ptr @.str.193)
  unreachable
add_ok889:
  %t3043 = inttoptr i64 %t3040 to ptr
  %t3044 = alloca ptr
  store ptr %t3043, ptr %t3044
  %t3045 = load ptr, ptr %t3044
  %t3046 = load i8, ptr %t3045
  %t3047 = sext i8 %t3046 to i32
  call void @llvm.lifetime.start.p0(i64 4, ptr %t3048)
  store i32 %t3047, ptr %t3048
  %t3049 = load i32, ptr %t3048
  %t3050 = icmp ne i32 %t3049, 32
  %t3051 = load i32, ptr %t3048
  %t3052 = icmp ne i32 %t3051, 9
  %t3053 = and i1 %t3050, %t3052
  %t3054 = load i32, ptr %t3048
  %t3055 = icmp ne i32 %t3054, 10
  %t3056 = and i1 %t3053, %t3055
  %t3057 = load i32, ptr %t3048
  %t3058 = icmp ne i32 %t3057, 13
  %t3059 = and i1 %t3056, %t3058
  br i1 %t3059, label %if.then891, label %if.end893
if.then891:
  %t3060 = load i64, ptr %t2967
  call void @llvm.lifetime.end.p0(i64 4, ptr %t3048)
  ret i64 %t3060
if.end893:
  %t3061 = load i64, ptr %t2967
  %t3063 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3061, i64 1)
  %t3062 = extractvalue { i64, i1 } %t3063, 0
  %t3064 = extractvalue { i64, i1 } %t3063, 1
  br i1 %t3064, label %add_overflow895, label %add_ok894
add_overflow895:
  call void @panic(ptr @.str.194)
  unreachable
add_ok894:
  store i64 %t3062, ptr %t2967
  call void @llvm.lifetime.end.p0(i64 4, ptr %t3048)
  br label %loop.latch887
loop.latch887:
  br label %loop.header885, !llvm.loop !1037
loop.exit888:
  %t3065 = load i64, ptr %t2950
  ret i64 %t3065
}

; core::str::search::find
define %struct.Maybe__I64 @tml_N4core3str6search4findE_SS(ptr %s, ptr %pattern) #0 {
entry:
  %t3066 = alloca ptr
  store ptr %s, ptr %t3066
  %t3067 = alloca ptr
  store ptr %pattern, ptr %t3067
  %t3070 = alloca i64
  %t3073 = alloca i64
  %t3097 = alloca i64
  %t3105 = alloca i64
  %t3106 = alloca i64
  %t3068 = load ptr, ptr %t3066
  %t3069 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t3068)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3070)
  store i64 %t3069, ptr %t3070
  %t3071 = load ptr, ptr %t3067
  %t3072 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t3071)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3073)
  store i64 %t3072, ptr %t3073
  %t3074 = load i64, ptr %t3073
  %t3076 = sext i32 0 to i64
  %t3075 = icmp eq i64 %t3074, %t3076
  br i1 %t3075, label %if.then896, label %if.end898
if.then896:
  %t3078 = alloca %struct.Maybe__I64, align 8
  %t3079 = getelementptr inbounds %struct.Maybe__I64, ptr %t3078, i32 0, i32 0
  store i32 0, ptr %t3079
  %t3080 = getelementptr inbounds %struct.Maybe__I64, ptr %t3078, i32 0, i32 1
  %t3081 = bitcast ptr %t3080 to ptr
  store i32 0, ptr %t3081
  %t3077 = load %struct.Maybe__I64, ptr %t3078
  ret %struct.Maybe__I64 %t3077
if.end898:
  %t3082 = load i64, ptr %t3073
  %t3083 = load i64, ptr %t3070
  %t3084 = icmp sgt i64 %t3082, %t3083
  br i1 %t3084, label %if.then899, label %if.end901
if.then899:
  %t3086 = alloca %struct.Maybe__I64, align 8
  %t3087 = getelementptr inbounds %struct.Maybe__I64, ptr %t3086, i32 0, i32 0
  store i32 1, ptr %t3087
  %t3085 = load %struct.Maybe__I64, ptr %t3086
  ret %struct.Maybe__I64 %t3085
if.end901:
  %t3088 = load i64, ptr %t3070
  %t3090 = sext i32 32 to i64
  %t3089 = icmp sge i64 %t3088, %t3090
  br i1 %t3089, label %if.then902, label %if.end904
if.then902:
  %t3091 = load ptr, ptr %t3066
  %t3092 = load ptr, ptr %t3067
  %t3093 = call %struct.Maybe__I64 @tml_N4core3str4simd9find_simdE_SS(ptr %t3091, ptr %t3092)
  ret %struct.Maybe__I64 %t3093
if.end904:
  %t3094 = load ptr, ptr %t3066
  %t3096 = ptrtoint ptr %t3094 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3097)
  store i64 %t3096, ptr %t3097
  %t3098 = load ptr, ptr %t3067
  %t3099 = alloca ptr
  store ptr %t3098, ptr %t3099
  %t3100 = load i64, ptr %t3070
  %t3101 = load i64, ptr %t3073
  %t3103 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t3100, i64 %t3101)
  %t3102 = extractvalue { i64, i1 } %t3103, 0
  %t3104 = extractvalue { i64, i1 } %t3103, 1
  br i1 %t3104, label %sub_overflow906, label %sub_ok905
sub_overflow906:
  call void @panic(ptr @.str.195)
  unreachable
sub_ok905:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3105)
  store i64 %t3102, ptr %t3105
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3106)
  store i64 0, ptr %t3106
  br label %loop.preheader907
loop.preheader907:
  br label %loop.header908
loop.header908:
  %t3107 = load i64, ptr %t3106
  %t3108 = load i64, ptr %t3105
  %t3109 = icmp sle i64 %t3107, %t3108
  br i1 %t3109, label %loop.body909, label %loop.exit911
loop.body909:
  %t3110 = load i64, ptr %t3097
  %t3111 = load i64, ptr %t3106
  %t3113 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3110, i64 %t3111)
  %t3112 = extractvalue { i64, i1 } %t3113, 0
  %t3114 = extractvalue { i64, i1 } %t3113, 1
  br i1 %t3114, label %add_overflow913, label %add_ok912
add_overflow913:
  call void @panic(ptr @.str.151)
  unreachable
add_ok912:
  %t3115 = inttoptr i64 %t3112 to ptr
  %t3116 = load ptr, ptr %t3099
  %t3117 = load i64, ptr %t3073
  %t3118 = call i32 @memcmp(ptr %t3115, ptr %t3116, i64 %t3117)
  %t3119 = icmp eq i32 %t3118, 0
  br i1 %t3119, label %if.then914, label %if.end916
if.then914:
  %t3121 = alloca %struct.Maybe__I64, align 8
  %t3122 = getelementptr inbounds %struct.Maybe__I64, ptr %t3121, i32 0, i32 0
  store i32 0, ptr %t3122
  %t3123 = load i64, ptr %t3106
  %t3124 = getelementptr inbounds %struct.Maybe__I64, ptr %t3121, i32 0, i32 1
  %t3125 = bitcast ptr %t3124 to ptr
  store i64 %t3123, ptr %t3125
  %t3120 = load %struct.Maybe__I64, ptr %t3121
  ret %struct.Maybe__I64 %t3120
if.end916:
  %t3126 = load i64, ptr %t3106
  %t3128 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3126, i64 1)
  %t3127 = extractvalue { i64, i1 } %t3128, 0
  %t3129 = extractvalue { i64, i1 } %t3128, 1
  br i1 %t3129, label %add_overflow918, label %add_ok917
add_overflow918:
  call void @panic(ptr @.str.196)
  unreachable
add_ok917:
  store i64 %t3127, ptr %t3106
  br label %loop.latch910
loop.latch910:
  br label %loop.header908, !llvm.loop !1038
loop.exit911:
  %t3131 = alloca %struct.Maybe__I64, align 8
  %t3132 = getelementptr inbounds %struct.Maybe__I64, ptr %t3131, i32 0, i32 0
  store i32 1, ptr %t3132
  %t3130 = load %struct.Maybe__I64, ptr %t3131
  ret %struct.Maybe__I64 %t3130
}

; ir_diff::parser::parse_params
define internal %struct.List__IrParam @tml_N7ir_diff6parser12parse_paramsE_S(ptr %inside) #0 {
entry:
  %t3133 = alloca ptr
  store ptr %inside, ptr %t3133
  %t3135 = alloca %struct.List__IrParam
  %t3138 = alloca %struct.List__Str
  %t3139 = alloca i64
  %t3159 = alloca %struct.Maybe__I64
  %t3168 = alloca i64
  %t3134 = call %struct.List__IrParam @tml_N3std11collections4list13List__IrParam3newE(i64 2)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3135)
  store %struct.List__IrParam %t3134, ptr %t3135
  %t3136 = load ptr, ptr %t3133
  %t3137 = call %struct.List__Str @tml_N7ir_diff6parser14split_operandsE_S(ptr %t3136)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3138)
  store %struct.List__Str %t3137, ptr %t3138
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3139)
  store i64 0, ptr %t3139
  br label %loop.preheader919
loop.preheader919:
  br label %loop.header920
loop.header920:
  %t3140 = load i64, ptr %t3139
  %t3141 = load %struct.List__Str, ptr %t3138
  %t3142 = call i64 @tml_N3std11collections4list9List__Str3lenE(ptr %t3138)
  %t3143 = icmp slt i64 %t3140, %t3142
  br i1 %t3143, label %loop.body921, label %loop.exit923
loop.body921:
  %t3144 = load %struct.List__Str, ptr %t3138
  %t3145 = load i64, ptr %t3139
  %t3146 = call ptr @tml_N3std11collections4list9List__Str3getE(ptr %t3138, i64 %t3145)
  %t3147 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t3146)
  %t3148 = alloca ptr
  store ptr %t3147, ptr %t3148
  %t3149 = load ptr, ptr %t3148
  %t3150 = call i64 @strlen(ptr %t3149)
  %t3152 = sext i32 0 to i64
  %t3151 = icmp eq i64 %t3150, %t3152
  br i1 %t3151, label %if.then924, label %if.end926
if.then924:
  %t3153 = load i64, ptr %t3139
  %t3155 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3153, i64 1)
  %t3154 = extractvalue { i64, i1 } %t3155, 0
  %t3156 = extractvalue { i64, i1 } %t3155, 1
  br i1 %t3156, label %add_overflow928, label %add_ok927
add_overflow928:
  call void @panic(ptr @.str.197)
  unreachable
add_ok927:
  store i64 %t3154, ptr %t3139
  br label %loop.latch922
if.end926:
  %t3157 = load ptr, ptr %t3148
  %t3158 = call %struct.Maybe__I64 @tml_N4core3str6search4findE_SS(ptr %t3157, ptr @.str.198)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3159)
  store %struct.Maybe__I64 %t3158, ptr %t3159
  %t3160 = load %struct.Maybe__I64, ptr %t3159
  %t3161 = extractvalue %struct.Maybe__I64 %t3160, 0
  %t3162 = icmp eq i32 %t3161, 0
  br i1 %t3162, label %if.then929, label %if.else930
if.then929:
  %t3163 = load %struct.Maybe__I64, ptr %t3159
  %t3164 = extractvalue %struct.Maybe__I64 %t3163, 0
  %t3165 = alloca %struct.Maybe__I64
  store %struct.Maybe__I64 %t3163, ptr %t3165
  %t3166 = getelementptr inbounds %struct.Maybe__I64, ptr %t3165, i32 0, i32 1
  %t3167 = load i64, ptr %t3166
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3168)
  store i64 %t3167, ptr %t3168
  %t3169 = load ptr, ptr %t3148
  %t3170 = sext i32 0 to i64
  %t3171 = load i64, ptr %t3168
  %t3172 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t3169, i64 %t3170, i64 %t3171)
  %t3173 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t3172)
  %t3174 = alloca ptr
  store ptr %t3173, ptr %t3174
  %t3175 = load ptr, ptr %t3148
  %t3176 = load i64, ptr %t3168
  %t3178 = sext i32 1 to i64
  %t3179 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3176, i64 %t3178)
  %t3177 = extractvalue { i64, i1 } %t3179, 0
  %t3180 = extractvalue { i64, i1 } %t3179, 1
  br i1 %t3180, label %add_overflow933, label %add_ok932
add_overflow933:
  call void @panic(ptr @.str.199)
  unreachable
add_ok932:
  %t3181 = call ptr @tml_N4core3str5basic14substring_fromE_Sl(ptr %t3175, i64 %t3177)
  %t3182 = call ptr @tml_N4core3str9transform4trimE_S(ptr %t3181)
  %t3183 = alloca ptr
  store ptr %t3182, ptr %t3183
  %t3184 = load %struct.List__IrParam, ptr %t3135
  %t3185 = load ptr, ptr %t3174
  %t3186 = insertvalue %struct.IrParam undef, ptr %t3185, 0
  %t3187 = load ptr, ptr %t3183
  %t3188 = insertvalue %struct.IrParam %t3186, ptr %t3187, 1
  %t3189 = call {} @tml_N3std11collections4list13List__IrParam13push__IrParamE(ptr %t3135, %struct.IrParam %t3188)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t3168)
  br label %if.end931
if.else930:
  %t3190 = load %struct.List__IrParam, ptr %t3135
  %t3191 = load ptr, ptr %t3148
  %t3192 = insertvalue %struct.IrParam undef, ptr %t3191, 0
  %t3193 = insertvalue %struct.IrParam %t3192, ptr @.str.1, 1
  %t3194 = call {} @tml_N3std11collections4list13List__IrParam13push__IrParamE(ptr %t3135, %struct.IrParam %t3193)
  br label %if.end931
if.end931:
  %t3195 = phi {} [ %t3189, %add_ok932 ], [ %t3194, %if.else930 ]
  %t3196 = load i64, ptr %t3139
  %t3198 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3196, i64 1)
  %t3197 = extractvalue { i64, i1 } %t3198, 0
  %t3199 = extractvalue { i64, i1 } %t3198, 1
  br i1 %t3199, label %add_overflow935, label %add_ok934
add_overflow935:
  call void @panic(ptr @.str.200)
  unreachable
add_ok934:
  store i64 %t3197, ptr %t3139
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t3159)
  br label %loop.latch922
loop.latch922:
  br label %loop.header920, !llvm.loop !1039
loop.exit923:
  %t3200 = load %struct.List__IrParam, ptr %t3135
  %t3201 = load %struct.List__Str, ptr %t3138
  call void @tml_N3std11collections4list9List__Str4dropE(ptr %t3138)
  ret %struct.List__IrParam %t3200
}

; core::str::search::ends_with
define i1 @tml_N4core3str6search9ends_withE_SS(ptr %s, ptr %suffix) #0 {
entry:
  %t3202 = alloca ptr
  store ptr %s, ptr %t3202
  %t3203 = alloca ptr
  store ptr %suffix, ptr %t3203
  %t3206 = alloca i64
  %t3209 = alloca i64
  %t3221 = alloca i64
  %t3204 = load ptr, ptr %t3202
  %t3205 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t3204)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3206)
  store i64 %t3205, ptr %t3206
  %t3207 = load ptr, ptr %t3203
  %t3208 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t3207)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3209)
  store i64 %t3208, ptr %t3209
  %t3210 = load i64, ptr %t3209
  %t3211 = load i64, ptr %t3206
  %t3212 = icmp sgt i64 %t3210, %t3211
  br i1 %t3212, label %if.then936, label %if.end938
if.then936:
  ret i1 0
if.end938:
  %t3213 = load i64, ptr %t3209
  %t3215 = sext i32 0 to i64
  %t3214 = icmp eq i64 %t3213, %t3215
  br i1 %t3214, label %if.then939, label %if.end941
if.then939:
  ret i1 1
if.end941:
  %t3216 = load i64, ptr %t3206
  %t3217 = load i64, ptr %t3209
  %t3219 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t3216, i64 %t3217)
  %t3218 = extractvalue { i64, i1 } %t3219, 0
  %t3220 = extractvalue { i64, i1 } %t3219, 1
  br i1 %t3220, label %sub_overflow943, label %sub_ok942
sub_overflow943:
  call void @panic(ptr @.str.201)
  unreachable
sub_ok942:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3221)
  store i64 %t3218, ptr %t3221
  %t3222 = load ptr, ptr %t3202
  %t3224 = ptrtoint ptr %t3222 to i64
  %t3225 = load i64, ptr %t3221
  %t3227 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3224, i64 %t3225)
  %t3226 = extractvalue { i64, i1 } %t3227, 0
  %t3228 = extractvalue { i64, i1 } %t3227, 1
  br i1 %t3228, label %add_overflow945, label %add_ok944
add_overflow945:
  call void @panic(ptr @.str.202)
  unreachable
add_ok944:
  %t3229 = inttoptr i64 %t3226 to ptr
  %t3230 = load ptr, ptr %t3203
  %t3231 = load i64, ptr %t3209
  %t3232 = call i32 @memcmp(ptr %t3229, ptr %t3230, i64 %t3231)
  %t3233 = icmp eq i32 %t3232, 0
  ret i1 %t3233
}

; std::collections::buffer::buf_get_data
define ptr @tml_N3std11collections6buffer12buf_get_dataE_l(i64 %h) #0 {
entry:
  %t3234 = alloca i64
  store i64 %h, ptr %t3234
  %t3240 = alloca i64
  %t3235 = load i64, ptr %t3234
  %t3236 = inttoptr i64 %t3235 to ptr
  %t3237 = alloca ptr
  store ptr %t3236, ptr %t3237
  %t3238 = load ptr, ptr %t3237
  %t3239 = load i64, ptr %t3238
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3240)
  store i64 %t3239, ptr %t3240
  %t3241 = load i64, ptr %t3240
  %t3242 = inttoptr i64 %t3241 to ptr
  ret ptr %t3242
}

; core::str::simd::trim_end_simd
define i64 @tml_N4core3str4simd13trim_end_simdE_S(ptr %s) #0 {
entry:
  %t3243 = alloca ptr
  store ptr %s, ptr %t3243
  %t3246 = alloca i64
  %t3250 = alloca i64
  %t3252 = alloca i64
  %t3270 = alloca i32
  %t3244 = load ptr, ptr %t3243
  %t3245 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t3244)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3246)
  store i64 %t3245, ptr %t3246
  %t3247 = load ptr, ptr %t3243
  %t3249 = ptrtoint ptr %t3247 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3250)
  store i64 %t3249, ptr %t3250
  %t3251 = load i64, ptr %t3246
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3252)
  store i64 %t3251, ptr %t3252
  br label %loop.preheader946
loop.preheader946:
  br label %loop.header947
loop.header947:
  %t3253 = load i64, ptr %t3252
  %t3255 = sext i32 0 to i64
  %t3254 = icmp sgt i64 %t3253, %t3255
  br i1 %t3254, label %loop.body948, label %loop.exit950
loop.body948:
  %t3256 = load i64, ptr %t3250
  %t3257 = load i64, ptr %t3252
  %t3259 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3256, i64 %t3257)
  %t3258 = extractvalue { i64, i1 } %t3259, 0
  %t3260 = extractvalue { i64, i1 } %t3259, 1
  br i1 %t3260, label %add_overflow952, label %add_ok951
add_overflow952:
  call void @panic(ptr @.str.203)
  unreachable
add_ok951:
  %t3262 = sext i32 1 to i64
  %t3263 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t3258, i64 %t3262)
  %t3261 = extractvalue { i64, i1 } %t3263, 0
  %t3264 = extractvalue { i64, i1 } %t3263, 1
  br i1 %t3264, label %sub_overflow954, label %sub_ok953
sub_overflow954:
  call void @panic(ptr @.str.204)
  unreachable
sub_ok953:
  %t3265 = inttoptr i64 %t3261 to ptr
  %t3266 = alloca ptr
  store ptr %t3265, ptr %t3266
  %t3267 = load ptr, ptr %t3266
  %t3268 = load i8, ptr %t3267
  %t3269 = sext i8 %t3268 to i32
  call void @llvm.lifetime.start.p0(i64 4, ptr %t3270)
  store i32 %t3269, ptr %t3270
  %t3271 = load i32, ptr %t3270
  %t3272 = icmp ne i32 %t3271, 32
  %t3273 = load i32, ptr %t3270
  %t3274 = icmp ne i32 %t3273, 9
  %t3275 = and i1 %t3272, %t3274
  %t3276 = load i32, ptr %t3270
  %t3277 = icmp ne i32 %t3276, 10
  %t3278 = and i1 %t3275, %t3277
  %t3279 = load i32, ptr %t3270
  %t3280 = icmp ne i32 %t3279, 13
  %t3281 = and i1 %t3278, %t3280
  br i1 %t3281, label %if.then955, label %if.end957
if.then955:
  %t3282 = load i64, ptr %t3252
  call void @llvm.lifetime.end.p0(i64 4, ptr %t3270)
  ret i64 %t3282
if.end957:
  %t3283 = load i64, ptr %t3252
  %t3285 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t3283, i64 1)
  %t3284 = extractvalue { i64, i1 } %t3285, 0
  %t3286 = extractvalue { i64, i1 } %t3285, 1
  br i1 %t3286, label %sub_overflow959, label %sub_ok958
sub_overflow959:
  call void @panic(ptr @.str.205)
  unreachable
sub_ok958:
  store i64 %t3284, ptr %t3252
  call void @llvm.lifetime.end.p0(i64 4, ptr %t3270)
  br label %loop.latch949
loop.latch949:
  br label %loop.header947, !llvm.loop !1040
loop.exit950:
  %t3287 = sext i32 0 to i64
  ret i64 %t3287
}

; core::str::transform::is_whitespace
define internal i1 @tml_N4core3str9transform13is_whitespaceE_i(i32 %b) #0 {
entry:
  %t3288 = alloca i32
  store i32 %b, ptr %t3288
  %t3289 = load i32, ptr %t3288
  %t3290 = icmp eq i32 %t3289, 32
  %t3291 = load i32, ptr %t3288
  %t3292 = icmp eq i32 %t3291, 9
  %t3293 = or i1 %t3290, %t3292
  %t3294 = load i32, ptr %t3288
  %t3295 = icmp eq i32 %t3294, 10
  %t3296 = or i1 %t3293, %t3295
  %t3297 = load i32, ptr %t3288
  %t3298 = icmp eq i32 %t3297, 13
  %t3299 = or i1 %t3296, %t3298
  ret i1 %t3299
}

; core::str::split::split
define %struct.List__Str @tml_N4core3str5split5splitE_SS(ptr %s, ptr %delimiter) #0 {
entry:
  %t3300 = alloca ptr
  store ptr %s, ptr %t3300
  %t3301 = alloca ptr
  store ptr %delimiter, ptr %t3301
  %t3303 = alloca %struct.List__Str
  %t3306 = alloca i64
  %t3309 = alloca i64
  %t3332 = alloca i8
  %t3339 = alloca i64
  %t3343 = alloca i64
  %t3344 = alloca i64
  %t3345 = alloca i64
  %t3302 = call %struct.List__Str @tml_N3std11collections4list9List__Str3newE(i64 8)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3303)
  store %struct.List__Str %t3302, ptr %t3303
  %t3304 = load ptr, ptr %t3300
  %t3305 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t3304)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3306)
  store i64 %t3305, ptr %t3306
  %t3307 = load ptr, ptr %t3301
  %t3308 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t3307)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3309)
  store i64 %t3308, ptr %t3309
  %t3310 = load i64, ptr %t3306
  %t3312 = sext i32 0 to i64
  %t3311 = icmp eq i64 %t3310, %t3312
  br i1 %t3311, label %if.then960, label %if.end962
if.then960:
  %t3313 = load %struct.List__Str, ptr %t3303
  %t3314 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t3303, ptr @.str.1)
  %t3315 = load %struct.List__Str, ptr %t3303
  ret %struct.List__Str %t3315
if.end962:
  %t3316 = load i64, ptr %t3309
  %t3318 = sext i32 0 to i64
  %t3317 = icmp eq i64 %t3316, %t3318
  br i1 %t3317, label %if.then963, label %if.end965
if.then963:
  %t3319 = load %struct.List__Str, ptr %t3303
  %t3320 = load ptr, ptr %t3300
  %t3321 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t3303, ptr %t3320)
  %t3322 = load %struct.List__Str, ptr %t3303
  ret %struct.List__Str %t3322
if.end965:
  %t3323 = load i64, ptr %t3309
  %t3325 = sext i32 1 to i64
  %t3324 = icmp eq i64 %t3323, %t3325
  %t3326 = load i64, ptr %t3306
  %t3328 = sext i32 32 to i64
  %t3327 = icmp sge i64 %t3326, %t3328
  %t3329 = and i1 %t3324, %t3327
  br i1 %t3329, label %if.then966, label %if.end968
if.then966:
  %t3330 = load ptr, ptr %t3301
  %t3331 = load i8, ptr %t3330
  call void @llvm.lifetime.start.p0(i64 1, ptr %t3332)
  store i8 %t3331, ptr %t3332
  %t3333 = load ptr, ptr %t3300
  %t3334 = load i8, ptr %t3332
  %t3335 = call %struct.List__Str @tml_N4core3str4simd18split_by_byte_simdE_Sh(ptr %t3333, i8 %t3334)
  call void @llvm.lifetime.end.p0(i64 1, ptr %t3332)
  ret %struct.List__Str %t3335
if.end968:
  %t3336 = load ptr, ptr %t3300
  %t3338 = ptrtoint ptr %t3336 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3339)
  store i64 %t3338, ptr %t3339
  %t3340 = load ptr, ptr %t3301
  %t3342 = ptrtoint ptr %t3340 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3343)
  store i64 %t3342, ptr %t3343
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3344)
  store i64 0, ptr %t3344
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3345)
  store i64 0, ptr %t3345
  br label %loop.preheader969
loop.preheader969:
  br label %loop.header970
loop.header970:
  %t3346 = load i64, ptr %t3345
  %t3347 = load i64, ptr %t3306
  %t3348 = load i64, ptr %t3309
  %t3350 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t3347, i64 %t3348)
  %t3349 = extractvalue { i64, i1 } %t3350, 0
  %t3351 = extractvalue { i64, i1 } %t3350, 1
  br i1 %t3351, label %sub_overflow975, label %sub_ok974
sub_overflow975:
  call void @panic(ptr @.str.206)
  unreachable
sub_ok974:
  %t3352 = icmp sle i64 %t3346, %t3349
  br i1 %t3352, label %loop.body971, label %loop.exit973
loop.body971:
  %t3353 = load i64, ptr %t3339
  %t3354 = load i64, ptr %t3345
  %t3356 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3353, i64 %t3354)
  %t3355 = extractvalue { i64, i1 } %t3356, 0
  %t3357 = extractvalue { i64, i1 } %t3356, 1
  br i1 %t3357, label %add_overflow977, label %add_ok976
add_overflow977:
  call void @panic(ptr @.str.207)
  unreachable
add_ok976:
  %t3358 = inttoptr i64 %t3355 to ptr
  %t3359 = load i64, ptr %t3343
  %t3360 = inttoptr i64 %t3359 to ptr
  %t3361 = load i64, ptr %t3309
  %t3362 = call i32 @memcmp(ptr %t3358, ptr %t3360, i64 %t3361)
  %t3363 = icmp eq i32 %t3362, 0
  br i1 %t3363, label %if.then978, label %if.else979
if.then978:
  %t3364 = load %struct.List__Str, ptr %t3303
  %t3365 = load ptr, ptr %t3300
  %t3366 = load i64, ptr %t3344
  %t3367 = load i64, ptr %t3345
  %t3368 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t3365, i64 %t3366, i64 %t3367)
  %t3369 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t3303, ptr %t3368)
  %t3370 = load i64, ptr %t3345
  %t3371 = load i64, ptr %t3309
  %t3373 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3370, i64 %t3371)
  %t3372 = extractvalue { i64, i1 } %t3373, 0
  %t3374 = extractvalue { i64, i1 } %t3373, 1
  br i1 %t3374, label %add_overflow982, label %add_ok981
add_overflow982:
  call void @panic(ptr @.str.208)
  unreachable
add_ok981:
  store i64 %t3372, ptr %t3344
  %t3375 = load i64, ptr %t3344
  store i64 %t3375, ptr %t3345
  br label %if.end980
if.else979:
  %t3376 = load i64, ptr %t3345
  %t3378 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3376, i64 1)
  %t3377 = extractvalue { i64, i1 } %t3378, 0
  %t3379 = extractvalue { i64, i1 } %t3378, 1
  br i1 %t3379, label %add_overflow984, label %add_ok983
add_overflow984:
  call void @panic(ptr @.str.108)
  unreachable
add_ok983:
  store i64 %t3377, ptr %t3345
  br label %if.end980
if.end980:
  %t3380 = phi i64 [ %t3375, %add_ok981 ], [ %t3377, %add_ok983 ]
  br label %loop.latch972
loop.latch972:
  br label %loop.header970, !llvm.loop !1041
loop.exit973:
  %t3381 = load %struct.List__Str, ptr %t3303
  %t3382 = load ptr, ptr %t3300
  %t3383 = load i64, ptr %t3344
  %t3384 = load i64, ptr %t3306
  %t3385 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t3382, i64 %t3383, i64 %t3384)
  %t3386 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t3303, ptr %t3385)
  %t3387 = load %struct.List__Str, ptr %t3303
  ret %struct.List__Str %t3387
}

; core::fmt::helpers::count_digits_u64
define internal i64 @tml_N4core3fmt7helpers16count_digits_u64E_m(i64 %n) #0 {
entry:
  %t3388 = alloca i64
  store i64 %n, ptr %t3388
  %t3389 = load i64, ptr %t3388
  %t3391 = sext i32 10 to i64
  %t3390 = icmp ult i64 %t3389, %t3391
  br i1 %t3390, label %if.then985, label %if.end987
if.then985:
  %t3392 = sext i32 1 to i64
  ret i64 %t3392
if.end987:
  %t3393 = load i64, ptr %t3388
  %t3395 = sext i32 100 to i64
  %t3394 = icmp ult i64 %t3393, %t3395
  br i1 %t3394, label %if.then988, label %if.end990
if.then988:
  %t3396 = sext i32 2 to i64
  ret i64 %t3396
if.end990:
  %t3397 = load i64, ptr %t3388
  %t3399 = sext i32 1000 to i64
  %t3398 = icmp ult i64 %t3397, %t3399
  br i1 %t3398, label %if.then991, label %if.end993
if.then991:
  %t3400 = sext i32 3 to i64
  ret i64 %t3400
if.end993:
  %t3401 = load i64, ptr %t3388
  %t3403 = sext i32 10000 to i64
  %t3402 = icmp ult i64 %t3401, %t3403
  br i1 %t3402, label %if.then994, label %if.end996
if.then994:
  %t3404 = sext i32 4 to i64
  ret i64 %t3404
if.end996:
  %t3405 = load i64, ptr %t3388
  %t3407 = sext i32 100000 to i64
  %t3406 = icmp ult i64 %t3405, %t3407
  br i1 %t3406, label %if.then997, label %if.end999
if.then997:
  %t3408 = sext i32 5 to i64
  ret i64 %t3408
if.end999:
  %t3409 = load i64, ptr %t3388
  %t3411 = sext i32 1000000 to i64
  %t3410 = icmp ult i64 %t3409, %t3411
  br i1 %t3410, label %if.then1000, label %if.end1002
if.then1000:
  %t3412 = sext i32 6 to i64
  ret i64 %t3412
if.end1002:
  %t3413 = load i64, ptr %t3388
  %t3415 = sext i32 10000000 to i64
  %t3414 = icmp ult i64 %t3413, %t3415
  br i1 %t3414, label %if.then1003, label %if.end1005
if.then1003:
  %t3416 = sext i32 7 to i64
  ret i64 %t3416
if.end1005:
  %t3417 = load i64, ptr %t3388
  %t3419 = sext i32 100000000 to i64
  %t3418 = icmp ult i64 %t3417, %t3419
  br i1 %t3418, label %if.then1006, label %if.end1008
if.then1006:
  %t3420 = sext i32 8 to i64
  ret i64 %t3420
if.end1008:
  %t3421 = load i64, ptr %t3388
  %t3423 = sext i32 1000000000 to i64
  %t3422 = icmp ult i64 %t3421, %t3423
  br i1 %t3422, label %if.then1009, label %if.end1011
if.then1009:
  %t3424 = sext i32 9 to i64
  ret i64 %t3424
if.end1011:
  %t3425 = load i64, ptr %t3388
  %t3426 = icmp ult i64 %t3425, 10000000000
  br i1 %t3426, label %if.then1012, label %if.end1014
if.then1012:
  %t3427 = sext i32 10 to i64
  ret i64 %t3427
if.end1014:
  %t3428 = load i64, ptr %t3388
  %t3429 = icmp ult i64 %t3428, 100000000000
  br i1 %t3429, label %if.then1015, label %if.end1017
if.then1015:
  %t3430 = sext i32 11 to i64
  ret i64 %t3430
if.end1017:
  %t3431 = load i64, ptr %t3388
  %t3432 = icmp ult i64 %t3431, 1000000000000
  br i1 %t3432, label %if.then1018, label %if.end1020
if.then1018:
  %t3433 = sext i32 12 to i64
  ret i64 %t3433
if.end1020:
  %t3434 = load i64, ptr %t3388
  %t3435 = icmp ult i64 %t3434, 10000000000000
  br i1 %t3435, label %if.then1021, label %if.end1023
if.then1021:
  %t3436 = sext i32 13 to i64
  ret i64 %t3436
if.end1023:
  %t3437 = load i64, ptr %t3388
  %t3438 = icmp ult i64 %t3437, 100000000000000
  br i1 %t3438, label %if.then1024, label %if.end1026
if.then1024:
  %t3439 = sext i32 14 to i64
  ret i64 %t3439
if.end1026:
  %t3440 = load i64, ptr %t3388
  %t3441 = icmp ult i64 %t3440, 1000000000000000
  br i1 %t3441, label %if.then1027, label %if.end1029
if.then1027:
  %t3442 = sext i32 15 to i64
  ret i64 %t3442
if.end1029:
  %t3443 = load i64, ptr %t3388
  %t3444 = icmp ult i64 %t3443, 10000000000000000
  br i1 %t3444, label %if.then1030, label %if.end1032
if.then1030:
  %t3445 = sext i32 16 to i64
  ret i64 %t3445
if.end1032:
  %t3446 = load i64, ptr %t3388
  %t3447 = icmp ult i64 %t3446, 100000000000000000
  br i1 %t3447, label %if.then1033, label %if.end1035
if.then1033:
  %t3448 = sext i32 17 to i64
  ret i64 %t3448
if.end1035:
  %t3449 = load i64, ptr %t3388
  %t3450 = icmp ult i64 %t3449, 1000000000000000000
  br i1 %t3450, label %if.then1036, label %if.end1038
if.then1036:
  %t3451 = sext i32 18 to i64
  ret i64 %t3451
if.end1038:
  %t3452 = load i64, ptr %t3388
  %t3453 = icmp ult i64 %t3452, 10000000000000000000
  br i1 %t3453, label %if.then1039, label %if.end1041
if.then1039:
  %t3454 = sext i32 19 to i64
  ret i64 %t3454
if.end1041:
  %t3455 = sext i32 20 to i64
  ret i64 %t3455
}

; core::str::simd::split_lines_simd
define %struct.List__Str @tml_N4core3str4simd16split_lines_simdE_S(ptr %s) #0 {
entry:
  %t3456 = alloca ptr
  store ptr %s, ptr %t3456
  %t3459 = alloca i64
  %t3463 = alloca i64
  %t3465 = alloca %struct.List__Str
  %t3469 = alloca %struct.List__I64
  %t3470 = alloca i64
  %t3471 = alloca i64
  %t3479 = alloca i64
  %t3496 = alloca i8
  %t3457 = load ptr, ptr %t3456
  %t3458 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t3457)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3459)
  store i64 %t3458, ptr %t3459
  %t3460 = load ptr, ptr %t3456
  %t3462 = ptrtoint ptr %t3460 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3463)
  store i64 %t3462, ptr %t3463
  %t3464 = call %struct.List__Str @tml_N3std11collections4list9List__Str3newE(i64 16)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3465)
  store %struct.List__Str %t3464, ptr %t3465
  %t3466 = load ptr, ptr %t3456
  %t3467 = trunc i32 10 to i8
  %t3468 = call %struct.List__I64 @tml_N4core3str4simd13find_all_byteE_Sh(ptr %t3466, i8 %t3467)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3469)
  store %struct.List__I64 %t3468, ptr %t3469
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3470)
  store i64 0, ptr %t3470
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3471)
  store i64 0, ptr %t3471
  br label %loop.preheader1042
loop.preheader1042:
  br label %loop.header1043
loop.header1043:
  %t3472 = load i64, ptr %t3471
  %t3473 = load %struct.List__I64, ptr %t3469
  %t3474 = call i64 @tml_N3std11collections4list9List__I643lenE(ptr %t3469)
  %t3475 = icmp slt i64 %t3472, %t3474
  br i1 %t3475, label %loop.body1044, label %loop.exit1046
loop.body1044:
  %t3476 = load %struct.List__I64, ptr %t3469
  %t3477 = load i64, ptr %t3471
  %t3478 = call i64 @tml_N3std11collections4list9List__I643getE(ptr %t3469, i64 %t3477)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3479)
  store i64 %t3478, ptr %t3479
  %t3480 = load i64, ptr %t3479
  %t3481 = load i64, ptr %t3470
  %t3482 = icmp sgt i64 %t3480, %t3481
  br i1 %t3482, label %if.then1047, label %if.end1049
if.then1047:
  %t3483 = load i64, ptr %t3463
  %t3484 = load i64, ptr %t3479
  %t3486 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3483, i64 %t3484)
  %t3485 = extractvalue { i64, i1 } %t3486, 0
  %t3487 = extractvalue { i64, i1 } %t3486, 1
  br i1 %t3487, label %add_overflow1051, label %add_ok1050
add_overflow1051:
  call void @panic(ptr @.str.209)
  unreachable
add_ok1050:
  %t3489 = sext i32 1 to i64
  %t3490 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t3485, i64 %t3489)
  %t3488 = extractvalue { i64, i1 } %t3490, 0
  %t3491 = extractvalue { i64, i1 } %t3490, 1
  br i1 %t3491, label %sub_overflow1053, label %sub_ok1052
sub_overflow1053:
  call void @panic(ptr @.str.210)
  unreachable
sub_ok1052:
  %t3492 = inttoptr i64 %t3488 to ptr
  %t3493 = alloca ptr
  store ptr %t3492, ptr %t3493
  %t3494 = load ptr, ptr %t3493
  %t3495 = load i8, ptr %t3494
  call void @llvm.lifetime.start.p0(i64 1, ptr %t3496)
  store i8 %t3495, ptr %t3496
  %t3497 = load i8, ptr %t3496
  %t3498 = zext i8 %t3497 to i32
  %t3499 = icmp eq i32 %t3498, 13
  br i1 %t3499, label %if.then1054, label %if.end1056
if.then1054:
  %t3500 = load i64, ptr %t3479
  %t3502 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t3500, i64 1)
  %t3501 = extractvalue { i64, i1 } %t3502, 0
  %t3503 = extractvalue { i64, i1 } %t3502, 1
  br i1 %t3503, label %sub_overflow1058, label %sub_ok1057
sub_overflow1058:
  call void @panic(ptr @.str.211)
  unreachable
sub_ok1057:
  store i64 %t3501, ptr %t3479
  br label %if.end1056
if.end1056:
  call void @llvm.lifetime.end.p0(i64 1, ptr %t3496)
  br label %if.end1049
if.end1049:
  %t3504 = load %struct.List__Str, ptr %t3465
  %t3505 = load ptr, ptr %t3456
  %t3506 = load i64, ptr %t3470
  %t3507 = load i64, ptr %t3479
  %t3508 = call ptr @tml_N4core3str4simd13substring_rawE_Sll(ptr %t3505, i64 %t3506, i64 %t3507)
  %t3509 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t3465, ptr %t3508)
  %t3510 = load %struct.List__I64, ptr %t3469
  %t3511 = load i64, ptr %t3471
  %t3512 = call i64 @tml_N3std11collections4list9List__I643getE(ptr %t3469, i64 %t3511)
  %t3514 = sext i32 1 to i64
  %t3515 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3512, i64 %t3514)
  %t3513 = extractvalue { i64, i1 } %t3515, 0
  %t3516 = extractvalue { i64, i1 } %t3515, 1
  br i1 %t3516, label %add_overflow1060, label %add_ok1059
add_overflow1060:
  call void @panic(ptr @.str.212)
  unreachable
add_ok1059:
  store i64 %t3513, ptr %t3470
  %t3517 = load i64, ptr %t3471
  %t3519 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3517, i64 1)
  %t3518 = extractvalue { i64, i1 } %t3519, 0
  %t3520 = extractvalue { i64, i1 } %t3519, 1
  br i1 %t3520, label %add_overflow1062, label %add_ok1061
add_overflow1062:
  call void @panic(ptr @.str.213)
  unreachable
add_ok1061:
  store i64 %t3518, ptr %t3471
  call void @llvm.lifetime.end.p0(i64 8, ptr %t3479)
  br label %loop.latch1045
loop.latch1045:
  br label %loop.header1043, !llvm.loop !1042
loop.exit1046:
  %t3521 = load i64, ptr %t3470
  %t3522 = load i64, ptr %t3459
  %t3523 = icmp sle i64 %t3521, %t3522
  br i1 %t3523, label %if.then1063, label %if.end1065
if.then1063:
  %t3524 = load %struct.List__Str, ptr %t3465
  %t3525 = load ptr, ptr %t3456
  %t3526 = load i64, ptr %t3470
  %t3527 = load i64, ptr %t3459
  %t3528 = call ptr @tml_N4core3str4simd13substring_rawE_Sll(ptr %t3525, i64 %t3526, i64 %t3527)
  %t3529 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t3465, ptr %t3528)
  br label %if.end1065
if.end1065:
  %t3530 = load %struct.List__Str, ptr %t3465
  %t3531 = load %struct.List__I64, ptr %t3469
  call void @tml_N3std11collections4list9List__I644dropE(ptr %t3469)
  ret %struct.List__Str %t3530
}

; core::str::basic::substring_from
define ptr @tml_N4core3str5basic14substring_fromE_Sl(ptr %s, i64 %start) #0 {
entry:
  %t3532 = alloca ptr
  store ptr %s, ptr %t3532
  %t3533 = alloca i64
  store i64 %start, ptr %t3533
  %t3534 = load ptr, ptr %t3532
  %t3535 = load i64, ptr %t3533
  %t3536 = load ptr, ptr %t3532
  %t3537 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t3536)
  %t3538 = call ptr @tml_N4core3str5basic9substringE_Sll(ptr %t3534, i64 %t3535, i64 %t3537)
  ret ptr %t3538
}

; core::str::convert::bytes
define %struct.List__U8 @tml_N4core3str7convert5bytesE_S(ptr %s) #0 {
entry:
  %t3539 = alloca ptr
  store ptr %s, ptr %t3539
  %t3542 = alloca i64
  %t3545 = alloca %struct.List__U8
  %t3549 = alloca i64
  %t3550 = alloca i64
  %t3563 = alloca i8
  %t3540 = load ptr, ptr %t3539
  %t3541 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t3540)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3542)
  store i64 %t3541, ptr %t3542
  %t3543 = load i64, ptr %t3542
  %t3544 = call %struct.List__U8 @tml_N3std11collections4list8List__U83newE(i64 %t3543)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3545)
  store %struct.List__U8 %t3544, ptr %t3545
  %t3546 = load ptr, ptr %t3539
  %t3548 = ptrtoint ptr %t3546 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3549)
  store i64 %t3548, ptr %t3549
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3550)
  store i64 0, ptr %t3550
  br label %loop.preheader1066
loop.preheader1066:
  br label %loop.header1067
loop.header1067:
  %t3551 = load i64, ptr %t3550
  %t3552 = load i64, ptr %t3542
  %t3553 = icmp slt i64 %t3551, %t3552
  br i1 %t3553, label %loop.body1068, label %loop.exit1070
loop.body1068:
  %t3554 = load i64, ptr %t3549
  %t3555 = load i64, ptr %t3550
  %t3557 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3554, i64 %t3555)
  %t3556 = extractvalue { i64, i1 } %t3557, 0
  %t3558 = extractvalue { i64, i1 } %t3557, 1
  br i1 %t3558, label %add_overflow1072, label %add_ok1071
add_overflow1072:
  call void @panic(ptr @.str.214)
  unreachable
add_ok1071:
  %t3559 = inttoptr i64 %t3556 to ptr
  %t3560 = alloca ptr
  store ptr %t3559, ptr %t3560
  %t3561 = load ptr, ptr %t3560
  %t3562 = load i8, ptr %t3561
  call void @llvm.lifetime.start.p0(i64 1, ptr %t3563)
  store i8 %t3562, ptr %t3563
  %t3564 = load %struct.List__U8, ptr %t3545
  %t3565 = load i8, ptr %t3563
  %t3566 = call {} @tml_N3std11collections4list8List__U88push__U8E(ptr %t3545, i8 %t3565)
  %t3567 = load i64, ptr %t3550
  %t3569 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3567, i64 1)
  %t3568 = extractvalue { i64, i1 } %t3569, 0
  %t3570 = extractvalue { i64, i1 } %t3569, 1
  br i1 %t3570, label %add_overflow1074, label %add_ok1073
add_overflow1074:
  call void @panic(ptr @.str.215)
  unreachable
add_ok1073:
  store i64 %t3568, ptr %t3550
  call void @llvm.lifetime.end.p0(i64 1, ptr %t3563)
  br label %loop.latch1069
loop.latch1069:
  br label %loop.header1067, !llvm.loop !1043
loop.exit1070:
  %t3571 = load %struct.List__U8, ptr %t3545
  ret %struct.List__U8 %t3571
}

; ir_diff::parser::is_linkage_keyword
define internal i1 @tml_N7ir_diff6parser18is_linkage_keywordE_S(ptr %t) #0 {
entry:
  %t3572 = alloca ptr
  store ptr %t, ptr %t3572
  %t3573 = load ptr, ptr %t3572
  %t3575 = call i32 @strcmp(ptr %t3573, ptr @.str.216)
  %t3574 = icmp eq i32 %t3575, 0
  br i1 %t3574, label %if.then1075, label %if.end1077
if.then1075:
  ret i1 1
if.end1077:
  %t3576 = load ptr, ptr %t3572
  %t3578 = call i32 @strcmp(ptr %t3576, ptr @.str.217)
  %t3577 = icmp eq i32 %t3578, 0
  br i1 %t3577, label %if.then1078, label %if.end1080
if.then1078:
  ret i1 1
if.end1080:
  %t3579 = load ptr, ptr %t3572
  %t3581 = call i32 @strcmp(ptr %t3579, ptr @.str.218)
  %t3580 = icmp eq i32 %t3581, 0
  br i1 %t3580, label %if.then1081, label %if.end1083
if.then1081:
  ret i1 1
if.end1083:
  %t3582 = load ptr, ptr %t3572
  %t3584 = call i32 @strcmp(ptr %t3582, ptr @.str.219)
  %t3583 = icmp eq i32 %t3584, 0
  br i1 %t3583, label %if.then1084, label %if.end1086
if.then1084:
  ret i1 1
if.end1086:
  %t3585 = load ptr, ptr %t3572
  %t3587 = call i32 @strcmp(ptr %t3585, ptr @.str.220)
  %t3586 = icmp eq i32 %t3587, 0
  br i1 %t3586, label %if.then1087, label %if.end1089
if.then1087:
  ret i1 1
if.end1089:
  %t3588 = load ptr, ptr %t3572
  %t3590 = call i32 @strcmp(ptr %t3588, ptr @.str.221)
  %t3589 = icmp eq i32 %t3590, 0
  br i1 %t3589, label %if.then1090, label %if.end1092
if.then1090:
  ret i1 1
if.end1092:
  %t3591 = load ptr, ptr %t3572
  %t3593 = call i32 @strcmp(ptr %t3591, ptr @.str.222)
  %t3592 = icmp eq i32 %t3593, 0
  br i1 %t3592, label %if.then1093, label %if.end1095
if.then1093:
  ret i1 1
if.end1095:
  %t3594 = load ptr, ptr %t3572
  %t3596 = call i32 @strcmp(ptr %t3594, ptr @.str.223)
  %t3595 = icmp eq i32 %t3596, 0
  br i1 %t3595, label %if.then1096, label %if.end1098
if.then1096:
  ret i1 1
if.end1098:
  %t3597 = load ptr, ptr %t3572
  %t3599 = call i32 @strcmp(ptr %t3597, ptr @.str.224)
  %t3598 = icmp eq i32 %t3599, 0
  br i1 %t3598, label %if.then1099, label %if.end1101
if.then1099:
  ret i1 1
if.end1101:
  %t3600 = load ptr, ptr %t3572
  %t3602 = call i32 @strcmp(ptr %t3600, ptr @.str.225)
  %t3601 = icmp eq i32 %t3602, 0
  br i1 %t3601, label %if.then1102, label %if.end1104
if.then1102:
  ret i1 1
if.end1104:
  %t3603 = load ptr, ptr %t3572
  %t3605 = call i32 @strcmp(ptr %t3603, ptr @.str.226)
  %t3604 = icmp eq i32 %t3605, 0
  br i1 %t3604, label %if.then1105, label %if.end1107
if.then1105:
  ret i1 1
if.end1107:
  %t3606 = load ptr, ptr %t3572
  %t3608 = call i32 @strcmp(ptr %t3606, ptr @.str.227)
  %t3607 = icmp eq i32 %t3608, 0
  br i1 %t3607, label %if.then1108, label %if.end1110
if.then1108:
  ret i1 1
if.end1110:
  %t3609 = load ptr, ptr %t3572
  %t3611 = call i32 @strcmp(ptr %t3609, ptr @.str.228)
  %t3610 = icmp eq i32 %t3611, 0
  br i1 %t3610, label %if.then1111, label %if.end1113
if.then1111:
  ret i1 1
if.end1113:
  %t3612 = load ptr, ptr %t3572
  %t3614 = call i32 @strcmp(ptr %t3612, ptr @.str.229)
  %t3613 = icmp eq i32 %t3614, 0
  br i1 %t3613, label %if.then1114, label %if.end1116
if.then1114:
  ret i1 1
if.end1116:
  %t3615 = load ptr, ptr %t3572
  %t3617 = call i32 @strcmp(ptr %t3615, ptr @.str.230)
  %t3616 = icmp eq i32 %t3617, 0
  br i1 %t3616, label %if.then1117, label %if.end1119
if.then1117:
  ret i1 1
if.end1119:
  %t3618 = load ptr, ptr %t3572
  %t3620 = call i32 @strcmp(ptr %t3618, ptr @.str.231)
  %t3619 = icmp eq i32 %t3620, 0
  br i1 %t3619, label %if.then1120, label %if.end1122
if.then1120:
  ret i1 1
if.end1122:
  %t3621 = load ptr, ptr %t3572
  %t3623 = call i32 @strcmp(ptr %t3621, ptr @.str.232)
  %t3622 = icmp eq i32 %t3623, 0
  br i1 %t3622, label %if.then1123, label %if.end1125
if.then1123:
  ret i1 1
if.end1125:
  %t3624 = load ptr, ptr %t3572
  %t3626 = call i32 @strcmp(ptr %t3624, ptr @.str.233)
  %t3625 = icmp eq i32 %t3626, 0
  br i1 %t3625, label %if.then1126, label %if.end1128
if.then1126:
  ret i1 1
if.end1128:
  %t3627 = load ptr, ptr %t3572
  %t3629 = call i32 @strcmp(ptr %t3627, ptr @.str.234)
  %t3628 = icmp eq i32 %t3629, 0
  br i1 %t3628, label %if.then1129, label %if.end1131
if.then1129:
  ret i1 1
if.end1131:
  %t3630 = load ptr, ptr %t3572
  %t3632 = call i32 @strcmp(ptr %t3630, ptr @.str.235)
  %t3631 = icmp eq i32 %t3632, 0
  br i1 %t3631, label %if.then1132, label %if.end1134
if.then1132:
  ret i1 1
if.end1134:
  %t3633 = load ptr, ptr %t3572
  %t3635 = call i32 @strcmp(ptr %t3633, ptr @.str.236)
  %t3634 = icmp eq i32 %t3635, 0
  br i1 %t3634, label %if.then1135, label %if.end1137
if.then1135:
  ret i1 1
if.end1137:
  %t3636 = load ptr, ptr %t3572
  %t3638 = call i32 @strcmp(ptr %t3636, ptr @.str.237)
  %t3637 = icmp eq i32 %t3638, 0
  br i1 %t3637, label %if.then1138, label %if.end1140
if.then1138:
  ret i1 1
if.end1140:
  ret i1 0
}

; core::str::convert::concat
define ptr @tml_N4core3str7convert6concatE_SS(ptr %a, ptr %b) #0 {
entry:
  %t3639 = alloca ptr
  store ptr %a, ptr %t3639
  %t3640 = alloca ptr
  store ptr %b, ptr %t3640
  %t3653 = alloca ptr
  %t3641 = load ptr, ptr %t3639
  %t3642 = load ptr, ptr %t3640
  %t3643 = add i64 0, 0
  %t3644 = call i64 @strlen(ptr %t3641)
  %t3645 = add i64 %t3643, %t3644
  %t3646 = call i64 @strlen(ptr %t3642)
  %t3647 = add i64 %t3645, %t3646
  %t3648 = add i64 %t3647, 1
  %t3649 = call ptr @malloc(i64 %t3648)
  call void @llvm.memcpy.p0.p0.i64(ptr %t3649, ptr %t3641, i64 %t3644, i1 false)
  %t3650 = add i64 0, %t3644
  %t3651 = getelementptr i8, ptr %t3649, i64 %t3650
  call void @llvm.memcpy.p0.p0.i64(ptr %t3651, ptr %t3642, i64 %t3646, i1 false)
  %t3652 = getelementptr i8, ptr %t3649, i64 %t3647
  store i8 0, ptr %t3652
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3653)
  store ptr %t3649, ptr %t3653
  %t3654 = load ptr, ptr %t3653
  ret ptr %t3654
}

; ir_diff::differ::str_eq
define internal i1 @tml_N7ir_diff6differ6str_eqE_SS(ptr %a, ptr %b) #0 {
entry:
  %t3655 = alloca ptr
  store ptr %a, ptr %t3655
  %t3656 = alloca ptr
  store ptr %b, ptr %t3656
  %t3659 = alloca i64
  %t3662 = alloca i64
  %t3666 = alloca i64
  %t3657 = load ptr, ptr %t3655
  %t3658 = call i64 @strlen(ptr %t3657)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3659)
  store i64 %t3658, ptr %t3659
  %t3660 = load ptr, ptr %t3656
  %t3661 = call i64 @strlen(ptr %t3660)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3662)
  store i64 %t3661, ptr %t3662
  %t3663 = load i64, ptr %t3659
  %t3664 = load i64, ptr %t3662
  %t3665 = icmp ne i64 %t3663, %t3664
  br i1 %t3665, label %if.then1141, label %if.end1143
if.then1141:
  ret i1 0
if.end1143:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3666)
  store i64 0, ptr %t3666
  br label %loop.preheader1144
loop.preheader1144:
  br label %loop.header1145
loop.header1145:
  %t3667 = load i64, ptr %t3666
  %t3668 = load i64, ptr %t3659
  %t3669 = icmp slt i64 %t3667, %t3668
  br i1 %t3669, label %loop.body1146, label %loop.exit1148
loop.body1146:
  %t3670 = load ptr, ptr %t3655
  %t3671 = load i64, ptr %t3666
  %t3672 = call i32 @tml_N4core3str5basic7char_atE_Sl(ptr %t3670, i64 %t3671)
  %t3673 = load ptr, ptr %t3656
  %t3674 = load i64, ptr %t3666
  %t3675 = call i32 @tml_N4core3str5basic7char_atE_Sl(ptr %t3673, i64 %t3674)
  %t3676 = icmp ne i32 %t3672, %t3675
  br i1 %t3676, label %if.then1149, label %if.end1151
if.then1149:
  ret i1 0
if.end1151:
  %t3677 = load i64, ptr %t3666
  %t3679 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3677, i64 1)
  %t3678 = extractvalue { i64, i1 } %t3679, 0
  %t3680 = extractvalue { i64, i1 } %t3679, 1
  br i1 %t3680, label %add_overflow1153, label %add_ok1152
add_overflow1153:
  call void @panic(ptr @.str.238)
  unreachable
add_ok1152:
  store i64 %t3678, ptr %t3666
  br label %loop.latch1147
loop.latch1147:
  br label %loop.header1145, !llvm.loop !1044
loop.exit1148:
  ret i1 1
}

; std::collections::buffer::buf_mem_free
define void @tml_N3std11collections6buffer12buf_mem_freeE_Pv(ptr %ptr) #0 {
entry:
  %t3681 = alloca ptr
  store ptr %ptr, ptr %t3681
  %t3682 = load ptr, ptr %t3681
  call void @mem_free(ptr %t3682)
  ret void
}

; ir_diff::differ::build_context
define internal %struct.List__Str @tml_N7ir_diff6differ13build_contextE_R4ListISEl(ptr %lines, i64 %idx) #0 {
entry:
  %t3683 = alloca ptr
  store ptr %lines, ptr %t3683
  %t3684 = alloca i64
  store i64 %idx, ptr %t3684
  %t3686 = alloca %struct.List__Str
  %t3689 = alloca i64
  %t3694 = alloca i64
  %t3702 = alloca i64
  %t3708 = alloca i64
  %t3685 = call %struct.List__Str @tml_N3std11collections4list9List__Str3newE(i64 8)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3686)
  store %struct.List__Str %t3685, ptr %t3686
  %t3687 = load ptr, ptr %t3683
  %t3688 = call i64 @tml_N3std11collections4list9List__Str3lenE(ptr %t3687)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3689)
  store i64 %t3688, ptr %t3689
  %t3690 = load i64, ptr %t3684
  %t3692 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t3690, i64 3)
  %t3691 = extractvalue { i64, i1 } %t3692, 0
  %t3693 = extractvalue { i64, i1 } %t3692, 1
  br i1 %t3693, label %sub_overflow1155, label %sub_ok1154
sub_overflow1155:
  call void @panic(ptr @.str.239)
  unreachable
sub_ok1154:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3694)
  store i64 %t3691, ptr %t3694
  %t3695 = load i64, ptr %t3694
  %t3697 = sext i32 0 to i64
  %t3696 = icmp slt i64 %t3695, %t3697
  br i1 %t3696, label %if.then1156, label %if.end1158
if.then1156:
  store i64 0, ptr %t3694
  br label %if.end1158
if.end1158:
  %t3698 = load i64, ptr %t3684
  %t3700 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3698, i64 4)
  %t3699 = extractvalue { i64, i1 } %t3700, 0
  %t3701 = extractvalue { i64, i1 } %t3700, 1
  br i1 %t3701, label %add_overflow1160, label %add_ok1159
add_overflow1160:
  call void @panic(ptr @.str.240)
  unreachable
add_ok1159:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3702)
  store i64 %t3699, ptr %t3702
  %t3703 = load i64, ptr %t3702
  %t3704 = load i64, ptr %t3689
  %t3705 = icmp sgt i64 %t3703, %t3704
  br i1 %t3705, label %if.then1161, label %if.end1163
if.then1161:
  %t3706 = load i64, ptr %t3689
  store i64 %t3706, ptr %t3702
  br label %if.end1163
if.end1163:
  %t3707 = load i64, ptr %t3694
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3708)
  store i64 %t3707, ptr %t3708
  br label %loop.preheader1164
loop.preheader1164:
  br label %loop.header1165
loop.header1165:
  %t3709 = load i64, ptr %t3708
  %t3710 = load i64, ptr %t3702
  %t3711 = icmp slt i64 %t3709, %t3710
  br i1 %t3711, label %loop.body1166, label %loop.exit1168
loop.body1166:
  %t3712 = load %struct.List__Str, ptr %t3686
  %t3713 = load ptr, ptr %t3683
  %t3714 = load i64, ptr %t3708
  %t3715 = call ptr @tml_N3std11collections4list9List__Str3getE(ptr %t3713, i64 %t3714)
  %t3716 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t3686, ptr %t3715)
  %t3717 = load i64, ptr %t3708
  %t3719 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3717, i64 1)
  %t3718 = extractvalue { i64, i1 } %t3719, 0
  %t3720 = extractvalue { i64, i1 } %t3719, 1
  br i1 %t3720, label %add_overflow1170, label %add_ok1169
add_overflow1170:
  call void @panic(ptr @.str.241)
  unreachable
add_ok1169:
  store i64 %t3718, ptr %t3708
  br label %loop.latch1167
loop.latch1167:
  br label %loop.header1165, !llvm.loop !1045
loop.exit1168:
  %t3721 = load %struct.List__Str, ptr %t3686
  ret %struct.List__Str %t3721
}

; core::str::simd::contains_simd
define i1 @tml_N4core3str4simd13contains_simdE_SS(ptr %s, ptr %pattern) #0 {
entry:
  %t3722 = alloca ptr
  store ptr %s, ptr %t3722
  %t3723 = alloca ptr
  store ptr %pattern, ptr %t3723
  %t3724 = load ptr, ptr %t3722
  %t3725 = load ptr, ptr %t3723
  %t3726 = call i1 @tml_N4core3str4simd13contains_fastE_SS(ptr %t3724, ptr %t3725)
  ret i1 %t3726
}

; core::str::basic::char_at
define i32 @tml_N4core3str5basic7char_atE_Sl(ptr %s, i64 %index) #0 {
entry:
  %t3727 = alloca ptr
  store ptr %s, ptr %t3727
  %t3728 = alloca i64
  store i64 %index, ptr %t3728
  %t3732 = alloca i64
  %t3749 = alloca i8
  %t3729 = load ptr, ptr %t3727
  %t3731 = ptrtoint ptr %t3729 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3732)
  store i64 %t3731, ptr %t3732
  %t3733 = load i64, ptr %t3732
  %t3735 = sext i32 0 to i64
  %t3734 = icmp eq i64 %t3733, %t3735
  %t3736 = load i64, ptr %t3728
  %t3738 = sext i32 0 to i64
  %t3737 = icmp slt i64 %t3736, %t3738
  %t3739 = or i1 %t3734, %t3737
  br i1 %t3739, label %if.then1171, label %if.end1173
if.then1171:
  ret i32 0
if.end1173:
  %t3740 = load i64, ptr %t3732
  %t3741 = load i64, ptr %t3728
  %t3743 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3740, i64 %t3741)
  %t3742 = extractvalue { i64, i1 } %t3743, 0
  %t3744 = extractvalue { i64, i1 } %t3743, 1
  br i1 %t3744, label %add_overflow1175, label %add_ok1174
add_overflow1175:
  call void @panic(ptr @.str.208)
  unreachable
add_ok1174:
  %t3745 = inttoptr i64 %t3742 to ptr
  %t3746 = alloca ptr
  store ptr %t3745, ptr %t3746
  %t3747 = load ptr, ptr %t3746
  %t3748 = load i8, ptr %t3747
  call void @llvm.lifetime.start.p0(i64 1, ptr %t3749)
  store i8 %t3748, ptr %t3749
  %t3750 = load i8, ptr %t3749
  %t3751 = zext i8 %t3750 to i32
  ret i32 %t3751
}
; DEBUG LAZY type_name=I8x16 method=splat

define internal %struct.I8x16 @tml_N4core4simd5i8x165I8x165splatE(i8 %v) #0 {
entry:
  %t3752 = alloca i8
  store i8 %v, ptr %t3752
  %t3753 = load i8, ptr %t3752
  %t3754 = insertelement <16 x i8> undef, i8 %t3753, i32 0
  %t3755 = load i8, ptr %t3752
  %t3756 = insertelement <16 x i8> %t3754, i8 %t3755, i32 1
  %t3757 = load i8, ptr %t3752
  %t3758 = insertelement <16 x i8> %t3756, i8 %t3757, i32 2
  %t3759 = load i8, ptr %t3752
  %t3760 = insertelement <16 x i8> %t3758, i8 %t3759, i32 3
  %t3761 = load i8, ptr %t3752
  %t3762 = insertelement <16 x i8> %t3760, i8 %t3761, i32 4
  %t3763 = load i8, ptr %t3752
  %t3764 = insertelement <16 x i8> %t3762, i8 %t3763, i32 5
  %t3765 = load i8, ptr %t3752
  %t3766 = insertelement <16 x i8> %t3764, i8 %t3765, i32 6
  %t3767 = load i8, ptr %t3752
  %t3768 = insertelement <16 x i8> %t3766, i8 %t3767, i32 7
  %t3769 = load i8, ptr %t3752
  %t3770 = insertelement <16 x i8> %t3768, i8 %t3769, i32 8
  %t3771 = load i8, ptr %t3752
  %t3772 = insertelement <16 x i8> %t3770, i8 %t3771, i32 9
  %t3773 = load i8, ptr %t3752
  %t3774 = insertelement <16 x i8> %t3772, i8 %t3773, i32 10
  %t3775 = load i8, ptr %t3752
  %t3776 = insertelement <16 x i8> %t3774, i8 %t3775, i32 11
  %t3777 = load i8, ptr %t3752
  %t3778 = insertelement <16 x i8> %t3776, i8 %t3777, i32 12
  %t3779 = load i8, ptr %t3752
  %t3780 = insertelement <16 x i8> %t3778, i8 %t3779, i32 13
  %t3781 = load i8, ptr %t3752
  %t3782 = insertelement <16 x i8> %t3780, i8 %t3781, i32 14
  %t3783 = load i8, ptr %t3752
  %t3784 = insertelement <16 x i8> %t3782, i8 %t3783, i32 15
  %t3785 = alloca <16 x i8>
  store <16 x i8> %t3784, ptr %t3785
  %t3786 = load <16 x i8>, ptr %t3785
  ret %struct.I8x16 %t3786
}

; ir_diff::differ::is_type_arg_char
define internal i1 @tml_N7ir_diff6differ16is_type_arg_charE_i(i32 %c) #0 {
entry:
  %t3787 = alloca i32
  store i32 %c, ptr %t3787
  %t3788 = load i32, ptr %t3787
  %t3789 = icmp sge i32 %t3788, 65
  %t3790 = load i32, ptr %t3787
  %t3791 = icmp sle i32 %t3790, 90
  %t3792 = and i1 %t3789, %t3791
  br i1 %t3792, label %if.then1176, label %if.end1178
if.then1176:
  ret i1 1
if.end1178:
  %t3793 = load i32, ptr %t3787
  %t3794 = icmp sge i32 %t3793, 48
  %t3795 = load i32, ptr %t3787
  %t3796 = icmp sle i32 %t3795, 57
  %t3797 = and i1 %t3794, %t3796
  br i1 %t3797, label %if.then1179, label %if.end1181
if.then1179:
  ret i1 1
if.end1181:
  ret i1 0
}

; core::str::simd::find_simd
define %struct.Maybe__I64 @tml_N4core3str4simd9find_simdE_SS(ptr %s, ptr %pattern) #0 {
entry:
  %t3798 = alloca ptr
  store ptr %s, ptr %t3798
  %t3799 = alloca ptr
  store ptr %pattern, ptr %t3799
  %t3802 = alloca i64
  %t3805 = alloca i64
  %t3823 = alloca i64
  %t3831 = alloca i64
  %t3834 = alloca i8
  %t3837 = alloca %struct.I8x16
  %t3838 = alloca i64
  %t3857 = alloca %struct.I8x16
  %t3862 = alloca %struct.I8x16
  %t3865 = alloca i32
  %t3870 = alloca i32
  %t3877 = alloca i64
  %t3800 = load ptr, ptr %t3798
  %t3801 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t3800)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3802)
  store i64 %t3801, ptr %t3802
  %t3803 = load ptr, ptr %t3799
  %t3804 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t3803)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3805)
  store i64 %t3804, ptr %t3805
  %t3806 = load i64, ptr %t3805
  %t3808 = sext i32 0 to i64
  %t3807 = icmp eq i64 %t3806, %t3808
  br i1 %t3807, label %if.then1182, label %if.end1184
if.then1182:
  %t3810 = alloca %struct.Maybe__I64, align 8
  %t3811 = getelementptr inbounds %struct.Maybe__I64, ptr %t3810, i32 0, i32 0
  store i32 0, ptr %t3811
  %t3812 = getelementptr inbounds %struct.Maybe__I64, ptr %t3810, i32 0, i32 1
  %t3813 = bitcast ptr %t3812 to ptr
  store i32 0, ptr %t3813
  %t3809 = load %struct.Maybe__I64, ptr %t3810
  ret %struct.Maybe__I64 %t3809
if.end1184:
  %t3814 = load i64, ptr %t3805
  %t3815 = load i64, ptr %t3802
  %t3816 = icmp sgt i64 %t3814, %t3815
  br i1 %t3816, label %if.then1185, label %if.end1187
if.then1185:
  %t3818 = alloca %struct.Maybe__I64, align 8
  %t3819 = getelementptr inbounds %struct.Maybe__I64, ptr %t3818, i32 0, i32 0
  store i32 1, ptr %t3819
  %t3817 = load %struct.Maybe__I64, ptr %t3818
  ret %struct.Maybe__I64 %t3817
if.end1187:
  %t3820 = load ptr, ptr %t3798
  %t3822 = ptrtoint ptr %t3820 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3823)
  store i64 %t3822, ptr %t3823
  %t3824 = load ptr, ptr %t3799
  %t3825 = alloca ptr
  store ptr %t3824, ptr %t3825
  %t3826 = load i64, ptr %t3802
  %t3827 = load i64, ptr %t3805
  %t3829 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t3826, i64 %t3827)
  %t3828 = extractvalue { i64, i1 } %t3829, 0
  %t3830 = extractvalue { i64, i1 } %t3829, 1
  br i1 %t3830, label %sub_overflow1189, label %sub_ok1188
sub_overflow1189:
  call void @panic(ptr @.str.242)
  unreachable
sub_ok1188:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3831)
  store i64 %t3828, ptr %t3831
  %t3832 = load ptr, ptr %t3825
  %t3833 = load i8, ptr %t3832
  call void @llvm.lifetime.start.p0(i64 1, ptr %t3834)
  store i8 %t3833, ptr %t3834
  %t3835 = load i8, ptr %t3834
  %t3836 = call %struct.I8x16 @tml_N4core4simd5i8x165I8x165splatE(i8 %t3835)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3837)
  store %struct.I8x16 %t3836, ptr %t3837
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3838)
  store i64 0, ptr %t3838
  br label %loop.preheader1190
loop.preheader1190:
  br label %loop.header1191
loop.header1191:
  %t3839 = load i64, ptr %t3838
  %t3841 = sext i32 16 to i64
  %t3842 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3839, i64 %t3841)
  %t3840 = extractvalue { i64, i1 } %t3842, 0
  %t3843 = extractvalue { i64, i1 } %t3842, 1
  br i1 %t3843, label %add_overflow1196, label %add_ok1195
add_overflow1196:
  call void @panic(ptr @.str.243)
  unreachable
add_ok1195:
  %t3844 = load i64, ptr %t3802
  %t3845 = icmp sle i64 %t3840, %t3844
  %t3846 = load i64, ptr %t3838
  %t3847 = load i64, ptr %t3831
  %t3848 = icmp sle i64 %t3846, %t3847
  %t3849 = and i1 %t3845, %t3848
  br i1 %t3849, label %loop.body1192, label %loop.exit1194
loop.body1192:
  %t3850 = load i64, ptr %t3823
  %t3851 = load i64, ptr %t3838
  %t3853 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3850, i64 %t3851)
  %t3852 = extractvalue { i64, i1 } %t3853, 0
  %t3854 = extractvalue { i64, i1 } %t3853, 1
  br i1 %t3854, label %add_overflow1198, label %add_ok1197
add_overflow1198:
  call void @panic(ptr @.str.244)
  unreachable
add_ok1197:
  %t3855 = inttoptr i64 %t3852 to ptr
  %t3856 = load <16 x i8>, ptr %t3855, align 1
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3857)
  store %struct.I8x16 %t3856, ptr %t3857
  %t3858 = load %struct.I8x16, ptr %t3857
  %t3859 = load %struct.I8x16, ptr %t3837
  %t3860 = icmp eq %struct.I8x16 %t3858, %t3859
  %t3861 = sext <16 x i1> %t3860 to <16 x i8>
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3862)
  store %struct.I8x16 %t3861, ptr %t3862
  %t3863 = load %struct.I8x16, ptr %t3862
  %t3864 = call i32 @llvm.x86.sse2.pmovmskb.128(<16 x i8> %t3863)
  call void @llvm.lifetime.start.p0(i64 4, ptr %t3865)
  store i32 %t3864, ptr %t3865
  br label %loop.preheader1199
loop.preheader1199:
  br label %loop.header1200
loop.header1200:
  %t3866 = load i32, ptr %t3865
  %t3867 = icmp ne i32 %t3866, 0
  br i1 %t3867, label %loop.body1201, label %loop.exit1203
loop.body1201:
  %t3868 = load i32, ptr %t3865
  %t3869 = call i32 @tml_N4core3str4simd3ctzE_i(i32 %t3868)
  call void @llvm.lifetime.start.p0(i64 4, ptr %t3870)
  store i32 %t3869, ptr %t3870
  %t3871 = load i64, ptr %t3838
  %t3872 = load i32, ptr %t3870
  %t3873 = sext i32 %t3872 to i64
  %t3875 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3871, i64 %t3873)
  %t3874 = extractvalue { i64, i1 } %t3875, 0
  %t3876 = extractvalue { i64, i1 } %t3875, 1
  br i1 %t3876, label %add_overflow1205, label %add_ok1204
add_overflow1205:
  call void @panic(ptr @.str.245)
  unreachable
add_ok1204:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3877)
  store i64 %t3874, ptr %t3877
  %t3878 = load i64, ptr %t3877
  %t3879 = load i64, ptr %t3831
  %t3880 = icmp sle i64 %t3878, %t3879
  br i1 %t3880, label %if.then1206, label %if.end1208
if.then1206:
  %t3881 = load i64, ptr %t3823
  %t3882 = load i64, ptr %t3877
  %t3884 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3881, i64 %t3882)
  %t3883 = extractvalue { i64, i1 } %t3884, 0
  %t3885 = extractvalue { i64, i1 } %t3884, 1
  br i1 %t3885, label %add_overflow1210, label %add_ok1209
add_overflow1210:
  call void @panic(ptr @.str.246)
  unreachable
add_ok1209:
  %t3886 = inttoptr i64 %t3883 to ptr
  %t3887 = load ptr, ptr %t3825
  %t3888 = load i64, ptr %t3805
  %t3889 = call i32 @memcmp(ptr %t3886, ptr %t3887, i64 %t3888)
  %t3890 = icmp eq i32 %t3889, 0
  br i1 %t3890, label %if.then1211, label %if.end1213
if.then1211:
  %t3892 = alloca %struct.Maybe__I64, align 8
  %t3893 = getelementptr inbounds %struct.Maybe__I64, ptr %t3892, i32 0, i32 0
  store i32 0, ptr %t3893
  %t3894 = load i64, ptr %t3877
  %t3895 = getelementptr inbounds %struct.Maybe__I64, ptr %t3892, i32 0, i32 1
  %t3896 = bitcast ptr %t3895 to ptr
  store i64 %t3894, ptr %t3896
  %t3891 = load %struct.Maybe__I64, ptr %t3892
  call void @llvm.lifetime.end.p0(i64 8, ptr %t3877)
  call void @llvm.lifetime.end.p0(i64 4, ptr %t3870)
  call void @llvm.lifetime.end.p0(i64 4, ptr %t3865)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t3862)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t3857)
  ret %struct.Maybe__I64 %t3891
if.end1213:
  br label %if.end1208
if.end1208:
  %t3897 = load i32, ptr %t3865
  %t3898 = load i32, ptr %t3865
  %t3900 = call { i32, i1 } @llvm.ssub.with.overflow.i32(i32 %t3898, i32 1)
  %t3899 = extractvalue { i32, i1 } %t3900, 0
  %t3901 = extractvalue { i32, i1 } %t3900, 1
  br i1 %t3901, label %sub_overflow1215, label %sub_ok1214
sub_overflow1215:
  call void @panic(ptr @.str.247)
  unreachable
sub_ok1214:
  %t3902 = and i32 %t3897, %t3899
  store i32 %t3902, ptr %t3865
  call void @llvm.lifetime.end.p0(i64 8, ptr %t3877)
  call void @llvm.lifetime.end.p0(i64 4, ptr %t3870)
  br label %loop.latch1202
loop.latch1202:
  br label %loop.header1200, !llvm.loop !1047
loop.exit1203:
  %t3903 = load i64, ptr %t3838
  %t3905 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3903, i64 16)
  %t3904 = extractvalue { i64, i1 } %t3905, 0
  %t3906 = extractvalue { i64, i1 } %t3905, 1
  br i1 %t3906, label %add_overflow1217, label %add_ok1216
add_overflow1217:
  call void @panic(ptr @.str.248)
  unreachable
add_ok1216:
  store i64 %t3904, ptr %t3838
  call void @llvm.lifetime.end.p0(i64 4, ptr %t3865)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t3862)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t3857)
  br label %loop.latch1193
loop.latch1193:
  br label %loop.header1191, !llvm.loop !1046
loop.exit1194:
  br label %loop.preheader1218
loop.preheader1218:
  br label %loop.header1219
loop.header1219:
  %t3907 = load i64, ptr %t3838
  %t3908 = load i64, ptr %t3831
  %t3909 = icmp sle i64 %t3907, %t3908
  br i1 %t3909, label %loop.body1220, label %loop.exit1222
loop.body1220:
  %t3910 = load i64, ptr %t3823
  %t3911 = load i64, ptr %t3838
  %t3913 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3910, i64 %t3911)
  %t3912 = extractvalue { i64, i1 } %t3913, 0
  %t3914 = extractvalue { i64, i1 } %t3913, 1
  br i1 %t3914, label %add_overflow1224, label %add_ok1223
add_overflow1224:
  call void @panic(ptr @.str.249)
  unreachable
add_ok1223:
  %t3915 = inttoptr i64 %t3912 to ptr
  %t3916 = load ptr, ptr %t3825
  %t3917 = load i64, ptr %t3805
  %t3918 = call i32 @memcmp(ptr %t3915, ptr %t3916, i64 %t3917)
  %t3919 = icmp eq i32 %t3918, 0
  br i1 %t3919, label %if.then1225, label %if.end1227
if.then1225:
  %t3921 = alloca %struct.Maybe__I64, align 8
  %t3922 = getelementptr inbounds %struct.Maybe__I64, ptr %t3921, i32 0, i32 0
  store i32 0, ptr %t3922
  %t3923 = load i64, ptr %t3838
  %t3924 = getelementptr inbounds %struct.Maybe__I64, ptr %t3921, i32 0, i32 1
  %t3925 = bitcast ptr %t3924 to ptr
  store i64 %t3923, ptr %t3925
  %t3920 = load %struct.Maybe__I64, ptr %t3921
  ret %struct.Maybe__I64 %t3920
if.end1227:
  %t3926 = load i64, ptr %t3838
  %t3928 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3926, i64 1)
  %t3927 = extractvalue { i64, i1 } %t3928, 0
  %t3929 = extractvalue { i64, i1 } %t3928, 1
  br i1 %t3929, label %add_overflow1229, label %add_ok1228
add_overflow1229:
  call void @panic(ptr @.str.250)
  unreachable
add_ok1228:
  store i64 %t3927, ptr %t3838
  br label %loop.latch1221
loop.latch1221:
  br label %loop.header1219, !llvm.loop !1048
loop.exit1222:
  %t3931 = alloca %struct.Maybe__I64, align 8
  %t3932 = getelementptr inbounds %struct.Maybe__I64, ptr %t3931, i32 0, i32 0
  store i32 1, ptr %t3932
  %t3930 = load %struct.Maybe__I64, ptr %t3931
  ret %struct.Maybe__I64 %t3930
}

; core::str::simd::ctz
define internal i32 @tml_N4core3str4simd3ctzE_i(i32 %mask) #0 {
entry:
  %t3933 = alloca i32
  store i32 %mask, ptr %t3933
  %t3934 = alloca i32
  %t3936 = alloca i32
  call void @llvm.lifetime.start.p0(i64 4, ptr %t3934)
  store i32 0, ptr %t3934
  %t3935 = load i32, ptr %t3933
  call void @llvm.lifetime.start.p0(i64 4, ptr %t3936)
  store i32 %t3935, ptr %t3936
  br label %loop.preheader1230
loop.preheader1230:
  br label %loop.header1231
loop.header1231:
  %t3937 = load i32, ptr %t3936
  %t3938 = and i32 %t3937, 1
  %t3939 = icmp eq i32 %t3938, 0
  %t3940 = load i32, ptr %t3934
  %t3941 = icmp slt i32 %t3940, 32
  %t3942 = and i1 %t3939, %t3941
  br i1 %t3942, label %loop.body1232, label %loop.exit1234
loop.body1232:
  %t3943 = load i32, ptr %t3936
  %t3944 = ashr i32 %t3943, 1
  store i32 %t3944, ptr %t3936
  %t3945 = load i32, ptr %t3934
  %t3947 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %t3945, i32 1)
  %t3946 = extractvalue { i32, i1 } %t3947, 0
  %t3948 = extractvalue { i32, i1 } %t3947, 1
  br i1 %t3948, label %add_overflow1236, label %add_ok1235
add_overflow1236:
  call void @panic(ptr @.str.251)
  unreachable
add_ok1235:
  store i32 %t3946, ptr %t3934
  br label %loop.latch1233
loop.latch1233:
  br label %loop.header1231, !llvm.loop !1049
loop.exit1234:
  %t3949 = load i32, ptr %t3934
  ret i32 %t3949
}
; DEBUG LAZY type_name=I8x16 method=bor

define internal %struct.I8x16 @tml_N4core4simd5i8x165I8x163borE(ptr %this, %struct.I8x16 %other) #0 {
entry:
  %t3950 = alloca %struct.I8x16
  store %struct.I8x16 %other, ptr %t3950
  %t3951 = load <16 x i8>, ptr %this
  %t3952 = alloca <16 x i8>
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3952)
  store <16 x i8> %t3951, ptr %t3952
  %t3953 = load <16 x i8>, ptr %t3950
  %t3954 = alloca <16 x i8>
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3954)
  store <16 x i8> %t3953, ptr %t3954
  %t3955 = load <16 x i8>, ptr %t3952
  %t3956 = load <16 x i8>, ptr %t3954
  %t3957 = or <16 x i8> %t3955, %t3956
  %t3958 = alloca <16 x i8>
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3958)
  store <16 x i8> %t3957, ptr %t3958
  %t3959 = call %struct.I8x16 @tml_N4core4simd5i8x165I8x164zeroE()
  %t3960 = alloca %struct.I8x16
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3960)
  store %struct.I8x16 %t3959, ptr %t3960
  %t3961 = load <16 x i8>, ptr %t3958
  store <16 x i8> %t3961, ptr %t3960
  %t3962 = load %struct.I8x16, ptr %t3960
  ret %struct.I8x16 %t3962
}

; core::str::simd::split_by_byte_simd
define %struct.List__Str @tml_N4core3str4simd18split_by_byte_simdE_Sh(ptr %s, i8 %delim_byte) #0 {
entry:
  %t3963 = alloca ptr
  store ptr %s, ptr %t3963
  %t3964 = alloca i8
  store i8 %delim_byte, ptr %t3964
  %t3967 = alloca i64
  %t3971 = alloca %struct.List__I64
  %t3979 = alloca %struct.List__Str
  %t3980 = alloca i64
  %t3981 = alloca i64
  %t3989 = alloca i64
  %t3965 = load ptr, ptr %t3963
  %t3966 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t3965)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3967)
  store i64 %t3966, ptr %t3967
  %t3968 = load ptr, ptr %t3963
  %t3969 = load i8, ptr %t3964
  %t3970 = call %struct.List__I64 @tml_N4core3str4simd13find_all_byteE_Sh(ptr %t3968, i8 %t3969)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3971)
  store %struct.List__I64 %t3970, ptr %t3971
  %t3972 = load %struct.List__I64, ptr %t3971
  %t3973 = call i64 @tml_N3std11collections4list9List__I643lenE(ptr %t3971)
  %t3975 = sext i32 1 to i64
  %t3976 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3973, i64 %t3975)
  %t3974 = extractvalue { i64, i1 } %t3976, 0
  %t3977 = extractvalue { i64, i1 } %t3976, 1
  br i1 %t3977, label %add_overflow1238, label %add_ok1237
add_overflow1238:
  call void @panic(ptr @.str.252)
  unreachable
add_ok1237:
  %t3978 = call %struct.List__Str @tml_N3std11collections4list9List__Str3newE(i64 %t3974)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t3979)
  store %struct.List__Str %t3978, ptr %t3979
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3980)
  store i64 0, ptr %t3980
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3981)
  store i64 0, ptr %t3981
  br label %loop.preheader1239
loop.preheader1239:
  br label %loop.header1240
loop.header1240:
  %t3982 = load i64, ptr %t3981
  %t3983 = load %struct.List__I64, ptr %t3971
  %t3984 = call i64 @tml_N3std11collections4list9List__I643lenE(ptr %t3971)
  %t3985 = icmp slt i64 %t3982, %t3984
  br i1 %t3985, label %loop.body1241, label %loop.exit1243
loop.body1241:
  %t3986 = load %struct.List__I64, ptr %t3971
  %t3987 = load i64, ptr %t3981
  %t3988 = call i64 @tml_N3std11collections4list9List__I643getE(ptr %t3971, i64 %t3987)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t3989)
  store i64 %t3988, ptr %t3989
  %t3990 = load %struct.List__Str, ptr %t3979
  %t3991 = load ptr, ptr %t3963
  %t3992 = load i64, ptr %t3980
  %t3993 = load i64, ptr %t3989
  %t3994 = call ptr @tml_N4core3str4simd13substring_rawE_Sll(ptr %t3991, i64 %t3992, i64 %t3993)
  %t3995 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t3979, ptr %t3994)
  %t3996 = load i64, ptr %t3989
  %t3998 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t3996, i64 1)
  %t3997 = extractvalue { i64, i1 } %t3998, 0
  %t3999 = extractvalue { i64, i1 } %t3998, 1
  br i1 %t3999, label %add_overflow1245, label %add_ok1244
add_overflow1245:
  call void @panic(ptr @.str.253)
  unreachable
add_ok1244:
  store i64 %t3997, ptr %t3980
  %t4000 = load i64, ptr %t3981
  %t4002 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4000, i64 1)
  %t4001 = extractvalue { i64, i1 } %t4002, 0
  %t4003 = extractvalue { i64, i1 } %t4002, 1
  br i1 %t4003, label %add_overflow1247, label %add_ok1246
add_overflow1247:
  call void @panic(ptr @.str.254)
  unreachable
add_ok1246:
  store i64 %t4001, ptr %t3981
  call void @llvm.lifetime.end.p0(i64 8, ptr %t3989)
  br label %loop.latch1242
loop.latch1242:
  br label %loop.header1240, !llvm.loop !1050
loop.exit1243:
  %t4004 = load %struct.List__Str, ptr %t3979
  %t4005 = load ptr, ptr %t3963
  %t4006 = load i64, ptr %t3980
  %t4007 = load i64, ptr %t3967
  %t4008 = call ptr @tml_N4core3str4simd13substring_rawE_Sll(ptr %t4005, i64 %t4006, i64 %t4007)
  %t4009 = call {} @tml_N3std11collections4list9List__Str9push__StrE(ptr %t3979, ptr %t4008)
  %t4010 = load %struct.List__Str, ptr %t3979
  %t4011 = load %struct.List__I64, ptr %t3971
  call void @tml_N3std11collections4list9List__I644dropE(ptr %t3971)
  ret %struct.List__Str %t4010
}

; core::str::simd::find_all_byte
define %struct.List__I64 @tml_N4core3str4simd13find_all_byteE_Sh(ptr %s, i8 %byte) #0 {
entry:
  %t4012 = alloca ptr
  store ptr %s, ptr %t4012
  %t4013 = alloca i8
  store i8 %byte, ptr %t4013
  %t4016 = alloca i64
  %t4020 = alloca i64
  %t4022 = alloca %struct.List__I64
  %t4026 = alloca i64
  %t4039 = alloca i8
  %t4053 = alloca %struct.I8x16
  %t4054 = alloca i64
  %t4069 = alloca %struct.I8x16
  %t4074 = alloca %struct.I8x16
  %t4077 = alloca i32
  %t4082 = alloca i32
  %t4113 = alloca i8
  %t4014 = load ptr, ptr %t4012
  %t4015 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t4014)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4016)
  store i64 %t4015, ptr %t4016
  %t4017 = load ptr, ptr %t4012
  %t4019 = ptrtoint ptr %t4017 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4020)
  store i64 %t4019, ptr %t4020
  %t4021 = call %struct.List__I64 @tml_N3std11collections4list9List__I643newE(i64 16)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t4022)
  store %struct.List__I64 %t4021, ptr %t4022
  %t4023 = load i64, ptr %t4016
  %t4025 = sext i32 32 to i64
  %t4024 = icmp slt i64 %t4023, %t4025
  br i1 %t4024, label %if.then1248, label %if.end1250
if.then1248:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4026)
  store i64 0, ptr %t4026
  br label %loop.preheader1251
loop.preheader1251:
  br label %loop.header1252
loop.header1252:
  %t4027 = load i64, ptr %t4026
  %t4028 = load i64, ptr %t4016
  %t4029 = icmp slt i64 %t4027, %t4028
  br i1 %t4029, label %loop.body1253, label %loop.exit1255
loop.body1253:
  %t4030 = load i64, ptr %t4020
  %t4031 = load i64, ptr %t4026
  %t4033 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4030, i64 %t4031)
  %t4032 = extractvalue { i64, i1 } %t4033, 0
  %t4034 = extractvalue { i64, i1 } %t4033, 1
  br i1 %t4034, label %add_overflow1257, label %add_ok1256
add_overflow1257:
  call void @panic(ptr @.str.255)
  unreachable
add_ok1256:
  %t4035 = inttoptr i64 %t4032 to ptr
  %t4036 = alloca ptr
  store ptr %t4035, ptr %t4036
  %t4037 = load ptr, ptr %t4036
  %t4038 = load i8, ptr %t4037
  call void @llvm.lifetime.start.p0(i64 1, ptr %t4039)
  store i8 %t4038, ptr %t4039
  %t4040 = load i8, ptr %t4039
  %t4041 = load i8, ptr %t4013
  %t4042 = icmp eq i8 %t4040, %t4041
  br i1 %t4042, label %if.then1258, label %if.end1260
if.then1258:
  %t4043 = load %struct.List__I64, ptr %t4022
  %t4044 = load i64, ptr %t4026
  %t4045 = call {} @tml_N3std11collections4list9List__I649push__I64E(ptr %t4022, i64 %t4044)
  br label %if.end1260
if.end1260:
  %t4046 = load i64, ptr %t4026
  %t4048 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4046, i64 1)
  %t4047 = extractvalue { i64, i1 } %t4048, 0
  %t4049 = extractvalue { i64, i1 } %t4048, 1
  br i1 %t4049, label %add_overflow1262, label %add_ok1261
add_overflow1262:
  call void @panic(ptr @.str.256)
  unreachable
add_ok1261:
  store i64 %t4047, ptr %t4026
  call void @llvm.lifetime.end.p0(i64 1, ptr %t4039)
  br label %loop.latch1254
loop.latch1254:
  br label %loop.header1252, !llvm.loop !1051
loop.exit1255:
  %t4050 = load %struct.List__I64, ptr %t4022
  call void @llvm.lifetime.end.p0(i64 8, ptr %t4026)
  ret %struct.List__I64 %t4050
if.end1250:
  %t4051 = load i8, ptr %t4013
  %t4052 = call %struct.I8x16 @tml_N4core4simd5i8x165I8x165splatE(i8 %t4051)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t4053)
  store %struct.I8x16 %t4052, ptr %t4053
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4054)
  store i64 0, ptr %t4054
  br label %loop.preheader1263
loop.preheader1263:
  br label %loop.header1264
loop.header1264:
  %t4055 = load i64, ptr %t4054
  %t4057 = sext i32 16 to i64
  %t4058 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4055, i64 %t4057)
  %t4056 = extractvalue { i64, i1 } %t4058, 0
  %t4059 = extractvalue { i64, i1 } %t4058, 1
  br i1 %t4059, label %add_overflow1269, label %add_ok1268
add_overflow1269:
  call void @panic(ptr @.str.257)
  unreachable
add_ok1268:
  %t4060 = load i64, ptr %t4016
  %t4061 = icmp sle i64 %t4056, %t4060
  br i1 %t4061, label %loop.body1265, label %loop.exit1267
loop.body1265:
  %t4062 = load i64, ptr %t4020
  %t4063 = load i64, ptr %t4054
  %t4065 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4062, i64 %t4063)
  %t4064 = extractvalue { i64, i1 } %t4065, 0
  %t4066 = extractvalue { i64, i1 } %t4065, 1
  br i1 %t4066, label %add_overflow1271, label %add_ok1270
add_overflow1271:
  call void @panic(ptr @.str.258)
  unreachable
add_ok1270:
  %t4067 = inttoptr i64 %t4064 to ptr
  %t4068 = load <16 x i8>, ptr %t4067, align 1
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t4069)
  store %struct.I8x16 %t4068, ptr %t4069
  %t4070 = load %struct.I8x16, ptr %t4069
  %t4071 = load %struct.I8x16, ptr %t4053
  %t4072 = icmp eq %struct.I8x16 %t4070, %t4071
  %t4073 = sext <16 x i1> %t4072 to <16 x i8>
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t4074)
  store %struct.I8x16 %t4073, ptr %t4074
  %t4075 = load %struct.I8x16, ptr %t4074
  %t4076 = call i32 @llvm.x86.sse2.pmovmskb.128(<16 x i8> %t4075)
  call void @llvm.lifetime.start.p0(i64 4, ptr %t4077)
  store i32 %t4076, ptr %t4077
  br label %loop.preheader1272
loop.preheader1272:
  br label %loop.header1273
loop.header1273:
  %t4078 = load i32, ptr %t4077
  %t4079 = icmp ne i32 %t4078, 0
  br i1 %t4079, label %loop.body1274, label %loop.exit1276
loop.body1274:
  %t4080 = load i32, ptr %t4077
  %t4081 = call i32 @tml_N4core3str4simd3ctzE_i(i32 %t4080)
  call void @llvm.lifetime.start.p0(i64 4, ptr %t4082)
  store i32 %t4081, ptr %t4082
  %t4083 = load %struct.List__I64, ptr %t4022
  %t4084 = load i64, ptr %t4054
  %t4085 = load i32, ptr %t4082
  %t4086 = sext i32 %t4085 to i64
  %t4088 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4084, i64 %t4086)
  %t4087 = extractvalue { i64, i1 } %t4088, 0
  %t4089 = extractvalue { i64, i1 } %t4088, 1
  br i1 %t4089, label %add_overflow1278, label %add_ok1277
add_overflow1278:
  call void @panic(ptr @.str.259)
  unreachable
add_ok1277:
  %t4090 = call {} @tml_N3std11collections4list9List__I649push__I64E(ptr %t4022, i64 %t4087)
  %t4091 = load i32, ptr %t4077
  %t4092 = load i32, ptr %t4077
  %t4094 = call { i32, i1 } @llvm.ssub.with.overflow.i32(i32 %t4092, i32 1)
  %t4093 = extractvalue { i32, i1 } %t4094, 0
  %t4095 = extractvalue { i32, i1 } %t4094, 1
  br i1 %t4095, label %sub_overflow1280, label %sub_ok1279
sub_overflow1280:
  call void @panic(ptr @.str.260)
  unreachable
sub_ok1279:
  %t4096 = and i32 %t4091, %t4093
  store i32 %t4096, ptr %t4077
  call void @llvm.lifetime.end.p0(i64 4, ptr %t4082)
  br label %loop.latch1275
loop.latch1275:
  br label %loop.header1273, !llvm.loop !1053
loop.exit1276:
  %t4097 = load i64, ptr %t4054
  %t4099 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4097, i64 16)
  %t4098 = extractvalue { i64, i1 } %t4099, 0
  %t4100 = extractvalue { i64, i1 } %t4099, 1
  br i1 %t4100, label %add_overflow1282, label %add_ok1281
add_overflow1282:
  call void @panic(ptr @.str.261)
  unreachable
add_ok1281:
  store i64 %t4098, ptr %t4054
  call void @llvm.lifetime.end.p0(i64 4, ptr %t4077)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t4074)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t4069)
  br label %loop.latch1266
loop.latch1266:
  br label %loop.header1264, !llvm.loop !1052
loop.exit1267:
  br label %loop.preheader1283
loop.preheader1283:
  br label %loop.header1284
loop.header1284:
  %t4101 = load i64, ptr %t4054
  %t4102 = load i64, ptr %t4016
  %t4103 = icmp slt i64 %t4101, %t4102
  br i1 %t4103, label %loop.body1285, label %loop.exit1287
loop.body1285:
  %t4104 = load i64, ptr %t4020
  %t4105 = load i64, ptr %t4054
  %t4107 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4104, i64 %t4105)
  %t4106 = extractvalue { i64, i1 } %t4107, 0
  %t4108 = extractvalue { i64, i1 } %t4107, 1
  br i1 %t4108, label %add_overflow1289, label %add_ok1288
add_overflow1289:
  call void @panic(ptr @.str.262)
  unreachable
add_ok1288:
  %t4109 = inttoptr i64 %t4106 to ptr
  %t4110 = alloca ptr
  store ptr %t4109, ptr %t4110
  %t4111 = load ptr, ptr %t4110
  %t4112 = load i8, ptr %t4111
  call void @llvm.lifetime.start.p0(i64 1, ptr %t4113)
  store i8 %t4112, ptr %t4113
  %t4114 = load i8, ptr %t4113
  %t4115 = load i8, ptr %t4013
  %t4116 = icmp eq i8 %t4114, %t4115
  br i1 %t4116, label %if.then1290, label %if.end1292
if.then1290:
  %t4117 = load %struct.List__I64, ptr %t4022
  %t4118 = load i64, ptr %t4054
  %t4119 = call {} @tml_N3std11collections4list9List__I649push__I64E(ptr %t4022, i64 %t4118)
  br label %if.end1292
if.end1292:
  %t4120 = load i64, ptr %t4054
  %t4122 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4120, i64 1)
  %t4121 = extractvalue { i64, i1 } %t4122, 0
  %t4123 = extractvalue { i64, i1 } %t4122, 1
  br i1 %t4123, label %add_overflow1294, label %add_ok1293
add_overflow1294:
  call void @panic(ptr @.str.263)
  unreachable
add_ok1293:
  store i64 %t4121, ptr %t4054
  call void @llvm.lifetime.end.p0(i64 1, ptr %t4113)
  br label %loop.latch1286
loop.latch1286:
  br label %loop.header1284, !llvm.loop !1054
loop.exit1287:
  %t4124 = load %struct.List__I64, ptr %t4022
  ret %struct.List__I64 %t4124
}

; core::str::simd::substring_raw
define internal ptr @tml_N4core3str4simd13substring_rawE_Sll(ptr %s, i64 %start, i64 %p_end) #0 {
entry:
  %t4125 = alloca ptr
  store ptr %s, ptr %t4125
  %t4126 = alloca i64
  store i64 %start, ptr %t4126
  %t4127 = alloca i64
  store i64 %p_end, ptr %t4127
  %t4133 = alloca i64
  %t4147 = alloca i64
  %t4128 = load i64, ptr %t4127
  %t4129 = load i64, ptr %t4126
  %t4131 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t4128, i64 %t4129)
  %t4130 = extractvalue { i64, i1 } %t4131, 0
  %t4132 = extractvalue { i64, i1 } %t4131, 1
  br i1 %t4132, label %sub_overflow1296, label %sub_ok1295
sub_overflow1296:
  call void @panic(ptr @.str.264)
  unreachable
sub_ok1295:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4133)
  store i64 %t4130, ptr %t4133
  %t4134 = load i64, ptr %t4133
  %t4136 = sext i32 0 to i64
  %t4135 = icmp sle i64 %t4134, %t4136
  br i1 %t4135, label %if.then1297, label %if.end1299
if.then1297:
  ret ptr @.str.1
if.end1299:
  %t4137 = load i64, ptr %t4133
  %t4139 = sext i32 1 to i64
  %t4140 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4137, i64 %t4139)
  %t4138 = extractvalue { i64, i1 } %t4140, 0
  %t4141 = extractvalue { i64, i1 } %t4140, 1
  br i1 %t4141, label %add_overflow1301, label %add_ok1300
add_overflow1301:
  call void @panic(ptr @.str.265)
  unreachable
add_ok1300:
  %t4142 = call ptr @mem_alloc(i64 %t4138)
  %t4143 = alloca ptr
  store ptr %t4142, ptr %t4143
  %t4144 = load ptr, ptr %t4125
  %t4146 = ptrtoint ptr %t4144 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4147)
  store i64 %t4146, ptr %t4147
  %t4148 = load i64, ptr %t4147
  %t4149 = load i64, ptr %t4126
  %t4151 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4148, i64 %t4149)
  %t4150 = extractvalue { i64, i1 } %t4151, 0
  %t4152 = extractvalue { i64, i1 } %t4151, 1
  br i1 %t4152, label %add_overflow1303, label %add_ok1302
add_overflow1303:
  call void @panic(ptr @.str.266)
  unreachable
add_ok1302:
  %t4153 = inttoptr i64 %t4150 to ptr
  %t4154 = load ptr, ptr %t4143
  %t4155 = load i64, ptr %t4133
  call void @llvm.memcpy.p0.p0.i64(ptr %t4154, ptr %t4153, i64 %t4155, i1 false)
  %t4156 = load ptr, ptr %t4143
  %t4158 = ptrtoint ptr %t4156 to i64
  %t4159 = load i64, ptr %t4133
  %t4161 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4158, i64 %t4159)
  %t4160 = extractvalue { i64, i1 } %t4161, 0
  %t4162 = extractvalue { i64, i1 } %t4161, 1
  br i1 %t4162, label %add_overflow1305, label %add_ok1304
add_overflow1305:
  call void @panic(ptr @.str.267)
  unreachable
add_ok1304:
  %t4163 = inttoptr i64 %t4160 to ptr
  %t4164 = alloca ptr
  store ptr %t4163, ptr %t4164
  %t4165 = load ptr, ptr %t4164
  %t4166 = trunc i32 0 to i8
  store i8 %t4166, ptr %t4165
  %t4167 = load ptr, ptr %t4143
  ret ptr %t4167
}

; core::str::simd::contains_fast
define i1 @tml_N4core3str4simd13contains_fastE_SS(ptr %s, ptr %pattern) #0 {
entry:
  %t4168 = alloca ptr
  store ptr %s, ptr %t4168
  %t4169 = alloca ptr
  store ptr %pattern, ptr %t4169
  %t4172 = alloca i64
  %t4175 = alloca i64
  %t4191 = alloca i64
  %t4199 = alloca i64
  %t4202 = alloca i8
  %t4205 = alloca %struct.I8x16
  %t4206 = alloca i64
  %t4225 = alloca %struct.I8x16
  %t4230 = alloca %struct.I8x16
  %t4233 = alloca i32
  %t4238 = alloca i32
  %t4245 = alloca i64
  %t4170 = load ptr, ptr %t4168
  %t4171 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t4170)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4172)
  store i64 %t4171, ptr %t4172
  %t4173 = load ptr, ptr %t4169
  %t4174 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t4173)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4175)
  store i64 %t4174, ptr %t4175
  %t4176 = load i64, ptr %t4175
  %t4178 = sext i32 0 to i64
  %t4177 = icmp eq i64 %t4176, %t4178
  br i1 %t4177, label %if.then1306, label %if.end1308
if.then1306:
  ret i1 1
if.end1308:
  %t4179 = load i64, ptr %t4175
  %t4180 = load i64, ptr %t4172
  %t4181 = icmp sgt i64 %t4179, %t4180
  br i1 %t4181, label %if.then1309, label %if.end1311
if.then1309:
  ret i1 0
if.end1311:
  %t4182 = load i64, ptr %t4172
  %t4184 = sext i32 32 to i64
  %t4183 = icmp slt i64 %t4182, %t4184
  br i1 %t4183, label %if.then1312, label %if.end1314
if.then1312:
  %t4185 = load ptr, ptr %t4168
  %t4186 = load ptr, ptr %t4169
  %t4187 = call i1 @tml_N4core3str4simd15contains_scalarE_SS(ptr %t4185, ptr %t4186)
  ret i1 %t4187
if.end1314:
  %t4188 = load ptr, ptr %t4168
  %t4190 = ptrtoint ptr %t4188 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4191)
  store i64 %t4190, ptr %t4191
  %t4192 = load ptr, ptr %t4169
  %t4193 = alloca ptr
  store ptr %t4192, ptr %t4193
  %t4194 = load i64, ptr %t4172
  %t4195 = load i64, ptr %t4175
  %t4197 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t4194, i64 %t4195)
  %t4196 = extractvalue { i64, i1 } %t4197, 0
  %t4198 = extractvalue { i64, i1 } %t4197, 1
  br i1 %t4198, label %sub_overflow1316, label %sub_ok1315
sub_overflow1316:
  call void @panic(ptr @.str.268)
  unreachable
sub_ok1315:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4199)
  store i64 %t4196, ptr %t4199
  %t4200 = load ptr, ptr %t4193
  %t4201 = load i8, ptr %t4200
  call void @llvm.lifetime.start.p0(i64 1, ptr %t4202)
  store i8 %t4201, ptr %t4202
  %t4203 = load i8, ptr %t4202
  %t4204 = call %struct.I8x16 @tml_N4core4simd5i8x165I8x165splatE(i8 %t4203)
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t4205)
  store %struct.I8x16 %t4204, ptr %t4205
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4206)
  store i64 0, ptr %t4206
  br label %loop.preheader1317
loop.preheader1317:
  br label %loop.header1318
loop.header1318:
  %t4207 = load i64, ptr %t4206
  %t4209 = sext i32 16 to i64
  %t4210 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4207, i64 %t4209)
  %t4208 = extractvalue { i64, i1 } %t4210, 0
  %t4211 = extractvalue { i64, i1 } %t4210, 1
  br i1 %t4211, label %add_overflow1323, label %add_ok1322
add_overflow1323:
  call void @panic(ptr @.str.269)
  unreachable
add_ok1322:
  %t4212 = load i64, ptr %t4172
  %t4213 = icmp sle i64 %t4208, %t4212
  %t4214 = load i64, ptr %t4206
  %t4215 = load i64, ptr %t4199
  %t4216 = icmp sle i64 %t4214, %t4215
  %t4217 = and i1 %t4213, %t4216
  br i1 %t4217, label %loop.body1319, label %loop.exit1321
loop.body1319:
  %t4218 = load i64, ptr %t4191
  %t4219 = load i64, ptr %t4206
  %t4221 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4218, i64 %t4219)
  %t4220 = extractvalue { i64, i1 } %t4221, 0
  %t4222 = extractvalue { i64, i1 } %t4221, 1
  br i1 %t4222, label %add_overflow1325, label %add_ok1324
add_overflow1325:
  call void @panic(ptr @.str.200)
  unreachable
add_ok1324:
  %t4223 = inttoptr i64 %t4220 to ptr
  %t4224 = load <16 x i8>, ptr %t4223, align 1
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t4225)
  store %struct.I8x16 %t4224, ptr %t4225
  %t4226 = load %struct.I8x16, ptr %t4225
  %t4227 = load %struct.I8x16, ptr %t4205
  %t4228 = icmp eq %struct.I8x16 %t4226, %t4227
  %t4229 = sext <16 x i1> %t4228 to <16 x i8>
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t4230)
  store %struct.I8x16 %t4229, ptr %t4230
  %t4231 = load %struct.I8x16, ptr %t4230
  %t4232 = call i32 @llvm.x86.sse2.pmovmskb.128(<16 x i8> %t4231)
  call void @llvm.lifetime.start.p0(i64 4, ptr %t4233)
  store i32 %t4232, ptr %t4233
  br label %loop.preheader1326
loop.preheader1326:
  br label %loop.header1327
loop.header1327:
  %t4234 = load i32, ptr %t4233
  %t4235 = icmp ne i32 %t4234, 0
  br i1 %t4235, label %loop.body1328, label %loop.exit1330
loop.body1328:
  %t4236 = load i32, ptr %t4233
  %t4237 = call i32 @tml_N4core3str4simd3ctzE_i(i32 %t4236)
  call void @llvm.lifetime.start.p0(i64 4, ptr %t4238)
  store i32 %t4237, ptr %t4238
  %t4239 = load i64, ptr %t4206
  %t4240 = load i32, ptr %t4238
  %t4241 = sext i32 %t4240 to i64
  %t4243 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4239, i64 %t4241)
  %t4242 = extractvalue { i64, i1 } %t4243, 0
  %t4244 = extractvalue { i64, i1 } %t4243, 1
  br i1 %t4244, label %add_overflow1332, label %add_ok1331
add_overflow1332:
  call void @panic(ptr @.str.270)
  unreachable
add_ok1331:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4245)
  store i64 %t4242, ptr %t4245
  %t4246 = load i64, ptr %t4245
  %t4247 = load i64, ptr %t4199
  %t4248 = icmp sle i64 %t4246, %t4247
  br i1 %t4248, label %if.then1333, label %if.end1335
if.then1333:
  %t4249 = load i64, ptr %t4191
  %t4250 = load i64, ptr %t4245
  %t4252 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4249, i64 %t4250)
  %t4251 = extractvalue { i64, i1 } %t4252, 0
  %t4253 = extractvalue { i64, i1 } %t4252, 1
  br i1 %t4253, label %add_overflow1337, label %add_ok1336
add_overflow1337:
  call void @panic(ptr @.str.271)
  unreachable
add_ok1336:
  %t4254 = inttoptr i64 %t4251 to ptr
  %t4255 = load ptr, ptr %t4193
  %t4256 = load i64, ptr %t4175
  %t4257 = call i32 @memcmp(ptr %t4254, ptr %t4255, i64 %t4256)
  %t4258 = icmp eq i32 %t4257, 0
  br i1 %t4258, label %if.then1338, label %if.end1340
if.then1338:
  call void @llvm.lifetime.end.p0(i64 8, ptr %t4245)
  call void @llvm.lifetime.end.p0(i64 4, ptr %t4238)
  call void @llvm.lifetime.end.p0(i64 4, ptr %t4233)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t4230)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t4225)
  ret i1 1
if.end1340:
  br label %if.end1335
if.end1335:
  %t4259 = load i32, ptr %t4233
  %t4260 = load i32, ptr %t4233
  %t4262 = call { i32, i1 } @llvm.ssub.with.overflow.i32(i32 %t4260, i32 1)
  %t4261 = extractvalue { i32, i1 } %t4262, 0
  %t4263 = extractvalue { i32, i1 } %t4262, 1
  br i1 %t4263, label %sub_overflow1342, label %sub_ok1341
sub_overflow1342:
  call void @panic(ptr @.str.239)
  unreachable
sub_ok1341:
  %t4264 = and i32 %t4259, %t4261
  store i32 %t4264, ptr %t4233
  call void @llvm.lifetime.end.p0(i64 8, ptr %t4245)
  call void @llvm.lifetime.end.p0(i64 4, ptr %t4238)
  br label %loop.latch1329
loop.latch1329:
  br label %loop.header1327, !llvm.loop !1056
loop.exit1330:
  %t4265 = load i64, ptr %t4206
  %t4267 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4265, i64 16)
  %t4266 = extractvalue { i64, i1 } %t4267, 0
  %t4268 = extractvalue { i64, i1 } %t4267, 1
  br i1 %t4268, label %add_overflow1344, label %add_ok1343
add_overflow1344:
  call void @panic(ptr @.str.272)
  unreachable
add_ok1343:
  store i64 %t4266, ptr %t4206
  call void @llvm.lifetime.end.p0(i64 4, ptr %t4233)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t4230)
  call void @llvm.lifetime.end.p0(i64 -1, ptr %t4225)
  br label %loop.latch1320
loop.latch1320:
  br label %loop.header1318, !llvm.loop !1055
loop.exit1321:
  br label %loop.preheader1345
loop.preheader1345:
  br label %loop.header1346
loop.header1346:
  %t4269 = load i64, ptr %t4206
  %t4270 = load i64, ptr %t4199
  %t4271 = icmp sle i64 %t4269, %t4270
  br i1 %t4271, label %loop.body1347, label %loop.exit1349
loop.body1347:
  %t4272 = load i64, ptr %t4191
  %t4273 = load i64, ptr %t4206
  %t4275 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4272, i64 %t4273)
  %t4274 = extractvalue { i64, i1 } %t4275, 0
  %t4276 = extractvalue { i64, i1 } %t4275, 1
  br i1 %t4276, label %add_overflow1351, label %add_ok1350
add_overflow1351:
  call void @panic(ptr @.str.273)
  unreachable
add_ok1350:
  %t4277 = inttoptr i64 %t4274 to ptr
  %t4278 = load ptr, ptr %t4193
  %t4279 = load i64, ptr %t4175
  %t4280 = call i32 @memcmp(ptr %t4277, ptr %t4278, i64 %t4279)
  %t4281 = icmp eq i32 %t4280, 0
  br i1 %t4281, label %if.then1352, label %if.end1354
if.then1352:
  ret i1 1
if.end1354:
  %t4282 = load i64, ptr %t4206
  %t4284 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4282, i64 1)
  %t4283 = extractvalue { i64, i1 } %t4284, 0
  %t4285 = extractvalue { i64, i1 } %t4284, 1
  br i1 %t4285, label %add_overflow1356, label %add_ok1355
add_overflow1356:
  call void @panic(ptr @.str.274)
  unreachable
add_ok1355:
  store i64 %t4283, ptr %t4206
  br label %loop.latch1348
loop.latch1348:
  br label %loop.header1346, !llvm.loop !1057
loop.exit1349:
  ret i1 0
}
; DEBUG LAZY type_name=I8x16 method=zero

define internal %struct.I8x16 @tml_N4core4simd5i8x165I8x164zeroE() #0 {
entry:
  %t4286 = trunc i32 0 to i8
  %t4287 = call %struct.I8x16 @tml_N4core4simd5i8x165I8x165splatE(i8 %t4286)
  ret %struct.I8x16 %t4287
}

; core::str::simd::contains_scalar
define i1 @tml_N4core3str4simd15contains_scalarE_SS(ptr %s, ptr %pattern) #0 {
entry:
  %t4288 = alloca ptr
  store ptr %s, ptr %t4288
  %t4289 = alloca ptr
  store ptr %pattern, ptr %t4289
  %t4292 = alloca i64
  %t4295 = alloca i64
  %t4305 = alloca i64
  %t4313 = alloca i64
  %t4314 = alloca i64
  %t4290 = load ptr, ptr %t4288
  %t4291 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t4290)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4292)
  store i64 %t4291, ptr %t4292
  %t4293 = load ptr, ptr %t4289
  %t4294 = call i64 @tml_N4core3str5basic3lenE_S(ptr %t4293)
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4295)
  store i64 %t4294, ptr %t4295
  %t4296 = load i64, ptr %t4295
  %t4298 = sext i32 0 to i64
  %t4297 = icmp eq i64 %t4296, %t4298
  br i1 %t4297, label %if.then1357, label %if.end1359
if.then1357:
  ret i1 1
if.end1359:
  %t4299 = load i64, ptr %t4295
  %t4300 = load i64, ptr %t4292
  %t4301 = icmp sgt i64 %t4299, %t4300
  br i1 %t4301, label %if.then1360, label %if.end1362
if.then1360:
  ret i1 0
if.end1362:
  %t4302 = load ptr, ptr %t4288
  %t4304 = ptrtoint ptr %t4302 to i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4305)
  store i64 %t4304, ptr %t4305
  %t4306 = load ptr, ptr %t4289
  %t4307 = alloca ptr
  store ptr %t4306, ptr %t4307
  %t4308 = load i64, ptr %t4292
  %t4309 = load i64, ptr %t4295
  %t4311 = call { i64, i1 } @llvm.ssub.with.overflow.i64(i64 %t4308, i64 %t4309)
  %t4310 = extractvalue { i64, i1 } %t4311, 0
  %t4312 = extractvalue { i64, i1 } %t4311, 1
  br i1 %t4312, label %sub_overflow1364, label %sub_ok1363
sub_overflow1364:
  call void @panic(ptr @.str.275)
  unreachable
sub_ok1363:
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4313)
  store i64 %t4310, ptr %t4313
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4314)
  store i64 0, ptr %t4314
  br label %loop.preheader1365
loop.preheader1365:
  br label %loop.header1366
loop.header1366:
  %t4315 = load i64, ptr %t4314
  %t4316 = load i64, ptr %t4313
  %t4317 = icmp sle i64 %t4315, %t4316
  br i1 %t4317, label %loop.body1367, label %loop.exit1369
loop.body1367:
  %t4318 = load i64, ptr %t4305
  %t4319 = load i64, ptr %t4314
  %t4321 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4318, i64 %t4319)
  %t4320 = extractvalue { i64, i1 } %t4321, 0
  %t4322 = extractvalue { i64, i1 } %t4321, 1
  br i1 %t4322, label %add_overflow1371, label %add_ok1370
add_overflow1371:
  call void @panic(ptr @.str.276)
  unreachable
add_ok1370:
  %t4323 = inttoptr i64 %t4320 to ptr
  %t4324 = load ptr, ptr %t4307
  %t4325 = load i64, ptr %t4295
  %t4326 = call i32 @memcmp(ptr %t4323, ptr %t4324, i64 %t4325)
  %t4327 = icmp eq i32 %t4326, 0
  br i1 %t4327, label %if.then1372, label %if.end1374
if.then1372:
  ret i1 1
if.end1374:
  %t4328 = load i64, ptr %t4314
  %t4330 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4328, i64 1)
  %t4329 = extractvalue { i64, i1 } %t4330, 0
  %t4331 = extractvalue { i64, i1 } %t4330, 1
  br i1 %t4331, label %add_overflow1376, label %add_ok1375
add_overflow1376:
  call void @panic(ptr @.str.277)
  unreachable
add_ok1375:
  store i64 %t4329, ptr %t4314
  br label %loop.latch1368
loop.latch1368:
  br label %loop.header1366, !llvm.loop !1058
loop.exit1369:
  ret i1 0
}

; Lazy transitive instantiations (round 0)

define internal void @tml_N3std11collections4list9List__Str4dropE(ptr %this) #0 {
entry:
  %t4332 = call {} @tml_N3std11collections4list9List__Str7destroyE(ptr %this)
  ret void
}

define internal %struct.List__IrFunction @tml_N3std11collections4list16List__IrFunction3newE(i64 %initial_capacity) #0 {
entry:
  %t4333 = alloca i64
  store i64 %initial_capacity, ptr %t4333
  %t4334 = load i64, ptr %t4333
  %t4335 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4335)
  store i64 %t4334, ptr %t4335
  %t4336 = load i64, ptr %t4335
  %t4338 = sext i32 4 to i64
  %t4337 = icmp slt i64 %t4336, %t4338
  br i1 %t4337, label %if.then1377, label %if.end1379
if.then1377:
  store i64 4, ptr %t4335
  br label %if.end1379
if.end1379:
  %t4339 = getelementptr inbounds %struct.IrFunction, ptr null, i32 1
  %t4340 = ptrtoint ptr %t4339 to i64
  %t4341 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4341)
  store i64 %t4340, ptr %t4341
  %t4342 = load i64, ptr %t4341
  %t4343 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4343)
  store i64 %t4342, ptr %t4343
  %t4344 = load i64, ptr %t4343
  %t4346 = sext i32 8 to i64
  %t4345 = icmp slt i64 %t4344, %t4346
  br i1 %t4345, label %if.then1380, label %if.end1382
if.then1380:
  store i64 8, ptr %t4343
  br label %if.end1382
if.end1382:
  %t4347 = call ptr @mem_alloc(i64 32)
  %t4348 = alloca ptr
  store ptr %t4347, ptr %t4348
  %t4349 = load ptr, ptr %t4348
  %t4351 = ptrtoint ptr %t4349 to i64
  %t4352 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4352)
  store i64 %t4351, ptr %t4352
  %t4353 = load i64, ptr %t4335
  %t4354 = load i64, ptr %t4343
  %t4356 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t4353, i64 %t4354)
  %t4355 = extractvalue { i64, i1 } %t4356, 0
  %t4357 = extractvalue { i64, i1 } %t4356, 1
  br i1 %t4357, label %mul_overflow1384, label %mul_ok1383
mul_overflow1384:
  call void @panic(ptr @.str.278)
  unreachable
mul_ok1383:
  %t4358 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4358)
  store i64 %t4355, ptr %t4358
  %t4359 = load i64, ptr %t4358
  %t4360 = call ptr @mem_alloc(i64 %t4359)
  %t4361 = alloca ptr
  store ptr %t4360, ptr %t4361
  %t4362 = load i64, ptr %t4352
  %t4363 = inttoptr i64 %t4362 to ptr
  %t4364 = alloca ptr
  store ptr %t4363, ptr %t4364
  %t4365 = load ptr, ptr %t4364
  %t4366 = load ptr, ptr %t4361
  %t4368 = ptrtoint ptr %t4366 to i64
  store i64 %t4368, ptr %t4365
  %t4369 = load i64, ptr %t4352
  %t4371 = sext i32 8 to i64
  %t4372 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4369, i64 %t4371)
  %t4370 = extractvalue { i64, i1 } %t4372, 0
  %t4373 = extractvalue { i64, i1 } %t4372, 1
  br i1 %t4373, label %add_overflow1386, label %add_ok1385
add_overflow1386:
  call void @panic(ptr @.str.208)
  unreachable
add_ok1385:
  %t4374 = inttoptr i64 %t4370 to ptr
  %t4375 = alloca ptr
  store ptr %t4374, ptr %t4375
  %t4376 = load ptr, ptr %t4375
  %t4377 = sext i32 0 to i64
  store i64 %t4377, ptr %t4376
  %t4378 = load i64, ptr %t4352
  %t4380 = sext i32 16 to i64
  %t4381 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4378, i64 %t4380)
  %t4379 = extractvalue { i64, i1 } %t4381, 0
  %t4382 = extractvalue { i64, i1 } %t4381, 1
  br i1 %t4382, label %add_overflow1388, label %add_ok1387
add_overflow1388:
  call void @panic(ptr @.str.109)
  unreachable
add_ok1387:
  %t4383 = inttoptr i64 %t4379 to ptr
  %t4384 = alloca ptr
  store ptr %t4383, ptr %t4384
  %t4385 = load ptr, ptr %t4384
  %t4386 = load i64, ptr %t4335
  store i64 %t4386, ptr %t4385
  %t4387 = load i64, ptr %t4352
  %t4389 = sext i32 24 to i64
  %t4390 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4387, i64 %t4389)
  %t4388 = extractvalue { i64, i1 } %t4390, 0
  %t4391 = extractvalue { i64, i1 } %t4390, 1
  br i1 %t4391, label %add_overflow1390, label %add_ok1389
add_overflow1390:
  call void @panic(ptr @.str.279)
  unreachable
add_ok1389:
  %t4392 = inttoptr i64 %t4388 to ptr
  %t4393 = alloca ptr
  store ptr %t4392, ptr %t4393
  %t4394 = load ptr, ptr %t4393
  %t4395 = load i64, ptr %t4343
  store i64 %t4395, ptr %t4394
  %t4396 = alloca %struct.List__IrFunction
  %t4397 = load ptr, ptr %t4348
  %t4398 = getelementptr inbounds %struct.List__IrFunction, ptr %t4396, i32 0, i32 0
  store ptr %t4397, ptr %t4398
  %t4399 = load %struct.List__IrFunction, ptr %t4396
  ret %struct.List__IrFunction %t4399
}

define internal void @tml_N3std11collections4list16List__IrFunction4dropE(ptr %this) #0 {
entry:
  %t4400 = call {} @tml_N3std11collections4list16List__IrFunction7destroyE(ptr %this)
  ret void
}

define internal %struct.List__IrGlobal @tml_N3std11collections4list14List__IrGlobal3newE(i64 %initial_capacity) #0 {
entry:
  %t4401 = alloca i64
  store i64 %initial_capacity, ptr %t4401
  %t4402 = load i64, ptr %t4401
  %t4403 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4403)
  store i64 %t4402, ptr %t4403
  %t4404 = load i64, ptr %t4403
  %t4406 = sext i32 4 to i64
  %t4405 = icmp slt i64 %t4404, %t4406
  br i1 %t4405, label %if.then1391, label %if.end1393
if.then1391:
  store i64 4, ptr %t4403
  br label %if.end1393
if.end1393:
  %t4407 = getelementptr inbounds %struct.IrGlobal, ptr null, i32 1
  %t4408 = ptrtoint ptr %t4407 to i64
  %t4409 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4409)
  store i64 %t4408, ptr %t4409
  %t4410 = load i64, ptr %t4409
  %t4411 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4411)
  store i64 %t4410, ptr %t4411
  %t4412 = load i64, ptr %t4411
  %t4414 = sext i32 8 to i64
  %t4413 = icmp slt i64 %t4412, %t4414
  br i1 %t4413, label %if.then1394, label %if.end1396
if.then1394:
  store i64 8, ptr %t4411
  br label %if.end1396
if.end1396:
  %t4415 = call ptr @mem_alloc(i64 32)
  %t4416 = alloca ptr
  store ptr %t4415, ptr %t4416
  %t4417 = load ptr, ptr %t4416
  %t4419 = ptrtoint ptr %t4417 to i64
  %t4420 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4420)
  store i64 %t4419, ptr %t4420
  %t4421 = load i64, ptr %t4403
  %t4422 = load i64, ptr %t4411
  %t4424 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t4421, i64 %t4422)
  %t4423 = extractvalue { i64, i1 } %t4424, 0
  %t4425 = extractvalue { i64, i1 } %t4424, 1
  br i1 %t4425, label %mul_overflow1398, label %mul_ok1397
mul_overflow1398:
  call void @panic(ptr @.str.278)
  unreachable
mul_ok1397:
  %t4426 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4426)
  store i64 %t4423, ptr %t4426
  %t4427 = load i64, ptr %t4426
  %t4428 = call ptr @mem_alloc(i64 %t4427)
  %t4429 = alloca ptr
  store ptr %t4428, ptr %t4429
  %t4430 = load i64, ptr %t4420
  %t4431 = inttoptr i64 %t4430 to ptr
  %t4432 = alloca ptr
  store ptr %t4431, ptr %t4432
  %t4433 = load ptr, ptr %t4432
  %t4434 = load ptr, ptr %t4429
  %t4436 = ptrtoint ptr %t4434 to i64
  store i64 %t4436, ptr %t4433
  %t4437 = load i64, ptr %t4420
  %t4439 = sext i32 8 to i64
  %t4440 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4437, i64 %t4439)
  %t4438 = extractvalue { i64, i1 } %t4440, 0
  %t4441 = extractvalue { i64, i1 } %t4440, 1
  br i1 %t4441, label %add_overflow1400, label %add_ok1399
add_overflow1400:
  call void @panic(ptr @.str.208)
  unreachable
add_ok1399:
  %t4442 = inttoptr i64 %t4438 to ptr
  %t4443 = alloca ptr
  store ptr %t4442, ptr %t4443
  %t4444 = load ptr, ptr %t4443
  %t4445 = sext i32 0 to i64
  store i64 %t4445, ptr %t4444
  %t4446 = load i64, ptr %t4420
  %t4448 = sext i32 16 to i64
  %t4449 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4446, i64 %t4448)
  %t4447 = extractvalue { i64, i1 } %t4449, 0
  %t4450 = extractvalue { i64, i1 } %t4449, 1
  br i1 %t4450, label %add_overflow1402, label %add_ok1401
add_overflow1402:
  call void @panic(ptr @.str.109)
  unreachable
add_ok1401:
  %t4451 = inttoptr i64 %t4447 to ptr
  %t4452 = alloca ptr
  store ptr %t4451, ptr %t4452
  %t4453 = load ptr, ptr %t4452
  %t4454 = load i64, ptr %t4403
  store i64 %t4454, ptr %t4453
  %t4455 = load i64, ptr %t4420
  %t4457 = sext i32 24 to i64
  %t4458 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4455, i64 %t4457)
  %t4456 = extractvalue { i64, i1 } %t4458, 0
  %t4459 = extractvalue { i64, i1 } %t4458, 1
  br i1 %t4459, label %add_overflow1404, label %add_ok1403
add_overflow1404:
  call void @panic(ptr @.str.279)
  unreachable
add_ok1403:
  %t4460 = inttoptr i64 %t4456 to ptr
  %t4461 = alloca ptr
  store ptr %t4460, ptr %t4461
  %t4462 = load ptr, ptr %t4461
  %t4463 = load i64, ptr %t4411
  store i64 %t4463, ptr %t4462
  %t4464 = alloca %struct.List__IrGlobal
  %t4465 = load ptr, ptr %t4416
  %t4466 = getelementptr inbounds %struct.List__IrGlobal, ptr %t4464, i32 0, i32 0
  store ptr %t4465, ptr %t4466
  %t4467 = load %struct.List__IrGlobal, ptr %t4464
  ret %struct.List__IrGlobal %t4467
}

define internal void @tml_N3std11collections4list14List__IrGlobal4dropE(ptr %this) #0 {
entry:
  %t4468 = call {} @tml_N3std11collections4list14List__IrGlobal7destroyE(ptr %this)
  ret void
}

define internal %struct.List__Str @tml_N3std11collections4list9List__Str3newE(i64 %initial_capacity) #0 {
entry:
  %t4469 = alloca i64
  store i64 %initial_capacity, ptr %t4469
  %t4470 = load i64, ptr %t4469
  %t4471 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4471)
  store i64 %t4470, ptr %t4471
  %t4472 = load i64, ptr %t4471
  %t4474 = sext i32 4 to i64
  %t4473 = icmp slt i64 %t4472, %t4474
  br i1 %t4473, label %if.then1405, label %if.end1407
if.then1405:
  store i64 4, ptr %t4471
  br label %if.end1407
if.end1407:
  %t4475 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4475)
  store i64 8, ptr %t4475
  %t4476 = load i64, ptr %t4475
  %t4477 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4477)
  store i64 %t4476, ptr %t4477
  %t4478 = load i64, ptr %t4477
  %t4480 = sext i32 8 to i64
  %t4479 = icmp slt i64 %t4478, %t4480
  br i1 %t4479, label %if.then1408, label %if.end1410
if.then1408:
  store i64 8, ptr %t4477
  br label %if.end1410
if.end1410:
  %t4481 = call ptr @mem_alloc(i64 32)
  %t4482 = alloca ptr
  store ptr %t4481, ptr %t4482
  %t4483 = load ptr, ptr %t4482
  %t4485 = ptrtoint ptr %t4483 to i64
  %t4486 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4486)
  store i64 %t4485, ptr %t4486
  %t4487 = load i64, ptr %t4471
  %t4488 = load i64, ptr %t4477
  %t4490 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t4487, i64 %t4488)
  %t4489 = extractvalue { i64, i1 } %t4490, 0
  %t4491 = extractvalue { i64, i1 } %t4490, 1
  br i1 %t4491, label %mul_overflow1412, label %mul_ok1411
mul_overflow1412:
  call void @panic(ptr @.str.278)
  unreachable
mul_ok1411:
  %t4492 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4492)
  store i64 %t4489, ptr %t4492
  %t4493 = load i64, ptr %t4492
  %t4494 = call ptr @mem_alloc(i64 %t4493)
  %t4495 = alloca ptr
  store ptr %t4494, ptr %t4495
  %t4496 = load i64, ptr %t4486
  %t4497 = inttoptr i64 %t4496 to ptr
  %t4498 = alloca ptr
  store ptr %t4497, ptr %t4498
  %t4499 = load ptr, ptr %t4498
  %t4500 = load ptr, ptr %t4495
  %t4502 = ptrtoint ptr %t4500 to i64
  store i64 %t4502, ptr %t4499
  %t4503 = load i64, ptr %t4486
  %t4505 = sext i32 8 to i64
  %t4506 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4503, i64 %t4505)
  %t4504 = extractvalue { i64, i1 } %t4506, 0
  %t4507 = extractvalue { i64, i1 } %t4506, 1
  br i1 %t4507, label %add_overflow1414, label %add_ok1413
add_overflow1414:
  call void @panic(ptr @.str.208)
  unreachable
add_ok1413:
  %t4508 = inttoptr i64 %t4504 to ptr
  %t4509 = alloca ptr
  store ptr %t4508, ptr %t4509
  %t4510 = load ptr, ptr %t4509
  %t4511 = sext i32 0 to i64
  store i64 %t4511, ptr %t4510
  %t4512 = load i64, ptr %t4486
  %t4514 = sext i32 16 to i64
  %t4515 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4512, i64 %t4514)
  %t4513 = extractvalue { i64, i1 } %t4515, 0
  %t4516 = extractvalue { i64, i1 } %t4515, 1
  br i1 %t4516, label %add_overflow1416, label %add_ok1415
add_overflow1416:
  call void @panic(ptr @.str.109)
  unreachable
add_ok1415:
  %t4517 = inttoptr i64 %t4513 to ptr
  %t4518 = alloca ptr
  store ptr %t4517, ptr %t4518
  %t4519 = load ptr, ptr %t4518
  %t4520 = load i64, ptr %t4471
  store i64 %t4520, ptr %t4519
  %t4521 = load i64, ptr %t4486
  %t4523 = sext i32 24 to i64
  %t4524 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4521, i64 %t4523)
  %t4522 = extractvalue { i64, i1 } %t4524, 0
  %t4525 = extractvalue { i64, i1 } %t4524, 1
  br i1 %t4525, label %add_overflow1418, label %add_ok1417
add_overflow1418:
  call void @panic(ptr @.str.279)
  unreachable
add_ok1417:
  %t4526 = inttoptr i64 %t4522 to ptr
  %t4527 = alloca ptr
  store ptr %t4526, ptr %t4527
  %t4528 = load ptr, ptr %t4527
  %t4529 = load i64, ptr %t4477
  store i64 %t4529, ptr %t4528
  %t4530 = alloca %struct.List__Str
  %t4531 = load ptr, ptr %t4482
  %t4532 = getelementptr inbounds %struct.List__Str, ptr %t4530, i32 0, i32 0
  store ptr %t4531, ptr %t4532
  %t4533 = load %struct.List__Str, ptr %t4530
  ret %struct.List__Str %t4533
}

define internal i64 @tml_N3std11collections4list9List__Str3lenE(ptr %this) #0 {
entry:
  %t4534 = getelementptr inbounds %struct.List__Str, ptr %this, i32 0, i32 0
  %t4535 = load ptr, ptr %t4534
  %t4537 = ptrtoint ptr %t4535 to i64
  %t4538 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4538)
  store i64 %t4537, ptr %t4538
  %t4539 = load i64, ptr %t4538
  %t4541 = sext i32 8 to i64
  %t4542 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4539, i64 %t4541)
  %t4540 = extractvalue { i64, i1 } %t4542, 0
  %t4543 = extractvalue { i64, i1 } %t4542, 1
  br i1 %t4543, label %add_overflow1420, label %add_ok1419
add_overflow1420:
  call void @panic(ptr @.str.280)
  unreachable
add_ok1419:
  %t4544 = inttoptr i64 %t4540 to ptr
  %t4545 = alloca ptr
  store ptr %t4544, ptr %t4545
  %t4546 = load ptr, ptr %t4545
  %t4547 = load i64, ptr %t4546
  ret i64 %t4547
}

define internal ptr @tml_N3std11collections4list9List__Str3getE(ptr %this, i64 %index) #0 {
entry:
  %t4548 = alloca i64
  store i64 %index, ptr %t4548
  %t4549 = getelementptr inbounds %struct.List__Str, ptr %this, i32 0, i32 0
  %t4550 = load ptr, ptr %t4549
  %t4552 = ptrtoint ptr %t4550 to i64
  %t4553 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4553)
  store i64 %t4552, ptr %t4553
  %t4554 = load i64, ptr %t4553
  %t4556 = sext i32 24 to i64
  %t4557 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4554, i64 %t4556)
  %t4555 = extractvalue { i64, i1 } %t4557, 0
  %t4558 = extractvalue { i64, i1 } %t4557, 1
  br i1 %t4558, label %add_overflow1422, label %add_ok1421
add_overflow1422:
  call void @panic(ptr @.str.281)
  unreachable
add_ok1421:
  %t4559 = inttoptr i64 %t4555 to ptr
  %t4560 = alloca ptr
  store ptr %t4559, ptr %t4560
  %t4561 = load ptr, ptr %t4560
  %t4562 = load i64, ptr %t4561
  %t4563 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4563)
  store i64 %t4562, ptr %t4563
  %t4564 = load i64, ptr %t4553
  %t4565 = inttoptr i64 %t4564 to ptr
  %t4566 = alloca ptr
  store ptr %t4565, ptr %t4566
  %t4567 = load ptr, ptr %t4566
  %t4568 = load i64, ptr %t4567
  %t4569 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4569)
  store i64 %t4568, ptr %t4569
  %t4570 = load i64, ptr %t4569
  %t4571 = load i64, ptr %t4548
  %t4572 = load i64, ptr %t4563
  %t4574 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t4571, i64 %t4572)
  %t4573 = extractvalue { i64, i1 } %t4574, 0
  %t4575 = extractvalue { i64, i1 } %t4574, 1
  br i1 %t4575, label %mul_overflow1424, label %mul_ok1423
mul_overflow1424:
  call void @panic(ptr @.str.282)
  unreachable
mul_ok1423:
  %t4577 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4570, i64 %t4573)
  %t4576 = extractvalue { i64, i1 } %t4577, 0
  %t4578 = extractvalue { i64, i1 } %t4577, 1
  br i1 %t4578, label %add_overflow1426, label %add_ok1425
add_overflow1426:
  call void @panic(ptr @.str.283)
  unreachable
add_ok1425:
  %t4579 = inttoptr i64 %t4576 to ptr
  %t4580 = alloca ptr
  store ptr %t4579, ptr %t4580
  %t4581 = load ptr, ptr %t4580
  %t4582 = load ptr, ptr %t4581
  %t4583 = alloca ptr
  store ptr %t4582, ptr %t4583
  %t4584 = load ptr, ptr %t4583
  ret ptr %t4584
}

define internal void @tml_N3std11collections4list9List__Str9push__StrE(ptr %this, ptr %value) #0 {
entry:
  %t4585 = alloca ptr
  store ptr %value, ptr %t4585
  %t4586 = getelementptr inbounds %struct.List__Str, ptr %this, i32 0, i32 0
  %t4587 = load ptr, ptr %t4586
  %t4589 = ptrtoint ptr %t4587 to i64
  %t4590 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4590)
  store i64 %t4589, ptr %t4590
  %t4591 = load i64, ptr %t4590
  %t4593 = sext i32 8 to i64
  %t4594 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4591, i64 %t4593)
  %t4592 = extractvalue { i64, i1 } %t4594, 0
  %t4595 = extractvalue { i64, i1 } %t4594, 1
  br i1 %t4595, label %add_overflow1428, label %add_ok1427
add_overflow1428:
  call void @panic(ptr @.str.284)
  unreachable
add_ok1427:
  %t4596 = inttoptr i64 %t4592 to ptr
  %t4597 = alloca ptr
  store ptr %t4596, ptr %t4597
  %t4598 = load i64, ptr %t4590
  %t4600 = sext i32 16 to i64
  %t4601 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4598, i64 %t4600)
  %t4599 = extractvalue { i64, i1 } %t4601, 0
  %t4602 = extractvalue { i64, i1 } %t4601, 1
  br i1 %t4602, label %add_overflow1430, label %add_ok1429
add_overflow1430:
  call void @panic(ptr @.str.285)
  unreachable
add_ok1429:
  %t4603 = inttoptr i64 %t4599 to ptr
  %t4604 = alloca ptr
  store ptr %t4603, ptr %t4604
  %t4605 = load i64, ptr %t4590
  %t4607 = sext i32 24 to i64
  %t4608 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4605, i64 %t4607)
  %t4606 = extractvalue { i64, i1 } %t4608, 0
  %t4609 = extractvalue { i64, i1 } %t4608, 1
  br i1 %t4609, label %add_overflow1432, label %add_ok1431
add_overflow1432:
  call void @panic(ptr @.str.286)
  unreachable
add_ok1431:
  %t4610 = inttoptr i64 %t4606 to ptr
  %t4611 = alloca ptr
  store ptr %t4610, ptr %t4611
  %t4612 = load ptr, ptr %t4597
  %t4613 = load i64, ptr %t4612
  %t4614 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4614)
  store i64 %t4613, ptr %t4614
  %t4615 = load ptr, ptr %t4604
  %t4616 = load i64, ptr %t4615
  %t4617 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4617)
  store i64 %t4616, ptr %t4617
  %t4618 = load ptr, ptr %t4611
  %t4619 = load i64, ptr %t4618
  %t4620 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4620)
  store i64 %t4619, ptr %t4620
  %t4621 = load i64, ptr %t4614
  %t4622 = load i64, ptr %t4617
  %t4623 = icmp sge i64 %t4621, %t4622
  br i1 %t4623, label %if.then1433, label %if.end1435
if.then1433:
  %t4624 = load i64, ptr %t4617
  %t4626 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t4624, i64 2)
  %t4625 = extractvalue { i64, i1 } %t4626, 0
  %t4627 = extractvalue { i64, i1 } %t4626, 1
  br i1 %t4627, label %mul_overflow1437, label %mul_ok1436
mul_overflow1437:
  call void @panic(ptr @.str.287)
  unreachable
mul_ok1436:
  %t4628 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4628)
  store i64 %t4625, ptr %t4628
  %t4629 = load i64, ptr %t4628
  %t4630 = load i64, ptr %t4620
  %t4632 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t4629, i64 %t4630)
  %t4631 = extractvalue { i64, i1 } %t4632, 0
  %t4633 = extractvalue { i64, i1 } %t4632, 1
  br i1 %t4633, label %mul_overflow1439, label %mul_ok1438
mul_overflow1439:
  call void @panic(ptr @.str.288)
  unreachable
mul_ok1438:
  %t4634 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4634)
  store i64 %t4631, ptr %t4634
  %t4635 = load i64, ptr %t4590
  %t4636 = inttoptr i64 %t4635 to ptr
  %t4637 = alloca ptr
  store ptr %t4636, ptr %t4637
  %t4638 = load ptr, ptr %t4637
  %t4639 = load i64, ptr %t4638
  %t4640 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4640)
  store i64 %t4639, ptr %t4640
  %t4641 = load i64, ptr %t4640
  %t4642 = inttoptr i64 %t4641 to ptr
  %t4643 = alloca ptr
  store ptr %t4642, ptr %t4643
  %t4644 = load ptr, ptr %t4643
  %t4645 = load i64, ptr %t4634
  %t4646 = call ptr @mem_realloc(ptr %t4644, i64 %t4645)
  %t4647 = alloca ptr
  store ptr %t4646, ptr %t4647
  %t4648 = load ptr, ptr %t4637
  %t4649 = load ptr, ptr %t4647
  %t4651 = ptrtoint ptr %t4649 to i64
  store i64 %t4651, ptr %t4648
  %t4652 = load ptr, ptr %t4604
  %t4653 = load i64, ptr %t4628
  store i64 %t4653, ptr %t4652
  call void @llvm.lifetime.end.p0(i64 8, ptr %t4640)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t4634)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t4628)
  br label %if.end1435
if.end1435:
  %t4654 = load i64, ptr %t4590
  %t4655 = inttoptr i64 %t4654 to ptr
  %t4656 = alloca ptr
  store ptr %t4655, ptr %t4656
  %t4657 = load ptr, ptr %t4656
  %t4658 = load i64, ptr %t4657
  %t4659 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4659)
  store i64 %t4658, ptr %t4659
  %t4660 = load i64, ptr %t4659
  %t4661 = load i64, ptr %t4614
  %t4662 = load i64, ptr %t4620
  %t4664 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t4661, i64 %t4662)
  %t4663 = extractvalue { i64, i1 } %t4664, 0
  %t4665 = extractvalue { i64, i1 } %t4664, 1
  br i1 %t4665, label %mul_overflow1441, label %mul_ok1440
mul_overflow1441:
  call void @panic(ptr @.str.289)
  unreachable
mul_ok1440:
  %t4667 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4660, i64 %t4663)
  %t4666 = extractvalue { i64, i1 } %t4667, 0
  %t4668 = extractvalue { i64, i1 } %t4667, 1
  br i1 %t4668, label %add_overflow1443, label %add_ok1442
add_overflow1443:
  call void @panic(ptr @.str.150)
  unreachable
add_ok1442:
  %t4669 = inttoptr i64 %t4666 to ptr
  %t4670 = alloca ptr
  store ptr %t4669, ptr %t4670
  %t4671 = load ptr, ptr %t4670
  %t4672 = load ptr, ptr %t4585
  store ptr %t4672, ptr %t4671
  %t4673 = load ptr, ptr %t4597
  %t4674 = load i64, ptr %t4614
  %t4676 = sext i32 1 to i64
  %t4677 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4674, i64 %t4676)
  %t4675 = extractvalue { i64, i1 } %t4677, 0
  %t4678 = extractvalue { i64, i1 } %t4677, 1
  br i1 %t4678, label %add_overflow1445, label %add_ok1444
add_overflow1445:
  call void @panic(ptr @.str.196)
  unreachable
add_ok1444:
  store i64 %t4675, ptr %t4673
  ret void
}

define internal void @tml_N3std11collections4list14List__IrGlobal14push__IrGlobalE(ptr %this, %struct.IrGlobal %value) #0 {
entry:
  %t4679 = alloca %struct.IrGlobal
  store %struct.IrGlobal %value, ptr %t4679
  %t4680 = getelementptr inbounds %struct.List__IrGlobal, ptr %this, i32 0, i32 0
  %t4681 = load ptr, ptr %t4680
  %t4683 = ptrtoint ptr %t4681 to i64
  %t4684 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4684)
  store i64 %t4683, ptr %t4684
  %t4685 = load i64, ptr %t4684
  %t4687 = sext i32 8 to i64
  %t4688 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4685, i64 %t4687)
  %t4686 = extractvalue { i64, i1 } %t4688, 0
  %t4689 = extractvalue { i64, i1 } %t4688, 1
  br i1 %t4689, label %add_overflow1447, label %add_ok1446
add_overflow1447:
  call void @panic(ptr @.str.284)
  unreachable
add_ok1446:
  %t4690 = inttoptr i64 %t4686 to ptr
  %t4691 = alloca ptr
  store ptr %t4690, ptr %t4691
  %t4692 = load i64, ptr %t4684
  %t4694 = sext i32 16 to i64
  %t4695 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4692, i64 %t4694)
  %t4693 = extractvalue { i64, i1 } %t4695, 0
  %t4696 = extractvalue { i64, i1 } %t4695, 1
  br i1 %t4696, label %add_overflow1449, label %add_ok1448
add_overflow1449:
  call void @panic(ptr @.str.285)
  unreachable
add_ok1448:
  %t4697 = inttoptr i64 %t4693 to ptr
  %t4698 = alloca ptr
  store ptr %t4697, ptr %t4698
  %t4699 = load i64, ptr %t4684
  %t4701 = sext i32 24 to i64
  %t4702 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4699, i64 %t4701)
  %t4700 = extractvalue { i64, i1 } %t4702, 0
  %t4703 = extractvalue { i64, i1 } %t4702, 1
  br i1 %t4703, label %add_overflow1451, label %add_ok1450
add_overflow1451:
  call void @panic(ptr @.str.286)
  unreachable
add_ok1450:
  %t4704 = inttoptr i64 %t4700 to ptr
  %t4705 = alloca ptr
  store ptr %t4704, ptr %t4705
  %t4706 = load ptr, ptr %t4691
  %t4707 = load i64, ptr %t4706
  %t4708 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4708)
  store i64 %t4707, ptr %t4708
  %t4709 = load ptr, ptr %t4698
  %t4710 = load i64, ptr %t4709
  %t4711 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4711)
  store i64 %t4710, ptr %t4711
  %t4712 = load ptr, ptr %t4705
  %t4713 = load i64, ptr %t4712
  %t4714 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4714)
  store i64 %t4713, ptr %t4714
  %t4715 = load i64, ptr %t4708
  %t4716 = load i64, ptr %t4711
  %t4717 = icmp sge i64 %t4715, %t4716
  br i1 %t4717, label %if.then1452, label %if.end1454
if.then1452:
  %t4718 = load i64, ptr %t4711
  %t4720 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t4718, i64 2)
  %t4719 = extractvalue { i64, i1 } %t4720, 0
  %t4721 = extractvalue { i64, i1 } %t4720, 1
  br i1 %t4721, label %mul_overflow1456, label %mul_ok1455
mul_overflow1456:
  call void @panic(ptr @.str.287)
  unreachable
mul_ok1455:
  %t4722 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4722)
  store i64 %t4719, ptr %t4722
  %t4723 = load i64, ptr %t4722
  %t4724 = load i64, ptr %t4714
  %t4726 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t4723, i64 %t4724)
  %t4725 = extractvalue { i64, i1 } %t4726, 0
  %t4727 = extractvalue { i64, i1 } %t4726, 1
  br i1 %t4727, label %mul_overflow1458, label %mul_ok1457
mul_overflow1458:
  call void @panic(ptr @.str.288)
  unreachable
mul_ok1457:
  %t4728 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4728)
  store i64 %t4725, ptr %t4728
  %t4729 = load i64, ptr %t4684
  %t4730 = inttoptr i64 %t4729 to ptr
  %t4731 = alloca ptr
  store ptr %t4730, ptr %t4731
  %t4732 = load ptr, ptr %t4731
  %t4733 = load i64, ptr %t4732
  %t4734 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4734)
  store i64 %t4733, ptr %t4734
  %t4735 = load i64, ptr %t4734
  %t4736 = inttoptr i64 %t4735 to ptr
  %t4737 = alloca ptr
  store ptr %t4736, ptr %t4737
  %t4738 = load ptr, ptr %t4737
  %t4739 = load i64, ptr %t4728
  %t4740 = call ptr @mem_realloc(ptr %t4738, i64 %t4739)
  %t4741 = alloca ptr
  store ptr %t4740, ptr %t4741
  %t4742 = load ptr, ptr %t4731
  %t4743 = load ptr, ptr %t4741
  %t4745 = ptrtoint ptr %t4743 to i64
  store i64 %t4745, ptr %t4742
  %t4746 = load ptr, ptr %t4698
  %t4747 = load i64, ptr %t4722
  store i64 %t4747, ptr %t4746
  call void @llvm.lifetime.end.p0(i64 8, ptr %t4734)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t4728)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t4722)
  br label %if.end1454
if.end1454:
  %t4748 = load i64, ptr %t4684
  %t4749 = inttoptr i64 %t4748 to ptr
  %t4750 = alloca ptr
  store ptr %t4749, ptr %t4750
  %t4751 = load ptr, ptr %t4750
  %t4752 = load i64, ptr %t4751
  %t4753 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4753)
  store i64 %t4752, ptr %t4753
  %t4754 = load i64, ptr %t4753
  %t4755 = load i64, ptr %t4708
  %t4756 = load i64, ptr %t4714
  %t4758 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t4755, i64 %t4756)
  %t4757 = extractvalue { i64, i1 } %t4758, 0
  %t4759 = extractvalue { i64, i1 } %t4758, 1
  br i1 %t4759, label %mul_overflow1460, label %mul_ok1459
mul_overflow1460:
  call void @panic(ptr @.str.289)
  unreachable
mul_ok1459:
  %t4761 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4754, i64 %t4757)
  %t4760 = extractvalue { i64, i1 } %t4761, 0
  %t4762 = extractvalue { i64, i1 } %t4761, 1
  br i1 %t4762, label %add_overflow1462, label %add_ok1461
add_overflow1462:
  call void @panic(ptr @.str.150)
  unreachable
add_ok1461:
  %t4763 = inttoptr i64 %t4760 to ptr
  %t4764 = alloca ptr
  store ptr %t4763, ptr %t4764
  %t4765 = load ptr, ptr %t4764
  %t4766 = load %struct.IrGlobal, ptr %t4679
  store %struct.IrGlobal %t4766, ptr %t4765
  %t4767 = load ptr, ptr %t4691
  %t4768 = load i64, ptr %t4708
  %t4770 = sext i32 1 to i64
  %t4771 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4768, i64 %t4770)
  %t4769 = extractvalue { i64, i1 } %t4771, 0
  %t4772 = extractvalue { i64, i1 } %t4771, 1
  br i1 %t4772, label %add_overflow1464, label %add_ok1463
add_overflow1464:
  call void @panic(ptr @.str.196)
  unreachable
add_ok1463:
  store i64 %t4769, ptr %t4767
  ret void
}

define internal void @tml_N3std11collections4list13List__IrParam4dropE(ptr %this) #0 {
entry:
  %t4773 = call {} @tml_N3std11collections4list13List__IrParam7destroyE(ptr %this)
  ret void
}

define internal %struct.List__IrBlock @tml_N3std11collections4list13List__IrBlock3newE(i64 %initial_capacity) #0 {
entry:
  %t4774 = alloca i64
  store i64 %initial_capacity, ptr %t4774
  %t4775 = load i64, ptr %t4774
  %t4776 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4776)
  store i64 %t4775, ptr %t4776
  %t4777 = load i64, ptr %t4776
  %t4779 = sext i32 4 to i64
  %t4778 = icmp slt i64 %t4777, %t4779
  br i1 %t4778, label %if.then1465, label %if.end1467
if.then1465:
  store i64 4, ptr %t4776
  br label %if.end1467
if.end1467:
  %t4780 = getelementptr inbounds %struct.IrBlock, ptr null, i32 1
  %t4781 = ptrtoint ptr %t4780 to i64
  %t4782 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4782)
  store i64 %t4781, ptr %t4782
  %t4783 = load i64, ptr %t4782
  %t4784 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4784)
  store i64 %t4783, ptr %t4784
  %t4785 = load i64, ptr %t4784
  %t4787 = sext i32 8 to i64
  %t4786 = icmp slt i64 %t4785, %t4787
  br i1 %t4786, label %if.then1468, label %if.end1470
if.then1468:
  store i64 8, ptr %t4784
  br label %if.end1470
if.end1470:
  %t4788 = call ptr @mem_alloc(i64 32)
  %t4789 = alloca ptr
  store ptr %t4788, ptr %t4789
  %t4790 = load ptr, ptr %t4789
  %t4792 = ptrtoint ptr %t4790 to i64
  %t4793 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4793)
  store i64 %t4792, ptr %t4793
  %t4794 = load i64, ptr %t4776
  %t4795 = load i64, ptr %t4784
  %t4797 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t4794, i64 %t4795)
  %t4796 = extractvalue { i64, i1 } %t4797, 0
  %t4798 = extractvalue { i64, i1 } %t4797, 1
  br i1 %t4798, label %mul_overflow1472, label %mul_ok1471
mul_overflow1472:
  call void @panic(ptr @.str.278)
  unreachable
mul_ok1471:
  %t4799 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4799)
  store i64 %t4796, ptr %t4799
  %t4800 = load i64, ptr %t4799
  %t4801 = call ptr @mem_alloc(i64 %t4800)
  %t4802 = alloca ptr
  store ptr %t4801, ptr %t4802
  %t4803 = load i64, ptr %t4793
  %t4804 = inttoptr i64 %t4803 to ptr
  %t4805 = alloca ptr
  store ptr %t4804, ptr %t4805
  %t4806 = load ptr, ptr %t4805
  %t4807 = load ptr, ptr %t4802
  %t4809 = ptrtoint ptr %t4807 to i64
  store i64 %t4809, ptr %t4806
  %t4810 = load i64, ptr %t4793
  %t4812 = sext i32 8 to i64
  %t4813 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4810, i64 %t4812)
  %t4811 = extractvalue { i64, i1 } %t4813, 0
  %t4814 = extractvalue { i64, i1 } %t4813, 1
  br i1 %t4814, label %add_overflow1474, label %add_ok1473
add_overflow1474:
  call void @panic(ptr @.str.208)
  unreachable
add_ok1473:
  %t4815 = inttoptr i64 %t4811 to ptr
  %t4816 = alloca ptr
  store ptr %t4815, ptr %t4816
  %t4817 = load ptr, ptr %t4816
  %t4818 = sext i32 0 to i64
  store i64 %t4818, ptr %t4817
  %t4819 = load i64, ptr %t4793
  %t4821 = sext i32 16 to i64
  %t4822 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4819, i64 %t4821)
  %t4820 = extractvalue { i64, i1 } %t4822, 0
  %t4823 = extractvalue { i64, i1 } %t4822, 1
  br i1 %t4823, label %add_overflow1476, label %add_ok1475
add_overflow1476:
  call void @panic(ptr @.str.109)
  unreachable
add_ok1475:
  %t4824 = inttoptr i64 %t4820 to ptr
  %t4825 = alloca ptr
  store ptr %t4824, ptr %t4825
  %t4826 = load ptr, ptr %t4825
  %t4827 = load i64, ptr %t4776
  store i64 %t4827, ptr %t4826
  %t4828 = load i64, ptr %t4793
  %t4830 = sext i32 24 to i64
  %t4831 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4828, i64 %t4830)
  %t4829 = extractvalue { i64, i1 } %t4831, 0
  %t4832 = extractvalue { i64, i1 } %t4831, 1
  br i1 %t4832, label %add_overflow1478, label %add_ok1477
add_overflow1478:
  call void @panic(ptr @.str.279)
  unreachable
add_ok1477:
  %t4833 = inttoptr i64 %t4829 to ptr
  %t4834 = alloca ptr
  store ptr %t4833, ptr %t4834
  %t4835 = load ptr, ptr %t4834
  %t4836 = load i64, ptr %t4784
  store i64 %t4836, ptr %t4835
  %t4837 = alloca %struct.List__IrBlock
  %t4838 = load ptr, ptr %t4789
  %t4839 = getelementptr inbounds %struct.List__IrBlock, ptr %t4837, i32 0, i32 0
  store ptr %t4838, ptr %t4839
  %t4840 = load %struct.List__IrBlock, ptr %t4837
  ret %struct.List__IrBlock %t4840
}

define internal void @tml_N3std11collections4list13List__IrBlock4dropE(ptr %this) #0 {
entry:
  %t4841 = call {} @tml_N3std11collections4list13List__IrBlock7destroyE(ptr %this)
  ret void
}

define internal %struct.List__IrInstr @tml_N3std11collections4list13List__IrInstr3newE(i64 %initial_capacity) #0 {
entry:
  %t4842 = alloca i64
  store i64 %initial_capacity, ptr %t4842
  %t4843 = load i64, ptr %t4842
  %t4844 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4844)
  store i64 %t4843, ptr %t4844
  %t4845 = load i64, ptr %t4844
  %t4847 = sext i32 4 to i64
  %t4846 = icmp slt i64 %t4845, %t4847
  br i1 %t4846, label %if.then1479, label %if.end1481
if.then1479:
  store i64 4, ptr %t4844
  br label %if.end1481
if.end1481:
  %t4848 = getelementptr inbounds %struct.IrInstr, ptr null, i32 1
  %t4849 = ptrtoint ptr %t4848 to i64
  %t4850 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4850)
  store i64 %t4849, ptr %t4850
  %t4851 = load i64, ptr %t4850
  %t4852 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4852)
  store i64 %t4851, ptr %t4852
  %t4853 = load i64, ptr %t4852
  %t4855 = sext i32 8 to i64
  %t4854 = icmp slt i64 %t4853, %t4855
  br i1 %t4854, label %if.then1482, label %if.end1484
if.then1482:
  store i64 8, ptr %t4852
  br label %if.end1484
if.end1484:
  %t4856 = call ptr @mem_alloc(i64 32)
  %t4857 = alloca ptr
  store ptr %t4856, ptr %t4857
  %t4858 = load ptr, ptr %t4857
  %t4860 = ptrtoint ptr %t4858 to i64
  %t4861 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4861)
  store i64 %t4860, ptr %t4861
  %t4862 = load i64, ptr %t4844
  %t4863 = load i64, ptr %t4852
  %t4865 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t4862, i64 %t4863)
  %t4864 = extractvalue { i64, i1 } %t4865, 0
  %t4866 = extractvalue { i64, i1 } %t4865, 1
  br i1 %t4866, label %mul_overflow1486, label %mul_ok1485
mul_overflow1486:
  call void @panic(ptr @.str.278)
  unreachable
mul_ok1485:
  %t4867 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4867)
  store i64 %t4864, ptr %t4867
  %t4868 = load i64, ptr %t4867
  %t4869 = call ptr @mem_alloc(i64 %t4868)
  %t4870 = alloca ptr
  store ptr %t4869, ptr %t4870
  %t4871 = load i64, ptr %t4861
  %t4872 = inttoptr i64 %t4871 to ptr
  %t4873 = alloca ptr
  store ptr %t4872, ptr %t4873
  %t4874 = load ptr, ptr %t4873
  %t4875 = load ptr, ptr %t4870
  %t4877 = ptrtoint ptr %t4875 to i64
  store i64 %t4877, ptr %t4874
  %t4878 = load i64, ptr %t4861
  %t4880 = sext i32 8 to i64
  %t4881 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4878, i64 %t4880)
  %t4879 = extractvalue { i64, i1 } %t4881, 0
  %t4882 = extractvalue { i64, i1 } %t4881, 1
  br i1 %t4882, label %add_overflow1488, label %add_ok1487
add_overflow1488:
  call void @panic(ptr @.str.208)
  unreachable
add_ok1487:
  %t4883 = inttoptr i64 %t4879 to ptr
  %t4884 = alloca ptr
  store ptr %t4883, ptr %t4884
  %t4885 = load ptr, ptr %t4884
  %t4886 = sext i32 0 to i64
  store i64 %t4886, ptr %t4885
  %t4887 = load i64, ptr %t4861
  %t4889 = sext i32 16 to i64
  %t4890 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4887, i64 %t4889)
  %t4888 = extractvalue { i64, i1 } %t4890, 0
  %t4891 = extractvalue { i64, i1 } %t4890, 1
  br i1 %t4891, label %add_overflow1490, label %add_ok1489
add_overflow1490:
  call void @panic(ptr @.str.109)
  unreachable
add_ok1489:
  %t4892 = inttoptr i64 %t4888 to ptr
  %t4893 = alloca ptr
  store ptr %t4892, ptr %t4893
  %t4894 = load ptr, ptr %t4893
  %t4895 = load i64, ptr %t4844
  store i64 %t4895, ptr %t4894
  %t4896 = load i64, ptr %t4861
  %t4898 = sext i32 24 to i64
  %t4899 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4896, i64 %t4898)
  %t4897 = extractvalue { i64, i1 } %t4899, 0
  %t4900 = extractvalue { i64, i1 } %t4899, 1
  br i1 %t4900, label %add_overflow1492, label %add_ok1491
add_overflow1492:
  call void @panic(ptr @.str.279)
  unreachable
add_ok1491:
  %t4901 = inttoptr i64 %t4897 to ptr
  %t4902 = alloca ptr
  store ptr %t4901, ptr %t4902
  %t4903 = load ptr, ptr %t4902
  %t4904 = load i64, ptr %t4852
  store i64 %t4904, ptr %t4903
  %t4905 = alloca %struct.List__IrInstr
  %t4906 = load ptr, ptr %t4857
  %t4907 = getelementptr inbounds %struct.List__IrInstr, ptr %t4905, i32 0, i32 0
  store ptr %t4906, ptr %t4907
  %t4908 = load %struct.List__IrInstr, ptr %t4905
  ret %struct.List__IrInstr %t4908
}

define internal void @tml_N3std11collections4list13List__IrInstr4dropE(ptr %this) #0 {
entry:
  %t4909 = call {} @tml_N3std11collections4list13List__IrInstr7destroyE(ptr %this)
  ret void
}

define internal void @tml_N3std11collections4list13List__IrBlock13push__IrBlockE(ptr %this, %struct.IrBlock %value) #0 {
entry:
  %t4910 = alloca %struct.IrBlock
  store %struct.IrBlock %value, ptr %t4910
  %t4911 = getelementptr inbounds %struct.List__IrBlock, ptr %this, i32 0, i32 0
  %t4912 = load ptr, ptr %t4911
  %t4914 = ptrtoint ptr %t4912 to i64
  %t4915 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4915)
  store i64 %t4914, ptr %t4915
  %t4916 = load i64, ptr %t4915
  %t4918 = sext i32 8 to i64
  %t4919 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4916, i64 %t4918)
  %t4917 = extractvalue { i64, i1 } %t4919, 0
  %t4920 = extractvalue { i64, i1 } %t4919, 1
  br i1 %t4920, label %add_overflow1494, label %add_ok1493
add_overflow1494:
  call void @panic(ptr @.str.284)
  unreachable
add_ok1493:
  %t4921 = inttoptr i64 %t4917 to ptr
  %t4922 = alloca ptr
  store ptr %t4921, ptr %t4922
  %t4923 = load i64, ptr %t4915
  %t4925 = sext i32 16 to i64
  %t4926 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4923, i64 %t4925)
  %t4924 = extractvalue { i64, i1 } %t4926, 0
  %t4927 = extractvalue { i64, i1 } %t4926, 1
  br i1 %t4927, label %add_overflow1496, label %add_ok1495
add_overflow1496:
  call void @panic(ptr @.str.285)
  unreachable
add_ok1495:
  %t4928 = inttoptr i64 %t4924 to ptr
  %t4929 = alloca ptr
  store ptr %t4928, ptr %t4929
  %t4930 = load i64, ptr %t4915
  %t4932 = sext i32 24 to i64
  %t4933 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4930, i64 %t4932)
  %t4931 = extractvalue { i64, i1 } %t4933, 0
  %t4934 = extractvalue { i64, i1 } %t4933, 1
  br i1 %t4934, label %add_overflow1498, label %add_ok1497
add_overflow1498:
  call void @panic(ptr @.str.286)
  unreachable
add_ok1497:
  %t4935 = inttoptr i64 %t4931 to ptr
  %t4936 = alloca ptr
  store ptr %t4935, ptr %t4936
  %t4937 = load ptr, ptr %t4922
  %t4938 = load i64, ptr %t4937
  %t4939 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4939)
  store i64 %t4938, ptr %t4939
  %t4940 = load ptr, ptr %t4929
  %t4941 = load i64, ptr %t4940
  %t4942 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4942)
  store i64 %t4941, ptr %t4942
  %t4943 = load ptr, ptr %t4936
  %t4944 = load i64, ptr %t4943
  %t4945 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4945)
  store i64 %t4944, ptr %t4945
  %t4946 = load i64, ptr %t4939
  %t4947 = load i64, ptr %t4942
  %t4948 = icmp sge i64 %t4946, %t4947
  br i1 %t4948, label %if.then1499, label %if.end1501
if.then1499:
  %t4949 = load i64, ptr %t4942
  %t4951 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t4949, i64 2)
  %t4950 = extractvalue { i64, i1 } %t4951, 0
  %t4952 = extractvalue { i64, i1 } %t4951, 1
  br i1 %t4952, label %mul_overflow1503, label %mul_ok1502
mul_overflow1503:
  call void @panic(ptr @.str.287)
  unreachable
mul_ok1502:
  %t4953 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4953)
  store i64 %t4950, ptr %t4953
  %t4954 = load i64, ptr %t4953
  %t4955 = load i64, ptr %t4945
  %t4957 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t4954, i64 %t4955)
  %t4956 = extractvalue { i64, i1 } %t4957, 0
  %t4958 = extractvalue { i64, i1 } %t4957, 1
  br i1 %t4958, label %mul_overflow1505, label %mul_ok1504
mul_overflow1505:
  call void @panic(ptr @.str.288)
  unreachable
mul_ok1504:
  %t4959 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4959)
  store i64 %t4956, ptr %t4959
  %t4960 = load i64, ptr %t4915
  %t4961 = inttoptr i64 %t4960 to ptr
  %t4962 = alloca ptr
  store ptr %t4961, ptr %t4962
  %t4963 = load ptr, ptr %t4962
  %t4964 = load i64, ptr %t4963
  %t4965 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4965)
  store i64 %t4964, ptr %t4965
  %t4966 = load i64, ptr %t4965
  %t4967 = inttoptr i64 %t4966 to ptr
  %t4968 = alloca ptr
  store ptr %t4967, ptr %t4968
  %t4969 = load ptr, ptr %t4968
  %t4970 = load i64, ptr %t4959
  %t4971 = call ptr @mem_realloc(ptr %t4969, i64 %t4970)
  %t4972 = alloca ptr
  store ptr %t4971, ptr %t4972
  %t4973 = load ptr, ptr %t4962
  %t4974 = load ptr, ptr %t4972
  %t4976 = ptrtoint ptr %t4974 to i64
  store i64 %t4976, ptr %t4973
  %t4977 = load ptr, ptr %t4929
  %t4978 = load i64, ptr %t4953
  store i64 %t4978, ptr %t4977
  call void @llvm.lifetime.end.p0(i64 8, ptr %t4965)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t4959)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t4953)
  br label %if.end1501
if.end1501:
  %t4979 = load i64, ptr %t4915
  %t4980 = inttoptr i64 %t4979 to ptr
  %t4981 = alloca ptr
  store ptr %t4980, ptr %t4981
  %t4982 = load ptr, ptr %t4981
  %t4983 = load i64, ptr %t4982
  %t4984 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t4984)
  store i64 %t4983, ptr %t4984
  %t4985 = load i64, ptr %t4984
  %t4986 = load i64, ptr %t4939
  %t4987 = load i64, ptr %t4945
  %t4989 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t4986, i64 %t4987)
  %t4988 = extractvalue { i64, i1 } %t4989, 0
  %t4990 = extractvalue { i64, i1 } %t4989, 1
  br i1 %t4990, label %mul_overflow1507, label %mul_ok1506
mul_overflow1507:
  call void @panic(ptr @.str.289)
  unreachable
mul_ok1506:
  %t4992 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4985, i64 %t4988)
  %t4991 = extractvalue { i64, i1 } %t4992, 0
  %t4993 = extractvalue { i64, i1 } %t4992, 1
  br i1 %t4993, label %add_overflow1509, label %add_ok1508
add_overflow1509:
  call void @panic(ptr @.str.150)
  unreachable
add_ok1508:
  %t4994 = inttoptr i64 %t4991 to ptr
  %t4995 = alloca ptr
  store ptr %t4994, ptr %t4995
  %t4996 = load ptr, ptr %t4995
  %t4997 = load %struct.IrBlock, ptr %t4910
  store %struct.IrBlock %t4997, ptr %t4996
  %t4998 = load ptr, ptr %t4922
  %t4999 = load i64, ptr %t4939
  %t5001 = sext i32 1 to i64
  %t5002 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t4999, i64 %t5001)
  %t5000 = extractvalue { i64, i1 } %t5002, 0
  %t5003 = extractvalue { i64, i1 } %t5002, 1
  br i1 %t5003, label %add_overflow1511, label %add_ok1510
add_overflow1511:
  call void @panic(ptr @.str.196)
  unreachable
add_ok1510:
  store i64 %t5000, ptr %t4998
  ret void
}

define internal void @tml_N3std11collections4list13List__IrInstr13push__IrInstrE(ptr %this, %struct.IrInstr %value) #0 {
entry:
  %t5004 = alloca %struct.IrInstr
  store %struct.IrInstr %value, ptr %t5004
  %t5005 = getelementptr inbounds %struct.List__IrInstr, ptr %this, i32 0, i32 0
  %t5006 = load ptr, ptr %t5005
  %t5008 = ptrtoint ptr %t5006 to i64
  %t5009 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5009)
  store i64 %t5008, ptr %t5009
  %t5010 = load i64, ptr %t5009
  %t5012 = sext i32 8 to i64
  %t5013 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5010, i64 %t5012)
  %t5011 = extractvalue { i64, i1 } %t5013, 0
  %t5014 = extractvalue { i64, i1 } %t5013, 1
  br i1 %t5014, label %add_overflow1513, label %add_ok1512
add_overflow1513:
  call void @panic(ptr @.str.284)
  unreachable
add_ok1512:
  %t5015 = inttoptr i64 %t5011 to ptr
  %t5016 = alloca ptr
  store ptr %t5015, ptr %t5016
  %t5017 = load i64, ptr %t5009
  %t5019 = sext i32 16 to i64
  %t5020 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5017, i64 %t5019)
  %t5018 = extractvalue { i64, i1 } %t5020, 0
  %t5021 = extractvalue { i64, i1 } %t5020, 1
  br i1 %t5021, label %add_overflow1515, label %add_ok1514
add_overflow1515:
  call void @panic(ptr @.str.285)
  unreachable
add_ok1514:
  %t5022 = inttoptr i64 %t5018 to ptr
  %t5023 = alloca ptr
  store ptr %t5022, ptr %t5023
  %t5024 = load i64, ptr %t5009
  %t5026 = sext i32 24 to i64
  %t5027 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5024, i64 %t5026)
  %t5025 = extractvalue { i64, i1 } %t5027, 0
  %t5028 = extractvalue { i64, i1 } %t5027, 1
  br i1 %t5028, label %add_overflow1517, label %add_ok1516
add_overflow1517:
  call void @panic(ptr @.str.286)
  unreachable
add_ok1516:
  %t5029 = inttoptr i64 %t5025 to ptr
  %t5030 = alloca ptr
  store ptr %t5029, ptr %t5030
  %t5031 = load ptr, ptr %t5016
  %t5032 = load i64, ptr %t5031
  %t5033 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5033)
  store i64 %t5032, ptr %t5033
  %t5034 = load ptr, ptr %t5023
  %t5035 = load i64, ptr %t5034
  %t5036 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5036)
  store i64 %t5035, ptr %t5036
  %t5037 = load ptr, ptr %t5030
  %t5038 = load i64, ptr %t5037
  %t5039 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5039)
  store i64 %t5038, ptr %t5039
  %t5040 = load i64, ptr %t5033
  %t5041 = load i64, ptr %t5036
  %t5042 = icmp sge i64 %t5040, %t5041
  br i1 %t5042, label %if.then1518, label %if.end1520
if.then1518:
  %t5043 = load i64, ptr %t5036
  %t5045 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5043, i64 2)
  %t5044 = extractvalue { i64, i1 } %t5045, 0
  %t5046 = extractvalue { i64, i1 } %t5045, 1
  br i1 %t5046, label %mul_overflow1522, label %mul_ok1521
mul_overflow1522:
  call void @panic(ptr @.str.287)
  unreachable
mul_ok1521:
  %t5047 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5047)
  store i64 %t5044, ptr %t5047
  %t5048 = load i64, ptr %t5047
  %t5049 = load i64, ptr %t5039
  %t5051 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5048, i64 %t5049)
  %t5050 = extractvalue { i64, i1 } %t5051, 0
  %t5052 = extractvalue { i64, i1 } %t5051, 1
  br i1 %t5052, label %mul_overflow1524, label %mul_ok1523
mul_overflow1524:
  call void @panic(ptr @.str.288)
  unreachable
mul_ok1523:
  %t5053 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5053)
  store i64 %t5050, ptr %t5053
  %t5054 = load i64, ptr %t5009
  %t5055 = inttoptr i64 %t5054 to ptr
  %t5056 = alloca ptr
  store ptr %t5055, ptr %t5056
  %t5057 = load ptr, ptr %t5056
  %t5058 = load i64, ptr %t5057
  %t5059 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5059)
  store i64 %t5058, ptr %t5059
  %t5060 = load i64, ptr %t5059
  %t5061 = inttoptr i64 %t5060 to ptr
  %t5062 = alloca ptr
  store ptr %t5061, ptr %t5062
  %t5063 = load ptr, ptr %t5062
  %t5064 = load i64, ptr %t5053
  %t5065 = call ptr @mem_realloc(ptr %t5063, i64 %t5064)
  %t5066 = alloca ptr
  store ptr %t5065, ptr %t5066
  %t5067 = load ptr, ptr %t5056
  %t5068 = load ptr, ptr %t5066
  %t5070 = ptrtoint ptr %t5068 to i64
  store i64 %t5070, ptr %t5067
  %t5071 = load ptr, ptr %t5023
  %t5072 = load i64, ptr %t5047
  store i64 %t5072, ptr %t5071
  call void @llvm.lifetime.end.p0(i64 8, ptr %t5059)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t5053)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t5047)
  br label %if.end1520
if.end1520:
  %t5073 = load i64, ptr %t5009
  %t5074 = inttoptr i64 %t5073 to ptr
  %t5075 = alloca ptr
  store ptr %t5074, ptr %t5075
  %t5076 = load ptr, ptr %t5075
  %t5077 = load i64, ptr %t5076
  %t5078 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5078)
  store i64 %t5077, ptr %t5078
  %t5079 = load i64, ptr %t5078
  %t5080 = load i64, ptr %t5033
  %t5081 = load i64, ptr %t5039
  %t5083 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5080, i64 %t5081)
  %t5082 = extractvalue { i64, i1 } %t5083, 0
  %t5084 = extractvalue { i64, i1 } %t5083, 1
  br i1 %t5084, label %mul_overflow1526, label %mul_ok1525
mul_overflow1526:
  call void @panic(ptr @.str.289)
  unreachable
mul_ok1525:
  %t5086 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5079, i64 %t5082)
  %t5085 = extractvalue { i64, i1 } %t5086, 0
  %t5087 = extractvalue { i64, i1 } %t5086, 1
  br i1 %t5087, label %add_overflow1528, label %add_ok1527
add_overflow1528:
  call void @panic(ptr @.str.150)
  unreachable
add_ok1527:
  %t5088 = inttoptr i64 %t5085 to ptr
  %t5089 = alloca ptr
  store ptr %t5088, ptr %t5089
  %t5090 = load ptr, ptr %t5089
  %t5091 = load %struct.IrInstr, ptr %t5004
  store %struct.IrInstr %t5091, ptr %t5090
  %t5092 = load ptr, ptr %t5016
  %t5093 = load i64, ptr %t5033
  %t5095 = sext i32 1 to i64
  %t5096 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5093, i64 %t5095)
  %t5094 = extractvalue { i64, i1 } %t5096, 0
  %t5097 = extractvalue { i64, i1 } %t5096, 1
  br i1 %t5097, label %add_overflow1530, label %add_ok1529
add_overflow1530:
  call void @panic(ptr @.str.196)
  unreachable
add_ok1529:
  store i64 %t5094, ptr %t5092
  ret void
}

define internal void @tml_N3std11collections4list16List__IrFunction16push__IrFunctionE(ptr %this, %struct.IrFunction %value) #0 {
entry:
  %t5098 = alloca %struct.IrFunction
  store %struct.IrFunction %value, ptr %t5098
  %t5099 = getelementptr inbounds %struct.List__IrFunction, ptr %this, i32 0, i32 0
  %t5100 = load ptr, ptr %t5099
  %t5102 = ptrtoint ptr %t5100 to i64
  %t5103 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5103)
  store i64 %t5102, ptr %t5103
  %t5104 = load i64, ptr %t5103
  %t5106 = sext i32 8 to i64
  %t5107 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5104, i64 %t5106)
  %t5105 = extractvalue { i64, i1 } %t5107, 0
  %t5108 = extractvalue { i64, i1 } %t5107, 1
  br i1 %t5108, label %add_overflow1532, label %add_ok1531
add_overflow1532:
  call void @panic(ptr @.str.284)
  unreachable
add_ok1531:
  %t5109 = inttoptr i64 %t5105 to ptr
  %t5110 = alloca ptr
  store ptr %t5109, ptr %t5110
  %t5111 = load i64, ptr %t5103
  %t5113 = sext i32 16 to i64
  %t5114 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5111, i64 %t5113)
  %t5112 = extractvalue { i64, i1 } %t5114, 0
  %t5115 = extractvalue { i64, i1 } %t5114, 1
  br i1 %t5115, label %add_overflow1534, label %add_ok1533
add_overflow1534:
  call void @panic(ptr @.str.285)
  unreachable
add_ok1533:
  %t5116 = inttoptr i64 %t5112 to ptr
  %t5117 = alloca ptr
  store ptr %t5116, ptr %t5117
  %t5118 = load i64, ptr %t5103
  %t5120 = sext i32 24 to i64
  %t5121 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5118, i64 %t5120)
  %t5119 = extractvalue { i64, i1 } %t5121, 0
  %t5122 = extractvalue { i64, i1 } %t5121, 1
  br i1 %t5122, label %add_overflow1536, label %add_ok1535
add_overflow1536:
  call void @panic(ptr @.str.286)
  unreachable
add_ok1535:
  %t5123 = inttoptr i64 %t5119 to ptr
  %t5124 = alloca ptr
  store ptr %t5123, ptr %t5124
  %t5125 = load ptr, ptr %t5110
  %t5126 = load i64, ptr %t5125
  %t5127 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5127)
  store i64 %t5126, ptr %t5127
  %t5128 = load ptr, ptr %t5117
  %t5129 = load i64, ptr %t5128
  %t5130 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5130)
  store i64 %t5129, ptr %t5130
  %t5131 = load ptr, ptr %t5124
  %t5132 = load i64, ptr %t5131
  %t5133 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5133)
  store i64 %t5132, ptr %t5133
  %t5134 = load i64, ptr %t5127
  %t5135 = load i64, ptr %t5130
  %t5136 = icmp sge i64 %t5134, %t5135
  br i1 %t5136, label %if.then1537, label %if.end1539
if.then1537:
  %t5137 = load i64, ptr %t5130
  %t5139 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5137, i64 2)
  %t5138 = extractvalue { i64, i1 } %t5139, 0
  %t5140 = extractvalue { i64, i1 } %t5139, 1
  br i1 %t5140, label %mul_overflow1541, label %mul_ok1540
mul_overflow1541:
  call void @panic(ptr @.str.287)
  unreachable
mul_ok1540:
  %t5141 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5141)
  store i64 %t5138, ptr %t5141
  %t5142 = load i64, ptr %t5141
  %t5143 = load i64, ptr %t5133
  %t5145 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5142, i64 %t5143)
  %t5144 = extractvalue { i64, i1 } %t5145, 0
  %t5146 = extractvalue { i64, i1 } %t5145, 1
  br i1 %t5146, label %mul_overflow1543, label %mul_ok1542
mul_overflow1543:
  call void @panic(ptr @.str.288)
  unreachable
mul_ok1542:
  %t5147 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5147)
  store i64 %t5144, ptr %t5147
  %t5148 = load i64, ptr %t5103
  %t5149 = inttoptr i64 %t5148 to ptr
  %t5150 = alloca ptr
  store ptr %t5149, ptr %t5150
  %t5151 = load ptr, ptr %t5150
  %t5152 = load i64, ptr %t5151
  %t5153 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5153)
  store i64 %t5152, ptr %t5153
  %t5154 = load i64, ptr %t5153
  %t5155 = inttoptr i64 %t5154 to ptr
  %t5156 = alloca ptr
  store ptr %t5155, ptr %t5156
  %t5157 = load ptr, ptr %t5156
  %t5158 = load i64, ptr %t5147
  %t5159 = call ptr @mem_realloc(ptr %t5157, i64 %t5158)
  %t5160 = alloca ptr
  store ptr %t5159, ptr %t5160
  %t5161 = load ptr, ptr %t5150
  %t5162 = load ptr, ptr %t5160
  %t5164 = ptrtoint ptr %t5162 to i64
  store i64 %t5164, ptr %t5161
  %t5165 = load ptr, ptr %t5117
  %t5166 = load i64, ptr %t5141
  store i64 %t5166, ptr %t5165
  call void @llvm.lifetime.end.p0(i64 8, ptr %t5153)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t5147)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t5141)
  br label %if.end1539
if.end1539:
  %t5167 = load i64, ptr %t5103
  %t5168 = inttoptr i64 %t5167 to ptr
  %t5169 = alloca ptr
  store ptr %t5168, ptr %t5169
  %t5170 = load ptr, ptr %t5169
  %t5171 = load i64, ptr %t5170
  %t5172 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5172)
  store i64 %t5171, ptr %t5172
  %t5173 = load i64, ptr %t5172
  %t5174 = load i64, ptr %t5127
  %t5175 = load i64, ptr %t5133
  %t5177 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5174, i64 %t5175)
  %t5176 = extractvalue { i64, i1 } %t5177, 0
  %t5178 = extractvalue { i64, i1 } %t5177, 1
  br i1 %t5178, label %mul_overflow1545, label %mul_ok1544
mul_overflow1545:
  call void @panic(ptr @.str.289)
  unreachable
mul_ok1544:
  %t5180 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5173, i64 %t5176)
  %t5179 = extractvalue { i64, i1 } %t5180, 0
  %t5181 = extractvalue { i64, i1 } %t5180, 1
  br i1 %t5181, label %add_overflow1547, label %add_ok1546
add_overflow1547:
  call void @panic(ptr @.str.150)
  unreachable
add_ok1546:
  %t5182 = inttoptr i64 %t5179 to ptr
  %t5183 = alloca ptr
  store ptr %t5182, ptr %t5183
  %t5184 = load ptr, ptr %t5183
  %t5185 = load %struct.IrFunction, ptr %t5098
  store %struct.IrFunction %t5185, ptr %t5184
  %t5186 = load ptr, ptr %t5110
  %t5187 = load i64, ptr %t5127
  %t5189 = sext i32 1 to i64
  %t5190 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5187, i64 %t5189)
  %t5188 = extractvalue { i64, i1 } %t5190, 0
  %t5191 = extractvalue { i64, i1 } %t5190, 1
  br i1 %t5191, label %add_overflow1549, label %add_ok1548
add_overflow1549:
  call void @panic(ptr @.str.196)
  unreachable
add_ok1548:
  store i64 %t5188, ptr %t5186
  ret void
}

define internal %struct.List__FunctionDiff @tml_N3std11collections4list18List__FunctionDiff3newE(i64 %initial_capacity) #0 {
entry:
  %t5192 = alloca i64
  store i64 %initial_capacity, ptr %t5192
  %t5193 = load i64, ptr %t5192
  %t5194 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5194)
  store i64 %t5193, ptr %t5194
  %t5195 = load i64, ptr %t5194
  %t5197 = sext i32 4 to i64
  %t5196 = icmp slt i64 %t5195, %t5197
  br i1 %t5196, label %if.then1550, label %if.end1552
if.then1550:
  store i64 4, ptr %t5194
  br label %if.end1552
if.end1552:
  %t5198 = getelementptr inbounds %struct.FunctionDiff, ptr null, i32 1
  %t5199 = ptrtoint ptr %t5198 to i64
  %t5200 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5200)
  store i64 %t5199, ptr %t5200
  %t5201 = load i64, ptr %t5200
  %t5202 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5202)
  store i64 %t5201, ptr %t5202
  %t5203 = load i64, ptr %t5202
  %t5205 = sext i32 8 to i64
  %t5204 = icmp slt i64 %t5203, %t5205
  br i1 %t5204, label %if.then1553, label %if.end1555
if.then1553:
  store i64 8, ptr %t5202
  br label %if.end1555
if.end1555:
  %t5206 = call ptr @mem_alloc(i64 32)
  %t5207 = alloca ptr
  store ptr %t5206, ptr %t5207
  %t5208 = load ptr, ptr %t5207
  %t5210 = ptrtoint ptr %t5208 to i64
  %t5211 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5211)
  store i64 %t5210, ptr %t5211
  %t5212 = load i64, ptr %t5194
  %t5213 = load i64, ptr %t5202
  %t5215 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5212, i64 %t5213)
  %t5214 = extractvalue { i64, i1 } %t5215, 0
  %t5216 = extractvalue { i64, i1 } %t5215, 1
  br i1 %t5216, label %mul_overflow1557, label %mul_ok1556
mul_overflow1557:
  call void @panic(ptr @.str.278)
  unreachable
mul_ok1556:
  %t5217 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5217)
  store i64 %t5214, ptr %t5217
  %t5218 = load i64, ptr %t5217
  %t5219 = call ptr @mem_alloc(i64 %t5218)
  %t5220 = alloca ptr
  store ptr %t5219, ptr %t5220
  %t5221 = load i64, ptr %t5211
  %t5222 = inttoptr i64 %t5221 to ptr
  %t5223 = alloca ptr
  store ptr %t5222, ptr %t5223
  %t5224 = load ptr, ptr %t5223
  %t5225 = load ptr, ptr %t5220
  %t5227 = ptrtoint ptr %t5225 to i64
  store i64 %t5227, ptr %t5224
  %t5228 = load i64, ptr %t5211
  %t5230 = sext i32 8 to i64
  %t5231 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5228, i64 %t5230)
  %t5229 = extractvalue { i64, i1 } %t5231, 0
  %t5232 = extractvalue { i64, i1 } %t5231, 1
  br i1 %t5232, label %add_overflow1559, label %add_ok1558
add_overflow1559:
  call void @panic(ptr @.str.208)
  unreachable
add_ok1558:
  %t5233 = inttoptr i64 %t5229 to ptr
  %t5234 = alloca ptr
  store ptr %t5233, ptr %t5234
  %t5235 = load ptr, ptr %t5234
  %t5236 = sext i32 0 to i64
  store i64 %t5236, ptr %t5235
  %t5237 = load i64, ptr %t5211
  %t5239 = sext i32 16 to i64
  %t5240 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5237, i64 %t5239)
  %t5238 = extractvalue { i64, i1 } %t5240, 0
  %t5241 = extractvalue { i64, i1 } %t5240, 1
  br i1 %t5241, label %add_overflow1561, label %add_ok1560
add_overflow1561:
  call void @panic(ptr @.str.109)
  unreachable
add_ok1560:
  %t5242 = inttoptr i64 %t5238 to ptr
  %t5243 = alloca ptr
  store ptr %t5242, ptr %t5243
  %t5244 = load ptr, ptr %t5243
  %t5245 = load i64, ptr %t5194
  store i64 %t5245, ptr %t5244
  %t5246 = load i64, ptr %t5211
  %t5248 = sext i32 24 to i64
  %t5249 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5246, i64 %t5248)
  %t5247 = extractvalue { i64, i1 } %t5249, 0
  %t5250 = extractvalue { i64, i1 } %t5249, 1
  br i1 %t5250, label %add_overflow1563, label %add_ok1562
add_overflow1563:
  call void @panic(ptr @.str.279)
  unreachable
add_ok1562:
  %t5251 = inttoptr i64 %t5247 to ptr
  %t5252 = alloca ptr
  store ptr %t5251, ptr %t5252
  %t5253 = load ptr, ptr %t5252
  %t5254 = load i64, ptr %t5202
  store i64 %t5254, ptr %t5253
  %t5255 = alloca %struct.List__FunctionDiff
  %t5256 = load ptr, ptr %t5207
  %t5257 = getelementptr inbounds %struct.List__FunctionDiff, ptr %t5255, i32 0, i32 0
  store ptr %t5256, ptr %t5257
  %t5258 = load %struct.List__FunctionDiff, ptr %t5255
  ret %struct.List__FunctionDiff %t5258
}

define internal void @tml_N3std11collections4list18List__FunctionDiff4dropE(ptr %this) #0 {
entry:
  %t5259 = call {} @tml_N3std11collections4list18List__FunctionDiff7destroyE(ptr %this)
  ret void
}

define internal %struct.List__I64 @tml_N3std11collections4list9List__I643newE(i64 %initial_capacity) #0 {
entry:
  %t5260 = alloca i64
  store i64 %initial_capacity, ptr %t5260
  %t5261 = load i64, ptr %t5260
  %t5262 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5262)
  store i64 %t5261, ptr %t5262
  %t5263 = load i64, ptr %t5262
  %t5265 = sext i32 4 to i64
  %t5264 = icmp slt i64 %t5263, %t5265
  br i1 %t5264, label %if.then1564, label %if.end1566
if.then1564:
  store i64 4, ptr %t5262
  br label %if.end1566
if.end1566:
  %t5266 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5266)
  store i64 8, ptr %t5266
  %t5267 = load i64, ptr %t5266
  %t5268 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5268)
  store i64 %t5267, ptr %t5268
  %t5269 = load i64, ptr %t5268
  %t5271 = sext i32 8 to i64
  %t5270 = icmp slt i64 %t5269, %t5271
  br i1 %t5270, label %if.then1567, label %if.end1569
if.then1567:
  store i64 8, ptr %t5268
  br label %if.end1569
if.end1569:
  %t5272 = call ptr @mem_alloc(i64 32)
  %t5273 = alloca ptr
  store ptr %t5272, ptr %t5273
  %t5274 = load ptr, ptr %t5273
  %t5276 = ptrtoint ptr %t5274 to i64
  %t5277 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5277)
  store i64 %t5276, ptr %t5277
  %t5278 = load i64, ptr %t5262
  %t5279 = load i64, ptr %t5268
  %t5281 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5278, i64 %t5279)
  %t5280 = extractvalue { i64, i1 } %t5281, 0
  %t5282 = extractvalue { i64, i1 } %t5281, 1
  br i1 %t5282, label %mul_overflow1571, label %mul_ok1570
mul_overflow1571:
  call void @panic(ptr @.str.278)
  unreachable
mul_ok1570:
  %t5283 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5283)
  store i64 %t5280, ptr %t5283
  %t5284 = load i64, ptr %t5283
  %t5285 = call ptr @mem_alloc(i64 %t5284)
  %t5286 = alloca ptr
  store ptr %t5285, ptr %t5286
  %t5287 = load i64, ptr %t5277
  %t5288 = inttoptr i64 %t5287 to ptr
  %t5289 = alloca ptr
  store ptr %t5288, ptr %t5289
  %t5290 = load ptr, ptr %t5289
  %t5291 = load ptr, ptr %t5286
  %t5293 = ptrtoint ptr %t5291 to i64
  store i64 %t5293, ptr %t5290
  %t5294 = load i64, ptr %t5277
  %t5296 = sext i32 8 to i64
  %t5297 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5294, i64 %t5296)
  %t5295 = extractvalue { i64, i1 } %t5297, 0
  %t5298 = extractvalue { i64, i1 } %t5297, 1
  br i1 %t5298, label %add_overflow1573, label %add_ok1572
add_overflow1573:
  call void @panic(ptr @.str.208)
  unreachable
add_ok1572:
  %t5299 = inttoptr i64 %t5295 to ptr
  %t5300 = alloca ptr
  store ptr %t5299, ptr %t5300
  %t5301 = load ptr, ptr %t5300
  %t5302 = sext i32 0 to i64
  store i64 %t5302, ptr %t5301
  %t5303 = load i64, ptr %t5277
  %t5305 = sext i32 16 to i64
  %t5306 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5303, i64 %t5305)
  %t5304 = extractvalue { i64, i1 } %t5306, 0
  %t5307 = extractvalue { i64, i1 } %t5306, 1
  br i1 %t5307, label %add_overflow1575, label %add_ok1574
add_overflow1575:
  call void @panic(ptr @.str.109)
  unreachable
add_ok1574:
  %t5308 = inttoptr i64 %t5304 to ptr
  %t5309 = alloca ptr
  store ptr %t5308, ptr %t5309
  %t5310 = load ptr, ptr %t5309
  %t5311 = load i64, ptr %t5262
  store i64 %t5311, ptr %t5310
  %t5312 = load i64, ptr %t5277
  %t5314 = sext i32 24 to i64
  %t5315 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5312, i64 %t5314)
  %t5313 = extractvalue { i64, i1 } %t5315, 0
  %t5316 = extractvalue { i64, i1 } %t5315, 1
  br i1 %t5316, label %add_overflow1577, label %add_ok1576
add_overflow1577:
  call void @panic(ptr @.str.279)
  unreachable
add_ok1576:
  %t5317 = inttoptr i64 %t5313 to ptr
  %t5318 = alloca ptr
  store ptr %t5317, ptr %t5318
  %t5319 = load ptr, ptr %t5318
  %t5320 = load i64, ptr %t5268
  store i64 %t5320, ptr %t5319
  %t5321 = alloca %struct.List__I64
  %t5322 = load ptr, ptr %t5273
  %t5323 = getelementptr inbounds %struct.List__I64, ptr %t5321, i32 0, i32 0
  store ptr %t5322, ptr %t5323
  %t5324 = load %struct.List__I64, ptr %t5321
  ret %struct.List__I64 %t5324
}

define internal i64 @tml_N3std11collections4list16List__IrFunction3lenE(ptr %this) #0 {
entry:
  %t5325 = getelementptr inbounds %struct.List__IrFunction, ptr %this, i32 0, i32 0
  %t5326 = load ptr, ptr %t5325
  %t5328 = ptrtoint ptr %t5326 to i64
  %t5329 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5329)
  store i64 %t5328, ptr %t5329
  %t5330 = load i64, ptr %t5329
  %t5332 = sext i32 8 to i64
  %t5333 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5330, i64 %t5332)
  %t5331 = extractvalue { i64, i1 } %t5333, 0
  %t5334 = extractvalue { i64, i1 } %t5333, 1
  br i1 %t5334, label %add_overflow1579, label %add_ok1578
add_overflow1579:
  call void @panic(ptr @.str.280)
  unreachable
add_ok1578:
  %t5335 = inttoptr i64 %t5331 to ptr
  %t5336 = alloca ptr
  store ptr %t5335, ptr %t5336
  %t5337 = load ptr, ptr %t5336
  %t5338 = load i64, ptr %t5337
  ret i64 %t5338
}

define internal void @tml_N3std11collections4list9List__I644dropE(ptr %this) #0 {
entry:
  %t5339 = call {} @tml_N3std11collections4list9List__I647destroyE(ptr %this)
  ret void
}

define internal %struct.IrFunction @tml_N3std11collections4list16List__IrFunction3getE(ptr %this, i64 %index) #0 {
entry:
  %t5340 = alloca i64
  store i64 %index, ptr %t5340
  %t5341 = getelementptr inbounds %struct.List__IrFunction, ptr %this, i32 0, i32 0
  %t5342 = load ptr, ptr %t5341
  %t5344 = ptrtoint ptr %t5342 to i64
  %t5345 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5345)
  store i64 %t5344, ptr %t5345
  %t5346 = load i64, ptr %t5345
  %t5348 = sext i32 24 to i64
  %t5349 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5346, i64 %t5348)
  %t5347 = extractvalue { i64, i1 } %t5349, 0
  %t5350 = extractvalue { i64, i1 } %t5349, 1
  br i1 %t5350, label %add_overflow1581, label %add_ok1580
add_overflow1581:
  call void @panic(ptr @.str.281)
  unreachable
add_ok1580:
  %t5351 = inttoptr i64 %t5347 to ptr
  %t5352 = alloca ptr
  store ptr %t5351, ptr %t5352
  %t5353 = load ptr, ptr %t5352
  %t5354 = load i64, ptr %t5353
  %t5355 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5355)
  store i64 %t5354, ptr %t5355
  %t5356 = load i64, ptr %t5345
  %t5357 = inttoptr i64 %t5356 to ptr
  %t5358 = alloca ptr
  store ptr %t5357, ptr %t5358
  %t5359 = load ptr, ptr %t5358
  %t5360 = load i64, ptr %t5359
  %t5361 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5361)
  store i64 %t5360, ptr %t5361
  %t5362 = load i64, ptr %t5361
  %t5363 = load i64, ptr %t5340
  %t5364 = load i64, ptr %t5355
  %t5366 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5363, i64 %t5364)
  %t5365 = extractvalue { i64, i1 } %t5366, 0
  %t5367 = extractvalue { i64, i1 } %t5366, 1
  br i1 %t5367, label %mul_overflow1583, label %mul_ok1582
mul_overflow1583:
  call void @panic(ptr @.str.282)
  unreachable
mul_ok1582:
  %t5369 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5362, i64 %t5365)
  %t5368 = extractvalue { i64, i1 } %t5369, 0
  %t5370 = extractvalue { i64, i1 } %t5369, 1
  br i1 %t5370, label %add_overflow1585, label %add_ok1584
add_overflow1585:
  call void @panic(ptr @.str.283)
  unreachable
add_ok1584:
  %t5371 = inttoptr i64 %t5368 to ptr
  %t5372 = alloca ptr
  store ptr %t5371, ptr %t5372
  %t5373 = load ptr, ptr %t5372
  %t5374 = load %struct.IrFunction, ptr %t5373
  %t5375 = alloca %struct.IrFunction
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t5375)
  store %struct.IrFunction %t5374, ptr %t5375
  %t5376 = load %struct.IrFunction, ptr %t5375
  ret %struct.IrFunction %t5376
}

define internal void @tml_N3std11collections4list9List__I649push__I64E(ptr %this, i64 %value) #0 {
entry:
  %t5377 = alloca i64
  store i64 %value, ptr %t5377
  %t5378 = getelementptr inbounds %struct.List__I64, ptr %this, i32 0, i32 0
  %t5379 = load ptr, ptr %t5378
  %t5381 = ptrtoint ptr %t5379 to i64
  %t5382 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5382)
  store i64 %t5381, ptr %t5382
  %t5383 = load i64, ptr %t5382
  %t5385 = sext i32 8 to i64
  %t5386 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5383, i64 %t5385)
  %t5384 = extractvalue { i64, i1 } %t5386, 0
  %t5387 = extractvalue { i64, i1 } %t5386, 1
  br i1 %t5387, label %add_overflow1587, label %add_ok1586
add_overflow1587:
  call void @panic(ptr @.str.284)
  unreachable
add_ok1586:
  %t5388 = inttoptr i64 %t5384 to ptr
  %t5389 = alloca ptr
  store ptr %t5388, ptr %t5389
  %t5390 = load i64, ptr %t5382
  %t5392 = sext i32 16 to i64
  %t5393 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5390, i64 %t5392)
  %t5391 = extractvalue { i64, i1 } %t5393, 0
  %t5394 = extractvalue { i64, i1 } %t5393, 1
  br i1 %t5394, label %add_overflow1589, label %add_ok1588
add_overflow1589:
  call void @panic(ptr @.str.285)
  unreachable
add_ok1588:
  %t5395 = inttoptr i64 %t5391 to ptr
  %t5396 = alloca ptr
  store ptr %t5395, ptr %t5396
  %t5397 = load i64, ptr %t5382
  %t5399 = sext i32 24 to i64
  %t5400 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5397, i64 %t5399)
  %t5398 = extractvalue { i64, i1 } %t5400, 0
  %t5401 = extractvalue { i64, i1 } %t5400, 1
  br i1 %t5401, label %add_overflow1591, label %add_ok1590
add_overflow1591:
  call void @panic(ptr @.str.286)
  unreachable
add_ok1590:
  %t5402 = inttoptr i64 %t5398 to ptr
  %t5403 = alloca ptr
  store ptr %t5402, ptr %t5403
  %t5404 = load ptr, ptr %t5389
  %t5405 = load i64, ptr %t5404
  %t5406 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5406)
  store i64 %t5405, ptr %t5406
  %t5407 = load ptr, ptr %t5396
  %t5408 = load i64, ptr %t5407
  %t5409 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5409)
  store i64 %t5408, ptr %t5409
  %t5410 = load ptr, ptr %t5403
  %t5411 = load i64, ptr %t5410
  %t5412 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5412)
  store i64 %t5411, ptr %t5412
  %t5413 = load i64, ptr %t5406
  %t5414 = load i64, ptr %t5409
  %t5415 = icmp sge i64 %t5413, %t5414
  br i1 %t5415, label %if.then1592, label %if.end1594
if.then1592:
  %t5416 = load i64, ptr %t5409
  %t5418 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5416, i64 2)
  %t5417 = extractvalue { i64, i1 } %t5418, 0
  %t5419 = extractvalue { i64, i1 } %t5418, 1
  br i1 %t5419, label %mul_overflow1596, label %mul_ok1595
mul_overflow1596:
  call void @panic(ptr @.str.287)
  unreachable
mul_ok1595:
  %t5420 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5420)
  store i64 %t5417, ptr %t5420
  %t5421 = load i64, ptr %t5420
  %t5422 = load i64, ptr %t5412
  %t5424 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5421, i64 %t5422)
  %t5423 = extractvalue { i64, i1 } %t5424, 0
  %t5425 = extractvalue { i64, i1 } %t5424, 1
  br i1 %t5425, label %mul_overflow1598, label %mul_ok1597
mul_overflow1598:
  call void @panic(ptr @.str.288)
  unreachable
mul_ok1597:
  %t5426 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5426)
  store i64 %t5423, ptr %t5426
  %t5427 = load i64, ptr %t5382
  %t5428 = inttoptr i64 %t5427 to ptr
  %t5429 = alloca ptr
  store ptr %t5428, ptr %t5429
  %t5430 = load ptr, ptr %t5429
  %t5431 = load i64, ptr %t5430
  %t5432 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5432)
  store i64 %t5431, ptr %t5432
  %t5433 = load i64, ptr %t5432
  %t5434 = inttoptr i64 %t5433 to ptr
  %t5435 = alloca ptr
  store ptr %t5434, ptr %t5435
  %t5436 = load ptr, ptr %t5435
  %t5437 = load i64, ptr %t5426
  %t5438 = call ptr @mem_realloc(ptr %t5436, i64 %t5437)
  %t5439 = alloca ptr
  store ptr %t5438, ptr %t5439
  %t5440 = load ptr, ptr %t5429
  %t5441 = load ptr, ptr %t5439
  %t5443 = ptrtoint ptr %t5441 to i64
  store i64 %t5443, ptr %t5440
  %t5444 = load ptr, ptr %t5396
  %t5445 = load i64, ptr %t5420
  store i64 %t5445, ptr %t5444
  call void @llvm.lifetime.end.p0(i64 8, ptr %t5432)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t5426)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t5420)
  br label %if.end1594
if.end1594:
  %t5446 = load i64, ptr %t5382
  %t5447 = inttoptr i64 %t5446 to ptr
  %t5448 = alloca ptr
  store ptr %t5447, ptr %t5448
  %t5449 = load ptr, ptr %t5448
  %t5450 = load i64, ptr %t5449
  %t5451 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5451)
  store i64 %t5450, ptr %t5451
  %t5452 = load i64, ptr %t5451
  %t5453 = load i64, ptr %t5406
  %t5454 = load i64, ptr %t5412
  %t5456 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5453, i64 %t5454)
  %t5455 = extractvalue { i64, i1 } %t5456, 0
  %t5457 = extractvalue { i64, i1 } %t5456, 1
  br i1 %t5457, label %mul_overflow1600, label %mul_ok1599
mul_overflow1600:
  call void @panic(ptr @.str.289)
  unreachable
mul_ok1599:
  %t5459 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5452, i64 %t5455)
  %t5458 = extractvalue { i64, i1 } %t5459, 0
  %t5460 = extractvalue { i64, i1 } %t5459, 1
  br i1 %t5460, label %add_overflow1602, label %add_ok1601
add_overflow1602:
  call void @panic(ptr @.str.150)
  unreachable
add_ok1601:
  %t5461 = inttoptr i64 %t5458 to ptr
  %t5462 = alloca ptr
  store ptr %t5461, ptr %t5462
  %t5463 = load ptr, ptr %t5462
  %t5464 = load i64, ptr %t5377
  store i64 %t5464, ptr %t5463
  %t5465 = load ptr, ptr %t5389
  %t5466 = load i64, ptr %t5406
  %t5468 = sext i32 1 to i64
  %t5469 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5466, i64 %t5468)
  %t5467 = extractvalue { i64, i1 } %t5469, 0
  %t5470 = extractvalue { i64, i1 } %t5469, 1
  br i1 %t5470, label %add_overflow1604, label %add_ok1603
add_overflow1604:
  call void @panic(ptr @.str.196)
  unreachable
add_ok1603:
  store i64 %t5467, ptr %t5465
  ret void
}

define internal void @tml_N3std11collections4list18List__FunctionDiff18push__FunctionDiffE(ptr %this, %struct.FunctionDiff %value) #0 {
entry:
  %t5471 = alloca %struct.FunctionDiff
  store %struct.FunctionDiff %value, ptr %t5471
  %t5472 = getelementptr inbounds %struct.List__FunctionDiff, ptr %this, i32 0, i32 0
  %t5473 = load ptr, ptr %t5472
  %t5475 = ptrtoint ptr %t5473 to i64
  %t5476 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5476)
  store i64 %t5475, ptr %t5476
  %t5477 = load i64, ptr %t5476
  %t5479 = sext i32 8 to i64
  %t5480 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5477, i64 %t5479)
  %t5478 = extractvalue { i64, i1 } %t5480, 0
  %t5481 = extractvalue { i64, i1 } %t5480, 1
  br i1 %t5481, label %add_overflow1606, label %add_ok1605
add_overflow1606:
  call void @panic(ptr @.str.284)
  unreachable
add_ok1605:
  %t5482 = inttoptr i64 %t5478 to ptr
  %t5483 = alloca ptr
  store ptr %t5482, ptr %t5483
  %t5484 = load i64, ptr %t5476
  %t5486 = sext i32 16 to i64
  %t5487 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5484, i64 %t5486)
  %t5485 = extractvalue { i64, i1 } %t5487, 0
  %t5488 = extractvalue { i64, i1 } %t5487, 1
  br i1 %t5488, label %add_overflow1608, label %add_ok1607
add_overflow1608:
  call void @panic(ptr @.str.285)
  unreachable
add_ok1607:
  %t5489 = inttoptr i64 %t5485 to ptr
  %t5490 = alloca ptr
  store ptr %t5489, ptr %t5490
  %t5491 = load i64, ptr %t5476
  %t5493 = sext i32 24 to i64
  %t5494 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5491, i64 %t5493)
  %t5492 = extractvalue { i64, i1 } %t5494, 0
  %t5495 = extractvalue { i64, i1 } %t5494, 1
  br i1 %t5495, label %add_overflow1610, label %add_ok1609
add_overflow1610:
  call void @panic(ptr @.str.286)
  unreachable
add_ok1609:
  %t5496 = inttoptr i64 %t5492 to ptr
  %t5497 = alloca ptr
  store ptr %t5496, ptr %t5497
  %t5498 = load ptr, ptr %t5483
  %t5499 = load i64, ptr %t5498
  %t5500 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5500)
  store i64 %t5499, ptr %t5500
  %t5501 = load ptr, ptr %t5490
  %t5502 = load i64, ptr %t5501
  %t5503 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5503)
  store i64 %t5502, ptr %t5503
  %t5504 = load ptr, ptr %t5497
  %t5505 = load i64, ptr %t5504
  %t5506 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5506)
  store i64 %t5505, ptr %t5506
  %t5507 = load i64, ptr %t5500
  %t5508 = load i64, ptr %t5503
  %t5509 = icmp sge i64 %t5507, %t5508
  br i1 %t5509, label %if.then1611, label %if.end1613
if.then1611:
  %t5510 = load i64, ptr %t5503
  %t5512 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5510, i64 2)
  %t5511 = extractvalue { i64, i1 } %t5512, 0
  %t5513 = extractvalue { i64, i1 } %t5512, 1
  br i1 %t5513, label %mul_overflow1615, label %mul_ok1614
mul_overflow1615:
  call void @panic(ptr @.str.287)
  unreachable
mul_ok1614:
  %t5514 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5514)
  store i64 %t5511, ptr %t5514
  %t5515 = load i64, ptr %t5514
  %t5516 = load i64, ptr %t5506
  %t5518 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5515, i64 %t5516)
  %t5517 = extractvalue { i64, i1 } %t5518, 0
  %t5519 = extractvalue { i64, i1 } %t5518, 1
  br i1 %t5519, label %mul_overflow1617, label %mul_ok1616
mul_overflow1617:
  call void @panic(ptr @.str.288)
  unreachable
mul_ok1616:
  %t5520 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5520)
  store i64 %t5517, ptr %t5520
  %t5521 = load i64, ptr %t5476
  %t5522 = inttoptr i64 %t5521 to ptr
  %t5523 = alloca ptr
  store ptr %t5522, ptr %t5523
  %t5524 = load ptr, ptr %t5523
  %t5525 = load i64, ptr %t5524
  %t5526 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5526)
  store i64 %t5525, ptr %t5526
  %t5527 = load i64, ptr %t5526
  %t5528 = inttoptr i64 %t5527 to ptr
  %t5529 = alloca ptr
  store ptr %t5528, ptr %t5529
  %t5530 = load ptr, ptr %t5529
  %t5531 = load i64, ptr %t5520
  %t5532 = call ptr @mem_realloc(ptr %t5530, i64 %t5531)
  %t5533 = alloca ptr
  store ptr %t5532, ptr %t5533
  %t5534 = load ptr, ptr %t5523
  %t5535 = load ptr, ptr %t5533
  %t5537 = ptrtoint ptr %t5535 to i64
  store i64 %t5537, ptr %t5534
  %t5538 = load ptr, ptr %t5490
  %t5539 = load i64, ptr %t5514
  store i64 %t5539, ptr %t5538
  call void @llvm.lifetime.end.p0(i64 8, ptr %t5526)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t5520)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t5514)
  br label %if.end1613
if.end1613:
  %t5540 = load i64, ptr %t5476
  %t5541 = inttoptr i64 %t5540 to ptr
  %t5542 = alloca ptr
  store ptr %t5541, ptr %t5542
  %t5543 = load ptr, ptr %t5542
  %t5544 = load i64, ptr %t5543
  %t5545 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5545)
  store i64 %t5544, ptr %t5545
  %t5546 = load i64, ptr %t5545
  %t5547 = load i64, ptr %t5500
  %t5548 = load i64, ptr %t5506
  %t5550 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5547, i64 %t5548)
  %t5549 = extractvalue { i64, i1 } %t5550, 0
  %t5551 = extractvalue { i64, i1 } %t5550, 1
  br i1 %t5551, label %mul_overflow1619, label %mul_ok1618
mul_overflow1619:
  call void @panic(ptr @.str.289)
  unreachable
mul_ok1618:
  %t5553 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5546, i64 %t5549)
  %t5552 = extractvalue { i64, i1 } %t5553, 0
  %t5554 = extractvalue { i64, i1 } %t5553, 1
  br i1 %t5554, label %add_overflow1621, label %add_ok1620
add_overflow1621:
  call void @panic(ptr @.str.150)
  unreachable
add_ok1620:
  %t5555 = inttoptr i64 %t5552 to ptr
  %t5556 = alloca ptr
  store ptr %t5555, ptr %t5556
  %t5557 = load ptr, ptr %t5556
  %t5558 = load %struct.FunctionDiff, ptr %t5471
  store %struct.FunctionDiff %t5558, ptr %t5557
  %t5559 = load ptr, ptr %t5483
  %t5560 = load i64, ptr %t5500
  %t5562 = sext i32 1 to i64
  %t5563 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5560, i64 %t5562)
  %t5561 = extractvalue { i64, i1 } %t5563, 0
  %t5564 = extractvalue { i64, i1 } %t5563, 1
  br i1 %t5564, label %add_overflow1623, label %add_ok1622
add_overflow1623:
  call void @panic(ptr @.str.196)
  unreachable
add_ok1622:
  store i64 %t5561, ptr %t5559
  ret void
}

define internal i1 @tml_N3std11collections4list9List__Str8is_emptyE(ptr %this) #0 {
entry:
  %t5565 = call i64 @tml_N3std11collections4list9List__Str3lenE(ptr %this)
  %t5567 = sext i32 0 to i64
  %t5566 = icmp eq i64 %t5565, %t5567
  ret i1 %t5566
}

define internal i1 @tml_N3std11collections4list18List__FunctionDiff8is_emptyE(ptr %this) #0 {
entry:
  %t5568 = call i64 @tml_N3std11collections4list18List__FunctionDiff3lenE(ptr %this)
  %t5570 = sext i32 0 to i64
  %t5569 = icmp eq i64 %t5568, %t5570
  ret i1 %t5569
}

define internal %struct.FunctionDiff @tml_N3std11collections4list18List__FunctionDiff3getE(ptr %this, i64 %index) #0 {
entry:
  %t5571 = alloca i64
  store i64 %index, ptr %t5571
  %t5572 = getelementptr inbounds %struct.List__FunctionDiff, ptr %this, i32 0, i32 0
  %t5573 = load ptr, ptr %t5572
  %t5575 = ptrtoint ptr %t5573 to i64
  %t5576 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5576)
  store i64 %t5575, ptr %t5576
  %t5577 = load i64, ptr %t5576
  %t5579 = sext i32 24 to i64
  %t5580 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5577, i64 %t5579)
  %t5578 = extractvalue { i64, i1 } %t5580, 0
  %t5581 = extractvalue { i64, i1 } %t5580, 1
  br i1 %t5581, label %add_overflow1625, label %add_ok1624
add_overflow1625:
  call void @panic(ptr @.str.281)
  unreachable
add_ok1624:
  %t5582 = inttoptr i64 %t5578 to ptr
  %t5583 = alloca ptr
  store ptr %t5582, ptr %t5583
  %t5584 = load ptr, ptr %t5583
  %t5585 = load i64, ptr %t5584
  %t5586 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5586)
  store i64 %t5585, ptr %t5586
  %t5587 = load i64, ptr %t5576
  %t5588 = inttoptr i64 %t5587 to ptr
  %t5589 = alloca ptr
  store ptr %t5588, ptr %t5589
  %t5590 = load ptr, ptr %t5589
  %t5591 = load i64, ptr %t5590
  %t5592 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5592)
  store i64 %t5591, ptr %t5592
  %t5593 = load i64, ptr %t5592
  %t5594 = load i64, ptr %t5571
  %t5595 = load i64, ptr %t5586
  %t5597 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5594, i64 %t5595)
  %t5596 = extractvalue { i64, i1 } %t5597, 0
  %t5598 = extractvalue { i64, i1 } %t5597, 1
  br i1 %t5598, label %mul_overflow1627, label %mul_ok1626
mul_overflow1627:
  call void @panic(ptr @.str.282)
  unreachable
mul_ok1626:
  %t5600 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5593, i64 %t5596)
  %t5599 = extractvalue { i64, i1 } %t5600, 0
  %t5601 = extractvalue { i64, i1 } %t5600, 1
  br i1 %t5601, label %add_overflow1629, label %add_ok1628
add_overflow1629:
  call void @panic(ptr @.str.283)
  unreachable
add_ok1628:
  %t5602 = inttoptr i64 %t5599 to ptr
  %t5603 = alloca ptr
  store ptr %t5602, ptr %t5603
  %t5604 = load ptr, ptr %t5603
  %t5605 = load %struct.FunctionDiff, ptr %t5604
  %t5606 = alloca %struct.FunctionDiff
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t5606)
  store %struct.FunctionDiff %t5605, ptr %t5606
  %t5607 = load %struct.FunctionDiff, ptr %t5606
  ret %struct.FunctionDiff %t5607
}

define internal i64 @tml_N3std11collections4list18List__FunctionDiff3lenE(ptr %this) #0 {
entry:
  %t5608 = getelementptr inbounds %struct.List__FunctionDiff, ptr %this, i32 0, i32 0
  %t5609 = load ptr, ptr %t5608
  %t5611 = ptrtoint ptr %t5609 to i64
  %t5612 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5612)
  store i64 %t5611, ptr %t5612
  %t5613 = load i64, ptr %t5612
  %t5615 = sext i32 8 to i64
  %t5616 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5613, i64 %t5615)
  %t5614 = extractvalue { i64, i1 } %t5616, 0
  %t5617 = extractvalue { i64, i1 } %t5616, 1
  br i1 %t5617, label %add_overflow1631, label %add_ok1630
add_overflow1631:
  call void @panic(ptr @.str.280)
  unreachable
add_ok1630:
  %t5618 = inttoptr i64 %t5614 to ptr
  %t5619 = alloca ptr
  store ptr %t5618, ptr %t5619
  %t5620 = load ptr, ptr %t5619
  %t5621 = load i64, ptr %t5620
  ret i64 %t5621
}

define internal void @tml_N3std11collections4list8List__U84dropE(ptr %this) #0 {
entry:
  %t5622 = call {} @tml_N3std11collections4list8List__U87destroyE(ptr %this)
  ret void
}

define internal i8 @tml_N3std11collections4list8List__U83getE(ptr %this, i64 %index) #0 {
entry:
  %t5623 = alloca i64
  store i64 %index, ptr %t5623
  %t5624 = getelementptr inbounds %struct.List__U8, ptr %this, i32 0, i32 0
  %t5625 = load ptr, ptr %t5624
  %t5627 = ptrtoint ptr %t5625 to i64
  %t5628 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5628)
  store i64 %t5627, ptr %t5628
  %t5629 = load i64, ptr %t5628
  %t5631 = sext i32 24 to i64
  %t5632 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5629, i64 %t5631)
  %t5630 = extractvalue { i64, i1 } %t5632, 0
  %t5633 = extractvalue { i64, i1 } %t5632, 1
  br i1 %t5633, label %add_overflow1633, label %add_ok1632
add_overflow1633:
  call void @panic(ptr @.str.281)
  unreachable
add_ok1632:
  %t5634 = inttoptr i64 %t5630 to ptr
  %t5635 = alloca ptr
  store ptr %t5634, ptr %t5635
  %t5636 = load ptr, ptr %t5635
  %t5637 = load i64, ptr %t5636
  %t5638 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5638)
  store i64 %t5637, ptr %t5638
  %t5639 = load i64, ptr %t5628
  %t5640 = inttoptr i64 %t5639 to ptr
  %t5641 = alloca ptr
  store ptr %t5640, ptr %t5641
  %t5642 = load ptr, ptr %t5641
  %t5643 = load i64, ptr %t5642
  %t5644 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5644)
  store i64 %t5643, ptr %t5644
  %t5645 = load i64, ptr %t5644
  %t5646 = load i64, ptr %t5623
  %t5647 = load i64, ptr %t5638
  %t5649 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5646, i64 %t5647)
  %t5648 = extractvalue { i64, i1 } %t5649, 0
  %t5650 = extractvalue { i64, i1 } %t5649, 1
  br i1 %t5650, label %mul_overflow1635, label %mul_ok1634
mul_overflow1635:
  call void @panic(ptr @.str.282)
  unreachable
mul_ok1634:
  %t5652 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5645, i64 %t5648)
  %t5651 = extractvalue { i64, i1 } %t5652, 0
  %t5653 = extractvalue { i64, i1 } %t5652, 1
  br i1 %t5653, label %add_overflow1637, label %add_ok1636
add_overflow1637:
  call void @panic(ptr @.str.283)
  unreachable
add_ok1636:
  %t5654 = inttoptr i64 %t5651 to ptr
  %t5655 = alloca ptr
  store ptr %t5654, ptr %t5655
  %t5656 = load ptr, ptr %t5655
  %t5657 = load i8, ptr %t5656
  %t5658 = alloca i8
  call void @llvm.lifetime.start.p0(i64 1, ptr %t5658)
  store i8 %t5657, ptr %t5658
  %t5659 = load i8, ptr %t5658
  ret i8 %t5659
}

define internal i64 @tml_N3std11collections4list9List__I643lenE(ptr %this) #0 {
entry:
  %t5660 = getelementptr inbounds %struct.List__I64, ptr %this, i32 0, i32 0
  %t5661 = load ptr, ptr %t5660
  %t5663 = ptrtoint ptr %t5661 to i64
  %t5664 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5664)
  store i64 %t5663, ptr %t5664
  %t5665 = load i64, ptr %t5664
  %t5667 = sext i32 8 to i64
  %t5668 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5665, i64 %t5667)
  %t5666 = extractvalue { i64, i1 } %t5668, 0
  %t5669 = extractvalue { i64, i1 } %t5668, 1
  br i1 %t5669, label %add_overflow1639, label %add_ok1638
add_overflow1639:
  call void @panic(ptr @.str.280)
  unreachable
add_ok1638:
  %t5670 = inttoptr i64 %t5666 to ptr
  %t5671 = alloca ptr
  store ptr %t5670, ptr %t5671
  %t5672 = load ptr, ptr %t5671
  %t5673 = load i64, ptr %t5672
  ret i64 %t5673
}

define internal i64 @tml_N3std11collections4list9List__I643getE(ptr %this, i64 %index) #0 {
entry:
  %t5674 = alloca i64
  store i64 %index, ptr %t5674
  %t5675 = getelementptr inbounds %struct.List__I64, ptr %this, i32 0, i32 0
  %t5676 = load ptr, ptr %t5675
  %t5678 = ptrtoint ptr %t5676 to i64
  %t5679 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5679)
  store i64 %t5678, ptr %t5679
  %t5680 = load i64, ptr %t5679
  %t5682 = sext i32 24 to i64
  %t5683 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5680, i64 %t5682)
  %t5681 = extractvalue { i64, i1 } %t5683, 0
  %t5684 = extractvalue { i64, i1 } %t5683, 1
  br i1 %t5684, label %add_overflow1641, label %add_ok1640
add_overflow1641:
  call void @panic(ptr @.str.281)
  unreachable
add_ok1640:
  %t5685 = inttoptr i64 %t5681 to ptr
  %t5686 = alloca ptr
  store ptr %t5685, ptr %t5686
  %t5687 = load ptr, ptr %t5686
  %t5688 = load i64, ptr %t5687
  %t5689 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5689)
  store i64 %t5688, ptr %t5689
  %t5690 = load i64, ptr %t5679
  %t5691 = inttoptr i64 %t5690 to ptr
  %t5692 = alloca ptr
  store ptr %t5691, ptr %t5692
  %t5693 = load ptr, ptr %t5692
  %t5694 = load i64, ptr %t5693
  %t5695 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5695)
  store i64 %t5694, ptr %t5695
  %t5696 = load i64, ptr %t5695
  %t5697 = load i64, ptr %t5674
  %t5698 = load i64, ptr %t5689
  %t5700 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5697, i64 %t5698)
  %t5699 = extractvalue { i64, i1 } %t5700, 0
  %t5701 = extractvalue { i64, i1 } %t5700, 1
  br i1 %t5701, label %mul_overflow1643, label %mul_ok1642
mul_overflow1643:
  call void @panic(ptr @.str.282)
  unreachable
mul_ok1642:
  %t5703 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5696, i64 %t5699)
  %t5702 = extractvalue { i64, i1 } %t5703, 0
  %t5704 = extractvalue { i64, i1 } %t5703, 1
  br i1 %t5704, label %add_overflow1645, label %add_ok1644
add_overflow1645:
  call void @panic(ptr @.str.283)
  unreachable
add_ok1644:
  %t5705 = inttoptr i64 %t5702 to ptr
  %t5706 = alloca ptr
  store ptr %t5705, ptr %t5706
  %t5707 = load ptr, ptr %t5706
  %t5708 = load i64, ptr %t5707
  %t5709 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5709)
  store i64 %t5708, ptr %t5709
  %t5710 = load i64, ptr %t5709
  ret i64 %t5710
}

define internal i64 @tml_N3std11collections4list13List__IrBlock3lenE(ptr %this) #0 {
entry:
  %t5711 = getelementptr inbounds %struct.List__IrBlock, ptr %this, i32 0, i32 0
  %t5712 = load ptr, ptr %t5711
  %t5714 = ptrtoint ptr %t5712 to i64
  %t5715 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5715)
  store i64 %t5714, ptr %t5715
  %t5716 = load i64, ptr %t5715
  %t5718 = sext i32 8 to i64
  %t5719 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5716, i64 %t5718)
  %t5717 = extractvalue { i64, i1 } %t5719, 0
  %t5720 = extractvalue { i64, i1 } %t5719, 1
  br i1 %t5720, label %add_overflow1647, label %add_ok1646
add_overflow1647:
  call void @panic(ptr @.str.280)
  unreachable
add_ok1646:
  %t5721 = inttoptr i64 %t5717 to ptr
  %t5722 = alloca ptr
  store ptr %t5721, ptr %t5722
  %t5723 = load ptr, ptr %t5722
  %t5724 = load i64, ptr %t5723
  ret i64 %t5724
}

define internal %struct.IrBlock @tml_N3std11collections4list13List__IrBlock3getE(ptr %this, i64 %index) #0 {
entry:
  %t5725 = alloca i64
  store i64 %index, ptr %t5725
  %t5726 = getelementptr inbounds %struct.List__IrBlock, ptr %this, i32 0, i32 0
  %t5727 = load ptr, ptr %t5726
  %t5729 = ptrtoint ptr %t5727 to i64
  %t5730 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5730)
  store i64 %t5729, ptr %t5730
  %t5731 = load i64, ptr %t5730
  %t5733 = sext i32 24 to i64
  %t5734 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5731, i64 %t5733)
  %t5732 = extractvalue { i64, i1 } %t5734, 0
  %t5735 = extractvalue { i64, i1 } %t5734, 1
  br i1 %t5735, label %add_overflow1649, label %add_ok1648
add_overflow1649:
  call void @panic(ptr @.str.281)
  unreachable
add_ok1648:
  %t5736 = inttoptr i64 %t5732 to ptr
  %t5737 = alloca ptr
  store ptr %t5736, ptr %t5737
  %t5738 = load ptr, ptr %t5737
  %t5739 = load i64, ptr %t5738
  %t5740 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5740)
  store i64 %t5739, ptr %t5740
  %t5741 = load i64, ptr %t5730
  %t5742 = inttoptr i64 %t5741 to ptr
  %t5743 = alloca ptr
  store ptr %t5742, ptr %t5743
  %t5744 = load ptr, ptr %t5743
  %t5745 = load i64, ptr %t5744
  %t5746 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5746)
  store i64 %t5745, ptr %t5746
  %t5747 = load i64, ptr %t5746
  %t5748 = load i64, ptr %t5725
  %t5749 = load i64, ptr %t5740
  %t5751 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5748, i64 %t5749)
  %t5750 = extractvalue { i64, i1 } %t5751, 0
  %t5752 = extractvalue { i64, i1 } %t5751, 1
  br i1 %t5752, label %mul_overflow1651, label %mul_ok1650
mul_overflow1651:
  call void @panic(ptr @.str.282)
  unreachable
mul_ok1650:
  %t5754 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5747, i64 %t5750)
  %t5753 = extractvalue { i64, i1 } %t5754, 0
  %t5755 = extractvalue { i64, i1 } %t5754, 1
  br i1 %t5755, label %add_overflow1653, label %add_ok1652
add_overflow1653:
  call void @panic(ptr @.str.283)
  unreachable
add_ok1652:
  %t5756 = inttoptr i64 %t5753 to ptr
  %t5757 = alloca ptr
  store ptr %t5756, ptr %t5757
  %t5758 = load ptr, ptr %t5757
  %t5759 = load %struct.IrBlock, ptr %t5758
  %t5760 = alloca %struct.IrBlock
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t5760)
  store %struct.IrBlock %t5759, ptr %t5760
  %t5761 = load %struct.IrBlock, ptr %t5760
  ret %struct.IrBlock %t5761
}

define internal i64 @tml_N3std11collections4list13List__IrInstr3lenE(ptr %this) #0 {
entry:
  %t5762 = getelementptr inbounds %struct.List__IrInstr, ptr %this, i32 0, i32 0
  %t5763 = load ptr, ptr %t5762
  %t5765 = ptrtoint ptr %t5763 to i64
  %t5766 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5766)
  store i64 %t5765, ptr %t5766
  %t5767 = load i64, ptr %t5766
  %t5769 = sext i32 8 to i64
  %t5770 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5767, i64 %t5769)
  %t5768 = extractvalue { i64, i1 } %t5770, 0
  %t5771 = extractvalue { i64, i1 } %t5770, 1
  br i1 %t5771, label %add_overflow1655, label %add_ok1654
add_overflow1655:
  call void @panic(ptr @.str.280)
  unreachable
add_ok1654:
  %t5772 = inttoptr i64 %t5768 to ptr
  %t5773 = alloca ptr
  store ptr %t5772, ptr %t5773
  %t5774 = load ptr, ptr %t5773
  %t5775 = load i64, ptr %t5774
  ret i64 %t5775
}

define internal %struct.IrInstr @tml_N3std11collections4list13List__IrInstr3getE(ptr %this, i64 %index) #0 {
entry:
  %t5776 = alloca i64
  store i64 %index, ptr %t5776
  %t5777 = getelementptr inbounds %struct.List__IrInstr, ptr %this, i32 0, i32 0
  %t5778 = load ptr, ptr %t5777
  %t5780 = ptrtoint ptr %t5778 to i64
  %t5781 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5781)
  store i64 %t5780, ptr %t5781
  %t5782 = load i64, ptr %t5781
  %t5784 = sext i32 24 to i64
  %t5785 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5782, i64 %t5784)
  %t5783 = extractvalue { i64, i1 } %t5785, 0
  %t5786 = extractvalue { i64, i1 } %t5785, 1
  br i1 %t5786, label %add_overflow1657, label %add_ok1656
add_overflow1657:
  call void @panic(ptr @.str.281)
  unreachable
add_ok1656:
  %t5787 = inttoptr i64 %t5783 to ptr
  %t5788 = alloca ptr
  store ptr %t5787, ptr %t5788
  %t5789 = load ptr, ptr %t5788
  %t5790 = load i64, ptr %t5789
  %t5791 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5791)
  store i64 %t5790, ptr %t5791
  %t5792 = load i64, ptr %t5781
  %t5793 = inttoptr i64 %t5792 to ptr
  %t5794 = alloca ptr
  store ptr %t5793, ptr %t5794
  %t5795 = load ptr, ptr %t5794
  %t5796 = load i64, ptr %t5795
  %t5797 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5797)
  store i64 %t5796, ptr %t5797
  %t5798 = load i64, ptr %t5797
  %t5799 = load i64, ptr %t5776
  %t5800 = load i64, ptr %t5791
  %t5802 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5799, i64 %t5800)
  %t5801 = extractvalue { i64, i1 } %t5802, 0
  %t5803 = extractvalue { i64, i1 } %t5802, 1
  br i1 %t5803, label %mul_overflow1659, label %mul_ok1658
mul_overflow1659:
  call void @panic(ptr @.str.282)
  unreachable
mul_ok1658:
  %t5805 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5798, i64 %t5801)
  %t5804 = extractvalue { i64, i1 } %t5805, 0
  %t5806 = extractvalue { i64, i1 } %t5805, 1
  br i1 %t5806, label %add_overflow1661, label %add_ok1660
add_overflow1661:
  call void @panic(ptr @.str.283)
  unreachable
add_ok1660:
  %t5807 = inttoptr i64 %t5804 to ptr
  %t5808 = alloca ptr
  store ptr %t5807, ptr %t5808
  %t5809 = load ptr, ptr %t5808
  %t5810 = load %struct.IrInstr, ptr %t5809
  %t5811 = alloca %struct.IrInstr
  call void @llvm.lifetime.start.p0(i64 -1, ptr %t5811)
  store %struct.IrInstr %t5810, ptr %t5811
  %t5812 = load %struct.IrInstr, ptr %t5811
  ret %struct.IrInstr %t5812
}

define internal %struct.List__IrParam @tml_N3std11collections4list13List__IrParam3newE(i64 %initial_capacity) #0 {
entry:
  %t5813 = alloca i64
  store i64 %initial_capacity, ptr %t5813
  %t5814 = load i64, ptr %t5813
  %t5815 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5815)
  store i64 %t5814, ptr %t5815
  %t5816 = load i64, ptr %t5815
  %t5818 = sext i32 4 to i64
  %t5817 = icmp slt i64 %t5816, %t5818
  br i1 %t5817, label %if.then1662, label %if.end1664
if.then1662:
  store i64 4, ptr %t5815
  br label %if.end1664
if.end1664:
  %t5819 = getelementptr inbounds %struct.IrParam, ptr null, i32 1
  %t5820 = ptrtoint ptr %t5819 to i64
  %t5821 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5821)
  store i64 %t5820, ptr %t5821
  %t5822 = load i64, ptr %t5821
  %t5823 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5823)
  store i64 %t5822, ptr %t5823
  %t5824 = load i64, ptr %t5823
  %t5826 = sext i32 8 to i64
  %t5825 = icmp slt i64 %t5824, %t5826
  br i1 %t5825, label %if.then1665, label %if.end1667
if.then1665:
  store i64 8, ptr %t5823
  br label %if.end1667
if.end1667:
  %t5827 = call ptr @mem_alloc(i64 32)
  %t5828 = alloca ptr
  store ptr %t5827, ptr %t5828
  %t5829 = load ptr, ptr %t5828
  %t5831 = ptrtoint ptr %t5829 to i64
  %t5832 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5832)
  store i64 %t5831, ptr %t5832
  %t5833 = load i64, ptr %t5815
  %t5834 = load i64, ptr %t5823
  %t5836 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5833, i64 %t5834)
  %t5835 = extractvalue { i64, i1 } %t5836, 0
  %t5837 = extractvalue { i64, i1 } %t5836, 1
  br i1 %t5837, label %mul_overflow1669, label %mul_ok1668
mul_overflow1669:
  call void @panic(ptr @.str.278)
  unreachable
mul_ok1668:
  %t5838 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5838)
  store i64 %t5835, ptr %t5838
  %t5839 = load i64, ptr %t5838
  %t5840 = call ptr @mem_alloc(i64 %t5839)
  %t5841 = alloca ptr
  store ptr %t5840, ptr %t5841
  %t5842 = load i64, ptr %t5832
  %t5843 = inttoptr i64 %t5842 to ptr
  %t5844 = alloca ptr
  store ptr %t5843, ptr %t5844
  %t5845 = load ptr, ptr %t5844
  %t5846 = load ptr, ptr %t5841
  %t5848 = ptrtoint ptr %t5846 to i64
  store i64 %t5848, ptr %t5845
  %t5849 = load i64, ptr %t5832
  %t5851 = sext i32 8 to i64
  %t5852 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5849, i64 %t5851)
  %t5850 = extractvalue { i64, i1 } %t5852, 0
  %t5853 = extractvalue { i64, i1 } %t5852, 1
  br i1 %t5853, label %add_overflow1671, label %add_ok1670
add_overflow1671:
  call void @panic(ptr @.str.208)
  unreachable
add_ok1670:
  %t5854 = inttoptr i64 %t5850 to ptr
  %t5855 = alloca ptr
  store ptr %t5854, ptr %t5855
  %t5856 = load ptr, ptr %t5855
  %t5857 = sext i32 0 to i64
  store i64 %t5857, ptr %t5856
  %t5858 = load i64, ptr %t5832
  %t5860 = sext i32 16 to i64
  %t5861 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5858, i64 %t5860)
  %t5859 = extractvalue { i64, i1 } %t5861, 0
  %t5862 = extractvalue { i64, i1 } %t5861, 1
  br i1 %t5862, label %add_overflow1673, label %add_ok1672
add_overflow1673:
  call void @panic(ptr @.str.109)
  unreachable
add_ok1672:
  %t5863 = inttoptr i64 %t5859 to ptr
  %t5864 = alloca ptr
  store ptr %t5863, ptr %t5864
  %t5865 = load ptr, ptr %t5864
  %t5866 = load i64, ptr %t5815
  store i64 %t5866, ptr %t5865
  %t5867 = load i64, ptr %t5832
  %t5869 = sext i32 24 to i64
  %t5870 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5867, i64 %t5869)
  %t5868 = extractvalue { i64, i1 } %t5870, 0
  %t5871 = extractvalue { i64, i1 } %t5870, 1
  br i1 %t5871, label %add_overflow1675, label %add_ok1674
add_overflow1675:
  call void @panic(ptr @.str.279)
  unreachable
add_ok1674:
  %t5872 = inttoptr i64 %t5868 to ptr
  %t5873 = alloca ptr
  store ptr %t5872, ptr %t5873
  %t5874 = load ptr, ptr %t5873
  %t5875 = load i64, ptr %t5823
  store i64 %t5875, ptr %t5874
  %t5876 = alloca %struct.List__IrParam
  %t5877 = load ptr, ptr %t5828
  %t5878 = getelementptr inbounds %struct.List__IrParam, ptr %t5876, i32 0, i32 0
  store ptr %t5877, ptr %t5878
  %t5879 = load %struct.List__IrParam, ptr %t5876
  ret %struct.List__IrParam %t5879
}

define internal void @tml_N3std11collections4list13List__IrParam13push__IrParamE(ptr %this, %struct.IrParam %value) #0 {
entry:
  %t5880 = alloca %struct.IrParam
  store %struct.IrParam %value, ptr %t5880
  %t5881 = getelementptr inbounds %struct.List__IrParam, ptr %this, i32 0, i32 0
  %t5882 = load ptr, ptr %t5881
  %t5884 = ptrtoint ptr %t5882 to i64
  %t5885 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5885)
  store i64 %t5884, ptr %t5885
  %t5886 = load i64, ptr %t5885
  %t5888 = sext i32 8 to i64
  %t5889 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5886, i64 %t5888)
  %t5887 = extractvalue { i64, i1 } %t5889, 0
  %t5890 = extractvalue { i64, i1 } %t5889, 1
  br i1 %t5890, label %add_overflow1677, label %add_ok1676
add_overflow1677:
  call void @panic(ptr @.str.284)
  unreachable
add_ok1676:
  %t5891 = inttoptr i64 %t5887 to ptr
  %t5892 = alloca ptr
  store ptr %t5891, ptr %t5892
  %t5893 = load i64, ptr %t5885
  %t5895 = sext i32 16 to i64
  %t5896 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5893, i64 %t5895)
  %t5894 = extractvalue { i64, i1 } %t5896, 0
  %t5897 = extractvalue { i64, i1 } %t5896, 1
  br i1 %t5897, label %add_overflow1679, label %add_ok1678
add_overflow1679:
  call void @panic(ptr @.str.285)
  unreachable
add_ok1678:
  %t5898 = inttoptr i64 %t5894 to ptr
  %t5899 = alloca ptr
  store ptr %t5898, ptr %t5899
  %t5900 = load i64, ptr %t5885
  %t5902 = sext i32 24 to i64
  %t5903 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5900, i64 %t5902)
  %t5901 = extractvalue { i64, i1 } %t5903, 0
  %t5904 = extractvalue { i64, i1 } %t5903, 1
  br i1 %t5904, label %add_overflow1681, label %add_ok1680
add_overflow1681:
  call void @panic(ptr @.str.286)
  unreachable
add_ok1680:
  %t5905 = inttoptr i64 %t5901 to ptr
  %t5906 = alloca ptr
  store ptr %t5905, ptr %t5906
  %t5907 = load ptr, ptr %t5892
  %t5908 = load i64, ptr %t5907
  %t5909 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5909)
  store i64 %t5908, ptr %t5909
  %t5910 = load ptr, ptr %t5899
  %t5911 = load i64, ptr %t5910
  %t5912 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5912)
  store i64 %t5911, ptr %t5912
  %t5913 = load ptr, ptr %t5906
  %t5914 = load i64, ptr %t5913
  %t5915 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5915)
  store i64 %t5914, ptr %t5915
  %t5916 = load i64, ptr %t5909
  %t5917 = load i64, ptr %t5912
  %t5918 = icmp sge i64 %t5916, %t5917
  br i1 %t5918, label %if.then1682, label %if.end1684
if.then1682:
  %t5919 = load i64, ptr %t5912
  %t5921 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5919, i64 2)
  %t5920 = extractvalue { i64, i1 } %t5921, 0
  %t5922 = extractvalue { i64, i1 } %t5921, 1
  br i1 %t5922, label %mul_overflow1686, label %mul_ok1685
mul_overflow1686:
  call void @panic(ptr @.str.287)
  unreachable
mul_ok1685:
  %t5923 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5923)
  store i64 %t5920, ptr %t5923
  %t5924 = load i64, ptr %t5923
  %t5925 = load i64, ptr %t5915
  %t5927 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5924, i64 %t5925)
  %t5926 = extractvalue { i64, i1 } %t5927, 0
  %t5928 = extractvalue { i64, i1 } %t5927, 1
  br i1 %t5928, label %mul_overflow1688, label %mul_ok1687
mul_overflow1688:
  call void @panic(ptr @.str.288)
  unreachable
mul_ok1687:
  %t5929 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5929)
  store i64 %t5926, ptr %t5929
  %t5930 = load i64, ptr %t5885
  %t5931 = inttoptr i64 %t5930 to ptr
  %t5932 = alloca ptr
  store ptr %t5931, ptr %t5932
  %t5933 = load ptr, ptr %t5932
  %t5934 = load i64, ptr %t5933
  %t5935 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5935)
  store i64 %t5934, ptr %t5935
  %t5936 = load i64, ptr %t5935
  %t5937 = inttoptr i64 %t5936 to ptr
  %t5938 = alloca ptr
  store ptr %t5937, ptr %t5938
  %t5939 = load ptr, ptr %t5938
  %t5940 = load i64, ptr %t5929
  %t5941 = call ptr @mem_realloc(ptr %t5939, i64 %t5940)
  %t5942 = alloca ptr
  store ptr %t5941, ptr %t5942
  %t5943 = load ptr, ptr %t5932
  %t5944 = load ptr, ptr %t5942
  %t5946 = ptrtoint ptr %t5944 to i64
  store i64 %t5946, ptr %t5943
  %t5947 = load ptr, ptr %t5899
  %t5948 = load i64, ptr %t5923
  store i64 %t5948, ptr %t5947
  call void @llvm.lifetime.end.p0(i64 8, ptr %t5935)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t5929)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t5923)
  br label %if.end1684
if.end1684:
  %t5949 = load i64, ptr %t5885
  %t5950 = inttoptr i64 %t5949 to ptr
  %t5951 = alloca ptr
  store ptr %t5950, ptr %t5951
  %t5952 = load ptr, ptr %t5951
  %t5953 = load i64, ptr %t5952
  %t5954 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5954)
  store i64 %t5953, ptr %t5954
  %t5955 = load i64, ptr %t5954
  %t5956 = load i64, ptr %t5909
  %t5957 = load i64, ptr %t5915
  %t5959 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5956, i64 %t5957)
  %t5958 = extractvalue { i64, i1 } %t5959, 0
  %t5960 = extractvalue { i64, i1 } %t5959, 1
  br i1 %t5960, label %mul_overflow1690, label %mul_ok1689
mul_overflow1690:
  call void @panic(ptr @.str.289)
  unreachable
mul_ok1689:
  %t5962 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5955, i64 %t5958)
  %t5961 = extractvalue { i64, i1 } %t5962, 0
  %t5963 = extractvalue { i64, i1 } %t5962, 1
  br i1 %t5963, label %add_overflow1692, label %add_ok1691
add_overflow1692:
  call void @panic(ptr @.str.150)
  unreachable
add_ok1691:
  %t5964 = inttoptr i64 %t5961 to ptr
  %t5965 = alloca ptr
  store ptr %t5964, ptr %t5965
  %t5966 = load ptr, ptr %t5965
  %t5967 = load %struct.IrParam, ptr %t5880
  store %struct.IrParam %t5967, ptr %t5966
  %t5968 = load ptr, ptr %t5892
  %t5969 = load i64, ptr %t5909
  %t5971 = sext i32 1 to i64
  %t5972 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t5969, i64 %t5971)
  %t5970 = extractvalue { i64, i1 } %t5972, 0
  %t5973 = extractvalue { i64, i1 } %t5972, 1
  br i1 %t5973, label %add_overflow1694, label %add_ok1693
add_overflow1694:
  call void @panic(ptr @.str.196)
  unreachable
add_ok1693:
  store i64 %t5970, ptr %t5968
  ret void
}

define internal %struct.List__U8 @tml_N3std11collections4list8List__U83newE(i64 %initial_capacity) #0 {
entry:
  %t5974 = alloca i64
  store i64 %initial_capacity, ptr %t5974
  %t5975 = load i64, ptr %t5974
  %t5976 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5976)
  store i64 %t5975, ptr %t5976
  %t5977 = load i64, ptr %t5976
  %t5979 = sext i32 4 to i64
  %t5978 = icmp slt i64 %t5977, %t5979
  br i1 %t5978, label %if.then1695, label %if.end1697
if.then1695:
  store i64 4, ptr %t5976
  br label %if.end1697
if.end1697:
  %t5980 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5980)
  store i64 1, ptr %t5980
  %t5981 = load i64, ptr %t5980
  %t5982 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5982)
  store i64 %t5981, ptr %t5982
  %t5983 = load i64, ptr %t5982
  %t5985 = sext i32 8 to i64
  %t5984 = icmp slt i64 %t5983, %t5985
  br i1 %t5984, label %if.then1698, label %if.end1700
if.then1698:
  store i64 8, ptr %t5982
  br label %if.end1700
if.end1700:
  %t5986 = call ptr @mem_alloc(i64 32)
  %t5987 = alloca ptr
  store ptr %t5986, ptr %t5987
  %t5988 = load ptr, ptr %t5987
  %t5990 = ptrtoint ptr %t5988 to i64
  %t5991 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5991)
  store i64 %t5990, ptr %t5991
  %t5992 = load i64, ptr %t5976
  %t5993 = load i64, ptr %t5982
  %t5995 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t5992, i64 %t5993)
  %t5994 = extractvalue { i64, i1 } %t5995, 0
  %t5996 = extractvalue { i64, i1 } %t5995, 1
  br i1 %t5996, label %mul_overflow1702, label %mul_ok1701
mul_overflow1702:
  call void @panic(ptr @.str.278)
  unreachable
mul_ok1701:
  %t5997 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t5997)
  store i64 %t5994, ptr %t5997
  %t5998 = load i64, ptr %t5997
  %t5999 = call ptr @mem_alloc(i64 %t5998)
  %t6000 = alloca ptr
  store ptr %t5999, ptr %t6000
  %t6001 = load i64, ptr %t5991
  %t6002 = inttoptr i64 %t6001 to ptr
  %t6003 = alloca ptr
  store ptr %t6002, ptr %t6003
  %t6004 = load ptr, ptr %t6003
  %t6005 = load ptr, ptr %t6000
  %t6007 = ptrtoint ptr %t6005 to i64
  store i64 %t6007, ptr %t6004
  %t6008 = load i64, ptr %t5991
  %t6010 = sext i32 8 to i64
  %t6011 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t6008, i64 %t6010)
  %t6009 = extractvalue { i64, i1 } %t6011, 0
  %t6012 = extractvalue { i64, i1 } %t6011, 1
  br i1 %t6012, label %add_overflow1704, label %add_ok1703
add_overflow1704:
  call void @panic(ptr @.str.208)
  unreachable
add_ok1703:
  %t6013 = inttoptr i64 %t6009 to ptr
  %t6014 = alloca ptr
  store ptr %t6013, ptr %t6014
  %t6015 = load ptr, ptr %t6014
  %t6016 = sext i32 0 to i64
  store i64 %t6016, ptr %t6015
  %t6017 = load i64, ptr %t5991
  %t6019 = sext i32 16 to i64
  %t6020 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t6017, i64 %t6019)
  %t6018 = extractvalue { i64, i1 } %t6020, 0
  %t6021 = extractvalue { i64, i1 } %t6020, 1
  br i1 %t6021, label %add_overflow1706, label %add_ok1705
add_overflow1706:
  call void @panic(ptr @.str.109)
  unreachable
add_ok1705:
  %t6022 = inttoptr i64 %t6018 to ptr
  %t6023 = alloca ptr
  store ptr %t6022, ptr %t6023
  %t6024 = load ptr, ptr %t6023
  %t6025 = load i64, ptr %t5976
  store i64 %t6025, ptr %t6024
  %t6026 = load i64, ptr %t5991
  %t6028 = sext i32 24 to i64
  %t6029 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t6026, i64 %t6028)
  %t6027 = extractvalue { i64, i1 } %t6029, 0
  %t6030 = extractvalue { i64, i1 } %t6029, 1
  br i1 %t6030, label %add_overflow1708, label %add_ok1707
add_overflow1708:
  call void @panic(ptr @.str.279)
  unreachable
add_ok1707:
  %t6031 = inttoptr i64 %t6027 to ptr
  %t6032 = alloca ptr
  store ptr %t6031, ptr %t6032
  %t6033 = load ptr, ptr %t6032
  %t6034 = load i64, ptr %t5982
  store i64 %t6034, ptr %t6033
  %t6035 = alloca %struct.List__U8
  %t6036 = load ptr, ptr %t5987
  %t6037 = getelementptr inbounds %struct.List__U8, ptr %t6035, i32 0, i32 0
  store ptr %t6036, ptr %t6037
  %t6038 = load %struct.List__U8, ptr %t6035
  ret %struct.List__U8 %t6038
}

define internal void @tml_N3std11collections4list8List__U88push__U8E(ptr %this, i8 %value) #0 {
entry:
  %t6039 = alloca i8
  store i8 %value, ptr %t6039
  %t6040 = getelementptr inbounds %struct.List__U8, ptr %this, i32 0, i32 0
  %t6041 = load ptr, ptr %t6040
  %t6043 = ptrtoint ptr %t6041 to i64
  %t6044 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6044)
  store i64 %t6043, ptr %t6044
  %t6045 = load i64, ptr %t6044
  %t6047 = sext i32 8 to i64
  %t6048 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t6045, i64 %t6047)
  %t6046 = extractvalue { i64, i1 } %t6048, 0
  %t6049 = extractvalue { i64, i1 } %t6048, 1
  br i1 %t6049, label %add_overflow1710, label %add_ok1709
add_overflow1710:
  call void @panic(ptr @.str.284)
  unreachable
add_ok1709:
  %t6050 = inttoptr i64 %t6046 to ptr
  %t6051 = alloca ptr
  store ptr %t6050, ptr %t6051
  %t6052 = load i64, ptr %t6044
  %t6054 = sext i32 16 to i64
  %t6055 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t6052, i64 %t6054)
  %t6053 = extractvalue { i64, i1 } %t6055, 0
  %t6056 = extractvalue { i64, i1 } %t6055, 1
  br i1 %t6056, label %add_overflow1712, label %add_ok1711
add_overflow1712:
  call void @panic(ptr @.str.285)
  unreachable
add_ok1711:
  %t6057 = inttoptr i64 %t6053 to ptr
  %t6058 = alloca ptr
  store ptr %t6057, ptr %t6058
  %t6059 = load i64, ptr %t6044
  %t6061 = sext i32 24 to i64
  %t6062 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t6059, i64 %t6061)
  %t6060 = extractvalue { i64, i1 } %t6062, 0
  %t6063 = extractvalue { i64, i1 } %t6062, 1
  br i1 %t6063, label %add_overflow1714, label %add_ok1713
add_overflow1714:
  call void @panic(ptr @.str.286)
  unreachable
add_ok1713:
  %t6064 = inttoptr i64 %t6060 to ptr
  %t6065 = alloca ptr
  store ptr %t6064, ptr %t6065
  %t6066 = load ptr, ptr %t6051
  %t6067 = load i64, ptr %t6066
  %t6068 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6068)
  store i64 %t6067, ptr %t6068
  %t6069 = load ptr, ptr %t6058
  %t6070 = load i64, ptr %t6069
  %t6071 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6071)
  store i64 %t6070, ptr %t6071
  %t6072 = load ptr, ptr %t6065
  %t6073 = load i64, ptr %t6072
  %t6074 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6074)
  store i64 %t6073, ptr %t6074
  %t6075 = load i64, ptr %t6068
  %t6076 = load i64, ptr %t6071
  %t6077 = icmp sge i64 %t6075, %t6076
  br i1 %t6077, label %if.then1715, label %if.end1717
if.then1715:
  %t6078 = load i64, ptr %t6071
  %t6080 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t6078, i64 2)
  %t6079 = extractvalue { i64, i1 } %t6080, 0
  %t6081 = extractvalue { i64, i1 } %t6080, 1
  br i1 %t6081, label %mul_overflow1719, label %mul_ok1718
mul_overflow1719:
  call void @panic(ptr @.str.287)
  unreachable
mul_ok1718:
  %t6082 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6082)
  store i64 %t6079, ptr %t6082
  %t6083 = load i64, ptr %t6082
  %t6084 = load i64, ptr %t6074
  %t6086 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t6083, i64 %t6084)
  %t6085 = extractvalue { i64, i1 } %t6086, 0
  %t6087 = extractvalue { i64, i1 } %t6086, 1
  br i1 %t6087, label %mul_overflow1721, label %mul_ok1720
mul_overflow1721:
  call void @panic(ptr @.str.288)
  unreachable
mul_ok1720:
  %t6088 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6088)
  store i64 %t6085, ptr %t6088
  %t6089 = load i64, ptr %t6044
  %t6090 = inttoptr i64 %t6089 to ptr
  %t6091 = alloca ptr
  store ptr %t6090, ptr %t6091
  %t6092 = load ptr, ptr %t6091
  %t6093 = load i64, ptr %t6092
  %t6094 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6094)
  store i64 %t6093, ptr %t6094
  %t6095 = load i64, ptr %t6094
  %t6096 = inttoptr i64 %t6095 to ptr
  %t6097 = alloca ptr
  store ptr %t6096, ptr %t6097
  %t6098 = load ptr, ptr %t6097
  %t6099 = load i64, ptr %t6088
  %t6100 = call ptr @mem_realloc(ptr %t6098, i64 %t6099)
  %t6101 = alloca ptr
  store ptr %t6100, ptr %t6101
  %t6102 = load ptr, ptr %t6091
  %t6103 = load ptr, ptr %t6101
  %t6105 = ptrtoint ptr %t6103 to i64
  store i64 %t6105, ptr %t6102
  %t6106 = load ptr, ptr %t6058
  %t6107 = load i64, ptr %t6082
  store i64 %t6107, ptr %t6106
  call void @llvm.lifetime.end.p0(i64 8, ptr %t6094)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t6088)
  call void @llvm.lifetime.end.p0(i64 8, ptr %t6082)
  br label %if.end1717
if.end1717:
  %t6108 = load i64, ptr %t6044
  %t6109 = inttoptr i64 %t6108 to ptr
  %t6110 = alloca ptr
  store ptr %t6109, ptr %t6110
  %t6111 = load ptr, ptr %t6110
  %t6112 = load i64, ptr %t6111
  %t6113 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6113)
  store i64 %t6112, ptr %t6113
  %t6114 = load i64, ptr %t6113
  %t6115 = load i64, ptr %t6068
  %t6116 = load i64, ptr %t6074
  %t6118 = call { i64, i1 } @llvm.smul.with.overflow.i64(i64 %t6115, i64 %t6116)
  %t6117 = extractvalue { i64, i1 } %t6118, 0
  %t6119 = extractvalue { i64, i1 } %t6118, 1
  br i1 %t6119, label %mul_overflow1723, label %mul_ok1722
mul_overflow1723:
  call void @panic(ptr @.str.289)
  unreachable
mul_ok1722:
  %t6121 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t6114, i64 %t6117)
  %t6120 = extractvalue { i64, i1 } %t6121, 0
  %t6122 = extractvalue { i64, i1 } %t6121, 1
  br i1 %t6122, label %add_overflow1725, label %add_ok1724
add_overflow1725:
  call void @panic(ptr @.str.150)
  unreachable
add_ok1724:
  %t6123 = inttoptr i64 %t6120 to ptr
  %t6124 = alloca ptr
  store ptr %t6123, ptr %t6124
  %t6125 = load ptr, ptr %t6124
  %t6126 = load i8, ptr %t6039
  store i8 %t6126, ptr %t6125
  %t6127 = load ptr, ptr %t6051
  %t6128 = load i64, ptr %t6068
  %t6130 = sext i32 1 to i64
  %t6131 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %t6128, i64 %t6130)
  %t6129 = extractvalue { i64, i1 } %t6131, 0
  %t6132 = extractvalue { i64, i1 } %t6131, 1
  br i1 %t6132, label %add_overflow1727, label %add_ok1726
add_overflow1727:
  call void @panic(ptr @.str.196)
  unreachable
add_ok1726:
  store i64 %t6129, ptr %t6127
  ret void
}

define internal void @tml_N3std11collections4list9List__Str7destroyE(ptr %this) #0 {
entry:
  %t6133 = getelementptr inbounds %struct.List__Str, ptr %this, i32 0, i32 0
  %t6134 = load ptr, ptr %t6133
  %t6136 = zext i32 0 to i64
  %t6135 = inttoptr i64 %t6136 to ptr
  %t6137 = icmp eq ptr %t6134, %t6135
  br i1 %t6137, label %if.then1728, label %if.end1730
if.then1728:
  ret void
if.end1730:
  %t6138 = getelementptr inbounds %struct.List__Str, ptr %this, i32 0, i32 0
  %t6139 = load ptr, ptr %t6138
  %t6141 = ptrtoint ptr %t6139 to i64
  %t6142 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6142)
  store i64 %t6141, ptr %t6142
  %t6143 = load i64, ptr %t6142
  %t6144 = inttoptr i64 %t6143 to ptr
  %t6145 = alloca ptr
  store ptr %t6144, ptr %t6145
  %t6146 = load ptr, ptr %t6145
  %t6147 = load i64, ptr %t6146
  %t6148 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6148)
  store i64 %t6147, ptr %t6148
  %t6149 = load i64, ptr %t6148
  %t6150 = inttoptr i64 %t6149 to ptr
  call void @mem_free(ptr %t6150)
  %t6151 = getelementptr inbounds %struct.List__Str, ptr %this, i32 0, i32 0
  %t6152 = load ptr, ptr %t6151
  call void @mem_free(ptr %t6152)
  %t6154 = zext i32 0 to i64
  %t6153 = inttoptr i64 %t6154 to ptr
  %t6155 = getelementptr inbounds %struct.List__Str, ptr %this, i32 0, i32 0
  store ptr %t6153, ptr %t6155
  ret void
}

define internal void @tml_N3std11collections4list16List__IrFunction7destroyE(ptr %this) #0 {
entry:
  %t6156 = getelementptr inbounds %struct.List__IrFunction, ptr %this, i32 0, i32 0
  %t6157 = load ptr, ptr %t6156
  %t6159 = zext i32 0 to i64
  %t6158 = inttoptr i64 %t6159 to ptr
  %t6160 = icmp eq ptr %t6157, %t6158
  br i1 %t6160, label %if.then1731, label %if.end1733
if.then1731:
  ret void
if.end1733:
  %t6161 = getelementptr inbounds %struct.List__IrFunction, ptr %this, i32 0, i32 0
  %t6162 = load ptr, ptr %t6161
  %t6164 = ptrtoint ptr %t6162 to i64
  %t6165 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6165)
  store i64 %t6164, ptr %t6165
  %t6166 = load i64, ptr %t6165
  %t6167 = inttoptr i64 %t6166 to ptr
  %t6168 = alloca ptr
  store ptr %t6167, ptr %t6168
  %t6169 = load ptr, ptr %t6168
  %t6170 = load i64, ptr %t6169
  %t6171 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6171)
  store i64 %t6170, ptr %t6171
  %t6172 = load i64, ptr %t6171
  %t6173 = inttoptr i64 %t6172 to ptr
  call void @mem_free(ptr %t6173)
  %t6174 = getelementptr inbounds %struct.List__IrFunction, ptr %this, i32 0, i32 0
  %t6175 = load ptr, ptr %t6174
  call void @mem_free(ptr %t6175)
  %t6177 = zext i32 0 to i64
  %t6176 = inttoptr i64 %t6177 to ptr
  %t6178 = getelementptr inbounds %struct.List__IrFunction, ptr %this, i32 0, i32 0
  store ptr %t6176, ptr %t6178
  ret void
}

define internal void @tml_N3std11collections4list14List__IrGlobal7destroyE(ptr %this) #0 {
entry:
  %t6179 = getelementptr inbounds %struct.List__IrGlobal, ptr %this, i32 0, i32 0
  %t6180 = load ptr, ptr %t6179
  %t6182 = zext i32 0 to i64
  %t6181 = inttoptr i64 %t6182 to ptr
  %t6183 = icmp eq ptr %t6180, %t6181
  br i1 %t6183, label %if.then1734, label %if.end1736
if.then1734:
  ret void
if.end1736:
  %t6184 = getelementptr inbounds %struct.List__IrGlobal, ptr %this, i32 0, i32 0
  %t6185 = load ptr, ptr %t6184
  %t6187 = ptrtoint ptr %t6185 to i64
  %t6188 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6188)
  store i64 %t6187, ptr %t6188
  %t6189 = load i64, ptr %t6188
  %t6190 = inttoptr i64 %t6189 to ptr
  %t6191 = alloca ptr
  store ptr %t6190, ptr %t6191
  %t6192 = load ptr, ptr %t6191
  %t6193 = load i64, ptr %t6192
  %t6194 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6194)
  store i64 %t6193, ptr %t6194
  %t6195 = load i64, ptr %t6194
  %t6196 = inttoptr i64 %t6195 to ptr
  call void @mem_free(ptr %t6196)
  %t6197 = getelementptr inbounds %struct.List__IrGlobal, ptr %this, i32 0, i32 0
  %t6198 = load ptr, ptr %t6197
  call void @mem_free(ptr %t6198)
  %t6200 = zext i32 0 to i64
  %t6199 = inttoptr i64 %t6200 to ptr
  %t6201 = getelementptr inbounds %struct.List__IrGlobal, ptr %this, i32 0, i32 0
  store ptr %t6199, ptr %t6201
  ret void
}

define internal void @tml_N3std11collections4list13List__IrParam7destroyE(ptr %this) #0 {
entry:
  %t6202 = getelementptr inbounds %struct.List__IrParam, ptr %this, i32 0, i32 0
  %t6203 = load ptr, ptr %t6202
  %t6205 = zext i32 0 to i64
  %t6204 = inttoptr i64 %t6205 to ptr
  %t6206 = icmp eq ptr %t6203, %t6204
  br i1 %t6206, label %if.then1737, label %if.end1739
if.then1737:
  ret void
if.end1739:
  %t6207 = getelementptr inbounds %struct.List__IrParam, ptr %this, i32 0, i32 0
  %t6208 = load ptr, ptr %t6207
  %t6210 = ptrtoint ptr %t6208 to i64
  %t6211 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6211)
  store i64 %t6210, ptr %t6211
  %t6212 = load i64, ptr %t6211
  %t6213 = inttoptr i64 %t6212 to ptr
  %t6214 = alloca ptr
  store ptr %t6213, ptr %t6214
  %t6215 = load ptr, ptr %t6214
  %t6216 = load i64, ptr %t6215
  %t6217 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6217)
  store i64 %t6216, ptr %t6217
  %t6218 = load i64, ptr %t6217
  %t6219 = inttoptr i64 %t6218 to ptr
  call void @mem_free(ptr %t6219)
  %t6220 = getelementptr inbounds %struct.List__IrParam, ptr %this, i32 0, i32 0
  %t6221 = load ptr, ptr %t6220
  call void @mem_free(ptr %t6221)
  %t6223 = zext i32 0 to i64
  %t6222 = inttoptr i64 %t6223 to ptr
  %t6224 = getelementptr inbounds %struct.List__IrParam, ptr %this, i32 0, i32 0
  store ptr %t6222, ptr %t6224
  ret void
}

define internal void @tml_N3std11collections4list13List__IrBlock7destroyE(ptr %this) #0 {
entry:
  %t6225 = getelementptr inbounds %struct.List__IrBlock, ptr %this, i32 0, i32 0
  %t6226 = load ptr, ptr %t6225
  %t6228 = zext i32 0 to i64
  %t6227 = inttoptr i64 %t6228 to ptr
  %t6229 = icmp eq ptr %t6226, %t6227
  br i1 %t6229, label %if.then1740, label %if.end1742
if.then1740:
  ret void
if.end1742:
  %t6230 = getelementptr inbounds %struct.List__IrBlock, ptr %this, i32 0, i32 0
  %t6231 = load ptr, ptr %t6230
  %t6233 = ptrtoint ptr %t6231 to i64
  %t6234 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6234)
  store i64 %t6233, ptr %t6234
  %t6235 = load i64, ptr %t6234
  %t6236 = inttoptr i64 %t6235 to ptr
  %t6237 = alloca ptr
  store ptr %t6236, ptr %t6237
  %t6238 = load ptr, ptr %t6237
  %t6239 = load i64, ptr %t6238
  %t6240 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6240)
  store i64 %t6239, ptr %t6240
  %t6241 = load i64, ptr %t6240
  %t6242 = inttoptr i64 %t6241 to ptr
  call void @mem_free(ptr %t6242)
  %t6243 = getelementptr inbounds %struct.List__IrBlock, ptr %this, i32 0, i32 0
  %t6244 = load ptr, ptr %t6243
  call void @mem_free(ptr %t6244)
  %t6246 = zext i32 0 to i64
  %t6245 = inttoptr i64 %t6246 to ptr
  %t6247 = getelementptr inbounds %struct.List__IrBlock, ptr %this, i32 0, i32 0
  store ptr %t6245, ptr %t6247
  ret void
}

define internal void @tml_N3std11collections4list13List__IrInstr7destroyE(ptr %this) #0 {
entry:
  %t6248 = getelementptr inbounds %struct.List__IrInstr, ptr %this, i32 0, i32 0
  %t6249 = load ptr, ptr %t6248
  %t6251 = zext i32 0 to i64
  %t6250 = inttoptr i64 %t6251 to ptr
  %t6252 = icmp eq ptr %t6249, %t6250
  br i1 %t6252, label %if.then1743, label %if.end1745
if.then1743:
  ret void
if.end1745:
  %t6253 = getelementptr inbounds %struct.List__IrInstr, ptr %this, i32 0, i32 0
  %t6254 = load ptr, ptr %t6253
  %t6256 = ptrtoint ptr %t6254 to i64
  %t6257 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6257)
  store i64 %t6256, ptr %t6257
  %t6258 = load i64, ptr %t6257
  %t6259 = inttoptr i64 %t6258 to ptr
  %t6260 = alloca ptr
  store ptr %t6259, ptr %t6260
  %t6261 = load ptr, ptr %t6260
  %t6262 = load i64, ptr %t6261
  %t6263 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6263)
  store i64 %t6262, ptr %t6263
  %t6264 = load i64, ptr %t6263
  %t6265 = inttoptr i64 %t6264 to ptr
  call void @mem_free(ptr %t6265)
  %t6266 = getelementptr inbounds %struct.List__IrInstr, ptr %this, i32 0, i32 0
  %t6267 = load ptr, ptr %t6266
  call void @mem_free(ptr %t6267)
  %t6269 = zext i32 0 to i64
  %t6268 = inttoptr i64 %t6269 to ptr
  %t6270 = getelementptr inbounds %struct.List__IrInstr, ptr %this, i32 0, i32 0
  store ptr %t6268, ptr %t6270
  ret void
}

define internal void @tml_N3std11collections4list18List__FunctionDiff7destroyE(ptr %this) #0 {
entry:
  %t6271 = getelementptr inbounds %struct.List__FunctionDiff, ptr %this, i32 0, i32 0
  %t6272 = load ptr, ptr %t6271
  %t6274 = zext i32 0 to i64
  %t6273 = inttoptr i64 %t6274 to ptr
  %t6275 = icmp eq ptr %t6272, %t6273
  br i1 %t6275, label %if.then1746, label %if.end1748
if.then1746:
  ret void
if.end1748:
  %t6276 = getelementptr inbounds %struct.List__FunctionDiff, ptr %this, i32 0, i32 0
  %t6277 = load ptr, ptr %t6276
  %t6279 = ptrtoint ptr %t6277 to i64
  %t6280 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6280)
  store i64 %t6279, ptr %t6280
  %t6281 = load i64, ptr %t6280
  %t6282 = inttoptr i64 %t6281 to ptr
  %t6283 = alloca ptr
  store ptr %t6282, ptr %t6283
  %t6284 = load ptr, ptr %t6283
  %t6285 = load i64, ptr %t6284
  %t6286 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6286)
  store i64 %t6285, ptr %t6286
  %t6287 = load i64, ptr %t6286
  %t6288 = inttoptr i64 %t6287 to ptr
  call void @mem_free(ptr %t6288)
  %t6289 = getelementptr inbounds %struct.List__FunctionDiff, ptr %this, i32 0, i32 0
  %t6290 = load ptr, ptr %t6289
  call void @mem_free(ptr %t6290)
  %t6292 = zext i32 0 to i64
  %t6291 = inttoptr i64 %t6292 to ptr
  %t6293 = getelementptr inbounds %struct.List__FunctionDiff, ptr %this, i32 0, i32 0
  store ptr %t6291, ptr %t6293
  ret void
}

define internal void @tml_N3std11collections4list9List__I647destroyE(ptr %this) #0 {
entry:
  %t6294 = getelementptr inbounds %struct.List__I64, ptr %this, i32 0, i32 0
  %t6295 = load ptr, ptr %t6294
  %t6297 = zext i32 0 to i64
  %t6296 = inttoptr i64 %t6297 to ptr
  %t6298 = icmp eq ptr %t6295, %t6296
  br i1 %t6298, label %if.then1749, label %if.end1751
if.then1749:
  ret void
if.end1751:
  %t6299 = getelementptr inbounds %struct.List__I64, ptr %this, i32 0, i32 0
  %t6300 = load ptr, ptr %t6299
  %t6302 = ptrtoint ptr %t6300 to i64
  %t6303 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6303)
  store i64 %t6302, ptr %t6303
  %t6304 = load i64, ptr %t6303
  %t6305 = inttoptr i64 %t6304 to ptr
  %t6306 = alloca ptr
  store ptr %t6305, ptr %t6306
  %t6307 = load ptr, ptr %t6306
  %t6308 = load i64, ptr %t6307
  %t6309 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6309)
  store i64 %t6308, ptr %t6309
  %t6310 = load i64, ptr %t6309
  %t6311 = inttoptr i64 %t6310 to ptr
  call void @mem_free(ptr %t6311)
  %t6312 = getelementptr inbounds %struct.List__I64, ptr %this, i32 0, i32 0
  %t6313 = load ptr, ptr %t6312
  call void @mem_free(ptr %t6313)
  %t6315 = zext i32 0 to i64
  %t6314 = inttoptr i64 %t6315 to ptr
  %t6316 = getelementptr inbounds %struct.List__I64, ptr %this, i32 0, i32 0
  store ptr %t6314, ptr %t6316
  ret void
}

define internal void @tml_N3std11collections4list8List__U87destroyE(ptr %this) #0 {
entry:
  %t6317 = getelementptr inbounds %struct.List__U8, ptr %this, i32 0, i32 0
  %t6318 = load ptr, ptr %t6317
  %t6320 = zext i32 0 to i64
  %t6319 = inttoptr i64 %t6320 to ptr
  %t6321 = icmp eq ptr %t6318, %t6319
  br i1 %t6321, label %if.then1752, label %if.end1754
if.then1752:
  ret void
if.end1754:
  %t6322 = getelementptr inbounds %struct.List__U8, ptr %this, i32 0, i32 0
  %t6323 = load ptr, ptr %t6322
  %t6325 = ptrtoint ptr %t6323 to i64
  %t6326 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6326)
  store i64 %t6325, ptr %t6326
  %t6327 = load i64, ptr %t6326
  %t6328 = inttoptr i64 %t6327 to ptr
  %t6329 = alloca ptr
  store ptr %t6328, ptr %t6329
  %t6330 = load ptr, ptr %t6329
  %t6331 = load i64, ptr %t6330
  %t6332 = alloca i64
  call void @llvm.lifetime.start.p0(i64 8, ptr %t6332)
  store i64 %t6331, ptr %t6332
  %t6333 = load i64, ptr %t6332
  %t6334 = inttoptr i64 %t6333 to ptr
  call void @mem_free(ptr %t6334)
  %t6335 = getelementptr inbounds %struct.List__U8, ptr %this, i32 0, i32 0
  %t6336 = load ptr, ptr %t6335
  call void @mem_free(ptr %t6336)
  %t6338 = zext i32 0 to i64
  %t6337 = inttoptr i64 %t6338 to ptr
  %t6339 = getelementptr inbounds %struct.List__U8, ptr %this, i32 0, i32 0
  store ptr %t6337, ptr %t6339
  ret void
}
; String constants
@.str.0 = private constant [3 x i8] c"rb\00"
@.str.1 = private constant [1 x i8] c"\00"
@.str.2 = private constant [36 x i8] c"integer overflow on addition at :79\00"
@.str.3 = private constant [36 x i8] c"integer overflow on addition at :87\00"
@.str.4 = private constant [71 x i8] c"Usage: ir-diff <file_a.ll> <file_b.ll> [--summary] [--function <name>]\00"
@.str.5 = private constant [59 x i8] c"Compare two LLVM IR files and report semantic differences.\00"
@.str.6 = private constant [9 x i8] c"Options:\00"
@.str.7 = private constant [62 x i8] c"  --summary        Print only summary (number of differences)\00"
@.str.8 = private constant [51 x i8] c"  --function <n>   Compare only the named function\00"
@.str.9 = private constant [12 x i8] c"Exit codes:\00"
@.str.10 = private constant [38 x i8] c"  0  Files are semantically identical\00"
@.str.11 = private constant [18 x i8] c"  1  Files differ\00"
@.str.12 = private constant [38 x i8] c"  2  Usage error or file read failure\00"
@.str.13 = private constant [37 x i8] c"integer overflow on addition at :137\00"
@.str.14 = private constant [3 x i8] c"--\00"
@.str.15 = private constant [7 x i8] c"--help\00"
@.str.16 = private constant [10 x i8] c"--summary\00"
@.str.17 = private constant [11 x i8] c"--function\00"
@.str.18 = private constant [37 x i8] c"integer overflow on addition at :157\00"
@.str.19 = private constant [37 x i8] c"integer overflow on addition at :159\00"
@.str.20 = private constant [29 x i8] c"error: could not read file: \00"
@.str.21 = private constant [24 x i8] c"error: failed to parse \00"
@.str.22 = private constant [19 x i8] c"error: diff failed\00"
@.str.23 = private constant [37 x i8] c"IR files are semantically identical.\00"
@.str.24 = private constant [21 x i8] c"Functions only in A:\00"
@.str.25 = private constant [3 x i8] c"  \00"
@.str.26 = private constant [37 x i8] c"integer overflow on addition at :237\00"
@.str.27 = private constant [21 x i8] c"Functions only in B:\00"
@.str.28 = private constant [37 x i8] c"integer overflow on addition at :246\00"
@.str.29 = private constant [28 x i8] c"Functions with differences:\00"
@.str.30 = private constant [37 x i8] c"integer overflow on addition at :255\00"
@.str.31 = private constant [8 x i8] c"--- a: \00"
@.str.32 = private constant [39 x i8] c"  (function exists only in first file)\00"
@.str.33 = private constant [37 x i8] c"integer overflow on addition at :267\00"
@.str.34 = private constant [8 x i8] c"+++ b: \00"
@.str.35 = private constant [40 x i8] c"  (function exists only in second file)\00"
@.str.36 = private constant [37 x i8] c"integer overflow on addition at :276\00"
@.str.37 = private constant [5 x i8] c"@@@ \00"
@.str.38 = private constant [5 x i8] c" @@@\00"
@.str.39 = private constant [3 x i8] c"- \00"
@.str.40 = private constant [3 x i8] c"+ \00"
@.str.41 = private constant [37 x i8] c"integer overflow on addition at :289\00"
@.str.42 = private constant [3 x i8] c"0x\00"
@.str.43 = private constant [37 x i8] c"integer overflow on addition at :391\00"
@.str.44 = private constant [37 x i8] c"integer overflow on addition at :394\00"
@.str.45 = private constant [2 x i8] c"!\00"
@.str.46 = private constant [4 x i8] c" = \00"
@.str.47 = private constant [37 x i8] c"integer overflow on addition at :408\00"
@.str.48 = private constant [2 x i8] c"@\00"
@.str.49 = private constant [37 x i8] c"integer overflow on addition at :418\00"
@.str.50 = private constant [9 x i8] c"declare \00"
@.str.51 = private constant [37 x i8] c"integer overflow on addition at :424\00"
@.str.52 = private constant [8 x i8] c"define \00"
@.str.53 = private constant [26 x i8] c"malformed function header\00"
@.str.54 = private constant [37 x i8] c"integer overflow on addition at :450\00"
@.str.55 = private constant [2 x i8] c"}\00"
@.str.56 = private constant [37 x i8] c"integer overflow on addition at :457\00"
@.str.57 = private constant [37 x i8] c"integer overflow on addition at :461\00"
@.str.58 = private constant [2 x i8] c";\00"
@.str.59 = private constant [37 x i8] c"integer overflow on addition at :465\00"
@.str.60 = private constant [40 x i8] c"integer overflow on subtraction at :477\00"
@.str.61 = private constant [37 x i8] c"integer overflow on addition at :480\00"
@.str.62 = private constant [6 x i8] c"entry\00"
@.str.63 = private constant [37 x i8] c"integer overflow on addition at :492\00"
@.str.64 = private constant [45 x i8] c"function body not closed before end of input\00"
@.str.65 = private constant [37 x i8] c"integer overflow on addition at :520\00"
@.str.66 = private constant [3 x i8] c"0o\00"
@.str.67 = private constant [37 x i8] c"integer overflow on addition at :326\00"
@.str.68 = private constant [37 x i8] c"integer overflow on addition at :336\00"
@.str.69 = private constant [3 x i8] c"0b\00"
@.str.70 = private constant [5 x i8] c"true\00"
@.str.71 = private constant [6 x i8] c"false\00"
@.str.72 = private constant [2 x i8] c"\22\00"
@.str.73 = private constant [2 x i8] c"'\00"
@.str.74 = private constant [2 x i8] c"0\00"
@.str.75 = private constant [37 x i8] c"integer overflow on addition at :286\00"
@.str.76 = private constant [37 x i8] c"integer overflow on addition at :290\00"
@.str.77 = private constant [37 x i8] c"integer overflow on addition at :294\00"
@.str.78 = private constant [40 x i8] c"integer overflow on subtraction at :297\00"
@.str.79 = private constant [37 x i8] c"integer overflow on addition at :300\00"
@.str.80 = private constant [37 x i8] c"integer overflow on addition at :301\00"
@.str.81 = private constant [40 x i8] c"integer overflow on subtraction at :303\00"
@.str.82 = private constant [17 x i8] c"0123456789ABCDEF\00"
@.str.83 = private constant [17 x i8] c"0123456789abcdef\00"
@.str.84 = private constant [37 x i8] c"integer overflow on addition at :327\00"
@.str.85 = private constant [37 x i8] c"integer overflow on addition at :331\00"
@.str.86 = private constant [37 x i8] c"integer overflow on addition at :335\00"
@.str.87 = private constant [40 x i8] c"integer overflow on subtraction at :338\00"
@.str.88 = private constant [37 x i8] c"integer overflow on addition at :341\00"
@.str.89 = private constant [37 x i8] c"integer overflow on addition at :342\00"
@.str.90 = private constant [40 x i8] c"integer overflow on subtraction at :344\00"
@.str.91 = private constant [37 x i8] c"integer overflow on addition at :416\00"
@.str.92 = private constant [36 x i8] c"integer overflow on addition at :49\00"
@.str.93 = private constant [36 x i8] c"integer overflow on addition at :54\00"
@.str.94 = private constant [36 x i8] c"integer overflow on addition at :58\00"
@.str.95 = private constant [39 x i8] c"integer overflow on subtraction at :58\00"
@.str.96 = private constant [39 x i8] c"integer overflow on subtraction at :63\00"
@.str.97 = private constant [201 x i8] c"00010203040506070809101112131415161718192021222324252627282930313233343536373839404142434445464748495051525354555657585960616263646566676869707172737475767778798081828384858687888990919293949596979899\00"
@.str.98 = private constant [39 x i8] c"integer overflow on subtraction at :46\00"
@.str.99 = private constant [21 x i8] c"-9223372036854775808\00"
@.str.100 = private constant [39 x i8] c"integer overflow on subtraction at :55\00"
@.str.101 = private constant [36 x i8] c"integer overflow on addition at :60\00"
@.str.102 = private constant [36 x i8] c"integer overflow on addition at :61\00"
@.str.103 = private constant [36 x i8] c"integer overflow on addition at :66\00"
@.str.104 = private constant [39 x i8] c"integer overflow on subtraction at :69\00"
@.str.105 = private constant [42 x i8] c"integer overflow on multiplication at :73\00"
@.str.106 = private constant [36 x i8] c"integer overflow on addition at :74\00"
@.str.107 = private constant [36 x i8] c"integer overflow on addition at :75\00"
@.str.108 = private constant [36 x i8] c"integer overflow on addition at :76\00"
@.str.109 = private constant [36 x i8] c"integer overflow on addition at :77\00"
@.str.110 = private constant [39 x i8] c"integer overflow on subtraction at :77\00"
@.str.111 = private constant [39 x i8] c"integer overflow on subtraction at :78\00"
@.str.112 = private constant [42 x i8] c"integer overflow on multiplication at :83\00"
@.str.113 = private constant [36 x i8] c"integer overflow on addition at :84\00"
@.str.114 = private constant [36 x i8] c"integer overflow on addition at :85\00"
@.str.115 = private constant [36 x i8] c"integer overflow on addition at :86\00"
@.str.116 = private constant [39 x i8] c"integer overflow on subtraction at :87\00"
@.str.117 = private constant [36 x i8] c"integer overflow on addition at :89\00"
@.str.118 = private constant [36 x i8] c"integer overflow on addition at :90\00"
@.str.119 = private constant [40 x i8] c"integer overflow on subtraction at :151\00"
@.str.120 = private constant [37 x i8] c"integer overflow on addition at :153\00"
@.str.121 = private constant [40 x i8] c"integer overflow on subtraction at :159\00"
@.str.122 = private constant [40 x i8] c"integer overflow on subtraction at :160\00"
@.str.123 = private constant [40 x i8] c"integer overflow on subtraction at :165\00"
@.str.124 = private constant [37 x i8] c"integer overflow on addition at :169\00"
@.str.125 = private constant [40 x i8] c"integer overflow on subtraction at :169\00"
@.str.126 = private constant [40 x i8] c"integer overflow on subtraction at :170\00"
@.str.127 = private constant [37 x i8] c"integer overflow on addition at :173\00"
@.str.128 = private constant [16 x i8] c"source_filename\00"
@.str.129 = private constant [8 x i8] c"target \00"
@.str.130 = private constant [13 x i8] c"attributes #\00"
@.str.131 = private constant [6 x i8] c"!llvm\00"
@.str.132 = private constant [37 x i8] c"integer overflow on addition at :200\00"
@.str.133 = private constant [3 x i8] c" @\00"
@.str.134 = private constant [2 x i8] c" \00"
@.str.135 = private constant [40 x i8] c"integer overflow on subtraction at :237\00"
@.str.136 = private constant [37 x i8] c"integer overflow on addition at :240\00"
@.str.137 = private constant [2 x i8] c"(\00"
@.str.138 = private constant [37 x i8] c"integer overflow on addition at :249\00"
@.str.139 = private constant [37 x i8] c"integer overflow on addition at :259\00"
@.str.140 = private constant [40 x i8] c"integer overflow on subtraction at :261\00"
@.str.141 = private constant [37 x i8] c"integer overflow on addition at :273\00"
@.str.142 = private constant [2 x i8] c"{\00"
@.str.143 = private constant [40 x i8] c"integer overflow on subtraction at :279\00"
@.str.144 = private constant [37 x i8] c"integer overflow on addition at :111\00"
@.str.145 = private constant [37 x i8] c"integer overflow on addition at :116\00"
@.str.146 = private constant [40 x i8] c"integer overflow on subtraction at :119\00"
@.str.147 = private constant [43 x i8] c"integer overflow on multiplication at :124\00"
@.str.148 = private constant [37 x i8] c"integer overflow on addition at :125\00"
@.str.149 = private constant [37 x i8] c"integer overflow on addition at :126\00"
@.str.150 = private constant [37 x i8] c"integer overflow on addition at :127\00"
@.str.151 = private constant [37 x i8] c"integer overflow on addition at :128\00"
@.str.152 = private constant [40 x i8] c"integer overflow on subtraction at :128\00"
@.str.153 = private constant [40 x i8] c"integer overflow on subtraction at :129\00"
@.str.154 = private constant [43 x i8] c"integer overflow on multiplication at :134\00"
@.str.155 = private constant [37 x i8] c"integer overflow on addition at :135\00"
@.str.156 = private constant [37 x i8] c"integer overflow on addition at :136\00"
@.str.157 = private constant [37 x i8] c"integer overflow on addition at :138\00"
@.str.158 = private constant [40 x i8] c"integer overflow on subtraction at :138\00"
@.str.159 = private constant [37 x i8] c"integer overflow on addition at :140\00"
@.str.160 = private constant [37 x i8] c"integer overflow on addition at :141\00"
@.str.161 = private constant [37 x i8] c"integer overflow on addition at :319\00"
@.str.162 = private constant [37 x i8] c"integer overflow on addition at :330\00"
@.str.163 = private constant [37 x i8] c"integer overflow on addition at :179\00"
@.str.164 = private constant [2 x i8] c":\00"
@.str.165 = private constant [40 x i8] c"integer overflow on subtraction at :541\00"
@.str.166 = private constant [2 x i8] c"\09\00"
@.str.167 = private constant [37 x i8] c"integer overflow on addition at :225\00"
@.str.168 = private constant [37 x i8] c"integer overflow on addition at :229\00"
@.str.169 = private constant [37 x i8] c"integer overflow on addition at :233\00"
@.str.170 = private constant [40 x i8] c"integer overflow on subtraction at :236\00"
@.str.171 = private constant [37 x i8] c"integer overflow on addition at :239\00"
@.str.172 = private constant [40 x i8] c"integer overflow on subtraction at :242\00"
@.str.173 = private constant [37 x i8] c"integer overflow on addition at :139\00"
@.str.174 = private constant [4 x i8] c", !\00"
@.str.175 = private constant [36 x i8] c"integer overflow on addition at :57\00"
@.str.176 = private constant [36 x i8] c"integer overflow on addition at :67\00"
@.str.177 = private constant [2 x i8] c"%\00"
@.str.178 = private constant [37 x i8] c"integer overflow on addition at :166\00"
@.str.179 = private constant [37 x i8] c"integer overflow on addition at :152\00"
@.str.180 = private constant [37 x i8] c"integer overflow on addition at :205\00"
@.str.181 = private constant [37 x i8] c"integer overflow on addition at :207\00"
@.str.182 = private constant [37 x i8] c"integer overflow on addition at :417\00"
@.str.183 = private constant [36 x i8] c"integer overflow on addition at :52\00"
@.str.184 = private constant [39 x i8] c"integer overflow on subtraction at :90\00"
@.str.185 = private constant [39 x i8] c"integer overflow on subtraction at :95\00"
@.str.186 = private constant [40 x i8] c"integer overflow on subtraction at :102\00"
@.str.187 = private constant [40 x i8] c"integer overflow on subtraction at :118\00"
@.str.188 = private constant [37 x i8] c"integer overflow on addition at :123\00"
@.str.189 = private constant [37 x i8] c"integer overflow on addition at :541\00"
@.str.190 = private constant [37 x i8] c"integer overflow on addition at :542\00"
@.str.191 = private constant [37 x i8] c"integer overflow on addition at :559\00"
@.str.192 = private constant [37 x i8] c"integer overflow on addition at :561\00"
@.str.193 = private constant [37 x i8] c"integer overflow on addition at :566\00"
@.str.194 = private constant [37 x i8] c"integer overflow on addition at :571\00"
@.str.195 = private constant [40 x i8] c"integer overflow on subtraction at :125\00"
@.str.196 = private constant [37 x i8] c"integer overflow on addition at :131\00"
@.str.197 = private constant [37 x i8] c"integer overflow on addition at :188\00"
@.str.198 = private constant [3 x i8] c" %\00"
@.str.199 = private constant [37 x i8] c"integer overflow on addition at :198\00"
@.str.200 = private constant [37 x i8] c"integer overflow on addition at :203\00"
@.str.201 = private constant [39 x i8] c"integer overflow on subtraction at :94\00"
@.str.202 = private constant [36 x i8] c"integer overflow on addition at :95\00"
@.str.203 = private constant [37 x i8] c"integer overflow on addition at :600\00"
@.str.204 = private constant [40 x i8] c"integer overflow on subtraction at :600\00"
@.str.205 = private constant [40 x i8] c"integer overflow on subtraction at :605\00"
@.str.206 = private constant [39 x i8] c"integer overflow on subtraction at :70\00"
@.str.207 = private constant [36 x i8] c"integer overflow on addition at :71\00"
@.str.208 = private constant [36 x i8] c"integer overflow on addition at :73\00"
@.str.209 = private constant [38 x i8] c"integer overflow on addition at :1086\00"
@.str.210 = private constant [41 x i8] c"integer overflow on subtraction at :1086\00"
@.str.211 = private constant [41 x i8] c"integer overflow on subtraction at :1089\00"
@.str.212 = private constant [38 x i8] c"integer overflow on addition at :1093\00"
@.str.213 = private constant [38 x i8] c"integer overflow on addition at :1094\00"
@.str.214 = private constant [37 x i8] c"integer overflow on addition at :303\00"
@.str.215 = private constant [37 x i8] c"integer overflow on addition at :306\00"
@.str.216 = private constant [8 x i8] c"private\00"
@.str.217 = private constant [9 x i8] c"internal\00"
@.str.218 = private constant [21 x i8] c"available_externally\00"
@.str.219 = private constant [9 x i8] c"linkonce\00"
@.str.220 = private constant [5 x i8] c"weak\00"
@.str.221 = private constant [7 x i8] c"common\00"
@.str.222 = private constant [10 x i8] c"appending\00"
@.str.223 = private constant [12 x i8] c"extern_weak\00"
@.str.224 = private constant [13 x i8] c"linkonce_odr\00"
@.str.225 = private constant [9 x i8] c"weak_odr\00"
@.str.226 = private constant [9 x i8] c"external\00"
@.str.227 = private constant [7 x i8] c"hidden\00"
@.str.228 = private constant [10 x i8] c"protected\00"
@.str.229 = private constant [8 x i8] c"default\00"
@.str.230 = private constant [10 x i8] c"dllimport\00"
@.str.231 = private constant [10 x i8] c"dllexport\00"
@.str.232 = private constant [13 x i8] c"unnamed_addr\00"
@.str.233 = private constant [19 x i8] c"local_unnamed_addr\00"
@.str.234 = private constant [13 x i8] c"thread_local\00"
@.str.235 = private constant [9 x i8] c"constant\00"
@.str.236 = private constant [7 x i8] c"global\00"
@.str.237 = private constant [13 x i8] c"addrspace(0)\00"
@.str.238 = private constant [37 x i8] c"integer overflow on addition at :134\00"
@.str.239 = private constant [40 x i8] c"integer overflow on subtraction at :217\00"
@.str.240 = private constant [37 x i8] c"integer overflow on addition at :221\00"
@.str.241 = private constant [37 x i8] c"integer overflow on addition at :228\00"
@.str.242 = private constant [40 x i8] c"integer overflow on subtraction at :362\00"
@.str.243 = private constant [37 x i8] c"integer overflow on addition at :368\00"
@.str.244 = private constant [37 x i8] c"integer overflow on addition at :369\00"
@.str.245 = private constant [37 x i8] c"integer overflow on addition at :375\00"
@.str.246 = private constant [37 x i8] c"integer overflow on addition at :377\00"
@.str.247 = private constant [40 x i8] c"integer overflow on subtraction at :381\00"
@.str.248 = private constant [37 x i8] c"integer overflow on addition at :383\00"
@.str.249 = private constant [37 x i8] c"integer overflow on addition at :387\00"
@.str.250 = private constant [37 x i8] c"integer overflow on addition at :390\00"
@.str.251 = private constant [36 x i8] c"integer overflow on addition at :28\00"
@.str.252 = private constant [38 x i8] c"integer overflow on addition at :1041\00"
@.str.253 = private constant [38 x i8] c"integer overflow on addition at :1047\00"
@.str.254 = private constant [38 x i8] c"integer overflow on addition at :1048\00"
@.str.255 = private constant [37 x i8] c"integer overflow on addition at :957\00"
@.str.256 = private constant [37 x i8] c"integer overflow on addition at :962\00"
@.str.257 = private constant [37 x i8] c"integer overflow on addition at :971\00"
@.str.258 = private constant [37 x i8] c"integer overflow on addition at :972\00"
@.str.259 = private constant [37 x i8] c"integer overflow on addition at :979\00"
@.str.260 = private constant [40 x i8] c"integer overflow on subtraction at :981\00"
@.str.261 = private constant [37 x i8] c"integer overflow on addition at :983\00"
@.str.262 = private constant [37 x i8] c"integer overflow on addition at :988\00"
@.str.263 = private constant [37 x i8] c"integer overflow on addition at :993\00"
@.str.264 = private constant [41 x i8] c"integer overflow on subtraction at :1007\00"
@.str.265 = private constant [38 x i8] c"integer overflow on addition at :1011\00"
@.str.266 = private constant [38 x i8] c"integer overflow on addition at :1013\00"
@.str.267 = private constant [38 x i8] c"integer overflow on addition at :1014\00"
@.str.268 = private constant [40 x i8] c"integer overflow on subtraction at :194\00"
@.str.269 = private constant [37 x i8] c"integer overflow on addition at :202\00"
@.str.270 = private constant [37 x i8] c"integer overflow on addition at :210\00"
@.str.271 = private constant [37 x i8] c"integer overflow on addition at :212\00"
@.str.272 = private constant [37 x i8] c"integer overflow on addition at :219\00"
@.str.273 = private constant [37 x i8] c"integer overflow on addition at :224\00"
@.str.274 = private constant [37 x i8] c"integer overflow on addition at :227\00"
@.str.275 = private constant [40 x i8] c"integer overflow on subtraction at :244\00"
@.str.276 = private constant [37 x i8] c"integer overflow on addition at :247\00"
@.str.277 = private constant [37 x i8] c"integer overflow on addition at :250\00"
@.str.278 = private constant [42 x i8] c"integer overflow on multiplication at :65\00"
@.str.279 = private constant [36 x i8] c"integer overflow on addition at :81\00"
@.str.280 = private constant [37 x i8] c"integer overflow on addition at :191\00"
@.str.281 = private constant [37 x i8] c"integer overflow on addition at :160\00"
@.str.282 = private constant [43 x i8] c"integer overflow on multiplication at :168\00"
@.str.283 = private constant [37 x i8] c"integer overflow on addition at :168\00"
@.str.284 = private constant [36 x i8] c"integer overflow on addition at :97\00"
@.str.285 = private constant [36 x i8] c"integer overflow on addition at :98\00"
@.str.286 = private constant [36 x i8] c"integer overflow on addition at :99\00"
@.str.287 = private constant [43 x i8] c"integer overflow on multiplication at :106\00"
@.str.288 = private constant [43 x i8] c"integer overflow on multiplication at :107\00"
@.str.289 = private constant [43 x i8] c"integer overflow on multiplication at :127\00"

; Entry point
define dso_local i32 @main(i32 %argc, ptr %argv) noinline {
entry:
  %ret = call i32 @tml_main()
  ret i32 %ret
}

; Route registration from @Get/@Post/@Put/@Delete/@Patch/@Head/@Options decorators

define void @__tml_register_routes(i64 %table, ptr %count_ptr, i64 %trees) {
entry:
  ret void
}

; Function attributes for optimization
attributes #0 = { nounwind "target-features"="+sse2,+sse4.2,+avx,+avx2,+fma" }

; Loop optimization metadata
!1000 = distinct !{!1000}
!1001 = distinct !{!1001}
!1002 = distinct !{!1002}
!1003 = distinct !{!1003}
!1004 = distinct !{!1004}
!1005 = distinct !{!1005}
!1006 = distinct !{!1006}
!1007 = distinct !{!1007}
!1008 = distinct !{!1008}
!1009 = distinct !{!1009}
!1010 = distinct !{!1010}
!1011 = distinct !{!1011}
!1012 = distinct !{!1012}
!1013 = distinct !{!1013}
!1014 = distinct !{!1014}
!1015 = distinct !{!1015}
!1016 = distinct !{!1016}
!1017 = distinct !{!1017}
!1018 = distinct !{!1018}
!1019 = distinct !{!1019}
!1020 = distinct !{!1020}
!1021 = distinct !{!1021}
!1022 = distinct !{!1022}
!1023 = distinct !{!1023}
!1024 = distinct !{!1024}
!1025 = distinct !{!1025}
!1026 = distinct !{!1026}
!1027 = distinct !{!1027}
!1028 = distinct !{!1028}
!1029 = distinct !{!1029}
!1030 = distinct !{!1030}
!1031 = distinct !{!1031}
!1032 = distinct !{!1032}
!1033 = distinct !{!1033}
!1034 = distinct !{!1034}
!1035 = distinct !{!1035}
!1036 = distinct !{!1036}
!1037 = distinct !{!1037}
!1038 = distinct !{!1038}
!1039 = distinct !{!1039}
!1040 = distinct !{!1040}
!1041 = distinct !{!1041}
!1042 = distinct !{!1042}
!1043 = distinct !{!1043}
!1044 = distinct !{!1044}
!1045 = distinct !{!1045}
!1046 = distinct !{!1046}
!1047 = distinct !{!1047}
!1048 = distinct !{!1048}
!1049 = distinct !{!1049}
!1050 = distinct !{!1050}
!1051 = distinct !{!1051}
!1052 = distinct !{!1052}
!1053 = distinct !{!1053}
!1054 = distinct !{!1054}
!1055 = distinct !{!1055}
!1056 = distinct !{!1056}
!1057 = distinct !{!1057}
!1058 = distinct !{!1058}

!llvm.ident = !{!0}
!0 = !{!"tml version 0.2.14"}
