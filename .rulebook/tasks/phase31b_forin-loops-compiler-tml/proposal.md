# Proposal: phase31b_forin-loops-compiler-tml

## Why
The compiler-tml library has ~105 manual index loop instances -- the highest density of any library. The AST writer alone has 40+. Converting to `for i in 0 to N` dogfoods the new syntax in the self-hosting compiler and eliminates boilerplate.

Source: docs/analysis/core-std-ergonomics-audit/

## What Changes
Replace all manual index loop patterns in `compiler-tml/src/` with for-in syntax. Pure mechanical refactor.

## Impact
- Affected specs: none
- Affected code: compiler-tml/src/ (~105 sites across 12 files)
- Breaking change: NO
- User benefit: Cleaner self-hosting compiler code
