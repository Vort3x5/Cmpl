// Tree-sitter grammar for Cmpl's Jai subset (aligned with examples/BasicJai).
module.exports = grammar({
  name: 'jai',

  extras: $ => [
    /\s/,
    $.comment,
  ],

  word: $ => $.identifier,

  conflicts: $ => [
    // `*x` can start a type or an address-of expression in edge cases
    [$.pointer_type, $.unary_expression],
  ],

  rules: {
    source_file: $ => repeat($._declaration),

    _declaration: $ => choice(
      $.library_declaration,
      $.procedure_declaration,
      $.struct_declaration,
      $._statement,
    ),

    // name :: #library "path";
    // name :: #system_library "vulkan";
    library_declaration: $ => seq(
      field('name', $.identifier),
      '::',
      '#',
      field('kind', choice('library', 'system_library')),
      field('path', $.string),
      ';',
    ),

    // name :: (params) -> T { body }
    // name :: (params) -> T #foreign lib;
    // name :: (params) #foreign lib;
    // name :: (params) -> T #foreign lib "CSymbol";
    procedure_declaration: $ => seq(
      field('name', $.identifier),
      '::',
      '(',
      optional($.parameter_list),
      ')',
      optional(seq('->', field('return_type', $.type))),
      choice(
        field('body', $.block),
        $.foreign_clause,
      ),
    ),

    foreign_clause: $ => seq(
      '#',
      'foreign',
      field('library', $.identifier),
      optional(field('symbol', $.string)),
      ';',
    ),

    parameter_list: $ => seq(
      $.parameter,
      repeat(seq(',', $.parameter)),
    ),

    parameter: $ => seq(
      field('name', $.identifier),
      optional(seq(':', field('type', $.type))),
    ),

    // Name :: struct { fields }
    struct_declaration: $ => seq(
      field('name', $.identifier),
      '::',
      'struct',
      '{',
      repeat($.struct_field),
      '}',
    ),

    struct_field: $ => seq(
      field('name', $.identifier),
      ':',
      field('type', $.type),
      ';',
    ),

    // Types: int | *Type | [N]Type
    type: $ => choice(
      $.identifier,
      $.pointer_type,
      $.array_type,
    ),

    pointer_type: $ => prec.left(2, seq('*', field('to', $.type))),

    array_type: $ => seq(
      '[',
      field('length', $.number),
      ']',
      field('element', $.type),
    ),

    // Statements
    _statement: $ => choice(
      $.block,
      $.return_statement,
      $.if_statement,
      $.for_statement,
      $.while_statement,
      $.defer_statement,
      $.typed_declaration,
      $.assignment_statement,
      $.expression_statement,
    ),

    block: $ => seq(
      '{',
      repeat($._statement),
      '}',
    ),

    return_statement: $ => seq(
      'return',
      optional($._expression),
      ';',
    ),

    defer_statement: $ => seq(
      'defer',
      field('body', $._statement),
    ),

    // Condition must prefer full exprs like `f(0) > 0` over bare `f`.
    if_statement: $ => prec.right(seq(
      'if',
      field('condition', prec.dynamic(1, $._expression)),
      field('consequence', $._statement),
      optional(seq('else', field('alternative', $._statement))),
    )),

    for_statement: $ => seq(
      'for',
      optional(field('reverse', '<')),
      optional(seq(field('iterator', $.identifier), ':')),
      field('start', $._expression),
      '..',
      field('end', $._expression),
      field('body', $._statement),
    ),

    while_statement: $ => seq(
      'while',
      field('condition', prec.dynamic(1, $._expression)),
      field('body', $._statement),
    ),

    // name: Type;
    typed_declaration: $ => seq(
      field('name', $.identifier),
      ':',
      field('type', $.type),
      ';',
    ),

    assignment_statement: $ => choice(
      // name := value;
      seq(
        field('left', $.identifier),
        ':=',
        field('right', $._expression),
        ';',
      ),
      // lhs = rhs;  (includes ptr.* = val, arr[i] = val, p.x = val)
      seq(
        field('left', $._expression),
        '=',
        field('right', $._expression),
        ';',
      ),
    ),

    expression_statement: $ => seq(
      $._expression,
      ';',
    ),

    // Expressions — layered so `f(0) > 0` parses as a comparison of a call.
    _expression: $ => choice(
      $.binary_expression,
      $.unary_expression,
      $._postfix_expression,
    ),

    binary_expression: $ => {
      const table = [
        [prec.left, 1, choice('==', '!=')],
        [prec.left, 2, choice('<', '<=', '>', '>=')],
        [prec.left, 3, choice('+', '-')],
        [prec.left, 4, choice('*', '/', '%')],
      ];

      return choice(...table.map(([assoc, precedence, operator]) =>
        assoc(precedence, seq(
          field('left', $._expression),
          field('operator', operator),
          field('right', $._expression),
        ))
      ));
    },

    // ! - * (addr-of); deref is postfix .*
    unary_expression: $ => prec.right(5, seq(
      field('operator', choice('!', '-', '*')),
      field('operand', $._expression),
    )),

    _postfix_expression: $ => choice(
      $.call_expression,
      $.index_expression,
      $.field_access,
      $.deref_expression,
      $._primary_expression,
    ),

    // The_Way_to_Jai §10: ptr.*
    deref_expression: $ => prec(10, seq(
      field('operand', $._postfix_expression),
      '.*',
    )),

    call_expression: $ => prec(10, seq(
      field('function', $._postfix_expression),
      '(',
      optional($.argument_list),
      ')',
    )),

    argument_list: $ => seq(
      $._expression,
      repeat(seq(',', $._expression)),
    ),

    index_expression: $ => prec(10, seq(
      field('array', $._postfix_expression),
      '[',
      field('index', $._expression),
      ']',
    )),

    field_access: $ => prec(10, seq(
      field('object', $._postfix_expression),
      '.',
      field('field', $.identifier),
    )),

    _primary_expression: $ => choice(
      $.parenthesized_expression,
      $.number,
      $.string,
      $.identifier,
    ),

    parenthesized_expression: $ => seq(
      '(',
      $._expression,
      ')',
    ),

    identifier: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

    number: $ => /\d+/,

    string: $ => /"([^"\\]|\\.)*"/,

    comment: $ => token(choice(
      seq('//', /.*/),
      seq('/*', /[^*]*\*+([^/*][^*]*\*+)*/, '/'),
    )),
  },
});
