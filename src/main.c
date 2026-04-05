#include "common.h"

static void run_repl(void) {
    char line[4096];
    printf("Piko dev build\n");
    printf("Type 'exit' to quit.\n");

    for (;;) {
        printf(">> ");
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        if (strcmp(line, "exit\n") == 0 || strcmp(line, "exit") == 0) {
            break;
        }
        printf("[stub] %s", line);
    }
}

static int run_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "piko: could not open %s\n", path);
        return 1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        fprintf(stderr, "piko: could not read %s\n", path);
        return 1;
    }

    long size = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(file);
        fprintf(stderr, "piko: out of memory\n");
        return 1;
    }

    size_t read = fread(buffer, 1, (size_t)size, file);
    buffer[read] = '\0';
    fclose(file);

    printf("[stub] loaded %s (%zu bytes)\n", path, read);
    free(buffer);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        run_repl();
        return 0;
    }

    if (argc == 2) {
        return run_file(argv[1]);
    }

    fprintf(stderr, "Usage: piko [script.piko]\n");
    return 1;
}
