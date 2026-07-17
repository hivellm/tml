## 1. Parser
- [x] 1.1 After `behavior Ident`, peek `=` → parse as BehaviorAlias — parser_decl.cpp checks for Assign token after name+generics
- [x] 1.2 Add BehaviorAlias AST node — BehaviorAliasDecl struct in ast_decls.hpp:435, added to Decl variant
- [x] 1.3 Wire through AST visitor/serialization — hir_builder.cpp and ast_reader.cpp handle BehaviorAliasDecl

## 2. Type checker
- [x] 2.1 Register behavior alias in type environment — define_behavior_alias/lookup_behavior_alias in env.hpp, behavior_aliases_ map
- [x] 2.2 When alias name used as a bound, expand to constituent bounds — alias expansion in trait solver
- [x] 2.3 Validate all constituent behaviors exist — type checker validates at registration

## 3. Tail (mandatory)
- [x] 3.1 Update or create documentation covering the implementation — documented in v0.3.0 patch notes
- [x] 3.2 Write tests covering the new behavior — behavior_alias.test.tml (Friendly = Greetable, impl + call)
- [x] 3.3 Run tests and confirm they pass — test passes
