#include "kernel/types.h"
#include "user.h"

void recursiveProcess(int leftPipe[2]) {
    // in the context of a child process: the process entering the function is called "this child";
    // This child will create a new sub-process, "new child".
    // This child will read from leftPipe, and write into rightPipe.
    // New child will read from its leftPipe (which is actually rightPipe).
    // This indicates that we need recursive pattern.
    // This child will be responsible for closing leftPipe.
    close(leftPipe[1]);
    int input, prime;
    if (read(leftPipe[0], &prime, sizeof(prime)) != sizeof(prime)) {
        fprintf(2, "Child failed to read input");
        close(leftPipe[0]);
        return;
    }
    printf("prime %d\n", prime);
    
    int child = -1;
    int rightPipe[2] = {-1, -1};
    while (read(leftPipe[0], &input, sizeof(input)) == sizeof(input)) {
        if (input % prime == 0) {
            continue;
        }
        // found another potential prime
        // create one new child lazily
        if (child == -1) {
            if (pipe(rightPipe)) {
                fprintf(2, "Failed to create right pipe");
                break;
            }
            // we don't need to read from right pipe.
            child = fork();
            if (child > 0) {
                close(rightPipe[0]);
            } else {
                close(rightPipe[1]);
                break;
            }
        }
        // this child process continues to process the input from leftPipe
        if (write(rightPipe[1], &input, sizeof(input)) != sizeof(input)) {
            fprintf(2, "Failed to write to right pipe");
            break;
        }
    }
    if (child == 0) {
        // new child use the rightPipe as its leftPipe.
        recursiveProcess(rightPipe);
    } else {
        // this child
        close(rightPipe[1]);
    }
    close(leftPipe[0]);
    wait(&child);
}

int main(int argc, char **argv) {
    // calculate the primes from [2, 35], using concurrent version of sieves of Eratosthenes 
    // described in https://swtch.com/~rsc/thread/#2, i.e., the CSP Thread model
    int leftPipe[2] = {0, 0};
    if (pipe(leftPipe)) {
        fprintf(2, "Failed to create initial pipe");
        return 1;
    }
    int pid = fork();
    int maxN = 37;
    if (pid > 0) {
        // main process simply writes into pipes
        close(leftPipe[0]);
        for (int i = 2; i <= maxN; ++i) {
            if (write(leftPipe[1], &i, sizeof(i)) != sizeof(i)) {
                fprintf(2, "Initial leftPipe write failed");
                break;
            }
        }
        close(leftPipe[1]);
        wait(&pid);
    } else {
        // child processes read from leftPipe and spawn
        recursiveProcess(leftPipe);
    }
    return 0;
}