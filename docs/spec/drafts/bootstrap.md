# The Bootstrap (Draft)

## The Interpreter vs. The Bootstrap

**Chirp is primarily built in Chirp.** As a guiding principle, the more stuff we can shove in the bootstrap code, the better. At the same time, the special rules under which the boostrap code operates is kept to a minimum.

A good example of that principle in action is the `final` keyword. We *could* have defined `true` as a keyword. We *could* have stipulated that symbols defined in the bootstrap. Instead, we figured it's better to make unshadowability a general-purpose feature that the bootstrap happens to make use of.

> [!Note]
> What belongs in the interpreter is still a bit in flux. For example, the current reference interpreter still exposes printing directly.

### The Interpreter (`chirp` binary): 

The interpreter must inject one and only one symbol into the global namespace prior to loading a bootstrap:

```chirp
`import(key: string, format: string = "chirp"): `any;
```

Furthermore, it must implement the `"__chirp_boot"` format like this:

- While loading the bootstrap: Provide the keyed values as the rest of the specification prescribes.
- In user code: Reject the query and abort evaluation.

### The Bootstrap (`lib/chirp/boot/`):

A set of Chirp modules evaluated before user code. Their public exports are published directly into the interpreter's initial global environment.

- A bootstrap is compliant if its observable public exports and semantics match the requirements specified in this specification as a whole.

- A spec-compliant interpreter is free to cache or even hardcode a spec-compliant bootstrap. However, it must provide a mechanism to replace it with an alternative one.

- After loading a compliant bootstrap, an interpreter may resolve standard bootstrap-provided bindings from the resulting global environment for its own internal use. For example, it may retrieve `` `any `` rather than hardcoding it, and may assume its presence when a known compliant bootstrap has been loaded.

## Specification Rules and Delimitations

From this chapter on, every feature and mechanism prescribed by the specification will contain two authoritative sections:

**The interpreter's burden:** What a compliant interpreter must provide as far as behavior and the "__chirp_boot" symbols it must expose.

**The bootstrap's burden:** What a compliant bootstrap provides as far as user-facing features go. This **can** involve a reference implementation of the feature as an addendum.
