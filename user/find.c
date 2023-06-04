#include "kernel/types.h"
#include "kernel/fcntl.h" // O_* flags
#include "kernel/stat.h"  // fstat()
#include "kernel/fs.h"  // struct dirent
#include "user.h"

void printUsage() {
    fprintf(2, "Find all files with the same name provided.\nUsage: find [dir] [pattern]\n");
}

int debug = 0;

int recursiveFindInDir(char *dir, char *filename) {
    if (debug) {
        printf("Recursing into %s\n", dir);
    }
    
    // find the file with the same filename in the given dir.
    int fd = open(dir, O_RDONLY);
    if (fd < 0) {
        fprintf(2, "Failed to open %s\n", dir);
        return -1;
    }
    // verify that dir is a directory
    struct stat st;
    if (fstat(fd, &st) < 0) {
        fprintf(2, "Failed to call fstat on directory %s\n", dir);
        return -1;
    }
    if (st.type != T_DIR) {
        fprintf(2, "%s is not a directory\n", dir);
        close(fd);
        return -1;
    }
    if (debug) {
        printf("Verifed %s is dir\n", dir);
    }

    struct dirent de;
    char fullpath[512] = {0};
    char *p = 0;
    if (strlen(fullpath) + DIRSIZ + 2 > sizeof fullpath) {
        fprintf(2, "Path too long");
        return -1;
    }
    strcpy(fullpath, dir);
    p = fullpath+strlen(fullpath);
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) {
            continue;
        }
        memcpy(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (debug) {
            printf("Read dirent %s\n", p);
        }

        if (strcmp(p, ".") == 0 || strcmp(p, "..") == 0) {
            if (debug) {
                printf("Skipping %s\n", de.name);
            }
            continue;
        }
        if (stat(fullpath, &st) < 0) {
            fprintf(2, "stat cannot stat %s\n", fullpath);
            continue;
        }
        if (st.type == T_FILE || st.type == T_DEVICE) {
            if (strcmp(p, filename) == 0) {
                printf("%s\n", fullpath);
            }
        } else {
            // directory
            if (recursiveFindInDir(fullpath, filename) != 0) {
                return -1;
            }
        }
    }

    if (close(fd)) {
        fprintf(2, "Failed to close fd of directory %s\n", dir);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printUsage();
        return 1;
    }

    return recursiveFindInDir(argv[1], argv[2]);
}