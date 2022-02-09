# done

* on the fly constant evaluation
* dynamic arrays (wide array pointers)
* array.length
* automatic pointer unpacking on subscript and member access
* string type (length and char-ptr)
* print strings

# todo

* namespaces
* function parameters
* else-if
* binops + - * / // %
* binops == != <= >= < >
* binops logical and, or
* function prototypes
* refer to struct before definition (if ptr to)
* export (vars, functions, types)
* import (
	import "./file.ja"; # only side effects
	import sym from "./some/file.ja"; # exclusive symbols
	import * from "./myfile.ja"; # all exported symbols
	import "./file.ja" as unit; # import in namespace
	)
* new/delete/resize
* auto pointer unpacking/packing
* pseudo ptr arithmetic via @ index
* print arrays
* print structs
* print ptrs contents
* print multiple arguments
* chained comparison (a == b > c  is  a == b && b > c)
* short-circuiting and, or
* codegen: spare () on equal level binop chains (except explicit ())
* owned ptr type !int (deletes automatically at scope end)
* explicit uninitialized var (var x : int = ?)
* for range (for x = 0..1 {} for x = 5 .. <1 {})
* for-each on arrays (for x in arr {...})
* unions
* tagged unions
* enums
* any type
* type-ids
* type-alias definition
* switch-case
* ffi
* build: cache C files, compiled objects
* self-hosted compiler
* float types
* correct float to string conversion
* break, continue (+ labeled)
* anonymous structs, unions
* use/mixin/with
* namespaces
* external methods
* forced inline
* argv
* file functions
* optional arguments
* variable arguments
* error type
* constants
* codegen: no ja_ prefix for members
