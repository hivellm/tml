// TML Compiler - Codegen Builtins Tests
// Comprehensive tests for all builtin functions in codegen

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

using namespace tml;
using namespace tml::codegen;
using namespace tml::types;
using namespace tml::parser;
using namespace tml::lexer;

class CodegenBuiltinsTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;

    auto generate(const std::string& code) -> std::string {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto module_result = parser.parse_module("test");
        EXPECT_TRUE(is_ok(module_result));
        auto& module = std::get<parser::Module>(module_result);

        TypeChecker checker;
        auto env_result = checker.check_module(module);
        EXPECT_TRUE(is_ok(env_result));
        auto& env = std::get<TypeEnv>(env_result);

        LLVMIRGen gen(env);
        auto ir_result = gen.generate(module);

        if (is_err(ir_result)) {
            auto& errors = std::get<std::vector<LLVMGenError>>(ir_result);
            for (const auto& err : errors) {
                ADD_FAILURE() << "Codegen error: " << err.message;
            }
            return "";
        }

        return std::get<std::string>(ir_result);
    }

    void expect_ir_contains(const std::string& ir, const std::string& pattern,
                            const std::string& msg) {
        EXPECT_NE(ir.find(pattern), std::string::npos) << msg;
    }
};

// ============================================================================
// Math Builtin Tests
// ============================================================================

TEST_F(CodegenBuiltinsTest, MathSqrt) {
    std::string ir = generate(R"(
        func main() {
            let x: I32 = 16
            let result: I32 = sqrt(x)
        }
    )");
    expect_ir_contains(ir, "@float_sqrt", "IR should call float_sqrt");
}

TEST_F(CodegenBuiltinsTest, MathPow) {
    std::string ir = generate(R"(
        func main() {
            let base: I32 = 2
            let exp: I32 = 3
            let result: I32 = pow(base, exp)
        }
    )");
    expect_ir_contains(ir, "@float_pow", "IR should call float_pow");
}

TEST_F(CodegenBuiltinsTest, MathAbs) {
    std::string ir = generate(R"(
        func main() {
            let x: I32 = -5
            let result: I32 = abs(x)
        }
    )");
    expect_ir_contains(ir, "@float_abs", "IR should call float_abs");
}

TEST_F(CodegenBuiltinsTest, MathFloor) {
    std::string ir = generate(R"(
        func main() {
            let x: I32 = 7
            let result: I32 = floor(x)
        }
    )");
    expect_ir_contains(ir, "@float_floor", "IR should call float_floor");
}

TEST_F(CodegenBuiltinsTest, MathCeil) {
    std::string ir = generate(R"(
        func main() {
            let x: I32 = 7
            let result: I32 = ceil(x)
        }
    )");
    expect_ir_contains(ir, "@float_ceil", "IR should call float_ceil");
}

TEST_F(CodegenBuiltinsTest, MathRound) {
    std::string ir = generate(R"(
        func main() {
            let x: I32 = 7
            let result: I32 = round(x)
        }
    )");
    expect_ir_contains(ir, "@float_round", "IR should call float_round");
}

TEST_F(CodegenBuiltinsTest, BlackBox) {
    std::string ir = generate(R"(
        func main() {
            let x: I32 = 42
            let result: I32 = black_box(x)
        }
    )");
    expect_ir_contains(ir, "@black_box_i32", "IR should call black_box_i32");
}

// ============================================================================
// Time Builtin Tests
// ============================================================================

TEST_F(CodegenBuiltinsTest, TimeMs) {
    std::string ir = generate(R"(
        func main() {
            let start: I32 = time_ms()
        }
    )");
    expect_ir_contains(ir, "@time_ms", "IR should call time_ms");
}

TEST_F(CodegenBuiltinsTest, TimeUs) {
    std::string ir = generate(R"(
        func main() {
            let start: I64 = time_us()
        }
    )");
    expect_ir_contains(ir, "@time_us", "IR should call time_us");
}

TEST_F(CodegenBuiltinsTest, TimeNs) {
    std::string ir = generate(R"(
        func main() {
            let start: I64 = time_ns()
        }
    )");
    expect_ir_contains(ir, "@time_ns", "IR should call time_ns");
}

