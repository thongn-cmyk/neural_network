#include <stdio.h>
#include <time.h>

int main() {
    struct timespec request = {
        .tv_sec = 1,          // 1 second
        .tv_nsec = 500'000'000  // 0.5 seconds (500 million nanoseconds)
    };
    struct timespec remaining;

    printf("Sleeping for 1.5 seconds...\n");

    // CLOCK_MONOTONIC is recommended as it is not affected by system time jumps
    if (clock_nanosleep(CLOCK_MONOTONIC, 0, &request, &remaining) != 0) {
        printf("Sleep was interrupted.\n");
    } else {
        printf("Wake up!\n");
    }

    return 0;
}