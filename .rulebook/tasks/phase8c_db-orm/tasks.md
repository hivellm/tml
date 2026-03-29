# Tasks: Database Library — ORM Layer

**Status**: Planning. 0% (0/15).
**Depends on**: phase8b_db-schema-migration

## Phase 1: Model & Field Mapping

- [ ] 1.1 `db/orm/model.tml` — Model behavior (table_name, columns, primary_key, from_row, to_values)
- [ ] 1.2 `db/orm/field.tml` — FieldType enum, FieldMapping
- [ ] 1.3 `db/orm/mapper.tml` — map_row[T: Model], map_values[T: Model]

## Phase 2: Repository

- [ ] 2.1 `db/orm/repository.tml` — Repository[T, C] type
- [ ] 2.2 insert(model), update(model), delete(model)
- [ ] 2.3 find_by_id(id), find_all(), count()

## Phase 3: QuerySet

- [ ] 3.1 `db/orm/query_set.tml` — QuerySet[T] with filter, order_by, limit, offset
- [ ] 3.2 exec(), first(), count()

## Phase 4: Relations

- [ ] 4.1 `db/orm/relation.tml` — HasOne, HasMany, BelongsTo, ManyToMany
- [ ] 4.2 Eager loading via with("relation_name")
- [ ] 4.3 `db/orm/mod.tml` — ORM exports

## Phase 5: Tests

- [ ] 5.1 Tests: model CRUD operations
- [ ] 5.2 Tests: QuerySet chaining
- [ ] 5.3 Tests: relationship loading
