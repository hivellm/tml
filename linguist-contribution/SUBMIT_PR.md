# How to submit TML to GitHub Linguist

## Step 1 — Fork and clone Linguist

```bash
# Fork https://github.com/github-linguist/linguist on GitHub, then:
git clone https://github.com/YOUR_USERNAME/linguist
cd linguist
git checkout -b add-tml-language
```

## Step 2 — Add the grammar

The grammar lives in a separate repository that Linguist references as a submodule.
You have two options:

### Option A: Publish the TextMate grammar as a standalone repo

1. Create `https://github.com/hivellm/tml-textmate` (or `tml-vscode`)
2. Put `syntaxes/tml.tmLanguage.json` from this repo there
3. In Linguist, edit `grammars.yml` and add:

```yaml
# Add to grammars.yml
source.tml:
  type: TextMate
  tm_scope: source.tml
  path: vendor/grammars/tml-textmate/syntaxes/tml.tmLanguage.json
  url: https://github.com/hivellm/tml-textmate
  license: MIT
```

Then add the grammar as a submodule:
```bash
git submodule add https://github.com/hivellm/tml-textmate vendor/grammars/tml-textmate
```

### Option B: Inline the grammar file directly

Copy `syntaxes/tml.tmLanguage.json` to `vendor/grammars/tml.tmLanguage.json`
(some small grammars are inlined without a submodule).

## Step 3 — Add to languages.yml

Copy the entry from `languages.yml.entry` in this folder into
`lib/linguist/languages.yml` — insert it alphabetically under "T".

## Step 4 — Add samples

```bash
mkdir -p samples/TML
# Copy all .tml files from linguist-contribution/samples/TML/ here
cp /path/to/tml/linguist-contribution/samples/TML/*.tml samples/TML/
```

## Step 5 — Regenerate and test

```bash
bundle install
bundle exec rake samples          # validate sample files
bundle exec rake linguist         # run the classifier
```

## Step 6 — Open the PR

Title: `Add TML programming language`

Body template:
```
## New language: TML

- **Type**: Programming
- **Website**: https://github.com/hivellm/tml
- **Extensions**: `.tml`
- **Grammar**: https://github.com/hivellm/tml-textmate

TML is a systems programming language with a self-hosting compiler
targeting LLVM IR. It features:
- Algebraic data types (type/enum with variants)
- Behaviors (like Rust traits)
- Generic types with bounds
- Pattern matching via `when`
- Optional chaining `?.` and let-else unwrapping
- Async/await concurrency model
- Low-level memory access via `lowlevel` blocks

The compiler and ~150k lines of standard library are written in TML itself.
```

## Checklist before submitting

- [ ] Grammar file correctly highlights all keywords
- [ ] Sample files cover: types, functions, pattern matching, generics, async, lowlevel
- [ ] `bundle exec rake samples` passes with no errors
- [ ] `bundle exec rake linguist` classifies `.tml` files as TML
- [ ] Entry in `languages.yml` is alphabetically sorted
- [ ] Color `#5A3FBE` does not conflict with existing languages
