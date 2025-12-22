#include "tml/ir/ir.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <functional>

namespace tml::ir {

IRBuilder::IRBuilder() = default;

// Simple hash function for stable ID generation
static auto simple_hash(const std::string& input) -> std::string {
    // Simple FNV-1a hash for demonstration
    // In production, use SHA-256
    uint64_t hash = 14695981039346656037ULL;
    for (char c : input) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(8) << (hash & 0xFFFFFFFF);
    return oss.str();
}

auto IRBuilder::generate_id(const std::string& name, const std::string& signature) -> StableId {
    std::string input = current_module_ + "::" + name + "::" + signature;
    return "@" + simple_hash(input);
}

auto IRBuilder::visibility_from_ast(parser::Visibility vis) -> Visibility {
    return vis == parser::Visibility::Public ? Visibility::Public : Visibility::Private;
}

auto IRBuilder::binary_op_to_string(parser::BinaryOp op) -> std::string {
    switch (op) {
        case parser::BinaryOp::Add: return "+";
        case parser::BinaryOp::Sub: return "-";
        case parser::BinaryOp::Mul: return "*";
        case parser::BinaryOp::Div: return "/";
        case parser::BinaryOp::Mod: return "%";
        case parser::BinaryOp::Eq: return "==";
        case parser::BinaryOp::Ne: return "!=";
        case parser::BinaryOp::Lt: return "<";
        case parser::BinaryOp::Le: return "<=";
        case parser::BinaryOp::Gt: return ">";
        case parser::BinaryOp::Ge: return ">=";
        case parser::BinaryOp::And: return "and";
        case parser::BinaryOp::Or: return "or";
        case parser::BinaryOp::BitAnd: return "&";
        case parser::BinaryOp::BitOr: return "|";
        case parser::BinaryOp::BitXor: return "^";
        case parser::BinaryOp::Shl: return "<<";
        case parser::BinaryOp::Shr: return ">>";
        case parser::BinaryOp::Assign: return "=";
        case parser::BinaryOp::AddAssign: return "+=";
        case parser::BinaryOp::SubAssign: return "-=";
        case parser::BinaryOp::MulAssign: return "*=";
        case parser::BinaryOp::DivAssign: return "/=";
        case parser::BinaryOp::ModAssign: return "%=";
        case parser::BinaryOp::BitAndAssign: return "&=";
        case parser::BinaryOp::BitOrAssign: return "|=";
        case parser::BinaryOp::BitXorAssign: return "^=";
        case parser::BinaryOp::ShlAssign: return ">>=";
        default: return "?";
    }
}

auto IRBuilder::unary_op_to_string(parser::UnaryOp op) -> std::string {
    switch (op) {
        case parser::UnaryOp::Neg: return "-";
        case parser::UnaryOp::Not: return "not";
        case parser::UnaryOp::BitNot: return "~";
        case parser::UnaryOp::Ref: return "ref";
        case parser::UnaryOp::RefMut: return "ref-mut";
        case parser::UnaryOp::Deref: return "deref";
        default: return "?";
    }
}

auto IRBuilder::build_module(const parser::Module& module, const std::string& module_name) -> IRModule {
    current_module_ = module_name;

    IRModule ir_module;
    ir_module.id = generate_id(module_name, "module");
    ir_module.name = module_name;

    // Build all declarations
    std::vector<IRConst> consts;
    std::vector<IRType> types;
    std::vector<IRBehavior> behaviors;
    std::vector<IRImpl> impls;
    std::vector<IRFunc> funcs;

    for (const auto& decl : module.decls) {
        std::visit([&](const auto& d) {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, parser::FuncDecl>) {
                funcs.push_back(build_func(d));
            } else if constexpr (std::is_same_v<T, parser::StructDecl>) {
                types.push_back(build_struct(d));
            } else if constexpr (std::is_same_v<T, parser::EnumDecl>) {
                types.push_back(build_enum(d));
            } else if constexpr (std::is_same_v<T, parser::TraitDecl>) {
                behaviors.push_back(build_trait(d));
            } else if constexpr (std::is_same_v<T, parser::ImplDecl>) {
                impls.push_back(build_impl(d));
            } else if constexpr (std::is_same_v<T, parser::ConstDecl>) {
                consts.push_back(build_const(d));
            }
            // Other declarations handled as needed
        }, decl->kind);
    }

