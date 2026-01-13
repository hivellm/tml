//! # LLVM IR Generator - Class OOP Support
//!
//! This file implements C#-style OOP class support with virtual dispatch.
//!
//! ## Class Layout
//!
//! Each class instance contains:
//! - Vtable pointer (first field, implicit)
//! - Fields from base class (inherited, in declaration order)
//! - Fields declared in this class
//!
//! ```llvm
//! %class.Dog = type { ptr, %class.Animal, i32 }  ; vtable, base, age
//! ```
//!
//! ## Vtable Structure
//!
//! Each class has a vtable containing:
//! - Destructor pointer (slot 0)
//! - Virtual methods in declaration order
//!
//! ```llvm
//! @vtable.Dog = global { ptr, ptr, ptr } { ptr @Dog_destructor, ptr @Dog_speak, ptr @Dog_walk }
//! ```
//!
//! ## Virtual Dispatch
//!
//! Virtual method calls:
//! 1. Load vtable pointer from object (field 0)
//! 2. Load function pointer from vtable slot
//! 3. Call function with object as first arg
//!
//! ## Key Methods
//!
//! | Method                 | Purpose                                |
//! |------------------------|----------------------------------------|
//! | `gen_class_decl`       | Generate class type and vtable         |
//! | `gen_class_vtable`     | Generate vtable global constant        |
//! | `gen_class_constructor`| Generate constructor function          |
//! | `gen_virtual_call`     | Generate virtual method dispatch       |

#include "codegen/llvm_ir_gen.hpp"
#include "lexer/token.hpp"
#include "types/env.hpp"
#include "types/type.hpp"

