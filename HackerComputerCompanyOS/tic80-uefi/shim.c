/*
 * Minimal C library + POSIX-ish stubs for linking TIC-80 static libs into a gnu-efi PE.
 * malloc uses Boot Services pools (requires InitializeLib before tic80_create).
 */
#include <efi.h>
#include <efilib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#include "shim_types.h"

typedef struct {
    int quot;
    int rem;
} div_t;

div_t div(int n, int d) {
    div_t r;

    if (d == 0) {
        r.quot = 0;
        r.rem = 0;
        return r;
    }
    r.quot = n / d;
    r.rem = n % d;
    return r;
}

#define HDRSZ (sizeof(size_t))
#define USER(p) ((void *)((char *)(p) + HDRSZ))
#define HEADER(u) ((size_t *)((char *)(u)-HDRSZ))

void *malloc(size_t n) {
    size_t *blk;
    size_t total;

    if (n == 0)
        n = 1;
    total = n + HDRSZ;
    if (EFI_ERROR(BS->AllocatePool(EfiLoaderData, total, (VOID **)&blk)))
        return NULL;
    blk[0] = n;
    return USER(blk);
}

void free(void *p) {
    if (!p)
        return;
    BS->FreePool(HEADER(p));
}

void *calloc(size_t n, size_t sz) {
    void *p;
    size_t t;

    if (n && sz > (size_t)-1 / n)
        return NULL;
    t = n * sz;
    p = malloc(t);
    if (p)
        ZeroMem(p, t);
    return p;
}

void *realloc(void *p, size_t n) {
    size_t oldsz;
    void *q;

    if (n == 0) {
        free(p);
        return NULL;
    }
    if (!p)
        return malloc(n);
    oldsz = HEADER(p)[0];
    q = malloc(n);
    if (!q)
        return NULL;
    CopyMem(q, p, oldsz < n ? oldsz : n);
    free(p);
    return q;
}

void *memmove(void *d, const void *s, size_t n) {
    UINT8 *a = d;
    const UINT8 *b = s;

    if (a < b) {
        while (n--)
            *a++ = *b++;
    } else {
        a += n;
        b += n;
        while (n--)
            *--a = *--b;
    }
    return d;
}

