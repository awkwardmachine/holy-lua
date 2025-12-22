#ifndef HOLYLUA_API_H
#define HOLYLUA_API_H

#include <math.h>

#define HL_NIL_NUMBER (0.0/0.0)

char* hl_tostring_number(double x);
char* hl_tostring_bool(int b);
char* hl_tostring_string(const char* s);
char* hl_concat_strings(const char* a, const char* b);

void hl_free_string(char* str);

void hl_print_no_newline(const char* s);
void hl_print_number_no_newline(double x);
void hl_print_bool_no_newline(int b);
void hl_print_string_no_newline(const char* s);
void hl_print_enum_no_newline(int e);

void hl_print(const char* s);
void hl_print_number(double x);
void hl_print_bool(int b);
void hl_print_string(const char* s);
void hl_print_enum(int e);

void hl_print_tab(void);
void hl_print_newline(void);

int hl_is_nil_number(double x);
int hl_is_nil_string(const char* s);
int hl_is_nil_bool(int b);

double hl_tonumber(const char* s);

double hl_floor_div_float(double a, double b);

const char* hl_type(double x);
const char* hl_type_str(const char* s);
const char* hl_type_bool(int b);

#endif