#define CCONF_IMPLEMENTATION
#include "cconfig.h"
#include <locale.h>
#include <assert.h>

void handler(CConfField *field, void *user) {
	if (field->type == CCONF_TYPE_NUMBER) {
		assert(strcmp(field->fieldname, "number") == 0);
		assert(field->as.num == 10);
		return;
	} else if (field->type == CCONF_TYPE_DECIMAL) {
		assert(strcmp(field->fieldname, "dec") == 0);
		assert(field->as.dec == -32.5);
		return;
	} else if (field->type == CCONF_TYPE_STRING) {
		assert(strcmp(field->fieldname, "str") == 0);
		assert(strcmp(field->as.str, "test string \"\" ''") == 0);
		return;
	} else if (field->type == CCONF_TYPE_BOOLEAN) {
		assert(strcmp(field->fieldname, "bool") == 0);
		assert(field->as.boolean);
		return;
	}

	assert(0);
}

int main(int argc, const char *argv[]) {
	if (argc < 2) {
		return 1;
	}

	setlocale(LC_NUMERIC, "C");

	CConfFile cconf = cconf_init();

	if (cconf_load(&cconf, argv[1], handler, NULL) != CCONF_STATUS_OK) {
		return 2;
	}

	cconf_free(&cconf);
	return 0;
}
