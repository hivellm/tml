TML_MODULE("compiler")

//! # MIR Text Reader
//!
//! This file parses MIR modules from text format.
//!
//! ## Parsing Features
//!
//! | Element     | Syntax                          |
//! |-------------|---------------------------------|
//! | Function    | `func @name(params) -> ret`     |
//! | Block       | `bb0:` or `entry:`              |
//! | Value       | `%0`, `%name`                   |
//! | Instruction | `%0 = add %1, %2`               |
//! | Return      | `ret %0` or `return`            |
//! | Branch      | `br bb1` or `br %cond, bb1, bb2`|
//!
//! ## Type Parsing
//!
//! - Primitives: i8, i16, i32, i64, f32, f64, bool, str
//! - Pointers: `*T`, `*mut T`
//! - Arrays: `[T; N]`
//! - Named: struct/enum names
//!
//! ## Error Reporting
//!
//! Errors include line number for debugging.

#include "serializer_internal.hpp"

#include <cctype>

namespace tml::mir {

// ============================================================================
// MirTextReader Implementation
// ============================================================================

MirTextReader::MirTextReader(std::istream& in) : in_(in) {}

void MirTextReader::set_error(const std::string& msg) {
    has_error_ = true;
    error_ = "Line " + std::to_string(line_num_) + ": " + msg;
}

auto MirTextReader::next_line() -> bool {
    if (!std::getline(in_, current_line_)) {
        return false;
    }
    ++line_num_;
    pos_ = 0;
    return true;
}

void MirTextReader::skip_whitespace() {
    while (pos_ < current_line_.size() && std::isspace(current_line_[pos_])) {
        ++pos_;
    }
}

auto MirTextReader::peek_char() -> char {
    if (pos_ >= current_line_.size())
        return '\0';
    return current_line_[pos_];
}

auto MirTextReader::read_char() -> char {
    if (pos_ >= current_line_.size())
        return '\0';
    return current_line_[pos_++];
}

auto MirTextReader::read_identifier() -> std::string {
    skip_whitespace();
    std::string result;
    while (pos_ < current_line_.size() &&
           (std::isalnum(current_line_[pos_]) || current_line_[pos_] == '_')) {
        result += current_line_[pos_++];
    }
    return result;
}

auto MirTextReader::read_number() -> int64_t {
    skip_whitespace();
    std::string num_str;
    bool negative = false;

    if (peek_char() == '-') {
        negative = true;
        ++pos_;
    }

    while (pos_ < current_line_.size() && std::isdigit(current_line_[pos_])) {
        num_str += current_line_[pos_++];
    }

    if (num_str.empty())
        return 0;

    int64_t value = std::stoll(num_str);
    return negative ? -value : value;
}

auto MirTextReader::read_string_literal() -> std::string {
    skip_whitespace();
    if (peek_char() != '"')
        return "";
    ++pos_;

    std::string result;
    while (pos_ < current_line_.size() && current_line_[pos_] != '"') {
        if (current_line_[pos_] == '\\' && pos_ + 1 < current_line_.size()) {
            ++pos_;
            switch (current_line_[pos_]) {
            case 'n':
                result += '\n';
                break;
            case 't':
                result += '\t';
                break;
            case '\\':
                result += '\\';
                break;
            case '"':
                result += '"';
                break;
            default:
                result += current_line_[pos_];
            }
        } else {
            result += current_line_[pos_];
        }
        ++pos_;
    }

    if (peek_char() == '"')
        ++pos_;

    return result;
}

auto MirTextReader::expect(char c) -> bool {
    skip_whitespace();
    if (peek_char() == c) {
        ++pos_;
        return true;
    }
    return false;
}

auto MirTextReader::expect(const std::string& s) -> bool {
    skip_whitespace();
    if (current_line_.substr(pos_, s.size()) == s) {
        pos_ += s.size();
        return true;
    }
    return false;
}

auto MirTextReader::read_type() -> MirTypePtr {
    skip_whitespace();
    std::string type_name = read_identifier();

    // Primitive types
    if (type_name == "i8")
        return make_i8_type();
    if (type_name == "i16")
        return make_i16_type();
    if (type_name == "i32")
        return make_i32_type();
    if (type_name == "i64")
        return make_i64_type();
    if (type_name == "f32")
        return make_f32_type();
    if (type_name == "f64")
        return make_f64_type();
    if (type_name == "bool")
        return make_bool_type();
    if (type_name == "unit" || type_name == "()")
        return make_unit_type();
    if (type_name == "str")
        return make_str_type();
    if (type_name == "ptr")
        return make_ptr_type();

    // Pointer type: *T or *mut T
    if (type_name.empty() && peek_char() == '*') {
        ++pos_;
        bool is_mut = false;
        if (current_line_.substr(pos_, 3) == "mut") {
            is_mut = true;
            pos_ += 3;
        }
        auto pointee = read_type();
        return make_pointer_type(pointee, is_mut);
    }

    // Array type: [T; N]
    if (type_name.empty() && peek_char() == '[') {
        ++pos_;
        auto element = read_type();
        expect(';');
        auto size = static_cast<size_t>(read_number());
        expect(']');
        return make_array_type(element, size);
    }

    // Default to struct type
    return make_struct_type(type_name);
}

auto MirTextReader::read_value_ref() -> Value {
    skip_whitespace();
    Value v;
    v.type = make_i32_type(); // Default type

    if (peek_char() == '%') {
        ++pos_;
        v.id = static_cast<uint32_t>(read_number());
    } else {
        // Could be a constant
        v.id = INVALID_VALUE;
    }
    return v;
}

auto MirTextReader::read_function() -> Function {
    Function func;

    // Parse: func @name(params) -> return_type {
    skip_whitespace();
    if (!expect("func"))
        return func;

    skip_whitespace();
    if (peek_char() == '@')
        ++pos_;
    func.name = read_identifier();

    // Parse parameters
    if (expect('(')) {
        while (!expect(')') && pos_ < current_line_.size()) {
            skip_whitespace();
            if (peek_char() == '%')
                ++pos_;
            std::string param_name = read_identifier();
            if (expect(':')) {
                auto type = read_type();
                FunctionParam param;
                param.name = param_name;
                param.type = type;
                param.value_id = static_cast<uint32_t>(func.params.size());
                func.params.push_back(param);
            }
            expect(',');
        }
    }

    // Parse return type
    if (expect("->")) {
        func.return_type = read_type();
    } else {
        func.return_type = make_unit_type();
    }

    return func;
}

auto MirTextReader::read_block(Function& func) -> BasicBlock {
    BasicBlock block;

    // Parse: bb0: or entry:
    skip_whitespace();
    std::string label = read_identifier();

    // Remove trailing colon
    if (!label.empty() && label.back() == ':') {
        label.pop_back();
    } else if (peek_char() == ':') {
        ++pos_;
    }

    // Extract block ID from label (bb0 -> 0, bb1 -> 1, etc.)
    if (label.substr(0, 2) == "bb") {
        try {
            block.id = static_cast<uint32_t>(std::stoul(label.substr(2)));
        } catch (...) {
            block.id = func.next_block_id++;
        }
    } else {
        block.id = func.next_block_id++;
    }
    block.name = label;

    return block;
}

auto MirTextReader::read_instruction() -> InstructionData {
    InstructionData inst;
    inst.result = INVALID_VALUE;
    inst.type = make_unit_type();

    skip_whitespace();

    // Check for result assignment: %0 = ...
    if (peek_char() == '%') {
        ++pos_;
        inst.result = static_cast<uint32_t>(read_number());
        skip_whitespace();
        if (!expect('=')) {
            return inst;
        }
    }

    skip_whitespace();
    std::string opcode = read_identifier();

    // Parse different instruction types
    if (opcode == "add" || opcode == "sub" || opcode == "mul" || opcode == "div" ||
        opcode == "mod" || opcode == "eq" || opcode == "ne" || opcode == "lt" || opcode == "le" ||
        opcode == "gt" || opcode == "ge" || opcode == "and" || opcode == "or" || opcode == "xor" ||
        opcode == "shl" || opcode == "shr") {
        BinaryInst bin;
        if (opcode == "add")
            bin.op = BinOp::Add;
        else if (opcode == "sub")
            bin.op = BinOp::Sub;
        else if (opcode == "mul")
            bin.op = BinOp::Mul;
        else if (opcode == "div")
            bin.op = BinOp::Div;
        else if (opcode == "mod")
            bin.op = BinOp::Mod;
        else if (opcode == "eq")
            bin.op = BinOp::Eq;
        else if (opcode == "ne")
            bin.op = BinOp::Ne;
        else if (opcode == "lt")
            bin.op = BinOp::Lt;
        else if (opcode == "le")
            bin.op = BinOp::Le;
        else if (opcode == "gt")
            bin.op = BinOp::Gt;
        else if (opcode == "ge")
            bin.op = BinOp::Ge;
        else if (opcode == "and")
            bin.op = BinOp::And;
        else if (opcode == "or")
            bin.op = BinOp::Or;
        else if (opcode == "xor")
            bin.op = BinOp::BitXor;
        else if (opcode == "shl")
            bin.op = BinOp::Shl;
        else if (opcode == "shr")
            bin.op = BinOp::Shr;

        bin.left = read_value_ref();
        expect(',');
        bin.right = read_value_ref();
        inst.inst = bin;
    } else if (opcode == "neg" || opcode == "not" || opcode == "bitnot") {
        UnaryInst unary;
        if (opcode == "neg")
            unary.op = UnaryOp::Neg;
        else if (opcode == "not")
            unary.op = UnaryOp::Not;
        else
            unary.op = UnaryOp::BitNot;
        unary.operand = read_value_ref();
        inst.inst = unary;
    } else if (opcode == "load") {
        LoadInst load;
        load.ptr = read_value_ref();
        inst.inst = load;
    } else if (opcode == "store") {
        StoreInst store;
        store.value = read_value_ref();
        expect(',');
        store.ptr = read_value_ref();
        inst.inst = store;
    } else if (opcode == "alloca") {
        AllocaInst alloca_inst;
        alloca_inst.alloc_type = read_type();
        alloca_inst.name = "";
        inst.inst = alloca_inst;
    } else if (opcode == "call") {
        CallInst call;
        if (peek_char() == '@')
            ++pos_;
        call.func_name = read_identifier();
        if (expect('(')) {
            while (!expect(')') && pos_ < current_line_.size()) {
                call.args.push_back(read_value_ref());
                expect(',');
            }
        }
        call.return_type = make_unit_type();
        inst.inst = call;
    } else if (opcode == "const") {
        ConstantInst constant;
        skip_whitespace();
        if (peek_char() == '"') {
            constant.value = ConstString{read_string_literal()};
            inst.type = make_str_type();
        } else if (current_line_.substr(pos_, 4) == "true") {
            pos_ += 4;
            constant.value = ConstBool{true};
            inst.type = make_bool_type();
        } else if (current_line_.substr(pos_, 5) == "false") {
            pos_ += 5;
            constant.value = ConstBool{false};
            inst.type = make_bool_type();
        } else if (current_line_.substr(pos_, 4) == "unit") {
            pos_ += 4;
            constant.value = ConstUnit{};
            inst.type = make_unit_type();
        } else {
            int64_t val = read_number();
            constant.value = ConstInt{val, true, 32};
            inst.type = make_i32_type();
        }
        inst.inst = constant;
    } else if (opcode == "ret" || opcode == "return") {
        // Terminator, not regular instruction
        // Will be handled separately
    } else if (opcode == "br" || opcode == "branch") {
        // Terminator
    }

    return inst;
}

auto MirTextReader::read_terminator() -> std::optional<Terminator> {
    skip_whitespace();
    std::string opcode = read_identifier();

    if (opcode == "ret" || opcode == "return") {
        ReturnTerm ret;
        skip_whitespace();
        if (pos_ < current_line_.size() && peek_char() != '\0' && peek_char() != ';') {
            ret.value = read_value_ref();
        }
        return ret;
    } else if (opcode == "br" || opcode == "branch") {
        skip_whitespace();
        if (current_line_.substr(pos_, 2) == "if" ||
            (peek_char() == '%' || std::isdigit(peek_char()))) {
            // Conditional branch: br if %cond, bb1, bb2
            // or: br %cond, bb1, bb2
            if (current_line_.substr(pos_, 2) == "if") {
                pos_ += 2;
            }
            CondBranchTerm cond_term;
            cond_term.condition = read_value_ref();
            expect(',');
            skip_whitespace();
            if (current_line_.substr(pos_, 2) == "bb") {
                pos_ += 2;
                cond_term.true_block = static_cast<uint32_t>(read_number());
            }
            expect(',');
            skip_whitespace();
            if (current_line_.substr(pos_, 2) == "bb") {
                pos_ += 2;
                cond_term.false_block = static_cast<uint32_t>(read_number());
            }
            return cond_term;
        } else {
            // Unconditional branch: br bb1
            BranchTerm branch;
            skip_whitespace();
            if (current_line_.substr(pos_, 2) == "bb") {
                pos_ += 2;
                branch.target = static_cast<uint32_t>(read_number());
            }
            return branch;
        }
    } else if (opcode == "unreachable") {
        return UnreachableTerm{};
    }

    return std::nullopt;
}

auto MirTextReader::read_module() -> Module {
    Module module;

    // Parse module structure from text format
    // Format:
    // ; MIR Module: name
    // func @func_name(params) -> ret_type {
    //   bb0:
    //     %0 = ...
    //     ret %0
    // }

    Function* current_func = nullptr;
    BasicBlock* current_block = nullptr;
    bool in_function = false;

    while (next_line()) {
        skip_whitespace();

        // Skip empty lines and comments
        if (pos_ >= current_line_.size() || peek_char() == '\0') {
            continue;
        }

        if (peek_char() == ';') {
            // Comment line - check for module name
            if (current_line_.find("; MIR Module:") != std::string::npos) {
                pos_ = current_line_.find(":") + 1;
                skip_whitespace();
                module.name = read_identifier();
            }
            continue;
        }

        // Function definition
        if (current_line_.substr(pos_, 4) == "func") {
            auto func = read_function();
            module.functions.push_back(std::move(func));
            current_func = &module.functions.back();
            in_function = true;
            continue;
        }

        // End of function
        if (peek_char() == '}') {
            in_function = false;
            current_func = nullptr;
            current_block = nullptr;
            continue;
        }

        if (!in_function || !current_func) {
            continue;
        }

        // Block label
        if (std::isalpha(peek_char()) && current_line_.find(':') != std::string::npos) {
            auto block = read_block(*current_func);
            current_func->blocks.push_back(std::move(block));
            current_block = &current_func->blocks.back();
            continue;
        }

        // Instruction or terminator
        if (current_block) {
            // Check if it's a terminator
            std::string first_word;
            size_t saved_pos = pos_;
            if (peek_char() == '%') {
                // Skip result assignment
                while (pos_ < current_line_.size() && peek_char() != '=')
                    ++pos_;
                if (peek_char() == '=')
                    ++pos_;
                skip_whitespace();
            }
            first_word = read_identifier();
            pos_ = saved_pos; // restore

            if (first_word == "ret" || first_word == "return" || first_word == "br" ||
                first_word == "branch" || first_word == "unreachable") {
                auto term = read_terminator();
                if (term) {
                    current_block->terminator = *term;
                }
            } else {
                auto inst = read_instruction();
                if (inst.result != INVALID_VALUE ||
                    !std::holds_alternative<BinaryInst>(inst.inst) ||
                    std::get<BinaryInst>(inst.inst).op != BinOp::Add) {
                    current_block->instructions.push_back(std::move(inst));
                }
            }
        }
    }

    return module;
}

} // namespace tml::mir
