#include "common.h"
#include "lexer.h"
#include "memory.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include "builtins.h"

#include <ctype.h>

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
    char wrapped[4128];

    VM vm;
    vm_init(&vm);
    builtins_install(&vm);

    printf("Piko REPL (C11 bytecode VM)\n");
    printf("Type 'exit' or Ctrl+D to quit.\n");

    for (;;) {
        printf(">> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        size_t len = strlen(line);
        while (len > 0 && isspace((unsigned char)line[len - 1])) {
            line[--len] = '\0';
        }
        if (len == 0) {
            continue;
        }
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        }

        /* Auto-append ';' for single-line interactive expressions/statements. */
        const char *source = line;
        if (line[len - 1] != ';' && line[len - 1] != '}') {
            snprintf(wrapped, sizeof(wrapped), "%s;", line);
            source = wrapped;
        }

        bool had_parse_error = false;
        Node *program = parse_source(source, &had_parse_error);
        if (had_parse_error) {
            node_free(program);
            continue;
        }

        /* In interactive mode, if the user typed a single expression statement, print its value. */
        if (program->type == NODE_PROGRAM &&
            program->as.program.count == 1 &&
            program->as.program.items[0]->type == NODE_EXPR_STMT) {
            Node *stmt = program->as.program.items[0];
            Node *expr = stmt->as.expr_stmt.expr;
            stmt->type = NODE_PRINT_STMT;
            stmt->as.print_stmt.expr = expr;
        }

        Chunk chunk;
        gc_set_enabled(false);
        bool compile_ok = compile_program(program, &chunk);
        if (compile_ok) {
            vm_interpret(&vm, &chunk);
        }

        chunk_free(&chunk);
        node_free(program);
    }

    vm_free(&vm);
}

static int run_file(const char *path) {
    size_t read = 0;
    char *buffer = read_file_bytes(path, &read);
    if (buffer == NULL) {
        return 1;
    }

    bool had_parse_error = false;
    Node *program = parse_source(buffer, &had_parse_error);
    if (had_parse_error) {
        node_free(program);
        free(buffer);
        return 1;
    }

    VM vm;
    vm_init(&vm);
    gc_set_enabled(false);

    Chunk chunk;
    bool compile_ok = compile_program(program, &chunk);
    if (!compile_ok) {
        chunk_free(&chunk);
        vm_free(&vm);
        node_free(program);
        free(buffer);
        return 1;
    }

    builtins_install(&vm);
    InterpretResult result = vm_interpret(&vm, &chunk);

    vm_free(&vm);
    chunk_free(&chunk);
    node_free(program);
    (void)read;
    free(buffer);
    return result == VM_INTERPRET_OK ? 0 : 1;
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

static int run_parse_file(const char *path) {
    char *source = read_file_bytes(path, NULL);
    if (source == NULL) {
        return 1;
    }

    bool had_error = false;
    Node *program = parse_source(source, &had_error);
    node_print(program, 0);
    node_free(program);
    free(source);

    return had_error ? 1 : 0;
}

static int run_compile_file(const char *path) {
    char *source = read_file_bytes(path, NULL);
    if (source == NULL) {
        return 1;
    }

    bool had_parse_error = false;
    Node *program = parse_source(source, &had_parse_error);
    if (had_parse_error) {
        node_free(program);
        free(source);
        return 1;
    }

    Chunk chunk;
    bool ok = compile_program(program, &chunk);
    if (ok) {
        disassemble_chunk(&chunk, path);
    }

    chunk_free(&chunk);
    gc_free_all();
    node_free(program);
    free(source);
    return ok ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        run_repl();
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "--lex") == 0) {
        return run_lex_file(argv[2]);
    }

    if (argc == 3 && strcmp(argv[1], "--parse") == 0) {
        return run_parse_file(argv[2]);
    }

    if (argc == 3 && strcmp(argv[1], "--compile") == 0) {
        return run_compile_file(argv[2]);
    }

    if (argc == 2) {
        return run_file(argv[1]);
    }

    fprintf(stderr, "Usage: piko [script.piko]\n");
    fprintf(stderr, "       piko --lex <script.piko>\n");
    fprintf(stderr, "       piko --parse <script.piko>\n");
    fprintf(stderr, "       piko --compile <script.piko>\n");
    return 1;
}
