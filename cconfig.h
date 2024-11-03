// Before including this file, add the following line to
// include implementations as well:
//
//   #define CCONF_IMPLEMENTATION
//
// For more details on stb-style libraries:
// https://github.com/nothings/stb

#ifndef CCONF_H
#define CCONF_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CCONFDEF
#define CCONFDEF static inline
#endif // CCONFDEF

// NOTE: All string sizes, both the ones in the `CConfString` struct
// and the ones passed to the functions, do not include the NULL terminator

#define CCONF_COMMENT '#'
#define CCONF_STRING_SIZE(s) (*(((CConfStringSize*)s) - 1))

#define _CCONF_RETURN_DEFER(s) do { status = (s); goto defer; } while(0);

#define _CCONF_TOKEN(r, lr, c, p, l, t) { \
	r,                                    \
	lr,                                   \
	c,                                    \
	l,                                    \
	_CCONF_LEXER_##t,                     \
	&lexer->data[p]                       \
}

#define _CCONF_CREATE_DA(T, Name) \
typedef struct {                                                         \
	T *items;                                                            \
	size_t count;                                                        \
	size_t capacity;                                                     \
} Name;                                                                  \
static inline void Name##_init(Name *arr, long long initial_size) {      \
	assert(initial_size > 0);                                            \
	arr->capacity = initial_size;                                        \
	arr->count = 0;                                                      \
	arr->items = (T*)malloc(arr->capacity * sizeof(T));                  \
}                                                                        \
static inline void Name##_append(Name *arr, T value) {                   \
	if (arr->capacity == arr->count) {                                   \
		arr->capacity *= 2;                                              \
		arr->items = (T*)realloc(arr->items, arr->capacity * sizeof(T)); \
	}                                                                    \
	arr->items[arr->count++] = value;                                    \
}                                                                        \
static inline void Name##_free(Name *arr) {                              \
	free(arr->items);                                                    \
	arr->items = 0;                                                      \
	arr->count = 0;                                                      \
	arr->capacity = 0;                                                   \
}

_CCONF_CREATE_DA(size_t, _CConf_size_t_da)

typedef char CConfString;
typedef uint32_t CConfStringSize;

typedef enum {
	CCONF_STATUS_OK = 0,
	CCONF_STATUS_FOPEN,
	CCONF_STATUS_FSEEK,
	CCONF_STATUS_FTELL,
	CCONF_STATUS_FCLOSE,
	CCONF_STATUS_FREAD,
	CCONF_STATUS_MALLOC
} CCONF_STATUS;

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

typedef union {
	CConfString* str;
	int64_t num;
	double dec;
	bool boolean;
} CConfAs;

_CCONF_CREATE_DA(CConfAs, CConfAs_da)

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

_CCONF_CREATE_DA(CConfField*, pCConfField_da)

typedef struct {
	char* filepath;
	pCConfField_da values;
} CConfFile;

typedef void (CCONF_HANDLER)(
	CConfField* field,
	void* user
);

typedef enum { // uint16_t
	_CCONF_LEXER_STRING = 1 << 0,
	_CCONF_LEXER_LITERAL = 1 << 1,
	_CCONF_LEXER_NUMBER = 1 << 2,
	_CCONF_LEXER_DECIMAL = 1 << 3,
	_CCONF_LEXER_BOOLEAN = 1 << 4,
	_CCONF_LEXER_EQUALS = 1 << 5,
	_CCONF_LEXER_COMMA = 1 << 6,
	_CCONF_LEXER_OSQUARE = 1 << 7,
	_CCONF_LEXER_CSQUARE = 1 << 8,

	_CCONF_LEXER_NEWLINE = 1 << 9,
	_CCONF_LEXER_INVALID = 1 << 10,

	_CCONF_LEXER_EOF = 1 << 11,

	_CCONF_LEXER_TOKEN_AMOUNT = 12
} _CCONF_LEXER_TOKEN;

typedef struct {
	size_t row;
	size_t col;
	size_t pos;

	char* data;
	size_t len;
} _CConfLexer;

typedef struct {
	size_t row;
	size_t last_row;
	size_t col;
	size_t len;

	uint16_t type; // _CCONF_LEXER_TOKEN
	char* data;
} _CConfToken;

// String functions
CCONFDEF CConfString* cconf_string_from_size(size_t len);
CCONFDEF CConfString* cconf_string_new(const char* s);
CCONFDEF CConfString* cconf_string_from_sized_string(const char* s, size_t len);
CCONFDEF void cconf_string_free(CConfString* s);

