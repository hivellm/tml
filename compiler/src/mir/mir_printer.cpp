//! # MIR Pretty Printer
//!
//! This file implements human-readable MIR output for debugging.
//!
//! ## Output Format
//!
//! ```text
//! ; MIR Module: main
//!
//! struct Point { x: I32, y: I32 }
//!
//! func add(a: I32, b: I32) -> I32 {
//! bb0:
//!     %0 = add %a, %b
//!     return %0
//! }
//! ```
//!
//! ## Features
//!
//! - Struct and enum definitions with type parameters
//! - Function signatures with parameter types
//! - Basic blocks with predecessor comments
//! - All instruction types and terminators
//! - SSA values and type annotations

#include "mir/mir.hpp"

#include <sstream>

namespace tml::mir {

MirPrinter::MirPrinter(bool use_colors) : use_colors_(use_colors) {}

auto MirPrinter::print_module(const Module& module) -> std::string {
    std::ostringstream out;
    out << "; MIR Module: " << module.name << "\n\n";

    // Print struct definitions
    if (!module.structs.empty()) {
        out << "; Struct Definitions\n";
        for (const auto& s : module.structs) {
            out << "struct " << s.name;
            if (!s.type_params.empty()) {
                out << "[";
                for (size_t i = 0; i < s.type_params.size(); ++i) {
                    if (i > 0)
                        out << ", ";
                    out << s.type_params[i];
                }
                out << "]";
            }
            out << " {\n";
            for (const auto& field : s.fields) {
                out << "    " << field.name << ": " << print_type(field.type) << "\n";
            }
            out << "}\n\n";
        }
    }

    // Print enum definitions
    if (!module.enums.empty()) {
        out << "; Enum Definitions\n";
        for (const auto& e : module.enums) {
            out << "enum " << e.name;
            if (!e.type_params.empty()) {
                out << "[";
                for (size_t i = 0; i < e.type_params.size(); ++i) {
                    if (i > 0)
                        out << ", ";
                    out << e.type_params[i];
                }
                out << "]";
            }
            out << " {\n";
            for (const auto& v : e.variants) {
                out << "    " << v.name;
                if (!v.payload_types.empty()) {
                    out << "(";
                    for (size_t i = 0; i < v.payload_types.size(); ++i) {
                        if (i > 0)
                            out << ", ";
                        out << print_type(v.payload_types[i]);
                    }
                    out << ")";
                }
                out << "\n";
            }
            out << "}\n\n";
        }
    }

    // Print functions
    for (const auto& func : module.functions) {
        out << print_function(func) << "\n";
    }

    return out.str();
}

auto MirPrinter::print_function(const Function& func) -> std::string {
    std::ostringstream out;

    // Function signature
    out << "func " << func.name << "(";
    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i > 0)
            out << ", ";
        out << "%" << func.params[i].value_id << " " << func.params[i].name << ": "
            << print_type(func.params[i].type);
    }
    out << ")";
    if (!func.return_type->is_unit()) {
        out << " -> " << print_type(func.return_type);
    }
    out << " {\n";

    // Print basic blocks
    for (const auto& block : func.blocks) {
        out << print_block(block);
    }

    out << "}\n";
    return out.str();
}

auto MirPrinter::print_block(const BasicBlock& block) -> std::string {
    std::ostringstream out;

    out << block.name << ":\n";

    // Print predecessors as comment
    if (!block.predecessors.empty()) {
        out << "    ; preds: ";
        for (size_t i = 0; i < block.predecessors.size(); ++i) {
            if (i > 0)
                out << ", ";
            out << "bb" << block.predecessors[i];
        }
        out << "\n";
    }

    // Print instructions
    for (const auto& inst : block.instructions) {
        out << "    " << print_instruction(inst) << "\n";
    }

    // Print terminator
    if (block.terminator.has_value()) {
        out << "    " << print_terminator(*block.terminator) << "\n";
    }

    return out.str();
}

