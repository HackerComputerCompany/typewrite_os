/* Extra libc / glibc-ish symbols for TIC-80 + Lua + libpng without glibc (EFI link). */
#include "shim_types.h"

#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

long strtol(const char *nptr, char **endptr, int base);

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define SHIM_ISBIT(bit) ((bit) < 8 ? ((1u << (bit)) << 8) : ((1u << (bit)) >> 8))
#else
#define SHIM_ISBIT(bit) (1u << (bit))
#endif
#define SHIM_ISupper SHIM_ISBIT(0)
#define SHIM_ISlower SHIM_ISBIT(1)
#define SHIM_ISalpha SHIM_ISBIT(2)
#define SHIM_ISdigit SHIM_ISBIT(3)
#define SHIM_ISxdigit SHIM_ISBIT(4)
#define SHIM_ISspace SHIM_ISBIT(5)
#define SHIM_ISprint SHIM_ISBIT(6)
#define SHIM_ISgraph SHIM_ISBIT(7)
#define SHIM_ISblank SHIM_ISBIT(8)
#define SHIM_IScntrl SHIM_ISBIT(9)
#define SHIM_ISpunct SHIM_ISBIT(10)
#define SHIM_ISalnum SHIM_ISBIT(11)

void abort(void) {
    for (;;)
        ;
}

FILE __shim_stdin, __shim_stdout, __shim_stderr;
FILE *stdin = &__shim_stdin;
FILE *stdout = &__shim_stdout;
FILE *stderr = &__shim_stderr;

int printf(const char *fmt, ...) {
    (void)fmt;
    return 0;
}

int vprintf(const char *fmt, va_list ap) {
    (void)fmt;
    (void)ap;
    return 0;
}

int fprintf(FILE *stream, const char *fmt, ...) {
    (void)stream;
    (void)fmt;
    return 0;
}

int fputs(const char *s, FILE *stream) {
    (void)s;
    (void)stream;
    return 0;
}

int fputc(int c, FILE *stream) {
    (void)stream;
    return c;
}

int getc(FILE *stream) {
    (void)stream;
    return -1;
}

int feof(FILE *stream) {
    (void)stream;
    return 1;
}

char *fgets(char *s, int n, FILE *stream) {
    (void)s;
    (void)n;
    (void)stream;
    return NULL;
}

FILE *fopen64(const char *path, const char *mode) {
    (void)path;
    (void)mode;
    return NULL;
}

FILE *freopen64(const char *path, const char *mode, FILE *stream) {
    (void)path;
    (void)mode;
    (void)stream;
    return NULL;
}

char *getenv(const char *name) {
    (void)name;
    return NULL;
}

clock_t clock(void) {
    return 0;
}

int strcoll(const char *a, const char *b) {
    return strcmp(a, b);
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    unsigned char uc = (unsigned char)c;

    while (n--) {
        if (*p == uc)
            return (void *)p;
        p++;
    }
    return NULL;
}

size_t strspn(const char *s, const char *accept) {
    size_t n = 0;

    while (s[n] && strchr(accept, (unsigned char)s[n]))
        n++;
    return n;
}

char *strpbrk(const char *s, const char *accept) {
    for (; *s; s++) {
        if (strchr(accept, (unsigned char)*s))
            return (char *)s;
    }
    return NULL;
}

double strtod(const char *nptr, char **endptr) {
    return (double)strtol(nptr, endptr, 10);
}

double atof(const char *nptr) {
    return strtod(nptr, NULL);
}

static struct lconv shim_lconv;

struct lconv *localeconv(void) {
    static int once;

    if (!once) {
        shim_lconv.decimal_point = ".";
        shim_lconv.thousands_sep = "";
        shim_lconv.grouping = "";
        shim_lconv.int_curr_symbol = "";
        shim_lconv.currency_symbol = "";
        shim_lconv.mon_decimal_point = "";
        shim_lconv.mon_thousands_sep = "";
        shim_lconv.mon_grouping = "";
        shim_lconv.positive_sign = "";
        shim_lconv.negative_sign = "";
        shim_lconv.int_frac_digits = CHAR_MAX;
        shim_lconv.frac_digits = CHAR_MAX;
        shim_lconv.p_cs_precedes = CHAR_MAX;
        shim_lconv.p_sep_by_space = CHAR_MAX;
        shim_lconv.n_cs_precedes = CHAR_MAX;
        shim_lconv.n_sep_by_space = CHAR_MAX;
        shim_lconv.p_sign_posn = CHAR_MAX;
        shim_lconv.n_sign_posn = CHAR_MAX;
        once = 1;
    }
    return &shim_lconv;
}

struct tm *localtime(const time_t *t) {
    (void)t;
    return NULL;
}

struct tm *gmtime(const time_t *t) {
    (void)t;
    return NULL;
}

size_t strftime(char *s, size_t max, const char *fmt, const struct tm *tm) {
    (void)fmt;
    (void)tm;
    if (max)
        s[0] = 0;
    return 0;
}

int tolower(int c) {
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 'a';
    return c;
}

int toupper(int c) {
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';
    return c;
}

static int32_t shim_ctype_tolower[384];
static int32_t shim_ctype_toupper[384];
static unsigned short shim_ctype_b[384];
static const int32_t *shim_ctype_tolower_mid;
static const int32_t *shim_ctype_toupper_mid;
static const unsigned short *shim_ctype_b_mid;

static void shim_ctype_init(void) {
    static int once;
    int i;

    if (once)
        return;
    for (i = 0; i < 384; i++) {
        int c = i - 128;
        unsigned short f = 0;

        shim_ctype_tolower[i] = (int32_t)(unsigned char)tolower(c);
        shim_ctype_toupper[i] = (int32_t)(unsigned char)toupper(c);

        if (c >= 'A' && c <= 'Z') {
            f |= SHIM_ISupper | SHIM_ISalpha;
        } else if (c >= 'a' && c <= 'z') {
            f |= SHIM_ISlower | SHIM_ISalpha;
        } else if (c >= '0' && c <= '9') {
            f |= SHIM_ISdigit | SHIM_ISxdigit;
        }
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
            f |= SHIM_ISxdigit;
        if (c == ' ' || c == '\t')
            f |= SHIM_ISblank | SHIM_ISspace;
        if (c == '\n' || c == '\r' || c == '\f' || c == '\v')
            f |= SHIM_ISspace;
        if (c >= 32 && c < 127) {
            f |= SHIM_ISprint | SHIM_ISgraph;
            if (!(f & SHIM_ISalnum) && c != ' ')
                f |= SHIM_ISpunct;
        }
        if (c >= 0 && c < 32)
            f |= SHIM_IScntrl;
        if (c == 127)
            f |= SHIM_IScntrl;
        if ((f & SHIM_ISalpha) || (f & SHIM_ISdigit))
            f |= SHIM_ISalnum;

        shim_ctype_b[i] = f;
    }
    shim_ctype_tolower_mid = shim_ctype_tolower + 128;
    shim_ctype_toupper_mid = shim_ctype_toupper + 128;
    shim_ctype_b_mid = shim_ctype_b + 128;
    once = 1;
}

const unsigned short **__ctype_b_loc(void) {
    shim_ctype_init();
    return (const unsigned short **)&shim_ctype_b_mid;
}

const int32_t **__ctype_tolower_loc(void) {
    shim_ctype_init();
    return (const int32_t **)&shim_ctype_tolower_mid;
}

const int32_t **__ctype_toupper_loc(void) {
    shim_ctype_init();
    return (const int32_t **)&shim_ctype_toupper_mid;
}
