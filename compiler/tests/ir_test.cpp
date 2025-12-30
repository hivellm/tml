// IR (Intermediate Representation) tests
//
// Tests for the IR types and structures

#include "ir/ir.hpp"

#include <gtest/gtest.h>
#include <memory>

using namespace tml::ir;

// ============================================================================
// IR Type Tests
// ============================================================================

TEST(IRTest, IRTypeRefBasic) {
    auto type_ref = std::make_unique<IRTypeRef>();
    type_ref->name = "I32";
    EXPECT_EQ(type_ref->name, "I32");
    EXPECT_TRUE(type_ref->type_args.empty());
}

TEST(IRTest, IRTypeRefGeneric) {
    // Vec[I32]
    auto inner = std::make_unique<IRTypeRef>();
    inner->name = "I32";

    auto vec_type = std::make_unique<IRTypeRef>();
    vec_type->name = "Vec";
    vec_type->type_args.push_back(std::move(inner));

    EXPECT_EQ(vec_type->name, "Vec");
    EXPECT_EQ(vec_type->type_args.size(), 1u);
    EXPECT_EQ(vec_type->type_args[0]->name, "I32");
}

TEST(IRTest, IRRefType) {
    auto inner = std::make_unique<IRTypeRef>();
    inner->name = "I32";

    IRRefType ref_type;
    ref_type.is_mut = false;
    ref_type.inner = std::move(inner);

    EXPECT_FALSE(ref_type.is_mut);
    EXPECT_EQ(ref_type.inner->name, "I32");
}

TEST(IRTest, IRMutRefType) {
    auto inner = std::make_unique<IRTypeRef>();
    inner->name = "Str";

    IRRefType mut_ref;
    mut_ref.is_mut = true;
    mut_ref.inner = std::move(inner);

    EXPECT_TRUE(mut_ref.is_mut);
    EXPECT_EQ(mut_ref.inner->name, "Str");
}

TEST(IRTest, IRSliceType) {
    auto element = std::make_unique<IRTypeRef>();
    element->name = "U8";

    IRSliceType slice;
    slice.element = std::move(element);

    EXPECT_EQ(slice.element->name, "U8");
}

TEST(IRTest, IRArrayType) {
    auto element = std::make_unique<IRTypeRef>();
    element->name = "F64";

    IRArrayType array;
    array.element = std::move(element);
    array.size = 10;

    EXPECT_EQ(array.element->name, "F64");
    EXPECT_EQ(array.size, 10u);
}

TEST(IRTest, IRTupleType) {
    IRTupleType tuple;

    auto first = std::make_unique<IRTypeRef>();
    first->name = "I32";
    tuple.elements.push_back(std::move(first));

    auto second = std::make_unique<IRTypeRef>();
    second->name = "Str";
    tuple.elements.push_back(std::move(second));

    EXPECT_EQ(tuple.elements.size(), 2u);
    EXPECT_EQ(tuple.elements[0]->name, "I32");
    EXPECT_EQ(tuple.elements[1]->name, "Str");
}

TEST(IRTest, IRFuncType) {
    IRFuncType func_type;

    // (I32, I32) -> Bool
    auto param1 = std::make_unique<IRTypeRef>();
    param1->name = "I32";
    func_type.params.push_back(std::move(param1));

    auto param2 = std::make_unique<IRTypeRef>();
    param2->name = "I32";
    func_type.params.push_back(std::move(param2));

    auto ret = std::make_unique<IRTypeRef>();
    ret->name = "Bool";
    func_type.ret = std::move(ret);

    EXPECT_EQ(func_type.params.size(), 2u);
    EXPECT_EQ(func_type.ret->name, "Bool");
}

TEST(IRTest, IRTypeKindVariantRef) {
    // Test that IRTypeKind can hold an IRTypeRef
    auto simple = std::make_unique<IRTypeRef>();
    simple->name = "I32";

    // Move the content, not the struct with variant
    IRTypeKind kind1;
    kind1.emplace<IRTypeRef>(IRTypeRef{simple->name, {}});
    EXPECT_TRUE(std::holds_alternative<IRTypeRef>(kind1));
}

TEST(IRTest, IRTypeKindVariantSlice) {
    // Test that IRTypeKind can hold a slice type
    auto elem = std::make_unique<IRTypeRef>();
    elem->name = "U8";

    IRSliceType slice;
    slice.element = std::move(elem);

    IRTypeKind kind2 = std::move(slice);
    EXPECT_TRUE(std::holds_alternative<IRSliceType>(kind2));
}

// ============================================================================
// Visibility Tests
// ============================================================================

