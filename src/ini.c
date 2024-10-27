#include "ini.h"
#include "util.h"
#include <ctype.h>

CREATE_DA(size_t)

#define max(x, y) ((x > y)? x : y)

#define INI_COMMENT '#'
#define INI_TOKEN(r, lr, c, p, l, t) \
(IniToken){                      \
	.row = r,                    \
	.last_row = lr,              \
	.col = c,                    \
	.len = l,                    \
	.type = INI_LEXER_##t,       \
	.data = &lexer->data[p]      \
}

#define RETURN_DEFER(s) do { status = (s); goto defer; } while(0);

typedef enum { // u16
	INI_LEXER_STRING       = 1 << 0,
	INI_LEXER_LITERAL      = 1 << 1,
	INI_LEXER_NUMBER       = 1 << 2,
	INI_LEXER_DECIMAL      = 1 << 3,
	INI_LEXER_BOOLEAN      = 1 << 4,
	INI_LEXER_EQUALS       = 1 << 5,
	INI_LEXER_COMMA        = 1 << 6,
	INI_LEXER_OSQUARE      = 1 << 7,
	INI_LEXER_CSQUARE      = 1 << 8,

	INI_LEXER_NEWLINE      = 1 << 9,
	INI_LEXER_INVALID      = 1 << 10,

	INI_LEXER_EOF          = 1 << 11,

	INI_LEXER_TOKEN_AMOUNT = 12
} INI_LEXER_TOKEN;

typedef struct {
	size_t row;
	size_t col;
	size_t pos;

	char *data;
	size_t len;
} IniLexer;

typedef struct {
	size_t row;
	size_t last_row;
	size_t col;
	size_t len;

	u16 type; // INI_LEXER_TOKEN
	char *data;
} IniToken;

static inline u8 popcnt(u16 tokens) {
	u8 res = 0;

	for (u8 i = 0; i < 16; i++) {
		if ((tokens >> i) & 1) {
			res++;
		}
	}

	return res;
}

static inline void ini_token_format_line(IniToken *token, char *buf) {
	sprintf(buf, "%zu:%zu", token->row + 1, token->col + 1);
}

static inline void ini_token_format_name(INI_LEXER_TOKEN token, char *buf) {
	static_assert(INI_LEXER_TOKEN_AMOUNT == 12, "Incorrect token amount");
	strcat(buf, "`");

	switch (token) {
	case INI_LEXER_STRING:
		strcat(buf, "String");
		break;

	case INI_LEXER_LITERAL:
		strcat(buf, "Literal");
		break;

	case INI_LEXER_NUMBER:
		strcat(buf, "Number");
		break;

	case INI_LEXER_DECIMAL:
		strcat(buf, "Decimal");
		break;

	case INI_LEXER_BOOLEAN:
		strcat(buf, "Boolean");
		break;

	case INI_LEXER_EQUALS:
		strcat(buf, "Equals");
		break;

	case INI_LEXER_COMMA:
		strcat(buf, "Comma");
		break;

	case INI_LEXER_OSQUARE:
		strcat(buf, "Open square brackets");
		break;

	case INI_LEXER_CSQUARE:
		strcat(buf, "Close square brackets");
		break;

	case INI_LEXER_NEWLINE:
		strcat(buf, "Newline");
		break;

	case INI_LEXER_INVALID:
		strcat(buf, "Invalid");
		break;

	case INI_LEXER_EOF:
		strcat(buf, "End Of File");
		break;

	default:
		assert(0 && "Unreachable");
	}

	strcat(buf, "`");
}

IniFile ini_init(void) {
	return (IniFile){ 0 };
}

static inline bool ini_lexer_is_eof(IniLexer *lexer) {
	return (s64)lexer->pos >= (s64)lexer->len - 1;
}

static inline bool ini_lexer_next(IniLexer *lexer, char *c) {
	if (ini_lexer_is_eof(lexer)) return false;

	if (lexer->data[lexer->pos] == '\n') {
		lexer->row++;
		lexer->col = 0;
	} else {
		lexer->col++;
	}

	if (c != NULL) {
		*c = lexer->data[lexer->pos];
	}

	lexer->pos++;

	return true;
}

