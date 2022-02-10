# types

## integer

| size in bits | signed | unsigned |
| --- | --- | --- |
| 8  | `int8`  | `uint8`  |
| 16 | `int16` | `uint16` |
| 32 | `int32` | `uint32` |
| 64 | `int64` (alias `int`) | `uint64` (alias `uint`) |

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
