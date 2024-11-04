#define CCONF_IMPLEMENTATION
#include "cconfig.h"
#include <locale.h>
#include <assert.h>

typedef struct {
    CConfField *str;
} Options;

void handler(CConfField *field, void *user) {
    Options *opts = (Options*)user;

    if (strcmp(field->fieldname, "str") == 0) {
        assert(field->type == CCONF_TYPE_STRING);
        opts->str = field;
        return;
    }

    assert(0);
}

int main(int argc, const char *argv[]) {
    if (argv < 2) {
        return 1;
    }

    setlocale(LC_NUMERIC, "C");

    CConfFile cconf = cconf_init();
    Options opts = { 0 };

    if (cconf_load(&cconf, argv[1], handler, (void*)&opts) != CCONF_STATUS_OK) {
        return 1;
    }

    assert(opts.str != 0);

    {
        CConfField *new_field = cconf_field_new(cconf_string_new("new_field1"), CCONF_TYPE_STRING);
        new_field->as.str = cconf_string_new("New string\nalso multiline");
        cconf_append_field(&cconf, new_field);
    }

    {
        CConfField *new_field = cconf_field_new(cconf_string_new("new_field2"), CCONF_TYPE_DECIMAL);
        new_field->as.dec = -5.5;
        cconf_append_field(&cconf, new_field);
    }

    {
        CConfField *new_field = cconf_field_new(cconf_string_new("new_field3"), CCONF_TYPE_STRING);
        new_field->as.str = cconf_string_new("Another string");
        cconf_append_field(&cconf, new_field);
    }

    {
        CConfField *new_field = cconf_field_new(cconf_string_new("new_field4"), CCONF_TYPE_NUMBER);
        new_field->as.num = 20;
        cconf_append_field(&cconf, new_field);
    }

    {
        CConfField *new_field = cconf_field_new(cconf_string_new("new_field5"), CCONF_TYPE_BOOLEAN);
        new_field->as.boolean = false;
        cconf_append_field(&cconf, new_field);
    }

    {
        CConfField *new_field = cconf_field_new(cconf_string_new("new_array"), CCONF_TYPE_STRING_ARR);
        CConfAs_da_init(&new_field->arr, 3);

        CConfAs as;
        as.str = cconf_string_new("String1");
        CConfAs_da_append(&new_field->arr, as);

        as.str = cconf_string_new("String2\nMultiline");
        CConfAs_da_append(&new_field->arr, as);

        as.str = cconf_string_new("String3");
        CConfAs_da_append(&new_field->arr, as);

        cconf_append_field(&cconf, new_field);
    }

    cconf_write(&cconf);
    cconf_free(&cconf);
    return 0;
}