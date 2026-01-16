//! # JSON Library Tests
//!
//! Comprehensive test suite for the TML JSON library.
//!
//! ## Test Coverage
//!
//! - Value construction and type queries
//! - Parser tests (primitives, strings, arrays, objects, errors)
//! - Serializer tests (compact, pretty, escapes)
//! - Builder tests
//! - JSON-RPC tests
//! - Roundtrip tests

#include "common.hpp"
#include "json/json.hpp"

#include <gtest/gtest.h>
#include <sstream>

using namespace tml::json;

// Type aliases for Result types used in tests
using JsonResult = std::variant<JsonValue, JsonError>;
using RpcReqResult = std::variant<JsonRpcRequest, JsonRpcError>;
using RpcRespResult = std::variant<JsonRpcResponse, JsonRpcError>;

// Helper functions for Result handling
inline bool json_is_ok(const JsonResult& r) {
    return std::holds_alternative<JsonValue>(r);
}

inline JsonValue& json_unwrap(JsonResult& r) {
    return std::get<JsonValue>(r);
}

inline const JsonError& json_unwrap_err(const JsonResult& r) {
    return std::get<JsonError>(r);
}

inline bool rpc_req_is_ok(const RpcReqResult& r) {
    return std::holds_alternative<JsonRpcRequest>(r);
}

inline JsonRpcRequest& rpc_req_unwrap(RpcReqResult& r) {
    return std::get<JsonRpcRequest>(r);
}

inline bool rpc_resp_is_ok(const RpcRespResult& r) {
    return std::holds_alternative<JsonRpcResponse>(r);
}

inline JsonRpcResponse& rpc_resp_unwrap(RpcRespResult& r) {
    return std::get<JsonRpcResponse>(r);
}

// ============================================================================
// JsonValue Construction Tests
// ============================================================================

TEST(JsonValueTest, NullConstruction) {
    JsonValue v;
    EXPECT_TRUE(v.is_null());
    EXPECT_FALSE(v.is_bool());
    EXPECT_FALSE(v.is_number());
    EXPECT_FALSE(v.is_string());
    EXPECT_FALSE(v.is_array());
    EXPECT_FALSE(v.is_object());
}

TEST(JsonValueTest, BoolConstruction) {
    JsonValue t(true);
    JsonValue f(false);

    EXPECT_TRUE(t.is_bool());
    EXPECT_TRUE(f.is_bool());
    EXPECT_TRUE(t.as_bool());
    EXPECT_FALSE(f.as_bool());
}

TEST(JsonValueTest, IntegerConstruction) {
    JsonValue pos(int64_t(42));
    JsonValue neg(int64_t(-100));
    JsonValue zero(int64_t(0));

    EXPECT_TRUE(pos.is_number());
    EXPECT_TRUE(pos.is_integer());
    EXPECT_FALSE(pos.is_float());
    EXPECT_EQ(pos.as_i64(), 42);

    EXPECT_EQ(neg.as_i64(), -100);
    EXPECT_EQ(zero.as_i64(), 0);
}

TEST(JsonValueTest, UnsignedConstruction) {
    JsonValue v(uint64_t(18446744073709551615ULL));
    EXPECT_TRUE(v.is_number());
    EXPECT_TRUE(v.is_integer());
    EXPECT_EQ(v.as_u64(), 18446744073709551615ULL);
}

TEST(JsonValueTest, FloatConstruction) {
    JsonValue v(3.14159);
    EXPECT_TRUE(v.is_number());
    EXPECT_TRUE(v.is_float());
    EXPECT_FALSE(v.is_integer());
    EXPECT_DOUBLE_EQ(v.as_f64(), 3.14159);
}

TEST(JsonValueTest, StringConstruction) {
    JsonValue v("hello");
    EXPECT_TRUE(v.is_string());
    EXPECT_EQ(v.as_string(), "hello");
}

TEST(JsonValueTest, ArrayConstruction) {
    JsonArray arr;
    arr.push_back(JsonValue(1));
    arr.push_back(JsonValue(2));
    arr.push_back(JsonValue(3));

    JsonValue v(std::move(arr));
    EXPECT_TRUE(v.is_array());
    EXPECT_EQ(v.as_array().size(), 3);
    EXPECT_EQ(v.as_array()[0].as_i64(), 1);
}

