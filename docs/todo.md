# done

* on the fly constant evaluation
* dynamic arrays (wide array pointers)
* array.length
* automatic pointer unpacking on subscript and member access
* string type (length and char-ptr)
* print strings
* argv
* export (vars, functions, types)
* else-if
* binops == != <= >= < >
* binops + - * // %
* function prototypes
* compare strings equal
* function parameters
* codegen: externalize runtime.h
* refactor: use DeclFlags
* new operator
* import (
	import "./file.ja"; # only side effects
	import sym, sym2 from "./some/file.ja"; # exclusive symbols
	)
* break, continue
* use default struct member inits
* for-each on arrays (for x in arr {...})

# wip

* ffi
	X load dlls
	X load functions
	X load variables
	check that funcs are protos
	check that vars are pointer types
	runtime check loaded dll and symbols
* self hosting

# for self hosting

* binops logical and, or
* binop float division /
* unions
* tagged unions
* enums
* switch-case
* delete/resize
* variable arguments

# todo

* break, continue with labels
* import (
	import * from "./myfile.ja"; # all exported symbols
	import "./file.ja" as unit; # import in namespace
	)
* refer to struct before definition (if ptr to)
* auto pointer unpacking/packing
* print arrays
* print structs
* print ptrs contents
* print multiple arguments
* chained comparison (a == b > c  is  a == b && b > c)
* short-circuiting and, or
* codegen: spare () on equal level binop chains (except explicit ())
* owned ptr type !int (deletes automatically at scope end
	; must also delete before return, break or continue)
* explicit uninitialized var (var x : int = ?)
* for range (for x = 0..1 {} for x = 5 .. <1 {})
* type-ids
* type-alias definition
* build: cache C files, compiled objects
* float types
* correct float to string conversion
* anonymous structs, unions
* use/mixin/with
* forced inline
* optional arguments
* codegen: no ja_ prefix for members

# ideas

* file functions
* error type
* namespaces
* external methods
* any type
* pseudo ptr arithmetic via @ index
* constants