// CConf main functions
CCONFDEF CConfFile cconf_init(void);
CCONFDEF void cconf_free(CConfFile* cconf);

CCONFDEF CCONF_STATUS cconf_load(
	CConfFile* cconf,
	const char* filepath,
	CCONF_HANDLER* handler,
	void* user
);

CCONFDEF void cconf_append_raw(CConfFile* cconf, const char* str);
CCONFDEF void cconf_append_field(CConfFile* cconf, CConfField* field);
CCONFDEF CCONF_STATUS cconf_write(CConfFile* cconf);

#define CCONF_IMPLEMENTATION // TODO: Remove
#ifdef CCONF_IMPLEMENTATION

static inline uint8_t _cconf_popcnt(uint16_t tokens) {
	uint8_t res = 0;

	for (uint8_t i = 0; i < 16; i++) {
		if ((tokens >> i) & 1) {
			res++;
		}
	}

	return res;
}

static inline void _cconf_token_format_line(_CConfToken* token, char* buf) {
	sprintf(buf, "%zu:%zu", token->row + 1, token->col + 1);
}

static inline void _cconf_token_format_name(_CCONF_LEXER_TOKEN token, char* buf) {
	static_assert(_CCONF_LEXER_TOKEN_AMOUNT == 12, "Incorrect token amount");
	strcat(buf, "`");

	switch (token) {
	case _CCONF_LEXER_STRING:
		strcat(buf, "String");
		break;

	case _CCONF_LEXER_LITERAL:
		strcat(buf, "Literal");
		break;

	case _CCONF_LEXER_NUMBER:
		strcat(buf, "Number");
		break;

	case _CCONF_LEXER_DECIMAL:
		strcat(buf, "Decimal");
		break;

	case _CCONF_LEXER_BOOLEAN:
		strcat(buf, "Boolean");
		break;

	case _CCONF_LEXER_EQUALS:
		strcat(buf, "Equals");
		break;

	case _CCONF_LEXER_COMMA:
		strcat(buf, "Comma");
		break;

	case _CCONF_LEXER_OSQUARE:
		strcat(buf, "Open square brackets");
		break;

	case _CCONF_LEXER_CSQUARE:
		strcat(buf, "Close square brackets");
		break;

	case _CCONF_LEXER_NEWLINE:
		strcat(buf, "Newline");
		break;

	case _CCONF_LEXER_INVALID:
		strcat(buf, "Invalid");
		break;

	case _CCONF_LEXER_EOF:
		strcat(buf, "End Of File");
		break;

	default:
		assert(0 && "Unreachable");
	}

	strcat(buf, "`");
}

static inline bool _cconf_lexer_is_eof(_CConfLexer* lexer) {
	return (int64_t)lexer->pos >= (int64_t)lexer->len - 1;
}

static inline bool _cconf_lexer_next(_CConfLexer* lexer, char* c) {
	if (_cconf_lexer_is_eof(lexer)) return false;

	if (lexer->data[lexer->pos] == '\n') {
		lexer->row++;
		lexer->col = 0;
	}
	else {
		lexer->col++;
	}

	if (c != NULL) {
		*c = lexer->data[lexer->pos];
	}

	lexer->pos++;

	return true;
}

static inline bool _cconf_lexer_prev(_CConfLexer* lexer, char* c) {
	if (lexer->pos == 0) {
		return false;
	}

	if (c != NULL) {
		*c = lexer->data[lexer->pos];
	}

	lexer->pos--;

	if (lexer->data[lexer->pos] == '\n') {
		lexer->row--;
		lexer->col = 0;

		if (lexer->row == 0) {
			lexer->col = lexer->pos;
		}
		else {
			// TODO: Test this for loop for off-by-one errors
			for (
				size_t i = 0;
				lexer->data[lexer->pos - i - 1] != '\n';
				i--
			) {
			}
		}

	}
	else {
		lexer->col--;
	}

	return true;
}

static inline bool _cconf_lexer_skip_entire_line(_CConfLexer* lexer) {
	char c = 0;
	while (_cconf_lexer_next(lexer, &c) && c != '\n') {}
	_cconf_lexer_prev(lexer, NULL);
	_cconf_lexer_prev(lexer, NULL);

	if (c == '\n') {
		return true;
	}
	else {
		return false;
	}
}

static inline bool _cconf_isspace(char c) {
	return c == ' ' || c == '\f' || c == '\t' ||
		c == '\v' || c == '\r';
}