static inline bool ini_lexer_prev(IniLexer *lexer, char *c) {
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
		} else {
			// TODO: Test this for loop for off-by-one errors
			for (
				size_t i = 0;
				lexer->data[lexer->pos - i - 1] != '\n';
				i--
			) {}
		}

	} else {
		lexer->col--;
	}

	return true;
}

static inline bool ini_lexer_skip_entire_line(IniLexer *lexer) {
	char c = 0;
	while (ini_lexer_next(lexer, &c) && c != '\n') {}
	ini_lexer_prev(lexer, NULL);
	ini_lexer_prev(lexer, NULL);

	if (c == '\n') {
		return true;
	} else {
		return false;
	}
}

static inline bool ini_isspace(char c) {
	return c == ' ' || c == '\f' || c == '\t' ||
	       c == '\v'|| c == '\r';
}

static inline bool ini_lexer_strip_left(IniLexer *lexer) {
	while (ini_isspace(lexer->data[lexer->pos])) {
		if (!ini_lexer_next(lexer, NULL)) {
			return false;
		}
	}

	return !ini_lexer_is_eof(lexer);
}

static inline IniToken ini_lexer_read_literal(IniLexer *lexer) {
	size_t sr = lexer->row;
	size_t sc = lexer->col;
	size_t sp = lexer->pos;

	char current = lexer->data[lexer->pos];
	int len = 0;

	if (!isalpha(current)) {
		return INI_TOKEN(sr, sr, sc, sp, 0, INVALID);
	}

	ini_lexer_next(lexer, &current);

	while (isalnum(current)) {
		ini_lexer_next(lexer, &current);
		len++;
	}

	ini_lexer_prev(lexer, NULL);

	if (strncmp(&lexer->data[lexer->pos - len], "true", len) == 0) {
		return INI_TOKEN(sr, lexer->row, sc, sp, len, BOOLEAN);

	} else if (strncmp(&lexer->data[lexer->pos - len], "false", len) == 0) {
		return INI_TOKEN(sr, lexer->row, sc, sp, len, BOOLEAN);
	}

	return INI_TOKEN(sr, lexer->row, sc, sp, len, LITERAL);
}

static inline IniToken ini_lexer_read_number(IniLexer *lexer) {
	size_t sr = lexer->row;
	size_t sc = lexer->col;
	size_t sp = lexer->pos;

	char current;
	int len = 1;
	bool sep_appeared = false;
	bool has_digit = false;

	ini_lexer_next(lexer, &current);

	if (current == '-' || current == '+') {
		ini_lexer_next(lexer, &current);
		len++;
	}

	while (isdigit(current) || current == '.') {
		if (current == '.') {
			if (sep_appeared) {
				return INI_TOKEN(sr, sr, sc, sp, 0, INVALID);
			} else {
				sep_appeared = true;
			}
		} else {
			has_digit = true;
		}

		ini_lexer_next(lexer, &current);
		len++;
	}

	ini_lexer_prev(lexer, &current);
	len--;

	if (!has_digit) {
		return INI_TOKEN(sr, sr, sc, sp, 0, INVALID);
	}

	if (sep_appeared) {
		return INI_TOKEN(sr, lexer->row, sc, sp, len, DECIMAL);
	}

	return INI_TOKEN(sr, lexer->row, sc, sp, len, NUMBER);
}

static inline IniToken ini_lexer_read_string(IniLexer *lexer, char d) {
	size_t sr = lexer->row;
	size_t sc = lexer->col;
	size_t sp = lexer->pos;

	char current = lexer->data[lexer->pos];
	int len = 0;

	while (current != d) {
		ini_lexer_next(lexer, &current);
		len++;

		if (current == '\\') {
			// TODO: Special characters like \n
			
			ini_lexer_next(lexer, NULL);
			ini_lexer_next(lexer, &current);
			len += 2;
		}
	}

	len--; // To remove the trailing quote
	return INI_TOKEN(sr, lexer->row, sc, sp, len, STRING);
}

