# TML v1.0 — Compiler Intrinsics

## 1. Overview

Compiler intrinsics are built-in functions that map directly to hardware instructions or compiler primitives. They are essential for:
- Performance-critical operations
- Hardware access
- Compiler implementation
- Low-level memory operations

## 2. Arithmetic Intrinsics

### 2.1 Overflow-Checked Arithmetic

```tml
module intrinsics

/// Add with overflow check
public func add_with_overflow[T: Integer](a: T, b: T) -> (T, Bool)

/// Subtract with overflow check
public func sub_with_overflow[T: Integer](a: T, b: T) -> (T, Bool)

/// Multiply with overflow check
public func mul_with_overflow[T: Integer](a: T, b: T) -> (T, Bool)

// Usage
let (result, overflow) = add_with_overflow(I32.MAX, 1)
if overflow {
    panic("overflow!")
}
```

### 2.2 Saturating Arithmetic

```tml
/// Saturating add (clamps to MAX/MIN)
public func saturating_add[T: Integer](a: T, b: T) -> T

/// Saturating subtract
public func saturating_sub[T: Integer](a: T, b: T) -> T

/// Saturating multiply
public func saturating_mul[T: Integer](a: T, b: T) -> T

// Usage
let x: U8 = saturating_add(250_u8, 10_u8)  // Returns 255
```

### 2.3 Wrapping Arithmetic

```tml
/// Wrapping add (two's complement wrap)
public func wrapping_add[T: Integer](a: T, b: T) -> T

/// Wrapping subtract
public func wrapping_sub[T: Integer](a: T, b: T) -> T

/// Wrapping multiply
public func wrapping_mul[T: Integer](a: T, b: T) -> T

/// Wrapping negate
public func wrapping_neg[T: SignedInteger](a: T) -> T

/// Wrapping absolute value
public func wrapping_abs[T: SignedInteger](a: T) -> T

// Usage
let x: U8 = wrapping_add(255_u8, 1_u8)  // Returns 0
```

### 2.4 Unchecked Arithmetic

```tml
/// Unchecked add (undefined behavior on overflow)
public lowlevel func unchecked_add[T: Integer](a: T, b: T) -> T

/// Unchecked subtract
public lowlevel func unchecked_sub[T: Integer](a: T, b: T) -> T

/// Unchecked multiply
public lowlevel func unchecked_mul[T: Integer](a: T, b: T) -> T

/// Unchecked divide (UB if divisor is zero)
public lowlevel func unchecked_div[T: Integer](a: T, b: T) -> T

/// Unchecked remainder
public lowlevel func unchecked_rem[T: Integer](a: T, b: T) -> T

/// Unchecked shift left
public lowlevel func unchecked_shl[T: Integer](a: T, b: U32) -> T

/// Unchecked shift right
public lowlevel func unchecked_shr[T: Integer](a: T, b: U32) -> T
```

### 2.5 Extended Arithmetic

```tml
/// Widening multiply (returns double-width result)
public func widening_mul_u64(a: U64, b: U64) -> U128
public func widening_mul_i64(a: I64, b: I64) -> I128

/// High part of multiply
public func mulhi_u64(a: U64, b: U64) -> U64
public func mulhi_i64(a: I64, b: I64) -> I64

/// Fused multiply-add
public func fma_f32(a: F32, b: F32, c: F32) -> F32  // a * b + c
public func fma_f64(a: F64, b: F64, c: F64) -> F64

/// Division with remainder
public func divmod[T: Integer](a: T, b: T) -> (T, T)  // (quotient, remainder)
```

## 3. Bit Manipulation Intrinsics

### 3.1 Counting

```tml
/// Count leading zeros
public func ctlz[T: Integer](x: T) -> U32

/// Count trailing zeros
public func cttz[T: Integer](x: T) -> U32

/// Count ones (population count)
public func ctpop[T: Integer](x: T) -> U32

/// Check if power of two
public func is_power_of_two[T: UnsignedInteger](x: T) -> Bool
```

### 3.2 Bit Reversal and Swap