TEST(JsonValueTest, ObjectConstruction) {
    JsonObject obj;
    obj["name"] = JsonValue("Alice");
    obj["age"] = JsonValue(30);

    JsonValue v(std::move(obj));
    EXPECT_TRUE(v.is_object());
    EXPECT_EQ(v.as_object().size(), 2);
    EXPECT_EQ(v.get("name")->as_string(), "Alice");
    EXPECT_EQ(v.get("age")->as_i64(), 30);
}

TEST(JsonValueTest, FactoryFunctions) {
    auto null_val = json_null();
    auto true_val = json_bool(true);
    auto int_val = json_int(42);
    auto float_val = json_float(3.14);
    auto str_val = json_string("test");

    EXPECT_TRUE(null_val.is_null());
    EXPECT_TRUE(true_val.as_bool());
    EXPECT_EQ(int_val.as_i64(), 42);
    EXPECT_DOUBLE_EQ(float_val.as_f64(), 3.14);
    EXPECT_EQ(str_val.as_string(), "test");
}

// ============================================================================
// Parser Tests - Primitives
// ============================================================================

TEST(JsonParserTest, ParseNull) {
    auto result = parse_json("null");
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_TRUE(json_unwrap(result).is_null());
}

TEST(JsonParserTest, ParseTrue) {
    auto result = parse_json("true");
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_TRUE(json_unwrap(result).is_bool());
    EXPECT_TRUE(json_unwrap(result).as_bool());
}

TEST(JsonParserTest, ParseFalse) {
    auto result = parse_json("false");
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_TRUE(json_unwrap(result).is_bool());
    EXPECT_FALSE(json_unwrap(result).as_bool());
}

// ============================================================================
// Parser Tests - Numbers
// ============================================================================

TEST(JsonParserTest, ParsePositiveInteger) {
    auto result = parse_json("42");
    ASSERT_TRUE(json_is_ok(result));
    auto& v = json_unwrap(result);
    EXPECT_TRUE(v.is_integer());
    EXPECT_EQ(v.as_i64(), 42);
}

TEST(JsonParserTest, ParseNegativeInteger) {
    auto result = parse_json("-100");
    ASSERT_TRUE(json_is_ok(result));
    auto& v = json_unwrap(result);
    EXPECT_TRUE(v.is_integer());
    EXPECT_EQ(v.as_i64(), -100);
}

TEST(JsonParserTest, ParseZero) {
    auto result = parse_json("0");
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_EQ(json_unwrap(result).as_i64(), 0);
}

TEST(JsonParserTest, ParseFloat) {
    auto result = parse_json("3.14159");
    ASSERT_TRUE(json_is_ok(result));
    auto& v = json_unwrap(result);
    EXPECT_TRUE(v.is_float());
    EXPECT_DOUBLE_EQ(v.as_f64(), 3.14159);
}

TEST(JsonParserTest, ParseScientificNotation) {
    auto result = parse_json("1.5e10");
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_TRUE(json_unwrap(result).is_float());
    EXPECT_DOUBLE_EQ(json_unwrap(result).as_f64(), 1.5e10);
}

TEST(JsonParserTest, ParseNegativeExponent) {
    auto result = parse_json("1e-5");
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_DOUBLE_EQ(json_unwrap(result).as_f64(), 1e-5);
}

TEST(JsonParserTest, ParseLargeInteger) {
    auto result = parse_json("9223372036854775807");
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_EQ(json_unwrap(result).as_i64(), INT64_MAX);
}

// ============================================================================
// Parser Tests - Strings
// ============================================================================

TEST(JsonParserTest, ParseSimpleString) {
    auto result = parse_json(R"("hello world")");
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_EQ(json_unwrap(result).as_string(), "hello world");
}

TEST(JsonParserTest, ParseEmptyString) {
    auto result = parse_json(R"("")");
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_EQ(json_unwrap(result).as_string(), "");
}

TEST(JsonParserTest, ParseEscapedQuote) {
    auto result = parse_json(R"("say \"hello\"")");
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_EQ(json_unwrap(result).as_string(), "say \"hello\"");
}

TEST(JsonParserTest, ParseEscapedBackslash) {
    auto result = parse_json(R"("path\\to\\file")");
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_EQ(json_unwrap(result).as_string(), "path\\to\\file");
}

