# Stdlib Core Specification

## Purpose

The standard library core provides foundational types and traits that all TML programs depend on. This includes primitive operations, core traits, and essential data structures.

## ADDED Requirements

### Requirement: Core Traits
The stdlib SHALL define core traits for common operations.

#### Scenario: Copy trait
Given a type implementing Copy
When the value is assigned to another variable
Then the value is copied rather than moved

#### Scenario: Clone trait
Given a type implementing Clone
When clone() is called
Then a deep copy of the value is produced

#### Scenario: Drop trait
Given a type implementing Drop
When the value goes out of scope
Then drop() is called to clean up resources

#### Scenario: Eq trait
Given two values of a type implementing Eq
When compared with == or !=
Then correct equality result is returned

#### Scenario: Ord trait
Given two values of a type implementing Ord
When compared with <, >, <=, >=
Then correct ordering result is returned

#### Scenario: Hash trait
Given a value of a type implementing Hash
When hash() is called with a hasher
Then consistent hash value is produced

#### Scenario: Display trait
Given a value of a type implementing Display
When to_string() is called
Then human-readable string representation is produced

#### Scenario: Debug trait
Given a value of a type implementing Debug
When debug_string() is called
Then debug string representation is produced

### Requirement: Primitive Operations
The stdlib MUST provide operations for all primitive types.

#### Scenario: Integer arithmetic
Given I32 values a and b
When arithmetic operations (+, -, *, /, %) are performed
Then correct results are produced with overflow checking

#### Scenario: Integer comparison
Given I32 values a and b
When comparison operations are performed
Then correct boolean results are returned

#### Scenario: Float arithmetic
Given F64 values a and b
When arithmetic operations are performed
Then IEEE 754 compliant results are produced

#### Scenario: Bool operations
Given Bool values
When and, or, not operations are performed
Then correct logical results are returned

#### Scenario: Char operations
Given Char value
When Unicode operations are performed
Then correct Unicode-aware results are returned

### Requirement: Option Type
The stdlib SHALL provide Option[T] for optional values.

#### Scenario: Some value
Given `Option.Some(42)`
When is_some() is called
Then true is returned

#### Scenario: None value
Given `Option.None`
When is_none() is called
Then true is returned

#### Scenario: Unwrap Some
Given `Option.Some(42)`
When unwrap() is called
Then 42 is returned

#### Scenario: Unwrap None
Given `Option.None`
When unwrap() is called
Then panic occurs

#### Scenario: Map Some
Given `Option.Some(42)` and function `|x| x * 2`
When map() is called
Then `Option.Some(84)` is returned

#### Scenario: Map None
Given `Option.None` and any function
When map() is called
Then `Option.None` is returned

#### Scenario: And then chaining
Given `Option.Some(42)` and function returning Option
When and_then() is called
Then function result is returned (flattened)

### Requirement: Result Type
The stdlib SHALL provide Result[T, E] for error handling.

#### Scenario: Ok value
Given `Result.Ok(42)`
When is_ok() is called
Then true is returned

#### Scenario: Err value
Given `Result.Err("error")`
When is_err() is called
Then true is returned

#### Scenario: Unwrap Ok
Given `Result.Ok(42)`
When unwrap() is called
Then 42 is returned

#### Scenario: Unwrap Err
Given `Result.Err("error")`
When unwrap() is called
Then panic occurs with error message

#### Scenario: Map Ok
Given `Result.Ok(42)` and function
When map() is called
Then function is applied to value

#### Scenario: Map Err
Given `Result.Err("error")` and function
When map_err() is called
Then function is applied to error

#### Scenario: Question mark operator
Given function returning Result and expression with ?
When expression produces Err
Then function returns early with that Err

### Requirement: Vec Type
The stdlib MUST provide Vec[T] for dynamic arrays.

#### Scenario: Create empty
Given `Vec.new()`
When len() is called
Then 0 is returned

#### Scenario: Push element
Given empty Vec and value
When push() is called
Then len() returns 1 and element is accessible

#### Scenario: Pop element
Given Vec with elements
When pop() is called
Then last element is returned as Option and removed

#### Scenario: Index access
Given Vec with elements
When accessed by index
Then correct element is returned (with bounds check)

#### Scenario: Iteration
Given Vec with elements
When iter() is called
Then iterator over elements is returned

#### Scenario: Drop behavior
Given Vec going out of scope
When drop is called
Then all elements are dropped and memory freed

### Requirement: String Type
The stdlib SHALL provide String for UTF-8 text.

#### Scenario: Create from literal
Given string literal "hello"
When String is created
Then valid UTF-8 String is produced

#### Scenario: Push char
Given String and Char
When push() is called
Then char is appended in UTF-8 encoding

#### Scenario: Push str
Given String and str slice
When push_str() is called
Then slice contents are appended

#### Scenario: Length
Given String with content
When len() is called
Then byte length is returned

#### Scenario: Chars iterator
Given String "héllo"
When chars() is called
Then iterator yields Unicode code points

#### Scenario: Bytes iterator
Given String
When bytes() is called
Then iterator yields raw UTF-8 bytes

### Requirement: HashMap Type
The stdlib MUST provide HashMap[K, V] for key-value storage.

#### Scenario: Create empty
Given `HashMap.new()`
When len() is called
Then 0 is returned

#### Scenario: Insert entry
Given HashMap and key-value pair
When insert() is called
Then entry is stored and old value returned if any

#### Scenario: Get entry
Given HashMap with entries
When get() is called with key
Then Option with value is returned

#### Scenario: Remove entry
Given HashMap with entries
When remove() is called with key
Then entry is removed and value returned

#### Scenario: Contains key
Given HashMap
When contains_key() is called
Then true if key exists, false otherwise

#### Scenario: Iteration
Given HashMap with entries
When iter() is called
Then iterator over (key, value) pairs is returned

### Requirement: Iterator Trait
The stdlib SHALL provide Iterator trait for sequences.

#### Scenario: Map method
Given iterator and transformation function
When map() is called
Then new iterator applying function is returned

#### Scenario: Filter method
Given iterator and predicate function
When filter() is called
Then new iterator with matching elements is returned

#### Scenario: Collect method
Given iterator
When collect() is called
Then elements are collected into target collection

#### Scenario: For each
Given iterator and side-effect function
When for_each() is called
Then function is called on each element

### Requirement: Memory Safety
All stdlib types MUST maintain memory safety.

#### Scenario: No use after free
Given any stdlib type
When used according to API
Then no use-after-free is possible

#### Scenario: No double free
Given any stdlib type with Drop
When drop occurs
Then memory is freed exactly once

#### Scenario: Bounds checking
Given indexed access to Vec or String
When index is out of bounds
Then panic occurs (not undefined behavior)