```tml
/// Reverse bits
public func bitreverse[T: Integer](x: T) -> T

/// Byte swap
public func bswap[T: Integer](x: T) -> T

/// Rotate left
public func rotate_left[T: Integer](x: T, n: U32) -> T

/// Rotate right
public func rotate_right[T: Integer](x: T, n: U32) -> T
```

### 3.3 Bit Extraction and Insertion

```tml
/// Extract bit field (x86 BEXTR)
public func bextr_u32(x: U32, start: U32, len: U32) -> U32
public func bextr_u64(x: U64, start: U32, len: U32) -> U64

/// Parallel bit deposit (x86 PDEP)
public func pdep_u32(x: U32, mask: U32) -> U32
public func pdep_u64(x: U64, mask: U64) -> U64

/// Parallel bit extract (x86 PEXT)
public func pext_u32(x: U32, mask: U32) -> U32
public func pext_u64(x: U64, mask: U64) -> U64

/// Bit zero high (BZHI)
public func bzhi_u32(x: U32, n: U32) -> U32
public func bzhi_u64(x: U64, n: U32) -> U64
```

## 4. Memory Intrinsics

### 4.1 Copy and Set

```tml
/// Copy memory (non-overlapping, like memcpy)
public lowlevel func copy_nonoverlapping[T](
    src: *const T,
    dst: *mut T,
    count: U64
)

/// Copy memory (may overlap, like memmove)
public lowlevel func copy[T](
    src: *const T,
    dst: *mut T,
    count: U64
)

/// Set memory to value (like memset)
public lowlevel func write_bytes[T](
    dst: *mut T,
    value: U8,
    count: U64
)

/// Compare memory (like memcmp)
public lowlevel func compare_bytes(
    a: *const U8,
    b: *const U8,
    count: U64
) -> I32
```

### 4.2 Prefetch

```tml
/// Prefetch for read
public func prefetch_read[T](ptr: *const T, locality: PrefetchLocality)

/// Prefetch for write
public func prefetch_write[T](ptr: *mut T, locality: PrefetchLocality)

public type PrefetchLocality =
    | None       // No temporal locality (L1 only)
    | Low        // Low temporal locality (L2)
    | Medium     // Medium temporal locality (L2+L3)
    | High       // High temporal locality (all levels)
```

### 4.3 Cache Control

```tml
/// Flush cache line
public lowlevel func cache_flush(ptr: *const Void)

/// Memory barrier (full fence)
public func memory_barrier()

/// Read barrier
public func read_barrier()

/// Write barrier
public func write_barrier()
```

### 4.4 Volatile Access

```tml
/// Volatile read (never optimized away)
public lowlevel func volatile_read[T](ptr: *const T) -> T

/// Volatile write
public lowlevel func volatile_write[T](ptr: *mut T, value: T)

/// Volatile set bytes
public lowlevel func volatile_set_bytes[T](dst: *mut T, value: U8, count: U64)

/// Volatile copy
public lowlevel func volatile_copy[T](src: *const T, dst: *mut T, count: U64)
```

## 5. Atomic Intrinsics

### 5.1 Atomic Load/Store

```tml
/// Atomic load
public func atomic_load[T](ptr: *const T, order: MemoryOrdering) -> T
    where T: AtomicType

/// Atomic store
public func atomic_store[T](ptr: *mut T, value: T, order: MemoryOrdering)
    where T: AtomicType
```

### 5.2 Atomic Read-Modify-Write