namespace tml::codegen {

// Helper to extract name from FuncParam pattern
static std::string get_class_param_name(const parser::FuncParam& param) {
    if (param.pattern && param.pattern->is<parser::IdentPattern>()) {
        return param.pattern->as<parser::IdentPattern>().name;
    }
    return "_anon";
}

// ============================================================================
// Class Type Generation
// ============================================================================

void LLVMIRGen::gen_class_decl(const parser::ClassDecl& c) {
    // Skip if already emitted
    if (class_types_.find(c.name) != class_types_.end()) {
        return;
    }

    // Generate LLVM type name
    std::string type_name = "%class." + c.name;

    // Collect field types
    // Class layout: { vtable_ptr, base_class_fields..., own_fields... }
    std::vector<std::string> field_types;
    field_types.push_back("ptr"); // Vtable pointer is always first

    // If class extends another, include base class as embedded struct
    if (c.extends) {
        std::string base_name = c.extends->segments.back();
        // Make sure base class type is generated first
        auto base_class = env_.lookup_class(base_name);
        if (base_class) {
            // Base class fields are embedded (excluding vtable since we have our own)
            // For simplicity, include base as embedded struct
            field_types.push_back("%class." + base_name);
        }
    }

    // Add own instance fields (non-static)
    std::vector<ClassFieldInfo> field_info;
    size_t field_offset = field_types.size(); // Start after vtable and base

    for (const auto& field : c.fields) {
        if (field.is_static)
            continue; // Static fields are globals, not in instance

        std::string ft = llvm_type_ptr(field.type);
        if (ft == "void")
            ft = "{}"; // Unit type in struct
        field_types.push_back(ft);

        field_info.push_back({field.name, static_cast<int>(field_offset++), ft, field.vis});
    }

    // Emit class type definition
    std::string def = type_name + " = type { ";
    for (size_t i = 0; i < field_types.size(); ++i) {
        if (i > 0)
            def += ", ";
        def += field_types[i];
    }
    def += " }";
    emit_line(def);

    // Register class type
    class_types_[c.name] = type_name;
    class_fields_[c.name] = field_info;

    // Generate static fields as global variables
    for (const auto& field : c.fields) {
        if (!field.is_static)
            continue;

        std::string field_type = llvm_type_ptr(field.type);
        if (field_type == "void")
            field_type = "{}";

        std::string global_name = "@class." + c.name + "." + field.name;

        // Generate initial value
        std::string init_value = "zeroinitializer";

        // Check if field has an explicit initializer
        if (field.init.has_value()) {
            const auto& init_expr = *field.init.value();
            if (init_expr.is<parser::LiteralExpr>()) {
                const auto& lit = init_expr.as<parser::LiteralExpr>();
                // Handle integer literals
                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                    init_value = std::to_string(lit.token.int_value().value);
                }
                // Handle float literals
                else if (lit.token.kind == lexer::TokenKind::FloatLiteral) {
                    init_value = std::to_string(lit.token.float_value().value);
                }
                // Handle bool literals
                else if (lit.token.kind == lexer::TokenKind::BoolLiteral) {
                    init_value = lit.token.bool_value() ? "true" : "false";
                }
                // Handle string literals
                else if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                    // For strings, we need to emit a global string constant
                    // and initialize with ptr to it
                    init_value = "null"; // Strings need runtime initialization
                }
            }
            // Handle unary expressions (e.g., -5)
            else if (init_expr.is<parser::UnaryExpr>()) {
                const auto& unary = init_expr.as<parser::UnaryExpr>();
                if (unary.op == parser::UnaryOp::Neg && unary.operand->is<parser::LiteralExpr>()) {
                    const auto& lit = unary.operand->as<parser::LiteralExpr>();
                    if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                        // Cast to signed before negation to avoid unsigned overflow warning
                        init_value =
                            std::to_string(-static_cast<int64_t>(lit.token.int_value().value));
                    } else if (lit.token.kind == lexer::TokenKind::FloatLiteral) {
                        init_value = std::to_string(-lit.token.float_value().value);
                    }
                }
            }
            // Handle binary constant expressions (e.g., 1 + 2)
            else if (init_expr.is<parser::BinaryExpr>()) {
                const auto& bin = init_expr.as<parser::BinaryExpr>();
                // Only handle simple constant folding for now
                if (bin.left->is<parser::LiteralExpr>() && bin.right->is<parser::LiteralExpr>()) {
                    const auto& left = bin.left->as<parser::LiteralExpr>();
                    const auto& right = bin.right->as<parser::LiteralExpr>();
                    if (left.token.kind == lexer::TokenKind::IntLiteral &&
                        right.token.kind == lexer::TokenKind::IntLiteral) {
                        int64_t lv = left.token.int_value().value;
                        int64_t rv = right.token.int_value().value;
                        int64_t result = 0;
                        switch (bin.op) {
                        case parser::BinaryOp::Add:
                            result = lv + rv;
                            break;
                        case parser::BinaryOp::Sub:
                            result = lv - rv;
                            break;
                        case parser::BinaryOp::Mul:
                            result = lv * rv;
                            break;
                        case parser::BinaryOp::Div:
                            result = rv != 0 ? lv / rv : 0;
                            break;
                        case parser::BinaryOp::Mod:
                            result = rv != 0 ? lv % rv : 0;
                            break;
                        case parser::BinaryOp::BitAnd:
                            result = lv & rv;
                            break;
                        case parser::BinaryOp::BitOr:
                            result = lv | rv;
                            break;
                        case parser::BinaryOp::BitXor:
                            result = lv ^ rv;
                            break;
                        case parser::BinaryOp::Shl:
                            result = lv << rv;
                            break;
                        case parser::BinaryOp::Shr:
                            result = lv >> rv;
                            break;
                        default:
                            break;
                        }
                        init_value = std::to_string(result);
                    }
                }
            }
        } else {
            // Generate default value based on type
            if (field_type == "i64" || field_type == "i32" || field_type == "i16" ||
                field_type == "i8") {
                init_value = "0";
            } else if (field_type == "double" || field_type == "float") {
                init_value = "0.0";
            } else if (field_type == "i1") {
                init_value = "false";
            }
        }

        emit_line(global_name + " = global " + field_type + " " + init_value);

        // Register static field for access
        static_fields_[c.name + "." + field.name] = {global_name, field_type};
    }

    // Generate vtable
    gen_class_vtable(c);

    // Generate constructors
    for (const auto& ctor : c.constructors) {
        gen_class_constructor(c, ctor);
    }

    // Generate methods
    for (const auto& method : c.methods) {
        gen_class_method(c, method);
    }
}