    // Sort items by kind, then alphabetically by name
    auto sort_by_name = [](auto& vec) {
        std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
            return a.name < b.name;
        });
    };

    sort_by_name(consts);
    sort_by_name(types);
    sort_by_name(behaviors);
    sort_by_name(funcs);
    // impls don't have names, sort by target type
    std::sort(impls.begin(), impls.end(), [](const auto& a, const auto& b) {
        return a.target_type < b.target_type;
    });

    // Add items in canonical order: const, type, behavior, impl, func
    for (auto& c : consts) ir_module.items.push_back(std::move(c));
    for (auto& t : types) ir_module.items.push_back(std::move(t));
    for (auto& b : behaviors) ir_module.items.push_back(std::move(b));
    for (auto& i : impls) ir_module.items.push_back(std::move(i));
    for (auto& f : funcs) ir_module.items.push_back(std::move(f));

    return ir_module;
}

auto IRBuilder::build_func(const parser::FuncDecl& func) -> IRFunc {
    IRFunc ir_func;
    ir_func.name = func.name;
    ir_func.vis = visibility_from_ast(func.vis);

    // Build signature for ID generation
    std::string sig;
    if (!func.params.empty()) {
        sig = "(";
        for (size_t i = 0; i < func.params.size(); ++i) {
            if (i > 0) sig += ",";
            // Simplified type representation
            sig += "param";
        }
        sig += ")";
    }
    if (func.return_type) {
        sig += "->ret";
    }
    ir_func.id = generate_id(func.name, sig);

    // Build generics
    for (const auto& gen : func.generics) {
        IRGenericParam param;
        param.name = gen.name;
        for (const auto& bound : gen.bounds) {
            // Extract bound name from TypePath
            if (!bound.segments.empty()) {
                param.bounds.push_back(bound.segments[0]);
            }
        }
        ir_func.generics.push_back(param);
    }

    // Build parameters
    for (const auto& p : func.params) {
        IRParam param;
        if (p.pattern->is<parser::IdentPattern>()) {
            param.name = p.pattern->as<parser::IdentPattern>().name;
        } else {
            param.name = "_";
        }
        param.type = build_type_expr(*p.type);
        ir_func.params.push_back(std::move(param));
    }

    // Return type
    if (func.return_type) {
        ir_func.return_type = build_type_expr(**func.return_type);
    }

    // Body
    if (func.body) {
        ir_func.body = build_block(*func.body);
    }

    return ir_func;
}

auto IRBuilder::build_struct(const parser::StructDecl& st) -> IRType {
    IRType ir_type;
    ir_type.name = st.name;
    ir_type.vis = visibility_from_ast(st.vis);
    ir_type.id = generate_id(st.name, "type");

    // Build generics
    for (const auto& gen : st.generics) {
        IRGenericParam param;
        param.name = gen.name;
        ir_type.generics.push_back(param);
    }

    // Build fields (sorted alphabetically)
    IRStructType struct_type;
    std::vector<IRField> fields;
    for (const auto& f : st.fields) {
        IRField field;
        field.name = f.name;
        field.type = build_type_expr(*f.type);
        field.vis = visibility_from_ast(f.vis);
        fields.push_back(std::move(field));
    }
    std::sort(fields.begin(), fields.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });
    struct_type.fields = std::move(fields);
    ir_type.kind = std::move(struct_type);

    return ir_type;
}

