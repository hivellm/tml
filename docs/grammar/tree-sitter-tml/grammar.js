// Tree-sitter grammar for TML
// For editor support: syntax highlighting, folding, indentation
//
// This grammar prioritizes:
// - Error recovery (partial parse on invalid input)
// - Incremental re-parsing
// - Fast highlighting
//
// It MAY accept a superset of valid TML for better error messages.

module.exports = grammar({
  name: 'tml',

  // Tokens that can appear anywhere
  extras: $ => [
    /\s/,
    $.line_comment,
    $.block_comment,
  ],

  // Handle conflicts
  conflicts: $ => [
    [$.primary_expr, $.struct_init],
    [$.path, $.ident],
    [$._type, $.path],
  ],

  // External scanner for complex tokens (optional)
  externals: $ => [
    $._raw_string_content,
  ],

  // Word boundary for keywords
  word: $ => $.ident,

  rules: {
    // ==========================================================================
    // TOP LEVEL
    // ==========================================================================

    source_file: $ => seq(
      optional($.module_header),
      repeat($._item),
    ),

    module_header: $ => seq('module', $.ident, optional(';')),

    _item: $ => seq(
      repeat($.decorator),
      optional($.visibility),
      choice(
        $.func_def,
        $.type_def,
        $.const_def,
        $.behavior_def,
        $.impl_block,
        $.mod_def,
        $.decorator_def,
        $.use_decl,
      ),
    ),

    // ==========================================================================
    // DECLARATIONS
    // ==========================================================================

    func_def: $ => seq(
      'func',
      field('name', $.ident),
      optional($.generic_params),
      '(',
      optional($.params),
      ')',
      optional(seq('->', field('return_type', $._type))),
      optional($.effects),
      choice($.block, ';'),
    ),

    type_def: $ => seq(
      'type',
      field('name', $.ident),
      optional($.generic_params),
      '=',
      field('body', $._type_body),
    ),

    const_def: $ => seq(
      'const',
      field('name', $.ident),
      ':',
      field('type', $._type),
      '=',
      field('value', $._expr),
      optional(';'),
    ),

    behavior_def: $ => seq(
      'behavior',
      field('name', $.ident),
      optional($.generic_params),
      '{',
      repeat($.behavior_item),
      '}',
    ),

    behavior_item: $ => seq(
      repeat($.decorator),
      'func',
      field('name', $.ident),
      optional($.generic_params),
      '(',
      optional($.params),
      ')',
      optional(seq('->', field('return_type', $._type))),
      optional($.effects),
      choice($.block, ';'),
    ),

    impl_block: $ => seq(
      'impl',
      optional($.generic_params),
      field('type', $._type),
      optional(seq('for', field('trait', $._type))),
      '{',
      repeat($.impl_item),
      '}',
    ),

    impl_item: $ => seq(
      repeat($.decorator),
      optional($.visibility),
      $.func_def,
    ),

    mod_def: $ => seq(
      'mod',
      field('name', $.ident),
      choice(
        seq('{', repeat($._item), '}'),
        ';',
      ),
    ),

    decorator_def: $ => seq(
      'decorator',
      field('name', $.ident),
      optional($.decorator_params),
      '{',
      repeat($.func_def),
      '}',
    ),

    decorator_params: $ => seq(
      '(',
      optional(seq(
        $.decorator_param,
        repeat(seq(',', $.decorator_param)),
        optional(','),
      )),
      ')',
    ),

    decorator_param: $ => seq(
      field('name', $.ident),
      ':',
      field('type', $._type),
      optional(seq('=', field('default', $._expr))),
    ),

    use_decl: $ => seq(
      'use',
      $.use_path,
      optional(seq('as', $.ident)),
      optional(';'),
    ),

    use_path: $ => seq(
      $._path_segment,
      repeat(seq('::', choice($.use_tree, $._path_segment))),
    ),

    use_tree: $ => choice(
      seq('{', optional(seq($.use_path, repeat(seq(',', $.use_path)))), '}'),
      '*',
    ),

    // ==========================================================================
    // VISIBILITY
    // ==========================================================================

    visibility: $ => seq(
      'pub',
      optional(seq('(', $.vis_scope, ')')),
    ),

    vis_scope: $ => choice('crate', 'super', 'self', $.path),

    // ==========================================================================
    // GENERICS & PARAMS
    // ==========================================================================

    generic_params: $ => seq(
      '[',
      optional(seq(
        $.generic_param,
        repeat(seq(',', $.generic_param)),
        optional(','),
      )),
      ']',
    ),

    generic_param: $ => seq(
      field('name', $.ident),
      optional(seq(':', $.type_bound)),
    ),

    type_bound: $ => seq(
      $.path,
      repeat(seq('+', $.path)),
    ),

    params: $ => seq(
      $.param,
      repeat(seq(',', $.param)),
      optional(','),
    ),

    param: $ => seq(
      optional('mut'),
      field('name', $.ident),
      ':',
      field('type', $._type),
    ),

    // ==========================================================================
    // TYPES
    // ==========================================================================

    _type: $ => choice(
      $.func_type,
      $.reference_type,
      $.pointer_type,
      $.array_type,
      $.tuple_type,
      $.path_type,
      $.never_type,
    ),

    func_type: $ => seq(
      'func',
      '(',
      optional($.type_list),
      ')',
      '->',
      $._type,
      optional($.effects),
    ),

    reference_type: $ => seq(
      optional('mut'),
      'ref',
      $._type,
    ),

    pointer_type: $ => seq(
      '*',
      optional('mut'),
      $._type,
    ),

    array_type: $ => seq(
      '[',
      field('element', $._type),
      ';',
      field('size', $._expr),
      ']',
    ),

    tuple_type: $ => seq(
      '(',
      optional($.type_list),
      ')',
    ),

    path_type: $ => seq(
      $.path,
      optional($.generic_args),
    ),

    never_type: $ => 'Never',

    generic_args: $ => seq(
      '[',
      $.type_list,
      ']',
    ),

    type_list: $ => seq(
      $._type,
      repeat(seq(',', $._type)),
      optional(','),
    ),

    _type_body: $ => choice(
      $.sum_type,
      $.struct_type,
      $._type,
    ),

    sum_type: $ => seq(
      optional('|'),
      $.variant,
      repeat1(seq('|', $.variant)),
    ),

    variant: $ => seq(
      field('name', $.ident),
      optional($.variant_fields),
    ),

    variant_fields: $ => choice(
      seq('(', optional($.type_list), ')'),
      seq('{', optional($.struct_fields), '}'),
    ),

    struct_type: $ => seq(
      '{',
      optional($.struct_fields),
      '}',
    ),

    struct_fields: $ => seq(
      $.struct_field,
      repeat(seq(',', $.struct_field)),
      optional(','),
    ),

    struct_field: $ => seq(
      repeat($.decorator),
      optional($.visibility),
      field('name', $.ident),
      ':',
      field('type', $._type),
      optional(seq('=', field('default', $._expr))),
    ),

    // ==========================================================================
    // EFFECTS
    // ==========================================================================

    effects: $ => seq(
      'with',
      $.ident,
      repeat(seq(',', $.ident)),
    ),

    // ==========================================================================
    // DECORATORS
    // ==========================================================================

    decorator: $ => seq(
      '@',
      $.decorator_expr,
    ),

    decorator_expr: $ => seq(
      $.path,
      optional($.decorator_args),
    ),

    decorator_args: $ => seq(
      '(',
      optional(seq(
        $.decorator_arg,
        repeat(seq(',', $.decorator_arg)),
        optional(','),
      )),
      ')',
    ),

    decorator_arg: $ => seq(
      optional(seq($.ident, ':')),
      $._expr,
    ),

    // ==========================================================================
    // EXPRESSIONS
    // ==========================================================================

    _expr: $ => $.assignment_expr,

    assignment_expr: $ => prec.right(1, seq(
      $.or_expr,
      optional(seq($.assign_op, $.assignment_expr)),
    )),

    assign_op: $ => choice('=', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=', '>>='),

    or_expr: $ => prec.left(2, seq(
      $.and_expr,
      repeat(seq('or', $.and_expr)),
    )),

    and_expr: $ => prec.left(3, seq(
      $.comparison_expr,
      repeat(seq('and', $.comparison_expr)),
    )),

    comparison_expr: $ => prec.left(4, seq(
      $.bitor_expr,
      optional(seq($.compare_op, $.bitor_expr)),
    )),

    compare_op: $ => choice('==', '!=', '<=', '>=', '<', '>'),

    bitor_expr: $ => prec.left(5, seq(
      $.bitxor_expr,
      repeat(seq('|', $.bitxor_expr)),
    )),

    bitxor_expr: $ => prec.left(6, seq(
      $.bitand_expr,
      repeat(seq('^', $.bitand_expr)),
    )),

    bitand_expr: $ => prec.left(7, seq(
      $.shift_expr,
      repeat(seq('&', $.shift_expr)),
    )),

    shift_expr: $ => prec.left(8, seq(
      $.additive_expr,
      repeat(seq(choice('<<', '>>'), $.additive_expr)),
    )),

    additive_expr: $ => prec.left(9, seq(
      $.multiplicative_expr,
      repeat(seq(choice('+', '-'), $.multiplicative_expr)),
    )),

    multiplicative_expr: $ => prec.left(10, seq(
      $.power_expr,
      repeat(seq(choice('*', '/', '%'), $.power_expr)),
    )),

    power_expr: $ => prec.right(11, seq(
      $.unary_expr,
      optional(seq('**', $.power_expr)),
    )),

    unary_expr: $ => choice(
      seq($.unary_op, $.unary_expr),
      $.postfix_expr,
    ),

    unary_op: $ => choice('-', 'not', '~', 'ref', seq('mut', 'ref'), '*'),

    postfix_expr: $ => prec.left(12, seq(
      $.primary_expr,
      repeat($.postfix_op),
    )),

    postfix_op: $ => choice(
      $.call_expr,
      $.index_expr,
      $.field_access,
      $.method_call,
      $.propagate,
      $.await_expr,
      $.cast_expr,
    ),

    call_expr: $ => seq('(', optional($.arg_list), ')'),

    arg_list: $ => seq(
      $.arg,
      repeat(seq(',', $.arg)),
      optional(','),
    ),

    arg: $ => seq(
      optional(seq($.ident, ':')),
      $._expr,
    ),

    index_expr: $ => seq('[', $._expr, ']'),

    field_access: $ => seq('.', $.ident),

    method_call: $ => seq(
      '.',
      $.ident,
      optional($.generic_args),
      '(',
      optional($.arg_list),
      ')',
    ),

    propagate: $ => '!',

    await_expr: $ => seq('.', 'await'),

    cast_expr: $ => seq('as', $._type),

    // ==========================================================================
    // PRIMARY EXPRESSIONS
    // ==========================================================================

    primary_expr: $ => choice(
      $._literal,
      'this',
      $.parenthesized_expr,
      $.tuple_expr,
      $.block,
      $.if_expr,
      $.when_expr,
      $.loop_expr,
      $.for_expr,
      $.return_expr,
      $.break_expr,
      $.continue_expr,
      $.closure,
      $.struct_init,
      $.array_init,
      $.quote_expr,
      $.path,
    ),

    parenthesized_expr: $ => seq('(', $._expr, ')'),

    tuple_expr: $ => seq(
      '(',
      $._expr,
      ',',
      optional(seq($._expr, repeat(seq(',', $._expr)), optional(','))),
      ')',
    ),

    // ==========================================================================
    // CONTROL FLOW
    // ==========================================================================

    if_expr: $ => prec.right(seq(
      'if',
      choice(
        seq(optional('let'), $.pattern, '=', $._expr),
        $._expr,
      ),
      'then',
      $._expr,
      optional(seq('else', $._expr)),
    )),

    when_expr: $ => seq(
      'when',
      $._expr,
      '{',
      repeat($.match_arm),
      '}',
    ),

    match_arm: $ => seq(
      $.pattern,
      optional(seq('if', $._expr)),
      '->',
      $._expr,
      optional(','),
    ),

    loop_expr: $ => seq(
      'loop',
      optional($._expr),
      $.block,
    ),

    for_expr: $ => seq(
      'for',
      $.pattern,
      'in',
      $._expr,
      $.block,
    ),

    return_expr: $ => prec.right(seq(
      'return',
      optional($._expr),
    )),

    break_expr: $ => prec.right(seq(
      'break',
      optional($.ident),
      optional($._expr),
    )),

    continue_expr: $ => seq(
      'continue',
      optional($.ident),
    ),

    // ==========================================================================
    // BLOCKS & STATEMENTS
    // ==========================================================================

    block: $ => seq(
      '{',
      repeat($._statement),
      optional($._expr),
      '}',
    ),

    _statement: $ => choice(
      $.let_stmt,
      $.expr_stmt,
      $._item,
      ';',
    ),

    let_stmt: $ => seq(
      'let',
      optional('mut'),
      $.pattern,
      optional(seq(':', $._type)),
      '=',
      $._expr,
      optional(';'),
    ),

    expr_stmt: $ => seq($._expr, ';'),

    // ==========================================================================
    // PATTERNS
    // ==========================================================================

    pattern: $ => $.or_pattern,

    or_pattern: $ => prec.left(seq(
      $._pattern_no_or,
      repeat(seq('|', $._pattern_no_or)),
    )),

    _pattern_no_or: $ => $.guard_pattern,

    guard_pattern: $ => seq(
      $._primary_pattern,
      optional(seq('if', $._expr)),
    ),

    _primary_pattern: $ => choice(
      $._literal,
      $.wildcard_pattern,
      $.range_pattern,
      $.ident_pattern,
      $.tuple_pattern,
      $.struct_pattern,
      $.variant_pattern,
      $.ref_pattern,
    ),

    wildcard_pattern: $ => '_',

    range_pattern: $ => seq(
      $._literal,
      choice('to', 'through'),
      $._literal,
    ),

    ident_pattern: $ => seq(
      optional('mut'),
      $.ident,
      optional(seq('@', $.pattern)),
    ),

    tuple_pattern: $ => seq(
      '(',
      optional(seq($.pattern, repeat(seq(',', $.pattern)), optional(','))),
      ')',
    ),

    struct_pattern: $ => seq(
      $.path,
      '{',
      optional($.field_patterns),
      optional('..'),
      '}',
    ),

    field_patterns: $ => seq(
      $.field_pattern,
      repeat(seq(',', $.field_pattern)),
      optional(','),
    ),

    field_pattern: $ => seq(
      $.ident,
      optional(seq(':', $.pattern)),
    ),

    variant_pattern: $ => seq(
      $.path,
      optional(seq(
        '(',
        optional(seq($.pattern, repeat(seq(',', $.pattern)), optional(','))),
        ')',
      )),
    ),

    ref_pattern: $ => seq('ref', optional('mut'), $.pattern),

    // ==========================================================================
    // CLOSURES
    // ==========================================================================

    closure: $ => seq(
      'do',
      optional($.closure_params),
      optional(seq('->', $._type)),
      choice($.block, $._expr),
    ),

    closure_params: $ => choice(
      seq('(', optional($.params), ')'),
      $.ident,
    ),

    // ==========================================================================
    // STRUCT & ARRAY INIT
    // ==========================================================================

    struct_init: $ => seq(
      $.path,
      '{',
      optional($.field_inits),
      optional(seq('..', $._expr)),
      '}',
    ),

    field_inits: $ => seq(
      $.field_init,
      repeat(seq(',', $.field_init)),
      optional(','),
    ),

    field_init: $ => seq(
      $.ident,
      optional(seq(':', $._expr)),
    ),

    array_init: $ => seq(
      '[',
      optional(choice(
        seq($._expr, repeat(seq(',', $._expr)), optional(',')),
        seq($._expr, ';', $._expr),
      )),
      ']',
    ),

    // ==========================================================================
    // QUOTE (METAPROGRAMMING)
    // ==========================================================================

    quote_expr: $ => seq(
      'quote',
      '{',
      repeat($.quote_content),
      '}',
    ),

    quote_content: $ => choice(
      $.splice,
      $.quote_token,
    ),

    splice: $ => choice(
      seq('${', $._expr, '}'),
      seq('$', $.ident),
    ),

    quote_token: $ => /[^${}]+/,

    // ==========================================================================
    // LITERALS
    // ==========================================================================

    _literal: $ => choice(
      $.string_literal,
      $.char_literal,
      $.number_literal,
      $.bool_literal,
      $.unit_literal,
    ),

    string_literal: $ => choice(
      $.raw_string,
      $.basic_string,
    ),

    basic_string: $ => seq(
      '"',
      repeat(choice(
        $.escape_sequence,
        $.string_content,
        $.interpolation,
      )),
      '"',
    ),

    string_content: $ => /[^"\\{]+/,

    interpolation: $ => seq('{', $._expr, '}'),

    raw_string: $ => seq(
      '"""',
      /[^"]*(""?[^"]*)*/,
      '"""',
    ),

    escape_sequence: $ => token(seq(
      '\\',
      choice(
        /[nrt\\"0]/,
        /x[0-9a-fA-F]{2}/,
        /u\{[0-9a-fA-F]+\}/,
      ),
    )),

    char_literal: $ => seq(
      "'",
      choice($.escape_sequence, /[^'\\]/),
      "'",
    ),

    number_literal: $ => choice(
      $.float_literal,
      $.int_literal,
    ),

    float_literal: $ => token(seq(
      /[0-9][0-9_]*/,
      choice(
        seq('.', /[0-9][0-9_]*/, optional(/[eE][+-]?[0-9][0-9_]*/)),
        /[eE][+-]?[0-9][0-9_]*/,
      ),
      optional(/_?(f32|f64)/),
    )),

    int_literal: $ => token(seq(
      choice(
        /0x[0-9a-fA-F][0-9a-fA-F_]*/,
        /0o[0-7][0-7_]*/,
        /0b[01][01_]*/,
        /[0-9][0-9_]*/,
      ),
      optional(/_?(i8|i16|i32|i64|i128|u8|u16|u32|u64|u128)/),
    )),

    bool_literal: $ => choice('true', 'false'),

    unit_literal: $ => seq('(', ')'),

    // ==========================================================================
    // IDENTIFIERS & PATHS
    // ==========================================================================

    path: $ => seq(
      $._path_segment,
      repeat(seq('::', $._path_segment)),
    ),

    _path_segment: $ => choice(
      $.ident,
      'crate',
      'super',
      'self',
      'This',
    ),

    ident: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

    // ==========================================================================
    // COMMENTS
    // ==========================================================================

    line_comment: $ => token(seq('//', /.*/)),

    block_comment: $ => token(seq(
      '/*',
      /[^*]*\*+([^/*][^*]*\*+)*/,
      '/',
    )),
  },
});
