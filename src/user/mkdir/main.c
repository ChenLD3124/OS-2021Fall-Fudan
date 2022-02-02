#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
int main(int argc, char *argv[]) {
    /* DONE: Lab9 Shell */
    int i;
    if (argc < 2) {
        fprintf(2, "Usage: mkdir files...\n");
        exit(1);
    }
    for (i = 1; i < argc; i++) {
        if (mkdir(argv[i],0) < 0) {
            fprintf(2, "mkdir: %s failed to create\n", argv[i]);
            break;
        }
    }
    exit(0);
}