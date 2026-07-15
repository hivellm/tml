TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Runtime Declarations
//!
//! This file emits the target header and runtime type/function declarations.
//! Module import codegen is in runtime_modules.cpp.
//!
//! ## Dead Declaration Elimination (Phase 3)
//!
//! Runtime function declarations are registered in a catalog during init.
//! During codegen, emit_line() auto-detects @symbol references and marks
//! them as needed. At finalization, only needed declarations are emitted.
//!
//! ## Emitted Sections
//!
//! | Method                         | Emits                         |
//! |--------------------------------|-------------------------------|
//! | `emit_header`                  | Target triple, comments       |
//! | `emit_runtime_decls`           | Struct types, catalog init    |
//! | `finalize_runtime_decls`       | Only needed function declares |
//!
//! ## Runtime Types
//!
//! | Type            | Layout              | Purpose             |
//! |-----------------|---------------------|---------------------|
//! | `%struct.tml_str` | `{ ptr, i64 }`    | String slice        |
//! | `%struct.Ordering` | `{ i32 }`        | Comparison result   |

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "codegen/target.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "version_generated.hpp"

#include <filesystem>
#include <sstream>
#include <unordered_set>

namespace tml::codegen {

void LLVMIRGen::emit_header() {
    // Source filename (like Rust's `source_filename = "file.rs"`)
    if (!options_.source_file.empty()) {
        std::filesystem::path p(options_.source_file);
        emit_line("source_filename = \"" + p.filename().string() + "\"");
    }

    // Target datalayout computed from the target triple
    auto target = Target::from_triple(options_.target_triple);
    if (target) {
        emit_line("target datalayout = \"" + target->to_data_layout() + "\"");
    }

    emit_line("target triple = \"" + options_.target_triple + "\"");
    emit_line("");

    // Compiler identification embedded in the binary.
    // comdat any + linkonce_odr: linker keeps one copy across multiple .obj files.
    // @llvm.used: prevents LLVM from stripping the unreferenced constant.
    // Identifiable via `strings foo.exe | grep "tml version"`.
    std::string ident = "tml version " + std::string(tml::VERSION);
    emit_line("$__tml_ident = comdat any");
    emit_line("@__tml_ident = linkonce_odr constant [" + std::to_string(ident.size() + 1) +
              " x i8] c\"" + ident + "\\00\", comdat, align 1");
    emit_line(
        "@llvm.used = appending global [1 x ptr] [ptr @__tml_ident], section \"llvm.metadata\"");
    emit_line("");
}

// ============================================================================
// Runtime Declaration Catalog
// ============================================================================

void LLVMIRGen::init_runtime_catalog() {
    runtime_catalog_.clear();
    runtime_catalog_index_.clear();
    needed_runtime_decls_.clear();

    auto add = [&](const std::string& name, const std::string& ir,
                   std::vector<std::string> deps = {}) {
        size_t idx = runtime_catalog_.size();
        runtime_catalog_.push_back({name, ir, std::move(deps)});
        runtime_catalog_index_[name] = idx;
    };

    // --- C stdlib functions ---
    // NOTE: All non-intrinsic declarations use `dso_local` to prevent LLVM 23+
    // from merging function declarations with similar signatures during codegen.
    // Without this, LLVM's GlobalOpt can merge e.g. print_i32(i32) with println(ptr)
    // because they are both `void(...)` at the opaque-pointer level, causing segfaults.
    add("printf", "declare dso_local i32 @printf(ptr, ...)");
    add("puts", "declare dso_local i32 @puts(ptr)");
    add("putchar", "declare dso_local i32 @putchar(i32)");
    add("malloc", "declare dso_local noalias ptr @malloc(i64) allocsize(0) allockind(\"alloc,uninitialized\") \"alloc-family\"=\"malloc\"");
    add("free", "declare dso_local void @free(ptr) nounwind allockind(\"free\") \"alloc-family\"=\"malloc\"");
    add("tml_str_free", "declare dso_local void @tml_str_free(ptr)");
    add("exit", "declare dso_local void @exit(i32) noreturn");
    // strlen/strcmp/memcmp carry the attribute set LLVM's SimplifyLibCalls
    // uses to recognize them as canonical libc functions. `readonly` +
    // `nounwind` + `willreturn` is enough for the optimizer to constant-
    // fold `strlen` of a compile-time literal (phase1h) and keeps
    // backward compatibility with older LLVM attribute syntax.
    add("strlen", "declare dso_local i64 @strlen(ptr) readonly nounwind willreturn");
    add("strcmp", "declare dso_local i32 @strcmp(ptr, ptr) readonly nounwind willreturn");
    add("memcmp", "declare dso_local i32 @memcmp(ptr, ptr, i64) readonly nounwind willreturn");
    add("getenv", "declare dso_local ptr @getenv(ptr)");
    add("tml_coverage_write_file", "declare dso_local void @tml_coverage_write_file(ptr)");

    // --- LLVM intrinsics ---
    add("llvm.memcpy.p0.p0.i64", "declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)");
    add("llvm.memmove.p0.p0.i64", "declare void @llvm.memmove.p0.p0.i64(ptr, ptr, i64, i1)");
    add("llvm.memset.p0.i64", "declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)");
    add("llvm.assume", "declare void @llvm.assume(i1) nounwind");

    // --- TML runtime ---
    add("panic", "declare dso_local void @panic(ptr) noreturn");
    add("assert_tml_loc", "declare dso_local void @assert_tml_loc(i32, ptr, ptr, i32) noreturn");

    // --- Panic catching ---
    add("tml_run_should_panic", "declare dso_local i32 @tml_run_should_panic(ptr)");
    add("tml_panic_message_contains", "declare dso_local i32 @tml_panic_message_contains(ptr)");
    add("tml_run_test_with_catch", "declare dso_local i32 @tml_run_test_with_catch(ptr)");

    // --- Test timeout ---
    add("tml_set_test_timeout", "declare dso_local void @tml_set_test_timeout(i32)");

    // --- Backtrace ---
    add("tml_enable_backtrace_on_panic", "declare dso_local void @tml_enable_backtrace_on_panic()");

    // --- Coverage (conditional, but registered for completeness) ---
    add("tml_cover_func", "declare dso_local void @tml_cover_func(ptr)");

    // --- Debug intrinsics ---
    add("llvm.dbg.declare",
        "declare void @llvm.dbg.declare(metadata, metadata, metadata) nounwind readnone");
    add("llvm.dbg.value",
        "declare void @llvm.dbg.value(metadata, metadata, metadata) nounwind readnone");

    // --- LLVM instrumentation profile ---
    add("llvm.instrprof.increment",
        "declare void @llvm.instrprof.increment(ptr, i64, i32, i32) #1");

    // --- Stack management ---
    add("llvm.stacksave", "declare ptr @llvm.stacksave() nounwind");
    add("llvm.stackrestore", "declare void @llvm.stackrestore(ptr) nounwind");

    // --- Lifetime intrinsics ---
    add("llvm.lifetime.start.p0",
        "declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) nounwind");
    add("llvm.lifetime.end.p0",
        "declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) nounwind");

    // --- I/O functions ---
    add("print", "declare dso_local void @print(ptr)");
    add("println", "declare dso_local void @println(ptr)");
    add("print_i32", "declare dso_local void @print_i32(i32)");
    add("print_i64", "declare dso_local void @print_i64(i64)");
    add("print_f64", "declare dso_local void @print_f64(double)");
    add("print_bool", "declare dso_local void @print_bool(i32)");

    // --- Float formatting (C runtime) ---
    add("f64_to_string", "declare dso_local ptr @f64_to_string(double)");
    add("f32_to_string", "declare dso_local ptr @f32_to_string(float)");
    add("f64_to_string_precision", "declare dso_local ptr @f64_to_string_precision(double, i64)");
    add("f32_to_string_precision", "declare dso_local ptr @f32_to_string_precision(float, i64)");
    add("f64_to_exp_string", "declare dso_local ptr @f64_to_exp_string(double, i32)");
    add("f32_to_exp_string", "declare dso_local ptr @f32_to_exp_string(float, i32)");

    // --- Random seed ---
    add("tml_random_seed", "declare dso_local i64 @tml_random_seed()");

    // --- CPU Profiler instrumentation (tml_profiler_enter/exit from profiler.cpp) ---
    add("tml_profiler_enter", "declare dso_local void @tml_profiler_enter(ptr, ptr, i32)");
    add("tml_profiler_exit", "declare dso_local void @tml_profiler_exit()");
    add("tml_profiler_is_active", "declare dso_local i32 @tml_profiler_is_active()");

    // --- Memory functions ---
    add("mem_alloc", "declare dso_local noalias ptr @mem_alloc(i64) allocsize(0) allockind(\"alloc,uninitialized\") \"alloc-family\"=\"malloc\"");
    add("mem_alloc_zeroed", "declare dso_local noalias ptr @mem_alloc_zeroed(i64) allocsize(0) allockind(\"alloc,zeroed\") \"alloc-family\"=\"malloc\"");
    add("mem_realloc", "declare dso_local noalias ptr @mem_realloc(ptr, i64) allocsize(1) allockind(\"realloc\") \"alloc-family\"=\"malloc\"");
    add("mem_free", "declare dso_local void @mem_free(ptr) nounwind allockind(\"free\") \"alloc-family\"=\"malloc\"");
    // mem_usable_size: wraps `_msize` (Windows) / `malloc_usable_size` (glibc)
    // / `malloc_size` (macOS). Guarded against non-heap pointers — returns 0
    // for `.rdata` literals. Used by `str_append` (phase1i) to amortize
    // repeated string concatenation to O(1) per append.
    add("mem_usable_size", "declare dso_local i64 @mem_usable_size(ptr) readonly nounwind willreturn");

    // phase1k length-prefixed Str buffers. Heap strings carry a 16-byte
    // header `[cap:i64 | len:i64]` at offset -16 so `Str.len()` is O(1)
    // and `str_append` skips the O(n) `strlen(accumulator)` that dominated
    // the phase1i timings. Literals in `.rdata` transparently fall back
    // to `strlen` via tml_str_len's image-range check.
    add("tml_str_alloc", "declare dso_local noalias ptr @tml_str_alloc(i64) nounwind");
    add("tml_str_alloc_len", "declare dso_local noalias ptr @tml_str_alloc_len(i64) nounwind");
    add("tml_str_alloc_with_cap", "declare dso_local noalias ptr @tml_str_alloc_with_cap(i64, i64) nounwind");
    add("tml_str_len", "declare dso_local i64 @tml_str_len(ptr) nounwind");
    add("tml_str_set_len", "declare dso_local void @tml_str_set_len(ptr, i64) nounwind");
    add("tml_str_cap", "declare dso_local i64 @tml_str_cap(ptr) nounwind");
    add("tml_str_realloc", "declare dso_local ptr @tml_str_realloc(ptr, i64) nounwind");

    add("mem_copy", "declare dso_local void @mem_copy(ptr, ptr, i64)");
    add("mem_move", "declare dso_local void @mem_move(ptr, ptr, i64)");
    add("mem_set", "declare dso_local void @mem_set(ptr, i32, i64)");
    add("mem_zero", "declare dso_local void @mem_zero(ptr, i64)");
    add("mem_compare", "declare dso_local i32 @mem_compare(ptr, ptr, i64)");
    add("mem_eq", "declare dso_local i32 @mem_eq(ptr, ptr, i64)");

    // --- Object pool ---
    add("pool_acquire", "declare dso_local ptr @pool_acquire(ptr, i64)");
    add("pool_release", "declare dso_local void @pool_release(ptr, ptr)");
    add("tls_pool_acquire", "declare dso_local ptr @tls_pool_acquire(ptr, i64)");
    add("tls_pool_release", "declare dso_local void @tls_pool_release(ptr, ptr, i64)");

    // --- Atomic operations ---
    add("atomic_fetch_add_i32", "declare dso_local i32 @atomic_fetch_add_i32(ptr, i32)");
    add("atomic_fetch_sub_i32", "declare dso_local i32 @atomic_fetch_sub_i32(ptr, i32)");
    add("atomic_load_i32", "declare dso_local i32 @atomic_load_i32(ptr)");
    add("atomic_store_i32", "declare dso_local void @atomic_store_i32(ptr, i32)");
    add("atomic_compare_exchange_i32",
        "declare dso_local i32 @atomic_compare_exchange_i32(ptr, i32, i32)");
    add("atomic_swap_i32", "declare dso_local i32 @atomic_swap_i32(ptr, i32)");
    add("atomic_fence", "declare dso_local void @atomic_fence()");
    add("atomic_fence_acquire", "declare dso_local void @atomic_fence_acquire()");
    add("atomic_fence_release", "declare dso_local void @atomic_fence_release()");

    // --- Async runtime ---
    add("tml_executor_new", "declare dso_local ptr @tml_executor_new()");
    add("tml_executor_destroy", "declare dso_local void @tml_executor_destroy(ptr)");
    add("tml_executor_spawn", "declare dso_local i64 @tml_executor_spawn(ptr, ptr, ptr, i64)");
    add("tml_executor_run", "declare dso_local i32 @tml_executor_run(ptr)");
    add("tml_executor_wake", "declare dso_local void @tml_executor_wake(ptr, i64)");
    add("tml_block_on_rt", "declare dso_local { i32, i32, i64 } @tml_block_on(ptr, ptr, i64)");
    add("tml_executor_poll_task", "declare dso_local i32 @tml_executor_poll_task(ptr, ptr)");
    add("tml_waker_create", "declare dso_local { ptr, ptr, i64 } @tml_waker_create(ptr, i64)");
    add("tml_waker_wake", "declare dso_local void @tml_waker_wake(ptr)");
    add("tml_waker_clone", "declare dso_local { ptr, ptr, i64 } @tml_waker_clone(ptr)");
    add("tml_waker_destroy", "declare dso_local void @tml_waker_destroy(ptr)");
    add("tml_poll_ready_i64", "declare dso_local { i32, i32, i64 } @tml_poll_ready_i64(i64)");
    add("tml_poll_ready_ptr", "declare dso_local { i32, i32, i64 } @tml_poll_ready_ptr(ptr)");
    add("tml_poll_pending", "declare dso_local { i32, i32, i64 } @tml_poll_pending()");
    add("tml_poll_is_ready", "declare dso_local i32 @tml_poll_is_ready(ptr)");
    add("tml_poll_is_pending", "declare dso_local i32 @tml_poll_is_pending(ptr)");
    add("tml_timer_new", "declare dso_local { i64, i64, i32 } @tml_timer_new(i64)");
    add("tml_timer_new_ptr", "declare dso_local void @tml_timer_new_ptr(i64, ptr)");
    add("tml_sleep_poll", "declare dso_local { i32, i32, i64 } @tml_sleep_poll(ptr, ptr)");
    add("tml_sleep_poll_ptr", "declare dso_local void @tml_sleep_poll_ptr(ptr, ptr, ptr)");
    add("tml_delay_poll", "declare dso_local { i32, i32, i64 } @tml_delay_poll(ptr, ptr)");
    add("tml_delay_poll_ptr", "declare dso_local void @tml_delay_poll_ptr(ptr, ptr, ptr)");
    add("tml_yield_poll", "declare dso_local { i32, i32, i64 } @tml_yield_poll(ptr, ptr)");
    add("tml_yield_poll_ptr", "declare dso_local void @tml_yield_poll_ptr(ptr, ptr, ptr)");
    add("tml_channel_new", "declare dso_local ptr @tml_channel_new(i64, i64)");
    add("tml_channel_destroy", "declare dso_local void @tml_channel_destroy(ptr)");
    add("tml_channel_try_send", "declare dso_local i32 @tml_channel_try_send(ptr, ptr)");
    add("tml_channel_try_recv", "declare dso_local i32 @tml_channel_try_recv(ptr, ptr)");
    add("tml_channel_close", "declare dso_local void @tml_channel_close(ptr)");
    add("tml_channel_is_empty", "declare dso_local i32 @tml_channel_is_empty(ptr)");
    add("tml_channel_is_full", "declare dso_local i32 @tml_channel_is_full(ptr)");
    add("tml_spawn",
        "declare dso_local { i64, ptr, i32, { i32, i32, i64 } } @tml_spawn(ptr, ptr, ptr, i64)");
    add("tml_join_poll", "declare dso_local { i32, i32, i64 } @tml_join_poll(ptr, ptr)");

    // --- Log runtime ---
    add("rt_log_msg", "declare dso_local void @rt_log_msg(i32, ptr, ptr)");
    add("rt_log_set_level", "declare dso_local void @rt_log_set_level(i32)");
    add("rt_log_get_level", "declare dso_local i32 @rt_log_get_level()");
    add("rt_log_enabled", "declare dso_local i32 @rt_log_enabled(i32)");
    add("rt_log_set_filter", "declare dso_local void @rt_log_set_filter(ptr)");
    add("rt_log_module_enabled", "declare dso_local i32 @rt_log_module_enabled(i32, ptr)");
    add("rt_log_structured", "declare dso_local void @rt_log_structured(i32, ptr, ptr, ptr)");
    add("rt_log_set_format", "declare dso_local void @rt_log_set_format(i32)");
    add("rt_log_get_format", "declare dso_local i32 @rt_log_get_format()");
    add("rt_log_open_file", "declare dso_local i32 @rt_log_open_file(ptr)");
    add("rt_log_close_file", "declare dso_local void @rt_log_close_file()");
    add("rt_log_init_from_env", "declare dso_local i32 @rt_log_init_from_env()");

    // --- Console runtime (diagnostics/console.c) ---
    add("rt_console_timer_start", "declare dso_local i64 @rt_console_timer_start(ptr, i64)");
    add("rt_console_timer_end", "declare dso_local i64 @rt_console_timer_end(ptr, i64)");
    add("rt_console_count_inc", "declare dso_local i64 @rt_console_count_inc(ptr)");
    add("rt_console_count_reset", "declare dso_local i64 @rt_console_count_reset(ptr)");
    add("rt_console_indent_inc", "declare dso_local i64 @rt_console_indent_inc()");
    add("rt_console_indent_dec", "declare dso_local i64 @rt_console_indent_dec()");
    add("rt_console_indent_get", "declare dso_local i64 @rt_console_indent_get()");

    // --- Inline IR definitions (multi-line, with dependencies) ---

    // core::str::basic::len — free function called by impl Str::len (AST codegen path).
    // Symbol: tml_N4core3str5basic3lenE_S (module=core::str::basic, func=len, param=Str)
    // Mirrors the MIR-path inline definition of @tml_N4core3str3lenE_S in mir_codegen.cpp.
    // Required because emit_module_pure_tml_functions() may be skipped (cached_library_state
    // fast path) or core::str::basic may not be in imported_module_paths, leaving this
    // free function body unregistered in pending_library_funcs_.
    add("tml_N4core3str5basic3lenE_S",
        "define internal i64 @tml_N4core3str5basic3lenE_S(ptr %s) {\n"
        "entry:\n"
        "  %is_null = icmp eq ptr %s, null\n"
        "  br i1 %is_null, label %str_len_ret0, label %str_len_call\n"
        "str_len_ret0:\n"
        "  ret i64 0\n"
        "str_len_call:\n"
        "  %r = call i64 @strlen(ptr %s)\n"
        "  ret i64 %r\n"
        "}",
        {"strlen"});

    // core::str::basic::is_empty — free function; depends on len above.
    add("tml_N4core3str5basic8is_emptyE_S",
        "define internal i1 @tml_N4core3str5basic8is_emptyE_S(ptr %s) {\n"
        "entry:\n"
        "  %len = call i64 @tml_N4core3str5basic3lenE_S(ptr %s)\n"
        "  %r = icmp eq i64 %len, 0\n"
        "  ret i1 %r\n"
        "}",
        {"tml_N4core3str5basic3lenE_S"});

    // core::str::search::starts_with(s: Str, prefix: Str) -> Bool
    // Symbol: tml_N4core3str6search11starts_withE_SS
    // Mirrors lib/core/src/str/search.tml starts_with implementation.
    add("tml_N4core3str6search11starts_withE_SS",
        "define internal i1 @tml_N4core3str6search11starts_withE_SS(ptr %s, ptr %prefix) {\n"
        "entry:\n"
        "  %pnull = icmp eq ptr %prefix, null\n"
        "  br i1 %pnull, label %ret_true, label %check_s\n"
        "check_s:\n"
        "  %snull = icmp eq ptr %s, null\n"
        "  br i1 %snull, label %ret_false, label %get_lens\n"
        "get_lens:\n"
        "  %s_len = call i64 @strlen(ptr %s)\n"
        "  %p_len = call i64 @strlen(ptr %prefix)\n"
        "  %pz = icmp eq i64 %p_len, 0\n"
        "  br i1 %pz, label %ret_true, label %check_fit\n"
        "check_fit:\n"
        "  %too_big = icmp ugt i64 %p_len, %s_len\n"
        "  br i1 %too_big, label %ret_false, label %do_cmp\n"
        "do_cmp:\n"
        "  %cmp = call i32 @memcmp(ptr %s, ptr %prefix, i64 %p_len)\n"
        "  %r = icmp eq i32 %cmp, 0\n"
        "  ret i1 %r\n"
        "ret_true:\n"
        "  ret i1 true\n"
        "ret_false:\n"
        "  ret i1 false\n"
        "}",
        {"strlen", "memcmp"});

    // core::str::search::ends_with(s: Str, suffix: Str) -> Bool
    // Symbol: tml_N4core3str6search9ends_withE_SS
    add("tml_N4core3str6search9ends_withE_SS",
        "define internal i1 @tml_N4core3str6search9ends_withE_SS(ptr %s, ptr %suffix) {\n"
        "entry:\n"
        "  %pnull = icmp eq ptr %suffix, null\n"
        "  br i1 %pnull, label %ret_true, label %check_s\n"
        "check_s:\n"
        "  %snull = icmp eq ptr %s, null\n"
        "  br i1 %snull, label %ret_false, label %get_lens\n"
        "get_lens:\n"
        "  %s_len = call i64 @strlen(ptr %s)\n"
        "  %p_len = call i64 @strlen(ptr %suffix)\n"
        "  %pz = icmp eq i64 %p_len, 0\n"
        "  br i1 %pz, label %ret_true, label %check_fit\n"
        "check_fit:\n"
        "  %too_big = icmp ugt i64 %p_len, %s_len\n"
        "  br i1 %too_big, label %ret_false, label %do_cmp\n"
        "do_cmp:\n"
        "  %offset = sub i64 %s_len, %p_len\n"
        "  %ptr = getelementptr i8, ptr %s, i64 %offset\n"
        "  %cmp = call i32 @memcmp(ptr %ptr, ptr %suffix, i64 %p_len)\n"
        "  %r = icmp eq i32 %cmp, 0\n"
        "  ret i1 %r\n"
        "ret_true:\n"
        "  ret i1 true\n"
        "ret_false:\n"
        "  ret i1 false\n"
        "}",
        {"strlen", "memcmp"});

    // core::str::search::contains(s: Str, pattern: Str) -> Bool
    // Symbol: tml_N4core3str6search8containsE_SS
    // Scalar scan: for each position i in [0, s_len - p_len], check memcmp.
    add("tml_N4core3str6search8containsE_SS",
        "define internal i1 @tml_N4core3str6search8containsE_SS(ptr %s, ptr %pattern) {\n"
        "entry:\n"
        "  %pnull = icmp eq ptr %pattern, null\n"
        "  br i1 %pnull, label %ret_true, label %check_s\n"
        "check_s:\n"
        "  %snull = icmp eq ptr %s, null\n"
        "  br i1 %snull, label %ret_false, label %get_lens\n"
        "get_lens:\n"
        "  %s_len = call i64 @strlen(ptr %s)\n"
        "  %p_len = call i64 @strlen(ptr %pattern)\n"
        "  %pz = icmp eq i64 %p_len, 0\n"
        "  br i1 %pz, label %ret_true, label %check_fit\n"
        "check_fit:\n"
        "  %too_big = icmp ugt i64 %p_len, %s_len\n"
        "  br i1 %too_big, label %ret_false, label %loop_init\n"
        "loop_init:\n"
        "  %limit = sub i64 %s_len, %p_len\n"
        "  br label %loop_hdr\n"
        "loop_hdr:\n"
        "  %i = phi i64 [ 0, %loop_init ], [ %i_next, %loop_cont ]\n"
        "  %done = icmp ugt i64 %i, %limit\n"
        "  br i1 %done, label %ret_false, label %loop_body\n"
        "loop_body:\n"
        "  %ptr = getelementptr i8, ptr %s, i64 %i\n"
        "  %cmp = call i32 @memcmp(ptr %ptr, ptr %pattern, i64 %p_len)\n"
        "  %eq = icmp eq i32 %cmp, 0\n"
        "  br i1 %eq, label %ret_true, label %loop_cont\n"
        "loop_cont:\n"
        "  %i_next = add i64 %i, 1\n"
        "  br label %loop_hdr\n"
        "ret_true:\n"
        "  ret i1 true\n"
        "ret_false:\n"
        "  ret i1 false\n"
        "}",
        {"strlen", "memcmp"});

    // core::str::transform::trim(s: Str) -> Str
    // Symbol: tml_N4core3str9transform4trimE_S
    // Scan forward for first non-whitespace, backward for last non-whitespace,
    // allocate new buffer of (end-start+1) bytes, memcpy, null-terminate.
    // Whitespace: space(32), tab(9), newline(10), CR(13).
    add("tml_N4core3str9transform4trimE_S",
        "define internal ptr @tml_N4core3str9transform4trimE_S(ptr %s) {\n"
        "entry:\n"
        "  %snull = icmp eq ptr %s, null\n"
        "  br i1 %snull, label %ret_null, label %get_len\n"
        "get_len:\n"
        "  %s_len = call i64 @strlen(ptr %s)\n"
        "  %zero = icmp eq i64 %s_len, 0\n"
        "  br i1 %zero, label %ret_null, label %scan_start\n"
        "scan_start:\n"
        "  br label %fwd_hdr\n"
        "fwd_hdr:\n"
        "  %fi = phi i64 [ 0, %scan_start ], [ %fi_next, %fwd_cont ]\n"
        "  %fdone = icmp uge i64 %fi, %s_len\n"
        "  br i1 %fdone, label %ret_empty, label %fwd_body\n"
        "fwd_body:\n"
        "  %fc = getelementptr i8, ptr %s, i64 %fi\n"
        "  %fv = load i8, ptr %fc\n"
        "  %fsp = icmp eq i8 %fv, 32\n"
        "  %ftb = icmp eq i8 %fv, 9\n"
        "  %fnl = icmp eq i8 %fv, 10\n"
        "  %fcr = icmp eq i8 %fv, 13\n"
        "  %fws1 = or i1 %fsp, %ftb\n"
        "  %fws2 = or i1 %fnl, %fcr\n"
        "  %fws = or i1 %fws1, %fws2\n"
        "  br i1 %fws, label %fwd_cont, label %scan_end\n"
        "fwd_cont:\n"
        "  %fi_next = add i64 %fi, 1\n"
        "  br label %fwd_hdr\n"
        "scan_end:\n"
        "  %start = phi i64 [ %fi, %fwd_body ]\n"
        "  %sl_minus1 = sub i64 %s_len, 1\n"
        "  br label %bwd_hdr\n"
        "bwd_hdr:\n"
        "  %bi = phi i64 [ %sl_minus1, %scan_end ], [ %bi_next, %bwd_cont ]\n"
        "  %bdone = icmp ult i64 %bi, %start\n"
        "  br i1 %bdone, label %ret_empty, label %bwd_body\n"
        "bwd_body:\n"
        "  %bc = getelementptr i8, ptr %s, i64 %bi\n"
        "  %bv = load i8, ptr %bc\n"
        "  %bsp = icmp eq i8 %bv, 32\n"
        "  %btb = icmp eq i8 %bv, 9\n"
        "  %bnl = icmp eq i8 %bv, 10\n"
        "  %bcr = icmp eq i8 %bv, 13\n"
        "  %bws1 = or i1 %bsp, %btb\n"
        "  %bws2 = or i1 %bnl, %bcr\n"
        "  %bws = or i1 %bws1, %bws2\n"
        "  br i1 %bws, label %bwd_cont, label %alloc\n"
        "bwd_cont:\n"
        "  %bi_next = sub i64 %bi, 1\n"
        "  br label %bwd_hdr\n"
        "alloc:\n"
        "  %end = phi i64 [ %bi, %bwd_body ]\n"
        "  %new_len = sub i64 %end, %start\n"
        "  %new_len1 = add i64 %new_len, 1\n"
        "  %alloc_sz = add i64 %new_len1, 1\n"
        "  %buf = call ptr @mem_alloc(i64 %alloc_sz)\n"
        "  %src = getelementptr i8, ptr %s, i64 %start\n"
        "  call void @llvm.memcpy.p0.p0.i64(ptr %buf, ptr %src, i64 %new_len1, i1 false)\n"
        "  %null_pos = getelementptr i8, ptr %buf, i64 %new_len1\n"
        "  store i8 0, ptr %null_pos\n"
        "  ret ptr %buf\n"
        "ret_empty:\n"
        "  %ebuf = call ptr @mem_alloc(i64 1)\n"
        "  store i8 0, ptr %ebuf\n"
        "  ret ptr %ebuf\n"
        "ret_null:\n"
        "  ret ptr null\n"
        "}",
        {"strlen", "mem_alloc", "llvm.memcpy.p0.p0.i64"});

    // str_eq depends on strcmp
    add("str_eq",
        "; String utilities (inline IR)\n"
        "define internal i32 @str_eq(ptr %a, ptr %b) {\n"
        "entry:\n"
        "  %a_null = icmp eq ptr %a, null\n"
        "  %b_null = icmp eq ptr %b, null\n"
        "  %both_null = and i1 %a_null, %b_null\n"
        "  br i1 %both_null, label %ret_true, label %check_either\n"
        "ret_true:\n"
        "  ret i32 1\n"
        "check_either:\n"
        "  %either_null = or i1 %a_null, %b_null\n"
        "  br i1 %either_null, label %ret_false, label %compare\n"
        "ret_false:\n"
        "  ret i32 0\n"
        "compare:\n"
        "  %cmp = call i32 @strcmp(ptr %a, ptr %b)\n"
        "  %eq = icmp eq i32 %cmp, 0\n"
        "  %result = zext i1 %eq to i32\n"
        "  ret i32 %result\n"
        "}",
        {"strcmp"});

    // .str.empty global needed by str_concat_opt
    add(".str.empty", "@.str.empty = private constant [1 x i8] c\"\\00\"");

    // str_concat_opt depends on strlen, mem_alloc, memcpy, .str.empty
    // Uses mem_alloc instead of malloc so the memory tracker can track
    // the allocation and tml_str_free can properly deregister it.
    add("str_concat_opt",
        "define internal ptr @str_concat_opt(ptr %a, ptr %b) {\n"
        "entry:\n"
        "  %a_null = icmp eq ptr %a, null\n"
        "  %a_safe = select i1 %a_null, ptr @.str.empty, ptr %a\n"
        "  %b_null = icmp eq ptr %b, null\n"
        "  %b_safe = select i1 %b_null, ptr @.str.empty, ptr %b\n"
        "  %len_a = call i64 @strlen(ptr %a_safe)\n"
        "  %len_b = call i64 @strlen(ptr %b_safe)\n"
        "  %total = add i64 %len_a, %len_b\n"
        "  %alloc = add i64 %total, 1\n"
        "  %buf = call ptr @mem_alloc(i64 %alloc)\n"
        "  call void @llvm.memcpy.p0.p0.i64(ptr %buf, ptr %a_safe, i64 %len_a, i1 false)\n"
        "  %dst = getelementptr i8, ptr %buf, i64 %len_a\n"
        "  call void @llvm.memcpy.p0.p0.i64(ptr %dst, ptr %b_safe, i64 %len_b, i1 false)\n"
        "  %end = getelementptr i8, ptr %buf, i64 %total\n"
        "  store i8 0, ptr %end\n"
        "  ret ptr %buf\n"
        "}",
        {"strlen", "mem_alloc", "llvm.memcpy.p0.p0.i64", ".str.empty"});

    // str_append — amortized O(1) string append (phase1i).
    //
    // Semantics: consumes `%a`, produces a result that may alias `%a`. MIR
    // codegen emits this for `ptr + ptr` `Add` instructions — the result is
    // a fresh SSA value, so aliasing with the input is transparent to the
    // caller. When the input is a heap allocation with enough capacity, we
    // append in place (single memcpy + NUL store, zero alloc). When the
    // block must grow, we realloc with exponential doubling so subsequent
    // appends keep hitting the fast path. Literals (`.rdata`) take a
    // `fresh` path that allocates with initial slack so the next concat on
    // the returned buffer starts amortizing.
    //
    // Result: `s = s + "x"` in a loop goes from O(n²) (fresh alloc + full
    // copy per iter) to O(n) total, matching Rust's `String::push_str`.
    // phase1k — length-prefixed buffers. `tml_str_len` reads the cached
    // `len` at `ptr[-8]` in O(1) for heap strings (falls back to `strlen`
    // for literals). `tml_str_cap` drives the in-place / grow / fresh
    // branch; allocations go through `tml_str_alloc_with_cap` /
    // `tml_str_realloc` so the returned buffer is always length-prefixed
    // and subsequent `str_append` calls hit the fast path.
    add("str_append",
        "define internal ptr @str_append(ptr %a, ptr %b) alwaysinline {\n"
        "entry:\n"
        "  %a_null = icmp eq ptr %a, null\n"
        "  %a_safe = select i1 %a_null, ptr @.str.empty, ptr %a\n"
        "  %b_null = icmp eq ptr %b, null\n"
        "  %b_safe = select i1 %b_null, ptr @.str.empty, ptr %b\n"
        "  %len_a = call i64 @tml_str_len(ptr %a_safe)\n"
        "  %len_b = call i64 @tml_str_len(ptr %b_safe)\n"
        "  %total = add i64 %len_a, %len_b\n"
        "  %cap = call i64 @tml_str_cap(ptr %a_safe)\n"
        "  %is_heap = icmp ugt i64 %cap, 0\n"
        "  %fits = icmp uge i64 %cap, %total\n"
        "  %inplace_ok = and i1 %is_heap, %fits\n"
        "  br i1 %inplace_ok, label %app_inplace, label %app_check\n"
        "app_inplace:\n"
        "  %dst_ip = getelementptr i8, ptr %a_safe, i64 %len_a\n"
        "  call void @llvm.memcpy.p0.p0.i64(ptr %dst_ip, ptr %b_safe, i64 %len_b, i1 false)\n"
        "  %end_ip = getelementptr i8, ptr %a_safe, i64 %total\n"
        "  store i8 0, ptr %end_ip\n"
        "  call void @tml_str_set_len(ptr %a_safe, i64 %total)\n"
        "  ret ptr %a_safe\n"
        "app_check:\n"
        "  br i1 %is_heap, label %app_grow, label %app_fresh\n"
        "app_grow:\n"
        "  %cap2 = shl i64 %cap, 1\n"
        "  %grow_hint = add i64 %cap2, 16\n"
        "  %grow_cmp = icmp ugt i64 %total, %grow_hint\n"
        "  %grow_sz = select i1 %grow_cmp, i64 %total, i64 %grow_hint\n"
        "  %buf_g = call ptr @tml_str_realloc(ptr %a_safe, i64 %grow_sz)\n"
        "  %dst_g = getelementptr i8, ptr %buf_g, i64 %len_a\n"
        "  call void @llvm.memcpy.p0.p0.i64(ptr %dst_g, ptr %b_safe, i64 %len_b, i1 false)\n"
        "  %end_g = getelementptr i8, ptr %buf_g, i64 %total\n"
        "  store i8 0, ptr %end_g\n"
        "  call void @tml_str_set_len(ptr %buf_g, i64 %total)\n"
        "  ret ptr %buf_g\n"
        "app_fresh:\n"
        "  %la2 = shl i64 %len_a, 1\n"
        "  %slack_hint = add i64 %la2, 32\n"
        "  %fresh_cmp = icmp ugt i64 %total, %slack_hint\n"
        "  %alloc_fresh = select i1 %fresh_cmp, i64 %total, i64 %slack_hint\n"
        "  %buf_f = call ptr @tml_str_alloc_with_cap(i64 %total, i64 %alloc_fresh)\n"
        "  call void @llvm.memcpy.p0.p0.i64(ptr %buf_f, ptr %a_safe, i64 %len_a, i1 false)\n"
        "  %dst_f = getelementptr i8, ptr %buf_f, i64 %len_a\n"
        "  call void @llvm.memcpy.p0.p0.i64(ptr %dst_f, ptr %b_safe, i64 %len_b, i1 false)\n"
        "  %end_f = getelementptr i8, ptr %buf_f, i64 %total\n"
        "  store i8 0, ptr %end_f\n"
        "  ret ptr %buf_f\n"
        "}",
        {"tml_str_len", "tml_str_cap", "tml_str_set_len",
         "tml_str_alloc_with_cap", "tml_str_realloc",
         "llvm.memcpy.p0.p0.i64", ".str.empty"});

    // phase1k — str_append_tracked: the augmented-concat call site emits
    // this variant (instead of str_append) when a shadow i64 alloca is
    // available for the accumulator's length. The caller pre-allocates
    // the shadow in its own stack frame; `str_append_tracked` reads
    // `len_a` from the shadow (SSA-promoted by LLVM SROA across loop
    // iterations) and writes the updated length back, skipping the
    // O(n) `strlen(a)` and the heap-header load/store that otherwise
    // keep `len` in memory every iteration. This is the same shape
    // rustc's `String::push_str` produces — `len` lives in a register
    // / phi-node for the duration of the loop.
    add("str_append_tracked",
        "define internal ptr @str_append_tracked(ptr %a, ptr %b, ptr %len_slot) alwaysinline {\n"
        "entry:\n"
        "  %a_null = icmp eq ptr %a, null\n"
        "  %a_safe = select i1 %a_null, ptr @.str.empty, ptr %a\n"
        "  %b_null = icmp eq ptr %b, null\n"
        "  %b_safe = select i1 %b_null, ptr @.str.empty, ptr %b\n"
        "  %shadow_len = load i64, ptr %len_slot, align 8\n"
        "  %a_magic_p = getelementptr i8, ptr %a_safe, i64 -24\n"
        "  %a_magic = load i64, ptr %a_magic_p\n"
        "  %a_is_ours = icmp eq i64 %a_magic, 3552126293525794637\n"
        "  br i1 %a_is_ours, label %at_has_hdr, label %at_literal\n"
        "at_has_hdr:\n"
        "  %at_cap_p = getelementptr i8, ptr %a_safe, i64 -16\n"
        "  %at_cap_hdr = load i64, ptr %at_cap_p\n"
        "  br label %at_done_a\n"
        "at_literal:\n"
        "  %at_lit_len = call i64 @strlen(ptr %a_safe)\n"
        "  br label %at_done_a\n"
        "at_done_a:\n"
        "  %at_len_a = phi i64 [ %shadow_len, %at_has_hdr ], [ %at_lit_len, %at_literal ]\n"
        "  %at_cap = phi i64 [ %at_cap_hdr, %at_has_hdr ], [ 0, %at_literal ]\n"
        "  %bt_magic_p = getelementptr i8, ptr %b_safe, i64 -24\n"
        "  %bt_magic = load i64, ptr %bt_magic_p\n"
        "  %bt_is_ours = icmp eq i64 %bt_magic, 3552126293525794637\n"
        "  br i1 %bt_is_ours, label %bt_has_hdr, label %bt_literal\n"
        "bt_has_hdr:\n"
        "  %bt_len_p = getelementptr i8, ptr %b_safe, i64 -8\n"
        "  %bt_len_hdr = load i64, ptr %bt_len_p\n"
        "  br label %bt_done\n"
        "bt_literal:\n"
        "  %bt_len_slow = call i64 @strlen(ptr %b_safe)\n"
        "  br label %bt_done\n"
        "bt_done:\n"
        "  %at_len_b = phi i64 [ %bt_len_hdr, %bt_has_hdr ], [ %bt_len_slow, %bt_literal ]\n"
        "  %at_total = add i64 %at_len_a, %at_len_b\n"
        "  %at_is_heap = icmp ugt i64 %at_cap, 0\n"
        "  %at_fits = icmp uge i64 %at_cap, %at_total\n"
        "  %at_inplace_ok = and i1 %at_is_heap, %at_fits\n"
        "  br i1 %at_inplace_ok, label %at_app_inplace, label %at_app_check\n"
        "at_app_inplace:\n"
        "  %at_dst_ip = getelementptr i8, ptr %a_safe, i64 %at_len_a\n"
        "  call void @llvm.memcpy.p0.p0.i64(ptr %at_dst_ip, ptr %b_safe, i64 %at_len_b, i1 false)\n"
        "  %at_end_ip = getelementptr i8, ptr %a_safe, i64 %at_total\n"
        "  store i8 0, ptr %at_end_ip\n"
        "  %at_hdr_len_p = getelementptr i8, ptr %a_safe, i64 -8\n"
        "  store i64 %at_total, ptr %at_hdr_len_p\n"
        "  store i64 %at_total, ptr %len_slot, align 8\n"
        "  ret ptr %a_safe\n"
        "at_app_check:\n"
        "  br i1 %at_is_heap, label %at_app_grow, label %at_app_fresh\n"
        "at_app_grow:\n"
        "  %at_cap2 = shl i64 %at_cap, 1\n"
        "  %at_grow_hint = add i64 %at_cap2, 16\n"
        "  %at_grow_cmp = icmp ugt i64 %at_total, %at_grow_hint\n"
        "  %at_grow_sz = select i1 %at_grow_cmp, i64 %at_total, i64 %at_grow_hint\n"
        "  %at_buf_g = call ptr @tml_str_realloc(ptr %a_safe, i64 %at_grow_sz)\n"
        "  %at_dst_g = getelementptr i8, ptr %at_buf_g, i64 %at_len_a\n"
        "  call void @llvm.memcpy.p0.p0.i64(ptr %at_dst_g, ptr %b_safe, i64 %at_len_b, i1 false)\n"
        "  %at_end_g = getelementptr i8, ptr %at_buf_g, i64 %at_total\n"
        "  store i8 0, ptr %at_end_g\n"
        "  %at_g_hdr_len_p = getelementptr i8, ptr %at_buf_g, i64 -8\n"
        "  store i64 %at_total, ptr %at_g_hdr_len_p\n"
        "  store i64 %at_total, ptr %len_slot, align 8\n"
        "  ret ptr %at_buf_g\n"
        "at_app_fresh:\n"
        "  %at_la2 = shl i64 %at_len_a, 1\n"
        "  %at_slack_hint = add i64 %at_la2, 32\n"
        "  %at_fresh_cmp = icmp ugt i64 %at_total, %at_slack_hint\n"
        "  %at_alloc_fresh = select i1 %at_fresh_cmp, i64 %at_total, i64 %at_slack_hint\n"
        "  %at_buf_f = call ptr @tml_str_alloc_with_cap(i64 %at_total, i64 %at_alloc_fresh)\n"
        "  call void @llvm.memcpy.p0.p0.i64(ptr %at_buf_f, ptr %a_safe, i64 %at_len_a, i1 false)\n"
        "  %at_dst_f = getelementptr i8, ptr %at_buf_f, i64 %at_len_a\n"
        "  call void @llvm.memcpy.p0.p0.i64(ptr %at_dst_f, ptr %b_safe, i64 %at_len_b, i1 false)\n"
        "  %at_end_f = getelementptr i8, ptr %at_buf_f, i64 %at_total\n"
        "  store i8 0, ptr %at_end_f\n"
        "  store i64 %at_total, ptr %len_slot, align 8\n"
        "  ret ptr %at_buf_f\n"
        "}",
        {"strlen", "tml_str_alloc_with_cap", "tml_str_realloc",
         "llvm.memcpy.p0.p0.i64", ".str.empty"});

    // str_concat_reuse: reallocs the left operand's buffer with exponential growth.
    // Used for augmented concat (result = result + expr) when left is known heap-allocated.
    // Allocates total*2+1 bytes to amortize future appends (O(1) amortized per iteration).
    // The extra capacity is preserved by realloc — strlen still returns the actual length.
    add("str_concat_reuse",
        "define internal ptr @str_concat_reuse(ptr %a, ptr %b) {\n"
        "entry:\n"
        "  %a_null = icmp eq ptr %a, null\n"
        "  br i1 %a_null, label %fallback, label %check_b\n"
        "check_b:\n"
        "  %b_null = icmp eq ptr %b, null\n"
        "  %b_safe = select i1 %b_null, ptr @.str.empty, ptr %b\n"
        "  %len_a = call i64 @strlen(ptr %a)\n"
        "  %len_b = call i64 @strlen(ptr %b_safe)\n"
        "  %total = add i64 %len_a, %len_b\n"
        "  %double = shl i64 %total, 1\n"
        "  %alloc = add i64 %double, 1\n"
        "  %buf = call ptr @mem_realloc(ptr %a, i64 %alloc)\n"
        "  %dst = getelementptr i8, ptr %buf, i64 %len_a\n"
        "  call void @llvm.memcpy.p0.p0.i64(ptr %dst, ptr %b_safe, i64 %len_b, i1 false)\n"
        "  %end = getelementptr i8, ptr %buf, i64 %total\n"
        "  store i8 0, ptr %end\n"
        "  ret ptr %buf\n"
        "fallback:\n"
        "  %b2 = select i1 %b_null, ptr @.str.empty, ptr %b\n"
        "  %len_b2 = call i64 @strlen(ptr %b2)\n"
        "  %alloc2 = add i64 %len_b2, 1\n"
        "  %buf2 = call ptr @mem_alloc(i64 %alloc2)\n"
        "  call void @llvm.memcpy.p0.p0.i64(ptr %buf2, ptr %b2, i64 %len_b2, i1 false)\n"
        "  %end2 = getelementptr i8, ptr %buf2, i64 %len_b2\n"
        "  store i8 0, ptr %end2\n"
        "  ret ptr %buf2\n"
        "}",
        {"strlen", "mem_alloc", "mem_realloc", "llvm.memcpy.p0.p0.i64", ".str.empty"});

    // Black box functions (no dependencies)
    add("black_box_i32", "; Black box (inline IR)\n"
                         "define internal i32 @black_box_i32(i32 %val) noinline {\n"
                         "entry:\n"
                         "  call void asm sideeffect \"\", \"r\"(i32 %val)\n"
                         "  ret i32 %val\n"
                         "}");
    add("black_box_i64", "define internal i64 @black_box_i64(i64 %val) noinline {\n"
                         "entry:\n"
                         "  call void asm sideeffect \"\", \"r\"(i64 %val)\n"
                         "  ret i64 %val\n"
                         "}");
    add("black_box_f64", "define internal double @black_box_f64(double %val) noinline {\n"
                         "entry:\n"
                         "  call void asm sideeffect \"\", \"r\"(double %val)\n"
                         "  ret double %val\n"
                         "}");

    // --- C frontend FFI bridge (phase24 Phase 2.3) ---
    //
    // Declared in compiler/include/cc/cc_bridge.hpp, implemented in
    // compiler/src/cc/cc_bridge.cpp. Every handle type
    // (`CcTokenStream`, `CcTranslationUnit`, `CcMirModule`,
    // `CcDiagnostics`) is an opaque forward-declared struct, so at
    // the ABI level all handles are plain `ptr`s. `CcAbiTarget` is a
    // plain `int32_t` enum — see the header for the encoded values.
    // `CcDiagnostic` is a POD aggregate of two pointers and three
    // int32s (32 bytes with trailing pad); on x86_64 (both SysV and
    // Windows x64) it's returned via an sret slot. LLVM's backend
    // handles the ABI lowering from the direct-aggregate return
    // shape declared here, matching what the C++ side emits.
    //
    // Registering these names in the catalogue means the TML-
    // compiled cc_driver and any future in-process consumer can
    // reference them through `@extern("c")` without the name being
    // dropped as unresolved at link time.
    add("cc_bridge_diagnostics_new",
        "declare dso_local ptr @cc_bridge_diagnostics_new()");
    add("cc_bridge_diagnostics_count",
        "declare dso_local i64 @cc_bridge_diagnostics_count(ptr)");
    add("cc_bridge_diagnostics_get",
        "declare dso_local { ptr, i32, i32, i32, ptr } "
        "@cc_bridge_diagnostics_get(ptr, i64)");
    add("cc_bridge_free_diagnostics",
        "declare dso_local void @cc_bridge_free_diagnostics(ptr)");
    add("cc_bridge_preproc",
        "declare dso_local ptr @cc_bridge_preproc(ptr, ptr, ptr, ptr, ptr)");
    add("cc_bridge_free_token_stream",
        "declare dso_local void @cc_bridge_free_token_stream(ptr)");
    add("cc_bridge_parse",
        "declare dso_local ptr @cc_bridge_parse(ptr, ptr)");
    add("cc_bridge_free_translation_unit",
        "declare dso_local void @cc_bridge_free_translation_unit(ptr)");
    add("cc_bridge_lower",
        "declare dso_local ptr @cc_bridge_lower(ptr, i32, ptr, ptr)");
    add("cc_bridge_free_mir_module",
        "declare dso_local void @cc_bridge_free_mir_module(ptr)");
    add("cc_bridge_mir_borrow",
        "declare dso_local ptr @cc_bridge_mir_borrow(ptr)");

    // --- Format string globals ---
    add(".fmt.int", "@.fmt.int = private constant [4 x i8] c\"%d\\0A\\00\"");
    add(".fmt.int.no_nl", "@.fmt.int.no_nl = private constant [3 x i8] c\"%d\\00\"");
    add(".fmt.i64", "@.fmt.i64 = private constant [5 x i8] c\"%ld\\0A\\00\"");
    add(".fmt.i64.no_nl", "@.fmt.i64.no_nl = private constant [4 x i8] c\"%ld\\00\"");
    add(".fmt.float", "@.fmt.float = private constant [4 x i8] c\"%f\\0A\\00\"");
    add(".fmt.float.no_nl", "@.fmt.float.no_nl = private constant [3 x i8] c\"%f\\00\"");
    add(".fmt.float3", "@.fmt.float3 = private constant [6 x i8] c\"%.3f\\0A\\00\"");
    add(".fmt.float3.no_nl", "@.fmt.float3.no_nl = private constant [5 x i8] c\"%.3f\\00\"");
    add(".fmt.str.no_nl", "@.fmt.str.no_nl = private constant [3 x i8] c\"%s\\00\"");
    add(".str.true", "@.str.true = private constant [5 x i8] c\"true\\00\"");
    add(".str.false", "@.str.false = private constant [6 x i8] c\"false\\00\"");
    add(".str.space", "@.str.space = private constant [2 x i8] c\" \\00\"");
    add(".str.newline", "@.str.newline = private constant [2 x i8] c\"\\0A\\00\"");
}

void LLVMIRGen::require_runtime_decl(const std::string& name) {
    if (needed_runtime_decls_.count(name))
        return; // already marked
    auto it = runtime_catalog_index_.find(name);
    if (it == runtime_catalog_index_.end())
        return; // not a catalog entry
    needed_runtime_decls_.insert(name);
    // Transitively require dependencies
    for (const auto& dep : runtime_catalog_[it->second].deps)
        require_runtime_decl(dep);
}

void LLVMIRGen::scan_for_runtime_refs(const std::string& text) {
    if (runtime_catalog_index_.empty() || needed_runtime_decls_.size() >= runtime_catalog_.size())
        return;
    size_t pos = text.find('@');
    while (pos != std::string::npos) {
        pos++; // skip '@'
        size_t end = pos;
        while (end < text.size() && (std::isalnum(static_cast<unsigned char>(text[end])) ||
                                     text[end] == '_' || text[end] == '.'))
            end++;
        if (end > pos) {
            auto it = runtime_catalog_index_.find(text.substr(pos, end - pos));
            if (it != runtime_catalog_index_.end())
                require_runtime_decl(runtime_catalog_[it->second].name);
        }
        pos = text.find('@', end);
    }
}

void LLVMIRGen::finalize_runtime_decls() {
    // In library_ir_only mode, force ALL declarations (workers may need any of them)
    if (options_.library_ir_only) {
        for (const auto& entry : runtime_catalog_) {
            needed_runtime_decls_.insert(entry.name);
        }
    }

    // Collect functions already declared/defined in the current IR output
    // to avoid emitting duplicate declarations (redefinition errors).
    // This runs in ALL modes (including library_ir_only) because @extern
    // declarations from user code may already have emitted declarations
    // for symbols that the runtime catalog also contains (e.g., strlen, memcmp).
    std::unordered_set<std::string> already_in_ir;
    {
        std::string current_ir = output_.str();
        std::istringstream stream(current_ir);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.find("define ") != std::string::npos ||
                line.find("declare ") != std::string::npos) {
                auto at_pos = line.find('@');
                if (at_pos != std::string::npos) {
                    size_t start = at_pos + 1;
                    size_t end = start;
                    while (end < line.size() &&
                           (std::isalnum(line[end]) || line[end] == '_' || line[end] == '.'))
                        end++;
                    already_in_ir.insert(line.substr(start, end - start));
                }
            }
        }
    }

    std::ostringstream decls;
    decls << "; Runtime declarations (on-demand)\n";
    for (const auto& entry : runtime_catalog_) {
        if (needed_runtime_decls_.count(entry.name) && !already_in_ir.count(entry.name)) {
            decls << entry.ir_text << "\n";
        }
    }
    decls << "\n";
    deferred_runtime_decls_ = decls.str();
}

