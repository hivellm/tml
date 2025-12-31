// LLVM IR generator - Await expression generation
// Handles: async/await for Future[T] values
//
// Note: Full async/await requires a runtime with cooperative scheduling.
// For now, this implementation generates synchronous code that executes
// the awaited expression directly. This allows async code to compile and
// run correctly for single-threaded scenarios.
//
// In a future implementation, this would:
// 1. Convert async functions to state machines
// 2. Generate poll() calls to check Future readiness
// 3. Yield to a scheduler when futures are pending

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_await(const parser::AwaitExpr& await_expr) -> std::string {
    // For now, we generate the awaited expression directly.
    // In an async function, `some_async_call().await` will:
    // 1. Call the async function (which returns immediately with Future[T])
    // 2. Extract the value from the Future
    //
    // Since we don't have a full async runtime yet, we:
    // - Execute the inner expression synchronously
    // - Return its result directly
    //
    // This works because async functions currently compile to regular functions
    // that return their value directly (not wrapped in Future[T]).

    std::string result = gen_expr(*await_expr.expr);

    // The awaited expression's type is already the "unwrapped" Future output type
    // (since async functions return T, not Future[T], in our current simplified model)

    return result;
}

} // namespace tml::codegen
