#include "kernel/types.h"
#include "kernel/param.h"
#include "user.h"

const int MAXARGLEN = 16;
const int DEBUG = 0;

int
main(int argc, char** argv) {
    char token;
    char childArgv[MAXARG][MAXARGLEN];
    char *childArgvPtr[MAXARG];
    if (argc < 2) {
        fprintf(2, "xargs requires at least a base command to run.\n");
        return -1;
    }
    if (DEBUG) {
        printf("OK.\n");
    }
    int i = 1;
    for (; i < argc; ++i) {
        int len = strlen(argv[i]);
        if (DEBUG) {
            printf("Length of argv[%d] is %d\n", i, len);
        }
        if (len > MAXARGLEN) {
            fprintf(2, "xargs argument too long. Max length is %d, got %d.\n", MAXARGLEN, len);
            return -1;
        }
        // xargs echo ... -- we copy echo to the first argument for the exec.
        char* dst = memcpy(&childArgv[i-1][0], argv[i], len);
        *(dst + len) = 0;
        childArgvPtr[i-1] = &childArgv[i-1][0];
    }
    if (i == MAXARG) {
        fprintf(2, "Too many arguments for xargs\n");
        return -2;
    }
    --i;
    if (DEBUG) {
        printf("After reading from argv, i=%d\n", i);
    }
    char *p = &childArgv[i][0];
    childArgvPtr[i] = p;
    childArgvPtr[i+1] = 0;
    while (read(0, &token, 1) == 1) {
        if (token == '\n') {
            *p++ = 0;
            if (DEBUG) {
                printf("Getting to exec %s\n", childArgvPtr[i]);
            }
            int pid = fork();
            if (pid > 0) {
                wait(&pid);
            } else {
                exec(argv[1], childArgvPtr);
            }
            memset(&childArgv[i][0], 0, MAXARGLEN);
            p = &childArgv[i][0];
        } else if (p - &childArgv[i][0] + 1 >= MAXARGLEN) {
            fprintf(2, "argument too long");
            return 1;
        } else {
            *p++ = token;
        }
    }
    
    return 0;
}