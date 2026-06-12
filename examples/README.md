# Chirp Examples

These are non-trivial Chirp source files meant to show what the
language is intended to feel like at realistic scale.

While the interpreter is fully capable of executing Chirp code, these specific examples cannot be run yet because they rely on standard library features that have not been implemented. Treat them as syntactically correct design examples, not runnable programs.

If you really want to see what the resulting AST looks like, you can do this:

```bash
build/interpreter/chirp --ast-dump examples/lexer.chirp
build/interpreter/chirp --ast-dump examples/parser.chirp
```