# Chirp Specification: Lexical Structure

This document defines the lexical grammar and tokenization rules for the Chirp language. 

Every Chirp program starts as a stream of raw Unicode characters, which the lexical scanner processes into a sequence of **tokens**, discarding whitespace and comments.

---

## Character Encoding & Source File Format

Chirp source files are expected to be encoded in **UTF-8**. 

Chirp fully embraces Unicode to support clean, expressive mathematical set operators (like `∈` and `∪`) in source code. However, to ensure maximum developer ergonomics across different terminal and editor setups, every Unicode operator has a corresponding, standard ASCII equivalent.

---

## Whitespace and Comments

### Whitespace
Whitespace separates tokens and is otherwise discarded by the core parser, except within string literals.
* Standard whitespace characters are: space (`' '`), horizontal tab (`'\t'`), carriage return (`'\r'`), and newline (`'\n'`).
* Newlines increment the source compiler's internal line tracker for diagnostics and error reporting.
* *Tooling Note:* For the sake of the `--format` tool and future language servers, the lexer may retain whitespace and comments as "Trivia" attached to structural tokens, allowing for lossless source reconstruction.

### Comments
Chirp supports single-line comments starting with double slashes `//`. 
* When the scanner encounters `//`, it discards all characters up to the end of the line (`'\n'`).

---

## Identifiers and Keywords

### Identifiers
Identifiers are user-defined names for bindings, types, and fields.
* **Lexical Rule:** An identifier must begin with an alphabetic character (`a..z`, `A..Z`) or an underscore `_`. Subsequent characters can be alphabetic, numeric (`0..9`), or underscores.
* Examples: `x`, `my_cool_thing`, `Point2D`, `_internal_value`.

### Keywords
Keywords are reserved identifiers that have special meaning in the language grammar. They dictate control flow and structural declarations, and cannot be used as standard identifier names.

The core keywords are:
```chirp
let    struct    if    for    else    while    match    break    do
```

`mut` and `final` are contextual binding modifiers, not reserved keywords. They
remain ordinary identifiers outside binding-modifier position. `true` and
`false` are ordinary identifiers bound by the boot prelude as final core values.
`undecided` is also a final boot binding.

---

## Literals

### Numeric Literals (Integers)
Currently, Chirp's bootstrap interpreter focuses on flat, unsigned whole numbers.
* **Lexical Rule:** A sequence of one or more digits `0..9`.
* Examples: `0`, `42`, `1000`.

### Character Literals
A single character enclosed in single quotes.
* **Lexical Rule:** `'` followed by any single character (or a backslash escape sequence) followed by `'`.
* Chirp supports standard C-style escape sequences: `\'`, `\"`, `\\`, `\n`, `\r`, `\t`, and `\0`.
* Examples: `'a'`, `'\n'`, `'\''`.

### String Literals
A sequence of characters enclosed in double quotes.
* **Lexical Rule:** `"` followed by zero or more characters (including escape sequences) followed by `"`.
* Examples: `"hello"`, `"Line 1\nLine 2"`.

---

## First-Class Symbol and Intrinsic Namespaces

To keep the parser lightweight and prevent namespace collision, Chirp utilizes two distinct, prefix-reserved lexical namespaces:

### Symbols (`#identifier`)
A hash symbol `#` followed directly by an identifier.
* **Lexical Rule:** `#` followed by a sequence of alphanumeric characters/underscores.
* Symbols are unique, first-class constants whose value is their own identity (equivalent to atoms in Erlang/Elixir or symbols in Ruby).
* Examples: `#eof`, `#identifier`, `#error`, `#pending`.

