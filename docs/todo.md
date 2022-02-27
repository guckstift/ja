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
* delete
* binops & | ^
* binops logical and, or
* pass arrays by value

# wip

* ffi
	X load dlls
	X load functions
	X load variables
	X runtime check loaded dll and symbols
	X check that funcs are protos
	X check that vars have no init
	(?check that vars are pointer types?)
* self hosting
* enums

# for self hosting

* binop float division /
* unions
* tagged unions
* switch-case
* resize operator for dyn arrays
* variable arguments

# todo

* default struct member inits for new
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

# ideas

* codegen: no ja_ prefix for members
* file functions
* error type
* namespaces
* external methods
* any type
* pseudo ptr arithmetic via @ index
* constants