int memcmp(const void *a, const void *b, size_t n) {
    const UINT8 *x = a, *y = b;

    while (n--) {
        if (*x != *y)
            return (int)*x - (int)*y;
        x++;
        y++;
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t n = 0;

    while (s[n])
        n++;
    return n;
}

char *strcpy(char *d, const char *s) {
    char *r = d;

    while ((*d++ = *s++))
        ;
    return r;
}

char *strncpy(char *d, const char *s, size_t n) {
    size_t i;

    for (i = 0; i < n && s[i]; i++)
        d[i] = s[i];
    for (; i < n; i++)
        d[i] = '\0';
    return d;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    if (n == 0)
        return 0;
    while (--n && *a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

char *strchr(const char *s, int c) {
    char ch = (char)c;

    while (*s) {
        if (*s == ch)
            return (char *)s;
        s++;
    }
    return ch == '\0' ? (char *)s : NULL;
}

char *strstr(const char *h, const char *n) {
    size_t nl = strlen(n);

    if (!nl)
        return (char *)h;
    for (; *h; h++) {
        if (strncmp(h, n, nl) == 0)
            return (char *)h;
    }
    return NULL;
}

static int isspace_c(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

long strtol(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    int neg = 0;
    long v = 0;

    while (isspace_c((unsigned char)*s))
        s++;
    if (*s == '-') {
        neg = 1;
        s++;
    } else if (*s == '+')
        s++;
    if (base == 0) {
        if (*s == '0') {
            s++;
            if (*s == 'x' || *s == 'X') {
                base = 16;
                s++;
            } else
                base = 8;
        } else
            base = 10;
    }
    while (*s) {
        int dig = -1;
        char c = *s;

        if (c >= '0' && c <= '9')
            dig = c - '0';
        else if (c >= 'a' && c <= 'z')
            dig = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z')
            dig = c - 'A' + 10;
        if (dig < 0 || dig >= base)
            break;
        v = v * base + dig;
        s++;
    }
    if (endptr)
        *endptr = (char *)(s == nptr ? nptr : s);
    return neg ? -v : v;
}

typedef int (*qsort_cmp)(const void *, const void *);

void qsort(void *base, size_t n, size_t w, qsort_cmp cmp) {
    UINT8 *a = base;
    UINT8 *tmp;
    size_t i, j;

    if (n < 2 || w == 0)
        return;
    tmp = malloc(w);
    if (!tmp)
        return;
    for (i = 0; i < n - 1; i++) {
        for (j = i + 1; j < n; j++) {
            if (cmp(a + i * w, a + j * w) > 0) {
                UINT8 *pi = a + i * w, *pj = a + j * w;
                memcpy(tmp, pi, w);
                memcpy(pi, pj, w);
                memcpy(pj, tmp, w);
            }
        }
    }
    free(tmp);
}

static unsigned long long seed = 1;

void srand(unsigned s) {
    seed = s ? s : 1;
}

int rand(void) {
    seed = seed * 1103515245ULL + 12345ULL;
    return (unsigned)(seed >> 16) & 0x7fff;
}

typedef long time_t;
time_t time(time_t *t) {
    if (t)
        *t = 0;
    return 0;
}

static int shim_errno;

int *__errno_location(void) {
    return &shim_errno;
}

void *dlopen(const char *path, int mode) {
    (void)path;
    (void)mode;
    return NULL;
}

void *dlsym(void *h, const char *sym) {
    (void)h;
    (void)sym;
    return NULL;
}

int dlclose(void *h) {
    (void)h;
    return 0;
}

FILE *fopen(const char *path, const char *mode) {
    (void)path;
    (void)mode;
    return NULL;
}

FILE *fdopen(int fd, const char *mode) {
    (void)fd;
    (void)mode;
    return NULL;
}

int fclose(FILE *f) {
    (void)f;
    return -1;
}

size_t fread(void *p, size_t sz, size_t n, FILE *f) {
    (void)p;
    (void)sz;
    (void)n;
    (void)f;
    return 0;
}

size_t fwrite(const void *p, size_t sz, size_t n, FILE *f) {
    (void)p;
    (void)sz;
    (void)n;
    (void)f;
    return 0;
}

int fflush(FILE *f) {
    (void)f;
    return -1;
}

int ferror(FILE *f) {
    (void)f;
    return 0;
}

int open(const char *path, int flags, ...) {
    (void)path;
    (void)flags;
    return -1;
}

int close(int fd) {
    (void)fd;
    return -1;
}

int remove(const char *path) {
    (void)path;
    return -1;
}

char *strerror(int err) {
    (void)err;
    return (char *)"error";
}

static int udecimal(char **out, char *end, unsigned long v, unsigned w, char pad) {
    char tmp[32];
    int n = 0, i;
    char *d = *out;

    if (v == 0) {
        tmp[n++] = '0';
    } else {
        while (v) {
            tmp[n++] = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    while (n < (int)w)
        tmp[n++] = pad;
    for (i = n - 1; i >= 0; i--) {
        if (d < end)
            *d++ = tmp[i];
    }
    *out = d;
    return n;
}

static int uhex(char **out, char *end, unsigned long v, int upper) {
    static const char *xd = "0123456789abcdef";
    static const char *XU = "0123456789ABCDEF";
    const char *dig = upper ? XU : xd;
    char buf[sizeof v * 2 + 1];
    int n = 0, i;
    char *d = *out;

    if (v == 0) {
        if (d < end)
            *d++ = '0';
        *out = d;
        return 1;
    }
    while (v) {
        buf[n++] = dig[v & 15];
        v >>= 4;
    }
    for (i = n - 1; i >= 0; i--) {
        if (d < end)
            *d++ = buf[i];
    }
    *out = d;
    return n;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    char *d = buf;
    char *end = buf + (size ? size - 1 : 0);
    int total = 0;

    if (size == 0)
        end = buf;
    while (*fmt) {
        if (*fmt != '%') {
            total++;
            if (d < end)
                *d++ = *fmt;
            fmt++;
            continue;
        }
        fmt++;
        if (*fmt == '%') {
            total++;
            if (d < end)
                *d++ = '%';
            fmt++;
            continue;
        }
        if (*fmt == 's') {
            const char *s = va_arg(ap, const char *);
            if (!s)
                s = "(null)";
            while (*s) {
                total++;
                if (d < end)
                    *d++ = *s;
                s++;
            }
            fmt++;
            continue;
        }
        if (*fmt == 'c') {
            char c = (char)va_arg(ap, int);
            total++;
            if (d < end)
                *d++ = c;
            fmt++;
            continue;
        }
        if (*fmt == 'd' || *fmt == 'i') {
            int x = va_arg(ap, int);
            unsigned long ax;
            int neg = 0;

            if (x < 0) {
                neg = 1;
                ax = (unsigned long)(-(long)x);
            } else
                ax = (unsigned long)x;
            if (neg) {
                total++;
                if (d < end)
                    *d++ = '-';
            }
            total += udecimal(&d, end, ax, 0, '0');
            fmt++;
            continue;
        }
        if (*fmt == 'u') {
            unsigned x = va_arg(ap, unsigned);
            total += udecimal(&d, end, x, 0, '0');
            fmt++;
            continue;
        }
        if (*fmt == 'x' || *fmt == 'X') {
            unsigned x = va_arg(ap, unsigned);
            int up = *fmt == 'X';
            total += uhex(&d, end, x, up);
            fmt++;
            continue;
        }
        if (*fmt == 'p') {
            void *p = va_arg(ap, void *);
            total += 2;
            if (d < end)
                *d++ = '0';
            if (d < end)
                *d++ = 'x';
            total += uhex(&d, end, (unsigned long)(UINTN)p, 0);
            fmt++;
            continue;
        }
        if (*fmt == 'l' && fmt[1] == 'd') {
            long x = va_arg(ap, long);
            fmt += 2;
            if (x < 0) {
                total++;
                if (d < end)
                    *d++ = '-';
                total += udecimal(&d, end, (unsigned long)(-x), 0, '0');
            } else
                total += udecimal(&d, end, (unsigned long)x, 0, '0');
            continue;
        }
        if (*fmt == 'l' && fmt[1] == 'u') {
            unsigned long x = va_arg(ap, unsigned long);
            fmt += 2;
            total += udecimal(&d, end, x, 0, '0');
            continue;
        }
        if (*fmt == 'l' && fmt[1] == 'x') {
            unsigned long x = va_arg(ap, unsigned long);
            fmt += 2;
            total += uhex(&d, end, x, 0);
            continue;
        }
        /* unknown: emit literal */
        total++;
        if (d < end)
            *d++ = '%';
    }
    if (size)
        *d = '\0';
    return total;
}

int snprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list ap;
    int r;

    va_start(ap, fmt);
    /* Studio uses small format buffers; cap avoids silent overflow. */
    r = vsnprintf(buf, 65536, fmt, ap);
    va_end(ap);
    return r;
}

int vsprintf(char *buf, const char *fmt, va_list ap) {
    return vsnprintf(buf, 65536, fmt, ap);
}