// ============================================================================
// Vtable Generation
// ============================================================================

void LLVMIRGen::gen_class_vtable(const parser::ClassDecl& c) {
    // Collect all virtual methods (inherited + own)
    std::vector<VirtualMethodInfo> vtable_methods;

    // First, inherit virtual methods from base class
    if (c.extends) {
        std::string base_name = c.extends->segments.back();
        auto it = class_vtable_layout_.find(base_name);
        if (it != class_vtable_layout_.end()) {
            vtable_methods = it->second;
        }
    }

    // Process own methods - add new virtuals or override existing
    for (const auto& method : c.methods) {
        if (!method.is_static) {
            bool found = false;

            // Check if this overrides a base method
            if (method.is_override) {
                for (auto& vm : vtable_methods) {
                    if (vm.name == method.name) {
                        // Override: update implementation
                        vm.impl_class = c.name;
                        found = true;
                        break;
                    }
                }
            }

            // Add new virtual method
            if (!found && (method.is_virtual || method.is_abstract)) {
                VirtualMethodInfo vmi;
                vmi.name = method.name;
                vmi.declaring_class = c.name;
                vmi.impl_class = method.is_abstract ? "" : c.name;
                vmi.vtable_index = vtable_methods.size();
                vtable_methods.push_back(vmi);
            }
        }
    }

    // Store vtable layout
    class_vtable_layout_[c.name] = vtable_methods;

    // Emit vtable type
    std::string vtable_type_name = "%vtable." + c.name;
    std::string vtable_type = vtable_type_name + " = type { ";
    for (size_t i = 0; i < vtable_methods.size(); ++i) {
        if (i > 0)
            vtable_type += ", ";
        vtable_type += "ptr"; // All method pointers
    }
    if (vtable_methods.empty()) {
        vtable_type += "ptr"; // At least one slot for type info
    }
    vtable_type += " }";
    emit_line(vtable_type);

    // Don't emit vtable global for abstract classes
    if (c.is_abstract) {
        return;
    }

    // Emit vtable global constant
    std::string vtable_name = "@vtable." + c.name;
    std::string vtable_value = "{ ";

    for (size_t i = 0; i < vtable_methods.size(); ++i) {
        if (i > 0)
            vtable_value += ", ";
        const auto& vm = vtable_methods[i];

        if (vm.impl_class.empty()) {
            // Abstract method - should not happen for non-abstract class
            vtable_value += "ptr null";
        } else {
            vtable_value += "ptr @tml_" + get_suite_prefix() + vm.impl_class + "_" + vm.name;
        }
    }

    if (vtable_methods.empty()) {
        vtable_value += "ptr null"; // Placeholder
    }
    vtable_value += " }";

    emit_line(vtable_name + " = internal constant " + vtable_type_name + " " + vtable_value);
}

// ============================================================================
// Constructor Generation
// ============================================================================

