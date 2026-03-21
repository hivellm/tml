# VTable Deduplication via vtable_content_to_name_

**Category**: codegen
**Tags**: codegen, vtable, optimization, dedup

## Description

Identical vtables (same method set, same implementations) are deduplicated using vtable_content_to_name_ map. This prevents bloat from multiple types implementing the same behavior with identical method bodies. The content hash includes method signatures and implementations.

## When to Use

When generating vtables for dynamic dispatch (dyn Behavior). The deduplication is automatic in the legacy codegen path.
