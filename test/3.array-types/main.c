#include "ini.h"
#include <locale.h>
#include <assert.h>

void handler(IniField *field, void *user) {
	if (field->type == INI_TYPE_NUMBER_ARR) {
		assert(strcmp(field->fieldname, "numarr") == 0);
		assert(field->arr.count == 4);
		assert(field->arr.items[0].num == 10);
		assert(field->arr.items[1].num == 20);
		assert(field->arr.items[2].num == 30);
		assert(field->arr.items[3].num == 40);
		return;
	} else if (field->type == INI_TYPE_DECIMAL_ARR) {
		assert(strcmp(field->fieldname, "decarr") == 0);
		assert(field->arr.count == 2);
		assert(field->arr.items[0].dec == -12.5);
		assert(field->arr.items[1].dec == 26.0);
		return;
	} else if (field->type == INI_TYPE_STRING_ARR) {
		assert(strcmp(field->fieldname, "stringarr") == 0);
		assert(field->arr.count == 3);
		assert(strcmp(field->arr.items[0].str, "multiline\nstring") == 0);
		assert(strcmp(field->arr.items[1].str, "string2") == 0);
		assert(strcmp(field->arr.items[2].str, "string3") == 0);
		return;
	} else if (field->type == INI_TYPE_BOOLEAN_ARR) {
		assert(strcmp(field->fieldname, "bools") == 0);
		assert(field->arr.count == 4);
		assert(field->arr.items[0].boolean == true);
		assert(field->arr.items[1].boolean == false);
		assert(field->arr.items[2].boolean == true);
		assert(field->arr.items[3].boolean == true);
		return;
	}

	assert(0);
}

int main(int argc, const char *argv[]) {
	if (argc < 2) {
		return 1;
	}

	setlocale(LC_NUMERIC, "C");

	IniFile ini = ini_init();

	if (ini_load(&ini, argv[1], handler, NULL) != INI_STATUS_OK) {
		return 2;
	}

	ini_free(&ini);
	return 0;
}
