//! # LLVM IR Generator - Virtual Dispatch, New/Base Expressions, Properties
//!
//! This file handles virtual method dispatch, interface vtable generation,
//! external class types, base/new expressions, property getters/setters,
//! vtable splitting (hot/cold), and speculative devirtualization.
//!
//! Extracted from class_codegen.cpp to reduce file size.

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/token.hpp"
#include "types/env.hpp"
#include "types/type.hpp"

namespace tml::codegen {

// ============================================================================
// Virtual Method Dispatch
// ============================================================================

auto LLVMIRGen::gen_virtual_call(const std::string& obj_reg, const std::string& class_name,
                                 const std::string& method_name,
                                 const std::vector<std::string>& args,
                                 const std::vector<std::string>& arg_types) -> std::string {

    // Check if this is a @value class - use direct dispatch instead of virtual
    bool is_value = value_classes_.count(class_name) > 0;

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

    if (is_value) {
        // Direct dispatch for @value classes - no vtable lookup
        std::string func_name = "@tml_" + get_suite_prefix() + class_name + "_" + method_name;

        // Call the method directly
        std::string result = fresh_reg();
        std::string call;
        if (ret_type == "void") {
            call = "  call void " + func_name + "(ptr " + obj_reg;
        } else {
            call = "  " + result + " = call " + ret_type + " " + func_name + "(ptr " + obj_reg;
        }
        for (size_t i = 0; i < args.size(); ++i) {
            call += ", " + arg_types[i] + " " + args[i];
        }
        call += ")";
        emit_line(call);

        last_expr_type_ = ret_type;
        return ret_type == "void" ? "void" : result;
    }

    // Virtual dispatch for regular classes

    // Look up vtable slot for this method
    auto it = class_vtable_layout_.find(class_name);
    if (it == class_vtable_layout_.end()) {
        report_error("Unknown class for virtual dispatch: " + class_name, SourceSpan{}, "C028");
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
        report_error("Method not found in vtable: " + method_name, SourceSpan{}, "C033");
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
// External Class Type Generation
// ============================================================================

void LLVMIRGen::emit_external_class_type(const std::string& name, const types::ClassDef& def) {
    // Skip if already emitted
    if (class_types_.find(name) != class_types_.end()) {
        return;
    }

    std::string type_name = "%class." + name;

    // Collect field types
    std::vector<std::string> field_types;
    field_types.push_back("ptr"); // Vtable pointer is always first

    // If base class, recursively emit it first
    if (def.base_class) {
        auto base_class = env_.lookup_class(*def.base_class);
        // Fallback: try module registry for non-imported base classes
        if (!base_class && env_.module_registry()) {
            // Search all modules for the base class
            for (const auto& [mod_name, mod] : env_.module_registry()->get_all_modules()) {
                auto found = env_.module_registry()->lookup_class(mod_name, *def.base_class);
                if (found) {
                    base_class = found;
                    break;
                }
            }
        }
        if (base_class) {
            if (class_types_.find(*def.base_class) == class_types_.end()) {
                emit_external_class_type(*def.base_class, *base_class);
            }
            field_types.push_back("%class." + *def.base_class);
        } else if (class_types_.find(*def.base_class) != class_types_.end()) {
            // Base class already registered (e.g., from earlier in this module)
            field_types.push_back("%class." + *def.base_class);
        }
    }

    // Add own instance fields
    std::vector<ClassFieldInfo> field_info;
    size_t field_offset = field_types.size();

    // First, add inherited fields from base class (for field access in methods)
    // This mirrors the logic in gen_class_decl (lines 176-206)
    if (def.base_class) {
        int base_class_idx = 1; // Base class is always at index 1 (after vtable ptr at 0)
        auto base_fields_it = class_fields_.find(*def.base_class);
        if (base_fields_it != class_fields_.end()) {
            for (const auto& base_field : base_fields_it->second) {
                ClassFieldInfo inherited;
                inherited.name = base_field.name;
                inherited.index = -1; // Not a direct index
                inherited.llvm_type = base_field.llvm_type;
                inherited.vis = base_field.vis;
                inherited.is_inherited = true;

                // Build the inheritance path: first step accesses base in current class
                inherited.inheritance_path.push_back({*def.base_class, base_class_idx});

                if (base_field.is_inherited) {
                    // Append the path from the base class to the actual field
                    for (const auto& step : base_field.inheritance_path) {
                        inherited.inheritance_path.push_back(step);
                    }
                } else {
                    // Field is directly in the base class - add final step
                    inherited.inheritance_path.push_back({*def.base_class, base_field.index});
                }
                field_info.push_back(inherited);
            }
        }
    }

    for (const auto& field : def.fields) {
        if (field.is_static)
            continue;

        std::string ft = llvm_type_from_semantic(field.type);
        if (ft == "void")
            ft = "{}";
        field_types.push_back(ft);

        ClassFieldInfo fi;
        fi.name = field.name;
        fi.index = static_cast<int>(field_offset++);
        fi.llvm_type = ft;
        fi.vis = static_cast<parser::MemberVisibility>(field.vis);
        field_info.push_back(fi);
    }

    // Emit class type definition
    std::string type_def = type_name + " = type { ";
    for (size_t i = 0; i < field_types.size(); ++i) {
        if (i > 0)
            type_def += ", ";
        type_def += field_types[i];
    }
    type_def += " }";
    emit_line(type_def);

    // Register class type
    class_types_[name] = type_name;
    class_fields_[name] = field_info;
}

// ============================================================================
// Base Expression Generation
// ============================================================================

auto LLVMIRGen::gen_base_expr(const parser::BaseExpr& base) -> std::string {
    // Get the 'this' pointer
    auto it = locals_.find("this");
    if (it == locals_.end()) {
        report_error("'base' used outside of class method", base.span, "C001");
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
        report_error("Cannot determine current class for base expression", base.span, "C029");
        return "null";
    }

    // Look up base class from type environment
    auto class_def = env_.lookup_class(current_class);
    if (!class_def || !class_def->base_class) {
        report_error("Class has no base class", base.span, "C030");
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
            report_error("Base class not found", base.span, "C031");
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
            report_error("Field not found in base class: " + base.member, base.span, "C034");
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
        report_error("Invalid class name in new expression", new_expr.span, "T066");
        return "null";
    }

    auto it = class_types_.find(class_name);
    if (it == class_types_.end()) {
        report_error("Unknown class: " + class_name, new_expr.span, "C032");
        return "null";
    }

    // Generate arguments and track types for constructor overload resolution
    std::vector<std::string> args;
    std::vector<std::string> arg_types;
    for (const auto& arg : new_expr.args) {
        args.push_back(gen_expr(*arg));
        arg_types.push_back(last_expr_type_.empty() ? "i64" : last_expr_type_);
    }

    // Build constructor lookup key based on argument types (for overload resolution)
    std::string ctor_key = class_name + "_new";
    if (!arg_types.empty()) {
        for (const auto& at : arg_types) {
            ctor_key += "_" + at;
        }
    }

    // Look up the constructor in functions_ map to get mangled name and return type
    std::string ctor_name;
    std::string ctor_ret_type = "ptr"; // Default: pointer return
    auto func_it = functions_.find(ctor_key);
    if (func_it != functions_.end()) {
        ctor_name = func_it->second.llvm_name;
        // Use the registered return type (value classes return struct, not ptr)
        if (!func_it->second.ret_type.empty()) {
            ctor_ret_type = func_it->second.ret_type;
        }
    } else {
        // Fallback: try without overload suffix for default constructor
        auto default_it = functions_.find(class_name + "_new");
        if (default_it != functions_.end()) {
            ctor_name = default_it->second.llvm_name;
            if (!default_it->second.ret_type.empty()) {
                ctor_ret_type = default_it->second.ret_type;
            }
        } else {
            // Last resort: generate basic name
            ctor_name = "@tml_" + get_suite_prefix() + class_name + "_new";
        }
    }

    std::string result = fresh_reg();
    std::string call = "  " + result + " = call " + ctor_ret_type + " " + ctor_name + "(";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0)
            call += ", ";
        call += arg_types[i] + " " + args[i];
    }
    call += ")";
    emit_line(call);

    last_expr_type_ = ctor_ret_type;
    return result;
}

// ============================================================================
// Property Getter/Setter Generation
// ============================================================================

void LLVMIRGen::gen_class_property(const parser::ClassDecl& c, const parser::PropertyDecl& prop) {
    std::string class_type = "%class." + c.name;
    std::string prop_type = llvm_type_ptr(prop.type);

    // Generate getter if present
    if (prop.has_getter) {
        std::string getter_name = "@tml_" + get_suite_prefix() + c.name + "_get_" + prop.name;

        // Getter signature: (this: ptr) -> PropertyType
        std::string sig;
        if (prop.is_static) {
            sig = "define " + prop_type + " " + getter_name + "()";
        } else {
            sig = "define " + prop_type + " " + getter_name + "(ptr %this)";
        }
        emit_line(sig + " {");
        emit_line("entry:");

        if (prop.getter) {
            // Set up 'this' for non-static properties
            if (!prop.is_static) {
                auto this_type = std::make_shared<types::Type>();
                this_type->kind = types::ClassType{c.name, "", {}};
                locals_["this"] = VarInfo{"%this", "ptr", this_type, std::nullopt};
            }

            // Generate getter expression body
            std::string result = gen_expr(**prop.getter);
            emit_line("  ret " + prop_type + " " + result);

            if (!prop.is_static) {
                locals_.erase("this");
            }
        } else {
            // No explicit getter body - generate default field access
            // Find backing field (typically _name or same as property)
            std::string backing_field = "_" + prop.name;
            bool found = false;
            int field_idx = -1;
            std::string field_type_str;

            // Look for backing field
            for (const auto& field_info : class_fields_[c.name]) {
                if (field_info.name == backing_field || field_info.name == prop.name) {
                    field_idx = field_info.index;
                    field_type_str = field_info.llvm_type;
                    found = true;
                    break;
                }
            }

            if (found) {
                std::string field_ptr = fresh_reg();
                emit_line("  " + field_ptr + " = getelementptr " + class_type +
                          ", ptr %this, i32 0, i32 " + std::to_string(field_idx));
                std::string value = fresh_reg();
                emit_line("  " + value + " = load " + prop_type + ", ptr " + field_ptr);
                emit_line("  ret " + prop_type + " " + value);
            } else {
                // Return zero-initialized value as fallback
                emit_line("  ret " + prop_type + " zeroinitializer");
            }
        }

        emit_line("}");
        emit_line("");

        // Register getter function
        std::string getter_sig = prop_type + " (" + std::string(prop.is_static ? "" : "ptr") + ")";
        std::vector<std::string> getter_params =
            prop.is_static ? std::vector<std::string>{} : std::vector<std::string>{"ptr"};
        functions_[c.name + "_get_" + prop.name] =
            FuncInfo{getter_name, getter_sig, prop_type, getter_params};
    }

    // Generate setter if present
    if (prop.has_setter) {
        std::string setter_name = "@tml_" + get_suite_prefix() + c.name + "_set_" + prop.name;

        // Setter signature: (this: ptr, value: PropertyType) -> void
        std::string sig;
        if (prop.is_static) {
            sig = "define void " + setter_name + "(" + prop_type + " %value)";
        } else {
            sig = "define void " + setter_name + "(ptr %this, " + prop_type + " %value)";
        }
        emit_line(sig + " {");
        emit_line("entry:");

        if (prop.setter) {
            // Set up 'this' and 'value' for the setter body
            if (!prop.is_static) {
                auto this_type = std::make_shared<types::Type>();
                this_type->kind = types::ClassType{c.name, "", {}};
                locals_["this"] = VarInfo{"%this", "ptr", this_type, std::nullopt};
            }

            // 'value' is the implicit parameter in setter
            auto value_type = resolve_parser_type_with_subs(*prop.type, {});
            locals_["value"] = VarInfo{"%value", prop_type, value_type, std::nullopt};

            // Generate setter expression body
            gen_expr(**prop.setter);

            locals_.erase("value");
            if (!prop.is_static) {
                locals_.erase("this");
            }
        } else {
            // No explicit setter body - generate default field store
            std::string backing_field = "_" + prop.name;
            bool found = false;
            int field_idx = -1;

            for (const auto& field_info : class_fields_[c.name]) {
                if (field_info.name == backing_field || field_info.name == prop.name) {
                    field_idx = field_info.index;
                    found = true;
                    break;
                }
            }

            if (found) {
                std::string field_ptr = fresh_reg();
                emit_line("  " + field_ptr + " = getelementptr " + class_type +
                          ", ptr %this, i32 0, i32 " + std::to_string(field_idx));
                emit_line("  store " + prop_type + " %value, ptr " + field_ptr);
            }
        }

        emit_line("  ret void");
        emit_line("}");
        emit_line("");

        // Register setter function
        std::vector<std::string> setter_params = prop.is_static
                                                     ? std::vector<std::string>{prop_type}
                                                     : std::vector<std::string>{"ptr", prop_type};
        std::string setter_sig =
            std::string("void (") + (prop.is_static ? "" : "ptr, ") + prop_type + ")";
        functions_[c.name + "_set_" + prop.name] =
            FuncInfo{setter_name, setter_sig, "void", setter_params};
    }

    // Clear locals after property generation
    locals_.clear();
}

// ============================================================================
// Phase 6.2: Vtable Splitting (Hot/Cold)
// ============================================================================

void LLVMIRGen::analyze_vtable_split(const parser::ClassDecl& c) {
    // Analyze methods and decide which should be in hot vs cold vtable
    // Heuristics for hot methods:
    // 1. Methods with @hot decorator
    // 2. Methods with "simple" names (get, set, is, has, do, on)
    // 3. Destructor is always cold (rarely called in tight loops)
    // 4. Abstract methods are cold (they have no implementation here)

    VtableSplitInfo split;
    split.primary_vtable_name = "@vtable." + c.name;
    split.secondary_vtable_name = "@vtable." + c.name + ".cold";

    // Get vtable layout if it exists
    auto it = class_vtable_layout_.find(c.name);
    if (it == class_vtable_layout_.end()) {
        return; // No vtable for this class
    }

    const auto& vtable_methods = it->second;

    // Analyze each method
    for (const auto& vm : vtable_methods) {
        bool is_hot = false;

        // Check heuristics
        const std::string& name = vm.name;

        // Hot patterns: common accessor patterns
        if (name.find("get") == 0 || name.find("set") == 0 || name.find("is") == 0 ||
            name.find("has") == 0 || name.find("do") == 0 || name.find("on") == 0 ||
            name == "size" || name == "len" || name == "length" || name == "empty" ||
            name == "count" || name == "value" || name == "next" || name == "prev" ||
            name == "item") {
            is_hot = true;
        }

        // Check for @hot decorator on the method
        for (const auto& method : c.methods) {
            if (method.name == name) {
                for (const auto& deco : method.decorators) {
                    if (deco.name == "hot") {
                        is_hot = true;
                        break;
                    }
                    if (deco.name == "cold") {
                        is_hot = false;
                        break;
                    }
                }
                break;
            }
        }

        // Destructor is typically cold
        if (name == "drop" || name == "destroy" || name == "finalize") {
            is_hot = false;
        }

        // Abstract methods are cold
        if (vm.impl_class.empty()) {
            is_hot = false;
        }

        if (is_hot) {
            split.hot_methods.push_back(name);
        } else {
            split.cold_methods.push_back(name);
        }
    }

    // Only split if we have both hot and cold methods and enough of each
    // to make splitting worthwhile (at least 2 cold methods)
    if (!split.hot_methods.empty() && split.cold_methods.size() >= 2) {
        vtable_splits_[c.name] = split;
        vtable_split_stats_.classes_with_split++;
        vtable_split_stats_.hot_methods_total += split.hot_methods.size();
        vtable_split_stats_.cold_methods_total += split.cold_methods.size();
    }
}

void LLVMIRGen::gen_split_vtables(const parser::ClassDecl& c) {
    auto split_it = vtable_splits_.find(c.name);
    if (split_it == vtable_splits_.end()) {
        return; // No split for this class
    }

    const auto& split = split_it->second;
    auto layout_it = class_vtable_layout_.find(c.name);
    if (layout_it == class_vtable_layout_.end()) {
        return;
    }

    const auto& vtable_methods = layout_it->second;

    // Generate hot vtable type and value
    std::string hot_type_name = "%vtable." + c.name + ".hot";
    std::string hot_type = hot_type_name + " = type { ";
    for (size_t i = 0; i < split.hot_methods.size(); ++i) {
        if (i > 0)
            hot_type += ", ";
        hot_type += "ptr";
    }
    if (split.hot_methods.empty()) {
        hot_type += "ptr"; // At least one slot
    }
    hot_type += " }";
    emit_line(hot_type);

    // Generate cold vtable type and value
    if (!split.cold_methods.empty()) {
        std::string cold_type_name = "%vtable." + c.name + ".cold";
        std::string cold_type = cold_type_name + " = type { ";
        for (size_t i = 0; i < split.cold_methods.size(); ++i) {
            if (i > 0)
                cold_type += ", ";
            cold_type += "ptr";
        }
        cold_type += " }";
        emit_line(cold_type);
    }

    // Generate hot vtable global
    std::string hot_value = "{ ";
    for (size_t i = 0; i < split.hot_methods.size(); ++i) {
        if (i > 0)
            hot_value += ", ";

        // Find the method implementation
        std::string impl_class;
        for (const auto& vm : vtable_methods) {
            if (vm.name == split.hot_methods[i]) {
                impl_class = vm.impl_class;
                break;
            }
        }

        if (impl_class.empty()) {
            hot_value += "ptr null";
        } else {
            hot_value += "ptr @tml_" + get_suite_prefix() + impl_class + "_" + split.hot_methods[i];
        }
    }
    if (split.hot_methods.empty()) {
        hot_value += "ptr null";
    }
    hot_value += " }";
    emit_line("@vtable." + c.name + ".hot = internal constant " + hot_type_name + " " + hot_value);

    // Generate cold vtable global
    if (!split.cold_methods.empty()) {
        std::string cold_value = "{ ";
        for (size_t i = 0; i < split.cold_methods.size(); ++i) {
            if (i > 0)
                cold_value += ", ";

            std::string impl_class;
            for (const auto& vm : vtable_methods) {
                if (vm.name == split.cold_methods[i]) {
                    impl_class = vm.impl_class;
                    break;
                }
            }

            if (impl_class.empty()) {
                cold_value += "ptr null";
            } else {
                cold_value +=
                    "ptr @tml_" + get_suite_prefix() + impl_class + "_" + split.cold_methods[i];
            }
        }
        cold_value += " }";
        std::string cold_type_name = "%vtable." + c.name + ".cold";
        emit_line("@vtable." + c.name + ".cold = internal constant " + cold_type_name + " " +
                  cold_value);
    }
}

bool LLVMIRGen::is_hot_method(const std::string& class_name, const std::string& method_name) const {
    auto it = vtable_splits_.find(class_name);
    if (it == vtable_splits_.end()) {
        return true; // No split, all methods are in primary vtable
    }

    const auto& split = it->second;
    for (const auto& hot : split.hot_methods) {
        if (hot == method_name) {
            return true;
        }
    }
    return false;
}

auto LLVMIRGen::get_split_vtable_index(const std::string& class_name,
                                       const std::string& method_name) -> std::pair<bool, size_t> {

    auto split_it = vtable_splits_.find(class_name);
    if (split_it == vtable_splits_.end()) {
        // No split - use original vtable layout
        auto layout_it = class_vtable_layout_.find(class_name);
        if (layout_it == class_vtable_layout_.end()) {
            return {true, SIZE_MAX};
        }

        for (const auto& vm : layout_it->second) {
            if (vm.name == method_name) {
                return {true, vm.vtable_index};
            }
        }
        return {true, SIZE_MAX};
    }

    const auto& split = split_it->second;

    // Check hot methods
    for (size_t i = 0; i < split.hot_methods.size(); ++i) {
        if (split.hot_methods[i] == method_name) {
            return {true, i};
        }
    }

    // Check cold methods
    for (size_t i = 0; i < split.cold_methods.size(); ++i) {
        if (split.cold_methods[i] == method_name) {
            return {false, i};
        }
    }

    return {true, SIZE_MAX};
}

// ============================================================================
// Phase 3: Speculative Devirtualization
// ============================================================================

void LLVMIRGen::init_type_frequency_hints() {
    // Initialize type frequency hints based on class hierarchy analysis
    // Higher frequency for:
    // - Sealed classes (most specific type)
    // - Leaf classes (no subclasses)
    // - Classes with @hot decorator

    for (const auto& [name, type] : class_types_) {
        float frequency = 0.5f; // Default

        auto class_def = env_.lookup_class(name);
        if (!class_def)
            continue;

        // Sealed classes are very likely to be the concrete type
        if (class_def->is_sealed) {
            frequency = 0.95f;
        }

        // Check if this is a leaf class (no known subclasses)
        bool is_leaf = true;
        for (const auto& [other_name, other_type] : class_types_) {
            auto other_def = env_.lookup_class(other_name);
            if (other_def && other_def->base_class && *other_def->base_class == name) {
                is_leaf = false;
                break;
            }
        }

        if (is_leaf && !class_def->is_abstract) {
            frequency = std::max(frequency, 0.85f);
        }

        // Abstract classes are never the concrete type
        if (class_def->is_abstract) {
            frequency = 0.0f;
        }

        type_frequency_hints_[name] = frequency;
    }
}

auto LLVMIRGen::analyze_spec_devirt(const std::string& receiver_class,
                                    const std::string& method_name)
    -> std::optional<SpeculativeDevirtInfo> {

    // Get frequency hint for the receiver class
    auto freq_it = type_frequency_hints_.find(receiver_class);
    float frequency = (freq_it != type_frequency_hints_.end()) ? freq_it->second : 0.5f;

    // If frequency is too low, speculative devirtualization is not profitable
    // Threshold: 70% - we want at least 70% probability of correct guess
    if (frequency < 0.70f) {
        return std::nullopt;
    }

    // Check if the method exists in this class
    auto class_def = env_.lookup_class(receiver_class);
    if (!class_def) {
        return std::nullopt;
    }

    bool has_method = false;
    for (const auto& m : class_def->methods) {
        if (m.sig.name == method_name) {
            has_method = true;
            break;
        }
    }

    // If method not found, check base classes
    if (!has_method && class_def->base_class) {
        std::string current = *class_def->base_class;
        while (!current.empty() && !has_method) {
            auto base_def = env_.lookup_class(current);
            if (!base_def)
                break;

            for (const auto& m : base_def->methods) {
                if (m.sig.name == method_name) {
                    has_method = true;
                    break;
                }
            }

            current = base_def->base_class.value_or("");
        }
    }

    if (!has_method) {
        return std::nullopt;
    }

    SpeculativeDevirtInfo info;
    info.expected_type = receiver_class;
    info.direct_call_target = "@tml_" + get_suite_prefix() + receiver_class + "_" + method_name;
    info.confidence = frequency;

    return info;
}

auto LLVMIRGen::gen_guarded_virtual_call(const std::string& obj_reg,
                                         const std::string& receiver_class,
                                         const SpeculativeDevirtInfo& spec_info,
                                         const std::string& method_name,
                                         const std::vector<std::string>& args,
                                         const std::vector<std::string>& arg_types) -> std::string {

    // Generate a type guard with fast path (direct call) and slow path (virtual dispatch)
    //
    // Code pattern:
    //   %vtable = load ptr, ptr %obj
    //   %expected_vtable = @vtable.ExpectedClass
    //   %is_expected = icmp eq ptr %vtable, %expected_vtable
    //   br i1 %is_expected, label %fast_path, label %slow_path
    // fast_path:
    //   %result_fast = call <ret> @direct_function(%obj, args...)
    //   br label %merge
    // slow_path:
    //   %result_slow = <virtual dispatch code>
    //   br label %merge
    // merge:
    //   %result = phi <ret> [ %result_fast, %fast_path ], [ %result_slow, %slow_path ]

    spec_devirt_stats_.guarded_calls++;

    std::string class_type = "%class." + receiver_class;

    // Look up return type
    std::string ret_type = "void";
    auto class_def = env_.lookup_class(receiver_class);
    if (class_def) {
        for (const auto& m : class_def->methods) {
            if (m.sig.name == method_name) {
                ret_type = llvm_type_from_semantic(m.sig.return_type);
                break;
            }
        }
    }

    // Load actual vtable pointer
    std::string vtable_ptr_ptr = fresh_reg();
    emit_line("  " + vtable_ptr_ptr + " = getelementptr " + class_type + ", ptr " + obj_reg +
              ", i32 0, i32 0");

    std::string actual_vtable = fresh_reg();
    emit_line("  " + actual_vtable + " = load ptr, ptr " + vtable_ptr_ptr);

    // Compare with expected vtable
    std::string expected_vtable = "@vtable." + spec_info.expected_type;
    std::string cmp_result = fresh_reg();
    emit_line("  " + cmp_result + " = icmp eq ptr " + actual_vtable + ", " + expected_vtable);

    // Generate labels
    std::string fast_path = fresh_label("spec_fast");
    std::string slow_path = fresh_label("spec_slow");
    std::string merge = fresh_label("spec_merge");

    emit_line("  br i1 " + cmp_result + ", label %" + fast_path + ", label %" + slow_path);

    // Fast path: direct call
    emit_line(fast_path + ":");
    std::string result_fast;
    std::string call_fast = "  ";
    if (ret_type != "void") {
        result_fast = fresh_reg();
        call_fast += result_fast + " = ";
    }
    call_fast += "call " + ret_type + " " + spec_info.direct_call_target + "(ptr " + obj_reg;
    for (size_t i = 0; i < args.size(); ++i) {
        call_fast += ", " + arg_types[i] + " " + args[i];
    }
    call_fast += ")";
    emit_line(call_fast);
    emit_line("  br label %" + merge);

    // Slow path: virtual dispatch
    emit_line(slow_path + ":");

    // Get vtable slot for this method
    auto layout_it = class_vtable_layout_.find(receiver_class);
    size_t vtable_slot = 0;
    if (layout_it != class_vtable_layout_.end()) {
        for (const auto& vm : layout_it->second) {
            if (vm.name == method_name) {
                vtable_slot = vm.vtable_index;
                break;
            }
        }
    }

    std::string vtable_type = "%vtable." + receiver_class;

    std::string func_ptr_ptr = fresh_reg();
    emit_line("  " + func_ptr_ptr + " = getelementptr " + vtable_type + ", ptr " + actual_vtable +
              ", i32 0, i32 " + std::to_string(vtable_slot));

    std::string func_ptr = fresh_reg();
    emit_line("  " + func_ptr + " = load ptr, ptr " + func_ptr_ptr);

    std::string result_slow;
    std::string call_slow = "  ";
    if (ret_type != "void") {
        result_slow = fresh_reg();
        call_slow += result_slow + " = ";
    }
    call_slow += "call " + ret_type + " " + func_ptr + "(ptr " + obj_reg;
    for (size_t i = 0; i < args.size(); ++i) {
        call_slow += ", " + arg_types[i] + " " + args[i];
    }
    call_slow += ")";
    emit_line(call_slow);
    emit_line("  br label %" + merge);

    // Merge
    emit_line(merge + ":");

    std::string result;
    if (ret_type != "void") {
        result = fresh_reg();
        emit_line("  " + result + " = phi " + ret_type + " [ " + result_fast + ", %" + fast_path +
                  " ], [ " + result_slow + ", %" + slow_path + " ]");
    }

    last_expr_type_ = ret_type;
    return ret_type == "void" ? "void" : result;
}

} // namespace tml::codegen
