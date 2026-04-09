#include "common.h"
#include "lexer.h"

static char *read_file_bytes(const char *path, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "piko: could not open %s\n", path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        fprintf(stderr, "piko: could not read %s\n", path);
        return NULL;
    }

    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        fprintf(stderr, "piko: could not read %s\n", path);
        return NULL;
    }
    rewind(file);

    char *buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(file);
        fprintf(stderr, "piko: out of memory\n");
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t)size, file);
    buffer[read] = '\0';
    fclose(file);

    if (out_size != NULL) {
        *out_size = read;
    }
    return buffer;
}

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
    size_t read = 0;
    char *buffer = read_file_bytes(path, &read);
    if (buffer == NULL) {
        return 1;
    }

    printf("[stub] loaded %s (%zu bytes)\n", path, read);
    free(buffer);
    return 0;
}

static int run_lex_file(const char *path) {
    char *source = read_file_bytes(path, NULL);
    if (source == NULL) {
        return 1;
    }

    lexer_dump(source);
    free(source);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        run_repl();
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "--lex") == 0) {
        return run_lex_file(argv[2]);
    }

    if (argc == 2) {
        return run_file(argv[1]);
    }

    fprintf(stderr, "Usage: piko [script.piko]\n");
    fprintf(stderr, "       piko --lex <script.piko>\n");
    return 1;
}
