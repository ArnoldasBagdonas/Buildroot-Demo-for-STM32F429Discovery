// src/libmath/math_operations.c

#include "libmath/math_operations.h"

// Function to add two integers
int add(int a, int b) {
    return a + b;
}

// Function to subtract one integer from another
int subtract(int a, int b) {
    return a - b;
}

// Function to multiply two doubles
double multiply(double a, double b) {
    return a * b;
}

// Function to divide one double by another
// Returns 0.0 if division by zero occurs; otherwise returns the quotient
double divide(double a, double b, int* error) {
    if (b == 0) {
        *error = 1; // Set error code for division by zero
        return 0.0; // Return a default value or handle as necessary
    }
    *error = 0; // Clear error code
    return a / b;
}