auto IRBuilder::build_enum(const parser::EnumDecl& en) -> IRType {
    IRType ir_type;
    ir_type.name = en.name;
    ir_type.vis = visibility_from_ast(en.vis);
    ir_type.id = generate_id(en.name, "enum");

    // Build generics
    for (const auto& gen : en.generics) {
        IRGenericParam param;
        param.name = gen.name;
        ir_type.generics.push_back(param);
    }

    // Build variants (sorted alphabetically)
    IREnumType enum_type;
    std::vector<IREnumVariant> variants;
    for (const auto& v : en.variants) {
        IREnumVariant variant;
        variant.name = v.name;
        if (v.tuple_fields) {
            for (const auto& field : *v.tuple_fields) {
                variant.fields.push_back(build_type_expr(*field));
            }
        }
        variants.push_back(std::move(variant));
    }
    std::sort(variants.begin(), variants.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });
    enum_type.variants = std::move(variants);
    ir_type.kind = std::move(enum_type);

    return ir_type;
}

auto IRBuilder::build_trait(const parser::TraitDecl& trait) -> IRBehavior {
    IRBehavior ir_behavior;
    ir_behavior.name = trait.name;
    ir_behavior.vis = visibility_from_ast(trait.vis);
    ir_behavior.id = generate_id(trait.name, "behavior");

    // Build generics
    for (const auto& gen : trait.generics) {
        IRGenericParam param;
        param.name = gen.name;
        ir_behavior.generics.push_back(param);
    }

    // Build super behaviors
    for (const auto& super : trait.super_traits) {
        if (!super.segments.empty()) {
            ir_behavior.super_behaviors.push_back(super.segments[0]);
        }
    }

    // Build methods (sorted alphabetically)
    std::vector<IRBehaviorMethod> methods;
    for (const auto& m : trait.methods) {
        IRBehaviorMethod method;
        method.name = m.name;
        for (const auto& p : m.params) {
            IRParam param;
            if (p.pattern->is<parser::IdentPattern>()) {
                param.name = p.pattern->as<parser::IdentPattern>().name;
            } else {
                param.name = "_";
            }
            param.type = build_type_expr(*p.type);
            method.params.push_back(std::move(param));
        }
        if (m.return_type) {
            method.return_type = build_type_expr(**m.return_type);
        }
        if (m.body) {
            method.default_impl = build_block(*m.body);
        }
        methods.push_back(std::move(method));
    }
    std::sort(methods.begin(), methods.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });
    ir_behavior.methods = std::move(methods);

    return ir_behavior;
}

auto IRBuilder::build_impl(const parser::ImplDecl& impl) -> IRImpl {
    IRImpl ir_impl;
    ir_impl.id = generate_id("impl", std::to_string(next_seq_++));

    // Build generics
    for (const auto& gen : impl.generics) {
        IRGenericParam param;
        param.name = gen.name;
        ir_impl.generics.push_back(param);
    }

    // Target type
    if (impl.self_type) {
        // Extract type name
        if (std::holds_alternative<parser::NamedType>(impl.self_type->kind)) {
            const auto& named = std::get<parser::NamedType>(impl.self_type->kind);
            if (!named.path.segments.empty()) {
                ir_impl.target_type = named.path.segments[0];
            }
        }
    }

    // Behavior being implemented (if any)
    if (impl.trait_path) {
        if (!impl.trait_path->segments.empty()) {
            ir_impl.behavior = impl.trait_path->segments[0];
        }
    }

    // Build methods (sorted alphabetically)
    std::vector<IRImplMethod> methods;
    for (const auto& m : impl.methods) {
        IRImplMethod method;
        method.name = m.name;
        method.id = generate_id(ir_impl.target_type + "::" + m.name, "method");

        for (const auto& p : m.params) {
            IRParam param;
            if (p.pattern->is<parser::IdentPattern>()) {
                param.name = p.pattern->as<parser::IdentPattern>().name;
            } else {
                param.name = "_";
            }
            param.type = build_type_expr(*p.type);
            method.params.push_back(std::move(param));
        }
        if (m.return_type) {
            method.return_type = build_type_expr(**m.return_type);
        }
        if (m.body) {
            method.body = build_block(*m.body);
        }
        methods.push_back(std::move(method));
    }
    std::sort(methods.begin(), methods.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });
    ir_impl.methods = std::move(methods);

    return ir_impl;
}