TEST_F(CodegenBuiltinsTest, ElapsedMs) {
    std::string ir = generate(R"(
        func main() {
            let start: I32 = time_ms()
            let elapsed: I32 = elapsed_ms(start)
        }
    )");
    expect_ir_contains(ir, "@elapsed_ms", "IR should call elapsed_ms");
}

TEST_F(CodegenBuiltinsTest, SleepMs) {
    std::string ir = generate(R"(
        func main() {
            sleep_ms(10)
        }
    )");
    expect_ir_contains(ir, "@sleep_ms", "IR should call sleep_ms");
}

// ============================================================================
// Memory Builtin Tests
// ============================================================================

TEST_F(CodegenBuiltinsTest, MemAlloc) {
    std::string ir = generate(R"(
        func main() {
            let ptr: *Unit = alloc(100)
        }
    )");
    expect_ir_contains(ir, "@malloc", "IR should call malloc");
}

TEST_F(CodegenBuiltinsTest, MemDealloc) {
    std::string ir = generate(R"(
        func main() {
            let ptr: *Unit = alloc(100)
            dealloc(ptr)
        }
    )");
    expect_ir_contains(ir, "@free", "IR should call free");
}

TEST_F(CodegenBuiltinsTest, MemCopy) {
    std::string ir = generate(R"(
        func main() {
            let src: *Unit = alloc(100)
            let dest: *Unit = alloc(100)
            mem_copy(dest, src, 100)
        }
    )");
    expect_ir_contains(ir, "@mem_copy", "IR should call mem_copy");
}

TEST_F(CodegenBuiltinsTest, MemSet) {
    std::string ir = generate(R"(
        func main() {
            let ptr: *Unit = alloc(100)
            mem_set(ptr, 0, 100)
        }
    )");
    expect_ir_contains(ir, "@mem_set", "IR should call mem_set");
}

TEST_F(CodegenBuiltinsTest, MemZero) {
    std::string ir = generate(R"(
        func main() {
            let ptr: *Unit = alloc(100)
            mem_zero(ptr, 100)
        }
    )");
    expect_ir_contains(ir, "@mem_zero", "IR should call mem_zero");
}

TEST_F(CodegenBuiltinsTest, MemCompare) {
    std::string ir = generate(R"(
        func main() {
            let a: *Unit = alloc(10)
            let b: *Unit = alloc(10)
            let cmp: I32 = mem_compare(a, b, 10)
        }
    )");
    expect_ir_contains(ir, "@mem_compare", "IR should call mem_compare");
}

TEST_F(CodegenBuiltinsTest, MemEq) {
    std::string ir = generate(R"(
        func main() {
            let a: *Unit = alloc(10)
            let b: *Unit = alloc(10)
            let size: I64 = 10
            let eq: Bool = mem_eq(a, b, size)
        }
    )");
    expect_ir_contains(ir, "@mem_eq", "IR should call mem_eq");
}

// ============================================================================
// Atomic Builtin Tests
// ============================================================================

TEST_F(CodegenBuiltinsTest, AtomicLoad) {
    std::string ir = generate(R"(
        func main() {
            let ptr: *Unit = alloc(4)
            let val: I32 = atomic_load(ptr)
        }
    )");
    expect_ir_contains(ir, "load atomic i32", "IR should use atomic load");
}

TEST_F(CodegenBuiltinsTest, AtomicStore) {
    std::string ir = generate(R"(
        func main() {
            let ptr: *Unit = alloc(4)
            atomic_store(ptr, 42)
        }
    )");
    expect_ir_contains(ir, "store atomic i32", "IR should use atomic store");
}

TEST_F(CodegenBuiltinsTest, AtomicAdd) {
    std::string ir = generate(R"(
        func main() {
            let ptr: *Unit = alloc(4)
            let old: I32 = atomic_add(ptr, 1)
        }
    )");
    expect_ir_contains(ir, "atomicrmw add", "IR should use atomicrmw add");
}

TEST_F(CodegenBuiltinsTest, AtomicSub) {
    std::string ir = generate(R"(
        func main() {
            let ptr: *Unit = alloc(4)
            let old: I32 = atomic_sub(ptr, 1)
        }
    )");
    expect_ir_contains(ir, "atomicrmw sub", "IR should use atomicrmw sub");
}