TEST(JsonParserTest, ParseEscapedNewline) {
    auto result = parse_json(R"("line1\nline2")");
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_EQ(json_unwrap(result).as_string(), "line1\nline2");
}

TEST(JsonParserTest, ParseEscapedTab) {
    auto result = parse_json(R"("col1\tcol2")");
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_EQ(json_unwrap(result).as_string(), "col1\tcol2");
}

TEST(JsonParserTest, ParseUnicodeEscape) {
    auto result = parse_json(R"("\u0041\u0042\u0043")");
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_EQ(json_unwrap(result).as_string(), "ABC");
}

// ============================================================================
// Parser Tests - Arrays
// ============================================================================

TEST(JsonParserTest, ParseEmptyArray) {
    auto result = parse_json("[]");
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_TRUE(json_unwrap(result).is_array());
    EXPECT_TRUE(json_unwrap(result).as_array().empty());
}

TEST(JsonParserTest, ParseIntegerArray) {
    auto result = parse_json("[1, 2, 3]");
    ASSERT_TRUE(json_is_ok(result));
    auto& arr = json_unwrap(result).as_array();
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0].as_i64(), 1);
    EXPECT_EQ(arr[1].as_i64(), 2);
    EXPECT_EQ(arr[2].as_i64(), 3);
}

TEST(JsonParserTest, ParseMixedArray) {
    auto result = parse_json(R"([1, "two", true, null])");
    ASSERT_TRUE(json_is_ok(result));
    auto& arr = json_unwrap(result).as_array();
    EXPECT_EQ(arr.size(), 4);
    EXPECT_EQ(arr[0].as_i64(), 1);
    EXPECT_EQ(arr[1].as_string(), "two");
    EXPECT_TRUE(arr[2].as_bool());
    EXPECT_TRUE(arr[3].is_null());
}

TEST(JsonParserTest, ParseNestedArrays) {
    auto result = parse_json("[[1, 2], [3, 4]]");
    ASSERT_TRUE(json_is_ok(result));
    auto& arr = json_unwrap(result).as_array();
    EXPECT_EQ(arr.size(), 2);
    EXPECT_EQ(arr[0].as_array()[0].as_i64(), 1);
    EXPECT_EQ(arr[1].as_array()[1].as_i64(), 4);
}

// ============================================================================
// Parser Tests - Objects
// ============================================================================

TEST(JsonParserTest, ParseEmptyObject) {
    auto result = parse_json("{}");
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_TRUE(json_unwrap(result).is_object());
    EXPECT_TRUE(json_unwrap(result).as_object().empty());
}

TEST(JsonParserTest, ParseSimpleObject) {
    auto result = parse_json(R"({"name": "Alice", "age": 30})");
    ASSERT_TRUE(json_is_ok(result));
    auto& obj = json_unwrap(result);
    EXPECT_EQ(obj.get("name")->as_string(), "Alice");
    EXPECT_EQ(obj.get("age")->as_i64(), 30);
}

TEST(JsonParserTest, ParseNestedObject) {
    auto result = parse_json(R"({
        "person": {
            "name": "Bob",
            "address": {
                "city": "NYC"
            }
        }
    })");
    ASSERT_TRUE(json_is_ok(result));
    auto& obj = json_unwrap(result);
    auto* person = obj.get("person");
    ASSERT_NE(person, nullptr);
    EXPECT_EQ(person->get("name")->as_string(), "Bob");
    EXPECT_EQ(person->get("address")->get("city")->as_string(), "NYC");
}

TEST(JsonParserTest, ParseObjectWithArray) {
    auto result = parse_json(R"({"scores": [95, 87, 92]})");
    ASSERT_TRUE(json_is_ok(result));
    auto& scores = json_unwrap(result).get("scores")->as_array();
    EXPECT_EQ(scores.size(), 3);
    EXPECT_EQ(scores[0].as_i64(), 95);
}

// ============================================================================
// Parser Tests - Errors
// ============================================================================

TEST(JsonParserTest, ErrorUnterminatedString) {
    auto result = parse_json(R"("hello)");
    EXPECT_FALSE(json_is_ok(result));
}

TEST(JsonParserTest, ErrorInvalidNumber) {
    auto result = parse_json("123abc");
    EXPECT_FALSE(json_is_ok(result));
}