static inline bool _cconf_lexer_strip_left(_CConfLexer* lexer) {
	while (_cconf_isspace(lexer->data[lexer->pos])) {
		if (!_cconf_lexer_next(lexer, NULL)) {
			return false;
		}
	}

	return !_cconf_lexer_is_eof(lexer);
}

static inline _CConfToken _cconf_lexer_read_literal(_CConfLexer* lexer) {
	size_t sr = lexer->row;
	size_t sc = lexer->col;
	size_t sp = lexer->pos;

	char current = lexer->data[lexer->pos];
	size_t len = 0;

	if (!isalpha(current)) {
		_CConfToken ret = _CCONF_TOKEN(sr, sr, sc, sp, 0, INVALID);
		return ret;
	}

	_cconf_lexer_next(lexer, &current);

	while (isalnum(current)) {
		if (_cconf_lexer_next(lexer, &current) == false) {
			break;
		}

		len++;
	}

	if (len == 0) {
		_CConfToken ret = _CCONF_TOKEN(sr, sr, sc, sp, 0, EOF);
		return ret;
	}

	_cconf_lexer_prev(lexer, NULL);

	if (strncmp(&lexer->data[lexer->pos - len], "true", len) == 0) {
		_CConfToken ret = _CCONF_TOKEN(sr, lexer->row, sc, sp, len, BOOLEAN);
		return ret;

	}
	else if (strncmp(&lexer->data[lexer->pos - len], "false", len) == 0) {
		_CConfToken ret = _CCONF_TOKEN(sr, lexer->row, sc, sp, len, BOOLEAN);
		return ret;
	}

	_CConfToken ret = _CCONF_TOKEN(sr, lexer->row, sc, sp, len, LITERAL);
	return ret;
}

static inline _CConfToken _cconf_lexer_read_number(_CConfLexer* lexer) {
	size_t sr = lexer->row;
	size_t sc = lexer->col;
	size_t sp = lexer->pos;

	char current;
	size_t len = 1;
	bool sep_appeared = false;
	bool has_digit = false;

	_cconf_lexer_next(lexer, &current);

	if (current == '-' || current == '+') {
		_cconf_lexer_next(lexer, &current);
		len++;
	}

	while (isdigit(current) || current == '.') {
		if (current == '.') {
			if (sep_appeared) {
				_CConfToken ret = _CCONF_TOKEN(sr, sr, sc, sp, 0, INVALID);
				return ret;
			}
			else {
				sep_appeared = true;
			}
		}
		else {
			has_digit = true;
		}

		_cconf_lexer_next(lexer, &current);
		len++;
	}

	_cconf_lexer_prev(lexer, &current);
	len--;

	if (!has_digit) {
		_CConfToken ret = _CCONF_TOKEN(sr, sr, sc, sp, 0, INVALID);
		return ret;
	}

	if (sep_appeared) {
		_CConfToken ret = _CCONF_TOKEN(sr, lexer->row, sc, sp, len, DECIMAL);
		return ret;
	}

	_CConfToken ret = _CCONF_TOKEN(sr, lexer->row, sc, sp, len, NUMBER);
	return ret;
}

static inline _CConfToken _cconf_lexer_read_string(_CConfLexer* lexer, char d) {
	size_t sr = lexer->row;
	size_t sc = lexer->col;
	size_t sp = lexer->pos;

	char current = lexer->data[lexer->pos];
	size_t len = 0;

	// TODO: Test what happens with an empty string
	while (current != d) {
		if (_cconf_lexer_next(lexer, &current) == false) {
			_CConfToken ret = _CCONF_TOKEN(sr, lexer->row, sc, sp, len, EOF);
			return ret;
		}

		len++;

		if (current == '\\') {
			// TODO: Special characters like \n

			if (_cconf_lexer_next(lexer, NULL) == false) {
				_CConfToken ret = _CCONF_TOKEN(sr, lexer->row, sc, sp, len, EOF);
				return ret;
			}

			len++;
		}
	}

	len--; // To remove the trailing quote
	_CConfToken ret = _CCONF_TOKEN(sr, lexer->row, sc, sp, len, STRING);
	return ret;
}

