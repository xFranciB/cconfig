#define CCONF_IMPLEMENTATION
#include "cconfig.h"
#include <locale.h>
#include <assert.h>

void handler(CConfField *field, void *user) {
    assert(
        strcmp(field->fieldname, "empty") == 0 ||
        strcmp(field->fieldname, "empty_at_eof") == 0
    );

    assert(field->type == CCONF_TYPE_STRING);
    assert(CCONF_STRING_SIZE(field->as.str) == 0);
    assert(strcmp(field->as.str, "") == 0);
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