TEST_F(CodegenBuiltinsTest, AtomicExchange) {
    std::string ir = generate(R"(
        func main() {
            let ptr: *Unit = alloc(4)
            let old: I32 = atomic_exchange(ptr, 100)
        }
    )");
    expect_ir_contains(ir, "atomicrmw xchg", "IR should use atomicrmw xchg");
}

TEST_F(CodegenBuiltinsTest, AtomicCas) {
    std::string ir = generate(R"(
        func main() {
            let ptr: *Unit = alloc(4)
            let success: Bool = atomic_cas(ptr, 0, 1)
        }
    )");
    expect_ir_contains(ir, "cmpxchg", "IR should use cmpxchg");
}

TEST_F(CodegenBuiltinsTest, AtomicAnd) {
    std::string ir = generate(R"(
        func main() {
            let ptr: *Unit = alloc(4)
            let old: I32 = atomic_and(ptr, 0xFF)
        }
    )");
    expect_ir_contains(ir, "atomicrmw and", "IR should use atomicrmw and");
}

TEST_F(CodegenBuiltinsTest, AtomicOr) {
    std::string ir = generate(R"(
        func main() {
            let ptr: *Unit = alloc(4)
            let old: I32 = atomic_or(ptr, 0xFF)
        }
    )");
    expect_ir_contains(ir, "atomicrmw or", "IR should use atomicrmw or");
}

TEST_F(CodegenBuiltinsTest, Fence) {
    std::string ir = generate(R"(
        func main() {
            fence()
        }
    )");
    expect_ir_contains(ir, "fence seq_cst", "IR should use fence seq_cst");
}

TEST_F(CodegenBuiltinsTest, FenceAcquire) {
    std::string ir = generate(R"(
        func main() {
            fence_acquire()
        }
    )");
    expect_ir_contains(ir, "fence acquire", "IR should use fence acquire");
}

TEST_F(CodegenBuiltinsTest, FenceRelease) {
    std::string ir = generate(R"(
        func main() {
            fence_release()
        }
    )");
    expect_ir_contains(ir, "fence release", "IR should use fence release");
}

// ============================================================================
// Sync Builtin Tests
// ============================================================================

TEST_F(CodegenBuiltinsTest, SpinLock) {
    std::string ir = generate(R"(
        func main() {
            let lock: *Unit = alloc(4)
            spin_lock(lock)
            spin_unlock(lock)
        }
    )");
    expect_ir_contains(ir, "atomicrmw xchg", "IR should use atomicrmw for spinlock");
}

TEST_F(CodegenBuiltinsTest, SpinTryLock) {
    std::string ir = generate(R"(
        func main() {
            let lock: *Unit = alloc(4)
            let acquired: Bool = spin_trylock(lock)
        }
    )");
    expect_ir_contains(ir, "atomicrmw xchg", "IR should use atomicrmw for trylock");
}

TEST_F(CodegenBuiltinsTest, ThreadYield) {
    std::string ir = generate(R"(
        func main() {
            thread_yield()
        }
    )");
    expect_ir_contains(ir, "@thread_yield", "IR should call thread_yield");
}

TEST_F(CodegenBuiltinsTest, ThreadSleep) {
    std::string ir = generate(R"(
        func main() {
            thread_sleep(100)
        }
    )");
    expect_ir_contains(ir, "@thread_sleep", "IR should call thread_sleep");
}

TEST_F(CodegenBuiltinsTest, ThreadId) {
    std::string ir = generate(R"(
        func main() {
            let id: I32 = thread_id()
        }
    )");
    expect_ir_contains(ir, "@thread_id", "IR should call thread_id");
}

TEST_F(CodegenBuiltinsTest, ChannelCreate) {
    std::string ir = generate(R"(
        func main() {
            let ch: *Unit = channel_create()
        }
    )");
    expect_ir_contains(ir, "@channel_create", "IR should call channel_create");
}

TEST_F(CodegenBuiltinsTest, ChannelDestroy) {
    std::string ir = generate(R"(
        func main() {
            let ch: *Unit = channel_create()
            channel_destroy(ch)
        }
    )");
    expect_ir_contains(ir, "@channel_destroy", "IR should call channel_destroy");
}

