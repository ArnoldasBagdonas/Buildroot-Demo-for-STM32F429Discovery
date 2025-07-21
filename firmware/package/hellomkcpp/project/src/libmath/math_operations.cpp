// src/libmath/math_operations.cpp

#include "libmath/math_operations.h"
#include <stdexcept>

int MathOperations::add(int a, int b) {
    return a + b;
}

int MathOperations::subtract(int a, int b) {
    return a - b;
}

double MathOperations::multiply(double a, double b) {
    return a * b;
}

double MathOperations::divide(double a, double b) {
    if (b != 0)
        return a / b;
    throw std::invalid_argument("Division by zero");
}
