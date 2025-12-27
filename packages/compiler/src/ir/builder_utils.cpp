#include "tml/ir/ir.hpp"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <sstream>

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
    case parser::BinaryOp::Add:
        return "+";
    case parser::BinaryOp::Sub:
        return "-";
    case parser::BinaryOp::Mul:
        return "*";
    case parser::BinaryOp::Div:
        return "/";
    case parser::BinaryOp::Mod:
        return "%";
    case parser::BinaryOp::Eq:
        return "==";
    case parser::BinaryOp::Ne:
        return "!=";
    case parser::BinaryOp::Lt:
        return "<";
    case parser::BinaryOp::Le:
        return "<=";
    case parser::BinaryOp::Gt:
        return ">";
    case parser::BinaryOp::Ge:
        return ">=";
    case parser::BinaryOp::And:
        return "and";
    case parser::BinaryOp::Or:
        return "or";
    case parser::BinaryOp::BitAnd:
        return "&";
    case parser::BinaryOp::BitOr:
        return "|";
    case parser::BinaryOp::BitXor:
        return "^";
    case parser::BinaryOp::Shl:
        return "<<";
    case parser::BinaryOp::Shr:
        return ">>";
    case parser::BinaryOp::Assign:
        return "=";
    case parser::BinaryOp::AddAssign:
        return "+=";
    case parser::BinaryOp::SubAssign:
        return "-=";
    case parser::BinaryOp::MulAssign:
        return "*=";
    case parser::BinaryOp::DivAssign:
        return "/=";
    case parser::BinaryOp::ModAssign:
        return "%=";
    case parser::BinaryOp::BitAndAssign:
        return "&=";
    case parser::BinaryOp::BitOrAssign:
        return "|=";
    case parser::BinaryOp::BitXorAssign:
        return "^=";
    case parser::BinaryOp::ShlAssign:
        return ">>=";
    default:
        return "?";
    }
}

auto IRBuilder::unary_op_to_string(parser::UnaryOp op) -> std::string {
    switch (op) {
    case parser::UnaryOp::Neg:
        return "-";
    case parser::UnaryOp::Not:
        return "not";
    case parser::UnaryOp::BitNot:
        return "~";
    case parser::UnaryOp::Ref:
        return "ref";
    case parser::UnaryOp::RefMut:
        return "ref-mut";
    case parser::UnaryOp::Deref:
        return "deref";
    default:
        return "?";
    }
}

} // namespace tml::ir
