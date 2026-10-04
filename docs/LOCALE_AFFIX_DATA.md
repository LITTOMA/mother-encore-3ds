# Checked locale affix content

`content/native-locale-grammar.json` records the reviewed French elision and
German genitive content from `Scripts/global/text_tools.gd` at upstream commit
`7d9246600fffe518408f5830d4848635019005a3`. The source digest, lock, inventory and
actual source bytes are checked before compilation. Ordinary compilation reads
the IR without replacing it. Vowel/end-character sets, finite case pairs and
both output suffixes live in an independent checked binary affix block inside
`localization.enclocale`; C++ supplies only matching, UTF-8 decoding and schemas.

The locale resource format is now 2. The nested affix block has its own version
1, capability 1, size, CRC, upstream identity, two typed profiles and bounded
UTF-8 strings. Unknown profiles, incomplete/duplicate matching or case pairs,
unrecognized versions/capabilities, malformed UTF-8, missing/truncated blocks
and damaged checksums reject the entire catalog before replacing its owner.
Save schema, gameplay rule compatibility, actor identities and the separate
locale preference format remain unchanged.

The previous native elision path used byte-based lowercase and only `aeiou`.
The source checks the first Unicode character against
`aeiouáàâäæéèêëíìîïóòôöœúùûü`. This migration includes that full source set and
the audited uppercase partner of each matching character as external case
pairs. Lowercase and those uppercase partners now follow the source instead of
silently taking the nonvowel branch. This is a finite reviewed mapping; it does
not claim a general Unicode lowercase service. German terminal `s`/`x` remains
case-sensitive as in the source, and its output includes the unchanged name.

The existing explicit `ninten`/`partylead` name contexts use these profiles in
both battle and House localized consumers. Implicit next-word elision, other
actor contexts, name declension and a general TextTools tag interpreter remain
unsupported and reject explicitly. This migration does not enable French or
German native language selection; English and Simplified Chinese admission and
existing naming/battle/House flows remain as before.

`locale_grammar_tests` covers the source vowel/case set, German case boundaries,
unknown/malformed parser paths, failure rollback and catalog ownership. An A/B
binary suffix change is consumed by the same `LocalizedPresentation` executable
while other translations remain unchanged. The asset tests check reproducible
compilation, source/IR mismatch, duplicate fields and independent external data.
Host checks do not establish emulator or physical-console acceptance.
