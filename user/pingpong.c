#include "kernel/types.h"
#include "user/user.h"

// test program to do ping-pong between 2 processes.
int 
main(int argc, char ** argv) {
    int parentPipe[2];
    int childPipe[2];
    if (pipe(parentPipe)) {
        fprintf(2, "Failed to open parent pipe\n");
        return 1;
    }
    if (pipe(childPipe)) {
        fprintf(2, "Failed to open child pipe\n");
        return 1;
    }
    int pid = fork();
    if (pid > 0) {
        // parent: main thread wait child, second thread
        close(parentPipe[1]);
        close(childPipe[0]);
        char buf[2] = {0, 0};
        // ping child
        int rc = write(childPipe[1], &buf, 1);  // write a byte to child
        if (rc != 1) {
            fprintf(2, "Parent expect write() to return 1 byte, got %d bytes instead\n", rc);
            rc = -1;
        }
        if (rc == 1) {
            rc = read(parentPipe[0], &buf, 1);  // read a byte from child
            if (rc != 1) {
                fprintf(2, "Parent expect read() to return 1 byte, got %d bytes instead\n", rc);
                rc = 1;
            }
            int mypid = getpid();
            printf("%d: received pong\n", mypid);
        }
        close(parentPipe[0]);
        close(childPipe[1]);
        return 0;
    } else if (pid == 0) {
        char buf[2] = {'x', 0};
        close(parentPipe[0]);
        close(childPipe[1]);

        int rc = read(childPipe[0], &buf[0], 1);
        if (rc != 1) {
            fprintf(2, "Child expect read() to return 1 byte, got %d bytes instead.\n", rc);
        }
        if (rc == 1) {
            int mypid = getpid();
            printf("%d: received ping\n", mypid);
            rc = write(parentPipe[1], &buf, 1);
            if (rc != 1) {
                fprintf(2, "Child expect write() to return 1 byte, got %d byte instead.\n", rc);
            }
        }
        close(parentPipe[1]);
        close(childPipe[0]);
        exit(0);
    } else {
        fprintf(2, "Fork error\n");
    }
    return 0;
}