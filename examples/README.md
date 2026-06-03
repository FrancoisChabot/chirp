# Chirp Examples

These are non-trivial Chirp source files meant to show what the
language is intended to feel like at realistic scale.

The current bootstrap executable can parse these files and print their AST, but
it does not execute them yet. Treat them as syntactically correct design examples,
not runnable programs.

If you really want to see what the resulting AST looks like, you can do this:

```bash
build/interpreter/chirp --ast-dump examples/lexer.chirp
build/interpreter/chirp --ast-dump examples/parser.chirp
```