### Intrinsics (`` `identifier ``)
A backtick `` ` `` followed directly by an identifier.
* **Lexical Rule:** `` ` `` followed by a sequence of alphanumeric characters/underscores.
* **The Formalism:** If a compiler-reserved keyword acts syntactically as an identifier (i.e., a primary expression that evaluates to a value), it must be given the backtick prefix. This creates an open namespace allowing the compiler to introduce new built-ins (like `` `import `` or `` `type ``) without ever shadowing user code. Standard keywords (like `if` or `match`) dictate grammar and do not receive backticks.
* **Boot Constants:** The core constants `true`, `false`, and `undecided` are final boot bindings. They keep ordinary literal-like ergonomics without needing lexer keyword special cases.
* **Boot Definitions:** Bootstrap sources may define backtick-prefixed bindings. User code may reference those bindings, but may not introduce new ones.
* Examples: `` `print ``, `` `import ``, `` `type ``.

---

## Operators and Punctuation

The following character sequences are recognized as operators or punctuation:

| Unicode Operator | ASCII Equivalent | Token Representation | Description |
|:----------------:|:----------------:|:--------------------:|:-----------|
| `=`              | N/A              | `#equal`             | Assignment or binding initialization |
| `==`             | N/A              | `#equal_equal`       | Equality comparison |
| `!=`             | N/A              | `#not_equal`         | Inequality comparison |
| `+`              | N/A              | `#plus`              | Addition |
| `-`              | N/A              | `#minus`             | Subtraction or unary negation |
| `*`              | N/A              | `#asterisk`          | Multiplication or pointer dereference |
| `/`              | N/A              | `#slash`             | Division |
| `;`              | N/A              | `#semicolon`         | Statement terminator |
| `,`              | N/A              | `#comma`             | Sequence separator |
| `:`              | N/A              | `#colon`             | Constraint or type annotation |
| `.`              | N/A              | `#dot`               | Field access or method call |
| `..`             | N/A              | `#dots`              | Range boundary definition |
| `+=`             | N/A              | `#plus_equal`        | In-place addition / mutation |
| `&&`             | N/A              | `#and_and`           | Logical AND |
| `||`             | N/A              | `#or_or`             | Logical OR |
| `->`             | N/A              | `#arrow`             | Pointer-set constraint |
| `=>`             | N/A              | `#fat_arrow`         | Lambda / Match mapping |
| `\|`             | N/A              | `#bar`               | Set comprehension divider |
| `{`              | N/A              | `#open_brace`        | Start of set, match, or struct |
| `}`              | N/A              | `#close_brace`       | End of set, match, or struct |
| `[`              | N/A              | `#open_bracket`      | Start of sequence / index |
| `]`              | N/A              | `#close_bracket`     | End of sequence / index |
| `(`              | N/A              | `#open_paren`        | Start of parameters / grouping |
| `)`              | N/A              | `#close_paren`       | End of parameters / grouping |
| **`∈`**          | **`` `in` ``**         | `#belonging`         | Set belonging test (or loop) |
| **`∪`**          | **`` `union` ``**      | `#union`             | Set union |
| **`∩`**          | **`` `intersection` ``**| `#intersection`     | Set intersection |
| **`⊆`**          | **`` `subset` ``**     | `#subset`            | Set subset test |

---

## Auxiliary Notes

* **Why First-Class Symbols?** 
  In C-like languages, developers frequently use `enum` constants (which require declaring a new type and namespace) or preprocessor macros (which lack type-safety). Chirp symbols `#symbol_name` require no declaration and map directly to efficient integer constants under the hood. They are incredibly useful for pattern matching, parsing stages, and error states without structural overhead.
* **Namespace Isolation via Backticks:** 
  By reserving the backtick `` ` `` prefix for compiler intrinsics, Chirp ensures that user-defined code can never accidentally conflict with compiler-provided functions or type definitions. Even if the compiler introduces a new intrinsic in a future release, it will be namespaced behind `` ` ``, guaranteeing absolute backward compatibility for user identifiers.
* **Ergonomic ASCII Equivalents:** 
  While mathematical notation like `∈` and `∪` looks gorgeous in specifications and documentation, they can be tedious to type on standard QWERTY keyboards. By utilizing the Intrinsics namespace (e.g., `` `in` `` or `` `union` ``), developers can type ASCII equivalents that parse to the exact same AST representation. This also allows the `chirp --format` tool to automatically perform in-place replacement of these ASCII fallbacks into their proper Unicode forms, upgrading the code aesthetics without polluting the user namespace.

---

## Next Steps

Having laid out the concrete characters and tokens that make up Chirp source code, we can now explore how user-defined types are endowed with custom behaviors.

Next up: [Grammar](05_grammar.md), it's the grammar!
