#include "tml/ir/ir.hpp"

#include <algorithm>
namespace tml::ir {

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
        if (p.pattern->template is<parser::IdentPattern>()) {
            param.name = p.pattern->template as<parser::IdentPattern>().name;
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
            if (p.pattern->template is<parser::IdentPattern>()) {
                param.name = p.pattern->template as<parser::IdentPattern>().name;
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
            if (p.pattern->template is<parser::IdentPattern>()) {
                param.name = p.pattern->template as<parser::IdentPattern>().name;
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


} // namespace tml::ir
