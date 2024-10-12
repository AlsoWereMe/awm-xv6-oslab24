#include "user.h"

int main(int argc, char *argv[]) {
    // Ensure there is only one time argument follows "sleep".
    if (argc > 2) {
        printf("too much arguments.\n");
        exit(1);
    } else if (argc == 1) {
        printf("Need a argument.\n");
        exit(1);
    }

    int ticks = atoi(argv[1]);
    sleep(ticks);
    printf("(nothing happens for a little while)\n");
    return 0;
}