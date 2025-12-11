#include "holylua_api.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

char* hl_tostring_number(double x) {
    if (isnan(x)) {
        char* buf = (char*)malloc(4);
        if (buf) strcpy(buf, "nil");
        return buf;
    }
    
    char* buf = (char*)malloc(32);
    if (!buf) return NULL;
    snprintf(buf, 32, "%g", x);
    return buf;
}

char* hl_tostring_bool(int b) {
    char* buf = (char*)malloc(6);
    if (!buf) return NULL;
    strcpy(buf, b ? "true" : "false");
    return buf;
}

char* hl_tostring_string(const char* s) {
    if (!s) {
        char* buf = (char*)malloc(4);
        if (buf) strcpy(buf, "nil");
        return buf;
    }

    size_t len = strlen(s);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    strcpy(buf, s);
    return buf;
}

char* hl_concat_strings(const char* a, const char* b) {
    const char* safe_a = a ? a : "nil";
    const char* safe_b = b ? b : "nil";

    size_t len_a = strlen(safe_a);
    size_t len_b = strlen(safe_b);

    char* result = (char*)malloc(len_a + len_b + 1);
    if (!result) return NULL;

    memcpy(result, safe_a, len_a);
    memcpy(result + len_a, safe_b, len_b);
    result[len_a + len_b] = '\0';

    return result;
}

void hl_free_string(char* str) {
    if (str) free(str);
}

void hl_print_no_newline(const char* s) {
    printf("%s", s ? s : "nil");
}

void hl_print_number_no_newline(double x) {
    if (isnan(x))
        printf("nil");
    else
        printf("%g", x);
}

void hl_print_bool_no_newline(int b) {
    printf("%s", b ? "true" : "false");
}

void hl_print_string_no_newline(const char* s) {
    hl_print_no_newline(s);
}

void hl_print(const char* s) {
    printf("%s\n", s ? s : "nil");
}

void hl_print_number(double x) {
    if (isnan(x))
        printf("nil\n");
    else
        printf("%g\n", x);
}

void hl_print_bool(int b) {
    printf("%s\n", b ? "true" : "false");
}

void hl_print_string(const char* s) {
    hl_print(s);
}

void hl_print_tab(void) {
    printf("\t");
}

void hl_print_newline(void) {
    printf("\n");
}

int hl_is_nil_number(double x) {
    return isnan(x);
}

int hl_is_nil_string(const char* s) {
    return s == NULL;
}

int hl_is_nil_bool(int b) {
    return b == -1;
}

double hl_tonumber(const char* s) {
    if (!s) return 0.0;
    return atof(s);
}

double hl_floor_div_float(double a, double b) {
    return floor(a / b);
}

const char* hl_type(double x) {
    return isnan(x) ? "nil" : "number";
}

const char* hl_type_str(const char* s) {
    return s ? "string" : "nil";
}

const char* hl_type_bool(int b) {
    if (b == -1) return "nil";
    return "bool";
}
