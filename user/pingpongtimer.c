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
        int rc = 0, counter = 0;
        while (1) {
            int rc = read(parentPipe[0], &buf, 1);
            if (rc == 0) {
                // remote closed the pipe.
                break;
            } else if (rc != 1) {
                fprintf(2, "Parent expect read() to return 1 byte, got %d bytes instead\n", rc);
                rc = 1;
                break;
            }
            rc = write(childPipe[1], &buf, 1);
            if (rc != 1) {
                fprintf(2, "Parent expect write() to return 1 byte, got %d bytes instead\n", rc);
                rc = 1;
                break;
            }
            ++counter;
        }
        close(parentPipe[0]);
        close(childPipe[1]);
        int childCounter = 0;
        if (wait(&childCounter)) {
            fprintf(1, "Parent: child exited before calling wait.\n");
        }
        fprintf(1, "Parent: complete ping-pong %d times. Child returned %d times. Exiting...\n", counter, childCounter);
        return rc;
    } else if (pid == 0) {
        // child: ping pong n times and measure wall time
        char buf[2] = {'x', 0};
        int n = 10000, i = 0;
        close(parentPipe[0]);
        close(childPipe[1]);
        uint64 startTime = uptime();
        for (; i < n; ++i) {
            int rc = write(parentPipe[1], &buf, 1);
            if (rc != 1) {
                fprintf(2, "Child expect write() to return 1 byte, got %d byte instead.\n", rc);
                break;
            }

            rc = read(childPipe[0], &buf[0], 1);
            if (rc == 0) {
                // remote closed the pipe
                break;
            } else if (rc != 1) {
                fprintf(2, "Child expect read() to return 1 byte, got %d bytes instead.\n", rc);
                break;
            }
        }
        uint64 endTime = uptime();
        close(parentPipe[1]);
        close(childPipe[0]);
        fprintf(1, "Child: ping-pong finished %d times. %l ticks passed. Exiting...\n", n, endTime - startTime);
        exit(i);
    } else {
        fprintf(2, "Fork error\n");
    }
    return 0;
}