auto MirPrinter::print_instruction(const InstructionData& inst) -> std::string {
    std::ostringstream out;

    // Result assignment
    if (inst.result != INVALID_VALUE) {
        out << "%" << inst.result << " = ";
    }

    // Instruction body
    std::visit(
        [&out, this](const auto& i) {
            using T = std::decay_t<decltype(i)>;

            if constexpr (std::is_same_v<T, BinaryInst>) {
                static const char* op_names[] = {"add", "sub",  "mul", "div",  "mod", "eq",
                                                 "ne",  "lt",   "le",  "gt",   "ge",  "and",
                                                 "or",  "band", "bor", "bxor", "shl", "shr"};
                out << op_names[static_cast<int>(i.op)] << " " << print_value(i.left) << ", "
                    << print_value(i.right);
            } else if constexpr (std::is_same_v<T, UnaryInst>) {
                static const char* op_names[] = {"neg", "not", "bnot"};
                out << op_names[static_cast<int>(i.op)] << " " << print_value(i.operand);
            } else if constexpr (std::is_same_v<T, LoadInst>) {
                out << "load " << print_value(i.ptr);
            } else if constexpr (std::is_same_v<T, StoreInst>) {
                out << "store " << print_value(i.value) << " to " << print_value(i.ptr);
            } else if constexpr (std::is_same_v<T, AllocaInst>) {
                out << "alloca " << print_type(i.alloc_type);
                if (!i.name.empty()) {
                    out << " ; " << i.name;
                }
            } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                out << "gep " << print_value(i.base);
                for (const auto& idx : i.indices) {
                    out << ", " << print_value(idx);
                }
            } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                out << "extractvalue " << print_value(i.aggregate);
                for (auto idx : i.indices) {
                    out << ", " << idx;
                }
            } else if constexpr (std::is_same_v<T, InsertValueInst>) {
                out << "insertvalue " << print_value(i.aggregate) << ", " << print_value(i.value);
                for (auto idx : i.indices) {
                    out << ", " << idx;
                }
            } else if constexpr (std::is_same_v<T, CallInst>) {
                out << "call " << i.func_name << "(";
                for (size_t j = 0; j < i.args.size(); ++j) {
                    if (j > 0)
                        out << ", ";
                    out << print_value(i.args[j]);
                }
                out << ")";
            } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                out << "methodcall " << print_value(i.receiver) << "." << i.method_name << "(";
                for (size_t j = 0; j < i.args.size(); ++j) {
                    if (j > 0)
                        out << ", ";
                    out << print_value(i.args[j]);
                }
                out << ")";
            } else if constexpr (std::is_same_v<T, CastInst>) {
                static const char* cast_names[] = {"bitcast", "trunc",  "zext",     "sext",
                                                   "fptrunc", "fpext",  "fptosi",   "fptoui",
                                                   "sitofp",  "uitofp", "ptrtoint", "inttoptr"};
                out << cast_names[static_cast<int>(i.kind)] << " " << print_value(i.operand)
                    << " to " << print_type(i.target_type);
            } else if constexpr (std::is_same_v<T, PhiInst>) {
                out << "phi ";
                for (size_t j = 0; j < i.incoming.size(); ++j) {
                    if (j > 0)
                        out << ", ";
                    out << "[" << print_value(i.incoming[j].first) << ", bb" << i.incoming[j].second
                        << "]";
                }
            } else if constexpr (std::is_same_v<T, ConstantInst>) {
                std::visit(
                    [&out](const auto& c) {
                        using C = std::decay_t<decltype(c)>;
                        if constexpr (std::is_same_v<C, ConstInt>) {
                            out << "const i" << c.bit_width << " " << c.value;
                        } else if constexpr (std::is_same_v<C, ConstFloat>) {
                            out << "const " << (c.is_f64 ? "f64" : "f32") << " " << c.value;
                        } else if constexpr (std::is_same_v<C, ConstBool>) {
                            out << "const bool " << (c.value ? "true" : "false");
                        } else if constexpr (std::is_same_v<C, ConstString>) {
                            out << "const str \"" << c.value << "\"";
                        } else if constexpr (std::is_same_v<C, ConstUnit>) {
                            out << "const unit";
                        }
                    },
                    i.value);
            } else if constexpr (std::is_same_v<T, SelectInst>) {
                out << "select " << print_value(i.condition) << ", " << print_value(i.true_val)
                    << ", " << print_value(i.false_val);
            } else if constexpr (std::is_same_v<T, StructInitInst>) {
                out << "struct " << i.struct_name << " {";
                for (size_t j = 0; j < i.fields.size(); ++j) {
                    if (j > 0)
                        out << ", ";
                    out << print_value(i.fields[j]);
                }
                out << "}";
            } else if constexpr (std::is_same_v<T, EnumInitInst>) {
                out << "enum " << i.enum_name << "::" << i.variant_name;
                if (!i.payload.empty()) {
                    out << "(";
                    for (size_t j = 0; j < i.payload.size(); ++j) {
                        if (j > 0)
                            out << ", ";
                        out << print_value(i.payload[j]);
                    }
                    out << ")";
                }
            } else if constexpr (std::is_same_v<T, TupleInitInst>) {
                out << "tuple (";
                for (size_t j = 0; j < i.elements.size(); ++j) {
                    if (j > 0)
                        out << ", ";
                    out << print_value(i.elements[j]);
                }
                out << ")";
            } else if constexpr (std::is_same_v<T, ArrayInitInst>) {
                out << "array [";
                for (size_t j = 0; j < i.elements.size(); ++j) {
                    if (j > 0)
                        out << ", ";
                    out << print_value(i.elements[j]);
                }
                out << "]";
            } else if constexpr (std::is_same_v<T, AwaitInst>) {
                out << "await " << print_value(i.poll_value);
                out << " (suspension " << i.suspension_id << ")";
            } else if constexpr (std::is_same_v<T, ClosureInitInst>) {
                out << "closure " << i.func_name << " [";
                for (size_t j = 0; j < i.captures.size(); ++j) {
                    if (j > 0)
                        out << ", ";
                    out << i.captures[j].first << " = " << print_value(i.captures[j].second);
                }
                out << "]";
            }
        },
        inst.inst);

    return out.str();
}

