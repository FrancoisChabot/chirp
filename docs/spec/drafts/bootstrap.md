# The Bootstrap (Draft)

This chapter clarifies the relationship between the core Chirp interpreter and the standard bootstrap, establishing rules for how language features should be designed, implemented, and specified.

## The Interpreter vs. The Bootstrap

Chirp's architecture cleanly separates the mechanical execution of code from the definition of its standard user-facing semantics. 

This separation means that **Chirp is primarily built in Chirp.** 

### The Interpreter (`chirp` binary): 

What belongs in the interpreter may evolve during bootstrapping. For example, the current reference interpreter still exposes printing directly. Such cases should be treated as implementation staging, not as a design preference.

The general rule is "If it can be done in the bootstrap, it should". However, that ambiguity is only the specification writer's problem. The specification will keep what lives where clear.

The interpreter must inject one and only one symbol into the global namespace prior to loading a bootstrap:

```chirp
`import(key: string, format: string = "chirp"): `any;
```

Furthermore, it must respond to queries on this endpoint when the format is `"__chirp_boot"` like so:

- While loading the bootstrap: Provide the keyed values as the rest of the specification prescribes

- In user code: Reject the query and abort evaluation.

### The Bootstrap (`lib/chirp/boot/`):

A set of Chirp modules evaluated before user code. Their public exports are published directly into the interpreter's initial global environment.

- A bootstrap is compliant if its observable public exports and semantics match the requirements specified in this specification as a whole.

- A spec-compliant interpreter is free to cache or even hardcode a spec-compliant bootstrap. However, it must provide a mechanism to replace it with an alternative one.

- After loading a compliant bootstrap, an interpreter may resolve standard bootstrap-provided bindings from the resulting global environment for its own internal use. For example, it may retrieve `` `any `` rather than hardcoding it, and may assume its presence when a known compliant bootstrap has been loaded.

## Specification Rules and Delimitations

From this chapter on, every feature and mechanism prescribed by the specification will be split in two parts:

**The interpreter's burden:** What a compliant interpreter must provide as far as behavior and the "__chirp_boot" symbols it must expose.

**The bootstrap's burden:** What a compliant bootstrap provides as far as user-facing features go. This **can** involve a reference implementation of the feature as an addendum, but the functionality should be presented from a user's point of view.