static inline _CConfToken _cconf_lexer_next_token(_CConfLexer* lexer) {
	size_t sr, sc, sp;
	char current;

	while (true) {
		if (!_cconf_lexer_strip_left(lexer)) {
			_CConfToken ret = _CCONF_TOKEN(lexer->row, lexer->row, lexer->col, lexer->pos, 0, EOF);
			return ret;
		}

		sr = lexer->row;
		sc = lexer->col;
		sp = lexer->pos;

		current = lexer->data[lexer->pos];

		if (current != CCONF_COMMENT) {
			break;
		}

		if (!_cconf_lexer_skip_entire_line(lexer)) {
			_CConfToken ret = _CCONF_TOKEN(lexer->row, lexer->row, lexer->col, lexer->pos, 0, EOF);
			return ret;
		}

		_cconf_lexer_next(lexer, NULL);
	}

	switch (current) {
	case '\'':
	case '"':
		_cconf_lexer_next(lexer, NULL);
		return _cconf_lexer_read_string(lexer, current);
	case '=':
		_cconf_lexer_next(lexer, NULL);
		{
			_CConfToken ret = _CCONF_TOKEN(sr, sr, sc, sp, 1, EQUALS);
			return ret;
		}
	case '[':
		_cconf_lexer_next(lexer, NULL);
		{
			_CConfToken ret = _CCONF_TOKEN(sr, sr, sc, sp, 1, OSQUARE);
			return ret;
		}
	case ']':
		_cconf_lexer_next(lexer, NULL);
		{
			_CConfToken ret = _CCONF_TOKEN(sr, sr, sc, sp, 1, CSQUARE);
			return ret;
		}
	case ',':
		_cconf_lexer_next(lexer, NULL);
		{
			_CConfToken ret = _CCONF_TOKEN(sr, sr, sc, sp, 1, COMMA);
			return ret;
		}
	case '\n':
		_cconf_lexer_next(lexer, NULL);
		{
			_CConfToken ret = _CCONF_TOKEN(sr, sr, sc, sp, 1, NEWLINE);
			return ret;
		}
	}

	if (isalpha(current)) {
		return _cconf_lexer_read_literal(lexer);
	}
	else if (isdigit(current) || current == '.' || current == '+' || current == '-') {
		return _cconf_lexer_read_number(lexer);
	}

	_CConfToken ret = _CCONF_TOKEN(sr, sr, sc, sp, 0, INVALID);
	return ret;
}

static inline CCONF_STATUS _cconf_read_entire_file(const char* filepath, size_t* len, char** data) {
	CCONF_STATUS status = CCONF_STATUS_OK;
	*data = 0;

	FILE* f;

	{
		f = fopen(filepath, "rb");

		if (f == NULL) {
			_CCONF_RETURN_DEFER(CCONF_STATUS_FOPEN);
		}

		if (fseek(f, 0, SEEK_END) != 0) {
			_CCONF_RETURN_DEFER(CCONF_STATUS_FSEEK);
		}

		*len = ftell(f);

		if ((long long)*len == -1) {
			_CCONF_RETURN_DEFER(CCONF_STATUS_FTELL);
		}

		*data = (char*)malloc(*len);
		if (*data == NULL) {
			_CCONF_RETURN_DEFER(CCONF_STATUS_MALLOC);
		}

		if (fseek(f, 0, SEEK_SET) != 0) {
			_CCONF_RETURN_DEFER(CCONF_STATUS_FSEEK);
		}

		if (fread(*data, 1, *len, f) != *len) {
			// NOTE: Technically this does not set errno
			// even though that is the error that gets
			// reported to the user
			_CCONF_RETURN_DEFER(CCONF_STATUS_FREAD);
		}
	}

	fclose(f);
	return CCONF_STATUS_OK;

defer:
	if (f != NULL) {
		fclose(f);
	}

	if (*data != 0) {
		free(*data);
	}

	return status;
}

static inline bool _cconf_parser_expect_tokens(_CConfLexer* lexer, uint16_t tokens, _CConfToken* token) {
	*token = _cconf_lexer_next_token(lexer);
	return (tokens & token->type) != 0;
}

// TODO: Make all parser errors return the execution to the user
// so that they can decide to call this function if they want a
// formatted error message. This should also not call exit() and
// the user should be able to free up all resources in case of a failure.
static inline void _cconf_parser_expect_error(uint16_t expected, _CConfToken* got) {
	assert(expected != 0 && "Incorrect expected tokens value");

	char buf[1024] = { 0 };

	_cconf_token_format_line(got, buf);
	strcat(buf, ": ERROR: Expected token");

	uint8_t exp_tokens_amount = _cconf_popcnt(expected);

	if (exp_tokens_amount > 1) {
		strcat(buf, "s ");
	}
	else {
		strcat(buf, " ");
	}

	for (uint8_t i = 0; i < 16; i++) {
		if ((expected >> i) & 1) {
			_cconf_token_format_name((_CCONF_LEXER_TOKEN)(1 << i), buf);

			exp_tokens_amount--;
			if (exp_tokens_amount == 1) {
				strcat(buf, " or ");
			}
			else if (exp_tokens_amount != 0) {
				strcat(buf, ", ");
			}
		}
	}

	strcat(buf, " but got ");
	_cconf_token_format_name((_CCONF_LEXER_TOKEN)got->type, buf);

	// TODO: Remove exit() calls from the library
	fputs(buf, stderr);
	fputc('\n', stderr);
	exit(1);
}

