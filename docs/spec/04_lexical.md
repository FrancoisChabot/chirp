# Chirp Specification: Lexical Structure

This document defines the lexical grammar and tokenization rules for the Chirp language. 

Every Chirp program starts as a stream of raw Unicode characters, which the lexical scanner processes into a sequence of **tokens**, discarding whitespace and comments.

---

## Character Encoding & Source File Format

Chirp source files are expected to be encoded in **UTF-8**. 

---

## Whitespace and Comments

### Whitespace

` `, `\t`, `\r`, `\n`

### Comments

Single-line comments starting with double slashes `//`. 

## Identifiers and Keywords

### Identifiers
Identifiers are user-defined names for bindings, types, and fields.
* **Lexical Rule:** An identifier must begin with an alphabetic character (`a..z`, `A..Z`) or an underscore `_`. Subsequent characters can be alphabetic, numeric (`0..9`), or underscores.
* Examples: `x`, `my_cool_thing`, `Point2D`, `_internal_value`.

#### Intrinsics (`` `identifier ``)
A backtick `` ` `` followed directly by an identifier. The only thing distinguishing those from regular identifiers is that they cannot be defined in user code.

### Keywords
Keywords are reserved identifiers that have special meaning in the language grammar. They dictate control flow and structural declarations, and cannot be used as standard identifier names.

The core keywords are:
```chirp
let    struct    if    for    else    while    match    break    do
```

`pub`, `mut`, and `final` are contextual declaration/binding modifiers, not reserved keywords. They
remain ordinary identifiers outside binding-modifier position.

> [!NOTE]
> There are some other identifiers that are defined in the global scope during bootstrapping. `int`, `bool`, `string`, `void`, `true`, `false`, `undecided`. Those are NOT keywords from a lexical pov, just identifiers that happen to have the final property.

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

### Symbols (`#identifier`)
A hash symbol `#` followed directly by an identifier.
* **Lexical Rule:** `#` followed by a sequence of alphanumeric characters/underscores.
* Symbols are unique, first-class constants whose value is their own identity (equivalent to atoms in Erlang/Elixir or symbols in Ruby).
* Examples: `#eof`, `#identifier`, `#error`, `#pending`.

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
| **`∉`**          | **`` `notin` ``**      | `#not_in`            | Set not belonging test |
| **`∪`**          | **`` `or` ``**         | `#union`             | Set union |
| **`∩`**          | **`` `and` ``**        | `#intersection`     | Set intersection |
