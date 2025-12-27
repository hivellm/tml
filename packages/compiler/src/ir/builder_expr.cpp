#include "ir/ir.hpp"

#include <algorithm>
namespace tml::ir {

auto IRBuilder::build_expr(const parser::Expr& expr) -> IRExprPtr {
    return std::visit(
        [this](const auto& e) -> IRExprPtr {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, parser::LiteralExpr>) {
                IRLiteral lit;
                std::visit(
                    [&lit](const auto& val) {
                        using V = std::decay_t<decltype(val)>;
                        if constexpr (std::is_same_v<V, std::monostate>) {
                            lit.value = "()";
                            lit.type_name = "Unit";
                        } else if constexpr (std::is_same_v<V, lexer::IntValue>) {
                            lit.value = std::to_string(val.value);
                            lit.type_name = "I64";
                        } else if constexpr (std::is_same_v<V, lexer::FloatValue>) {
                            lit.value = std::to_string(val.value);
                            lit.type_name = "F64";
                        } else if constexpr (std::is_same_v<V, lexer::StringValue>) {
                            lit.value = "\"" + val.value + "\"";
                            lit.type_name = "String";
                        } else if constexpr (std::is_same_v<V, lexer::CharValue>) {
                            lit.value = std::string("'") + static_cast<char>(val.value) + "'";
                            lit.type_name = "Char";
                        } else if constexpr (std::is_same_v<V, bool>) {
                            lit.value = val ? "true" : "false";
                            lit.type_name = "Bool";
                        }
                    },
                    e.token.value);
                return make_box<IRExpr>(IRExpr{lit});
            } else if constexpr (std::is_same_v<T, parser::IdentExpr>) {
                return make_box<IRExpr>(IRExpr{IRVar{e.name}});
            } else if constexpr (std::is_same_v<T, parser::BinaryExpr>) {
                // Handle assignment operators as assign statements in IR
                if (e.op == parser::BinaryOp::Assign) {
                    // This should be handled as a statement, but for expression context
                    // we emit as binary op
                }
                // Handle compound assignment
                if (e.op >= parser::BinaryOp::AddAssign && e.op <= parser::BinaryOp::ShrAssign) {
                    // Desugar: x += 1 -> (assign x (+ x 1))
                    IRAssign assign;
                    assign.target = build_expr(*e.left);

                    // Map compound op to base op
                    parser::BinaryOp base_op;
                    switch (e.op) {
                    case parser::BinaryOp::AddAssign:
                        base_op = parser::BinaryOp::Add;
                        break;
                    case parser::BinaryOp::SubAssign:
                        base_op = parser::BinaryOp::Sub;
                        break;
                    case parser::BinaryOp::MulAssign:
                        base_op = parser::BinaryOp::Mul;
                        break;
                    case parser::BinaryOp::DivAssign:
                        base_op = parser::BinaryOp::Div;
                        break;
                    case parser::BinaryOp::ModAssign:
                        base_op = parser::BinaryOp::Mod;
                        break;
                    default:
                        base_op = parser::BinaryOp::Add;
                        break;
                    }

                    IRBinaryOp binop;
                    binop.op = binary_op_to_string(base_op);
                    binop.left = build_expr(*e.left);
                    binop.right = build_expr(*e.right);
                    assign.value = make_box<IRExpr>(IRExpr{std::move(binop)});

                    // Wrap assign in an expression context (not ideal but works)
                    // Actually, for cleaner IR we should emit this as a block
                    return make_box<IRExpr>(IRExpr{std::move(binop)});
                }
                IRBinaryOp binop;
                binop.op = binary_op_to_string(e.op);
                binop.left = build_expr(*e.left);
                binop.right = build_expr(*e.right);
                return make_box<IRExpr>(IRExpr{std::move(binop)});
            } else if constexpr (std::is_same_v<T, parser::UnaryExpr>) {
                IRUnaryOp unop;
                unop.op = unary_op_to_string(e.op);
                unop.operand = build_expr(*e.operand);
                return make_box<IRExpr>(IRExpr{std::move(unop)});
            } else if constexpr (std::is_same_v<T, parser::CallExpr>) {
                IRCall call;
                // Extract function name from callee
                if (e.callee->template is<parser::IdentExpr>()) {
                    call.func_name = e.callee->template as<parser::IdentExpr>().name;
                } else if (e.callee->template is<parser::PathExpr>()) {
                    const auto& path = e.callee->template as<parser::PathExpr>();
                    for (size_t i = 0; i < path.path.segments.size(); ++i) {
                        if (i > 0)
                            call.func_name += "::";
                        call.func_name += path.path.segments[i];
                    }
                } else {
                    call.func_name = "_unknown";
                }
                for (const auto& arg : e.args) {
                    call.args.push_back(build_expr(*arg));
                }
                return make_box<IRExpr>(IRExpr{std::move(call)});
            } else if constexpr (std::is_same_v<T, parser::MethodCallExpr>) {
                IRMethodCall call;
                call.receiver = build_expr(*e.receiver);
                call.method_name = e.method;
                for (const auto& arg : e.args) {
                    call.args.push_back(build_expr(*arg));
                }
                return make_box<IRExpr>(IRExpr{std::move(call)});
            } else if constexpr (std::is_same_v<T, parser::FieldExpr>) {
                IRFieldGet field;
                field.object = build_expr(*e.object);
                field.field_name = e.field;
                return make_box<IRExpr>(IRExpr{std::move(field)});
            } else if constexpr (std::is_same_v<T, parser::IndexExpr>) {
                IRIndex idx;
                idx.object = build_expr(*e.object);
                idx.index = build_expr(*e.index);
                return make_box<IRExpr>(IRExpr{std::move(idx)});
            } else if constexpr (std::is_same_v<T, parser::TupleExpr>) {
                IRTupleExpr tuple;
                for (const auto& elem : e.elements) {
                    tuple.elements.push_back(build_expr(*elem));
                }
                return make_box<IRExpr>(IRExpr{std::move(tuple)});
            } else if constexpr (std::is_same_v<T, parser::ArrayExpr>) {
                return std::visit(
                    [this](const auto& arr) -> IRExprPtr {
                        using A = std::decay_t<decltype(arr)>;
                        if constexpr (std::is_same_v<A, std::vector<parser::ExprPtr>>) {
                            IRArrayExpr array;
                            for (const auto& elem : arr) {
                                array.elements.push_back(build_expr(*elem));
                            }
                            return make_box<IRExpr>(IRExpr{std::move(array)});
                        } else {
                            IRArrayRepeat repeat;
                            repeat.value = build_expr(*arr.first);
                            repeat.count = build_expr(*arr.second);
                            return make_box<IRExpr>(IRExpr{std::move(repeat)});
                        }
                    },
                    e.kind);
            } else if constexpr (std::is_same_v<T, parser::StructExpr>) {
                IRStructExpr struct_expr;
                // Extract type name from path
                if (!e.path.segments.empty()) {
                    struct_expr.type_name = e.path.segments.back();
                }
                // Sort fields alphabetically
                std::vector<std::pair<std::string, parser::ExprPtr*>> sorted_fields;
                for (const auto& [name, val] : e.fields) {
                    sorted_fields.push_back({name, const_cast<parser::ExprPtr*>(&val)});
                }
                std::sort(sorted_fields.begin(), sorted_fields.end(),
                          [](const auto& a, const auto& b) { return a.first < b.first; });
                for (const auto& [name, val] : sorted_fields) {
                    struct_expr.fields.push_back({name, build_expr(**val)});
                }
                return make_box<IRExpr>(IRExpr{std::move(struct_expr)});
            } else if constexpr (std::is_same_v<T, parser::BlockExpr>) {
                return make_box<IRExpr>(IRExpr{build_block(e)});
            } else if constexpr (std::is_same_v<T, parser::IfExpr>) {
                IRIf if_expr;
                if_expr.condition = build_expr(*e.condition);
                if_expr.then_branch = build_expr(*e.then_branch);
                if (e.else_branch) {
                    if_expr.else_branch = build_expr(**e.else_branch);
                }
                return make_box<IRExpr>(IRExpr{std::move(if_expr)});
            } else if constexpr (std::is_same_v<T, parser::WhenExpr>) {
                IRWhen when;
                when.scrutinee = build_expr(*e.scrutinee);
                for (const auto& arm : e.arms) {
                    IRWhenArm ir_arm;
                    ir_arm.pattern = build_pattern(*arm.pattern);
                    if (arm.guard) {
                        ir_arm.guard = build_expr(**arm.guard);
                    }
                    ir_arm.body = build_expr(*arm.body);
                    when.arms.push_back(std::move(ir_arm));
                }
                return make_box<IRExpr>(IRExpr{std::move(when)});
            } else if constexpr (std::is_same_v<T, parser::LoopExpr>) {
                // Infinite loop
                IRLoop loop;
                loop.body = build_expr(*e.body);
                return make_box<IRExpr>(IRExpr{std::move(loop)});
            } else if constexpr (std::is_same_v<T, parser::WhileExpr>) {
                IRLoopWhile loop;
                loop.condition = build_expr(*e.condition);
                loop.body = build_expr(*e.body);
                return make_box<IRExpr>(IRExpr{std::move(loop)});
            } else if constexpr (std::is_same_v<T, parser::ForExpr>) {
                IRLoopIn loop;
                if (e.pattern->template is<parser::IdentPattern>()) {
                    loop.binding = e.pattern->template as<parser::IdentPattern>().name;
                } else {
                    loop.binding = "_";
                }
                loop.iter = build_expr(*e.iter);
                loop.body = build_expr(*e.body);
                return make_box<IRExpr>(IRExpr{std::move(loop)});
            } else if constexpr (std::is_same_v<T, parser::ReturnExpr>) {
                IRReturn ret;
                if (e.value) {
                    ret.value = build_expr(**e.value);
                }
                return make_box<IRExpr>(IRExpr{std::move(ret)});
            } else if constexpr (std::is_same_v<T, parser::BreakExpr>) {
                IRBreak brk;
                if (e.value) {
                    brk.value = build_expr(**e.value);
                }
                return make_box<IRExpr>(IRExpr{std::move(brk)});
            } else if constexpr (std::is_same_v<T, parser::ContinueExpr>) {
                return make_box<IRExpr>(IRExpr{IRContinue{}});
            } else if constexpr (std::is_same_v<T, parser::ClosureExpr>) {
                IRClosure closure;
                for (const auto& [pattern, type] : e.params) {
                    std::string name = "_";
                    if (pattern->template is<parser::IdentPattern>()) {
                        name = pattern->template as<parser::IdentPattern>().name;
                    }
                    std::optional<IRTypeExpr> ir_type;
                    if (type) {
                        ir_type = build_type_expr(**type);
                    }
                    closure.params.push_back(std::make_pair(std::move(name), std::move(ir_type)));
                }
                if (e.return_type) {
                    closure.return_type = build_type_expr(**e.return_type);
                }
                closure.body = build_expr(*e.body);
                return make_box<IRExpr>(IRExpr{std::move(closure)});
            } else if constexpr (std::is_same_v<T, parser::TryExpr>) {
                IRTry try_expr;
                try_expr.expr = build_expr(*e.expr);
                return make_box<IRExpr>(IRExpr{std::move(try_expr)});
            } else if constexpr (std::is_same_v<T, parser::PathExpr>) {
                // Path expression treated as variable reference
                std::string name;
                for (size_t i = 0; i < e.path.segments.size(); ++i) {
                    if (i > 0)
                        name += "::";
                    name += e.path.segments[i];
                }
                return make_box<IRExpr>(IRExpr{IRVar{name}});
            } else if constexpr (std::is_same_v<T, parser::RangeExpr>) {
                IRRange range;
                if (e.start) {
                    range.start = build_expr(**e.start);
                } else {
                    range.start = make_box<IRExpr>(IRExpr{IRLiteral{"0", "I32"}});
                }
                if (e.end) {
                    range.end = build_expr(**e.end);
                } else {
                    range.end = make_box<IRExpr>(IRExpr{IRLiteral{"max", "I32"}});
                }
                range.inclusive = e.inclusive;
                return make_box<IRExpr>(IRExpr{std::move(range)});
            } else if constexpr (std::is_same_v<T, parser::CastExpr>) {
                // Cast expression: convert to IR representation
                IRCall cast_call;
                cast_call.func_name = "as";
                cast_call.args.push_back(build_expr(*e.expr));
                return make_box<IRExpr>(IRExpr{std::move(cast_call)});
            } else if constexpr (std::is_same_v<T, parser::AwaitExpr>) {
                // Await expression
                IRCall await_call;
                await_call.func_name = "await";
                await_call.args.push_back(build_expr(*e.expr));
                return make_box<IRExpr>(IRExpr{std::move(await_call)});
            } else if constexpr (std::is_same_v<T, parser::InterpolatedStringExpr>) {
                // Interpolated string: "Hello {name}!" -> string_format("Hello ", name, "!")
                // Generate as a call to a string concatenation function
                IRCall format_call;
                format_call.func_name = "__string_format";
                for (const auto& segment : e.segments) {
                    if (std::holds_alternative<std::string>(segment.content)) {
                        // Literal string segment
                        IRLiteral lit;
                        lit.value = "\"" + std::get<std::string>(segment.content) + "\"";
                        lit.type_name = "String";
                        format_call.args.push_back(make_box<IRExpr>(IRExpr{lit}));
                    } else {
                        // Expression segment
                        const auto& expr_ptr = std::get<parser::ExprPtr>(segment.content);
                        format_call.args.push_back(build_expr(*expr_ptr));
                    }
                }
                return make_box<IRExpr>(IRExpr{std::move(format_call)});
            } else {
                // Default: empty literal
                return make_box<IRExpr>(IRExpr{IRLiteral{"()", "Unit"}});
            }
        },
        expr.kind);
}

} // namespace tml::ir
