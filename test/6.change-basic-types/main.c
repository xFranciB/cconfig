#include "ini.h"
#include <locale.h>
#include <assert.h>

typedef struct {
	IniField *num;
	IniField *dec;
	IniField *str;
	IniField *boolean;
} Options;

void handler(IniField *field, void *user) {
	Options *opts = (Options*)user;

	if (strcmp(field->fieldname, "num") == 0) {
		assert(field->type == INI_TYPE_NUMBER);
		opts->num = field;
		return;
	} else if (strcmp(field->fieldname, "dec") == 0) {
		assert(field->type == INI_TYPE_DECIMAL);
		opts->dec = field;
		return;
	} else if (strcmp(field->fieldname, "str") == 0) {
		assert(field->type == INI_TYPE_STRING);
		opts->str = field;
		return;
	} else if (strcmp(field->fieldname, "bool") == 0) {
		assert(field->type == INI_TYPE_BOOLEAN);
		opts->boolean = field;
		return;
	}

	assert(0);
}

int main(int argc, const char *argv[]) {
	if (argc < 2) {
		return 1;
	}

	setlocale(LC_NUMERIC, "C");

	Options options = { 0 };
	IniFile ini = ini_init();

	if (ini_load(&ini, argv[1], handler, &options) != INI_STATUS_OK) {
		return 2;
	}

	assert(options.num != NULL);
	assert(options.dec != NULL);
	assert(options.str != NULL);
	assert(options.boolean != NULL);

	assert(options.num->as.num == 20);
	assert(options.dec->as.dec == 10.5);
	assert(strcmp(options.str->as.str, "Hello, World!") == 0);
	assert(options.boolean->as.boolean == false);

	options.num->as.num = 50;
	options.dec->as.dec = 69.50;
	ini_string_free(options.str->as.str);
	options.str->as.str = ini_string_new("Modified string of different size \"\" ''\nmultiline");
	options.boolean->as.boolean = true;

	options.num->dirty = true;
	options.dec->dirty = true;
	options.str->dirty = true;
	options.boolean->dirty = true;

	ini_write(&ini);

	options.boolean->as.boolean = false;
	options.boolean->dirty = true;

	ini_write(&ini);

	ini_free(&ini);
	return 0;
}
