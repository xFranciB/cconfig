#ifndef INI_H
#define INI_H

#include "util.h"

typedef enum {
	INI_STATUS_OK = 0,

	INI_STATUS_FOPEN,
	INI_STATUS_FSEEK,
	INI_STATUS_FTELL,
	INI_STATUS_FCLOSE,
	INI_STATUS_FREAD,
	INI_STATUS_MALLOC,

} INI_STATUS;

typedef enum {
	INI_TYPE_STRING,
	INI_TYPE_NUMBER,
	INI_TYPE_DECIMAL,
	INI_TYPE_BOOLEAN,
	INI_TYPE_STRING_ARR,
	INI_TYPE_NUMBER_ARR,
	INI_TYPE_DECIMAL_ARR,
	INI_TYPE_BOOLEAN_ARR,

	INI_TYPE_AMOUNT
} INI_TYPE;

typedef union {
	char *str;
	s64 num;
	double dec;
	bool boolean;
} IniAs;

CREATE_DA(IniAs)

typedef struct {
	char *fieldname;
	size_t fieldname_size;

	union {
		IniAs as;
		IniAs_da arr;
	};

	size_t _fline;
	size_t _lline;
	u8 type; // enum INI_TYPE
	bool dirty;
} IniField;

CREATE_DA(IniField)

typedef struct {
	char *filepath;
	IniField_da values;
} IniFile;

typedef void (INI_HANDLER)(
	IniField field
);

IniFile ini_init();
void ini_free(IniFile *ini);

INI_STATUS ini_load(
	IniFile *ini,
	const char *filepath,
	INI_HANDLER *handler
);

void ini_append_raw(IniFile *ini, const char *str);
void ini_append_field(/* .. */);
void ini_write(IniFile *ini);

#endif // INI_H