static inline IniToken ini_lexer_next_token(IniLexer *lexer) {
	size_t sr, sc, sp;
	char current;

	while (true) {
		if (!ini_lexer_strip_left(lexer)) {
			return INI_TOKEN(lexer->row, lexer->row, lexer->col, lexer->pos, 0, EOF);
		}

		sr = lexer->row;
		sc = lexer->col;
		sp = lexer->pos;

		current = lexer->data[lexer->pos];

		if (current != INI_COMMENT) {
			break;
		}

		if (!ini_lexer_skip_entire_line(lexer)) {
			return INI_TOKEN(lexer->row, lexer->row, lexer->col, lexer->pos, 0, EOF);
		}

		ini_lexer_next(lexer, NULL);
	}

	switch (current) {
	case '\'':
	case '"':
		ini_lexer_next(lexer, NULL);
		return ini_lexer_read_string(lexer, current);
	case '=':
		ini_lexer_next(lexer, NULL);
		return INI_TOKEN(sr, sr, sc, sp, 1, EQUALS);
	case '[':
		ini_lexer_next(lexer, NULL);
		return INI_TOKEN(sr, sr, sc, sp, 1, OSQUARE);
	case ']':
		ini_lexer_next(lexer, NULL);
		return INI_TOKEN(sr, sr, sc, sp, 1, CSQUARE);
	case ',':
		ini_lexer_next(lexer, NULL);
		return INI_TOKEN(sr, sr, sc, sp, 1, COMMA);
	case '\n':
		ini_lexer_next(lexer, NULL);
		return INI_TOKEN(sr, sr, sc, sp, 1, NEWLINE);
	}

	if (isalpha(current)) {
		return ini_lexer_read_literal(lexer);
	} else if (isdigit(current) || current == '.' || current == '+' || current == '-') {
		return ini_lexer_read_number(lexer);
	}

	return INI_TOKEN(sr, sr, sc, sp, 0, INVALID);
}

static inline INI_STATUS ini_read_entire_file(const char *filepath, size_t *len, char **data) {
	INI_STATUS status = INI_STATUS_OK;
	*data = 0;

	FILE *f;

	{
		f = fopen(filepath, "rb");

		if (f == NULL) {
			RETURN_DEFER(INI_STATUS_FOPEN);
		}

		if (fseek(f, 0, SEEK_END) != 0) {
			RETURN_DEFER(INI_STATUS_FSEEK);
		}

		*len = ftell(f);

		if ((long long) *len == -1) {
			RETURN_DEFER(INI_STATUS_FTELL);
		}

		*data = (char*) malloc(*len);
		if (*data == NULL) {
			RETURN_DEFER(INI_STATUS_MALLOC);
		}

		if (fseek(f, 0, SEEK_SET) != 0) {
			RETURN_DEFER(INI_STATUS_FSEEK);
		}

		if (fread(*data, 1, *len, f) != *len) {
			// NOTE: Technically this does not set errno
			// even though that is the error that gets
			// reported to the user
			RETURN_DEFER(INI_STATUS_FREAD);
		}
	}

	fclose(f);
	return INI_STATUS_OK;

defer:
	if (f != NULL) {
		fclose(f);
	}

	if (*data != 0) {
		free(*data);
	}

	return status;
}

static inline bool ini_parser_expect_tokens(IniLexer *lexer, u16 tokens, IniToken *token) {
	*token = ini_lexer_next_token(lexer);
	return (tokens & token->type) != 0;
}

static inline void ini_parser_expect_error(u16 expected, IniToken *got) {
	assert(expected != 0 && "Incorrect expected tokens value");

	char buf[1024] = { 0 };

	ini_token_format_line(got, buf);
	strcat(buf, ": ERROR: Expected token");

	u8 exp_tokens_amount = popcnt(expected);

	if (exp_tokens_amount > 1) {
		strcat(buf, "s ");
	} else {
		strcat(buf, " ");
	}

	for (u8 i = 0; i < 16; i++) {
		if ((expected >> i) & 1) {
			ini_token_format_name(1 << i, buf);

			exp_tokens_amount--;
			if (exp_tokens_amount == 1) {
				strcat(buf, " or ");
			} else if (exp_tokens_amount != 0) {
				strcat(buf, ", ");
			}
		}
	}

	strcat(buf, " but got ");
	ini_token_format_name(got->type, buf);

	ERROR_EXIT("%s\n", buf);
}

// TODO: Support special characters like \n
static inline char *ini_parser_copy_string(IniToken token) {
	size_t to_remove = 0;

	for (size_t i = 0; i < token.len; i++) {
		if (token.data[i] == '\\') {
			to_remove++;
			i++;
		}
	}

	char *res = (char*)malloc(
		sizeof(u32) +    // Fat pointer size
		((
			token.len - to_remove +
			1 // Null terminator
		) * sizeof(char))
	);

	size_t respos = 0;

	*((u32*)res) = token.len - to_remove + 1;
	res = (char*)((u32*) res + 1);

	for (size_t i = 0; i < token.len; i++) {
		if (token.data[i] == '\\') {
			continue;
		}

		res[respos++] = token.data[i];
	}

	return res;
}

