// TML source code formatter - pattern formatting

#include "tml/format/formatter.hpp"

namespace tml::format {

auto Formatter::format_pattern(const parser::Pattern& pattern) -> std::string {
    if (pattern.is<parser::WildcardPattern>()) {
        return "_";
    } else if (pattern.is<parser::IdentPattern>()) {
        const auto& ident = pattern.as<parser::IdentPattern>();
        std::string result;
        if (ident.is_mut)
            result = "mut ";
        result += ident.name;
        if (ident.type_annotation.has_value()) {
            result += ": " + format_type_ptr(ident.type_annotation.value());
        }
        return result;
    } else if (pattern.is<parser::LiteralPattern>()) {
        return std::string(pattern.as<parser::LiteralPattern>().literal.lexeme);
    } else if (pattern.is<parser::TuplePattern>()) {
        const auto& tuple = pattern.as<parser::TuplePattern>();
        std::string result = "(";
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += format_pattern(*tuple.elements[i]);
        }
        result += ")";
        return result;
    } else if (pattern.is<parser::StructPattern>()) {
        const auto& s = pattern.as<parser::StructPattern>();
        std::string result = format_type_path(s.path) + " { ";
        for (size_t i = 0; i < s.fields.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += s.fields[i].first + ": " + format_pattern(*s.fields[i].second);
        }
        if (s.has_rest) {
            if (!s.fields.empty())
                result += ", ";
            result += "..";
        }
        result += " }";
        return result;
    } else if (pattern.is<parser::EnumPattern>()) {
        const auto& e = pattern.as<parser::EnumPattern>();
        std::string result = format_type_path(e.path);
        if (e.payload.has_value()) {
            result += "(";
            for (size_t i = 0; i < e.payload->size(); ++i) {
                if (i > 0)
                    result += ", ";
                result += format_pattern(*(*e.payload)[i]);
            }
            result += ")";
        }
        return result;
    } else if (pattern.is<parser::OrPattern>()) {
        const auto& or_pat = pattern.as<parser::OrPattern>();
        std::string result;
        for (size_t i = 0; i < or_pat.patterns.size(); ++i) {
            if (i > 0)
                result += " | ";
            result += format_pattern(*or_pat.patterns[i]);
        }
        return result;
    } else if (pattern.is<parser::RangePattern>()) {
        const auto& range = pattern.as<parser::RangePattern>();
        std::string result;
        if (range.start.has_value()) {
            result += format_expr(*range.start.value());
        }
        result += range.inclusive ? " through " : " to ";
        if (range.end.has_value()) {
            result += format_expr(*range.end.value());
        }
        return result;
    }

    return "?";
}

} // namespace tml::format
