// MIR Builder - Control Flow Implementation
//
// This file contains functions for building control flow constructs:
// if/else, loops, when (pattern matching), break/continue/return.

#include "mir/mir_builder.hpp"

namespace tml::mir {

auto MirBuilder::build_if(const parser::IfExpr& if_expr) -> Value {
    auto cond = build_expr(*if_expr.condition);

    auto then_block = create_block("if_then");
    auto else_block = if_expr.else_branch.has_value() ? create_block("if_else") : 0;
    auto merge_block = create_block("if_merge");

    emit_cond_branch(cond, then_block, else_block ? else_block : merge_block);

    // Then branch
    switch_to_block(then_block);
    auto then_val = build_expr(*if_expr.then_branch);
    auto then_end = ctx_.current_block;
    if (!is_terminated()) {
        emit_branch(merge_block);
    }

    // Else branch
    Value else_val = const_unit();
    uint32_t else_end = 0;
    if (if_expr.else_branch.has_value()) {
        switch_to_block(else_block);
        else_val = build_expr(**if_expr.else_branch);
        else_end = ctx_.current_block;
        if (!is_terminated()) {
            emit_branch(merge_block);
        }
    }

    // Merge block
    switch_to_block(merge_block);

    // If both branches return a value, create phi
    if (if_expr.else_branch.has_value() && !then_val.type->is_unit()) {
        PhiInst phi;
        phi.incoming = {{then_val, then_end}, {else_val, else_end}};
        return emit(std::move(phi), then_val.type);
    }

    return const_unit();
}

auto MirBuilder::build_block(const parser::BlockExpr& block) -> Value {
    for (const auto& stmt : block.stmts) {
        build_stmt(*stmt);
        if (is_terminated())
            return const_unit();
    }

    if (block.expr.has_value()) {
        return build_expr(**block.expr);
    }

    return const_unit();
}

auto MirBuilder::build_loop(const parser::LoopExpr& loop) -> Value {
    auto header = create_block("loop_header");
    auto body = create_block("loop_body");
    auto exit = create_block("loop_exit");

    emit_branch(header);

    // Header just jumps to body (infinite loop)
    switch_to_block(header);
    emit_branch(body);

    // Push loop context
    ctx_.loop_stack.push({header, exit, std::nullopt});

    // Body
    switch_to_block(body);
    build_expr(*loop.body);
    if (!is_terminated()) {
        emit_branch(header);
    }

    ctx_.loop_stack.pop();

    switch_to_block(exit);
    return const_unit();
}

auto MirBuilder::build_while(const parser::WhileExpr& while_expr) -> Value {
    auto header = create_block("while_header");
    auto body = create_block("while_body");
    auto exit = create_block("while_exit");

    emit_branch(header);

    // Header evaluates condition
    switch_to_block(header);
    auto cond = build_expr(*while_expr.condition);
    emit_cond_branch(cond, body, exit);

    // Push loop context
    ctx_.loop_stack.push({header, exit, std::nullopt});

    // Body
    switch_to_block(body);
    build_expr(*while_expr.body);
    if (!is_terminated()) {
        emit_branch(header);
    }

    ctx_.loop_stack.pop();

    switch_to_block(exit);
    return const_unit();
}

auto MirBuilder::build_for(const parser::ForExpr& for_expr) -> Value {
    // For loop desugaring:
    // for pattern in iterable { body }
    //
    // Becomes:
    // let mut iter = iterable.into_iter();
    // loop {
    //     match iter.next() {
    //         Just(pattern) => { body }
    //         Nothing => break
    //     }
    // }

    // Build the iterable expression
    auto iterable = build_expr(*for_expr.iter);

    // Create blocks for the loop structure
    auto header = create_block("for_header");
    auto body = create_block("for_body");
    auto exit = create_block("for_exit");

    // Call into_iter on the iterable (or assume it's already an iterator)
    // For now, we assume the iterable is an array or range and use direct indexing

    // Allocate index variable
    auto index_alloca =
        emit(AllocaInst{make_i32_type(), "_for_idx"}, make_pointer_type(make_i32_type(), true));
    emit_void(StoreInst{index_alloca, const_int(0, 32, true)});

    emit_branch(header);

    // Header: check if index < length
    switch_to_block(header);
    auto index_val = emit(LoadInst{index_alloca}, make_i32_type());

    // Get length of iterable (assume array type has known size, or call len())
    Value length;
    if (auto* array_type = std::get_if<MirArrayType>(&iterable.type->kind)) {
        length = const_int(static_cast<int64_t>(array_type->size), 32, true);
    } else {
        // Call len() method for other types
        MethodCallInst len_call;
        len_call.receiver = iterable;
        len_call.method_name = "len";
        len_call.return_type = make_i32_type();
        length = emit(std::move(len_call), make_i32_type());
    }

    // index < length
    BinaryInst cmp;
    cmp.op = BinOp::Lt;
    cmp.left = index_val;
    cmp.right = length;
    auto cond = emit(std::move(cmp), make_bool_type());
    emit_cond_branch(cond, body, exit);

    // Push loop context
    ctx_.loop_stack.push({header, exit, std::nullopt});

    // Body: extract element, bind to pattern, execute body
    switch_to_block(body);

    // Get element at current index
    MirTypePtr element_type = make_i32_type();
    if (auto* array_type = std::get_if<MirArrayType>(&iterable.type->kind)) {
        element_type = array_type->element;
    } else if (auto* slice_type = std::get_if<MirSliceType>(&iterable.type->kind)) {
        element_type = slice_type->element;
    }

    GetElementPtrInst gep;
    gep.base = iterable;
    gep.indices = {index_val};
    gep.base_type = iterable.type;
    gep.result_type = make_pointer_type(element_type, false);
    auto elem_ptr = emit(std::move(gep), gep.result_type);

    LoadInst load;
    load.ptr = elem_ptr;
    load.result_type = element_type;
    auto element = emit(std::move(load), element_type);

    // Bind element to pattern
    build_pattern_binding(*for_expr.pattern, element);

    // Execute body
    build_expr(*for_expr.body);

    // Increment index
    if (!is_terminated()) {
        auto new_index_val = emit(LoadInst{index_alloca}, make_i32_type());
        BinaryInst inc;
        inc.op = BinOp::Add;
        inc.left = new_index_val;
        inc.right = const_int(1, 32, true);
        auto incremented = emit(std::move(inc), make_i32_type());
        emit_void(StoreInst{index_alloca, incremented});
        emit_branch(header);
    }

    ctx_.loop_stack.pop();

    switch_to_block(exit);
    return const_unit();
}

auto MirBuilder::build_return(const parser::ReturnExpr& ret) -> Value {
    if (ret.value.has_value()) {
        auto val = build_expr(**ret.value);
        emit_return(val);
    } else {
        emit_return();
    }
    return const_unit();
}

auto MirBuilder::build_break(const parser::BreakExpr& brk) -> Value {
    if (ctx_.loop_stack.empty())
        return const_unit();

    auto& loop = ctx_.loop_stack.top();

    if (brk.value.has_value()) {
        auto val = build_expr(**brk.value);
        loop.break_value = val;
    }

    emit_branch(loop.exit_block);
    return const_unit();
}

auto MirBuilder::build_continue(const parser::ContinueExpr& /*cont*/) -> Value {
    if (ctx_.loop_stack.empty())
        return const_unit();

    auto& loop = ctx_.loop_stack.top();
    emit_branch(loop.header_block);
    return const_unit();
}

auto MirBuilder::build_when(const parser::WhenExpr& when) -> Value {
    // Pattern matching implementation
    // when scrutinee {
    //     pattern1 => expr1,
    //     pattern2 => expr2,
    //     _ => default_expr,
    // }

    auto scrutinee = build_expr(*when.scrutinee);
    auto merge_block = create_block("when_merge");

    // Collect all arm results for phi node
    std::vector<std::pair<Value, uint32_t>> arm_results;
    MirTypePtr result_type = make_unit_type();

    // For each arm, create a test block and body block
    std::vector<uint32_t> test_blocks;
    std::vector<uint32_t> body_blocks;

    for (size_t i = 0; i < when.arms.size(); ++i) {
        test_blocks.push_back(create_block("when_test_" + std::to_string(i)));
        body_blocks.push_back(create_block("when_body_" + std::to_string(i)));
    }

    // Jump to first test
    emit_branch(test_blocks[0]);

    for (size_t i = 0; i < when.arms.size(); ++i) {
        const auto& arm = when.arms[i];
        uint32_t next_test = (i + 1 < when.arms.size()) ? test_blocks[i + 1] : merge_block;

        // Test block: check if pattern matches
        switch_to_block(test_blocks[i]);

        // Handle different pattern types
        bool is_wildcard = arm.pattern->is<parser::WildcardPattern>();
        bool is_ident = arm.pattern->is<parser::IdentPattern>();

        if (is_wildcard || is_ident) {
            // Wildcard or simple identifier always matches
            emit_branch(body_blocks[i]);
        } else if (arm.pattern->is<parser::LiteralPattern>()) {
            // Literal pattern: compare with scrutinee
            const auto& lit_pat = arm.pattern->as<parser::LiteralPattern>();
            auto lit_val = build_literal(parser::LiteralExpr{lit_pat.literal, lit_pat.span});

            BinaryInst cmp;
            cmp.op = BinOp::Eq;
            cmp.left = scrutinee;
            cmp.right = lit_val;
            auto match = emit(std::move(cmp), make_bool_type());

            emit_cond_branch(match, body_blocks[i], next_test);
        } else if (arm.pattern->is<parser::EnumPattern>()) {
            // Enum pattern: check discriminant
            const auto& enum_pat = arm.pattern->as<parser::EnumPattern>();
            std::string variant_name =
                enum_pat.path.segments.empty() ? "" : enum_pat.path.segments.back();

            // Get discriminant from scrutinee (first field of enum)
            ExtractValueInst extract_disc;
            extract_disc.aggregate = scrutinee;
            extract_disc.indices = {0}; // Discriminant is at index 0
            auto disc = emit(std::move(extract_disc), make_i32_type());

            // Look up expected discriminant value from enum definition
            int expected_disc = 0;
            if (auto* enum_type = std::get_if<MirEnumType>(&scrutinee.type->kind)) {
                if (auto enum_def = env_.lookup_enum(enum_type->name)) {
                    for (size_t vi = 0; vi < enum_def->variants.size(); ++vi) {
                        if (enum_def->variants[vi].first == variant_name) {
                            expected_disc = static_cast<int>(vi);
                            break;
                        }
                    }
                }
            }

            BinaryInst cmp;
            cmp.op = BinOp::Eq;
            cmp.left = disc;
            cmp.right = const_int(expected_disc, 32, true);
            auto match = emit(std::move(cmp), make_bool_type());

            emit_cond_branch(match, body_blocks[i], next_test);
        } else {
            // Other patterns: just jump to body for now
            emit_branch(body_blocks[i]);
        }

        // Body block: bind pattern variables, evaluate guard if present, execute body
        switch_to_block(body_blocks[i]);

        // Bind pattern variables
        build_pattern_binding(*arm.pattern, scrutinee);

        // Check guard if present
        if (arm.guard.has_value()) {
            auto guard_val = build_expr(**arm.guard);
            auto guard_pass = create_block("when_guard_pass_" + std::to_string(i));
            emit_cond_branch(guard_val, guard_pass, next_test);
            switch_to_block(guard_pass);
        }

        // Execute body
        auto body_val = build_expr(*arm.body);
        auto body_end_block = ctx_.current_block;

        if (!is_terminated()) {
            if (i == 0) {
                result_type = body_val.type;
            }
            arm_results.push_back({body_val, body_end_block});
            emit_branch(merge_block);
        }
    }

    // Merge block
    switch_to_block(merge_block);

    // Create phi node if we have results
    if (!arm_results.empty() && !result_type->is_unit()) {
        PhiInst phi;
        phi.incoming = arm_results;
        phi.result_type = result_type;
        return emit(std::move(phi), result_type);
    }

    return const_unit();
}

} // namespace tml::mir