auto IRBuilder::build_const(const parser::ConstDecl& cst) -> IRConst {
    IRConst ir_const;
    ir_const.name = cst.name;
    ir_const.vis = visibility_from_ast(cst.vis);
    ir_const.id = generate_id(cst.name, "const");
    ir_const.type = build_type_expr(*cst.type);
    ir_const.value = build_expr(*cst.value);
    return ir_const;
}

auto IRBuilder::build_expr(const parser::Expr& expr) -> IRExprPtr {
    return std::visit([this](const auto& e) -> IRExprPtr {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, parser::LiteralExpr>) {
            IRLiteral lit;
            std::visit([&lit](const auto& val) {
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
            }, e.token.value);
            return make_box<IRExpr>(IRExpr{lit});
        }
        else if constexpr (std::is_same_v<T, parser::IdentExpr>) {
            return make_box<IRExpr>(IRExpr{IRVar{e.name}});
        }
        else if constexpr (std::is_same_v<T, parser::BinaryExpr>) {
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
                    case parser::BinaryOp::AddAssign: base_op = parser::BinaryOp::Add; break;
                    case parser::BinaryOp::SubAssign: base_op = parser::BinaryOp::Sub; break;
                    case parser::BinaryOp::MulAssign: base_op = parser::BinaryOp::Mul; break;
                    case parser::BinaryOp::DivAssign: base_op = parser::BinaryOp::Div; break;
                    case parser::BinaryOp::ModAssign: base_op = parser::BinaryOp::Mod; break;
                    default: base_op = parser::BinaryOp::Add; break;
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
        }
        else if constexpr (std::is_same_v<T, parser::UnaryExpr>) {
            IRUnaryOp unop;
            unop.op = unary_op_to_string(e.op);
            unop.operand = build_expr(*e.operand);
            return make_box<IRExpr>(IRExpr{std::move(unop)});
        }
        else if constexpr (std::is_same_v<T, parser::CallExpr>) {
            IRCall call;
            // Extract function name from callee
            if (e.callee->is<parser::IdentExpr>()) {
                call.func_name = e.callee->as<parser::IdentExpr>().name;
            } else if (e.callee->is<parser::PathExpr>()) {
                const auto& path = e.callee->as<parser::PathExpr>();
                for (size_t i = 0; i < path.path.segments.size(); ++i) {
                    if (i > 0) call.func_name += "::";
                    call.func_name += path.path.segments[i];
                }
            } else {
                call.func_name = "_unknown";
            }
            for (const auto& arg : e.args) {
                call.args.push_back(build_expr(*arg));
            }
            return make_box<IRExpr>(IRExpr{std::move(call)});
        }
        else if constexpr (std::is_same_v<T, parser::MethodCallExpr>) {
            IRMethodCall call;
            call.receiver = build_expr(*e.receiver);
            call.method_name = e.method;
            for (const auto& arg : e.args) {
                call.args.push_back(build_expr(*arg));
            }
            return make_box<IRExpr>(IRExpr{std::move(call)});
        }
        else if constexpr (std::is_same_v<T, parser::FieldExpr>) {
            IRFieldGet field;
            field.object = build_expr(*e.object);
            field.field_name = e.field;
            return make_box<IRExpr>(IRExpr{std::move(field)});
        }
        else if constexpr (std::is_same_v<T, parser::IndexExpr>) {
            IRIndex idx;
            idx.object = build_expr(*e.object);
            idx.index = build_expr(*e.index);
            return make_box<IRExpr>(IRExpr{std::move(idx)});
        }
        else if constexpr (std::is_same_v<T, parser::TupleExpr>) {
            IRTupleExpr tuple;
            for (const auto& elem : e.elements) {
                tuple.elements.push_back(build_expr(*elem));
            }
            return make_box<IRExpr>(IRExpr{std::move(tuple)});
        }
        else if constexpr (std::is_same_v<T, parser::ArrayExpr>) {
            return std::visit([this](const auto& arr) -> IRExprPtr {
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
            }, e.kind);
        }
        else if constexpr (std::is_same_v<T, parser::StructExpr>) {
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
        }
        else if constexpr (std::is_same_v<T, parser::BlockExpr>) {
            return make_box<IRExpr>(IRExpr{build_block(e)});
        }
        else if constexpr (std::is_same_v<T, parser::IfExpr>) {
            IRIf if_expr;
            if_expr.condition = build_expr(*e.condition);
            if_expr.then_branch = build_expr(*e.then_branch);
            if (e.else_branch) {
                if_expr.else_branch = build_expr(**e.else_branch);
            }
            return make_box<IRExpr>(IRExpr{std::move(if_expr)});
        }
        else if constexpr (std::is_same_v<T, parser::WhenExpr>) {
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
        }
        else if constexpr (std::is_same_v<T, parser::LoopExpr>) {
            // Infinite loop
            IRLoop loop;
            loop.body = build_expr(*e.body);
            return make_box<IRExpr>(IRExpr{std::move(loop)});
        }
        else if constexpr (std::is_same_v<T, parser::WhileExpr>) {
            IRLoopWhile loop;
            loop.condition = build_expr(*e.condition);
            loop.body = build_expr(*e.body);
            return make_box<IRExpr>(IRExpr{std::move(loop)});
        }
        else if constexpr (std::is_same_v<T, parser::ForExpr>) {
            IRLoopIn loop;
            if (e.pattern->is<parser::IdentPattern>()) {
                loop.binding = e.pattern->as<parser::IdentPattern>().name;
            } else {
                loop.binding = "_";
            }
            loop.iter = build_expr(*e.iter);
            loop.body = build_expr(*e.body);
            return make_box<IRExpr>(IRExpr{std::move(loop)});
        }
        else if constexpr (std::is_same_v<T, parser::ReturnExpr>) {
            IRReturn ret;
            if (e.value) {
                ret.value = build_expr(**e.value);
            }
            return make_box<IRExpr>(IRExpr{std::move(ret)});
        }
        else if constexpr (std::is_same_v<T, parser::BreakExpr>) {
            IRBreak brk;
            if (e.value) {
                brk.value = build_expr(**e.value);
            }
            return make_box<IRExpr>(IRExpr{std::move(brk)});
        }
        else if constexpr (std::is_same_v<T, parser::ContinueExpr>) {
            return make_box<IRExpr>(IRExpr{IRContinue{}});
        }
        else if constexpr (std::is_same_v<T, parser::ClosureExpr>) {
            IRClosure closure;
            for (const auto& [pattern, type] : e.params) {
                std::string name = "_";
                if (pattern->is<parser::IdentPattern>()) {
                    name = pattern->as<parser::IdentPattern>().name;
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
        }
        else if constexpr (std::is_same_v<T, parser::TryExpr>) {
            IRTry try_expr;
            try_expr.expr = build_expr(*e.expr);
            return make_box<IRExpr>(IRExpr{std::move(try_expr)});
        }
        else if constexpr (std::is_same_v<T, parser::PathExpr>) {
            // Path expression treated as variable reference
            std::string name;
            for (size_t i = 0; i < e.path.segments.size(); ++i) {
                if (i > 0) name += "::";
                name += e.path.segments[i];
            }
            return make_box<IRExpr>(IRExpr{IRVar{name}});
        }
        else if constexpr (std::is_same_v<T, parser::RangeExpr>) {
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
        }
        else if constexpr (std::is_same_v<T, parser::CastExpr>) {
            // Cast expression: convert to IR representation
            IRCall cast_call;
            cast_call.func_name = "as";
            cast_call.args.push_back(build_expr(*e.expr));
            return make_box<IRExpr>(IRExpr{std::move(cast_call)});
        }
        else if constexpr (std::is_same_v<T, parser::AwaitExpr>) {
            // Await expression
            IRCall await_call;
            await_call.func_name = "await";
            await_call.args.push_back(build_expr(*e.expr));
            return make_box<IRExpr>(IRExpr{std::move(await_call)});
        }
        else {
            // Default: empty literal
            return make_box<IRExpr>(IRExpr{IRLiteral{"()", "Unit"}});
        }
    }, expr.kind);
}

auto IRBuilder::build_block(const parser::BlockExpr& block) -> IRBlock {
    IRBlock ir_block;
    for (const auto& stmt : block.stmts) {
        ir_block.stmts.push_back(build_stmt(*stmt));
    }
    if (block.expr) {
        ir_block.expr = build_expr(**block.expr);
    }
    return ir_block;
}

auto IRBuilder::build_stmt(const parser::Stmt& stmt) -> IRStmtPtr {
    return std::visit([this](const auto& s) -> IRStmtPtr {
        using T = std::decay_t<decltype(s)>;

        if constexpr (std::is_same_v<T, parser::LetStmt>) {
            IRLet let;
            let.pattern = build_pattern(*s.pattern);
            if (s.type_annotation) {
                let.type_annotation = build_type_expr(**s.type_annotation);
            }
            if (s.init) {
                let.init = build_expr(**s.init);
            } else {
                // No initializer - use unit
                let.init = make_box<IRExpr>(IRExpr{IRLiteral{"()", "Unit"}});
            }
            return make_box<IRStmt>(IRStmt{std::move(let)});
        }
        else if constexpr (std::is_same_v<T, parser::VarStmt>) {
            IRVarMut var;
            var.name = s.name;
            if (s.type_annotation) {
                var.type_annotation = build_type_expr(**s.type_annotation);
            }
            var.init = build_expr(*s.init);
            return make_box<IRStmt>(IRStmt{std::move(var)});
        }
        else if constexpr (std::is_same_v<T, parser::ExprStmt>) {
            // Check if this is an assignment expression
            if (s.expr->is<parser::BinaryExpr>()) {
                const auto& bin = s.expr->as<parser::BinaryExpr>();
                if (bin.op == parser::BinaryOp::Assign) {
                    IRAssign assign;
                    assign.target = build_expr(*bin.left);
                    assign.value = build_expr(*bin.right);
                    return make_box<IRStmt>(IRStmt{std::move(assign)});
                }
            }
            IRExprStmt expr_stmt;
            expr_stmt.expr = build_expr(*s.expr);
            return make_box<IRStmt>(IRStmt{std::move(expr_stmt)});
        }
        else if constexpr (std::is_same_v<T, parser::DeclPtr>) {
            // Nested declaration - convert to expression statement
            IRExprStmt expr_stmt;
            expr_stmt.expr = make_box<IRExpr>(IRExpr{IRLiteral{"()", "Unit"}});
            return make_box<IRStmt>(IRStmt{std::move(expr_stmt)});
        }
        else {
            // Default: empty expression statement
            IRExprStmt expr_stmt;
            expr_stmt.expr = make_box<IRExpr>(IRExpr{IRLiteral{"()", "Unit"}});
            return make_box<IRStmt>(IRStmt{std::move(expr_stmt)});
        }
    }, stmt.kind);
}

auto IRBuilder::build_pattern(const parser::Pattern& pattern) -> IRPatternPtr {
    return std::visit([this](const auto& p) -> IRPatternPtr {
        using T = std::decay_t<decltype(p)>;

        if constexpr (std::is_same_v<T, parser::LiteralPattern>) {
            IRPatternLit lit;
            std::visit([&lit](const auto& val) {
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
            }, p.literal.value);
            return make_box<IRPattern>(IRPattern{std::move(lit)});
        }
        else if constexpr (std::is_same_v<T, parser::IdentPattern>) {
            IRPatternBind bind;
            bind.name = p.name;
            bind.is_mut = p.is_mut;
            return make_box<IRPattern>(IRPattern{std::move(bind)});
        }
        else if constexpr (std::is_same_v<T, parser::WildcardPattern>) {
            return make_box<IRPattern>(IRPattern{IRPatternWild{}});
        }
        else if constexpr (std::is_same_v<T, parser::TuplePattern>) {
            IRPatternTuple tuple;
            for (const auto& elem : p.elements) {
                tuple.elements.push_back(build_pattern(*elem));
            }
            return make_box<IRPattern>(IRPattern{std::move(tuple)});
        }
        else if constexpr (std::is_same_v<T, parser::StructPattern>) {
            IRPatternStruct struct_pat;
            if (!p.path.segments.empty()) {
                struct_pat.type_name = p.path.segments.back();
            }
            for (const auto& [name, pat] : p.fields) {
                struct_pat.fields.push_back({name, build_pattern(*pat)});
            }
            return make_box<IRPattern>(IRPattern{std::move(struct_pat)});
        }
        else if constexpr (std::is_same_v<T, parser::EnumPattern>) {
            IRPatternVariant variant;
            if (!p.path.segments.empty()) {
                variant.variant_name = p.path.segments.back();
            }
            if (p.payload) {
                for (const auto& field : *p.payload) {
                    variant.fields.push_back(build_pattern(*field));
                }
            }
            return make_box<IRPattern>(IRPattern{std::move(variant)});
        }
        else {
            // Default: wildcard
            return make_box<IRPattern>(IRPattern{IRPatternWild{}});
        }
    }, pattern.kind);
}

auto IRBuilder::build_type_expr(const parser::Type& type) -> IRTypeExpr {
    return std::visit([this](const auto& t) -> IRTypeExpr {
        using T = std::decay_t<decltype(t)>;

        if constexpr (std::is_same_v<T, parser::NamedType>) {
            IRTypeRef ref;
            for (const auto& seg : t.path.segments) {
                if (ref.name.empty()) {
                    ref.name = seg;
                } else {
                    ref.name += "::" + seg;
                }
            }
            if (t.generics) {
                for (const auto& arg : t.generics->args) {
                    auto inner = build_type_expr(*arg);
                    ref.type_args.push_back(make_box<IRTypeRef>(
                        std::move(std::get<IRTypeRef>(inner.kind))));
                }
            }
            return IRTypeExpr{std::move(ref)};
        }
        else if constexpr (std::is_same_v<T, parser::RefType>) {
            IRRefType ref;
            ref.is_mut = t.is_mut;
            auto inner = build_type_expr(*t.inner);
            ref.inner = make_box<IRTypeRef>(std::move(std::get<IRTypeRef>(inner.kind)));
            return IRTypeExpr{std::move(ref)};
        }
        else if constexpr (std::is_same_v<T, parser::SliceType>) {
            IRSliceType slice;
            auto inner = build_type_expr(*t.element);
            slice.element = make_box<IRTypeRef>(std::move(std::get<IRTypeRef>(inner.kind)));
            return IRTypeExpr{std::move(slice)};
        }
        else if constexpr (std::is_same_v<T, parser::ArrayType>) {
            IRArrayType arr;
            auto inner = build_type_expr(*t.element);
            arr.element = make_box<IRTypeRef>(std::move(std::get<IRTypeRef>(inner.kind)));
            // Try to extract literal size from the expression
            if (t.size && t.size->is<parser::LiteralExpr>()) {
                const auto& lit = t.size->as<parser::LiteralExpr>();
                if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
                    arr.size = static_cast<size_t>(std::get<lexer::IntValue>(lit.token.value).value);
                } else {
                    arr.size = 0; // Unknown size
                }
            } else {
                arr.size = 0; // Dynamic or unknown size
            }
            return IRTypeExpr{std::move(arr)};
        }
        else if constexpr (std::is_same_v<T, parser::TupleType>) {
            IRTupleType tuple;
            for (const auto& elem : t.elements) {
                auto inner = build_type_expr(*elem);
                tuple.elements.push_back(make_box<IRTypeRef>(
                    std::move(std::get<IRTypeRef>(inner.kind))));
            }
            return IRTypeExpr{std::move(tuple)};
        }
        else if constexpr (std::is_same_v<T, parser::FuncType>) {
            IRFuncType func;
            for (const auto& param : t.params) {
                auto inner = build_type_expr(*param);
                func.params.push_back(make_box<IRTypeRef>(
                    std::move(std::get<IRTypeRef>(inner.kind))));
            }
            auto ret = build_type_expr(*t.return_type);
            func.ret = make_box<IRTypeRef>(std::move(std::get<IRTypeRef>(ret.kind)));
            return IRTypeExpr{std::move(func)};
        }
        else {
            // Default: unit type
            return IRTypeExpr{IRTypeRef{"Unit", {}}};
        }
    }, type.kind);
}

} // namespace tml::ir