TEST(JsonParserTest, ErrorTrailingComma) {
    auto result = parse_json("[1, 2, 3,]");
    EXPECT_FALSE(json_is_ok(result));
}

TEST(JsonParserTest, ErrorMissingColon) {
    auto result = parse_json(R"({"key" "value"})");
    EXPECT_FALSE(json_is_ok(result));
}

TEST(JsonParserTest, ErrorMissingValue) {
    auto result = parse_json(R"({"key":})");
    EXPECT_FALSE(json_is_ok(result));
}

TEST(JsonParserTest, ErrorLocation) {
    auto result = parse_json("{\n  \"key\": invalid\n}");
    ASSERT_FALSE(json_is_ok(result));
    auto& error = json_unwrap_err(result);
    EXPECT_GT(error.line, 0);
    EXPECT_GT(error.column, 0);
}

// ============================================================================
// Serializer Tests - Compact
// ============================================================================

TEST(JsonSerializerTest, SerializeNull) {
    JsonValue v;
    EXPECT_EQ(v.to_string(), "null");
}

TEST(JsonSerializerTest, SerializeBool) {
    EXPECT_EQ(JsonValue(true).to_string(), "true");
    EXPECT_EQ(JsonValue(false).to_string(), "false");
}

TEST(JsonSerializerTest, SerializeInteger) {
    EXPECT_EQ(JsonValue(int64_t(42)).to_string(), "42");
    EXPECT_EQ(JsonValue(int64_t(-100)).to_string(), "-100");
    EXPECT_EQ(JsonValue(int64_t(0)).to_string(), "0");
}

TEST(JsonSerializerTest, SerializeFloat) {
    auto str = JsonValue(3.14).to_string();
    EXPECT_TRUE(str.find("3.14") != std::string::npos);
}

TEST(JsonSerializerTest, SerializeString) {
    EXPECT_EQ(JsonValue("hello").to_string(), "\"hello\"");
}

TEST(JsonSerializerTest, SerializeStringEscapes) {
    EXPECT_EQ(JsonValue("say \"hi\"").to_string(), "\"say \\\"hi\\\"\"");
    EXPECT_EQ(JsonValue("line1\nline2").to_string(), "\"line1\\nline2\"");
    EXPECT_EQ(JsonValue("tab\there").to_string(), "\"tab\\there\"");
}

TEST(JsonSerializerTest, SerializeArray) {
    JsonArray arr;
    arr.push_back(JsonValue(1));
    arr.push_back(JsonValue(2));
    arr.push_back(JsonValue(3));
    JsonValue v(std::move(arr));
    EXPECT_EQ(v.to_string(), "[1,2,3]");
}

TEST(JsonSerializerTest, SerializeObject) {
    JsonObject obj;
    obj["a"] = JsonValue(1);
    obj["b"] = JsonValue(2);
    JsonValue v(std::move(obj));
    auto str = v.to_string();
    EXPECT_TRUE(str.find("\"a\":1") != std::string::npos);
    EXPECT_TRUE(str.find("\"b\":2") != std::string::npos);
}

// ============================================================================
// Serializer Tests - Pretty
// ============================================================================

TEST(JsonSerializerTest, PrettyPrintArray) {
    JsonArray arr;
    arr.push_back(JsonValue(1));
    arr.push_back(JsonValue(2));
    JsonValue v(std::move(arr));
    auto pretty = v.to_string_pretty(2);
    EXPECT_TRUE(pretty.find("[\n") != std::string::npos);
    EXPECT_TRUE(pretty.find("  1") != std::string::npos);
}

TEST(JsonSerializerTest, PrettyPrintObject) {
    JsonObject obj;
    obj["key"] = JsonValue("value");
    JsonValue v(std::move(obj));
    auto pretty = v.to_string_pretty(2);
    EXPECT_TRUE(pretty.find("{\n") != std::string::npos);
    EXPECT_TRUE(pretty.find("\"key\":") != std::string::npos);
}

TEST(JsonSerializerTest, PrettyPrintEmptyArray) {
    JsonValue v(JsonArray{});
    EXPECT_EQ(v.to_string_pretty(2), "[]");
}

TEST(JsonSerializerTest, PrettyPrintEmptyObject) {
    JsonValue v(JsonObject{});
    EXPECT_EQ(v.to_string_pretty(2), "{}");
}

// ============================================================================
// Roundtrip Tests
// ============================================================================

