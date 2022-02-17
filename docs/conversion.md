# conversion

The following implicit conversions are allowed:

* integral types to other integral types
* `string` to `cstring`
* static array pointer to dynamic array pointer (`>[n]type` to `>[]type`)
* static array of same length but different item types
  (`[n]type_a` to `[n]type_b`) when the items themselves are convertable into
  another

## explicit cast

By explicit casting, this is also allowed:

* one pointer to some other (`>type_a` to `>type_b`)