TEST_F(CodegenBuiltinsTest, ChannelLen) {
    std::string ir = generate(R"(
        func main() {
            let ch: *Unit = channel_create()
            let len: I32 = channel_len(ch)
        }
    )");
    expect_ir_contains(ir, "@channel_len", "IR should call channel_len");
}

TEST_F(CodegenBuiltinsTest, ChannelClose) {
    std::string ir = generate(R"(
        func main() {
            let ch: *Unit = channel_create()
            channel_close(ch)
        }
    )");
    expect_ir_contains(ir, "@channel_close", "IR should call channel_close");
}

TEST_F(CodegenBuiltinsTest, MutexCreate) {
    std::string ir = generate(R"(
        func main() {
            let m: *Unit = mutex_create()
        }
    )");
    expect_ir_contains(ir, "@mutex_create", "IR should call mutex_create");
}

TEST_F(CodegenBuiltinsTest, MutexLockUnlock) {
    std::string ir = generate(R"(
        func main() {
            let m: *Unit = mutex_create()
            mutex_lock(m)
            mutex_unlock(m)
        }
    )");
    expect_ir_contains(ir, "@mutex_lock", "IR should call mutex_lock");
    expect_ir_contains(ir, "@mutex_unlock", "IR should call mutex_unlock");
}

TEST_F(CodegenBuiltinsTest, MutexTryLock) {
    std::string ir = generate(R"(
        func main() {
            let m: *Unit = mutex_create()
            let acquired: Bool = mutex_try_lock(m)
        }
    )");
    expect_ir_contains(ir, "@mutex_try_lock", "IR should call mutex_try_lock");
}

TEST_F(CodegenBuiltinsTest, MutexDestroy) {
    std::string ir = generate(R"(
        func main() {
            let m: *Unit = mutex_create()
            mutex_destroy(m)
        }
    )");
    expect_ir_contains(ir, "@mutex_destroy", "IR should call mutex_destroy");
}

TEST_F(CodegenBuiltinsTest, WaitGroupCreate) {
    std::string ir = generate(R"(
        func main() {
            let wg: *Unit = waitgroup_create()
        }
    )");
    expect_ir_contains(ir, "@waitgroup_create", "IR should call waitgroup_create");
}

TEST_F(CodegenBuiltinsTest, WaitGroupAddDoneWait) {
    std::string ir = generate(R"(
        func main() {
            let wg: *Unit = waitgroup_create()
            waitgroup_add(wg, 1)
            waitgroup_done(wg)
            waitgroup_wait(wg)
        }
    )");
    expect_ir_contains(ir, "@waitgroup_add", "IR should call waitgroup_add");
    expect_ir_contains(ir, "@waitgroup_done", "IR should call waitgroup_done");
    expect_ir_contains(ir, "@waitgroup_wait", "IR should call waitgroup_wait");
}

TEST_F(CodegenBuiltinsTest, WaitGroupDestroy) {
    std::string ir = generate(R"(
        func main() {
            let wg: *Unit = waitgroup_create()
            waitgroup_destroy(wg)
        }
    )");
    expect_ir_contains(ir, "@waitgroup_destroy", "IR should call waitgroup_destroy");
}

// ============================================================================
// String Builtin Tests
// ============================================================================

TEST_F(CodegenBuiltinsTest, StrLen) {
    std::string ir = generate(R"(
        func main() {
            let s: Str = "hello"
            let len: I32 = str_len(s)
        }
    )");
    expect_ir_contains(ir, "@str_len", "IR should call str_len");
}

TEST_F(CodegenBuiltinsTest, StrHash) {
    std::string ir = generate(R"(
        func main() {
            let s: Str = "hello"
            let hash: I32 = str_hash(s)
        }
    )");
    expect_ir_contains(ir, "@str_hash", "IR should call str_hash");
}

TEST_F(CodegenBuiltinsTest, StrEq) {
    std::string ir = generate(R"(
        func main() {
            let a: Str = "hello"
            let b: Str = "world"
            let eq: Bool = str_eq(a, b)
        }
    )");
    expect_ir_contains(ir, "@str_eq", "IR should call str_eq");
}

TEST_F(CodegenBuiltinsTest, StrConcat) {
    std::string ir = generate(R"(
        func main() {
            let a: Str = "hello"
            let b: Str = "world"
            let c: Str = str_concat(a, b)
        }
    )");
    expect_ir_contains(ir, "@str_concat", "IR should call str_concat");
}

