# current state

## language

### keywords

* int8 int16 int32 int64 uint8 uint16 uint32 uint64
* bool true false

### types

* int8 int16 int32 int64 uint8 uint16 uint32 uint64
* int = int64, uint = uint64
* bool (1 byte)

### statements

* vardecl:
	* var IDENT ;
	* var IDENT = expr ;
	* var IDENT : type ;
	* var IDENT : type = expr ;
* assign
	* expr1 = expr2 ;
		* expr1 must be an lvalue