static inline u8 ini_parser_get_type(INI_LEXER_TOKEN type) {
	switch (type) {
	case INI_LEXER_STRING:
		return INI_TYPE_STRING;
	case INI_LEXER_NUMBER:
		return INI_TYPE_NUMBER;
	case INI_LEXER_DECIMAL:
		return INI_TYPE_DECIMAL;
	case INI_LEXER_BOOLEAN:
		return INI_TYPE_BOOLEAN;
	default:
		assert(0 && "Unreachable");
	}
}

static inline void ini_parser_get_primitive(IniToken token, IniAs *out) {
	switch (token.type) {
	case INI_LEXER_STRING:
		out->str = ini_parser_copy_string(token);
		break;

	case INI_LEXER_NUMBER:
		out->num = strtoll(token.data, NULL, 10);
		break;

	case INI_LEXER_DECIMAL:
		out->dec = strtod(token.data, NULL);
		break;

	case INI_LEXER_BOOLEAN:
		if (strncmp(token.data, "true", 4) == 0) {
			out->boolean = true;

		} else if (strncmp(token.data, "false", 5) == 0) {
			out->boolean = false;

		} else {
			assert(0 && "Unreachable");
		}
		break;

	default:
		assert(0 && "Unreachable");
	}
}

static inline bool ini_parse(IniFile *ini, IniLexer *lexer, INI_HANDLER *handler) {
	IniToken name_token;
	IniToken value_token;
	bool present;

	while (true) {
		present = ini_parser_expect_tokens(
			lexer,
			INI_LEXER_LITERAL | INI_LEXER_EOF | INI_LEXER_NEWLINE,
			&name_token
		);

		if (!present) {
			ini_parser_expect_error(INI_LEXER_LITERAL | INI_LEXER_EOF | INI_LEXER_NEWLINE, &name_token);
			return false;
		}

		if (name_token.type == INI_LEXER_NEWLINE) {
			continue;
		} else if (name_token.type == INI_LEXER_EOF) {
			break;
		}

		present = ini_parser_expect_tokens(
			lexer,
			INI_LEXER_EQUALS,
			&value_token
		);

		if (!present) {
			ini_parser_expect_error(INI_LEXER_EQUALS, &value_token);
			return false;
		}

		present = ini_parser_expect_tokens(
			lexer,
			INI_LEXER_STRING | INI_LEXER_NUMBER |
			INI_LEXER_DECIMAL | INI_LEXER_BOOLEAN |
			INI_LEXER_OSQUARE,
			&value_token
		);

		if (!present) {
			ini_parser_expect_error(
				INI_LEXER_STRING | INI_LEXER_NUMBER |
				INI_LEXER_DECIMAL | INI_LEXER_BOOLEAN |
				INI_LEXER_OSQUARE,
				&value_token
			);
			return false;
		}

		IniField field = {
			.fieldname = name_token.data,
			.fieldname_size = name_token.len,
			
			._fline = name_token.row,
			._lline = value_token.last_row,
			.dirty = false
		};

		switch (value_token.type) {
		case INI_LEXER_STRING:
		case INI_LEXER_NUMBER:
		case INI_LEXER_DECIMAL:
		case INI_LEXER_BOOLEAN:
			field.type = ini_parser_get_type(value_token.type);
			ini_parser_get_primitive(value_token, &field.as);
			break;

		case INI_LEXER_OSQUARE:
			while (true) {
				present = ini_parser_expect_tokens(
					lexer,
					INI_LEXER_STRING | INI_LEXER_NUMBER |
					INI_LEXER_DECIMAL | INI_LEXER_BOOLEAN |
					INI_LEXER_NEWLINE,
					&value_token
				);

				if (!present) {
					ini_parser_expect_error(
						INI_LEXER_STRING | INI_LEXER_NUMBER |
						INI_LEXER_DECIMAL | INI_LEXER_BOOLEAN |
						INI_LEXER_NEWLINE,
						&value_token
					);
					return false;
				}

				if (value_token.type == INI_LEXER_NEWLINE) {
					continue;
				}

				break;
			}

			field.type = ini_parser_get_type(value_token.type) + (INI_TYPE_STRING_ARR - INI_TYPE_STRING);
			IniAs_da_init(&field.arr, 2);
			IniAs_da_append(&field.arr, (IniAs){ 0 });
			ini_parser_get_primitive(value_token, &(field.arr.items[field.arr.count - 1]));

			{
				INI_LEXER_TOKEN exp_token = value_token.type;

				while (true) {
					while (true) {
						present = ini_parser_expect_tokens(
							lexer,
							INI_LEXER_COMMA | INI_LEXER_CSQUARE |
							INI_LEXER_NEWLINE,
							&value_token
						);

						if (!present) {
							ini_parser_expect_error(
								INI_LEXER_COMMA | INI_LEXER_CSQUARE |
								INI_LEXER_NEWLINE,
								&value_token
							);
							return false;
						}

						if (value_token.type == INI_LEXER_NEWLINE) {
							continue;
						}

						break;
					}

					if (value_token.type == INI_LEXER_CSQUARE) {
						break;
					}

					while (true) {
						present = ini_parser_expect_tokens(
							lexer,
							exp_token | INI_LEXER_NEWLINE,
							&value_token
						);

						if (!present) {
							ini_parser_expect_error(
								exp_token | INI_LEXER_NEWLINE,
								&value_token
							);
							return false;
						}

						if (value_token.type == INI_LEXER_NEWLINE) {
							continue;
						}

						break;
					}

					IniAs_da_append(&field.arr, (IniAs){ 0 });
					ini_parser_get_primitive(value_token, &(field.arr.items[field.arr.count - 1]));
				}
			}

			field._lline = value_token.last_row;
			break;
		}

		handler(field);
		IniField_da_append(&ini->values, field);

		present = ini_parser_expect_tokens(
			lexer,
			INI_LEXER_NEWLINE,
			&name_token
		);

		if (!present) {
			ini_parser_expect_error(INI_LEXER_NEWLINE, &name_token);
			return false;
		}
	}

	return true;
}