```tml
/// Atomic exchange
public func atomic_xchg[T](ptr: *mut T, value: T, order: MemoryOrdering) -> T

/// Atomic compare-exchange (strong)
public func atomic_cxchg[T](
    ptr: *mut T,
    old: T,
    new: T,
    success_order: MemoryOrdering,
    failure_order: MemoryOrdering
) -> (T, Bool)

/// Atomic compare-exchange (weak)
public func atomic_cxchg_weak[T](
    ptr: *mut T,
    old: T,
    new: T,
    success_order: MemoryOrdering,
    failure_order: MemoryOrdering
) -> (T, Bool)

/// Atomic add
public func atomic_add[T: Integer](ptr: *mut T, value: T, order: MemoryOrdering) -> T

/// Atomic subtract
public func atomic_sub[T: Integer](ptr: *mut T, value: T, order: MemoryOrdering) -> T

/// Atomic AND
public func atomic_and[T: Integer](ptr: *mut T, value: T, order: MemoryOrdering) -> T

/// Atomic OR
public func atomic_or[T: Integer](ptr: *mut T, value: T, order: MemoryOrdering) -> T

/// Atomic XOR
public func atomic_xor[T: Integer](ptr: *mut T, value: T, order: MemoryOrdering) -> T

/// Atomic NAND
public func atomic_nand[T: Integer](ptr: *mut T, value: T, order: MemoryOrdering) -> T

/// Atomic max
public func atomic_max[T: Integer](ptr: *mut T, value: T, order: MemoryOrdering) -> T

/// Atomic min
public func atomic_min[T: Integer](ptr: *mut T, value: T, order: MemoryOrdering) -> T

/// Atomic unsigned max
public func atomic_umax[T: UnsignedInteger](ptr: *mut T, value: T, order: MemoryOrdering) -> T

/// Atomic unsigned min
public func atomic_umin[T: UnsignedInteger](ptr: *mut T, value: T, order: MemoryOrdering) -> T
```

### 5.3 Fence

```tml
/// Memory fence
public func atomic_fence(order: MemoryOrdering)

/// Single-thread fence (compiler barrier)
public func atomic_singlethread_fence(order: MemoryOrdering)
```

## 6. Float Intrinsics

### 6.1 Basic Math

```tml
/// Square root
public func sqrt_f32(x: F32) -> F32
public func sqrt_f64(x: F64) -> F64

/// Sine
public func sin_f32(x: F32) -> F32
public func sin_f64(x: F64) -> F64

/// Cosine
public func cos_f32(x: F32) -> F32
public func cos_f64(x: F64) -> F64

/// Tangent
public func tan_f32(x: F32) -> F32
public func tan_f64(x: F64) -> F64

/// Exponential (e^x)
public func exp_f32(x: F32) -> F32
public func exp_f64(x: F64) -> F64

/// Exponential (2^x)
public func exp2_f32(x: F32) -> F32
public func exp2_f64(x: F64) -> F64

/// Natural logarithm
public func ln_f32(x: F32) -> F32
public func ln_f64(x: F64) -> F64

/// Base-2 logarithm
public func log2_f32(x: F32) -> F32
public func log2_f64(x: F64) -> F64

/// Base-10 logarithm
public func log10_f32(x: F32) -> F32
public func log10_f64(x: F64) -> F64

/// Power
public func pow_f32(base: F32, exp: F32) -> F32
public func pow_f64(base: F64, exp: F64) -> F64
```

### 6.2 Rounding

```tml
/// Floor
public func floor_f32(x: F32) -> F32
public func floor_f64(x: F64) -> F64

/// Ceiling
public func ceil_f32(x: F32) -> F32
public func ceil_f64(x: F64) -> F64

/// Round to nearest
public func round_f32(x: F32) -> F32
public func round_f64(x: F64) -> F64

/// Truncate toward zero
public func trunc_f32(x: F32) -> F32
public func trunc_f64(x: F64) -> F64

/// Round to nearest even
public func rint_f32(x: F32) -> F32
public func rint_f64(x: F64) -> F64

/// Round to integer
public func nearbyint_f32(x: F32) -> F32
public func nearbyint_f64(x: F64) -> F64
```

### 6.3 Special Operations

```tml
/// Absolute value
public func fabs_f32(x: F32) -> F32
public func fabs_f64(x: F64) -> F64

/// Copy sign
public func copysign_f32(magnitude: F32, sign: F32) -> F32
public func copysign_f64(magnitude: F64, sign: F64) -> F64

/// Minimum
public func fmin_f32(a: F32, b: F32) -> F32
public func fmin_f64(a: F64, b: F64) -> F64

/// Maximum
public func fmax_f32(a: F32, b: F32) -> F32
public func fmax_f64(a: F64, b: F64) -> F64

/// Fused multiply-add
public func fma_f32(a: F32, b: F32, c: F32) -> F32
public func fma_f64(a: F64, b: F64, c: F64) -> F64
```