void LLVMIRGen::gen_class_constructor(const parser::ClassDecl& c,
                                      const parser::ConstructorDecl& ctor) {
    std::string func_name = "@tml_" + get_suite_prefix() + c.name + "_new";
    std::string class_type = "%class." + c.name;

    // Build parameter list
    std::vector<std::string> param_types;
    std::vector<std::string> param_names;

    for (const auto& param : ctor.params) {
        param_types.push_back(llvm_type_ptr(param.type));
        param_names.push_back(get_class_param_name(param));
    }

    // Function signature - returns class pointer
    std::string sig = "define " + class_type + "* " + func_name + "(";
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (i > 0)
            sig += ", ";
        sig += param_types[i] + " %" + param_names[i];
    }
    sig += ")";
    emit_line(sig + " {");
    emit_line("entry:");

    // Allocate object
    std::string obj = fresh_reg();
    emit_line("  " + obj + " = call ptr @malloc(i64 ptrtoint (" + class_type + "* getelementptr (" +
              class_type + ", " + class_type + "* null, i32 1) to i64))");

    // Initialize vtable pointer (field 0)
    std::string vtable_ptr = fresh_reg();
    emit_line("  " + vtable_ptr + " = getelementptr " + class_type + ", ptr " + obj +
              ", i32 0, i32 0");
    emit_line("  store ptr @vtable." + c.name + ", ptr " + vtable_ptr);

    // Call base constructor if specified
    if (ctor.base_args && c.extends) {
        std::string base_name = c.extends->segments.back();
        std::string base_ctor_name = "@tml_" + get_suite_prefix() + base_name + "_new";

        // Generate arguments for base constructor
        std::vector<std::string> base_args;
        std::vector<std::string> base_arg_types;
        for (const auto& arg : *ctor.base_args) {
            base_args.push_back(gen_expr(*arg));
            // Use the type from gen_expr which sets last_expr_type_
            base_arg_types.push_back(last_expr_type_.empty() ? "i64" : last_expr_type_);
        }

        // Call base constructor
        std::string base_result = fresh_reg();
        std::string call = "  " + base_result + " = call ptr " + base_ctor_name + "(";
        for (size_t i = 0; i < base_args.size(); ++i) {
            if (i > 0)
                call += ", ";
            call += base_arg_types[i] + " " + base_args[i];
        }
        call += ")";
        emit_line(call);

        // Copy base object to embedded base field (field 1)
        // The base constructor returns a pointer to a new base object
        // We need to copy its contents into our embedded base
        std::string base_field_ptr = fresh_reg();
        emit_line("  " + base_field_ptr + " = getelementptr " + class_type + ", ptr " + obj +
                  ", i32 0, i32 1");

        // Copy base vtable pointer
        std::string base_vtable_ptr = fresh_reg();
        emit_line("  " + base_vtable_ptr + " = load ptr, ptr " + base_result);
        emit_line("  store ptr " + base_vtable_ptr + ", ptr " + base_field_ptr);

        // Note: Field copying for base class fields would need to be added here
        // For now we just initialize the vtable pointer of embedded base
    }

    // Generate constructor body
    if (ctor.body) {
        // Set up 'this' reference
        locals_["this"] = VarInfo{obj, class_type + "*", nullptr, std::nullopt};

        // Set up constructor parameters in locals
        for (size_t i = 0; i < param_names.size(); ++i) {
            locals_[param_names[i]] =
                VarInfo{"%" + param_names[i], param_types[i], nullptr, std::nullopt};
        }

        // Generate body statements
        for (const auto& stmt : ctor.body->stmts) {
            gen_stmt(*stmt);
        }

        // Generate trailing expression (if any - common in blocks)
        if (ctor.body->expr.has_value()) {
            gen_expr(*ctor.body->expr.value());
        }

        locals_.erase("this");
    }

    // Return the object
    emit_line("  ret " + class_type + "* " + obj);
    emit_line("}");
    emit_line("");
}

// ============================================================================
// Method Generation
// ============================================================================

