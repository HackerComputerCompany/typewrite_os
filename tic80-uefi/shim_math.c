/*
 * Minimal libm stand-ins for EFI link (avoid glibc libm IFUNC / dynamic resolver).
 */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double floor(double x) {
    long long i = (long long)x;

    if (x < 0 && (double)i != x)
        i--;
    return (double)i;
}

double ceil(double x) {
    double f = floor(x);

    if (x > f)
        return f + 1.0;
    return f;
}

double frexp(double x, int *exp) {
    *exp = 0;
    return x;
}

double ldexp(double x, int exp) {
    while (exp > 0) {
        x *= 2.0;
        exp--;
    }
    while (exp < 0) {
        x /= 2.0;
        exp++;
    }
    return x;
}

double modf(double x, double *iptr) {
    long long i = (long long)x;
    *iptr = (double)i;
    return x - (double)i;
}

double fmod(double x, double y) {
    if (y == 0.0)
        return 0.0;
    return x - floor(x / y) * y;
}

double sqrt(double x) {
    double g;

    if (x <= 0)
        return 0;
    g = x;
    for (int i = 0; i < 20; i++)
        g = 0.5 * (g + x / g);
    return g;
}

double sin(double x) {
    /* Taylor, reduced range */
    while (x > M_PI)
        x -= 2 * M_PI;
    while (x < -M_PI)
        x += 2 * M_PI;
    double x2 = x * x;
    return x - x * x2 / 6.0 + x * x2 * x2 / 120.0 - x * x2 * x2 * x2 / 5040.0;
}

double cos(double x) {
    return sin(x + M_PI / 2);
}

double tan(double x) {
    double c = cos(x);
    return c != 0 ? sin(x) / c : 0;
}

double exp(double x) {
    /* e^x via series for small |x| */
    double r = 1, t = 1;
    int i;

    for (i = 1; i < 30; i++) {
        t *= x / (double)i;
        r += t;
    }
    return r;
}

double log(double x) {
    if (x <= 0)
        return 0;
    /* rough ln */
    int e = 0;
    double m = x;

    while (m > 2) {
        m /= 2;
        e++;
    }
    while (m < 1) {
        m *= 2;
        e--;
    }
    m -= 1;
    return (double)e * 0.6931471805599453 + m - m * m / 2 + m * m * m / 3;
}

double log10(double x) {
    return log(x) / 2.302585092994046;
}

double log2(double x) {
    return log(x) / 0.6931471805599453;
}

double pow(double x, double y) {
    if (y == 0)
        return 1;
    if (x == 0)
        return 0;
    return exp(y * log(x));
}

double sinh(double x) {
    return 0.5 * (exp(x) - exp(-x));
}

double cosh(double x) {
    return 0.5 * (exp(x) + exp(-x));
}

double tanh(double x) {
    double e = exp(2 * x);
    return (e - 1) / (e + 1);
}

double asin(double x) {
    if (x <= -1)
        return -M_PI / 2;
    if (x >= 1)
        return M_PI / 2;
    return x + x * x * x / 6 + 3 * x * x * x * x * x / 40;
}

double acos(double x) {
    return M_PI / 2 - asin(x);
}

double atan2(double y, double x) {
    if (x == 0)
        return (y >= 0) ? M_PI / 2 : -M_PI / 2;
    return y / x; /* crude; rarely used at cart load */
}

double fmin(double a, double b) {
    return a < b ? a : b;
}

double fmax(double a, double b) {
    return a > b ? a : b;
}
