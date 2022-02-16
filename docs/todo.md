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

# wip

* import (
	X import "./file.ja"; # only side effects
	X import sym, sym2 from "./some/file.ja"; # exclusive symbols
	import * from "./myfile.ja"; # all exported symbols
	import "./file.ja" as unit; # import in namespace
	)

# for self hosting

* binops logical and, or
* binop float division /
* unions
* tagged unions
* enums
* switch-case
* break, continue (+ labeled)
* file functions
* new/delete/resize
* variable arguments

# todo

* codegen: externalize runtime.h
* use default struct member inits
* refer to struct before definition (if ptr to)
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
* any type
* type-ids
* type-alias definition
* ffi
* build: cache C files, compiled objects
* self hosting
* float types
* correct float to string conversion
* anonymous structs, unions
* use/mixin/with
* namespaces
* external methods
* forced inline
* optional arguments
* error type
* constants
* codegen: no ja_ prefix for members
