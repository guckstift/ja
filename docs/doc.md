/*
	* a variable's name must not be used before or in the initializer of its
	  own declaration
	* a function name or a type name however is allowed to be used before its
	  declaration
	* when a function uses a variable from an outer scope, all occurrences of
	  the function name must appear after the variable declaration
	* a named type can be used before or inside its own declaration, but only
	  as the target type of a pointer type
*/

var y = x; # error: x is declared later
var x = "Hello";

# -----------------------------------------------------------------------------

main(); # ok: main is declared later

function main()
{
	print "Hello";
}

# -----------------------------------------------------------------------------

function foo()
{
	print x; # error: x is not declared
}

# -----------------------------------------------------------------------------

function foo()
{
	print x; # error: x is declared later
}

var x = "Hello";

# -----------------------------------------------------------------------------

function bar()
{
	foo(); # error: foo uses x which is declared later
}

var x = "Hello";

function foo()
{
	print x;
}

# -----------------------------------------------------------------------------

function bar()
{
	return foo; // error: foo uses x which is declared later
}

var x = "Hello";

function foo()
{
	print x;
}
