# CConfig
Simple stb-style C/C++ library to parse and write configuration files.
Tested on `gcc 13.2.0`, `clang 18.1.3` and `MSVC 19.41.34123` with the `C99` standard.

Licensed under the NON-AI-MIT License.
See the `LICENSE` file and https://github.com/non-ai-licenses/non-ai-licenses for more details.

## Configuration file format
The configuration files follow a format, similar to the one used in ini files, described as follows:
```
# comment
<key> = <value>\n
```
The format is case-sensitive.
- The field `key` must be an alphanumeric value that begins with a letter (a-z, A-Z);
- The field `value` must be one of the following types:
    - **Number**: Sequence of only digits, optionally preceded by a sign (+, -);
    - **Decimal**: Sequence of digits separated at most once by a period "." to separate the decimal part, and optionally preceded by a sign (+, -). The integer part can be omitted if 0 (e.g. -.5);
    - **Boolean**: Either `true` or `false`;
    - **String**: Sequence of any characters enclosed in quotes (either single or double). Quotes inside the string can be escaped with a forward slash "\\";
    - **Array**: Ordered list of values contained in square brackets "[...]" and separated by a comma. The values inside the array must all be of the same type.
- All fields must terminate with a newline. Newlines are allowed between tokens (after `key` and/or after the equals sign "="), and any amount of whitespace is also allowed;
- Comments are preceded by a hash sign "#". Comments are allowed on their own line or at the end of a field.

Please note that comments at the end of a field might be overwritten when writing changes to the file.

## Documentation
**Note on terminology**:
- With `private` we mean values that should not be used by a user, these are all prefixed with an underscore "_" and *are not* documented. It is not recommended to rely on these because updates may and will change them without notice;
- With `public` we mean the public-facing API that is meant to be used by users, such as the defines, typedefs, structs, enums and functions documented below.
### Locale
**IMPORTANT**: Please note that numbers in this library are intended to work with the C locale, which you should set while using it:
```c
#include <locale.h>
setlocale(LC_NUMERIC, "C");
```
Other locales might work, but the C locale is the only one that is tested and guaranteed to work.

### Defines
This library is an stb-style library. Before including it you should add the following line:
```c
#define CCONF_IMPLEMENTATION
#include "cconfig.h"
```
For more information: https://github.com/nothings/stb

You can also change the way the functions of the public API are defined by adding the following line before including:
```c
#define CCONFDEF ...
// By default the library sets `static inline`
#include "cconfig.h"
```

Here is a list of all defined values, these *should not* be modified unless explicitly allowed (like in the case of `CCONFDEF`):
- `CCONFDEF` = static inline
- `CCONF_COMMENT` = #
- `CCONF_STRING_SIZE` = Macro to get size of `CConfString`

### Types
#### Dynamic arrays
CConfig ships with a few dynamic array types, such as `pCConfField_da` or `CConfAs_da`. All of these types are suffixed with a `_da`, and they all follow the same structure:
```c
// With `name` we indicate the name of the type,
// like `CConfAs_da`, with `T` we indicate the
// type of the values inside the array, like `CConfAs`
typedef struct {
	T *items;
	size_t count;
	size_t capacity;
} Name;
```
- `items` is a pointer to an array of the items;
- `count` is the amount of items present in the array;
- `capacity` is the size the array can hold (used internally to reallocate when space runs out)

#### CConfString
**NOTE**: All string sizes *do not* include the NULL-terminator. The size is similar to the output of the `strlen` function.

CConfig has its own (fat pointer) string type, called `CConfString`. It is a pointer to a `char` array, preceded by an integer of type `CConfStringSize` (a `uint32_t`).
- To get the size of a `CConfString` you can use the `CCONF_STRING_SIZE` macro;
- `CConfString`'s can be passed to libc functions or other functions that require C-strings as-is.

#### CConfFile
`CConfFile` is a struct defined as follows:
```c
typedef struct {
	char* filepath;
	pCConfField_da values;
} CConfFile;
```
- `filepath` is the path of the loaded configuration file;
- `values` is a dynamic array of pointers to `CConfField`'s, which represents all the loaded fields from the configuration file.

#### CConfField
`CConfField` is a struct defined as follows:
```c
typedef struct {
	CConfString* fieldname;

	union {
		CConfAs as;
		CConfAs_da arr;
	};

	size_t startl;
	size_t endl;
	uint8_t type; // enum CCONF_TYPE
	bool dirty;
} CConfField;
```

- `fieldname` is a `CConfString` with the name of the field;
- `union`
    - `as` is a `CConfAs` union for basic types;
    - `arr` is a dynamic array of `CConfAs` unions for arrays.
- `startl` and `endl` are, respectively, the first and the last line inside the configuration file where the field is located. (*NOTE*: Do not edit these fields as they are needed for writing to the configuration file correctly. This might lead to loss of data!);
- `type` represents the type of the field. Its value is always of type `CCONF_TYPE`;
- `dirty` is a flag that sets the field as needing to be written to the configuration file on the next call to `cconf_write()` (see the API documentation for more info).

