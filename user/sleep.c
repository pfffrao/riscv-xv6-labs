#include "kernel/types.h"  // types like uint are defined in a kernel header
#include "user/user.h"  // list of system calls

void printUsage() {
    fprintf(1, "Usage:\n\tsleep [number of seconds to sleep]. Only integer seconds are supported.\n");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 1;
    }
    int seconds = atoi(argv[1]);
    if (seconds != 0) {
        sleep(seconds);
    }
    return 0;
}