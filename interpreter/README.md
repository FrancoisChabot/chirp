These are chunks of the eventual self-hosted chirp interpreter and compiler. As the bootstrap interpreter matures and becomes capable of interpreting them in their entirety, we will keep adding more.

Once the interpreter and compiler in here is complete and the bootsrapper can deal with all of it, then we will switch over. 

However, It makes no sense to one-shot build the compiler in a language that doesn't exist yet. We expect that practical issues will be surfacing from time to time that will require refining the chirp language's spec. Because of this, the interpreter will likely be in a constant state of incompleteness.

## Desync warning

The syntax *used* by these files will always stay in sync with the chirp language as it is being refined, but their content may not. The logic expressed by the lexer and parser will be updated occasionally to keep up with refinements, but we will not ensure that it is always representative of the language's state in every single commit on the repo.