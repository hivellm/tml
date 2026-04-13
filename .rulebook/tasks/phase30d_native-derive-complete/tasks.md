## 1. Implementation
- [ ] 1.1 Debug derive: emit function that formats each field as `FieldName=<value>` separated by commas, wrapped in `TypeName { ... }` using sprintf into a growable buffer
- [ ] 1.2 Display derive: emit function that calls the Debug impl as the default representation; a manual Display impl on the same type takes precedence at link time
- [ ] 1.3 Default derive: emit constructor that zero-initialises each field (0 for I32/I64/F64, null ptr for Str, 0-tag for Maybe/Outcome, recursive Default for nested structs)
- [ ] 1.4 Serialize derive: emit function producing a JSON object string with each field serialised recursively; arrays use JSON array syntax; enums use `{"tag":"Name","value":...}`
- [ ] 1.5 Deserialize derive: emit function that parses a JSON object string, matches each key to a field name, assigns the parsed value; returns Outcome[T, ParseError]
- [ ] 1.6 FromStr derive: emit function that calls the Deserialize impl on the input string; returns Outcome[T, ParseError]
- [ ] 1.7 PartialOrd derive: emit function that compares fields lexicographically left-to-right; returns Maybe[Ordering] (Just(Less/Equal/Greater) or Nothing for unordered)
- [ ] 1.8 Reflect derive: emit a static TypeInfo record containing the type name as Str, field count as I64, and a static array of FieldInfo (name: Str, offset: I64, type_name: Str)

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
