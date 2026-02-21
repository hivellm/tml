//! # Borrow Error Explanations
//!
//! Error codes B001-B017 for ownership and lifetime errors.

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_borrow_explanations() {
    static const std::unordered_map<std::string, std::string> db = {

        {"B001", R"EX(
Use after move [B001]

A value was used after it was moved to another binding. In TML, when a
non-copyable value is assigned to a new variable or passed to a function,
ownership is transferred (moved). The original variable becomes invalid.

Example of erroneous code:

    let s = "hello"
    let t = s                   // 's' is moved to 't'
    print(s)                    // error: 's' was already moved

How to fix:

1. Use the new owner instead:
       print(t)

2. Duplicate the value if you need both:
       let t = s.duplicate()
       print(s)                 // 's' is still valid

3. Use a reference instead of moving:
       let t = ref s
       print(s)

Related: B005 (borrow after move), B011 (partial move)
)EX"},

        {"B002", R"EX(
Move while borrowed [B002]

A value was moved while there is still an active borrow (reference) to it.
Moving a value would invalidate any existing references, which could lead
to dangling pointers.

Example of erroneous code:

    let data = [1, 2, 3]
    let r = ref data            // immutable borrow
    let moved = data            // error: cannot move while borrowed
    print(r)                    // borrow still in use here

How to fix:

1. Drop the borrow before moving:
       let data = [1, 2, 3]
       {
           let r = ref data
           print(r)
       }                        // borrow ends here
       let moved = data         // now safe to move

2. Duplicate instead of moving:
       let data = [1, 2, 3]
       let r = ref data
       let copy = data.duplicate()
)EX"},

        {"B003", R"EX(
Assign to non-mutable variable [B003]

An attempt was made to modify a variable through a reference that is not
declared as mutable, or to assign to a `let` binding.

Example of erroneous code:

    let x = 42
    x = 10                      // 'x' is not mutable

How to fix:

    var x = 42                  // use 'var' for mutable binding
    x = 10                      // now assignment works

For references:

    var data = [1, 2, 3]
    let r = mut ref data        // mutable reference
    r.push(4)                   // mutation through mutable ref

Related: T013 (immutable assignment)
)EX"},

        {"B004", R"EX(
Assign while borrowed [B004]

A variable was assigned to while there is still an active borrow to it.
Assigning would change the value that the borrow points to, potentially
invalidating the reference.

Example of erroneous code:

    var x = 42
    let r = ref x               // borrow 'x'
    x = 10                      // error: 'x' is borrowed
    print(r)                    // borrow used here

How to fix:

1. Use the borrow before reassigning:
       var x = 42
       let r = ref x
       print(r)                 // use borrow here
       x = 10                   // now safe to reassign

2. Limit the borrow scope:
       var x = 42
       { let r = ref x; print(r) }
       x = 10
)EX"},

        {"B005", R"EX(
Borrow after move [B005]

An attempt was made to borrow a value that has already been moved. Once
a value is moved, the original binding is invalid and cannot be borrowed.

Example of erroneous code:

    let data = [1, 2, 3]
    let moved = data            // value moved here
    let r = ref data            // error: 'data' was moved

How to fix:

    let data = [1, 2, 3]
    let r = ref data            // borrow before moving
    // ... use r ...
    let moved = data            // move after borrow is done

Related: B001 (use after move)
)EX"},

        {"B006", R"EX(
Mutable borrow of non-mutable variable [B006]

A mutable reference (`mut ref`) was taken to a variable that was not
declared as mutable (`var`). Only `var` bindings can be mutably borrowed.

Example of erroneous code:

    let data = [1, 2, 3]
    let r = mut ref data        // error: 'data' is not mutable

How to fix:

    var data = [1, 2, 3]        // use 'var' to allow mutation
    let r = mut ref data        // now mutable borrow works
    r.push(4)
)EX"},

        {"B007", R"EX(
Mutable borrow while immutably borrowed [B007]

A mutable borrow was taken while an immutable borrow is still active.
TML enforces that you cannot have a mutable reference while any other
reference (mutable or immutable) exists to the same value.

Example of erroneous code:

    var data = [1, 2, 3]
    let r1 = ref data           // immutable borrow
    let r2 = mut ref data       // error: already borrowed immutably
    print(r1)                   // immutable borrow used here

How to fix:

1. Use the immutable borrow first:
       var data = [1, 2, 3]
       let r1 = ref data
       print(r1)                // done with immutable borrow
       let r2 = mut ref data    // now safe to borrow mutably

2. Use only one kind of borrow:
       var data = [1, 2, 3]
       let r = mut ref data
       // use r for both reading and writing

Related: B008 (double mutable borrow), B009 (immutable while mutable)
)EX"},

        {"B008", R"EX(
Double mutable borrow [B008]

Two mutable borrows were taken from the same value at the same time.
TML allows at most ONE mutable reference to a value at any given time.
This prevents data races and aliasing bugs.

Example of erroneous code:

    var data = [1, 2, 3]
    let r1 = mut ref data       // first mutable borrow
    let r2 = mut ref data       // error: already mutably borrowed
    r1.push(4)

How to fix:

1. Use one borrow at a time:
       var data = [1, 2, 3]
       {
           let r1 = mut ref data
           r1.push(4)
       }                        // first borrow ends
       let r2 = mut ref data    // now safe

2. Use a single reference for all mutations:
       var data = [1, 2, 3]
       let r = mut ref data
       r.push(4)
       r.push(5)

Related: B007 (mutable + immutable borrow conflict)
)EX"},

        {"B009", R"EX(
Immutable borrow while mutably borrowed [B009]

An immutable borrow was taken while a mutable borrow is still active.
TML enforces exclusive access for mutable borrows — no other references
(mutable or immutable) can exist simultaneously.

Example of erroneous code:

    var data = [1, 2, 3]
    let r_mut = mut ref data    // mutable borrow
    let r_imm = ref data        // error: already mutably borrowed
    r_mut.push(4)

How to fix:

    var data = [1, 2, 3]
    let r_mut = mut ref data
    r_mut.push(4)               // finish with mutable borrow
    // r_mut goes out of scope
    let r_imm = ref data        // now safe to borrow immutably

Related: B007 (mutable borrow while immutably borrowed)
)EX"},

        {"B010", R"EX(
Return local reference [B010]

A function returns a reference to a local variable. When the function
returns, the local variable is dropped, making the reference dangling.

Example of erroneous code:

    func bad() -> ref Str {
        let s = "hello"
        return ref s            // error: 's' will be dropped
    }

How to fix:

    func good() -> Str {
        let s = "hello"
        return s                // return owned value instead
    }

If you need to return a reference, it must refer to data that outlives
the function (e.g., a parameter or static data):

    func first(items: ref List[I32]) -> ref I32 {
        return ref items[0]     // borrows from the input
    }
)EX"},

        {"B011", R"EX(
Partial move [B011]

A field of a struct was moved, making the remaining struct partially
invalid. You cannot use the struct after one of its fields has been moved.

Example of erroneous code:

    type Pair { a: Str, b: Str }
    let p = Pair { a: "hello", b: "world" }
    let x = p.a                // moves p.a
    println(p.b)               // error: p is partially moved

How to fix:

    let x = p.a.duplicate()    // duplicate instead of move
    println(p.b)               // p is still fully valid

Related: B001 (use after move)
)EX"},

        {"B012", R"EX(
Overlapping borrow [B012]

Two borrows overlap in a way that violates borrowing rules. This can
happen when borrowing different parts of the same data structure.

Related: B007, B008
)EX"},

        {"B013", R"EX(
Use while borrowed [B013]

A value was used directly while an active borrow exists to it.

Related: B004 (assign while borrowed)
)EX"},

        {"B014", R"EX(
Closure captures moved value [B014]

A closure captures a variable that has already been moved. The closure
would hold a reference to invalid data.

Example of erroneous code:

    let data = [1, 2, 3]
    let moved = data            // moves data
    let f = do() { data.len() } // error: data was moved

How to fix:

    let data = [1, 2, 3]
    let f = do() { data.len() } // capture before moving
    let moved = data            // move after closure is done
)EX"},

        {"B015", R"EX(
Closure capture conflict [B015]

A closure captures a variable in a way that conflicts with other
borrows or captures. For example, two closures both capturing the
same variable mutably.

Related: B008 (double mutable borrow)
)EX"},

        {"B016", R"EX(
Partially moved value [B016]

A value was used after some of its fields have been moved out.

Related: B011 (partial move)
)EX"},

        {"B017", R"EX(
Reborrow outlives origin [B017]

A reborrow (taking a new reference from an existing reference) would
outlive the original reference it was derived from.

Related: B010 (return local reference)
)EX"},

    };
    return db;
}

} // namespace tml::cli::explain