void LLVMIRGen::gen_class_method(const parser::ClassDecl& c, const parser::ClassMethod& method) {
    if (method.is_abstract) {
        // Abstract methods have no body
        return;
    }

    std::string func_name = "@tml_" + get_suite_prefix() + c.name + "_" + method.name;
    std::string class_type = "%class." + c.name;

    // Build parameter list - first param is always 'this' for instance methods
    std::vector<std::string> param_types;
    std::vector<std::string> param_names;

    if (!method.is_static) {
        param_types.push_back("ptr"); // this pointer
        param_names.push_back("this");
    }

    for (const auto& param : method.params) {
        // Skip 'this' parameter - it's already added above for non-static methods
        std::string pname = get_class_param_name(param);
        if (pname == "this") {
            continue;
        }
        param_types.push_back(llvm_type_ptr(param.type));
        param_names.push_back(pname);
    }

    // Return type
    std::string ret_type = "void";
    if (method.return_type) {
        ret_type = llvm_type_ptr(*method.return_type);
    }

    // Function signature
    std::string sig = "define " + ret_type + " " + func_name + "(";
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (i > 0)
            sig += ", ";
        sig += param_types[i] + " %" + param_names[i];
    }
    sig += ")";
    emit_line(sig + " {");
    emit_line("entry:");

    // Set up locals for parameters
    if (!method.is_static) {
        // Create semantic type for 'this' so field access can infer the correct class type
        auto this_type = std::make_shared<types::Type>();
        this_type->kind = types::ClassType{c.name};
        locals_["this"] = VarInfo{"%this", "ptr", this_type, std::nullopt};
    }

    // Set up locals for other parameters (non-this)
    size_t param_idx = method.is_static ? 0 : 1; // Skip 'this' index
    for (const auto& param : method.params) {
        std::string pname = get_class_param_name(param);
        if (pname == "this")
            continue;
        if (param_idx >= param_names.size())
            break;

        // Resolve semantic type for the parameter
        types::TypePtr semantic = nullptr;
        if (param.type) {
            semantic = resolve_parser_type_with_subs(*param.type, {});
        }

        locals_[param_names[param_idx]] =
            VarInfo{"%" + param_names[param_idx], param_types[param_idx], semantic, std::nullopt};
        ++param_idx;
    }

    // Generate body
    if (method.body) {
        current_func_ = func_name;
        current_ret_type_ = ret_type;
        block_terminated_ = false; // Reset for new method body

        for (const auto& stmt : method.body->stmts) {
            gen_stmt(*stmt);
        }

        // Generate trailing expression (if any)
        if (method.body->expr.has_value() && !block_terminated_) {
            std::string expr_val = gen_expr(*method.body->expr.value());
            // Return the expression value for non-void methods
            // Note: If the expression was a ReturnExpr, gen_expr already emitted ret
            // and set block_terminated_, so we check again here
            if (ret_type != "void" && !block_terminated_) {
                emit_line("  ret " + ret_type + " " + expr_val);
                block_terminated_ = true;
            }
        }

        // Add implicit return for void functions
        if (ret_type == "void" && !block_terminated_) {
            emit_line("  ret void");
        }
    }

    emit_line("}");
    emit_line("");

    // Clear locals
    locals_.clear();

    // Register function
    functions_[c.name + "_" + method.name] = FuncInfo{
        func_name, ret_type + " (" + (method.is_static ? "" : "ptr") + ")", ret_type, param_types};
}

// ============================================================================
// Virtual Method Dispatch
// ============================================================================

