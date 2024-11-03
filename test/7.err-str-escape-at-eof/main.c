#include "cconfig.h"
#include <locale.h>
#include <assert.h>

void handler(CConfField *field, void *user) {
	(void)field;
	(void)user;
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
