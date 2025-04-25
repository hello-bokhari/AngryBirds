#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "raylib.h"

double toRadians(double degrees) {
    return degrees * PI / 180.0;
}

double toDegrees(double radians) {
    return radians * 180.0 / PI;
}