auto LLVMIRGen::gen_virtual_call(const std::string& obj_reg, const std::string& class_name,
                                 const std::string& method_name,
                                 const std::vector<std::string>& args,
                                 const std::vector<std::string>& arg_types) -> std::string {

    // Look up vtable slot for this method
    auto it = class_vtable_layout_.find(class_name);
    if (it == class_vtable_layout_.end()) {
        report_error("Unknown class for virtual dispatch: " + class_name, SourceSpan{});
        return "null";
    }

    size_t vtable_slot = SIZE_MAX;
    for (const auto& vm : it->second) {
        if (vm.name == method_name) {
            vtable_slot = vm.vtable_index;
            break;
        }
    }

    if (vtable_slot == SIZE_MAX) {
        report_error("Method not found in vtable: " + method_name, SourceSpan{});
        return "null";
    }

    std::string class_type = "%class." + class_name;
    std::string vtable_type = "%vtable." + class_name;

    // Load vtable pointer from object (field 0)
    std::string vtable_ptr_ptr = fresh_reg();
    emit_line("  " + vtable_ptr_ptr + " = getelementptr " + class_type + ", ptr " + obj_reg +
              ", i32 0, i32 0");

    std::string vtable_ptr = fresh_reg();
    emit_line("  " + vtable_ptr + " = load ptr, ptr " + vtable_ptr_ptr);

    // Load function pointer from vtable slot
    std::string func_ptr_ptr = fresh_reg();
    emit_line("  " + func_ptr_ptr + " = getelementptr " + vtable_type + ", ptr " + vtable_ptr +
              ", i32 0, i32 " + std::to_string(vtable_slot));

    std::string func_ptr = fresh_reg();
    emit_line("  " + func_ptr + " = load ptr, ptr " + func_ptr_ptr);

    // Get actual return type from method signature
    std::string ret_type = "void";
    auto class_def = env_.lookup_class(class_name);
    if (class_def) {
        for (const auto& m : class_def->methods) {
            if (m.sig.name == method_name) {
                ret_type = llvm_type_from_semantic(m.sig.return_type);
                break;
            }
        }
    }

    // Build function type for indirect call
    std::string func_type = ret_type + " (ptr";
    for (const auto& at : arg_types) {
        func_type += ", " + at;
    }
    func_type += ")";

    // Call the virtual function
    std::string result = fresh_reg();
    std::string call;
    if (ret_type == "void") {
        call = "  call void " + func_ptr + "(ptr " + obj_reg;
    } else {
        call = "  " + result + " = call " + ret_type + " " + func_ptr + "(ptr " + obj_reg;
    }
    for (size_t i = 0; i < args.size(); ++i) {
        call += ", " + arg_types[i] + " " + args[i];
    }
    call += ")";
    emit_line(call);

    last_expr_type_ = ret_type;
    return ret_type == "void" ? "void" : result;
}

// ============================================================================
// Interface Vtable Generation
// ============================================================================

void LLVMIRGen::gen_interface_decl(const parser::InterfaceDecl& iface) {
    // Interface is similar to a behavior - defines method signatures
    // Classes implementing the interface will have vtable slots for these methods

    std::vector<std::string> method_names;
    for (const auto& method : iface.methods) {
        method_names.push_back(method.name);
    }

    // Store interface method order for vtable generation
    interface_method_order_[iface.name] = method_names;

    // Emit dyn type for interface (fat pointer: data + vtable)
    std::string dyn_type = "%dyn." + iface.name + " = type { ptr, ptr }";
    emit_line(dyn_type);
}

// ============================================================================
// Base Expression Generation
// ============================================================================

