#include "util.h"
#include "ini.h"
#include <locale.h>
#include <stdio.h>

static inline void print_usage(const char *filename) {
	fprintf(stderr, "Usage: %s <ini file>\n", filename);
}

static inline void handler(
	IniField field
) {
	printf("(%zu, %zu) %.*s: ", field._fline, field._lline, (int)field.fieldname_size, field.fieldname);

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

	setlocale(LC_NUMERIC, "C");

	IniFile ini = ini_init();

	{
		INI_STATUS load_status;
		load_status = ini_load(&ini, argv[1], handler);
		if (load_status != INI_STATUS_OK) {
			ERROR_EXIT("Error number %d\n", load_status);
		}
	}

	// TEST: Decimal --> String
	ini.values.items[0].as.str = (char*)malloc(sizeof("Custom string") + sizeof(u32));
	*((u32*)ini.values.items[0].as.str) = strlen("Custom string");
	ini.values.items[0].as.str = (char*)((u32*)ini.values.items[0].as.str + 1);
	ini.values.items[0].type = INI_TYPE_STRING;
	memcpy(ini.values.items[0].as.str, "Custom string", sizeof("Custom string"));
	ini.values.items[0].dirty = true;

	// TEST: String --> Number
	free((u32*)ini.values.items[1].as.str - 1);
	ini.values.items[1].as.num = 420;
	ini.values.items[1].type = INI_TYPE_NUMBER;
	ini.values.items[1].dirty = true;

	// TEST: String --> Decimal
	free((u32*)ini.values.items[2].as.str - 1);
	ini.values.items[2].as.dec = -420.69;
	ini.values.items[2].type = INI_TYPE_DECIMAL;
	ini.values.items[2].dirty = true;

	// TEST: String array --> Boolean
	for (size_t i = 0; i < ini.values.items[3].arr.count; i++) {
		free((u32*)ini.values.items[3].arr.items[i].str - 1);
	}

	IniAs_da_free(&ini.values.items[3].arr);
	ini.values.items[3].as.boolean = false;
	ini.values.items[3].type = INI_TYPE_BOOLEAN;
	ini.values.items[3].dirty = true;

	// TEST: Boolean -> String array
	IniAs_da_init(&ini.values.items[4].arr, 3);

	IniAs as;
	as.str = malloc(sizeof("String 1") + sizeof(u32));
	*((u32*)as.str) = sizeof("String 1");
	as.str = (char*)((u32*)as.str + 1);
	memcpy(as.str, "String 1", sizeof("String 1"));
	IniAs_da_append(&ini.values.items[4].arr, as);

	as.str = malloc(sizeof("String 1") + sizeof(u32));
	*((u32*)as.str) = sizeof("String 1");
	as.str = (char*)((u32*)as.str + 1);
	memcpy(as.str, "String 2", sizeof("String 1"));
	IniAs_da_append(&ini.values.items[4].arr, as);

	as.str = malloc(sizeof("String 1") + sizeof(u32));
	*((u32*)as.str) = sizeof("String 1");
	as.str = (char*)((u32*)as.str + 1);
	memcpy(as.str, "String 3", sizeof("String 1"));
	IniAs_da_append(&ini.values.items[4].arr, as);

	ini.values.items[4].type = INI_TYPE_STRING_ARR;
	ini.values.items[4].dirty = true;

	// TEST: Boolean -> Boolean array
	IniAs_da_init(&ini.values.items[5].arr, 3);

	as.boolean = true;
	IniAs_da_append(&ini.values.items[5].arr, as);
	as.boolean = false;
	IniAs_da_append(&ini.values.items[5].arr, as);
	as.boolean = true;
	IniAs_da_append(&ini.values.items[5].arr, as);

	ini.values.items[5].type = INI_TYPE_BOOLEAN_ARR;
	ini.values.items[5].dirty = true;

	// TEST: String --> Decimal array
	free((u32*)ini.values.items[6].as.str - 1);

	IniAs_da_init(&ini.values.items[6].arr, 3);

	as.dec = 69.420;
	IniAs_da_append(&ini.values.items[6].arr, as);
	as.dec = -69.0;
	IniAs_da_append(&ini.values.items[6].arr, as);
	as.dec = -420.69;
	IniAs_da_append(&ini.values.items[6].arr, as);

	ini.values.items[6].type = INI_TYPE_DECIMAL_ARR;
	ini.values.items[6].dirty = true;

	// TEST: String --> Number array
	free((u32*)ini.values.items[7].as.str - 1);

	IniAs_da_init(&ini.values.items[7].arr, 3);

	as.num = 69;
	IniAs_da_append(&ini.values.items[7].arr, as);
	as.num = 420;
	IniAs_da_append(&ini.values.items[7].arr, as);
	as.num = 32;
	IniAs_da_append(&ini.values.items[7].arr, as);

	ini.values.items[7].type = INI_TYPE_DECIMAL_ARR;

	ini_write(&ini);
	ini_free(&ini);

	printf("--------------------------------------------------\n");

	return 0;
}
