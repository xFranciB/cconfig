// Before including this file, add the following line to
// include implementations as well:
//
//   #define INI_STRING_IMPLEMENTATION
//
// For more details on stb-style libraries:
// https://github.com/nothings/stb

#ifndef INI_STRING_H
#define INI_STRING_H

#include "util.h"
#include <stdlib.h>

#ifndef INISTRINGDEF
#define INISTRINGDEF static inline
#endif // INISTRINGDEF

// NOTE: All sizes, both the ones in the `IniString` struct
// and the ones passed to the functions, do not include the NULL terminator

typedef struct  {
	u32 size; // Without null terminator
	char value[];
} IniString;

INISTRINGDEF IniString *ini_string_from_size(size_t len);
INISTRINGDEF IniString *ini_string_new(const char *s);
INISTRINGDEF IniString *ini_string_from_sized_string(const char *s, size_t len);
INISTRINGDEF void ini_string_free(IniString *s);

#ifdef INI_STRING_IMPLEMENTATION

INISTRINGDEF IniString *ini_string_from_size(size_t len) {
	IniString *res = (IniString*)malloc(
		MEMBER_SIZE(IniString, size) +
		len + 1
	);

	res->size = len;
	return res;
}

INISTRINGDEF IniString *ini_string_new(const char *s) {
	size_t len = strlen(s);

	IniString *res = ini_string_from_size(len);

	res->size = len;
	memcpy(&res->value, s, len + 1);
		
	return res;
}

INISTRINGDEF IniString *ini_string_from_sized_string(const char *s, size_t len) {
	IniString *res = ini_string_from_size(len);
	memcpy(&res->value, s, len);
	res->value[res->size] = 0;

	return res;
}

INISTRINGDEF void ini_string_free(IniString *s) {
	free(s);
}

#endif // INI_STRING_IMPLEMENTATION
#endif // INI_STRING_H
