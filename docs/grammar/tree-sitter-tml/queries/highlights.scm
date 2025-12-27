; TML Tree-sitter Highlights
; For syntax highlighting in editors

; =============================================================================
; COMMENTS
; =============================================================================

(line_comment) @comment.line
(block_comment) @comment.block

; =============================================================================
; KEYWORDS
; =============================================================================

[
  "and"
  "as"
  "async"
  "await"
  "behavior"
  "break"
  "const"
  "continue"
  "crate"
  "decorator"
  "do"
  "else"
  "for"
  "func"
  "if"
  "impl"
  "in"
  "let"
  "loop"
  "mod"
  "mut"
  "not"
  "or"
  "pub"
  "quote"
  "ref"
  "return"
  "self"
  "super"
  "then"
  "this"
  "This"
  "through"
  "to"
  "type"
  "use"
  "when"
  "with"
] @keyword

; =============================================================================
; LITERALS
; =============================================================================

(bool_literal) @constant.builtin.boolean
(unit_literal) @constant.builtin

(int_literal) @constant.numeric.integer
(float_literal) @constant.numeric.float

(string_literal) @string
(basic_string) @string
(raw_string) @string
(char_literal) @string.special
(escape_sequence) @string.escape
(interpolation) @punctuation.special

; =============================================================================
; TYPES
; =============================================================================

(path_type (path) @type)
(never_type) @type.builtin

(generic_param (ident) @type.parameter)
(type_bound (path) @type)

; Built-in types
((path) @type.builtin
  (#match? @type.builtin "^(Bool|Char|String|Unit|Never|I8|I16|I32|I64|I128|U8|U16|U32|U64|U128|F32|F64|Maybe|Outcome|Vec|Map|Set|Heap|Shared|Sync)$"))

; =============================================================================
; FUNCTIONS
; =============================================================================

(func_def
  name: (ident) @function.definition)

(behavior_item
  name: (ident) @function.definition)

(method_call
  (ident) @function.method)

(call_expr) @function.call

; =============================================================================
; DECLARATIONS
; =============================================================================

(type_def
  name: (ident) @type.definition)

(behavior_def
  name: (ident) @type.definition)

(const_def
  name: (ident) @constant)

(mod_def
  name: (ident) @module)

(decorator_def
  name: (ident) @function.macro)

; =============================================================================
; VARIABLES & PARAMETERS
; =============================================================================

(param
  name: (ident) @variable.parameter)

(let_stmt
  (pattern (ident_pattern (ident) @variable)))

(for_expr
  (pattern (ident_pattern (ident) @variable)))

(closure_params (ident) @variable.parameter)

; =============================================================================
; FIELDS
; =============================================================================

(struct_field
  name: (ident) @property)

(field_access
  (ident) @property)

(field_init
  (ident) @property)

(field_pattern
  (ident) @property)

; =============================================================================
; VARIANTS
; =============================================================================

(variant
  name: (ident) @constructor)

(variant_pattern
  (path) @constructor)

; =============================================================================
; DECORATORS
; =============================================================================

(decorator
  "@" @punctuation.special
  (decorator_expr (path) @attribute))

; =============================================================================
; OPERATORS
; =============================================================================

[
  "+"
  "-"
  "*"
  "/"
  "%"
  "**"
  "=="
  "!="
  "<"
  ">"
  "<="
  ">="
  "&"
  "|"
  "^"
  "~"
  "<<"
  ">>"
  "="
  "+="
  "-="
  "*="
  "/="
  "%="
  "&="
  "|="
  "^="
  "<<="
  ">>="
  "!"
  "->"
  "=>"
] @operator

; =============================================================================
; PUNCTUATION
; =============================================================================

[
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @punctuation.bracket

[
  ","
  ";"
  ":"
  "::"
  "."
  ".."
  "@"
  "|"
] @punctuation.delimiter

; =============================================================================
; SPECIAL
; =============================================================================

(propagate) @operator.special

(splice
  ["${" "$" "}"] @punctuation.special)

; =============================================================================
; IMPORTS
; =============================================================================

(use_decl
  (use_path) @module)

(use_tree
  "*" @operator)

; =============================================================================
; VISIBILITY
; =============================================================================

(visibility) @keyword.modifier
