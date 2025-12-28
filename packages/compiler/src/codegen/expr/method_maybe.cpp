// LLVM IR generator - Maybe[T] method calls
// Extracted from method.cpp for maintainability

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_maybe_method(const parser::MethodCallExpr& call,
                                  const std::string& receiver,
                                  const std::string& enum_type_name,
                                  const std::string& tag_val,
                                  const types::NamedType& named) -> std::optional<std::string> {
    const std::string& method = call.method;

    // is_just() -> Bool (tag == 0)
    if (method == "is_just") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 0");
        last_expr_type_ = "i1";
        return result;
    }

    // is_nothing() -> Bool (tag == 1)
    if (method == "is_nothing") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 1");
        last_expr_type_ = "i1";
        return result;
    }

    // Get the inner type for value extraction
    types::TypePtr inner_type;
    if (!named.type_args.empty()) {
        inner_type = named.type_args[0];
    }
    std::string inner_llvm_type = inner_type ? llvm_type_from_semantic(inner_type, true) : "i32";

    // unwrap() -> T (get the value from Just, panics on Nothing)
    if (method == "unwrap" || method == "expect") {
        // Extract the data bytes as a pointer
        std::string data_ptr = fresh_reg();
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " + alloca_reg + ", i32 0, i32 1");

        // Load the value
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + inner_llvm_type + ", ptr " + data_ptr);
        last_expr_type_ = inner_llvm_type;
        return result;
    }

    // unwrap_or(default) -> T
    if (method == "unwrap_or") {
        if (call.args.empty()) {
            report_error("unwrap_or() requires a default value", call.span);
            return "0";
        }

        // Generate the default value
        std::string default_val = gen_expr(*call.args[0]);

        // Extract the data from Maybe if Just
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " + alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);

        // Select: is_just ? just_val : default_val
        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        std::string result = fresh_reg();
        emit_line("  " + result + " = select i1 " + is_just + ", " + inner_llvm_type + " " + just_val + ", " + inner_llvm_type + " " + default_val);
        last_expr_type_ = inner_llvm_type;
        return result;
    }

    // unwrap_or_else(f) -> T
    if (method == "unwrap_or_else") {
        if (call.args.empty() || !call.args[0]->is<parser::ClosureExpr>()) {
            report_error("unwrap_or_else requires a closure argument", call.span);
            return "0";
        }
        auto& closure = call.args[0]->as<parser::ClosureExpr>();

        std::string is_just_label = fresh_label("maybe_unwrap_or_else_just");
        std::string is_nothing_label = fresh_label("maybe_unwrap_or_else_nothing");
        std::string end_label = fresh_label("maybe_unwrap_or_else_end");

        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_just + ", label %" + is_just_label + ", label %" + is_nothing_label);

        // just block: return the value
        emit_line(is_just_label + ":");
        current_block_ = is_just_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " + alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);
        emit_line("  br label %" + end_label);

        // nothing block: call closure
        emit_line(is_nothing_label + ":");
        current_block_ = is_nothing_label;
        std::string closure_result = gen_expr(*closure.body);
        std::string nothing_end_block = current_block_;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + inner_llvm_type + " [ " + just_val + ", %" + is_just_label + " ], [ " + closure_result + ", %" + nothing_end_block + " ]");
        last_expr_type_ = inner_llvm_type;
        return result;
    }

    // unwrap_or_default() -> T
    if (method == "unwrap_or_default") {
        // Determine default value based on type
        std::string default_val;
        if (inner_llvm_type == "i8" || inner_llvm_type == "i16" || inner_llvm_type == "i32" ||
            inner_llvm_type == "i64" || inner_llvm_type == "i128") {
            default_val = "0";
        } else if (inner_llvm_type == "float" || inner_llvm_type == "double") {
            default_val = "0.0";
        } else if (inner_llvm_type == "i1") {
            default_val = "false";
        } else {
            default_val = "zeroinitializer";
        }

        // Extract just value
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " + alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);

        // Select: is_just ? just_val : default
        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        std::string result = fresh_reg();
        emit_line("  " + result + " = select i1 " + is_just + ", " + inner_llvm_type + " " + just_val + ", " + inner_llvm_type + " " + default_val);
        last_expr_type_ = inner_llvm_type;
        return result;
    }

    // map(f) -> Maybe[U]
    if (method == "map") {
        if (call.args.empty() || !call.args[0]->is<parser::ClosureExpr>()) {
            report_error("map requires a closure argument", call.span);
            return receiver;
        }
        auto& closure = call.args[0]->as<parser::ClosureExpr>();

        std::string is_just_label = fresh_label("maybe_map_just");
        std::string is_nothing_label = fresh_label("maybe_map_nothing");
        std::string end_label = fresh_label("maybe_map_end");

        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_just + ", label %" + is_just_label + ", label %" + is_nothing_label);

        // just block: apply closure and wrap in Just
        emit_line(is_just_label + ":");
        current_block_ = is_just_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " + alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);

        std::string param_name = "_";
        if (!closure.params.empty() && closure.params[0].first->is<parser::IdentPattern>()) {
            param_name = closure.params[0].first->as<parser::IdentPattern>().name;
        }
        std::string param_alloca = fresh_reg();
        emit_line("  " + param_alloca + " = alloca " + inner_llvm_type);
        emit_line("  store " + inner_llvm_type + " " + just_val + ", ptr " + param_alloca);
        locals_[param_name] = VarInfo{param_alloca, inner_llvm_type, nullptr, std::nullopt};
        std::string mapped_val = gen_expr(*closure.body);
        std::string mapped_type = last_expr_type_;
        locals_.erase(param_name);

        // Create Just(mapped_val)
        std::string result_type_name = enum_type_name;
        if (mapped_type != inner_llvm_type) {
            types::TypePtr mapped_semantic_type = semantic_type_from_llvm(mapped_type);
            std::vector<types::TypePtr> new_type_args = {mapped_semantic_type};
            std::string new_mangled = require_enum_instantiation("Maybe", new_type_args);
            result_type_name = "%struct." + new_mangled;
        }

        std::string just_alloca = fresh_reg();
        emit_line("  " + just_alloca + " = alloca " + result_type_name);
        std::string just_tag_ptr = fresh_reg();
        emit_line("  " + just_tag_ptr + " = getelementptr inbounds " + result_type_name + ", ptr " + just_alloca + ", i32 0, i32 0");
        emit_line("  store i32 0, ptr " + just_tag_ptr);
        std::string just_data_ptr = fresh_reg();
        emit_line("  " + just_data_ptr + " = getelementptr inbounds " + result_type_name + ", ptr " + just_alloca + ", i32 0, i32 1");
        emit_line("  store " + mapped_type + " " + mapped_val + ", ptr " + just_data_ptr);
        std::string just_result = fresh_reg();
        emit_line("  " + just_result + " = load " + result_type_name + ", ptr " + just_alloca);
        emit_line("  br label %" + end_label);

        // nothing block: return Nothing
        emit_line(is_nothing_label + ":");
        current_block_ = is_nothing_label;
        std::string nothing_alloca = fresh_reg();
        emit_line("  " + nothing_alloca + " = alloca " + result_type_name);
        std::string nothing_tag_ptr = fresh_reg();
        emit_line("  " + nothing_tag_ptr + " = getelementptr inbounds " + result_type_name + ", ptr " + nothing_alloca + ", i32 0, i32 0");
        emit_line("  store i32 1, ptr " + nothing_tag_ptr);
        std::string nothing_result = fresh_reg();
        emit_line("  " + nothing_result + " = load " + result_type_name + ", ptr " + nothing_alloca);
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + result_type_name + " [ " + just_result + ", %" + is_just_label + " ], [ " + nothing_result + ", %" + is_nothing_label + " ]");
        last_expr_type_ = result_type_name;
        return result;
    }

    // and_then(f) -> Maybe[U]
    if (method == "and_then") {
        if (call.args.empty() || !call.args[0]->is<parser::ClosureExpr>()) {
            report_error("and_then requires a closure argument", call.span);
            return receiver;
        }
        auto& closure = call.args[0]->as<parser::ClosureExpr>();

        std::string is_just_label = fresh_label("maybe_and_then_just");
        std::string is_nothing_label = fresh_label("maybe_and_then_nothing");
        std::string end_label = fresh_label("maybe_and_then_end");

        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_just + ", label %" + is_just_label + ", label %" + is_nothing_label);

        // just block: call closure (which returns a Maybe)
        emit_line(is_just_label + ":");
        current_block_ = is_just_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " + alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);

        std::string param_name = "_";
        if (!closure.params.empty() && closure.params[0].first->is<parser::IdentPattern>()) {
            param_name = closure.params[0].first->as<parser::IdentPattern>().name;
        }
        std::string param_alloca = fresh_reg();
        emit_line("  " + param_alloca + " = alloca " + inner_llvm_type);
        emit_line("  store " + inner_llvm_type + " " + just_val + ", ptr " + param_alloca);
        locals_[param_name] = VarInfo{param_alloca, inner_llvm_type, nullptr, std::nullopt};
        std::string closure_result = gen_expr(*closure.body);
        std::string just_end_block = current_block_;
        locals_.erase(param_name);
        emit_line("  br label %" + end_label);

        // nothing block: propagate Nothing
        emit_line(is_nothing_label + ":");
        current_block_ = is_nothing_label;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + enum_type_name + " [ " + closure_result + ", %" + just_end_block + " ], [ " + receiver + ", %" + is_nothing_label + " ]");
        last_expr_type_ = enum_type_name;
        return result;
    }

    // or_else(f) -> Maybe[T]
    if (method == "or_else") {
        if (call.args.empty() || !call.args[0]->is<parser::ClosureExpr>()) {
            report_error("or_else requires a closure argument", call.span);
            return receiver;
        }
        auto& closure = call.args[0]->as<parser::ClosureExpr>();

        std::string is_just_label = fresh_label("maybe_or_else_just");
        std::string is_nothing_label = fresh_label("maybe_or_else_nothing");
        std::string end_label = fresh_label("maybe_or_else_end");

        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_just + ", label %" + is_just_label + ", label %" + is_nothing_label);

        // just block: return self
        emit_line(is_just_label + ":");
        current_block_ = is_just_label;
        emit_line("  br label %" + end_label);

        // nothing block: call closure (which returns a Maybe)
        emit_line(is_nothing_label + ":");
        current_block_ = is_nothing_label;
        std::string closure_result = gen_expr(*closure.body);
        std::string nothing_end_block = current_block_;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + enum_type_name + " [ " + receiver + ", %" + is_just_label + " ], [ " + closure_result + ", %" + nothing_end_block + " ]");
        last_expr_type_ = enum_type_name;
        return result;
    }

    // contains(value) -> Bool
    if (method == "contains") {
        if (call.args.empty()) {
            report_error("contains requires an argument", call.span);
            return "false";
        }
        std::string cmp_val = gen_expr(*call.args[0]);

        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");

        std::string is_just_label = fresh_label("maybe_contains_just");
        std::string is_nothing_label = fresh_label("maybe_contains_nothing");
        std::string end_label = fresh_label("maybe_contains_end");
        emit_line("  br i1 " + is_just + ", label %" + is_just_label + ", label %" + is_nothing_label);

        emit_line(is_just_label + ":");
        current_block_ = is_just_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " + alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);

        std::string values_eq = fresh_reg();
        if (inner_llvm_type == "ptr") {
            // str_eq returns i32, convert to i1
            std::string eq_i32 = fresh_reg();
            emit_line("  " + eq_i32 + " = call i32 @str_eq(ptr " + just_val + ", ptr " + cmp_val + ")");
            emit_line("  " + values_eq + " = icmp ne i32 " + eq_i32 + ", 0");
        } else {
            emit_line("  " + values_eq + " = icmp eq " + inner_llvm_type + " " + just_val + ", " + cmp_val);
        }
        emit_line("  br label %" + end_label);

        emit_line(is_nothing_label + ":");
        current_block_ = is_nothing_label;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi i1 [ " + values_eq + ", %" + is_just_label + " ], [ false, %" + is_nothing_label + " ]");
        last_expr_type_ = "i1";
        return result;
    }

    // filter(predicate) -> Maybe[T]
    if (method == "filter") {
        if (call.args.empty() || !call.args[0]->is<parser::ClosureExpr>()) {
            report_error("filter requires a closure argument", call.span);
            return receiver;
        }
        auto& closure = call.args[0]->as<parser::ClosureExpr>();

        std::string is_just_label = fresh_label("maybe_filter_just");
        std::string is_nothing_label = fresh_label("maybe_filter_nothing");
        std::string keep_label = fresh_label("maybe_filter_keep");
        std::string discard_label = fresh_label("maybe_filter_discard");
        std::string end_label = fresh_label("maybe_filter_end");

        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_just + ", label %" + is_just_label + ", label %" + is_nothing_label);

        // just block: test predicate
        emit_line(is_just_label + ":");
        current_block_ = is_just_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " + alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);

        std::string param_name = "_";
        if (!closure.params.empty() && closure.params[0].first->is<parser::IdentPattern>()) {
            param_name = closure.params[0].first->as<parser::IdentPattern>().name;
        }
        std::string param_alloca = fresh_reg();
        emit_line("  " + param_alloca + " = alloca " + inner_llvm_type);
        emit_line("  store " + inner_llvm_type + " " + just_val + ", ptr " + param_alloca);
        locals_[param_name] = VarInfo{param_alloca, inner_llvm_type, nullptr, std::nullopt};
        std::string pred_result = gen_expr(*closure.body);
        std::string just_end_block = current_block_;
        locals_.erase(param_name);
        emit_line("  br i1 " + pred_result + ", label %" + keep_label + ", label %" + discard_label);

        // keep block: return Just
        emit_line(keep_label + ":");
        current_block_ = keep_label;
        emit_line("  br label %" + end_label);

        // discard block: return Nothing
        emit_line(discard_label + ":");
        current_block_ = discard_label;
        std::string nothing_alloca = fresh_reg();
        emit_line("  " + nothing_alloca + " = alloca " + enum_type_name);
        std::string nothing_tag_ptr = fresh_reg();
        emit_line("  " + nothing_tag_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " + nothing_alloca + ", i32 0, i32 0");
        emit_line("  store i32 1, ptr " + nothing_tag_ptr);
        std::string nothing_result = fresh_reg();
        emit_line("  " + nothing_result + " = load " + enum_type_name + ", ptr " + nothing_alloca);
        emit_line("  br label %" + end_label);

        // nothing block: return Nothing (original)
        emit_line(is_nothing_label + ":");
        current_block_ = is_nothing_label;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + enum_type_name + " [ " + receiver + ", %" + keep_label + " ], [ " + nothing_result + ", %" + discard_label + " ], [ " + receiver + ", %" + is_nothing_label + " ]");
        last_expr_type_ = enum_type_name;
        return result;
    }

    // alt(other) -> Maybe[T]
    if (method == "alt") {
        if (call.args.empty()) {
            report_error("alt requires an argument", call.span);
            return receiver;
        }

        std::string other = gen_expr(*call.args[0]);

        // if is_just, return self, else return other
        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        std::string result = fresh_reg();
        emit_line("  " + result + " = select i1 " + is_just + ", " + enum_type_name + " " + receiver + ", " + enum_type_name + " " + other);
        last_expr_type_ = enum_type_name;
        return result;
    }

    // xor(other) -> Maybe[T]
    if (method == "xor") {
        if (call.args.empty()) {
            report_error("xor requires an argument", call.span);
            return receiver;
        }

        std::string other = gen_expr(*call.args[0]);
        std::string other_type = last_expr_type_;

        // Get tag from other
        std::string other_alloca = fresh_reg();
        emit_line("  " + other_alloca + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + other + ", ptr " + other_alloca);
        std::string other_tag_ptr = fresh_reg();
        emit_line("  " + other_tag_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " + other_alloca + ", i32 0, i32 0");
        std::string other_tag = fresh_reg();
        emit_line("  " + other_tag + " = load i32, ptr " + other_tag_ptr);

        // Check conditions
        std::string self_is_just = fresh_reg();
        emit_line("  " + self_is_just + " = icmp eq i32 " + tag_val + ", 0");
        std::string other_is_just = fresh_reg();
        emit_line("  " + other_is_just + " = icmp eq i32 " + other_tag + ", 0");

        // xor: exactly one is Just
        // self_just && !other_just -> return self
        // !self_just && other_just -> return other
        // otherwise -> return Nothing

        std::string self_only_label = fresh_label("xor_self_only");
        std::string check_other_label = fresh_label("xor_check_other");
        std::string other_only_label = fresh_label("xor_other_only");
        std::string nothing_label = fresh_label("xor_nothing");
        std::string end_label = fresh_label("xor_end");

        emit_line("  br i1 " + self_is_just + ", label %" + check_other_label + ", label %" + other_only_label);

        // self is just, check if other is nothing
        emit_line(check_other_label + ":");
        current_block_ = check_other_label;
        std::string other_is_nothing = fresh_reg();
        emit_line("  " + other_is_nothing + " = icmp eq i32 " + other_tag + ", 1");
        emit_line("  br i1 " + other_is_nothing + ", label %" + self_only_label + ", label %" + nothing_label);

        // return self
        emit_line(self_only_label + ":");
        current_block_ = self_only_label;
        emit_line("  br label %" + end_label);

        // self is nothing, check if other is just
        emit_line(other_only_label + ":");
        current_block_ = other_only_label;
        emit_line("  br i1 " + other_is_just + ", label %" + end_label + ", label %" + nothing_label);

        // both are just or both are nothing -> return nothing
        emit_line(nothing_label + ":");
        current_block_ = nothing_label;
        std::string nothing_alloca = fresh_reg();
        emit_line("  " + nothing_alloca + " = alloca " + enum_type_name);
        std::string nothing_tag_ptr = fresh_reg();
        emit_line("  " + nothing_tag_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " + nothing_alloca + ", i32 0, i32 0");
        emit_line("  store i32 1, ptr " + nothing_tag_ptr);
        std::string nothing_result = fresh_reg();
        emit_line("  " + nothing_result + " = load " + enum_type_name + ", ptr " + nothing_alloca);
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + enum_type_name + " [ " + receiver + ", %" + self_only_label + " ], [ " + other + ", %" + other_only_label + " ], [ " + nothing_result + ", %" + nothing_label + " ]");
        last_expr_type_ = enum_type_name;
        return result;
    }

    // map_or(default, f) -> U
    if (method == "map_or") {
        if (call.args.size() < 2) {
            report_error("map_or requires a default value and a closure", call.span);
            return "0";
        }

        // Generate default value first
        std::string default_val = gen_expr(*call.args[0]);
        std::string default_type = last_expr_type_;

        if (!call.args[1]->is<parser::ClosureExpr>()) {
            report_error("map_or requires a closure as second argument", call.span);
            return default_val;
        }
        auto& closure = call.args[1]->as<parser::ClosureExpr>();

        std::string is_just_label = fresh_label("maybe_map_or_just");
        std::string is_nothing_label = fresh_label("maybe_map_or_nothing");
        std::string end_label = fresh_label("maybe_map_or_end");

        std::string is_just = fresh_reg();
        emit_line("  " + is_just + " = icmp eq i32 " + tag_val + ", 0");
        emit_line("  br i1 " + is_just + ", label %" + is_just_label + ", label %" + is_nothing_label);

        // just block: apply closure
        emit_line(is_just_label + ":");
        current_block_ = is_just_label;
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + enum_type_name);
        emit_line("  store " + enum_type_name + " " + receiver + ", ptr " + alloca_reg);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + enum_type_name + ", ptr " + alloca_reg + ", i32 0, i32 1");
        std::string just_val = fresh_reg();
        emit_line("  " + just_val + " = load " + inner_llvm_type + ", ptr " + data_ptr);

        std::string param_name = "_";
        if (!closure.params.empty() && closure.params[0].first->is<parser::IdentPattern>()) {
            param_name = closure.params[0].first->as<parser::IdentPattern>().name;
        }
        std::string param_alloca = fresh_reg();
        emit_line("  " + param_alloca + " = alloca " + inner_llvm_type);
        emit_line("  store " + inner_llvm_type + " " + just_val + ", ptr " + param_alloca);
        locals_[param_name] = VarInfo{param_alloca, inner_llvm_type, nullptr, std::nullopt};
        std::string mapped_val = gen_expr(*closure.body);
        std::string just_end_block = current_block_;
        locals_.erase(param_name);
        emit_line("  br label %" + end_label);

        // nothing block: return default
        emit_line(is_nothing_label + ":");
        current_block_ = is_nothing_label;
        emit_line("  br label %" + end_label);

        emit_line(end_label + ":");
        current_block_ = end_label;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi " + default_type + " [ " + mapped_val + ", %" + just_end_block + " ], [ " + default_val + ", %" + is_nothing_label + " ]");
        last_expr_type_ = default_type;
        return result;
    }

    // Method not handled
    return std::nullopt;
}

} // namespace tml::codegen
