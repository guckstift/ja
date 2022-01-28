# types

## integer

| size in bits | signed | unsigned |
| --- | --- | --- |
| 8  | `int8`  | `uint8`  |
| 16 | `int16` | `uint16` |
| 32 | `int32` | `uint32` |
| 64 | `int64` | `uint64` |

## bool

Stores only false or true. Requires 8 bits of size.

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
