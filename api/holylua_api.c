#include "holylua_api.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

static void format_double(char* buf, size_t buf_size, double x) {
    if (isnan(x)) {
        snprintf(buf, buf_size, "nil");
        return;
    }
    
    if (isinf(x)) {
        snprintf(buf, buf_size, "%sinfinity", x < 0 ? "-" : "");
        return;
    }
    
    if (x == 0.0) {
        snprintf(buf, buf_size, "0");
        return;
    }
    
    double abs_x = fabs(x);
    
    double intpart;
    double fracpart = modf(abs_x, &intpart);

    double epsilon = 1e-12;
    if (fracpart < epsilon || (1.0 - fracpart) < epsilon) {
        if (fracpart > 0.5) {
            intpart += 1.0;
        }
        snprintf(buf, buf_size, "%.0f", x < 0 ? -intpart : intpart);
        return;
    }

    int precision = 17;
    
    double scaled = abs_x;
    int decimal_places = 0;

    while (scaled < 1e12 && decimal_places < 15) {
        scaled *= 10;
        double int_scaled;
        double frac_scaled = modf(scaled, &int_scaled);
        if (fabs(frac_scaled) < epsilon) {
            decimal_places++;
            break;
        }
        decimal_places++;
    }

    if (abs_x > 1e12 || abs_x < 1e-12) {
        snprintf(buf, buf_size, "%.15e", x);
        char* e_pos = strchr(buf, 'e');
        if (e_pos) {
            char* before_e = e_pos - 1;
            while (before_e > buf && *before_e == '0') {
                memmove(before_e, before_e + 1, strlen(before_e));
                before_e--;
            }
            if (before_e > buf && *before_e == '.') {
                memmove(before_e, before_e + 1, strlen(before_e));
            }
        }
    } else {
        char temp_buf[64];
        snprintf(temp_buf, sizeof(temp_buf), "%.15f", x);

        char* dot = strchr(temp_buf, '.');
        if (dot) {
            size_t len = strlen(temp_buf);
            char* end = temp_buf + len - 1;
            
            while (end > dot && *end == '0') {
                *end = '\0';
                end--;
            }
            
            if (end == dot) {
                *dot = '\0';
            }
        }

        strncpy(buf, temp_buf, buf_size - 1);
        buf[buf_size - 1] = '\0';
    }
}


char* hl_tostring_number(double x) {
    if (isnan(x)) {
        char* buf = (char*)malloc(4);
        if (buf) strcpy(buf, "nil");
        return buf;
    }
    char* buf = (char*)malloc(64);
    if (!buf) return NULL;
    format_double(buf, 64, x);
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
    if (isnan(x)) {
        printf("nil");
    } else {
        char buf[64];
        format_double(buf, sizeof(buf), x);
        printf("%s", buf);
    }
}

void hl_print_bool_no_newline(int b) {
    printf("%s", b ? "true" : "false");
}

void hl_print_string_no_newline(const char* s) {
    hl_print_no_newline(s);
}

void hl_print_enum_no_newline(int e) {
    printf("%d", e);
}

void hl_print(const char* s) {
    printf("%s\n", s ? s : "nil");
}

void hl_print_number(double x) {
    if (isnan(x)) {
        printf("nil\n");
    } else {
        char buf[64];
        format_double(buf, sizeof(buf), x);
        printf("%s\n", buf);
    }
}

void hl_print_bool(int b) {
    printf("%s\n", b ? "true" : "false");
}

void hl_print_string(const char* s) {
    hl_print(s);
}

void hl_print_enum(int e) {
    printf("%d\n", e);
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