# types

## integer

| size in bits | signed | unsigned |
| --- | --- | --- |
| 8  | `int8`  | `uint8`  |
| 16 | `int16` | `uint16` |
| 32 | `int32` | `uint32` |
| 64 | `int64` | `uint64` |

## bool

Boolean that can only be false or true. Stored in a single byte.

## integral types

Integral types comprise integer types and bool.

## pointer

Stores the address to a value of a certain type.

```
> target_type
```

## static array

Stores a fixed number of consecutive items of a certain type

```
[size] item_type
```

## structure types

```
struct StructName {
	var member_a : int;
	var member_b = true;
	var member_c : uint = 90;
}

var st : StructName;
```

Structure members can be initialized with constant expressions in the structure
definition.

## incomplete types

Variables of incomplete types can not be declared.

An array type with undefined length is incomplete.

```
var x : []int;
```

A pointer type to an incomplete array is not incomplete.

```
var p : >[]int;
```

Except when the item type of the array is incomplete.

```
var p1 : >[][]int;
var p2 : >>[][]int;
var p3 : []>>[][]int;
```