TEST_F(CodegenBuiltinsTest, StrSubstring) {
    std::string ir = generate(R"(
        func main() {
            let s: Str = "hello world"
            let sub: Str = str_substring(s, 0, 5)
        }
    )");
    expect_ir_contains(ir, "@str_substring", "IR should call str_substring");
}

TEST_F(CodegenBuiltinsTest, StrContains) {
    std::string ir = generate(R"(
        func main() {
            let s: Str = "hello world"
            let has: Bool = str_contains(s, "world")
        }
    )");
    expect_ir_contains(ir, "@str_contains", "IR should call str_contains");
}

TEST_F(CodegenBuiltinsTest, StrStartsWith) {
    std::string ir = generate(R"(
        func main() {
            let s: Str = "hello world"
            let starts: Bool = str_starts_with(s, "hello")
        }
    )");
    expect_ir_contains(ir, "@str_starts_with", "IR should call str_starts_with");
}

TEST_F(CodegenBuiltinsTest, StrEndsWith) {
    std::string ir = generate(R"(
        func main() {
            let s: Str = "hello world"
            let ends: Bool = str_ends_with(s, "world")
        }
    )");
    expect_ir_contains(ir, "@str_ends_with", "IR should call str_ends_with");
}

TEST_F(CodegenBuiltinsTest, StrToUpper) {
    std::string ir = generate(R"(
        func main() {
            let s: Str = "hello"
            let upper: Str = str_to_upper(s)
        }
    )");
    expect_ir_contains(ir, "@str_to_upper", "IR should call str_to_upper");
}

TEST_F(CodegenBuiltinsTest, StrToLower) {
    std::string ir = generate(R"(
        func main() {
            let s: Str = "HELLO"
            let lower: Str = str_to_lower(s)
        }
    )");
    expect_ir_contains(ir, "@str_to_lower", "IR should call str_to_lower");
}

TEST_F(CodegenBuiltinsTest, StrTrim) {
    std::string ir = generate(R"(
        func main() {
            let s: Str = "  hello  "
            let trimmed: Str = str_trim(s)
        }
    )");
    expect_ir_contains(ir, "@str_trim", "IR should call str_trim");
}

TEST_F(CodegenBuiltinsTest, StrCharAt) {
    std::string ir = generate(R"(
        func main() {
            let s: Str = "hello"
            let c: Char = str_char_at(s, 0)
        }
    )");
    expect_ir_contains(ir, "@str_char_at", "IR should call str_char_at");
}

// ============================================================================
// Collections Builtin Tests - List
// ============================================================================

TEST_F(CodegenBuiltinsTest, ListCreate) {
    std::string ir = generate(R"(
        func main() {
            let list: *Unit = list_create()
        }
    )");
    expect_ir_contains(ir, "@list_create", "IR should call list_create");
}

TEST_F(CodegenBuiltinsTest, ListDestroy) {
    std::string ir = generate(R"(
        func main() {
            let list: *Unit = list_create()
            list_destroy(list)
        }
    )");
    expect_ir_contains(ir, "@list_destroy", "IR should call list_destroy");
}

TEST_F(CodegenBuiltinsTest, ListPush) {
    std::string ir = generate(R"(
        func main() {
            let list: *Unit = list_create()
            list_push(list, 42)
        }
    )");
    expect_ir_contains(ir, "@list_push", "IR should call list_push");
}

TEST_F(CodegenBuiltinsTest, ListPop) {
    std::string ir = generate(R"(
        func main() {
            let list: *Unit = list_create()
            list_push(list, 42)
            let val: I32 = list_pop(list)
        }
    )");
    expect_ir_contains(ir, "@list_pop", "IR should call list_pop");
}

TEST_F(CodegenBuiltinsTest, ListGetSet) {
    std::string ir = generate(R"(
        func main() {
            let list: *Unit = list_create()
            list_push(list, 42)
            let val: I32 = list_get(list, 0)
            list_set(list, 0, 100)
        }
    )");
    expect_ir_contains(ir, "@list_get", "IR should call list_get");
    expect_ir_contains(ir, "@list_set", "IR should call list_set");
}