auto LLVMIRGen::gen_base_expr(const parser::BaseExpr& base) -> std::string {
    // Get the 'this' pointer
    auto it = locals_.find("this");
    if (it == locals_.end()) {
        report_error("'base' used outside of class method", base.span);
        return "null";
    }

    std::string this_ptr = it->second.reg;

    // Look up the current class from the type context
    std::string current_class;
    for (const auto& [name, type] : class_types_) {
        if (it->second.type.find("%class." + name) != std::string::npos ||
            it->second.type == "ptr") {
            current_class = name;
            break;
        }
    }

    if (current_class.empty()) {
        report_error("Cannot determine current class for base expression", base.span);
        return "null";
    }

    // Look up base class from type environment
    auto class_def = env_.lookup_class(current_class);
    if (!class_def || !class_def->base_class) {
        report_error("Class has no base class", base.span);
        return "null";
    }

    std::string base_class = class_def->base_class.value();

    if (base.is_method_call) {
        // Generate direct (non-virtual) call to base class method
        std::string func_name = "@tml_" + get_suite_prefix() + base_class + "_" + base.member;

        // Cast this to base class type (embedded at field 1 after vtable)
        std::string base_ptr = fresh_reg();
        emit_line("  " + base_ptr + " = getelementptr %class." + current_class + ", ptr " +
                  this_ptr + ", i32 0, i32 1");

        // Generate arguments
        std::vector<std::string> args;
        std::vector<std::string> arg_types;
        for (const auto& arg : base.args) {
            args.push_back(gen_expr(*arg));
            arg_types.push_back(last_expr_type_.empty() ? "i64" : last_expr_type_);
        }

        // Look up return type
        std::string ret_type = "void";
        auto base_def = env_.lookup_class(base_class);
        if (base_def) {
            for (const auto& method : base_def->methods) {
                if (method.sig.name == base.member && method.sig.return_type) {
                    ret_type = llvm_type_from_semantic(method.sig.return_type);
                    break;
                }
            }
        }

        // Call the base method directly (non-virtual)
        std::string call = "  ";
        std::string result;
        if (ret_type != "void") {
            result = fresh_reg();
            call += result + " = ";
        }
        call += "call " + ret_type + " " + func_name + "(ptr " + base_ptr;
        for (size_t i = 0; i < args.size(); ++i) {
            call += ", " + arg_types[i] + " " + args[i];
        }
        call += ")";
        emit_line(call);

        return result.empty() ? "void" : result;
    } else {
        // Field access on base class
        auto base_class_def = env_.lookup_class(base_class);
        if (!base_class_def) {
            report_error("Base class not found", base.span);
            return "null";
        }

        int field_idx = -1;
        std::string field_type;
        for (size_t i = 0; i < base_class_def->fields.size(); ++i) {
            if (base_class_def->fields[i].name == base.member) {
                field_idx = static_cast<int>(i) + 1; // +1 for vtable
                field_type = llvm_type_from_semantic(base_class_def->fields[i].type);
                break;
            }
        }

        if (field_idx < 0) {
            report_error("Field not found in base class: " + base.member, base.span);
            return "null";
        }

        std::string base_ptr = fresh_reg();
        emit_line("  " + base_ptr + " = getelementptr %class." + current_class + ", ptr " +
                  this_ptr + ", i32 0, i32 1");

        std::string field_ptr = fresh_reg();
        emit_line("  " + field_ptr + " = getelementptr %class." + base_class + ", ptr " + base_ptr +
                  ", i32 0, i32 " + std::to_string(field_idx));

        std::string value = fresh_reg();
        emit_line("  " + value + " = load " + field_type + ", ptr " + field_ptr);

        return value;
    }
}

// ============================================================================
// New Expression Generation
// ============================================================================

auto LLVMIRGen::gen_new_expr(const parser::NewExpr& new_expr) -> std::string {
    std::string class_name;
    if (!new_expr.class_type.segments.empty()) {
        class_name = new_expr.class_type.segments.back();
    } else {
        report_error("Invalid class name in new expression", new_expr.span);
        return "null";
    }

    auto it = class_types_.find(class_name);
    if (it == class_types_.end()) {
        report_error("Unknown class: " + class_name, new_expr.span);
        return "null";
    }

    // Generate constructor call
    std::string ctor_name = "@tml_" + get_suite_prefix() + class_name + "_new";

    std::vector<std::string> args;
    std::vector<std::string> arg_types;
    for (const auto& arg : new_expr.args) {
        args.push_back(gen_expr(*arg));
        arg_types.push_back(last_expr_type_.empty() ? "i64" : last_expr_type_);
    }

    std::string result = fresh_reg();
    std::string call = "  " + result + " = call ptr " + ctor_name + "(";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0)
            call += ", ";
        call += arg_types[i] + " " + args[i];
    }
    call += ")";
    emit_line(call);

    return result;
}

} // namespace tml::codegen
