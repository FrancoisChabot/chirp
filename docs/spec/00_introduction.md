# The Chirp Specification

Behold, Chirp:

```
b ∈ S ≜ τ(S).bp(S, b.cv : b.lc ⊆ b.fc) : τ(S).br(S, b.lc) ⊆ {true, false, undecided}
```

That probably seems cryptic to you right now, but don't worry. By the end of the next chapter, these equations will make sense. By the end of the chapter after that, they should feel inevitable.

## Preface

The objective of this set of documents is to provide a single authoritative source of truth about the Chirp language for the purpose of implementors. While it may be a useful resource for users of the language from time to time, the document is not meant to be a user guide.

Chirp's design is a bit of an ouroboros of internal circular dependencies, and some aspects that appear fundamental at first glance are actually scaffolded on top of emergent behaviors that are far from evident when looking at the raw building blocks of the language. This speaks to the language's elegance, but it also means that its formal description can appear confusing as all heck on first contact.

To alleviate this, this specification begins with a chronicle of the design process, charting the exact sequence of reasoning that led to the core ontology.

---

## Introduction

> [!Note]
> This section doesn't represent the current state of chirp, but where we are planning to take it.

Chirp is meant to be used in *most* (though not all) places where you would reach for C, C++, Rust, or Zig. However, its architecture is fundamentally different: **It does not have a compiler in the traditional sense.**

The `chirp` executable is first and foremost an **interpreter**. This is where your "program" starts. In this environment, there are no physical constraints—types are first-class values, sets are dynamic predicates, and meta-programming is just ordinary, run-of-the-mill code execution.

**Compilation is not a phase of the executable; it is a library function** (provided by the standard library) that selectively lowers parts of your dynamic script into high-performance, zero-abstraction systems code.

The bridge between these two worlds is a process called **Calcification**. When you invoke compilation on a function entrypoint, the compiler freezes a closed-world snapshot of the interpreter's state and analyzes the flow of constraints. 

To enforce physical reality, the compiler enforces the **Golden Rule of Code Generation**:

> [!IMPORTANT]
> **Any binding that participates in dynamic runtime behavior MUST have its constraint reduced to a set containing only values with representable types (scalars, structs, tagged unions, or memory locations).**

If a binding in the compiled path cannot be proven down to a concrete physical layout, compilation halts. This ensures that the generated machine code (often lowered to C or LLVM IR) is deterministic, aligned, and blistering-fast.

---

## Conventions

### Belonging

We use **belonging** for the `∈` relationship instead of the conventional **membership**. This is intentional.

Sets are fundamental to Chirp, not something built on top of it. In fact, most aspects of the language are described in terms of set theory. That becomes a problem as soon as we introduce the `.` operator, because `foo.bar` reads as "the bar member of foo" to most people with OOP experience. As much as we'd like to give sets first dibs on the word, there is too much inertia behind that usage.

So in Chirp land, values **belong** to sets. They are not "members" of sets.

### Syntactic Hygiene: The Backtick

In Chirp code, (almost) all language intrinsics are prefixed with a backtick (e.g., `` `any `` or `` `type ``). This ensures that user-defined code can never accidentally conflict with system-provided symbols.

However, since \nested backticks (`` ``` `any` ``` ``) is visually noisy and cumbersome in Markdown text, this specification adopts a standard convention: the backtick prefix is ommited when discussing these intrinsics in standard text paragraphs (e.g., writing "the type of `any` is `Type`"). The backtick prefix is only strictly included inside full code blocks or when disambiguation is absolutely necessary.

### Mathematical Notation: The Type-Of Operator (`τ`)

Throughout this specification's formal equations, we use the Greek letter **`τ` (tau)** as a compact, single-character mathematical substitute for the language's `typeof(x)` operator.

---

## Specification Roadmap

This specification is organized to guide you step-by-step through the mechanics of the language:

- **[01_derivation.md](01_derivation.md):** Why Chirp is the way that it is.
- **[02_core.md](02_core.md):** Bindings, Sets, Values, and Types. That's all we need.
- **[03_machine.md](03_machine.md):** How code becomes emittable to C. 
- **[04_lexical.md](04_lexical.md):** Boring but necessary details about how to parse code. 
- **[05_grammar.md](05_grammar.md):** Less boring, but still necessary, details about how to read the code. 
