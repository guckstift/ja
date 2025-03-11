# unions

(1) declaring a union type `UnionName`:

```
union UnionName {
	var member1 : int;
	var member2 : bool;
}
```

(2) declaring a variable `my_union` of a named union type `UnionName`:

```
var my_union : UnionName;
```

(3) declaring a variable `my_union` of an anonymous union type:

```
var my_union : union {
	var member1 : int;
	var member2 : bool;
};
```

(4) declaring an anonymous union:

```
union {
	var member1 : int;
	var member2 : bool;
}
```

The members belong to the outer scope containing the union.

## tags

Unions are tagged by default.
The tag field is located right before the actual member space.
The tag values are automatically assigned to the members starting from zero counting up by 1 for each member.
The default type of the tag field is `uint64`.

The union definition can include a tag type specification:

```
union(uint8) UnionName {
	var member1 : int;
	var member2 : bool;
}
```

(5) Anonymous unions like in (3) and (4) can be tagged by another variable from the same scope containing the union:

```
var my_tag : int;
var my_union : union(my_tag) {
	0 => var member1 : int;
	1 => var member2 : bool;
};
```

or

```
var my_tag : int;
union(my_tag) {
	0 => var member1 : int;
	1 => var member2 : bool;
}
```

In the examples above the variable `my_tag` is used to tag the union variable `my_union` or the anonymous union.

(6) Each member of a tagged union must be associated with a tag value:

```
tag_value1 => var member1 ... ;
```

The tag values are not allowed to repeat creating an ambiguity
