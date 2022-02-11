# syntax

## statements

```
unit = stmts

stmts = stmt*

stmt =
	print | var | func | struct | if | while | return | import | export |
	assign | call

print = "print" expr ";"

var = "var" IDENT ( ":" type )? ( "=" expr )? ";"

func = "function" IDENT "(" ")" ( ":" type )? "{" stmts "}"

struct = "struct" IDENT "{" vars "}"

if = "if" expr "{" stmts "}" ( "else" "{" stmts "}" )?

while = "while" expr "{" stmts "}"

return = "return" expr ";"

import = "import" ( IDENT ("," IDENT)* "from" )? STRING ";"

export = "export" ( var | func | struct )

assign = expr "=" expr ";"

call = expr "(" ")" ";"
```

## types

```
type = primtype | nametype | ptrtype | arraytype

primtype =
	"int" | "int8" | "int16" | "int32" | "int64" |
	"uint" | "uint8" | "uint16" | "uint32" | "uint64" |
	"bool" | "string"

nametype = IDENT

ptrtype = ">" type

arraytype = "[" INT "]" type
```

## expressions

```
expr = binop

binop = prefix ( ("+" | "-") prefix )*

prefix = ptr | deref | postfix

ptr = ">" prefix

deref = "<" prefix

postfix = atom ( postfix_x )*

postfix_x = cast_x | call_x | subscript_x | member_x

cast_x = "as" type

call_x = "(" ")"

subscript_x = "[" index "]"

member_x = "." IDENT

atom = INT | "false" | "true" | STRING | "(" expr ")" | IDENT | array

array = "[" ( expr ("," expr)* )? "]"
```