TEST_F(CodegenBuiltinsTest, ListLen) {
    std::string ir = generate(R"(
        func main() {
            let list: *Unit = list_create()
            let len: I32 = list_len(list)
        }
    )");
    expect_ir_contains(ir, "@list_len", "IR should call list_len");
}

TEST_F(CodegenBuiltinsTest, ListCapacity) {
    std::string ir = generate(R"(
        func main() {
            let list: *Unit = list_create()
            let cap: I32 = list_capacity(list)
        }
    )");
    expect_ir_contains(ir, "@list_capacity", "IR should call list_capacity");
}

TEST_F(CodegenBuiltinsTest, ListClear) {
    std::string ir = generate(R"(
        func main() {
            let list: *Unit = list_create()
            list_clear(list)
        }
    )");
    expect_ir_contains(ir, "@list_clear", "IR should call list_clear");
}

TEST_F(CodegenBuiltinsTest, ListIsEmpty) {
    std::string ir = generate(R"(
        func main() {
            let list: *Unit = list_create()
            let empty: Bool = list_is_empty(list)
        }
    )");
    expect_ir_contains(ir, "@list_is_empty", "IR should call list_is_empty");
}

// ============================================================================
// Collections Builtin Tests - HashMap
// ============================================================================

TEST_F(CodegenBuiltinsTest, HashMapCreate) {
    std::string ir = generate(R"(
        func main() {
            let map: *Unit = hashmap_create()
        }
    )");
    expect_ir_contains(ir, "@hashmap_create", "IR should call hashmap_create");
}

TEST_F(CodegenBuiltinsTest, HashMapDestroy) {
    std::string ir = generate(R"(
        func main() {
            let map: *Unit = hashmap_create()
            hashmap_destroy(map)
        }
    )");
    expect_ir_contains(ir, "@hashmap_destroy", "IR should call hashmap_destroy");
}

TEST_F(CodegenBuiltinsTest, HashMapSetGet) {
    std::string ir = generate(R"(
        func main() {
            let map: *Unit = hashmap_create()
            hashmap_set(map, 1, 100)
            let val: I32 = hashmap_get(map, 1)
        }
    )");
    expect_ir_contains(ir, "@hashmap_set", "IR should call hashmap_set");
    expect_ir_contains(ir, "@hashmap_get", "IR should call hashmap_get");
}

TEST_F(CodegenBuiltinsTest, HashMapHas) {
    std::string ir = generate(R"(
        func main() {
            let map: *Unit = hashmap_create()
            hashmap_set(map, 1, 100)
            let has: Bool = hashmap_has(map, 1)
        }
    )");
    expect_ir_contains(ir, "@hashmap_has", "IR should call hashmap_has");
}

TEST_F(CodegenBuiltinsTest, HashMapRemove) {
    std::string ir = generate(R"(
        func main() {
            let map: *Unit = hashmap_create()
            hashmap_set(map, 1, 100)
            let removed: Bool = hashmap_remove(map, 1)
        }
    )");
    expect_ir_contains(ir, "@hashmap_remove", "IR should call hashmap_remove");
}

TEST_F(CodegenBuiltinsTest, HashMapLen) {
    std::string ir = generate(R"(
        func main() {
            let map: *Unit = hashmap_create()
            let len: I32 = hashmap_len(map)
        }
    )");
    expect_ir_contains(ir, "@hashmap_len", "IR should call hashmap_len");
}

TEST_F(CodegenBuiltinsTest, HashMapClear) {
    std::string ir = generate(R"(
        func main() {
            let map: *Unit = hashmap_create()
            hashmap_clear(map)
        }
    )");
    expect_ir_contains(ir, "@hashmap_clear", "IR should call hashmap_clear");
}

// ============================================================================
// Collections Builtin Tests - Buffer
// ============================================================================

TEST_F(CodegenBuiltinsTest, BufferCreate) {
    std::string ir = generate(R"(
        func main() {
            let buf: *Unit = buffer_create()
        }
    )");
    expect_ir_contains(ir, "@buffer_create", "IR should call buffer_create");
}

TEST_F(CodegenBuiltinsTest, BufferDestroy) {
    std::string ir = generate(R"(
        func main() {
            let buf: *Unit = buffer_create()
            buffer_destroy(buf)
        }
    )");
    expect_ir_contains(ir, "@buffer_destroy", "IR should call buffer_destroy");
}