auto MirPrinter::print_terminator(const Terminator& term) -> std::string {
    std::ostringstream out;

    std::visit(
        [&out, this](const auto& t) {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, ReturnTerm>) {
                out << "return";
                if (t.value.has_value()) {
                    out << " " << print_value(*t.value);
                }
            } else if constexpr (std::is_same_v<T, BranchTerm>) {
                out << "br bb" << t.target;
            } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                out << "br " << print_value(t.condition) << ", bb" << t.true_block << ", bb"
                    << t.false_block;
            } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                out << "switch " << print_value(t.discriminant) << " [\n";
                for (const auto& [val, block] : t.cases) {
                    out << "        " << val << " -> bb" << block << "\n";
                }
                out << "        default -> bb" << t.default_block << "\n";
                out << "    ]";
            } else if constexpr (std::is_same_v<T, UnreachableTerm>) {
                out << "unreachable";
            }
        },
        term);

    return out.str();
}

auto MirPrinter::print_value(const Value& val) -> std::string {
    if (!val.is_valid()) {
        return "<invalid>";
    }
    return "%" + std::to_string(val.id);
}

auto MirPrinter::print_type(const MirTypePtr& type) -> std::string {
    if (!type)
        return "<null>";

    std::ostringstream out;

    std::visit(
        [&out, this](const auto& t) {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, MirPrimitiveType>) {
                static const char* names[] = {"()",   "bool", "i8",  "i16", "i32", "i64",
                                              "i128", "u8",   "u16", "u32", "u64", "u128",
                                              "f32",  "f64",  "ptr", "str"};
                out << names[static_cast<int>(t.kind)];
            } else if constexpr (std::is_same_v<T, MirPointerType>) {
                out << (t.is_mut ? "*mut " : "*") << print_type(t.pointee);
            } else if constexpr (std::is_same_v<T, MirArrayType>) {
                out << "[" << print_type(t.element) << "; " << t.size << "]";
            } else if constexpr (std::is_same_v<T, MirSliceType>) {
                out << "[" << print_type(t.element) << "]";
            } else if constexpr (std::is_same_v<T, MirTupleType>) {
                out << "(";
                for (size_t i = 0; i < t.elements.size(); ++i) {
                    if (i > 0)
                        out << ", ";
                    out << print_type(t.elements[i]);
                }
                out << ")";
            } else if constexpr (std::is_same_v<T, MirStructType>) {
                out << t.name;
                if (!t.type_args.empty()) {
                    out << "[";
                    for (size_t i = 0; i < t.type_args.size(); ++i) {
                        if (i > 0)
                            out << ", ";
                        out << print_type(t.type_args[i]);
                    }
                    out << "]";
                }
            } else if constexpr (std::is_same_v<T, MirEnumType>) {
                out << t.name;
                if (!t.type_args.empty()) {
                    out << "[";
                    for (size_t i = 0; i < t.type_args.size(); ++i) {
                        if (i > 0)
                            out << ", ";
                        out << print_type(t.type_args[i]);
                    }
                    out << "]";
                }
            } else if constexpr (std::is_same_v<T, MirFunctionType>) {
                out << "func(";
                for (size_t i = 0; i < t.params.size(); ++i) {
                    if (i > 0)
                        out << ", ";
                    out << print_type(t.params[i]);
                }
                out << ") -> " << print_type(t.return_type);
            }
        },
        type->kind);

    return out.str();
}

} // namespace tml::mir