TEST(IRTest, VisibilityPrivate) {
    Visibility vis = Visibility::Private;
    EXPECT_EQ(vis, Visibility::Private);
    EXPECT_NE(vis, Visibility::Public);
}

TEST(IRTest, VisibilityPublic) {
    Visibility vis = Visibility::Public;
    EXPECT_EQ(vis, Visibility::Public);
    EXPECT_NE(vis, Visibility::Private);
}

// ============================================================================
// StableId Tests
// ============================================================================

TEST(IRTest, StableIdEmpty) {
    StableId id;
    EXPECT_TRUE(id.empty());
}

TEST(IRTest, StableIdAssignment) {
    StableId id = "a1b2c3d4";
    EXPECT_EQ(id.length(), 8u);
    EXPECT_EQ(id, "a1b2c3d4");
}

// ============================================================================
// IRTypeExpr Tests
// ============================================================================

TEST(IRTest, IRTypeExprWithTypeRef) {
    auto type_ref = std::make_unique<IRTypeRef>();
    type_ref->name = "MyStruct";

    IRTypeExpr expr;
    expr.kind.emplace<IRTypeRef>(IRTypeRef{type_ref->name, {}});

    EXPECT_TRUE(std::holds_alternative<IRTypeRef>(expr.kind));
    auto& ref = std::get<IRTypeRef>(expr.kind);
    EXPECT_EQ(ref.name, "MyStruct");
}

TEST(IRTest, IRTypeExprWithFuncType) {
    IRFuncType func_type;
    auto ret = std::make_unique<IRTypeRef>();
    ret->name = "Unit";
    func_type.ret = std::move(ret);

    IRTypeExpr expr;
    expr.kind = std::move(func_type);

    EXPECT_TRUE(std::holds_alternative<IRFuncType>(expr.kind));
}

// ============================================================================
// Nested Generic Type Tests
// ============================================================================

TEST(IRTest, NestedGenericType) {
    // Map[Str, Vec[I32]]
    auto i32_type = std::make_unique<IRTypeRef>();
    i32_type->name = "I32";

    auto vec_type = std::make_unique<IRTypeRef>();
    vec_type->name = "Vec";
    vec_type->type_args.push_back(std::move(i32_type));

    auto str_type = std::make_unique<IRTypeRef>();
    str_type->name = "Str";

    auto map_type = std::make_unique<IRTypeRef>();
    map_type->name = "Map";
    map_type->type_args.push_back(std::move(str_type));
    map_type->type_args.push_back(std::move(vec_type));

    EXPECT_EQ(map_type->name, "Map");
    EXPECT_EQ(map_type->type_args.size(), 2u);
    EXPECT_EQ(map_type->type_args[0]->name, "Str");
    EXPECT_EQ(map_type->type_args[1]->name, "Vec");
    EXPECT_EQ(map_type->type_args[1]->type_args.size(), 1u);
    EXPECT_EQ(map_type->type_args[1]->type_args[0]->name, "I32");
}

TEST(IRTest, MultipleTypeArgs) {
    // Result[T, E] with T=I32, E=Str
    auto t_type = std::make_unique<IRTypeRef>();
    t_type->name = "I32";

    auto e_type = std::make_unique<IRTypeRef>();
    e_type->name = "Str";

    auto result_type = std::make_unique<IRTypeRef>();
    result_type->name = "Result";
    result_type->type_args.push_back(std::move(t_type));
    result_type->type_args.push_back(std::move(e_type));

    EXPECT_EQ(result_type->name, "Result");
    EXPECT_EQ(result_type->type_args.size(), 2u);
}

// ============================================================================
// Array with Zero Size
// ============================================================================

TEST(IRTest, ZeroSizedArray) {
    auto element = std::make_unique<IRTypeRef>();
    element->name = "I32";

    IRArrayType array;
    array.element = std::move(element);
    array.size = 0;

    EXPECT_EQ(array.size, 0u);
}

// ============================================================================
// Empty Tuple
// ============================================================================

TEST(IRTest, EmptyTuple) {
    IRTupleType empty_tuple;
    EXPECT_TRUE(empty_tuple.elements.empty());
}

// ============================================================================
// Function Type with No Params
// ============================================================================

TEST(IRTest, FuncTypeNoParams) {
    IRFuncType func_type;

    auto ret = std::make_unique<IRTypeRef>();
    ret->name = "I32";
    func_type.ret = std::move(ret);

    EXPECT_TRUE(func_type.params.empty());
    EXPECT_EQ(func_type.ret->name, "I32");
}
