; Keywords
[
  "return"
  "if"
  "else"
  "for"
  "while"
  "struct"
  "defer"
  "foreign"
  "library"
  "system_library"
] @keyword

; Operators
[
  "::"
  ":"
  ":="
  "="
  "->"
  ".."
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
  "<<"
  "#"
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

; Library / foreign
(library_declaration
  name: (identifier) @constant)

(foreign_clause
  library: (identifier) @constant)

; Procedures
(procedure_declaration
  name: (identifier) @function)

; Structs / types
(struct_declaration
  name: (identifier) @type)

(type (identifier) @type)
(pointer_type "*" @operator)
(array_type) @type

; Calls
(call_expression
  function: (identifier) @function.call)

; Literals
(number) @number
(string) @string
(comment) @comment
(identifier) @variable