### 6.4 Conversion

```tml
/// Float to int (truncate toward zero)
public func fptosi_f32_i32(x: F32) -> I32
public func fptosi_f64_i64(x: F64) -> I64
public func fptoui_f32_u32(x: F32) -> U32
public func fptoui_f64_u64(x: F64) -> U64

/// Int to float
public func sitofp_i32_f32(x: I32) -> F32
public func sitofp_i64_f64(x: I64) -> F64
public func uitofp_u32_f32(x: U32) -> F32
public func uitofp_u64_f64(x: U64) -> F64

/// Float precision conversion
public func fptrunc_f64_f32(x: F64) -> F32
public func fpext_f32_f64(x: F32) -> F64
```

## 7. Type Intrinsics

### 7.1 Size and Alignment

```tml
/// Size of type in bytes
public func size_of[T]() -> U64

/// Alignment of type in bytes
public func align_of[T]() -> U64

/// Minimum alignment
public func min_align_of[T]() -> U64

/// Preferred alignment
public func pref_align_of[T]() -> U64
```

### 7.2 Type Properties

```tml
/// Type name as string
public func type_name[T]() -> ref static str

/// Type ID (unique per type)
public func type_id[T]() -> TypeId

/// Check if type needs drop
public func needs_drop[T]() -> Bool

/// Check if type is copy
public func is_copy[T]() -> Bool

/// Check if type is zero-sized
public func is_zst[T]() -> Bool
```

### 7.3 Transmutation

```tml
/// Reinterpret bits as different type
public lowlevel func transmute[T, U](value: T) -> U
    where size_of[T]() == size_of[U]()

/// Transmute with unchecked alignment
public lowlevel func transmute_unchecked[T, U](value: T) -> U
```

## 8. Control Flow Intrinsics

### 8.1 Hints

```tml
/// Unreachable code (undefined behavior if reached)
public lowlevel func unreachable() -> !

/// Assume condition is true (optimizer hint)
public lowlevel func assume(cond: Bool)

/// Likely branch hint
public func likely(cond: Bool) -> Bool

/// Unlikely branch hint
public func unlikely(cond: Bool) -> Bool

/// Black box (prevent optimization)
public func black_box[T](x: T) -> T
```

### 8.2 Debug

```tml
/// Breakpoint
public func breakpoint()

/// Debug trap
public func debug_trap()

/// Abort
public func abort() -> !
```

## 9. Platform-Specific Intrinsics

### 9.1 x86/x86_64

```tml
@when(target_arch = "x86_64")
module intrinsics.x86

/// CPUID
public func cpuid(leaf: U32, sub_leaf: U32) -> (U32, U32, U32, U32)

/// Read timestamp counter
public func rdtsc() -> U64

/// Read timestamp counter and processor ID
public func rdtscp() -> (U64, U32)

/// Pause (spin-loop hint)
public func pause()

/// CRC32
public func crc32_u8(crc: U32, data: U8) -> U32
public func crc32_u16(crc: U32, data: U16) -> U32
public func crc32_u32(crc: U32, data: U32) -> U32
public func crc32_u64(crc: U64, data: U64) -> U64

/// POPCNT
public func popcnt_u32(x: U32) -> U32
public func popcnt_u64(x: U64) -> U64

/// LZCNT
public func lzcnt_u32(x: U32) -> U32
public func lzcnt_u64(x: U64) -> U64

/// TZCNT
public func tzcnt_u32(x: U32) -> U32
public func tzcnt_u64(x: U64) -> U64

/// AES
@when(target_feature = "aes")
public func aesenc(data: U8x16, key: U8x16) -> U8x16
public func aesdec(data: U8x16, key: U8x16) -> U8x16
public func aeskeygenassist(key: U8x16, imm: U8) -> U8x16

/// RDRAND (hardware random)
public func rdrand_u16() -> Maybe[U16]
public func rdrand_u32() -> Maybe[U32]
public func rdrand_u64() -> Maybe[U64]

/// RDSEED
public func rdseed_u16() -> Maybe[U16]
public func rdseed_u32() -> Maybe[U32]
public func rdseed_u64() -> Maybe[U64]
```