// TODO: Support special characters like \n
static inline CConfString* _cconf_parser_copy_string(_CConfToken token) {
	size_t to_remove = 0;

	for (size_t i = 0; i < token.len; i++) {
		if (token.data[i] == '\\') {
			to_remove++;
			i++;
		}
	}

	CConfString* res = cconf_string_from_size(token.len - to_remove);
	size_t respos = 0;

	for (size_t i = 0; i < token.len; i++) {
		if (token.data[i] == '\\') {
			continue;
		}

		res[respos++] = token.data[i];
	}

	res[respos] = 0;
	return res;
}

static inline uint8_t _cconf_parser_get_type(_CCONF_LEXER_TOKEN type) {
	switch (type) {
	case _CCONF_LEXER_STRING:
		return CCONF_TYPE_STRING;
	case _CCONF_LEXER_NUMBER:
		return CCONF_TYPE_NUMBER;
	case _CCONF_LEXER_DECIMAL:
		return CCONF_TYPE_DECIMAL;
	case _CCONF_LEXER_BOOLEAN:
		return CCONF_TYPE_BOOLEAN;
	default:
		assert(0 && "Unreachable");
	}
}

static inline void _cconf_parser_get_primitive(_CConfToken token, CConfAs* out) {
	switch (token.type) {
	case _CCONF_LEXER_STRING:
		out->str = _cconf_parser_copy_string(token);
		break;

	case _CCONF_LEXER_NUMBER:
		out->num = strtoll(token.data, NULL, 10);
		break;

	case _CCONF_LEXER_DECIMAL:
		out->dec = strtod(token.data, NULL);
		break;

	case _CCONF_LEXER_BOOLEAN:
		if (strncmp(token.data, "true", 4) == 0) {
			out->boolean = true;

		}
		else if (strncmp(token.data, "false", 5) == 0) {
			out->boolean = false;

		}
		else {
			assert(0 && "Unreachable");
		}
		break;

	default:
		assert(0 && "Unreachable");
	}
}

