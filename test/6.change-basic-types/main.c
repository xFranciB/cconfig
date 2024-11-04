#define CCONF_IMPLEMENTATION
#include "cconfig.h"
#include <locale.h>
#include <assert.h>

typedef struct {
	CConfField *num;
	CConfField *dec;
	CConfField *str;
	CConfField *boolean;
} Options;

void handler(CConfField *field, void *user) {
	Options *opts = (Options*)user;

	if (strcmp(field->fieldname, "num") == 0) {
		assert(field->type == CCONF_TYPE_NUMBER);
		opts->num = field;
		return;
	} else if (strcmp(field->fieldname, "dec") == 0) {
		assert(field->type == CCONF_TYPE_DECIMAL);
		opts->dec = field;
		return;
	} else if (strcmp(field->fieldname, "str") == 0) {
		assert(field->type == CCONF_TYPE_STRING);
		opts->str = field;
		return;
	} else if (strcmp(field->fieldname, "bool") == 0) {
		assert(field->type == CCONF_TYPE_BOOLEAN);
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
	CConfFile cconf = cconf_init();

	if (cconf_load(&cconf, argv[1], handler, &options) != CCONF_STATUS_OK) {
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
	cconf_string_free(options.str->as.str);
	options.str->as.str = cconf_string_new("Modified string of different size \"\" ''\nmultiline");
	options.boolean->as.boolean = true;

	options.num->dirty = true;
	options.dec->dirty = true;
	options.str->dirty = true;
	options.boolean->dirty = true;

	cconf_write(&cconf);

	options.boolean->as.boolean = false;
	options.boolean->dirty = true;

	cconf_write(&cconf);

	cconf_free(&cconf);
	return 0;
}