### 9.2 ARM/AArch64

```tml
@when(target_arch = "aarch64")
module intrinsics.aarch64

/// DMB (Data Memory Barrier)
public func dmb()

/// DSB (Data Synchronization Barrier)
public func dsb()

/// ISB (Instruction Synchronization Barrier)
public func isb()

/// WFE (Wait For Event)
public func wfe()

/// WFI (Wait For Interrupt)
public func wfi()

/// SEV (Send Event)
public func sev()

/// YIELD
public func yield_cpu()

/// Read system register
public lowlevel func read_sysreg(reg: ref str) -> U64

/// Write system register
public lowlevel func write_sysreg(reg: ref str, value: U64)

/// CRC32
public func crc32b(crc: U32, data: U8) -> U32
public func crc32h(crc: U32, data: U16) -> U32
public func crc32w(crc: U32, data: U32) -> U32
public func crc32x(crc: U32, data: U64) -> U32

/// AES
@when(target_feature = "aes")
public func aese(data: U8x16, key: U8x16) -> U8x16
public func aesd(data: U8x16, key: U8x16) -> U8x16
```

## 10. Diagnostic Intrinsics

### 10.1 Static Assertions

```tml
/// Compile-time assertion
public func static_assert(cond: Bool, msg: ref str)

/// Compile-time assert equal
public func static_assert_eq[T: Eq](a: T, b: T, msg: ref str)

/// Compile-time type size check
public func static_assert_size[T](expected: U64)
```

### 10.2 Compile Information

```tml
/// Current file
public func file() -> ref static str

/// Current line
public func line() -> U32

/// Current column
public func column() -> U32

/// Current function name
public func function() -> ref static str

/// Current module path
public func module_path() -> ref static str
```

## 11. Usage Examples

### 11.1 Fast Population Count

```tml
func count_bits(x: U64) -> U32 {
    @when(target_feature = "popcnt")
    {
        return intrinsics.x86.popcnt_u64(x) as U32
    }

    @when(not(target_feature = "popcnt"))
    {
        // Software fallback
        var n: U32 = x
        n = n - ((n >> 1) & 0x5555555555555555)
        n = (n & 0x3333333333333333) + ((n >> 2) & 0x3333333333333333)
        n = (n + (n >> 4)) & 0x0F0F0F0F0F0F0F0F
        n = n * 0x0101010101010101
        return (n >> 56) as U32
    }
}
```

### 11.2 Branch-Free Min/Max

```tml
/// Branch-free integer minimum
func branchless_min(a: I32, b: I32) -> I32 {
    let diff: I32 = a - b
    let mask: I32 = diff >> 31  // All 1s if a < b, all 0s otherwise
    return b + (diff & mask)
}

/// Branch-free integer maximum
func branchless_max(a: I32, b: I32) -> I32 {
    let diff: I32 = a - b
    let mask: I32 = diff >> 31
    return a - (diff & mask)
}
```

### 11.3 Fast CRC32

```tml
@when(target_feature = "sse4.2")
func crc32_fast(data: ref [U8]) -> U32 {
    var crc: U32 = 0xFFFFFFFF
    var i: U64 = 0

    // Process 8 bytes at a time
    loop while i + 8 <= data.len() {
        lowlevel {
            let chunk: U64 = (data.as_ptr().add(i) as *const U64).read_unaligned()
            crc = intrinsics.x86.crc32_u64(crc as U64, chunk) as U32
        }
        i += 8
    }

    // Process remaining bytes
    loop while i < data.len() {
        crc = intrinsics.x86.crc32_u8(crc, data[i])
        i += 1
    }

    return crc ^ 0xFFFFFFFF
}
```

---

*Previous: [22-LOW-LEVEL.md](./22-LOW-LEVEL.md)*
*Next: [24-SYSCALL.md](./24-SYSCALL.md) — System Call Interface*
