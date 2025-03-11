# structures

(1) declaring a structure type `StructName`:

```
struct StructName {
	var member1 : int;
	var member2 : bool;
}
```

(2) declaring a variable `my_struct` of a named structure type `StructName`:

```
var my_struct : StructName;
```

(3) declaring a variable `my_struct` of an anonymous structure type:

```
var my_struct : struct {
	var member1 : int;
	var member2 : bool;
};
```

(4) declaring an anonymous structure:

```
struct {
	var member1 : int;
	var member2 : bool;
}
```

The members belong to the outer scope containing the structure.