TEST(JsonRoundtripTest, RoundtripInteger) {
    auto original = JsonValue(int64_t(123456789));
    auto result = parse_json(original.to_string());
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_EQ(json_unwrap(result).as_i64(), 123456789);
}

TEST(JsonRoundtripTest, RoundtripString) {
    auto original = JsonValue("hello \"world\"\nwith escapes");
    auto result = parse_json(original.to_string());
    ASSERT_TRUE(json_is_ok(result));
    EXPECT_EQ(json_unwrap(result).as_string(), "hello \"world\"\nwith escapes");
}

TEST(JsonRoundtripTest, RoundtripComplex) {
    auto original_result = parse_json(R"({
        "users": [
            {"name": "Alice", "age": 30},
            {"name": "Bob", "age": 25}
        ],
        "active": true,
        "count": 2
    })");
    ASSERT_TRUE(json_is_ok(original_result));
    auto& original = json_unwrap(original_result);

    auto roundtrip = parse_json(original.to_string());
    ASSERT_TRUE(json_is_ok(roundtrip));

    auto& result = json_unwrap(roundtrip);
    EXPECT_EQ(result.get("count")->as_i64(), 2);
    EXPECT_TRUE(result.get("active")->as_bool());
    EXPECT_EQ(result.get("users")->as_array().size(), 2);
}

// ============================================================================
// Builder Tests
// ============================================================================

TEST(JsonBuilderTest, BuildNull) {
    auto v = JsonBuilder().null().build();
    EXPECT_TRUE(v.is_null());
}

TEST(JsonBuilderTest, BuildBool) {
    EXPECT_TRUE(JsonBuilder().boolean(true).build().as_bool());
    EXPECT_FALSE(JsonBuilder().boolean(false).build().as_bool());
}

TEST(JsonBuilderTest, BuildInteger) {
    auto v = JsonBuilder().integer(42).build();
    EXPECT_EQ(v.as_i64(), 42);
}

TEST(JsonBuilderTest, BuildFloat) {
    auto v = JsonBuilder().floating(3.14).build();
    EXPECT_DOUBLE_EQ(v.as_f64(), 3.14);
}

TEST(JsonBuilderTest, BuildString) {
    auto v = JsonBuilder().string("hello").build();
    EXPECT_EQ(v.as_string(), "hello");
}

TEST(JsonBuilderTest, BuildSimpleArray) {
    auto v = JsonBuilder()
                 .array()
                 .item(1)
                 .item(2)
                 .item(3)
                 .end()
                 .build();

    EXPECT_TRUE(v.is_array());
    EXPECT_EQ(v.as_array().size(), 3);
    EXPECT_EQ(v.as_array()[0].as_i64(), 1);
}

TEST(JsonBuilderTest, BuildSimpleObject) {
    auto v = JsonBuilder()
                 .object()
                 .field("name", "Alice")
                 .field("age", 30)
                 .end()
                 .build();

    EXPECT_TRUE(v.is_object());
    EXPECT_EQ(v.get("name")->as_string(), "Alice");
    EXPECT_EQ(v.get("age")->as_i64(), 30);
}

TEST(JsonBuilderTest, BuildNestedStructure) {
    auto v = JsonBuilder()
                 .object()
                 .field("person",
                        JsonBuilder()
                            .object()
                            .field("name", "Bob")
                            .field("scores",
                                   JsonBuilder().array().item(95).item(87).end().build())
                            .end()
                            .build())
                 .end()
                 .build();

    EXPECT_EQ(v.get("person")->get("name")->as_string(), "Bob");
    EXPECT_EQ(v.get("person")->get("scores")->as_array().size(), 2);
}

// ============================================================================
// JSON-RPC Tests
// ============================================================================

TEST(JsonRpcTest, ParseRequest) {
    auto json = parse_json(R"({"jsonrpc":"2.0","method":"sum","params":[1,2,3],"id":1})");
    ASSERT_TRUE(json_is_ok(json));

    auto req = JsonRpcRequest::from_json(json_unwrap(json));
    ASSERT_TRUE(rpc_req_is_ok(req));

    auto& request = rpc_req_unwrap(req);
    EXPECT_EQ(request.method, "sum");
    EXPECT_TRUE(request.params.has_value());
    EXPECT_EQ(request.params->as_array().size(), 3);
    EXPECT_EQ(request.id->as_i64(), 1);
    EXPECT_FALSE(request.is_notification());
}

