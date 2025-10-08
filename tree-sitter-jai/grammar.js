module.exports = grammar({
  name: 'jai',

  extras: $ => [
    /\s/,
    $.comment,
  ],

  word: $ => $.identifier,

  rules: {
    source_file: $ => repeat($._declaration),

    _declaration: $ => choice(
      $.procedure_declaration,
      $.struct_declaration,
      $._statement,
    ),

    // Procedure: name :: (params) -> return_type { body }
    procedure_declaration: $ => seq(
      field('name', $.identifier),
      '::',
      '(',
      optional($.parameter_list),
      ')',
      optional(seq('->', field('return_type', $.identifier))),
      field('body', $.block),
    ),

    parameter_list: $ => seq(
      $.parameter,
      repeat(seq(',', $.parameter)),
    ),

    parameter: $ => seq(
      field('name', $.identifier),
      optional(seq(':', field('type', $.identifier))),
    ),

    // Struct: Name :: struct { fields }
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
      field('type', $.identifier),
      ';',
    ),

    // Statements
    _statement: $ => choice(
      $.block,
      $.return_statement,
      $.if_statement,
      $.for_statement,
      $.while_statement,
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

    if_statement: $ => prec.right(seq(
      'if',
      field('condition', choice(
        seq('(', $._expression, ')'),
        $._expression,
      )),
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
      field('condition', choice(
        seq('(', $._expression, ')'),
        $._expression,
      )),
      field('body', $._statement),
    ),

    assignment_statement: $ => choice(
      // Variable declaration: name := value;
      seq(
        field('left', $.identifier),
        ':=',
        field('right', $._expression),
        ';',
      ),
      // Regular assignment: lhs = rhs;
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

    // Expressions (with proper precedence)
    _expression: $ => choice(
      $.binary_expression,
      $.unary_expression,
      $.call_expression,
      $.index_expression,
      $.field_access,
      $.parenthesized_expression,
      $.number,
      $.identifier,
    ),

    binary_expression: $ => {
      const table = [
        [prec.left, 1, choice('==', '!=')],                    // equality
        [prec.left, 2, choice('<', '<=', '>', '>=')],          // comparison
        [prec.left, 3, choice('+', '-')],                      // term
        [prec.left, 4, choice('*', '/', '%')],                 // factor
      ];

      return choice(...table.map(([assoc, precedence, operator]) =>
        assoc(precedence, seq(
          field('left', $._expression),
          field('operator', operator),
          field('right', $._expression),
        ))
      ));
    },

    unary_expression: $ => prec.right(5, seq(
      field('operator', choice('!', '-')),
      field('operand', $._expression),
    )),

    call_expression: $ => prec(10, seq(
      field('function', $._expression),
      '(',
      optional($.argument_list),
      ')',
    )),

    argument_list: $ => seq(
      $._expression,
      repeat(seq(',', $._expression)),
    ),

    index_expression: $ => prec(10, seq(
      field('array', $._expression),
      '[',
      field('index', $._expression),
      ']',
    )),

    field_access: $ => prec(10, seq(
      field('object', $._expression),
      '.',
      field('field', $.identifier),
    )),

    parenthesized_expression: $ => prec(1, seq(
      '(',
      $._expression,
      ')',
    )),

    // Terminals
    identifier: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

    number: $ => /\d+/,

    comment: $ => token(choice(
      seq('//', /.*/),
      seq('/*', /[^*]*\*+([^/*][^*]*\*+)*/, '/'),
    )),
  },
});
