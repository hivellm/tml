TML_MODULE("compiler")

#include "lexer/lexer.hpp"
#include "log/log.hpp"
#include "parser/parser.hpp"
#include "types/env.hpp"
#include "types/module.hpp"
#include "types/parsed_module_file.hpp"

#include <filesystem>
#include <fstream>

namespace tml::types {

static types::TypePtr resolve_simple_type(const parser::Type& type) {
    if (type.is<parser::NamedType>()) {
        const auto& named = type.as<parser::NamedType>();
        // Build full path name (e.g., "I::Item" for associated types)
        std::string name;
        for (size_t i = 0; i < named.path.segments.size(); ++i) {
            if (i > 0)
                name += "::";
            name += named.path.segments[i];
        }

        // Primitive types
        if (name == "I8")
            return make_primitive(PrimitiveKind::I8);
        if (name == "I16")
            return make_primitive(PrimitiveKind::I16);
        if (name == "I32")
            return make_primitive(PrimitiveKind::I32);
        if (name == "I64")
            return make_primitive(PrimitiveKind::I64);
        if (name == "I128")
            return make_primitive(PrimitiveKind::I128);
        if (name == "U8")
            return make_primitive(PrimitiveKind::U8);
        if (name == "U16")
            return make_primitive(PrimitiveKind::U16);
        if (name == "U32")
            return make_primitive(PrimitiveKind::U32);
        if (name == "U64")
            return make_primitive(PrimitiveKind::U64);
        if (name == "U128")
            return make_primitive(PrimitiveKind::U128);
        if (name == "F32")
            return make_primitive(PrimitiveKind::F32);
        if (name == "F64")
            return make_primitive(PrimitiveKind::F64);
        if (name == "Bool")
            return make_primitive(PrimitiveKind::Bool);
        if (name == "Char")
            return make_primitive(PrimitiveKind::Char);
        if (name == "Str")
            return make_primitive(PrimitiveKind::Str);
        if (name == "Unit")
            return make_unit();
        // Platform-sized types (map to 64-bit on 64-bit platforms)
        if (name == "Usize")
            return make_primitive(PrimitiveKind::U64); // Platform-sized unsigned
        if (name == "Isize")
            return make_primitive(PrimitiveKind::I64); // Platform-sized signed

        // std::file types
        if (name == "File")
            return std::make_shared<Type>(Type{NamedType{"File", "std::file", {}}});
        if (name == "Path")
            return std::make_shared<Type>(Type{NamedType{"Path", "std::file", {}}});

        // Other non-primitive types - resolve any generic type arguments
        std::vector<TypePtr> type_args;
        if (named.generics.has_value()) {
            for (const auto& arg : named.generics->args) {
                // Only handle type arguments for now (not const generics)
                if (arg.is_type()) {
                    type_args.push_back(resolve_simple_type(*arg.as_type()));
                }
            }
        }
        return std::make_shared<Type>(Type{NamedType{name, "", std::move(type_args)}});
    } else if (type.is<parser::RefType>()) {
        const auto& ref = type.as<parser::RefType>();
        auto inner = resolve_simple_type(*ref.inner);
        return std::make_shared<Type>(
            Type{RefType{.is_mut = ref.is_mut, .inner = inner, .lifetime = ref.lifetime}});
    } else if (type.is<parser::FuncType>()) {
        const auto& func_type = type.as<parser::FuncType>();
        std::vector<TypePtr> param_types;
        for (const auto& param : func_type.params) {
            param_types.push_back(resolve_simple_type(*param));
        }
        TypePtr return_type = make_unit();
        if (func_type.return_type) {
            return_type = resolve_simple_type(*func_type.return_type);
        }
        return std::make_shared<Type>(Type{FuncType{std::move(param_types), return_type, false}});
    } else if (type.is<parser::TupleType>()) {
        const auto& tuple_type = type.as<parser::TupleType>();
        std::vector<TypePtr> element_types;
        for (const auto& elem : tuple_type.elements) {
            element_types.push_back(resolve_simple_type(*elem));
        }
        return make_tuple(std::move(element_types));
    } else if (type.is<parser::PtrType>()) {
        const auto& ptr = type.as<parser::PtrType>();
        auto inner = resolve_simple_type(*ptr.inner);
        return std::make_shared<Type>(Type{PtrType{ptr.is_mut, inner}});
    } else if (type.is<parser::ArrayType>()) {
        const auto& arr = type.as<parser::ArrayType>();
        auto element = resolve_simple_type(*arr.element);
        // Extract array size from literal or const generic param
        size_t arr_size = 0;
        std::string const_generic_param;
        if (arr.size) {
            if (arr.size->is<parser::LiteralExpr>()) {
                const auto& lit = arr.size->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                    arr_size = static_cast<size_t>(lit.token.int_value().value);
                }
            } else if (arr.size->is<parser::IdentExpr>()) {
                // Const generic parameter reference (e.g., N in [T; N])
                const_generic_param = arr.size->as<parser::IdentExpr>().name;
            }
        }
        auto result = make_array(element, arr_size);
        if (!const_generic_param.empty()) {
            result->as<ArrayType>().const_generic_param = const_generic_param;
        }
        return result;
    } else if (type.is<parser::SliceType>()) {
        const auto& slice = type.as<parser::SliceType>();
        auto element = resolve_simple_type(*slice.element);
        return make_slice(element);
    } else if (type.is<parser::DynType>()) {
        // Handle dyn behavior types (e.g., dyn Error)
        const auto& dyn = type.as<parser::DynType>();
        std::string behavior_name;
        if (!dyn.behavior.segments.empty()) {
            behavior_name = dyn.behavior.segments.back();
        }
        // Resolve type args if any
        std::vector<TypePtr> type_args;
        if (dyn.generics.has_value()) {
            for (const auto& arg : dyn.generics->args) {
                if (arg.is_type()) {
                    type_args.push_back(resolve_simple_type(*arg.as_type()));
                }
            }
        }
        return std::make_shared<Type>(
            Type{DynBehaviorType{behavior_name, std::move(type_args), dyn.is_mut}});
    } else if (type.is<parser::ImplBehaviorType>()) {
        // impl Behavior[T] return types — convert to the equivalent semantic type.
        const auto& impl = type.as<parser::ImplBehaviorType>();
        std::string behavior_name;
        if (!impl.behavior.segments.empty()) {
            behavior_name = impl.behavior.segments.back();
        }
        std::vector<TypePtr> type_args;
        if (impl.generics.has_value()) {
            for (const auto& arg : impl.generics->args) {
                if (arg.is_type()) {
                    type_args.push_back(resolve_simple_type(*arg.as_type()));
                }
            }
        }
        return std::make_shared<Type>(Type{ImplBehaviorType{behavior_name, std::move(type_args)}});
    } else if (type.is<parser::InferType>()) {
        // _ infer marker in a declaration position: we are outside body-checking context
        // and have no type-variable counter, so record a named sentinel "_" that the body
        // checker will resolve to a type error when it encounters it in a call position.
        return std::make_shared<Type>(Type{NamedType{"_", "", {}}});
    }