#### CCONF_STATUS
`CCONF_STATUS` is an enum defined as follows:
```c
typedef enum {
	CCONF_STATUS_OK = 0,
	CCONF_STATUS_FOPEN,
	CCONF_STATUS_FSEEK,
	CCONF_STATUS_FTELL,
	CCONF_STATUS_FCLOSE,
	CCONF_STATUS_FREAD,
	CCONF_STATUS_MALLOC
} CCONF_STATUS;
```
A value of `CCONF_STATUS_OK` indicates that no error occured when calling a library function, all other values represent a different kind of error occurred.

#### CCONF_TYPE
`CCONF_TYPE` is an enum defined as follows:
```c
typedef enum {
	CCONF_TYPE_STRING = 0,
	CCONF_TYPE_NUMBER,
	CCONF_TYPE_DECIMAL,
	CCONF_TYPE_BOOLEAN,
	CCONF_TYPE_STRING_ARR,
	CCONF_TYPE_NUMBER_ARR,
	CCONF_TYPE_DECIMAL_ARR,
	CCONF_TYPE_BOOLEAN_ARR,

	CCONF_TYPE_AMOUNT
} CCONF_TYPE;
```
This enum represents the type a field can have, it is used by `CConfField`.

#### CConfAs
`CConfAs` is an enum defined as follows:
```c
typedef union {
	CConfString* str;
	int64_t num;
	double dec;
	bool boolean;
} CConfAs;
```
It is used by the `CConfField` struct, and it represents one of the four basic (non-array) types a field can have.

### API
#### Dynamic arrays
**`..._da_init(Name *arr, size_t initial_size)`**
Initializes a dynamic array with a given initial size.

**`..._da_append(Name *arr, T value)`**
Appends a value into the array

**`..._da_free*(Name *arr)`**
Frees a dynamic array.
Note that this does not call `free` on any of the items inside the array.

#### CConfString
**`CCONFDEF CConfString* cconf_string_from_size(CConfStringSize len)`**
Constructs a new `CConfString` from a given size.
The `CConfString` is allocated and its size is set in memory. No further operations are carried out, which means that the resulting buffer is uninitialized.

**`CCONFDEF CConfString* cconf_string_new(const char* s)`**
Constructs a new `CConfString` from a NULL-terminated C-string.
Returns a pointer to the heap-allocated `CConfString`.

**`CCONFDEF CConfString* cconf_string_from_sized_string(const char* s, CConfStringSize len)`**
Constructs a new `CConfString` from a sized string, i.e. one that does not necessarly have a NULL-terminator, but whose size is known.

**`CCONFDEF void cconf_string_free(CConfString* s)`**
Frees a `CConfString`.

#### CConfig
**`CCONFDEF CConfFile cconf_init(void)`**
Instantiates a `CConfFile`.

**`CCONFDEF void cconf_free(CConfFile* cconf)`**
Frees a `CConfFile`

**`CCONFDEF CCONF_STATUS cconf_load(CConfFile* cconf, const char* filepath, CCONF_HANDLER* handler, void* user)`**
Loads a configuration file into a `CConfFile`.
`CCONF_HANDLER` is a function pointer defined as follows:
```c
typedef void (CCONF_HANDLER)(
	CConfField* field,
	void* user
);
```
The function passed into the `handler` parameter will be called on each new field parsed. The `user` parameter of the `cconf_load` function is passed directly to `handler` without modifying it.

**`CCONFDEF void cconf_append_field(CConfFile* cconf, CConfField* field)`**
TODO: Not implemented

**`CCONFDEF CCONF_STATUS cconf_write(CConfFile* cconf)`**
Writes any pending changes to any field to the configuration file.
This functions checks all `CConfField`'s inside `CConfFile`, and writes all the ones with `dirty` set to `true` to the configuration file, while also setting `dirty` to `false`.

## Testing
This library includes a testing framework, in the form of the `test.c` file. The testing framework works *only* under Linux (for now?).
To compile the testing framework, just run:
```bash
$ gcc -o runtest test.c
```
Then, to run it:
```bash
$ ./runtest RECORD   # To record updated tests
$ ./runtest TEST     # To run all tests
```
All the tests that will be run are found inside the `test/` directory. Each folder in there has the following structure:
```
<testnumber>.<testcase>
├─ main.c
├─ test.ini
└─ expected.txt (autogenerated by ./runtest RECORD)
```
All testcases are first compiled with the `cconfig.h` file, then they are all run with a copy of the `test.ini` file. Their result, along with the final state of the copied configuration file, is then stored into the `expected.txt` file when RECORDING, or inside the `actual.txt` file which is then compared against `expected.txt` when TESTING.