#include "util.h"
#include "ini.h"
#include <stdio.h>

static inline void print_usage(const char *filename) {
	fprintf(stderr, "Usage: %s <ini file>\n", filename);
}

static inline void handler(
	IniField field
) {
	printf("%.*s: ", (int)field.fieldname_size, field.fieldname);

	switch (field.type) {
	case INI_TYPE_STRING:
		printf("string(%.*s)\n", *((int*)field.as.str - 1), field.as.str);
		break;
	case INI_TYPE_NUMBER:
		printf("number(%ld)\n", field.as.num);
		break;
	case INI_TYPE_DECIMAL:
		printf("decimal(%lf)\n", field.as.dec);
		break;
	case INI_TYPE_BOOLEAN:
		printf("boolean(");
		if (field.as.boolean) {
			printf("true");
		} else {
			printf("false");
		}
		printf(")\n");
		break;
	case INI_TYPE_STRING_ARR:
		printf("stringarr(");
		for (size_t i = 0; i < field.arr.count; i++) {
			printf("%.*s", *((int*)(field.arr.items[i].str) - 1), field.arr.items[i].str);

			if (i != field.arr.count - 1) { 
				printf(", ");
			}
		}

		printf(")\n");
		break;
	case INI_TYPE_NUMBER_ARR:
		printf("numberarr(");
		for (size_t i = 0; i < field.arr.count; i++) {
			printf("%ld",  field.arr.items[i].num);

			if (i != field.arr.count - 1) { 
				printf(", ");
			}
		}

		printf(")\n");
		break;
	case INI_TYPE_DECIMAL_ARR:
		printf("decimalarr(");
		for (size_t i = 0; i < field.arr.count; i++) {
			printf("%lf",  field.arr.items[i].dec);

			if (i != field.arr.count - 1) { 
				printf(", ");
			}
		}

		printf(")\n");
		break;
	case INI_TYPE_BOOLEAN_ARR:
		printf("booleanarr(");
		for (size_t i = 0; i < field.arr.count; i++) {
			if (field.arr.items[i].boolean) {
				printf("true");
			} else {
				printf("false");
			}

			if (i != field.arr.count - 1) { 
				printf(", ");
			}
		}

		printf(")\n");
		break;
	}
}

int main(int argc, const char **argv) {
	if (argc != 2) {
		print_usage(argv[0]);
		exit(1);
	}

	IniFile ini = ini_init();

	{
		INI_STATUS load_status;
		load_status = ini_load(&ini, argv[1], handler);
		if (load_status != INI_STATUS_OK) {
			ERROR_EXIT("Error number %d\n", load_status);
		}
	}

	ini_free(&ini);

	return 0;
}