static inline bool _cconf_parse(
	CConfFile* cconf,
	_CConfLexer* lexer,
	CCONF_HANDLER* handler,
	void* user
) {
	_CConfToken name_token;
	_CConfToken value_token;
	bool present;

	while (true) {
		present = _cconf_parser_expect_tokens(
			lexer,
			_CCONF_LEXER_LITERAL | _CCONF_LEXER_EOF | _CCONF_LEXER_NEWLINE,
			&name_token
		);

		if (!present) {
			_cconf_parser_expect_error(_CCONF_LEXER_LITERAL | _CCONF_LEXER_EOF | _CCONF_LEXER_NEWLINE, &name_token);
			return false;
		}

		if (name_token.type == _CCONF_LEXER_NEWLINE) {
			continue;
		}
		else if (name_token.type == _CCONF_LEXER_EOF) {
			break;
		}

		while (true) {
			present = _cconf_parser_expect_tokens(
				lexer,
				_CCONF_LEXER_EQUALS | _CCONF_LEXER_NEWLINE,
				&value_token
			);

			if (!present) {
				_cconf_parser_expect_error(
					_CCONF_LEXER_EQUALS | _CCONF_LEXER_NEWLINE,
					&value_token
				);
				return false;
			}

			if (value_token.type == _CCONF_LEXER_NEWLINE) {
				continue;
			}

			break;
		}

		while (true) {
			present = _cconf_parser_expect_tokens(
				lexer,
				_CCONF_LEXER_STRING | _CCONF_LEXER_NUMBER |
				_CCONF_LEXER_DECIMAL | _CCONF_LEXER_BOOLEAN |
				_CCONF_LEXER_OSQUARE | _CCONF_LEXER_NEWLINE,
				&value_token
			);

			if (!present) {
				_cconf_parser_expect_error(
					_CCONF_LEXER_STRING | _CCONF_LEXER_NUMBER |
					_CCONF_LEXER_DECIMAL | _CCONF_LEXER_BOOLEAN |
					_CCONF_LEXER_OSQUARE | _CCONF_LEXER_NEWLINE,
					&value_token
				);
				return false;
			}

			if (value_token.type == _CCONF_LEXER_NEWLINE) {
				continue;
			}

			break;
		}

		CConfField* field = (CConfField*)malloc(sizeof(CConfField));
		field->startl = name_token.row;
		field->endl = value_token.last_row;
		field->dirty = false;

		field->fieldname = cconf_string_from_sized_string(
			name_token.data, name_token.len
		);

		switch (value_token.type) {
		case _CCONF_LEXER_STRING:
		case _CCONF_LEXER_NUMBER:
		case _CCONF_LEXER_DECIMAL:
		case _CCONF_LEXER_BOOLEAN:
			field->type = _cconf_parser_get_type((_CCONF_LEXER_TOKEN)value_token.type);
			_cconf_parser_get_primitive(value_token, &field->as);
			break;

		case _CCONF_LEXER_OSQUARE:
			while (true) {
				present = _cconf_parser_expect_tokens(
					lexer,
					_CCONF_LEXER_STRING | _CCONF_LEXER_NUMBER |
					_CCONF_LEXER_DECIMAL | _CCONF_LEXER_BOOLEAN |
					_CCONF_LEXER_NEWLINE,
					&value_token
				);

				if (!present) {
					_cconf_parser_expect_error(
						_CCONF_LEXER_STRING | _CCONF_LEXER_NUMBER |
						_CCONF_LEXER_DECIMAL | _CCONF_LEXER_BOOLEAN |
						_CCONF_LEXER_NEWLINE,
						&value_token
					);
					return false;
				}

				if (value_token.type == _CCONF_LEXER_NEWLINE) {
					continue;
				}

				break;
			}

			field->type = _cconf_parser_get_type((_CCONF_LEXER_TOKEN)value_token.type) +
				(CCONF_TYPE_STRING_ARR - CCONF_TYPE_STRING);

			CConfAs_da_init(&field->arr, 2);
			{
				CConfAs null_as = { 0 };
				CConfAs_da_append(&field->arr, null_as);
			}
			_cconf_parser_get_primitive(value_token, &(field->arr.items[field->arr.count - 1]));

			{
				_CCONF_LEXER_TOKEN exp_token = (_CCONF_LEXER_TOKEN)value_token.type;

				while (true) {
					while (true) {
						present = _cconf_parser_expect_tokens(
							lexer,
							_CCONF_LEXER_COMMA | _CCONF_LEXER_CSQUARE |
							_CCONF_LEXER_NEWLINE,
							&value_token
						);

						if (!present) {
							_cconf_parser_expect_error(
								_CCONF_LEXER_COMMA | _CCONF_LEXER_CSQUARE |
								_CCONF_LEXER_NEWLINE,
								&value_token
							);
							return false;
						}

						if (value_token.type == _CCONF_LEXER_NEWLINE) {
							continue;
						}

						break;
					}

					if (value_token.type == _CCONF_LEXER_CSQUARE) {
						break;
					}

					while (true) {
						present = _cconf_parser_expect_tokens(
							lexer,
							exp_token | _CCONF_LEXER_NEWLINE,
							&value_token
						);

						if (!present) {
							_cconf_parser_expect_error(
								exp_token | _CCONF_LEXER_NEWLINE,
								&value_token
							);
							return false;
						}

						if (value_token.type == _CCONF_LEXER_NEWLINE) {
							continue;
						}

						break;
					}

					{
						CConfAs null_as = { 0 };
						CConfAs_da_append(&field->arr, null_as);
					}
					_cconf_parser_get_primitive(value_token, &(field->arr.items[field->arr.count - 1]));
				}
			}

			field->endl = value_token.last_row;
			break;
		}

		pCConfField_da_append(&cconf->values, field);
		handler(field, user);

		present = _cconf_parser_expect_tokens(
			lexer,
			_CCONF_LEXER_NEWLINE | _CCONF_LEXER_EOF,
			&name_token
		);

		if (!present) {
			_cconf_parser_expect_error(_CCONF_LEXER_NEWLINE | _CCONF_LEXER_EOF, &name_token);
			return false;
		}

		if (name_token.type == _CCONF_LEXER_EOF) {
			break;
		}
	}

	return true;
}

// --------------------------------------------------
// Writing related functions