TEST_F(CodegenBuiltinsTest, BufferWriteReadByte) {
    std::string ir = generate(R"(
        func main() {
            let buf: *Unit = buffer_create()
            buffer_write_byte(buf, 65)
            let b: I32 = buffer_read_byte(buf)
        }
    )");
    expect_ir_contains(ir, "@buffer_write_byte", "IR should call buffer_write_byte");
    expect_ir_contains(ir, "@buffer_read_byte", "IR should call buffer_read_byte");
}

TEST_F(CodegenBuiltinsTest, BufferWriteReadI32) {
    std::string ir = generate(R"(
        func main() {
            let buf: *Unit = buffer_create()
            buffer_write_i32(buf, 12345)
            let val: I32 = buffer_read_i32(buf)
        }
    )");
    expect_ir_contains(ir, "@buffer_write_i32", "IR should call buffer_write_i32");
    expect_ir_contains(ir, "@buffer_read_i32", "IR should call buffer_read_i32");
}

TEST_F(CodegenBuiltinsTest, BufferLen) {
    std::string ir = generate(R"(
        func main() {
            let buf: *Unit = buffer_create()
            let len: I32 = buffer_len(buf)
        }
    )");
    expect_ir_contains(ir, "@buffer_len", "IR should call buffer_len");
}

TEST_F(CodegenBuiltinsTest, BufferCapacity) {
    std::string ir = generate(R"(
        func main() {
            let buf: *Unit = buffer_create()
            let cap: I32 = buffer_capacity(buf)
        }
    )");
    expect_ir_contains(ir, "@buffer_capacity", "IR should call buffer_capacity");
}

TEST_F(CodegenBuiltinsTest, BufferRemaining) {
    std::string ir = generate(R"(
        func main() {
            let buf: *Unit = buffer_create()
            let rem: I32 = buffer_remaining(buf)
        }
    )");
    expect_ir_contains(ir, "@buffer_remaining", "IR should call buffer_remaining");
}

TEST_F(CodegenBuiltinsTest, BufferClear) {
    std::string ir = generate(R"(
        func main() {
            let buf: *Unit = buffer_create()
            buffer_clear(buf)
        }
    )");
    expect_ir_contains(ir, "@buffer_clear", "IR should call buffer_clear");
}

TEST_F(CodegenBuiltinsTest, BufferResetRead) {
    std::string ir = generate(R"(
        func main() {
            let buf: *Unit = buffer_create()
            buffer_reset_read(buf)
        }
    )");
    expect_ir_contains(ir, "@buffer_reset_read", "IR should call buffer_reset_read");
}

// ============================================================================
// IO Builtin Tests
// ============================================================================

TEST_F(CodegenBuiltinsTest, Print) {
    std::string ir = generate(R"(
        func main() {
            print("hello")
        }
    )");
    expect_ir_contains(ir, "@printf", "IR should call printf");
}

TEST_F(CodegenBuiltinsTest, Println) {
    std::string ir = generate(R"(
        func main() {
            println("hello")
        }
    )");
    expect_ir_contains(ir, "@puts", "IR should call puts");
}

TEST_F(CodegenBuiltinsTest, PrintI32) {
    std::string ir = generate(R"(
        func main() {
            let x: I32 = 42
            println(x)
        }
    )");
    expect_ir_contains(ir, "@printf", "IR should call printf for integer");
}

// ============================================================================
// Assert Builtin Tests
// ============================================================================

TEST_F(CodegenBuiltinsTest, AssertEqI32) {
    std::string ir = generate(R"(
        func main() {
            assert_eq(1, 1)
        }
    )");
    expect_ir_contains(ir, "icmp eq i32", "IR should compare i32 values");
    expect_ir_contains(ir, "@panic", "IR should call panic on failure");
}

TEST_F(CodegenBuiltinsTest, AssertEqI64) {
    std::string ir = generate(R"(
        func main() {
            let a: I64 = 100
            let b: I64 = 100
            assert_eq(a, b)
        }
    )");
    expect_ir_contains(ir, "icmp eq i64", "IR should compare i64 values");
}

TEST_F(CodegenBuiltinsTest, AssertEqBool) {
    std::string ir = generate(R"(
        func main() {
            assert_eq(true, true)
        }
    )");
    expect_ir_contains(ir, "icmp eq i1", "IR should compare bool values");
}

