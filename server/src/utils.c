#include <stddef.h>
#include <sys/time.h>
#include "utils.h"

long long GetTimeMilliseconds(void) {
    struct timeval te; 
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
    return milliseconds;
}