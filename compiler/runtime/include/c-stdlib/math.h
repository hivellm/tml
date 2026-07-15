/* <math.h> — floating-point math entry points. */

#ifndef TML_C_STDLIB_MATH_H
#define TML_C_STDLIB_MATH_H

#define M_PI 3.14159265358979323846
#define M_E  2.7182818284590452354

double fabs(double x);
double sqrt(double x);
double pow(double x, double y);
double exp(double x);
double log(double x);
double log2(double x);
double log10(double x);
double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);
double floor(double x);
double ceil(double x);
double round(double x);
double trunc(double x);
double fmod(double x, double y);

float  fabsf(float x);
float  sqrtf(float x);

int    isnan(double x);
int    isinf(double x);
int    isfinite(double x);

#define HUGE_VAL (1.0 / 0.0)

#endif
