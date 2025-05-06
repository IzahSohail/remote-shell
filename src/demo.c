#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {

    // in order to get the same output
    fprintf(stderr, "Demo started with argc=%d\n", argc);
    for (int i = 0; i < argc; i++) {
        fprintf(stderr, "argv[%d]=%s\n", i, argv[i]);
    }

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <total_iterations>\n", argv[0]);
        return 1;
    }

    int total_iterations = atoi(argv[1]);
    fprintf(stderr, "Parsed total_iterations=%d\n", total_iterations);

    if (total_iterations <= 0) {
        fprintf(stderr, "Total iterations must be positive\n");
        return 1;
    }

    for (int i = 0; i < total_iterations; i++) {
        printf("Demo %d/%d\n", i, total_iterations - 1);
        fflush(stdout);  // well ensure output is sent immediately
        sleep(1);  // Simulate work for 1 second
    }

    return 0;
} 