TEST(JsonRpcTest, ParseNotification) {
    auto json = parse_json(R"({"jsonrpc":"2.0","method":"notify"})");
    ASSERT_TRUE(json_is_ok(json));

    auto req = JsonRpcRequest::from_json(json_unwrap(json));
    ASSERT_TRUE(rpc_req_is_ok(req));

    auto& request = rpc_req_unwrap(req);
    EXPECT_EQ(request.method, "notify");
    EXPECT_TRUE(request.is_notification());
}

TEST(JsonRpcTest, RequestToJson) {
    JsonRpcRequest req;
    req.method = "test";
    JsonArray params_arr;
    params_arr.push_back(JsonValue(1));
    params_arr.push_back(JsonValue(2));
    req.params = JsonValue(std::move(params_arr));
    req.id = JsonValue(42);

    auto json = req.to_json();
    EXPECT_EQ(json.get("jsonrpc")->as_string(), "2.0");
    EXPECT_EQ(json.get("method")->as_string(), "test");
    EXPECT_EQ(json.get("id")->as_i64(), 42);
}

TEST(JsonRpcTest, SuccessResponse) {
    auto response = JsonRpcResponse::success(JsonValue(42), JsonValue(1));
    EXPECT_FALSE(response.is_error());
    EXPECT_EQ(response.result->as_i64(), 42);

    auto json = response.to_json();
    EXPECT_EQ(json.get("result")->as_i64(), 42);
    EXPECT_EQ(json.get("id")->as_i64(), 1);
}

TEST(JsonRpcTest, ErrorResponse) {
    auto error = JsonRpcError::from_code(JsonRpcErrorCode::MethodNotFound);
    auto response = JsonRpcResponse::failure(std::move(error), JsonValue(1));

    EXPECT_TRUE(response.is_error());
    EXPECT_EQ(response.error->code, -32601);
    EXPECT_EQ(response.error->message, "Method not found");
}

TEST(JsonRpcTest, ParseSuccessResponse) {
    auto json = parse_json(R"({"jsonrpc":"2.0","result":42,"id":1})");
    ASSERT_TRUE(json_is_ok(json));

    auto resp = JsonRpcResponse::from_json(json_unwrap(json));
    ASSERT_TRUE(rpc_resp_is_ok(resp));

    auto& response = rpc_resp_unwrap(resp);
    EXPECT_FALSE(response.is_error());
    EXPECT_EQ(response.result->as_i64(), 42);
}

TEST(JsonRpcTest, ParseErrorResponse) {
    auto json =
        parse_json(R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid Request"},"id":1})");
    ASSERT_TRUE(json_is_ok(json));

    auto resp = JsonRpcResponse::from_json(json_unwrap(json));
    ASSERT_TRUE(rpc_resp_is_ok(resp));

    auto& response = rpc_resp_unwrap(resp);
    EXPECT_TRUE(response.is_error());
    EXPECT_EQ(response.error->code, -32600);
    EXPECT_EQ(response.error->message, "Invalid Request");
}

TEST(JsonRpcTest, ErrorFromCode) {
    auto parse_error = JsonRpcError::from_code(JsonRpcErrorCode::ParseError);
    EXPECT_EQ(parse_error.code, -32700);
    EXPECT_EQ(parse_error.message, "Parse error");

    auto invalid_request = JsonRpcError::from_code(JsonRpcErrorCode::InvalidRequest);
    EXPECT_EQ(invalid_request.code, -32600);

    auto method_not_found = JsonRpcError::from_code(JsonRpcErrorCode::MethodNotFound);
    EXPECT_EQ(method_not_found.code, -32601);

    auto invalid_params = JsonRpcError::from_code(JsonRpcErrorCode::InvalidParams);
    EXPECT_EQ(invalid_params.code, -32602);

    auto internal_error = JsonRpcError::from_code(JsonRpcErrorCode::InternalError);
    EXPECT_EQ(internal_error.code, -32603);
}

TEST(JsonRpcTest, CustomError) {
    auto error = JsonRpcError::make(-32001, "Custom error", JsonValue("extra data"));
    EXPECT_EQ(error.code, -32001);
    EXPECT_EQ(error.message, "Custom error");
    EXPECT_TRUE(error.data.has_value());
    EXPECT_EQ(error.data->as_string(), "extra data");
}