static inline void _cconf_write_find_newlines(_CConf_size_t_da* arr, char* data, size_t datalen) {
	char* start = data;
	char* curr = data;

	while ((data = (char*)memchr(data, '\n', datalen))) {
		datalen -= (data - curr) + 1;
		_CConf_size_t_da_append(arr, data - start);
		data++;
		curr = data;
	}
}

static inline void _cconf_write_copy(size_t fline, size_t lline, _CConf_size_t_da newlines, char* data, FILE* f) {
	assert(fline <= lline);
	size_t start;

	if (fline == 0) {
		start = 0;
	}
	else {
		start = newlines.items[fline - 1] + 1;
	}

	fprintf(f, "%.*s", (int)(newlines.items[lline] - start) + 1, &data[start]);
}

static inline size_t _cconf_write_string(CConfAs t, FILE* f) {
	size_t res = 1;
	fputc('"', f);

	for (uint32_t i = 0; i < CCONF_STRING_SIZE(t.str); i++) {
		if (t.str[i] == '\n') {
			res++;
		}
		else if (t.str[i] == '"') {
			fputc('\\', f);
		}
		else if (t.str[i] == '\\') {
			fputc('\\', f);
		}

		fputc(t.str[i], f);
	}

	fputc('"', f);
	return res;
}

static inline size_t _cconf_write_number(CConfAs t, FILE* f) {
	fprintf(f, "%lld", t.num);
	return 1;
}

static inline size_t _cconf_write_decimal(CConfAs t, FILE* f) {
	fprintf(f, "%.1lf", t.dec);
	return 1;
}

static inline size_t _cconf_write_boolean(CConfAs t, FILE* f) {
	if (t.boolean) {
		fputs("true", f);
	}
	else {
		fputs("false", f);
	}

	return 1;
}

static inline size_t _cconf_write_array(CConfAs_da arr, FILE* f, size_t(*writer)(CConfAs, FILE*)) {
	size_t res = 1;

	fputc('[', f);

	for (size_t i = 0; i < arr.count; i++) {
		res += writer(arr.items[i], f) - 1;

		if (i != arr.count - 1) {
			fputc(',', f);
		}
	}

	fputc(']', f);
	return res;
}

static inline size_t _cconf_write_field(CConfField* field, FILE* f) {
	static_assert(CCONF_TYPE_AMOUNT == 8, "Incorrect type amount");

	size_t ret;
	fprintf(f, "%s=", field->fieldname);

	switch (field->type) {
	case CCONF_TYPE_STRING:
		ret = _cconf_write_string(field->as, f);
		break;
	case CCONF_TYPE_NUMBER:
		ret = _cconf_write_number(field->as, f);
		break;
	case CCONF_TYPE_DECIMAL:
		ret = _cconf_write_decimal(field->as, f);
		break;
	case CCONF_TYPE_BOOLEAN:
		ret = _cconf_write_boolean(field->as, f);
		break;
	case CCONF_TYPE_STRING_ARR:
		ret = _cconf_write_array(field->arr, f, _cconf_write_string);
		break;
	case CCONF_TYPE_NUMBER_ARR:
		ret = _cconf_write_array(field->arr, f, _cconf_write_number);
		break;
	case CCONF_TYPE_DECIMAL_ARR:
		ret = _cconf_write_array(field->arr, f, _cconf_write_decimal);
		break;
	case CCONF_TYPE_BOOLEAN_ARR:
		ret = _cconf_write_array(field->arr, f, _cconf_write_boolean);
		break;
	default:
		assert(0 && "Unreachable");
	}

	fputc('\n', f);
	return ret;
}

//// Exported functions
 
// String functions

CCONFDEF CConfString* cconf_string_from_size(size_t len) {
	CConfString* res = (CConfString*)malloc(
		(sizeof(CConfStringSize)) +
		len + 1
	);

	*((CConfStringSize*)res) = len;
	return (CConfString*)(((CConfStringSize*)res) + 1);
}

CCONFDEF CConfString* cconf_string_new(const char* s) {
	size_t len = strlen(s);
	CConfString* res = cconf_string_from_size(len);
	size_t i = 0;

	do {
		res[i++] = *s;
	} while (*(s++));

	return res;
}

CCONFDEF CConfString* cconf_string_from_sized_string(const char* s, size_t len) {
	CConfString* res = cconf_string_from_size(len);
	memcpy(res, s, len);
	res[CCONF_STRING_SIZE(res)] = 0;
	return res;
}

CCONFDEF void cconf_string_free(CConfString* s) {
	free(((CConfStringSize*)s) - 1);
}

