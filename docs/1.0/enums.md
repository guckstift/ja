# enumerations

(1) declaring an enumeration type: `EnumType`:

```
enum EnumType {
	ZERO,
	ONE,
	TWO,
	SIX = 6,
	SEVEN,
}
```

The last trailing comma can be omitted.


The enumeration items are automatically assigned a value starting from zero counting up by 1 for each item.

(2) assigning an explicit enumeration value to an item:

```
enum EnumType {
	ONE,
	TWO = 6,
	THREE,
}
```

When an explicit value is assigned all subsequent items get