// ============================================================================
// JsonValue Equality Tests
// ============================================================================

TEST(JsonValueTest, EqualityNull) {
    JsonValue a;
    JsonValue b;
    EXPECT_EQ(a, b);
}

TEST(JsonValueTest, EqualityBool) {
    EXPECT_EQ(JsonValue(true), JsonValue(true));
    EXPECT_EQ(JsonValue(false), JsonValue(false));
    EXPECT_NE(JsonValue(true), JsonValue(false));
}

TEST(JsonValueTest, EqualityNumber) {
    EXPECT_EQ(JsonValue(int64_t(42)), JsonValue(int64_t(42)));
    EXPECT_NE(JsonValue(int64_t(42)), JsonValue(int64_t(43)));
}

TEST(JsonValueTest, EqualityString) {
    EXPECT_EQ(JsonValue("hello"), JsonValue("hello"));
    EXPECT_NE(JsonValue("hello"), JsonValue("world"));
}

TEST(JsonValueTest, EqualityDifferentTypes) {
    EXPECT_NE(JsonValue(int64_t(1)), JsonValue(true));
    EXPECT_NE(JsonValue("1"), JsonValue(int64_t(1)));
    EXPECT_NE(JsonValue(), JsonValue(false));
}

// ============================================================================
// Streaming Output Tests
// ============================================================================

TEST(JsonSerializerTest, WriteToStream) {
    JsonObject obj;
    obj["name"] = JsonValue("Alice");
    obj["age"] = JsonValue(int64_t(30));
    JsonValue v(std::move(obj));

    std::ostringstream oss;
    v.write_to(oss);

    EXPECT_EQ(oss.str(), R"({"age":30,"name":"Alice"})");
}

TEST(JsonSerializerTest, WriteToStreamPretty) {
    JsonArray arr;
    arr.push_back(JsonValue(1));
    arr.push_back(JsonValue(2));
    JsonValue v(std::move(arr));

    std::ostringstream oss;
    v.write_to_pretty(oss, 2);

    std::string expected = "[\n  1,\n  2\n]";
    EXPECT_EQ(oss.str(), expected);
}

// ============================================================================
// Merge and Extend Tests
// ============================================================================

TEST(JsonValueTest, MergeObjects) {
    JsonObject obj1;
    obj1["a"] = JsonValue(1);
    obj1["b"] = JsonValue(2);
    JsonValue a(std::move(obj1));

    JsonObject obj2;
    obj2["b"] = JsonValue(3);
    obj2["c"] = JsonValue(4);
    JsonValue b(std::move(obj2));

    a.merge(std::move(b));

    EXPECT_EQ(a.get("a")->as_i64(), 1);
    EXPECT_EQ(a.get("b")->as_i64(), 3);  // Replaced
    EXPECT_EQ(a.get("c")->as_i64(), 4);  // Added
}

TEST(JsonValueTest, ExtendArrays) {
    JsonArray arr1;
    arr1.push_back(JsonValue(1));
    arr1.push_back(JsonValue(2));
    JsonValue a(std::move(arr1));

    JsonArray arr2;
    arr2.push_back(JsonValue(3));
    arr2.push_back(JsonValue(4));
    JsonValue b(std::move(arr2));

    a.extend(std::move(b));

    EXPECT_EQ(a.as_array().size(), 4);
    EXPECT_EQ(a.as_array()[0].as_i64(), 1);
    EXPECT_EQ(a.as_array()[1].as_i64(), 2);
    EXPECT_EQ(a.as_array()[2].as_i64(), 3);
    EXPECT_EQ(a.as_array()[3].as_i64(), 4);
}

// ============================================================================
// Schema Validation Tests
// ============================================================================

TEST(JsonSchemaTest, ValidateNull) {
    auto schema = JsonSchema::null();
    EXPECT_TRUE(schema.validate(JsonValue()).valid);
    EXPECT_FALSE(schema.validate(JsonValue(true)).valid);
}

TEST(JsonSchemaTest, ValidateBoolean) {
    auto schema = JsonSchema::boolean();
    EXPECT_TRUE(schema.validate(JsonValue(true)).valid);
    EXPECT_TRUE(schema.validate(JsonValue(false)).valid);
    EXPECT_FALSE(schema.validate(JsonValue(1)).valid);
}