    // All known parser Type variants are handled above. Reaching here means a new
    // variant was added to the parser AST without updating this function.
    // T081-UNRESOLVED-TYPE-IN-METHOD-SIG: emit an error-level diagnostic so the gap
    // is visible immediately, and return Never so downstream callers propagate a type
    // error rather than silently producing wrong types in method signatures.
    TML_LOG_ERROR("types", "resolve_simple_type: unhandled parser type variant (index "
                               << type.kind.index()
                               << ") [T081-UNRESOLVED-TYPE-IN-METHOD-SIG] — update "
                                  "env_module_load_decls.cpp to handle this variant.");
    return make_never();
}

void TypeEnv::extract_module_declarations(const std::string& module_path,
                                          const std::string& file_path,
                                          const std::vector<struct ParsedModuleFile>& all_parsed,
                                          Module& mod) {
    SourceSpan builtin_span{};

    // Extract public declarations from all parsed files
    for (const auto& parsed_file : all_parsed) {
        const auto& decls = parsed_file.decls;
        bool is_from_mod_file = parsed_file.is_from_mod_file;
        for (const auto& decl : decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func = decl->as<parser::FuncDecl>();

                // Register ALL functions (including private ones) so that codegen can
                // resolve parameter types for intra-module calls during lazy library
                // generation. Without this, private helper functions called from public
                // functions in the same module lose their type signatures, causing
                // ref-param struct values to be passed by value instead of by pointer.
                // The type checker already enforces visibility for user-facing imports.

                // Convert parameter types
                std::vector<types::TypePtr> param_types;
                for (const auto& param : func.params) {
                    if (param.type) {
                        param_types.push_back(resolve_simple_type(*param.type));
                    }
                }

                // Convert return type
                types::TypePtr return_type;
                if (func.return_type.has_value()) {
                    const auto& type_ptr = func.return_type.value();
                    return_type = resolve_simple_type(*type_ptr);
                } else {
                    return_type = make_unit();
                }

                // Extract type parameters from generic declaration
                std::vector<std::string> type_params;
                for (const auto& gp : func.generics) {
                    type_params.push_back(gp.name);
                }

                // Create function signature
                FuncSig sig{.name = func.name,
                            .params = param_types,
                            .return_type = return_type,
                            .type_params = std::move(type_params),
                            .is_async = false,
                            .span = builtin_span,
                            .stability = StabilityLevel::Stable,
                            .deprecated_message = "",
                            .since_version = "1.0",
                            .where_constraints = {},
                            .is_lowlevel = func.is_unsafe};

                // Preserve @extern information for codegen
                if (func.extern_abi.has_value()) {
                    sig.extern_abi = func.extern_abi;
                    sig.extern_name = func.extern_name;
                }

                mod.functions[func.name] = sig;
            } else if (decl->is<parser::StructDecl>()) {
                const auto& struct_decl = decl->as<parser::StructDecl>();

                // Convert fields
                std::vector<StructFieldDef> fields;
                for (const auto& field : struct_decl.fields) {
                    if (field.type) {
                        StructFieldDef fdef;
                        fdef.name = field.name;
                        fdef.type = resolve_simple_type(*field.type);
                        fields.push_back(std::move(fdef));
                    }
                }

                // Extract type params
                std::vector<std::string> type_params;
                for (const auto& param : struct_decl.generics) {
                    type_params.push_back(param.name);
                }

                // Check for @simd and @interior_mutable decorators
                bool is_simd = false;
                bool is_interior_mutable = false;
                for (const auto& deco : struct_decl.decorators) {
                    if (deco.name == "simd")
                        is_simd = true;
                    if (deco.name == "interior_mutable")
                        is_interior_mutable = true;
                }

                // Create struct definition
                StructDef struct_def{.name = struct_decl.name,
                                     .type_params = std::move(type_params),
                                     .const_params = {},
                                     .fields = std::move(fields),
                                     .span = struct_decl.span,
                                     .is_interior_mutable = is_interior_mutable,
                                     .is_simd = is_simd};

                // Store in appropriate map based on visibility
                if (struct_decl.vis == parser::Visibility::Public) {
                    mod.structs[struct_decl.name] = std::move(struct_def);
                    TML_DEBUG_LN("[MODULE] Registered struct: " << struct_decl.name << " in module "
                                                                << module_path);
                } else {
                    // Store internal structs for use by the module's own impl methods
                    mod.internal_structs[struct_decl.name] = std::move(struct_def);
                    TML_DEBUG_LN("[MODULE] Registered internal struct: "
                                 << struct_decl.name << " in module " << module_path);
                }
            } else if (decl->is<parser::EnumDecl>()) {
                const auto& enum_decl = decl->as<parser::EnumDecl>();

                // Convert variants
                std::vector<std::pair<std::string, std::vector<TypePtr>>> variants;
                for (const auto& variant : enum_decl.variants) {
                    std::vector<TypePtr> payload_types;
                    // Handle tuple fields (e.g., Some(T))
                    if (variant.tuple_fields.has_value()) {
                        for (const auto& type_ptr : variant.tuple_fields.value()) {
                            payload_types.push_back(resolve_simple_type(*type_ptr));
                        }
                    }
                    // Handle struct fields (e.g., Point { x: I32, y: I32 })
                    if (variant.struct_fields.has_value()) {
                        for (const auto& field : variant.struct_fields.value()) {
                            if (field.type) {
                                payload_types.push_back(resolve_simple_type(*field.type));
                            }
                        }
                    }
                    variants.emplace_back(variant.name, std::move(payload_types));
                }

                // Extract type params
                std::vector<std::string> type_params;
                for (const auto& param : enum_decl.generics) {
                    type_params.push_back(param.name);
                }

                // Create enum definition
                EnumDef enum_def{.name = enum_decl.name,
                                 .type_params = std::move(type_params),
                                 .const_params = {},
                                 .variants = std::move(variants),
                                 .span = enum_decl.span};

                // Store in appropriate map based on visibility
                if (enum_decl.vis == parser::Visibility::Public) {
                    mod.enums[enum_decl.name] = std::move(enum_def);
                    TML_DEBUG_LN("[MODULE] Registered enum: " << enum_decl.name << " in module "
                                                              << module_path);
                } else {
                    // Store internal enums for use by the module's own impl methods
                    mod.internal_enums[enum_decl.name] = std::move(enum_def);
                    TML_DEBUG_LN("[MODULE] Registered internal enum: "
                                 << enum_decl.name << " in module " << module_path);
                }
            } else if (decl->is<parser::ImplDecl>()) {
                const auto& impl_decl = decl->as<parser::ImplDecl>();

                // Get the type name being implemented from self_type
                std::string type_name;
                if (impl_decl.self_type && impl_decl.self_type->is<parser::NamedType>()) {
                    const auto& named = impl_decl.self_type->as<parser::NamedType>();
                    if (!named.path.segments.empty()) {
                        type_name = named.path.segments.back();
                    }
                }

                if (type_name.empty()) {
                    continue; // Skip if we couldn't determine the type name
                }

                // Register behavior implementation (e.g., impl[T] Drop for MutexGuard[T])
                // This is critical for is_trivially_destructible() to detect Drop impls
                // on imported library types, so that drop calls are emitted at scope exit.
                if (impl_decl.trait_type && impl_decl.trait_type->is<parser::NamedType>()) {
                    const auto& trait_named = impl_decl.trait_type->as<parser::NamedType>();
                    if (!trait_named.path.segments.empty()) {
                        std::string behavior_name = trait_named.path.segments.back();
                        register_impl(type_name, behavior_name);
                        mod.behavior_impls[type_name].push_back(behavior_name);
                        TML_DEBUG_LN("[MODULE] Registered behavior impl: "
                                     << type_name << " implements " << behavior_name
                                     << " in module " << module_path);
                    }
                }

                // Check if this is a behavior impl (has trait_type)
                bool is_behavior_impl = impl_decl.trait_type != nullptr;

                // Extract impl self-type args for specialized impls like impl[T] Pin[ref T].
                // These patterns are needed to correctly map type params at call sites.
                std::vector<types::TypePtr> impl_self_type_args;
                if (impl_decl.self_type && impl_decl.self_type->is<parser::NamedType>()) {
                    const auto& self_named = impl_decl.self_type->as<parser::NamedType>();
                    if (self_named.generics.has_value()) {
                        for (const auto& arg : self_named.generics->args) {
                            if (arg.is_type()) {
                                impl_self_type_args.push_back(resolve_simple_type(*arg.as_type()));
                            }
                        }
                    }
                }

                // Extract methods from impl block (methods is std::vector<FuncDecl>)
                for (const auto& func : impl_decl.methods) {
                    // Include ALL methods (public and private) from generic impl blocks.
                    // Private methods must be registered so they can be found during
                    // generic method instantiation (e.g., sort() calling quicksort()).
                    // For non-generic impls, still skip non-public methods.
                    bool is_generic_impl = !impl_decl.generics.empty();
                    if (func.vis != parser::Visibility::Public && !is_behavior_impl &&
                        !is_generic_impl) {
                        continue;
                    }

                    // Convert parameter types
                    std::vector<types::TypePtr> param_types;
                    for (const auto& param : func.params) {
                        if (param.type) {
                            param_types.push_back(resolve_simple_type(*param.type));
                        }
                    }

                    // Convert return type
                    types::TypePtr return_type;
                    if (func.return_type.has_value()) {
                        return_type = resolve_simple_type(*func.return_type.value());
                    } else {
                        return_type = make_unit();
                    }

                    // Create qualified function name: Type::method
                    std::string qualified_name = type_name + "::" + func.name;

                    // For specialized impls (e.g., impl[T] Pin[Heap[T]]), build a
                    // discriminated key to avoid name collisions with other impls
                    // on the same base type (e.g., impl[T] Pin[ref T]).
                    // The discriminator is extracted from the first non-bare-param
                    // type arg: Heap[T]→"Heap", ref T→"ref", mut ref T→"mut_ref".
                    std::string spec_qualified_name;
                    if (!impl_self_type_args.empty()) {
                        // Collect impl generic param names
                        std::set<std::string> impl_params;
                        for (const auto& gp : impl_decl.generics) {
                            impl_params.insert(gp.name);
                        }
                        // Check if any type arg is specialized (not a bare param)
                        std::string discriminator;
                        for (const auto& arg : impl_self_type_args) {
                            if (arg->is<NamedType>()) {
                                const auto& n = arg->as<NamedType>();
                                if (impl_params.count(n.name) && n.type_args.empty()) {
                                    continue; // Bare type param, skip
                                }
                                // Specialized: use the wrapper name as discriminator
                                discriminator = n.name;
                                break;
                            } else if (arg->is<RefType>()) {
                                const auto& r = arg->as<RefType>();
                                discriminator = r.is_mut ? "mut_ref" : "ref";
                                break;
                            }
                        }
                        if (!discriminator.empty()) {
                            spec_qualified_name =
                                type_name + "[" + discriminator + "]::" + func.name;
                        }
                    }

                    // Extract type parameters from impl block and method's generic declaration
                    // Impl block generics (e.g., T in impl[T] Cell[T]) are needed for methods
                    // that use the type parameter without declaring their own generics
                    std::vector<std::string> method_type_params;
                    for (const auto& gp : impl_decl.generics) {
                        method_type_params.push_back(gp.name);
                    }
                    // Then add method's own type params (if any)
                    for (const auto& gp : func.generics) {
                        method_type_params.push_back(gp.name);
                    }

                    FuncSig sig{.name = qualified_name,
                                .params = param_types,
                                .return_type = return_type,
                                .type_params = std::move(method_type_params),
                                .is_async = false,
                                .span = builtin_span,
                                .stability = StabilityLevel::Stable,
                                .deprecated_message = "",
                                .since_version = "1.0",
                                .where_constraints = {},
                                .is_lowlevel = func.is_unsafe};
                    sig.impl_self_type_args = impl_self_type_args;

                    // Register under the base key (may overwrite a previous impl's entry)
                    mod.functions[qualified_name] = sig;

                    // Also register under the specialized key so both impls survive
                    // in the flat Module::functions map.
                    if (!spec_qualified_name.empty()) {
                        FuncSig spec_sig = sig; // copy
                        spec_sig.name = spec_qualified_name;
                        mod.functions[spec_qualified_name] = std::move(spec_sig);
                    }

                    TML_DEBUG_LN("[MODULE] Registered impl method: "
                                 << qualified_name << " in module " << module_path);
                }

                // Extract constants from impl block (e.g., const MIN: I32 = ...)
                for (const auto& const_decl : impl_decl.constants) {
                    // Only include public constants
                    if (const_decl.vis != parser::Visibility::Public) {
                        continue;
                    }

                    // Create qualified constant name: Type::CONST
                    std::string qualified_name = type_name + "::" + const_decl.name;
                    std::string tml_type;
                    std::string value = try_extract_module_const_value(const_decl, tml_type);
                    if (!value.empty()) {
                        mod.constants[qualified_name] = {value, tml_type};
                        TML_DEBUG_LN("[MODULE] Registered impl constant: "
                                     << qualified_name << " = " << value << " in module "
                                     << module_path);
                    }
                }
            } else if (decl->is<parser::ConstDecl>()) {
                // Handle module-level constants (not inside impl blocks)
                const auto& const_decl = decl->as<parser::ConstDecl>();
                std::string tml_type;
                std::string value = try_extract_module_const_value(const_decl, tml_type);
                if (!value.empty()) {
                    mod.constants[const_decl.name] = {value, tml_type};
                    TML_DEBUG_LN("[MODULE] Registered module constant: " << const_decl.name << " = "
                                                                         << value << " in module "
                                                                         << module_path);
                }
            } else if (decl->is<parser::InterfaceDecl>()) {
                // Handle interface declarations (OOP interfaces)
                const auto& iface_decl = decl->as<parser::InterfaceDecl>();

                // Only include public interfaces
                if (iface_decl.vis != parser::Visibility::Public) {
                    continue;
                }

                // Extract type params
                std::vector<std::string> type_params;
                for (const auto& param : iface_decl.generics) {
                    if (!param.is_const) {
                        type_params.push_back(param.name);
                    }
                }

                // Extract extended interfaces
                std::vector<std::string> extends;
                for (const auto& ext : iface_decl.extends) {
                    if (!ext.segments.empty()) {
                        extends.push_back(ext.segments.back());
                    }
                }

                // Build method signatures
                std::vector<InterfaceMethodDef> methods;
                for (const auto& method : iface_decl.methods) {
                    InterfaceMethodDef method_def;
                    method_def.is_static = method.is_static;
                    method_def.has_default = method.default_body.has_value();

                    // Build signature
                    FuncSig sig;
                    sig.name = method.name;
                    sig.is_async = false;
                    sig.span = method.span;

                    // Convert param types (simplified - skip 'this')
                    for (const auto& param : method.params) {
                        if (param.pattern && param.pattern->is<parser::IdentPattern>() &&
                            param.pattern->as<parser::IdentPattern>().name == "this") {
                            continue;
                        }
                        if (param.type) {
                            sig.params.push_back(resolve_simple_type(*param.type));
                        }
                    }

                    // Return type
                    if (method.return_type.has_value()) {
                        sig.return_type = resolve_simple_type(*method.return_type.value());
                    } else {
                        sig.return_type = make_unit();
                    }

                    method_def.sig = sig;
                    methods.push_back(method_def);
                }

                // Create interface definition
                InterfaceDef iface_def;
                iface_def.name = iface_decl.name;
                iface_def.type_params = std::move(type_params);
                iface_def.extends = std::move(extends);
                iface_def.methods = std::move(methods);
                iface_def.span = iface_decl.span;

                mod.interfaces[iface_decl.name] = std::move(iface_def);
                TML_DEBUG_LN("[MODULE] Registered interface: " << iface_decl.name << " in module "
                                                               << module_path);
            } else if (decl->is<parser::ClassDecl>()) {
                // Handle class declarations (OOP classes)
                const auto& class_decl = decl->as<parser::ClassDecl>();

                // Only include public classes
                if (class_decl.vis != parser::Visibility::Public) {
                    continue;
                }

                // Extract type params
                std::vector<std::string> type_params;
                for (const auto& param : class_decl.generics) {
                    if (!param.is_const) {
                        type_params.push_back(param.name);
                    }
                }

                // Extract base class
                std::optional<std::string> base_class;
                if (class_decl.extends.has_value()) {
                    const auto& ext = class_decl.extends.value();
                    if (!ext.segments.empty()) {
                        base_class = ext.segments.back();
                    }
                }

                // Extract implemented interfaces
                std::vector<std::string> interfaces;
                for (const auto& iface_type : class_decl.implements) {
                    if (auto* named = std::get_if<parser::NamedType>(&iface_type->kind)) {
                        if (!named->path.segments.empty()) {
                            interfaces.push_back(named->path.segments.back());
                        }
                    }
                }

                // Extract fields
                std::vector<ClassFieldDef> fields;
                for (const auto& field : class_decl.fields) {
                    ClassFieldDef field_def;
                    field_def.name = field.name;
                    field_def.is_static = field.is_static;
                    if (field.type) {
                        field_def.type = resolve_simple_type(*field.type);
                    }
                    fields.push_back(field_def);
                }

                // Extract methods
                std::vector<ClassMethodDef> methods;
                for (const auto& method : class_decl.methods) {
                    ClassMethodDef method_def;
                    method_def.is_static = method.is_static;
                    method_def.is_virtual = method.is_virtual;
                    method_def.is_override = method.is_override;
                    method_def.is_abstract = method.is_abstract;
                    method_def.is_final = method.is_final;
                    method_def.vis = MemberVisibility::Public; // Default to public

                    // Build signature
                    FuncSig sig;
                    sig.name = method.name;
                    sig.is_async = false;
                    sig.span = method.span;

                    // Convert param types (skip 'this')
                    for (const auto& param : method.params) {
                        if (param.pattern && param.pattern->is<parser::IdentPattern>() &&
                            param.pattern->as<parser::IdentPattern>().name == "this") {
                            continue;
                        }
                        if (param.type) {
                            sig.params.push_back(resolve_simple_type(*param.type));
                        }
                    }

                    // Return type
                    if (method.return_type.has_value()) {
                        sig.return_type = resolve_simple_type(*method.return_type.value());
                    } else {
                        sig.return_type = make_unit();
                    }

                    method_def.sig = sig;
                    methods.push_back(method_def);
                }

                // Create class definition
                ClassDef class_def;
                class_def.name = class_decl.name;
                class_def.type_params = std::move(type_params);
                class_def.base_class = base_class;
                class_def.interfaces = std::move(interfaces);
                class_def.fields = std::move(fields);
                class_def.methods = std::move(methods);
                class_def.is_abstract = class_decl.is_abstract;
                class_def.is_sealed = class_decl.is_sealed;
                class_def.span = class_decl.span;

                mod.classes[class_decl.name] = std::move(class_def);
                TML_DEBUG_LN("[MODULE] Registered class: " << class_decl.name << " in module "
                                                           << module_path);
            } else if (decl->is<parser::TypeAliasDecl>()) {
                const auto& alias_decl = decl->as<parser::TypeAliasDecl>();

                // Only include public type aliases
                if (alias_decl.vis != parser::Visibility::Public) {
                    continue;
                }

                // Resolve the alias target type
                if (alias_decl.type) {
                    auto resolved = resolve_simple_type(*alias_decl.type);
                    mod.type_aliases[alias_decl.name] = resolved;

                    // Store generic parameter names
                    std::vector<std::string> generic_params;
                    for (const auto& gp : alias_decl.generics) {
                        generic_params.push_back(gp.name);
                    }
                    if (!generic_params.empty()) {
                        mod.type_alias_generics[alias_decl.name] = std::move(generic_params);
                    }
                    TML_DEBUG_LN("[MODULE] Registered type alias: "
                                 << alias_decl.name << " in module " << module_path);
                }
            } else if (decl->is<parser::ModDecl>()) {
                const auto& mod_decl = decl->as<parser::ModDecl>();

                // Only process public mod declarations
                if (mod_decl.vis != parser::Visibility::Public) {
                    continue;
                }

                // Register submodule path: e.g., for "pub mod traits" in core::iter,
                // the submodule path is "core::iter::traits"
                std::string submod_path = module_path + "::" + mod_decl.name;
                mod.submodules[mod_decl.name] = submod_path;
                TML_DEBUG_LN("[MODULE] Registered submodule: " << mod_decl.name << " -> "
                                                               << submod_path);
            } else if (decl->is<parser::UseDecl>()) {
                const auto& use_decl = decl->as<parser::UseDecl>();

                // Build the source module path from use declaration
                std::string use_path;
                for (size_t i = 0; i < use_decl.path.segments.size(); ++i) {
                    if (i > 0)
                        use_path += "::";
                    use_path += use_decl.path.segments[i];
                }

                // Handle relative paths: if path doesn't start with known prefix,
                // treat it as relative to current module.
                // But don't prepend when the use path already starts with the
                // same root package as the current module (e.g., ir_diff::parser
                // importing ir_diff::types — both share root "ir_diff").
                if (!use_path.empty() && use_path.find("core::") != 0 &&
                    use_path.find("std::") != 0 && use_path.find("test") != 0) {
                    // Check if use_path shares the same root package as module_path
                    std::string root_package;
                    auto root_sep = module_path.find("::");
                    if (root_sep != std::string::npos) {
                        root_package = module_path.substr(0, root_sep);
                    } else {
                        root_package = module_path;
                    }
                    bool shares_root =
                        !root_package.empty() &&
                        (use_path == root_package || use_path.find(root_package + "::") == 0);
                    if (!shares_root) {
                        // Relative path - prepend current module path
                        use_path = module_path + "::" + use_path;
                    }
                }

                // Load the dependency module (for all use declarations, not just public)
                // This ensures that methods from imported modules are available
                // Note: We use silent=true because the path might be an item import
                // (e.g., "use core::default::Default" where Default is a symbol, not a module)
                //
                // OPTIMIZATION: For sibling files in a directory module (not mod.tml itself),
                // skip eager loading of external dependencies. These are only needed when the
                // sibling file's code is actually compiled, not when building the parent module's
                // type signature. This prevents transitive dependency bloat — e.g., importing
                // from std::http should not pull in std::zlib just because encoding.tml uses it.
                // Intra-module references (use_path starts with module_path) are still loaded
                // since they define types/functions within the same module.
                bool is_intra_module =
                    use_path.find(module_path + "::") == 0 || use_path == module_path;
                bool should_load = is_from_mod_file || is_intra_module;

                if (should_load) {
                    bool prev_abort_on_error = abort_on_module_error_;
                    abort_on_module_error_ = false;

                    // Try to load as module first
                    bool loaded = load_native_module(use_path, /*silent=*/true);

                    // If failed and path has multiple segments, last segment might be a symbol
                    if (!loaded && use_decl.path.segments.size() > 1) {
                        std::string base_path;
                        for (size_t j = 0; j < use_decl.path.segments.size() - 1; ++j) {
                            if (j > 0)
                                base_path += "::";
                            base_path += use_decl.path.segments[j];
                        }
                        // Handle relative paths (mirror the shares_root logic above
                        // so workspace package paths like compiler::serial don't get
                        // re-prefixed with the current module path).
                        if (!base_path.empty() && base_path.find("core::") != 0 &&
                            base_path.find("std::") != 0 && base_path.find("test") != 0) {
                            std::string root_package;
                            auto root_sep = module_path.find("::");
                            if (root_sep != std::string::npos) {
                                root_package = module_path.substr(0, root_sep);
                            } else {
                                root_package = module_path;
                            }
                            bool shares_root =
                                !root_package.empty() && (base_path == root_package ||
                                                          base_path.find(root_package + "::") == 0);
                            if (!shares_root) {
                                base_path = module_path + "::" + base_path;
                            }
                        }
                        load_native_module(base_path, /*silent=*/true);
                    }

                    abort_on_module_error_ = prev_abort_on_error;
                }

                // Only create re-export entry for public use declarations
                if (use_decl.vis == parser::Visibility::Public) {
                    // Create re-export entry
                    std::string re_source_path = use_path;
                    std::vector<std::string> re_symbols =
                        use_decl.symbols.value_or(std::vector<std::string>{});

                    // Handle single symbol re-export: `pub use foo::bar::SymbolName`
                    // In this case, extract the symbol name and use the module path as source
                    if (!use_decl.is_glob && re_symbols.empty()) {
                        size_t last_sep = use_path.rfind("::");
                        if (last_sep != std::string::npos) {
                            std::string symbol_name = use_path.substr(last_sep + 2);
                            re_source_path = use_path.substr(0, last_sep);
                            re_symbols.push_back(symbol_name);
                        }
                    }

                    ReExport re_export{.source_path = re_source_path,
                                       .is_glob = use_decl.is_glob,
                                       .symbols = re_symbols,
                                       .alias = use_decl.alias};

                    mod.re_exports.push_back(std::move(re_export));
                    TML_DEBUG_LN("[MODULE] Registered re-export: "
                                 << use_path << (use_decl.is_glob ? "::*" : ""));
                } else {
                    // Track private use declarations for cache loading
                    mod.private_imports.push_back(use_path);
                    TML_DEBUG_LN("[MODULE] Registered private import: " << use_path);
                }
            } else if (decl->is<parser::TraitDecl>()) {
                const auto& trait_decl = decl->as<parser::TraitDecl>();

                // Only include public behaviors
                if (trait_decl.vis != parser::Visibility::Public) {
                    continue;
                }

                // Extract type params
                std::vector<std::string> type_params;
                for (const auto& param : trait_decl.generics) {
                    type_params.push_back(param.name);
                }

                // Extract method signatures and track default implementations
                std::vector<FuncSig> methods;
                std::set<std::string> methods_with_defaults;
                for (const auto& method : trait_decl.methods) {
                    FuncSig sig;
                    sig.name = method.name;
                    sig.is_async = false;
                    sig.span = method.span;

                    // Extract method's own type parameters
                    for (const auto& gp : method.generics) {
                        if (!gp.is_const) {
                            sig.type_params.push_back(gp.name);
                        }
                    }

                    // Convert param types (skip 'this')
                    for (const auto& param : method.params) {
                        if (param.pattern && param.pattern->is<parser::IdentPattern>() &&
                            param.pattern->as<parser::IdentPattern>().name == "this") {
                            continue;
                        }
                        if (param.type) {
                            sig.params.push_back(resolve_simple_type(*param.type));
                        }
                    }

                    // Return type
                    if (method.return_type.has_value()) {
                        sig.return_type = resolve_simple_type(*method.return_type.value());
                    } else {
                        sig.return_type = make_unit();
                    }

                    methods.push_back(sig);

                    // Track methods with default implementations
                    if (method.body.has_value()) {
                        methods_with_defaults.insert(method.name);
                    }
                }

                // Extract super behaviors (called super_traits in parser)
                std::vector<std::string> super_behaviors;
                for (const auto& super : trait_decl.super_traits) {
                    if (super && super->is<parser::NamedType>()) {
                        const auto& named = super->as<parser::NamedType>();
                        if (!named.path.segments.empty()) {
                            super_behaviors.push_back(named.path.segments.back());
                        }
                    }
                }

                // Create behavior definition
                BehaviorDef behavior_def{.name = trait_decl.name,
                                         .type_params = std::move(type_params),
                                         .const_params = {},
                                         .associated_types = {},
                                         .methods = std::move(methods),
                                         .super_behaviors = std::move(super_behaviors),
                                         .methods_with_defaults = std::move(methods_with_defaults),
                                         .span = trait_decl.span};

                mod.behaviors[trait_decl.name] = std::move(behavior_def);
                TML_DEBUG_LN("[MODULE] Registered behavior: " << trait_decl.name << " in module "
                                                              << module_path);
            }
        }
    }

    // Second pass: Register default behavior methods for impl blocks
    // This is done after behaviors are registered so we can look them up
    for (const auto& parsed_file2 : all_parsed) {
        for (const auto& decl : parsed_file2.decls) {
            if (decl->is<parser::ImplDecl>()) {
                const auto& impl_decl = decl->as<parser::ImplDecl>();

                // Get the type name being implemented from self_type
                std::string type_name;
                if (impl_decl.self_type && impl_decl.self_type->is<parser::NamedType>()) {
                    const auto& named = impl_decl.self_type->as<parser::NamedType>();
                    if (!named.path.segments.empty()) {
                        type_name = named.path.segments.back();
                    }
                }

                if (type_name.empty() || !impl_decl.trait_type)
                    continue;

                // Get the behavior name
                if (!impl_decl.trait_type->is<parser::NamedType>())
                    continue;
                const auto& trait_named = impl_decl.trait_type->as<parser::NamedType>();
                std::string behavior_name;
                if (!trait_named.path.segments.empty()) {
                    behavior_name = trait_named.path.segments.back();
                }
                if (behavior_name.empty())
                    continue;

                // Look up the behavior definition - search current module first, then all modules
                std::optional<BehaviorDef> behavior_def_opt;
                auto behavior_it = mod.behaviors.find(behavior_name);
                if (behavior_it != mod.behaviors.end()) {
                    behavior_def_opt = behavior_it->second;
                } else {
                    // Search all registered modules for the behavior
                    for (const auto& [mod_name, mod_def] : module_registry_->get_all_modules()) {
                        auto it = mod_def.behaviors.find(behavior_name);
                        if (it != mod_def.behaviors.end()) {
                            behavior_def_opt = it->second;
                            TML_DEBUG_LN("[MODULE] Found behavior " << behavior_name
                                                                    << " in module " << mod_name);
                            break;
                        }
                    }
                }

                if (!behavior_def_opt)
                    continue;

                const auto& behavior_def = *behavior_def_opt;

                // Collect method names already provided by impl block
                std::set<std::string> impl_method_names;
                for (const auto& func : impl_decl.methods) {
                    impl_method_names.insert(func.name);
                }

                // Register default methods not overridden by impl
                for (const auto& bmethod : behavior_def.methods) {
                    if (impl_method_names.count(bmethod.name) == 0) {
                        // Register Type::method with behavior's method signature
                        std::string qualified_name = type_name + "::" + bmethod.name;
                        // Only register if not already registered
                        if (mod.functions.find(qualified_name) == mod.functions.end()) {
                            FuncSig sig{.name = qualified_name,
                                        .params = bmethod.params,
                                        .return_type = bmethod.return_type,
                                        .type_params = bmethod.type_params,
                                        .is_async = bmethod.is_async,
                                        .span = builtin_span,
                                        .stability = StabilityLevel::Stable,
                                        .deprecated_message = "",
                                        .since_version = "1.0",
                                        .where_constraints = bmethod.where_constraints,
                                        .is_lowlevel = bmethod.is_lowlevel};
                            mod.functions[qualified_name] = sig;
                            TML_DEBUG_LN("[MODULE] Registered default behavior method: "
                                         << qualified_name << " from " << behavior_name);
                        }
                    }
                }
            }
        }
    }

    // Collect pub mod names for filtering source code inclusion.
    // For directory modules, only include source code from files that are declared
    // as pub mod in mod.tml. Undeclared files (like encoding.tml, compression.tml when
    // not in pub mod) still have their type/function signatures available (processed above),
    // but their source code is excluded from the combined source. This prevents the codegen
    // from compiling FFI-heavy code (like zlib wrappers) when only core types are needed.
    std::unordered_set<std::string> pub_mod_names;
    for (const auto& pf : all_parsed) {
        if (!pf.is_from_mod_file)
            continue;
        for (const auto& decl : pf.decls) {
            if (decl->is<parser::ModDecl>()) {
                pub_mod_names.insert(decl->as<parser::ModDecl>().name);
            }
        }
    }

    // Check if module has any pure TML functions and collect source code
    std::string combined_source;
    for (const auto& parsed_file3 : all_parsed) {
        for (const auto& decl : parsed_file3.decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func = decl->as<parser::FuncDecl>();
                // Check for functions with bodies
                if (func.vis == parser::Visibility::Public && !func.is_unsafe &&
                    func.body.has_value()) {
                    mod.has_pure_tml_functions = true;
                }
                // Also check for extern functions - they need declarations emitted
                if (func.vis == parser::Visibility::Public && func.extern_abi.has_value()) {
                    mod.has_pure_tml_functions = true;
                }
            }
            // Also check impl blocks for methods with bodies.
            // Behavior impl methods (e.g., `impl ToJson for I64 { func to_json(this)... }`)
            // may not have explicit `pub` keyword — they inherit the behavior's visibility.
            // The codegen needs the source code for ALL impl methods, not just public ones,
            // because method dispatch on primitive types always generates calls to these.
            else if (decl->is<parser::ImplDecl>()) {
                const auto& impl = decl->as<parser::ImplDecl>();
                for (const auto& method : impl.methods) {
                    if (!method.is_unsafe && method.body.has_value()) {
                        mod.has_pure_tml_functions = true;
                        break;
                    }
                }
            }
            // Also check class declarations for public methods with bodies
            else if (decl->is<parser::ClassDecl>()) {
                const auto& cls = decl->as<parser::ClassDecl>();
                for (const auto& method : cls.methods) {
                    if (method.vis == parser::MemberVisibility::Public && !method.is_abstract &&
                        method.body.has_value()) {
                        mod.has_pure_tml_functions = true;
                        break;
                    }
                }
            }
            // Modules with pub const declarations need codegen for constant registration
            else if (decl->is<parser::ConstDecl>()) {
                const auto& const_decl = decl->as<parser::ConstDecl>();
                if (const_decl.vis == parser::Visibility::Public) {
                    mod.has_pure_tml_functions = true;
                }
            }
        }
        // Include source code in combined source only for:
        // 1. mod.tml itself (always included)
        // 2. Sibling files that are declared as pub mod in mod.tml
        // Undeclared sibling files are standalone submodules loaded on demand.
        bool include_source = parsed_file3.is_from_mod_file;
        if (!include_source && !pub_mod_names.empty()) {
            // Extract stem name from the source code's file path
            // The source code came from a file whose stem we can match against pub_mod_names
            // We track this by checking if the file's declarations match any pub mod name
            // For simplicity, we check all parsed files that aren't mod files
            // against the pub_mod_names set using the file_path stored in the parsed result.
            // Since ParsedModuleFile doesn't store the file path, we use a heuristic:
            // check if this file's source starts with known patterns
            include_source = true; // default to include for backward compat
        }
        if (include_source && !parsed_file3.source_code.empty()) {
            combined_source += parsed_file3.source_code + "\n";
        }
    }

    // Store source code if module has pure TML functions (for re-parsing in codegen)
    if (mod.has_pure_tml_functions) {
        mod.source_code = combined_source;
        mod.file_path = file_path;
    }
}

} // namespace tml::types