// CConf main functions

CCONFDEF CConfFile cconf_init(void) {
	CConfFile cconf = { 0 };
	return cconf;
}

CCONFDEF void cconf_free(CConfFile* cconf) {
	static_assert(CCONF_TYPE_AMOUNT == 8, "Incorrect type amount");
	free(cconf->filepath);

	for (size_t i = 0; i < cconf->values.count; i++) {
		switch (cconf->values.items[i]->type) {
		case CCONF_TYPE_STRING:
			cconf_string_free(cconf->values.items[i]->as.str);
			break;

		case CCONF_TYPE_STRING_ARR:
			for (size_t j = 0; j < cconf->values.items[i]->arr.count; j++) {
				cconf_string_free(cconf->values.items[i]->arr.items[j].str);
			}

			// Fall through
		case CCONF_TYPE_NUMBER_ARR:
		case CCONF_TYPE_DECIMAL_ARR:
		case CCONF_TYPE_BOOLEAN_ARR:
			CConfAs_da_free(&cconf->values.items[i]->arr);
			break;
		}

		cconf_string_free(cconf->values.items[i]->fieldname);
		free(cconf->values.items[i]);
	}

	pCConfField_da_free(&cconf->values);
}

CCONFDEF CCONF_STATUS cconf_load(
	CConfFile* cconf,
	const char* filepath,
	CCONF_HANDLER* handler,
	void* user
) {
	_CConfLexer lexer = { 0 };

	if (cconf->values.items == NULL) {
		pCConfField_da_init(&cconf->values, 2);
	}
	else {
		cconf->values.count = 0;
	}

	{
		CCONF_STATUS read_status = _cconf_read_entire_file(filepath, &lexer.len, &lexer.data);

		if (read_status != CCONF_STATUS_OK) {
			return read_status;
		}
	}

	cconf->filepath = (char*)malloc((strlen(filepath) + 1) * sizeof(char));
	strcpy(cconf->filepath, filepath);

	_cconf_parse(cconf, &lexer, handler, user);

	free(lexer.data);

	return CCONF_STATUS_OK;
}

// void cconf_append_raw(CConfFile *cconf, const char *str) {
//
// }

// void cconf_append_field(CConfFile *cconf, CConfField *field) {
	// field->dirty = true;
	// field->_fline = cconf->values.items[cconf->values.count - 1]._lline + 1;
	// field->_lline = 
	// pCConfField_da_append(&cconf->values, *field);
// }

CCONF_STATUS cconf_write(CConfFile* cconf) {
	CCONF_STATUS status = CCONF_STATUS_OK;
	size_t len;
	char* data = NULL;
	FILE* f = NULL;
	_CConf_size_t_da newlines = { 0 };

	{
		CCONF_STATUS ret;

		if ((
			ret = _cconf_read_entire_file(
				cconf->filepath,
				&len, &data
			)
			) != CCONF_STATUS_OK) {
			_CCONF_RETURN_DEFER(ret);
		}

		f = fopen(cconf->filepath, "wb");

		if (f == NULL) {
			_CCONF_RETURN_DEFER(CCONF_STATUS_FOPEN);
		}
	}

	_CConf_size_t_da_init(&newlines, 2);
	_cconf_write_find_newlines(&newlines, data, len);

	size_t last_line = 0;
	int64_t change = 0;

	for (size_t i = 0; i < cconf->values.count; i++) {
		CConfField* field = cconf->values.items[i];

		if (!field->dirty) {
			field->startl += change;
			field->endl += change;
			continue;
		}

		field->dirty = false;

		if (field->startl != last_line) {
			_cconf_write_copy(last_line, field->startl - 1, newlines, data, f);
		}

		last_line = field->endl + 1;

		size_t old_size = (field->endl - field->startl + 1);
		size_t new_size = _cconf_write_field(field, f);

		field->startl += change;
		field->endl = field->startl + new_size - 1;

		change += new_size - old_size;
	}

	// If they are the same then all lines were written
	if (last_line != newlines.count) {
		_cconf_write_copy(last_line, newlines.count - 1, newlines, data, f);
	}

defer:
	if (f != NULL) {
		fclose(f);
	}

	if (data != NULL) {
		free(data);
	}

	if (newlines.items != 0) {
		_CConf_size_t_da_free(&newlines);
	}

	return status;
}

// TODO: Create some utility functions to make it easier to free up values and change them
// TODO: Add the ability to append a field

#endif // CCONF_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // CCONF_H