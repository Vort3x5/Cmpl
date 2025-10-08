; Keywords
[
  "return"
  "if"
  "else"
  "for"
  "while"
  "struct"
] @keyword

; Operators
[
  "::"
  ":"
  ":="
  "="
  "->"
  "+"
  "-"
  "*"
  "/"
  "%"
  "=="
  "!="
  "<"
  "<="
  ">"
  ">="
  "!"
] @operator

; Punctuation
[
  "("
  ")"
  "{"
  "}"
  "["
  "]"
  "."
  ","
  ";"
] @punctuation.bracket

; Procedure Declarations
(procedure_declaration
  name: (identifier) @function)

; Struct Declarations
(struct_declaration
  name: (identifier) @type)

; Function Calls
(call_expression
  function: (identifier) @function.call)

; Numbers and Comments
(number) @number
(comment) @comment