TEST_F(CodegenBuiltinsTest, AssertEqStr) {
    std::string ir = generate(R"(
        func main() {
            assert_eq("hello", "hello")
        }
    )");
    expect_ir_contains(ir, "@str_eq", "IR should call str_eq for string comparison");
}

// ============================================================================
// Logical Operator Tests (&&, ||, !)
// ============================================================================

TEST_F(CodegenBuiltinsTest, LogicalAndOperator) {
    std::string ir = generate(R"(
        func main() {
            let a: Bool = true
            let b: Bool = false
            let c: Bool = a && b
        }
    )");
    // && uses LLVM and instruction on i1
    expect_ir_contains(ir, "and i1", "IR should use 'and i1' for &&");
}

TEST_F(CodegenBuiltinsTest, LogicalOrOperator) {
    std::string ir = generate(R"(
        func main() {
            let a: Bool = true
            let b: Bool = false
            let c: Bool = a || b
        }
    )");
    // || uses LLVM or instruction on i1
    expect_ir_contains(ir, "or i1", "IR should use 'or i1' for ||");
}

TEST_F(CodegenBuiltinsTest, LogicalNotOperator) {
    std::string ir = generate(R"(
        func main() {
            let a: Bool = true
            let b: Bool = !a
        }
    )");
    expect_ir_contains(ir, "xor i1", "IR should use xor for logical not");
}

TEST_F(CodegenBuiltinsTest, LogicalAndKeyword) {
    std::string ir = generate(R"(
        func main() {
            let a: Bool = true
            let b: Bool = false
            let c: Bool = a and b
        }
    )");
    expect_ir_contains(ir, "and i1", "IR should use 'and i1' for 'and'");
}

TEST_F(CodegenBuiltinsTest, LogicalOrKeyword) {
    std::string ir = generate(R"(
        func main() {
            let a: Bool = true
            let b: Bool = false
            let c: Bool = a or b
        }
    )");
    expect_ir_contains(ir, "or i1", "IR should use 'or i1' for 'or'");
}

TEST_F(CodegenBuiltinsTest, LogicalNotKeyword) {
    std::string ir = generate(R"(
        func main() {
            let a: Bool = true
            let b: Bool = not a
        }
    )");
    expect_ir_contains(ir, "xor i1", "IR should use xor for 'not'");
}

// ============================================================================
// Type Cast Tests (as)
// ============================================================================

TEST_F(CodegenBuiltinsTest, CastI32ToI64) {
    std::string ir = generate(R"(
        func main() {
            let x: I32 = 42
            let y: I64 = x as I64
        }
    )");
    expect_ir_contains(ir, "sext i32", "IR should sign-extend i32 to i64");
}

TEST_F(CodegenBuiltinsTest, CastI64ToI32) {
    std::string ir = generate(R"(
        func main() {
            let x: I64 = 42
            let y: I32 = x as I32
        }
    )");
    expect_ir_contains(ir, "trunc i64", "IR should truncate i64 to i32");
}

TEST_F(CodegenBuiltinsTest, CastI32ToF64) {
    std::string ir = generate(R"(
        func main() {
            let x: I32 = 42
            let y: F64 = x as F64
        }
    )");
    expect_ir_contains(ir, "sitofp i32", "IR should convert i32 to f64");
}

TEST_F(CodegenBuiltinsTest, CastF64ToI32) {
    std::string ir = generate(R"(
        func main() {
            let x: F64 = 3.14
            let y: I32 = x as I32
        }
    )");
    expect_ir_contains(ir, "fptosi double", "IR should convert f64 to i32");
}

TEST_F(CodegenBuiltinsTest, CastBoolToI32) {
    std::string ir = generate(R"(
        func main() {
            let x: Bool = true
            let y: I32 = x as I32
        }
    )");
    expect_ir_contains(ir, "zext i1", "IR should zero-extend bool to i32");
}

TEST_F(CodegenBuiltinsTest, CastI32ToBool) {
    std::string ir = generate(R"(
        func main() {
            let x: I32 = 1
            let y: Bool = x as Bool
        }
    )");
    expect_ir_contains(ir, "icmp ne i32", "IR should compare i32 != 0 for bool cast");
}
