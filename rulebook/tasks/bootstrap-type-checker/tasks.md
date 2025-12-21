# Tasks: Bootstrap Type Checker

## Progress: 0% (0/36 tasks complete)

## 1. Setup Phase
- [ ] 1.1 Create `src/types/` directory structure
- [ ] 1.2 Set up CMake configuration for types module
- [ ] 1.3 Create base header files with include guards

## 2. Type Representation Phase
- [ ] 2.1 Implement `Type` base class hierarchy
- [ ] 2.2 Implement primitive types (I32, F64, Bool, etc.)
- [ ] 2.3 Implement compound types (Tuple, Array, Slice)
- [ ] 2.4 Implement struct type representation
- [ ] 2.5 Implement enum type representation
- [ ] 2.6 Implement function type representation
- [ ] 2.7 Implement reference types (&T, &mut T)
- [ ] 2.8 Implement pointer types (*const T, *mut T)
- [ ] 2.9 Implement generic type with type parameters
- [ ] 2.10 Implement type variables for inference

## 3. Type Environment Phase
- [ ] 3.1 Implement `TypeEnv` for type bindings
- [ ] 3.2 Implement scope management (push/pop scope)
- [ ] 3.3 Implement type lookup by name
- [ ] 3.4 Implement trait registry
- [ ] 3.5 Implement implementation registry

## 4. Type Inference Phase
- [ ] 4.1 Implement type variable generation
- [ ] 4.2 Implement unification algorithm
- [ ] 4.3 Implement occurs check (prevent infinite types)
- [ ] 4.4 Implement type substitution
- [ ] 4.5 Implement constraint generation
- [ ] 4.6 Implement constraint solving
- [ ] 4.7 Implement generalization (let-polymorphism)

## 5. Expression Type Checking Phase
- [ ] 5.1 Implement literal type inference
- [ ] 5.2 Implement identifier type lookup
- [ ] 5.3 Implement binary expression type checking
- [ ] 5.4 Implement unary expression type checking
- [ ] 5.5 Implement call expression type checking
- [ ] 5.6 Implement field access type checking
- [ ] 5.7 Implement index expression type checking
- [ ] 5.8 Implement closure type inference
- [ ] 5.9 Implement if/when expression type checking

## 6. Statement Type Checking Phase
- [ ] 6.1 Implement variable declaration type checking
- [ ] 6.2 Implement assignment type checking
- [ ] 6.3 Implement return statement type checking
- [ ] 6.4 Implement control flow type checking

## 7. Declaration Type Checking Phase
- [ ] 7.1 Implement function signature checking
- [ ] 7.2 Implement struct declaration checking
- [ ] 7.3 Implement enum declaration checking
- [ ] 7.4 Implement trait declaration checking
- [ ] 7.5 Implement implement block checking
- [ ] 7.6 Implement generic parameter checking
- [ ] 7.7 Implement where clause checking

## 8. Trait Resolution Phase
- [ ] 8.1 Implement trait bound checking
- [ ] 8.2 Implement method resolution
- [ ] 8.3 Implement associated type resolution
- [ ] 8.4 Implement orphan rule checking

## 9. Testing Phase
- [ ] 9.1 Write unit tests for type representation
- [ ] 9.2 Write unit tests for type inference
- [ ] 9.3 Write unit tests for expression checking
- [ ] 9.4 Write unit tests for trait resolution
- [ ] 9.5 Write integration tests with full programs
- [ ] 9.6 Verify test coverage ≥95%

## 10. Documentation Phase
- [ ] 10.1 Document public API in header files
- [ ] 10.2 Update CHANGELOG.md with type checker implementation
