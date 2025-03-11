# types

* signed integers
  * `int` = `int64`, `int32`, `int16`, `int8`
* unsigned integers
  * `uint` = `uint64`, `uint32`, `uint16`, `uint8`
* floats
  * `float` = `float64`, `float32`
* boolean
  * `bool`
  * can be `true` or `false`
  * stored in a whole byte
* array
  * `[length] item_type`
  * fixed length
* pointer
  * `* target_type`
* slice
  * `[] item_type`
  * points to a range of items
  * consists of a pointer and a length (of type `uint`)