// ============================================================================
// emit_runtime_decls — emits types + initializes catalog (no function declares)
// ============================================================================

void LLVMIRGen::emit_runtime_decls() {
    // String type: { ptr, i64 } (pointer to data, length)
    emit_line("; Runtime type declarations");
    emit_line("%struct.tml_str = type { ptr, i64 }");

    // Core comparison type (core::cmp)
    // Ordering is a simple enum: Less=0, Equal=1, Greater=2
    emit_line("%struct.Ordering = type { i32 }");
    struct_types_["Ordering"] = "%struct.Ordering";
    struct_fields_["Ordering"] = {{"value", 0, "i32", types::make_i32()}};

    // --- On-demand type declarations (Phase 51) ---
    // Compute which optional categories are needed based on imports.
    // In library_ir_only mode (shared library for test suites), emit everything
    // because the shared lib is linked against multiple workers with varying imports.
    bool needs_sync_atomics = options_.library_ir_only;
    bool needs_logging = options_.library_ir_only;
    bool needs_console = options_.library_ir_only;
    bool needs_collections = options_.library_ir_only;
    bool needs_thread = options_.library_ir_only;

    if (!options_.library_ir_only) {
        const auto& imports = env_.all_imports();
        for (const auto& [name, sym] : imports) {
            const auto& path = sym.module_path;
            if (!needs_sync_atomics &&
                (path.find("std::sync") == 0 || path.find("std::thread") == 0 ||
                 path.find("core::sync") == 0))
                needs_sync_atomics = true;
            if (!needs_logging && path.find("std::log") == 0)
                needs_logging = true;
            if (!needs_console && path.find("std::console") == 0)
                needs_console = true;
            if (!needs_collections && path.find("std::collections") == 0)
                needs_collections = true;
            if (!needs_thread && path.find("std::thread") == 0)
                needs_thread = true;
        }
    }

    // Deferred type declarations (depend on import scan results)
    if (needs_collections) {
        emit_line("%struct.HashMapIter = type { ptr }");
        struct_types_["HashMapIter"] = "%struct.HashMapIter";
        struct_fields_["HashMapIter"] = {{"handle", 0, "ptr", types::make_ptr(types::make_unit())}};
    }
    if (needs_thread || needs_sync_atomics) {
        emit_line("%struct.RawThread = type { i64 }");
        struct_types_["RawThread"] = "%struct.RawThread";
        struct_fields_["RawThread"] = {
            {"_handle", 0, "i64", types::make_primitive(types::PrimitiveKind::U64)}};
        emit_line("%struct.RawPtr = type { i64 }");
        struct_types_["RawPtr"] = "%struct.RawPtr";
        struct_fields_["RawPtr"] = {{"addr", 0, "i64", types::make_i64()}};
    }
    emit_line("");

    // Initialize the runtime declaration catalog
    init_runtime_catalog();

    // Force-require conditional categories based on imports
    if (options_.coverage_enabled) {
        require_runtime_decl("tml_cover_func");
        require_runtime_decl("tml_coverage_write_file");
        declared_externals_.insert("tml_cover_func");
        declared_externals_.insert("tml_coverage_write_file");
    }

    if (options_.emit_debug_info) {
        require_runtime_decl("llvm.dbg.declare");
        require_runtime_decl("llvm.dbg.value");
    }

    if (options_.llvm_source_coverage) {
        require_runtime_decl("llvm.instrprof.increment");
    }

    if (needs_sync_atomics) {
        require_runtime_decl("atomic_fetch_add_i32");
        require_runtime_decl("atomic_fetch_sub_i32");
        require_runtime_decl("atomic_load_i32");
        require_runtime_decl("atomic_store_i32");
        require_runtime_decl("atomic_compare_exchange_i32");
        require_runtime_decl("atomic_swap_i32");
        require_runtime_decl("atomic_fence");
        require_runtime_decl("atomic_fence_acquire");
        require_runtime_decl("atomic_fence_release");
        declared_externals_.insert("atomic_fetch_add_i32");
        declared_externals_.insert("atomic_fetch_sub_i32");
        declared_externals_.insert("atomic_load_i32");
        declared_externals_.insert("atomic_store_i32");
        declared_externals_.insert("atomic_compare_exchange_i32");
        declared_externals_.insert("atomic_swap_i32");
        declared_externals_.insert("atomic_fence");
        declared_externals_.insert("atomic_fence_acquire");
        declared_externals_.insert("atomic_fence_release");
    }

    if (needs_logging) {
        require_runtime_decl("rt_log_msg");
        require_runtime_decl("rt_log_set_level");
        require_runtime_decl("rt_log_get_level");
        require_runtime_decl("rt_log_enabled");
        require_runtime_decl("rt_log_set_filter");
        require_runtime_decl("rt_log_module_enabled");
        require_runtime_decl("rt_log_structured");
        require_runtime_decl("rt_log_set_format");
        require_runtime_decl("rt_log_get_format");
        require_runtime_decl("rt_log_open_file");
        require_runtime_decl("rt_log_close_file");
        require_runtime_decl("rt_log_init_from_env");
        declared_externals_.insert("rt_log_msg");
        declared_externals_.insert("rt_log_set_level");
        declared_externals_.insert("rt_log_get_level");
        declared_externals_.insert("rt_log_enabled");
        declared_externals_.insert("rt_log_set_filter");
        declared_externals_.insert("rt_log_module_enabled");
        declared_externals_.insert("rt_log_structured");
        declared_externals_.insert("rt_log_set_format");
        declared_externals_.insert("rt_log_get_format");
        declared_externals_.insert("rt_log_open_file");
        declared_externals_.insert("rt_log_close_file");
        declared_externals_.insert("rt_log_init_from_env");

        // Register log functions in functions_ map for lowlevel calls
        functions_["rt_log_msg"] =
            FuncInfo{"@rt_log_msg", "void (i32, ptr, ptr)", "void", {"i32", "ptr", "ptr"}};
        functions_["rt_log_set_level"] =
            FuncInfo{"@rt_log_set_level", "void (i32)", "void", {"i32"}};
        functions_["rt_log_get_level"] = FuncInfo{"@rt_log_get_level", "i32 ()", "i32", {}};
        functions_["rt_log_enabled"] = FuncInfo{"@rt_log_enabled", "i32 (i32)", "i32", {"i32"}};
        functions_["rt_log_set_filter"] =
            FuncInfo{"@rt_log_set_filter", "void (ptr)", "void", {"ptr"}};
        functions_["rt_log_module_enabled"] =
            FuncInfo{"@rt_log_module_enabled", "i32 (i32, ptr)", "i32", {"i32", "ptr"}};
        functions_["rt_log_structured"] = FuncInfo{"@rt_log_structured",
                                                   "void (i32, ptr, ptr, ptr)",
                                                   "void",
                                                   {"i32", "ptr", "ptr", "ptr"}};
        functions_["rt_log_set_format"] =
            FuncInfo{"@rt_log_set_format", "void (i32)", "void", {"i32"}};
        functions_["rt_log_get_format"] = FuncInfo{"@rt_log_get_format", "i32 ()", "i32", {}};
        functions_["rt_log_open_file"] = FuncInfo{"@rt_log_open_file", "i32 (ptr)", "i32", {"ptr"}};
        functions_["rt_log_close_file"] = FuncInfo{"@rt_log_close_file", "void ()", "void", {}};
        functions_["rt_log_init_from_env"] = FuncInfo{"@rt_log_init_from_env", "i32 ()", "i32", {}};
    }

    if (needs_console) {
        require_runtime_decl("rt_console_timer_start");
        require_runtime_decl("rt_console_timer_end");
        require_runtime_decl("rt_console_count_inc");
        require_runtime_decl("rt_console_count_reset");
        require_runtime_decl("rt_console_indent_inc");
        require_runtime_decl("rt_console_indent_dec");
        require_runtime_decl("rt_console_indent_get");
        declared_externals_.insert("rt_console_timer_start");
        declared_externals_.insert("rt_console_timer_end");
        declared_externals_.insert("rt_console_count_inc");
        declared_externals_.insert("rt_console_count_reset");
        declared_externals_.insert("rt_console_indent_inc");
        declared_externals_.insert("rt_console_indent_dec");
        declared_externals_.insert("rt_console_indent_get");

        // Register console functions in functions_ map for lowlevel calls
        functions_["rt_console_timer_start"] =
            FuncInfo{"@rt_console_timer_start", "i64 (ptr, i64)", "i64", {"ptr", "i64"}};
        functions_["rt_console_timer_end"] =
            FuncInfo{"@rt_console_timer_end", "i64 (ptr, i64)", "i64", {"ptr", "i64"}};
        functions_["rt_console_count_inc"] =
            FuncInfo{"@rt_console_count_inc", "i64 (ptr)", "i64", {"ptr"}};
        functions_["rt_console_count_reset"] =
            FuncInfo{"@rt_console_count_reset", "i64 (ptr)", "i64", {"ptr"}};
        functions_["rt_console_indent_inc"] =
            FuncInfo{"@rt_console_indent_inc", "i64 ()", "i64", {}};
        functions_["rt_console_indent_dec"] =
            FuncInfo{"@rt_console_indent_dec", "i64 ()", "i64", {}};
        functions_["rt_console_indent_get"] =
            FuncInfo{"@rt_console_indent_get", "i64 ()", "i64", {}};
    }

    // Register functions_ map entries for lowlevel calls (lightweight, no IR emitted)
    functions_["random_seed"] = FuncInfo{"@tml_random_seed", "i64 ()", "i64", {}};
    functions_["tml_random_seed"] = FuncInfo{"@tml_random_seed", "i64 ()", "i64", {}};
    functions_["print_str"] = FuncInfo{"@print", "void (ptr)", "void", {"ptr"}};
    functions_["println_str"] = FuncInfo{"@println", "void (ptr)", "void", {"ptr"}};
    functions_["f64_to_string"] = FuncInfo{"@f64_to_string", "ptr (double)", "ptr", {"double"}};
    functions_["f32_to_string"] = FuncInfo{"@f32_to_string", "ptr (float)", "ptr", {"float"}};
    functions_["f64_to_string_precision"] =
        FuncInfo{"@f64_to_string_precision", "ptr (double, i64)", "ptr", {"double", "i64"}};
    functions_["f32_to_string_precision"] =
        FuncInfo{"@f32_to_string_precision", "ptr (float, i64)", "ptr", {"float", "i64"}};
    functions_["f64_to_exp_string"] =
        FuncInfo{"@f64_to_exp_string", "ptr (double, i32)", "ptr", {"double", "i32"}};
    functions_["f32_to_exp_string"] =
        FuncInfo{"@f32_to_exp_string", "ptr (float, i32)", "ptr", {"float", "i32"}};

    // Placeholder for deferred declarations (will be replaced in generate())
    emit_line("; {{RUNTIME_DECLS_PLACEHOLDER}}");
    emit_line("");
}

// emit_module_lowlevel_decls, emit_module_pure_tml_functions, and
// emit_string_constants are in runtime_modules.cpp

} // namespace tml::codegen