void ini_free(IniFile *ini) {
	static_assert(INI_TYPE_AMOUNT == 8, "Incorrect type amount");
	free(ini->filepath);

	for (size_t i = 0; i < ini->values.count; i++) {
		switch (ini->values.items[i].type) {
		case INI_TYPE_STRING:
			free((u32*)ini->values.items[i].as.str - 1);
			break;

		case INI_TYPE_STRING_ARR:
			for (size_t j = 0; j < ini->values.items[i].arr.count; j++) {
				free(ini->values.items[i].arr.items[j].str - 4);
			}

			// Fall through
		case INI_TYPE_NUMBER_ARR:
		case INI_TYPE_DECIMAL_ARR:
		case INI_TYPE_BOOLEAN_ARR:
			IniAs_da_free(&ini->values.items[i].arr);
			break;
		}
	}

	IniField_da_free(&ini->values);
}

INI_STATUS ini_load(
	IniFile *ini,
	const char *filepath,
	INI_HANDLER *handler
) {
	IniLexer lexer = { 0 };

	if (ini->values.items == NULL) {
		IniField_da_init(&ini->values, 2);
	} else {
		ini->values.count = 0;
	}

	{
		INI_STATUS read_status = ini_read_entire_file(filepath, &lexer.len, &lexer.data);

		if (read_status != INI_STATUS_OK) {
			return read_status;
		}
	}

	ini->filepath = (char*)malloc((strlen(filepath) + 1) * sizeof(char));
	strcpy(ini->filepath, filepath);

	ini_parse(ini, &lexer, handler);

	free(lexer.data);

	return INI_STATUS_OK;
}

// void ini_append_raw(IniFile *ini, const char *str) {
//
// }

// void ini_append_field(/* .. */) {
// 
// }

// --------------------------------------------------
// Writing related functions

static inline void ini_write_find_newlines(size_t_da *arr, char *data, size_t datalen) {
	char *start = data;
	char *curr = data;

	while ((data = (char*)memchr(data, '\n', datalen))) {
		datalen -= (data - curr) + 1;
		size_t_da_append(arr, data - start);
		data++;
		curr = data;
	}
}