TEST(JsonSchemaTest, ValidateInteger) {
    auto schema = JsonSchema::integer();
    EXPECT_TRUE(schema.validate(JsonValue(int64_t(42))).valid);
    EXPECT_FALSE(schema.validate(JsonValue(3.14)).valid);
    EXPECT_FALSE(schema.validate(JsonValue("42")).valid);
}

TEST(JsonSchemaTest, ValidateNumber) {
    auto schema = JsonSchema::number();
    EXPECT_TRUE(schema.validate(JsonValue(int64_t(42))).valid);
    EXPECT_TRUE(schema.validate(JsonValue(3.14)).valid);
    EXPECT_FALSE(schema.validate(JsonValue("42")).valid);
}

TEST(JsonSchemaTest, ValidateString) {
    auto schema = JsonSchema::string();
    EXPECT_TRUE(schema.validate(JsonValue("hello")).valid);
    EXPECT_FALSE(schema.validate(JsonValue(42)).valid);
}

TEST(JsonSchemaTest, ValidateArray) {
    auto schema = JsonSchema::array();
    JsonArray arr;
    arr.push_back(JsonValue(1));
    arr.push_back(JsonValue("mixed"));
    EXPECT_TRUE(schema.validate(JsonValue(std::move(arr))).valid);
    EXPECT_FALSE(schema.validate(JsonValue("not an array")).valid);
}

TEST(JsonSchemaTest, ValidateArrayOfIntegers) {
    auto schema = JsonSchema::array_of(JsonSchema::integer());

    JsonArray arr1;
    arr1.push_back(JsonValue(1));
    arr1.push_back(JsonValue(2));
    arr1.push_back(JsonValue(3));
    EXPECT_TRUE(schema.validate(JsonValue(std::move(arr1))).valid);

    JsonArray arr2;
    arr2.push_back(JsonValue(1));
    arr2.push_back(JsonValue("not an int"));
    auto result = schema.validate(JsonValue(std::move(arr2)));
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.path, "[1]");
}

TEST(JsonSchemaTest, ValidateObject) {
    auto schema = JsonSchema::object();
    JsonObject obj;
    obj["key"] = JsonValue("value");
    EXPECT_TRUE(schema.validate(JsonValue(std::move(obj))).valid);
    EXPECT_FALSE(schema.validate(JsonValue("not an object")).valid);
}

TEST(JsonSchemaTest, ValidateRequiredFields) {
    auto schema = JsonSchema::object()
                      .required("name", JsonSchema::string())
                      .required("age", JsonSchema::integer());

    JsonObject obj1;
    obj1["name"] = JsonValue("Alice");
    obj1["age"] = JsonValue(int64_t(30));
    EXPECT_TRUE(schema.validate(JsonValue(std::move(obj1))).valid);

    JsonObject obj2;
    obj2["name"] = JsonValue("Bob");
    // Missing age
    auto result = schema.validate(JsonValue(std::move(obj2)));
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.error.find("missing required field") != std::string::npos);
}

TEST(JsonSchemaTest, ValidateOptionalFields) {
    auto schema = JsonSchema::object()
                      .required("name", JsonSchema::string())
                      .optional("email", JsonSchema::string());

    JsonObject obj1;
    obj1["name"] = JsonValue("Alice");
    EXPECT_TRUE(schema.validate(JsonValue(std::move(obj1))).valid);

    JsonObject obj2;
    obj2["name"] = JsonValue("Bob");
    obj2["email"] = JsonValue("bob@example.com");
    EXPECT_TRUE(schema.validate(JsonValue(std::move(obj2))).valid);

    JsonObject obj3;
    obj3["name"] = JsonValue("Charlie");
    obj3["email"] = JsonValue(12345); // Wrong type
    auto result = schema.validate(JsonValue(std::move(obj3)));
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.path, "email");
}

TEST(JsonSchemaTest, ValidateAny) {
    auto schema = JsonSchema::any();
    EXPECT_TRUE(schema.validate(JsonValue()).valid);
    EXPECT_TRUE(schema.validate(JsonValue(true)).valid);
    EXPECT_TRUE(schema.validate(JsonValue(42)).valid);
    EXPECT_TRUE(schema.validate(JsonValue("hello")).valid);
}
