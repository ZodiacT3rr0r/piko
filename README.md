# Piko

Piko is a bytecode-compiled scripting language written in C11.
It is designed as a compact, modern language for scripting and data transformation, with first-class functions, closures, lists, maps, builtins, and a stack-based virtual machine.

## Highlights

- dynamically typed runtime
- recursive-descent parser
- bytecode compiler
- stack-based virtual machine
- closures with upvalue capture
- mark-and-sweep garbage collection
- lists, maps, and native builtins
- pipeline operator for functional-style data flow
- REPL and file execution

## What Piko is for

Piko is meant to feel like a small, practical scripting language you can use for quick programs, data processing, and language experimentation.

It is intentionally designed to demonstrate the full implementation path of a real language:

source text → lexer → parser → AST → compiler → bytecode → VM

## Building

Requirements:
- GCC or Clang
- GNU Make
- C11-compatible toolchain

Build the project:

```sh
make
```

Clean build outputs:

```sh
make clean
```

## Running

Start the REPL:

```sh
./piko
```

Run a script file:

```sh
./piko path/to/script.piko
```

## Testing

Run the sample programs:

```sh
make test
```

The tests are simple `.piko` scripts that exercise the language features and runtime behavior.

## Language features

Piko supports:
- variables and assignment
- arithmetic and comparison
- `if`, `while`, and `for in`
- functions and recursion
- closures and lexical scoping
- strings, lists, and maps
- indexing and mutation
- pipeline-based function composition
- native builtins such as `print`, `len`, `push`, `pop`, `map`, `filter`, and `reduce`

## Architecture

Piko is organized as a classic compiler pipeline implemented in C:

1. **Lexer** — converts source text into tokens
2. **Parser** — builds an abstract syntax tree
3. **Compiler** — emits bytecode into chunks
4. **VM** — executes bytecode on a stack machine
5. **GC** — reclaims unused heap objects

This structure keeps the language implementation modular and easier to debug.

## Development notes

The project is split into small source files so each subsystem stays isolated:
- runtime values
- heap objects
- bytecode chunks
- tables and interned strings
- lexer and parser
- AST representation
- compiler
- virtual machine
- builtins

## Example

```piko
fn make_counter() {
	let count = 0;
	return fn() {
		count = count + 1;
		return count;
	};
}

let counter = make_counter();
print(counter());
print(counter());
```

## Why this project exists

Piko is a systems programming and language design project. It demonstrates how to build a real language runtime from scratch in C while keeping the codebase small, understandable, and maintainable.
