#include "ini.h"
#include <locale.h>
#include <assert.h>

void handler(IniField *field, void *user) {
	assert(strcmp(field->fieldname, "str") == 0);
	assert(field->type == INI_TYPE_STRING);
	assert(strcmp(field->as.str, "  \n\n'") == 0);
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