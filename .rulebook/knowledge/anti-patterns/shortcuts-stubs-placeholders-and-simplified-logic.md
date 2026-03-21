# Shortcuts, stubs, placeholders, and simplified logic

**Category**: code
**Tags**: policy, quality, mandatory, no-shortcuts, no-stubs, no-placeholders, no-todo

## Description

Taking shortcuts to deliver results faster is expressly forbidden. This includes: simplifying logic, adding TODO/FIXME/HACK comments as placeholders, creating stubs or placeholder implementations, altering existing logic to avoid complexity, reducing requested scope, skipping edge cases or error handling, and delivering partial implementations. Response time is NOT important — only the quality of the final result matters.

## Example

❌ WRONG: func parse(input: Str) -> Maybe[I32] { /* TODO: implement proper parsing */ return Nothing }\n❌ WRONG: // HACK: simplified version, handles only positive numbers\n❌ WRONG: Delivering 3 of 5 requested functions saying "the rest can be added later"\n\n✅ CORRECT: Research the correct approach, implement the full parse function with all edge cases (negative numbers, overflow, whitespace, invalid chars), test it thoroughly.

## When to Use

Always. This rule applies to every task, every agent, every implementation — no exceptions.

## When NOT to Use

Never. There is no scenario where shortcuts are acceptable.