static inline void ini_write_copy(size_t fline, size_t lline, size_t_da newlines, char *data, FILE *f) {
	assert(fline <= lline);
	size_t start;

	if (fline == 0) {
		start = 0;
	} else {
		start = newlines.items[fline - 1] + 1;
	}

	fprintf(f, "%.*s", (int)(newlines.items[lline] - start) + 1, &data[start]);
}

static inline size_t ini_write_string(IniAs t, FILE *f) {
	size_t res = 1;
	fputc('"', f);

	for (u32 i = 0; i < *(((u32*)t.str) - 1); i++) {
		if (t.str[i] == '\n') {
			res++;
		} else if (t.str[i] == '"') {
			fputc('\\', f);
		}

		fputc(t.str[i], f);
	}

	fputc('"', f);
	return res;
}

static inline size_t ini_write_number(IniAs t, FILE *f) {
	fprintf(f, "%ld", t.num);
	return 1;
}

static inline size_t ini_write_decimal(IniAs t, FILE *f) {
	fprintf(f, "%.1lf", t.dec);
	return 1;
}

static inline size_t ini_write_boolean(IniAs t, FILE *f) {
	if (t.boolean) {
		fputs("true", f);
	} else {
		fputs("false", f);
	}

	return 1;
}

static inline size_t ini_write_array(IniAs_da arr, FILE *f, size_t (*writer)(IniAs, FILE*)) {
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

static inline void ini_write_field(IniField *field, FILE *f) {
	static_assert(INI_TYPE_AMOUNT == 8, "Incorrect type amount");

	fprintf(f, "%.*s=", (int)field->fieldname_size, field->fieldname);

	switch (field->type) {
	case INI_TYPE_STRING:
		ini_write_string(field->as, f);
		break;
	case INI_TYPE_NUMBER:
		ini_write_number(field->as, f);
		break;
	case INI_TYPE_DECIMAL:
		ini_write_decimal(field->as, f);
		break;
	case INI_TYPE_BOOLEAN:
		ini_write_boolean(field->as, f);
		break;
	case INI_TYPE_STRING_ARR:
		ini_write_array(field->arr, f, ini_write_string);
		break;
	case INI_TYPE_NUMBER_ARR:
		ini_write_array(field->arr, f, ini_write_number);
		break;
	case INI_TYPE_DECIMAL_ARR:
		ini_write_array(field->arr, f, ini_write_decimal);
		break;
	case INI_TYPE_BOOLEAN_ARR:
		ini_write_array(field->arr, f, ini_write_boolean);
		break;
	default:
		assert(0 && "Unreachable");
	}

	fputc('\n', f);
}

INI_STATUS ini_write(IniFile *ini) {
	INI_STATUS status = INI_STATUS_OK;
	size_t len;
	char *data = NULL;
	FILE *f = NULL;
	size_t_da newlines = { 0 };

	{
		INI_STATUS ret;

		if ((
			ret = ini_read_entire_file(
				ini->filepath,
				&len, &data
			)
		) != INI_STATUS_OK) {
			RETURN_DEFER(ret);
		}

		f = fopen(ini->filepath, "wb");

		if (f == NULL) {
			RETURN_DEFER(INI_STATUS_FOPEN);
		}
	}

	size_t_da_init(&newlines, 2);
	ini_write_find_newlines(&newlines, data, len);

	size_t last_line = 0;

	for (size_t i = 0; i < ini->values.count; i++) {
		IniField *field = &ini->values.items[i];

		if (!field->dirty) {
			continue;
		}

		field->dirty = false;

		if (field->_fline != last_line) {
			ini_write_copy(last_line, field->_fline - 1, newlines, data, f);
		}
		
		ini_write_field(field, f);
		last_line = field->_lline + 1;
	}

	ini_write_copy(last_line, newlines.count - 1, newlines, data, f);

defer:
	if (f != NULL) {
		fclose(f);
	}

	if (data != NULL) {
		free(data);
	}

	if (newlines.items != 0) {
		size_t_da_free(&newlines);
	}

	return status;
}

// BUG: Writing a string with a \ in it causes the slash to be printed as a single \, without escaping it.
// BUG: An unclosed string causes the program to go into an infinite loop
// TODO: Switch away from fat pointers, use NULL-terminated everywhere
// or a struct with the size next to the NULL-terminated pointer
// TODO: Create some utility functions to make it easier to free up values and change them
// TODO: Add the ability to append a field
