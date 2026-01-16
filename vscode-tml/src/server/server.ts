import {
    createConnection,
    TextDocuments,
    ProposedFeatures,
    InitializeParams,
    InitializeResult,
    TextDocumentSyncKind,
    CompletionItem,
    CompletionItemKind,
    Hover,
    MarkupKind,
    TextDocumentPositionParams,
    SemanticTokensParams,
    SemanticTokens,
    SemanticTokensBuilder,
    SemanticTokensLegend,
    Diagnostic,
    DiagnosticSeverity,
    DidChangeConfigurationNotification,
    DidChangeConfigurationParams,
    TextDocumentChangeEvent,
    Location,
    Range,
    Position,
    SymbolKind,
    TypeHierarchyItem,
    TypeHierarchyPrepareParams,
    TypeHierarchySupertypesParams,
    TypeHierarchySubtypesParams
} from 'vscode-languageserver/node';

import { TextDocument } from 'vscode-languageserver-textdocument';
import { exec } from 'child_process';
import { promisify } from 'util';
import { setTimeout as setTimeoutNode, clearTimeout as clearTimeoutNode } from 'timers';
import * as path from 'path';
import * as fs from 'fs';
import * as os from 'os';

const execAsync = promisify(exec);

// Create a connection for the server
const connection = createConnection(ProposedFeatures.all);

// Create a text document manager
const documents: TextDocuments<TextDocument> = new TextDocuments(TextDocument);

// Semantic token types
const tokenTypes = [
    'namespace',
    'type',
    'class',
    'enum',
    'interface',
    'struct',
    'typeParameter',
    'parameter',
    'variable',
    'property',
    'enumMember',
    'event',
    'function',
    'method',
    'macro',
    'keyword',
    'modifier',
    'comment',
    'string',
    'number',
    'regexp',
    'operator',
    'decorator'
];

// Semantic token modifiers
const tokenModifiers = [
    'declaration',
    'definition',
    'readonly',
    'static',
    'deprecated',
    'abstract',
    'async',
    'modification',
    'documentation',
    'defaultLibrary'
];

const legend: SemanticTokensLegend = {
    tokenTypes,
    tokenModifiers
};

// TML Keywords with documentation
const TML_KEYWORDS: { name: string; doc: string; kind: CompletionItemKind }[] = [
    { name: 'and', doc: 'Logical AND operator', kind: CompletionItemKind.Keyword },
    { name: 'as', doc: 'Type casting operator', kind: CompletionItemKind.Keyword },
    { name: 'async', doc: 'Asynchronous function marker', kind: CompletionItemKind.Keyword },
    { name: 'await', doc: 'Wait for async result', kind: CompletionItemKind.Keyword },
    { name: 'behavior', doc: 'Define a behavior (trait)', kind: CompletionItemKind.Keyword },
    { name: 'break', doc: 'Exit a loop', kind: CompletionItemKind.Keyword },
    { name: 'const', doc: 'Constant declaration', kind: CompletionItemKind.Keyword },
    { name: 'continue', doc: 'Skip to next loop iteration', kind: CompletionItemKind.Keyword },
    { name: 'crate', doc: 'Reference to current crate', kind: CompletionItemKind.Keyword },
    { name: 'decorator', doc: 'Function decorator', kind: CompletionItemKind.Keyword },
    { name: 'do', doc: 'Closure/lambda syntax: do(x) x + 1', kind: CompletionItemKind.Keyword },
    { name: 'else', doc: 'Alternative branch', kind: CompletionItemKind.Keyword },
    { name: 'false', doc: 'Boolean false literal', kind: CompletionItemKind.Keyword },
    { name: 'for', doc: 'For loop: for item in collection { }', kind: CompletionItemKind.Keyword },
    { name: 'func', doc: 'Function declaration', kind: CompletionItemKind.Keyword },
    { name: 'if', doc: 'Conditional branch', kind: CompletionItemKind.Keyword },
    { name: 'impl', doc: 'Implementation block', kind: CompletionItemKind.Keyword },
    { name: 'in', doc: 'Used in for loops', kind: CompletionItemKind.Keyword },
    { name: 'let', doc: 'Variable declaration', kind: CompletionItemKind.Keyword },
    { name: 'loop', doc: 'Infinite loop', kind: CompletionItemKind.Keyword },
    { name: 'lowlevel', doc: 'Unsafe code block', kind: CompletionItemKind.Keyword },
    { name: 'mod', doc: 'Module declaration', kind: CompletionItemKind.Keyword },
    { name: 'mut', doc: 'Mutable modifier', kind: CompletionItemKind.Keyword },
    { name: 'not', doc: 'Logical NOT operator', kind: CompletionItemKind.Keyword },
    { name: 'or', doc: 'Logical OR operator', kind: CompletionItemKind.Keyword },
    { name: 'pub', doc: 'Public visibility modifier', kind: CompletionItemKind.Keyword },
    { name: 'quote', doc: 'Macro quoting', kind: CompletionItemKind.Keyword },
    { name: 'ref', doc: 'Reference type', kind: CompletionItemKind.Keyword },
    { name: 'return', doc: 'Return from function', kind: CompletionItemKind.Keyword },
    { name: 'super', doc: 'Parent module reference', kind: CompletionItemKind.Keyword },
    { name: 'then', doc: 'Used after conditions (if x then y)', kind: CompletionItemKind.Keyword },
    { name: 'this', doc: 'Current instance reference', kind: CompletionItemKind.Keyword },
    { name: 'This', doc: 'Current type reference', kind: CompletionItemKind.Keyword },
    { name: 'through', doc: 'Inclusive range end (1 through 10)', kind: CompletionItemKind.Keyword },
    { name: 'to', doc: 'Exclusive range end (1 to 10)', kind: CompletionItemKind.Keyword },
    { name: 'true', doc: 'Boolean true literal', kind: CompletionItemKind.Keyword },
    { name: 'type', doc: 'Type/struct declaration', kind: CompletionItemKind.Keyword },
    { name: 'use', doc: 'Import declaration', kind: CompletionItemKind.Keyword },
    { name: 'when', doc: 'Pattern matching expression', kind: CompletionItemKind.Keyword },
    { name: 'where', doc: 'Generic constraints', kind: CompletionItemKind.Keyword },
    { name: 'while', doc: 'While loop', kind: CompletionItemKind.Keyword },
    { name: 'with', doc: 'Context expression', kind: CompletionItemKind.Keyword },
    // OOP keywords
    { name: 'class', doc: 'Class declaration', kind: CompletionItemKind.Keyword },
    { name: 'interface', doc: 'Interface declaration', kind: CompletionItemKind.Keyword },
    { name: 'extends', doc: 'Class inheritance', kind: CompletionItemKind.Keyword },
    { name: 'implements', doc: 'Interface implementation', kind: CompletionItemKind.Keyword },
    { name: 'override', doc: 'Override parent method', kind: CompletionItemKind.Keyword },
    { name: 'virtual', doc: 'Virtual method declaration', kind: CompletionItemKind.Keyword },
    { name: 'abstract', doc: 'Abstract class/method', kind: CompletionItemKind.Keyword },
    { name: 'sealed', doc: 'Sealed class (cannot be extended)', kind: CompletionItemKind.Keyword },
    { name: 'base', doc: 'Reference to parent class', kind: CompletionItemKind.Keyword },
    { name: 'new', doc: 'Create new instance', kind: CompletionItemKind.Keyword },
    { name: 'prop', doc: 'Property declaration', kind: CompletionItemKind.Keyword },
    { name: 'private', doc: 'Private access modifier', kind: CompletionItemKind.Keyword },
    { name: 'protected', doc: 'Protected access modifier', kind: CompletionItemKind.Keyword },
    { name: 'static', doc: 'Static member', kind: CompletionItemKind.Keyword },
];

// Sets for quick lookup
const keywordSet = new Set(TML_KEYWORDS.map(k => k.name));

// TML Primitive Types
const TML_TYPES: { name: string; doc: string }[] = [
    { name: 'Bool', doc: 'Boolean type: true or false' },
    { name: 'I8', doc: 'Signed 8-bit integer (-128 to 127)' },
    { name: 'I16', doc: 'Signed 16-bit integer' },
    { name: 'I32', doc: 'Signed 32-bit integer (default integer type)' },
    { name: 'I64', doc: 'Signed 64-bit integer' },
    { name: 'I128', doc: 'Signed 128-bit integer' },
    { name: 'U8', doc: 'Unsigned 8-bit integer (0 to 255)' },
    { name: 'U16', doc: 'Unsigned 16-bit integer' },
    { name: 'U32', doc: 'Unsigned 32-bit integer' },
    { name: 'U64', doc: 'Unsigned 64-bit integer' },
    { name: 'U128', doc: 'Unsigned 128-bit integer' },
    { name: 'F32', doc: 'Single-precision floating point (32-bit)' },
    { name: 'F64', doc: 'Double-precision floating point (64-bit)' },
    { name: 'Char', doc: 'Unicode character' },
    { name: 'String', doc: 'UTF-8 encoded string' },
    { name: 'Str', doc: 'String slice (borrowed string reference)' },
    { name: 'Unit', doc: 'Unit type (empty tuple)' },
    { name: 'Never', doc: 'Never type (function never returns)' },
    { name: 'Ptr', doc: 'Raw pointer type' },
    { name: 'Text', doc: 'Immutable text with SSO optimization' },
];

// TML Collection Types
const TML_COLLECTION_TYPES: { name: string; doc: string }[] = [
    { name: 'List', doc: 'Dynamic array: List[T]' },
    { name: 'Map', doc: 'Hash map: Map[K, V]' },
    { name: 'HashMap', doc: 'Hash map (alias for Map): HashMap[K, V]' },
    { name: 'Set', doc: 'Hash set: Set[T]' },
    { name: 'Vec', doc: 'Vector (alias for List): Vec[T]' },
    { name: 'Buffer', doc: 'Byte buffer for I/O operations' },
];

// TML Wrapper Types
const TML_WRAPPER_TYPES: { name: string; doc: string }[] = [
    { name: 'Maybe', doc: 'Optional value: Just(value) or Nothing' },
    { name: 'Outcome', doc: 'Result type: Ok(value) or Err(error)' },
    { name: 'Result', doc: 'Alias for Outcome: Result[T, E]' },
    { name: 'Option', doc: 'Alias for Maybe: Option[T]' },
    { name: 'Heap', doc: 'Heap-allocated value (like Box)' },
    { name: 'Shared', doc: 'Reference-counted pointer (like Rc)' },
    { name: 'Sync', doc: 'Thread-safe reference-counted pointer (like Arc)' },
];

const allTypes = [...TML_TYPES, ...TML_COLLECTION_TYPES, ...TML_WRAPPER_TYPES];
const typeSet = new Set(allTypes.map(t => t.name));

// TML Enum Variants
const TML_VARIANTS: { name: string; doc: string }[] = [
    { name: 'Just', doc: 'Maybe variant containing a value' },
    { name: 'Nothing', doc: 'Maybe variant representing absence of value' },
    { name: 'Ok', doc: 'Outcome variant representing success' },
    { name: 'Err', doc: 'Outcome variant representing error' },
    { name: 'Less', doc: 'Ordering: left < right' },
    { name: 'Equal', doc: 'Ordering: left == right' },
    { name: 'Greater', doc: 'Ordering: left > right' },
];

const variantSet = new Set(TML_VARIANTS.map(v => v.name));

// TML Builtin Functions
const TML_BUILTINS: { name: string; doc: string; signature: string }[] = [
    { name: 'print', doc: 'Print to stdout without newline', signature: 'func print(msg: Str)' },
    { name: 'println', doc: 'Print to stdout with newline', signature: 'func println(msg: Str)' },
    { name: 'eprint', doc: 'Print to stderr without newline', signature: 'func eprint(msg: Str)' },
    { name: 'eprintln', doc: 'Print to stderr with newline', signature: 'func eprintln(msg: Str)' },
    { name: 'panic', doc: 'Terminate with error message', signature: 'func panic(msg: Str) -> Never' },
    { name: 'assert', doc: 'Assert condition is true', signature: 'func assert(cond: Bool)' },
    { name: 'assert_eq', doc: 'Assert two values are equal', signature: 'func assert_eq[T](left: T, right: T)' },
    { name: 'assert_ne', doc: 'Assert two values are not equal', signature: 'func assert_ne[T](left: T, right: T)' },
    { name: 'dbg', doc: 'Debug print with file and line info', signature: 'func dbg[T](value: T) -> T' },
    { name: 'size_of', doc: 'Get size of type in bytes', signature: 'func size_of[T]() -> U64' },
    { name: 'align_of', doc: 'Get alignment of type in bytes', signature: 'func align_of[T]() -> U64' },
    { name: 'drop', doc: 'Explicitly drop a value', signature: 'func drop[T](value: T)' },
    { name: 'forget', doc: 'Forget a value without running destructor', signature: 'func forget[T](value: T)' },
];

const builtinSet = new Set(TML_BUILTINS.map(f => f.name));

// TML Standard Library Modules for import completion
const TML_MODULES: { name: string; doc: string; members: string[] }[] = [
    { name: 'std', doc: 'Standard library root module', members: ['io', 'fs', 'collections', 'time', 'math', 'net', 'env', 'fmt', 'iter', 'sync', 'thread'] },
    { name: 'std::io', doc: 'Input/output operations', members: ['stdin', 'stdout', 'stderr', 'Read', 'Write', 'BufReader', 'BufWriter'] },
    { name: 'std::fs', doc: 'File system operations', members: ['File', 'read', 'write', 'exists', 'create_dir', 'remove', 'Path'] },
    { name: 'std::collections', doc: 'Collection types', members: ['List', 'Map', 'Set', 'HashMap', 'HashSet', 'VecDeque', 'BinaryHeap'] },
    { name: 'std::time', doc: 'Time and duration utilities', members: ['Instant', 'Duration', 'now', 'sleep'] },
    { name: 'std::math', doc: 'Mathematical functions', members: ['abs', 'min', 'max', 'sqrt', 'pow', 'sin', 'cos', 'tan', 'PI', 'E'] },
    { name: 'std::net', doc: 'Networking utilities', members: ['TcpStream', 'TcpListener', 'UdpSocket', 'IpAddr'] },
    { name: 'std::env', doc: 'Environment variables and arguments', members: ['var', 'args', 'current_dir', 'set_var'] },
    { name: 'std::fmt', doc: 'Formatting utilities', members: ['format', 'Display', 'Debug', 'write'] },
    { name: 'std::iter', doc: 'Iterator utilities', members: ['Iterator', 'IntoIterator', 'map', 'filter', 'fold', 'collect'] },
    { name: 'std::sync', doc: 'Synchronization primitives', members: ['Mutex', 'RwLock', 'Arc', 'Barrier', 'Condvar'] },
    { name: 'std::thread', doc: 'Threading utilities', members: ['spawn', 'current', 'sleep', 'yield_now', 'JoinHandle'] },
    { name: 'core', doc: 'Core language primitives', members: ['mem', 'ptr', 'slice', 'ops', 'cmp', 'convert', 'default'] },
    { name: 'core::mem', doc: 'Memory manipulation', members: ['size_of', 'align_of', 'drop', 'forget', 'swap', 'replace'] },
    { name: 'core::ptr', doc: 'Raw pointer operations', members: ['null', 'null_mut', 'read', 'write', 'copy'] },
    { name: 'test', doc: 'Testing framework', members: ['assert', 'assert_eq', 'assert_ne', 'should_panic', 'ignore'] },
];

// TML Effects and Capabilities
const TML_EFFECTS: { name: string; doc: string }[] = [
    { name: 'pure', doc: 'Function has no side effects - only depends on inputs' },
    { name: 'io', doc: 'Function may perform I/O operations (read/write files, network, console)' },
    { name: 'throws', doc: 'Function may throw an exception or panic' },
    { name: 'async', doc: 'Function is asynchronous and returns a Future' },
    { name: 'unsafe', doc: 'Function contains unsafe/lowlevel code' },
    { name: 'diverges', doc: 'Function may not return (infinite loop or panic)' },
    { name: 'alloc', doc: 'Function may allocate memory' },
    { name: 'nondet', doc: 'Function may have non-deterministic behavior (random, time-based)' },
];

const TML_CAPABILITIES: { name: string; doc: string }[] = [
    { name: 'Read', doc: 'Capability to read from a resource' },
    { name: 'Write', doc: 'Capability to write to a resource' },
    { name: 'Exec', doc: 'Capability to execute code or spawn processes' },
    { name: 'Net', doc: 'Capability to access the network' },
    { name: 'Fs', doc: 'Capability to access the file system' },
    { name: 'Env', doc: 'Capability to access environment variables' },
    { name: 'Time', doc: 'Capability to access system time' },
    { name: 'Random', doc: 'Capability to generate random numbers' },
];

// Contract keywords
const TML_CONTRACTS: { name: string; doc: string; syntax: string }[] = [
    { name: 'requires', doc: 'Precondition that must be true when function is called', syntax: 'requires condition' },
    { name: 'ensures', doc: 'Postcondition that will be true when function returns', syntax: 'ensures condition' },
    { name: 'invariant', doc: 'Condition that must remain true throughout execution', syntax: 'invariant condition' },
    { name: 'assert', doc: 'Runtime assertion that condition is true', syntax: 'assert(condition)' },
    { name: 'assume', doc: 'Assume condition is true (for optimization)', syntax: 'assume(condition)' },
];

const effectSet = new Set(TML_EFFECTS.map(e => e.name));
const capabilitySet = new Set(TML_CAPABILITIES.map(c => c.name));
const contractSet = new Set(TML_CONTRACTS.map(c => c.name));

// Snippets for common patterns
const TML_SNIPPETS: { label: string; insertText: string; doc: string }[] = [
    {
        label: 'func',
        insertText: 'func ${1:name}(${2:params}) -> ${3:ReturnType} {\n\t$0\n}',
        doc: 'Function declaration'
    },
    {
        label: 'if',
        insertText: 'if ${1:condition} {\n\t$0\n}',
        doc: 'If statement'
    },
    {
        label: 'ifelse',
        insertText: 'if ${1:condition} {\n\t$2\n} else {\n\t$0\n}',
        doc: 'If-else statement'
    },
    {
        label: 'for',
        insertText: 'for ${1:item} in ${2:collection} {\n\t$0\n}',
        doc: 'For loop'
    },
    {
        label: 'while',
        insertText: 'while ${1:condition} {\n\t$0\n}',
        doc: 'While loop'
    },
    {
        label: 'loop',
        insertText: 'loop {\n\t$0\n\tbreak\n}',
        doc: 'Infinite loop'
    },
    {
        label: 'when',
        insertText: 'when ${1:value} {\n\t${2:pattern} => $0,\n}',
        doc: 'Pattern matching'
    },
    {
        label: 'type',
        insertText: 'type ${1:Name} {\n\t${2:field}: ${3:Type},\n}',
        doc: 'Type (struct) declaration'
    },
    {
        label: 'impl',
        insertText: 'impl ${1:Type} {\n\tfunc ${2:method}(this) -> ${3:ReturnType} {\n\t\t$0\n\t}\n}',
        doc: 'Implementation block'
    },
    {
        label: 'behavior',
        insertText: 'behavior ${1:Name} {\n\tfunc ${2:method}(this) -> ${3:ReturnType}\n}',
        doc: 'Behavior (trait) declaration'
    },
    {
        label: 'test',
        insertText: '@test\nfunc test_${1:name}() {\n\t$0\n}',
        doc: 'Test function'
    },
    {
        label: 'class',
        insertText: 'class ${1:Name} {\n\t${2:field}: ${3:Type}\n\n\tfunc new(${4:params}) -> This {\n\t\treturn This { $0 }\n\t}\n}',
        doc: 'Class declaration'
    },
    {
        label: 'interface',
        insertText: 'interface ${1:Name} {\n\tfunc ${2:method}(this) -> ${3:ReturnType}\n}',
        doc: 'Interface declaration'
    },
];

// OOP-specific snippets for class bodies
const TML_OOP_SNIPPETS: { label: string; insertText: string; doc: string; sortText?: string }[] = [
    // Override patterns
    {
        label: 'override func',
        insertText: 'override func ${1:method}(${2:this}) -> ${3:ReturnType} {\n\t$0\n}',
        doc: 'Override a virtual method from the parent class',
        sortText: '0override'
    },
    {
        label: 'override func (with base call)',
        insertText: 'override func ${1:method}(${2:this}) -> ${3:ReturnType} {\n\tbase.${1:method}()\n\t$0\n}',
        doc: 'Override a method and call the base implementation',
        sortText: '0override_base'
    },
    // Virtual method patterns
    {
        label: 'virtual func',
        insertText: 'virtual func ${1:method}(${2:this}) -> ${3:ReturnType} {\n\t$0\n}',
        doc: 'Declare a virtual method that can be overridden',
        sortText: '0virtual'
    },
    {
        label: 'abstract func',
        insertText: 'abstract func ${1:method}(${2:this}) -> ${3:ReturnType}',
        doc: 'Declare an abstract method (no body)',
        sortText: '0abstract'
    },
    // Property patterns
    {
        label: 'prop (auto)',
        insertText: 'prop ${1:name}: ${2:Type}',
        doc: 'Auto-implemented property with getter and setter',
        sortText: '0prop'
    },
    {
        label: 'prop (get)',
        insertText: 'prop ${1:name}: ${2:Type} { get }',
        doc: 'Read-only property',
        sortText: '0prop_get'
    },
    {
        label: 'prop (get/set)',
        insertText: 'prop ${1:name}: ${2:Type} {\n\tget { return this._${1:name} }\n\tset { this._${1:name} = value }\n}',
        doc: 'Property with custom getter and setter',
        sortText: '0prop_getset'
    },
    // Class patterns
    {
        label: 'class (extends)',
        insertText: 'class ${1:Name} extends ${2:Base} {\n\tfunc new(${3:params}) -> This {\n\t\treturn This {\n\t\t\tbase: ${2:Base}::new($4),\n\t\t\t$0\n\t\t}\n\t}\n}',
        doc: 'Class that extends a base class',
        sortText: '0class_extends'
    },
    {
        label: 'class (implements)',
        insertText: 'class ${1:Name} implements ${2:Interface} {\n\t$0\n}',
        doc: 'Class that implements an interface',
        sortText: '0class_implements'
    },
    {
        label: 'class (extends + implements)',
        insertText: 'class ${1:Name} extends ${2:Base} implements ${3:Interface} {\n\tfunc new(${4:params}) -> This {\n\t\treturn This {\n\t\t\tbase: ${2:Base}::new($5),\n\t\t\t$0\n\t\t}\n\t}\n}',
        doc: 'Class that extends and implements',
        sortText: '0class_ext_impl'
    },
    {
        label: 'abstract class',
        insertText: 'abstract class ${1:Name} {\n\tabstract func ${2:method}(this) -> ${3:ReturnType}\n\n\tvirtual func ${4:concreteMethod}(this) -> ${5:ReturnType} {\n\t\t$0\n\t}\n}',
        doc: 'Abstract class with abstract and virtual methods',
        sortText: '0abstract_class'
    },
    {
        label: 'sealed class',
        insertText: 'sealed class ${1:Name} {\n\t$0\n}',
        doc: 'Sealed class that cannot be extended',
        sortText: '0sealed_class'
    },
    // Constructor patterns
    {
        label: 'func new',
        insertText: 'func new(${1:params}) -> This {\n\treturn This { $0 }\n}',
        doc: 'Constructor method',
        sortText: '0new'
    },
    {
        label: 'func new (with base)',
        insertText: 'func new(${1:params}) -> This {\n\treturn This {\n\t\tbase: ${2:Base}::new($3),\n\t\t$0\n\t}\n}',
        doc: 'Constructor that initializes base class',
        sortText: '0new_base'
    },
    // Static member patterns
    {
        label: 'static func',
        insertText: 'static func ${1:name}(${2:params}) -> ${3:ReturnType} {\n\t$0\n}',
        doc: 'Static method',
        sortText: '0static_func'
    },
    {
        label: 'static let',
        insertText: 'static let ${1:NAME}: ${2:Type} = ${3:value}',
        doc: 'Static constant',
        sortText: '0static_let'
    },
    // Interface pattern
    {
        label: 'interface (full)',
        insertText: 'interface ${1:Name} {\n\tfunc ${2:method}(this) -> ${3:ReturnType}\n\n\tprop ${4:property}: ${5:Type} { get }\n}',
        doc: 'Interface with methods and properties',
        sortText: '0interface_full'
    },
];

// ============================================================================
// Class/Interface Tracking for IDE Navigation
// ============================================================================

interface ClassInfo {
    name: string;
    uri: string;
    range: { start: { line: number; character: number }; end: { line: number; character: number } };
    baseClass: string | null;
    interfaces: string[];
    isAbstract: boolean;
    isSealed: boolean;
    methods: MethodInfo[];
}

interface InterfaceInfo {
    name: string;
    uri: string;
    range: { start: { line: number; character: number }; end: { line: number; character: number } };
    extends: string[];
    methods: MethodInfo[];
}

interface MethodInfo {
    name: string;
    line: number;
    isVirtual: boolean;
    isOverride: boolean;
    isAbstract: boolean;
    isStatic: boolean;
}

// Global symbol index - maps symbol names to their definitions
const classIndex: Map<string, ClassInfo> = new Map();
const interfaceIndex: Map<string, InterfaceInfo> = new Map();
// Map from uri to symbols defined in that document
const documentSymbols: Map<string, { classes: string[]; interfaces: string[] }> = new Map();

// Parse a document and extract class/interface definitions
function indexDocument(document: TextDocument): void {
    const uri = document.uri;
    const text = document.getText();
    const lines = text.split('\n');

    // Clear old symbols from this document
    const oldSymbols = documentSymbols.get(uri);
    if (oldSymbols) {
        for (const cls of oldSymbols.classes) {
            classIndex.delete(cls);
        }
        for (const iface of oldSymbols.interfaces) {
            interfaceIndex.delete(iface);
        }
    }

    const newClasses: string[] = [];
    const newInterfaces: string[] = [];

    for (let lineNum = 0; lineNum < lines.length; lineNum++) {
        const line = lines[lineNum];

        // Match class declaration
        const classMatch = line.match(/^\s*(abstract\s+|sealed\s+)?class\s+(\w+)(?:\s*\[[\w,\s]+\])?(?:\s+extends\s+(\w+))?(?:\s+implements\s+([\w,\s]+))?/);
        if (classMatch) {
            const isAbstract = !!classMatch[1]?.includes('abstract');
            const isSealed = !!classMatch[1]?.includes('sealed');
            const className = classMatch[2];
            const baseClass = classMatch[3] || null;
            const interfaces = classMatch[4] ? classMatch[4].split(',').map(s => s.trim()) : [];

            // Find the end of the class (matching brace)
            let braceDepth = 0;
            let endLine = lineNum;
            let foundOpen = false;
            for (let i = lineNum; i < lines.length; i++) {
                for (const ch of lines[i]) {
                    if (ch === '{') {
                        braceDepth++;
                        foundOpen = true;
                    } else if (ch === '}') {
                        braceDepth--;
                        if (foundOpen && braceDepth === 0) {
                            endLine = i;
                            break;
                        }
                    }
                }
                if (foundOpen && braceDepth === 0) break;
            }

            // Extract methods
            const methods: MethodInfo[] = [];
            for (let i = lineNum + 1; i <= endLine; i++) {
                const methodMatch = lines[i].match(/^\s*(virtual\s+|override\s+|abstract\s+|static\s+)*func\s+(\w+)/);
                if (methodMatch) {
                    const modifiers = methodMatch[1] || '';
                    methods.push({
                        name: methodMatch[2],
                        line: i,
                        isVirtual: modifiers.includes('virtual'),
                        isOverride: modifiers.includes('override'),
                        isAbstract: modifiers.includes('abstract'),
                        isStatic: modifiers.includes('static')
                    });
                }
            }

            classIndex.set(className, {
                name: className,
                uri,
                range: {
                    start: { line: lineNum, character: 0 },
                    end: { line: endLine, character: lines[endLine]?.length || 0 }
                },
                baseClass,
                interfaces,
                isAbstract,
                isSealed,
                methods
            });
            newClasses.push(className);
        }

        // Match interface declaration
        const interfaceMatch = line.match(/^\s*interface\s+(\w+)(?:\s*\[[\w,\s]+\])?(?:\s+extends\s+([\w,\s]+))?/);
        if (interfaceMatch) {
            const interfaceName = interfaceMatch[1];
            const extendsInterfaces = interfaceMatch[2] ? interfaceMatch[2].split(',').map(s => s.trim()) : [];

            // Find the end of the interface
            let braceDepth = 0;
            let endLine = lineNum;
            let foundOpen = false;
            for (let i = lineNum; i < lines.length; i++) {
                for (const ch of lines[i]) {
                    if (ch === '{') {
                        braceDepth++;
                        foundOpen = true;
                    } else if (ch === '}') {
                        braceDepth--;
                        if (foundOpen && braceDepth === 0) {
                            endLine = i;
                            break;
                        }
                    }
                }
                if (foundOpen && braceDepth === 0) break;
            }

            // Extract methods
            const methods: MethodInfo[] = [];
            for (let i = lineNum + 1; i <= endLine; i++) {
                const methodMatch = lines[i].match(/^\s*func\s+(\w+)/);
                if (methodMatch) {
                    methods.push({
                        name: methodMatch[1],
                        line: i,
                        isVirtual: false,
                        isOverride: false,
                        isAbstract: true, // Interface methods are implicitly abstract
                        isStatic: false
                    });
                }
            }

            interfaceIndex.set(interfaceName, {
                name: interfaceName,
                uri,
                range: {
                    start: { line: lineNum, character: 0 },
                    end: { line: endLine, character: lines[endLine]?.length || 0 }
                },
                extends: extendsInterfaces,
                methods
            });
            newInterfaces.push(interfaceName);
        }
    }

    documentSymbols.set(uri, { classes: newClasses, interfaces: newInterfaces });
}

// Find all classes that extend a given base class
function findSubclasses(baseName: string): ClassInfo[] {
    const result: ClassInfo[] = [];
    for (const [, cls] of classIndex) {
        if (cls.baseClass === baseName) {
            result.push(cls);
        }
    }
    return result;
}

// Find all classes that implement a given interface
function findImplementations(interfaceName: string): ClassInfo[] {
    const result: ClassInfo[] = [];
    for (const [, cls] of classIndex) {
        if (cls.interfaces.includes(interfaceName)) {
            result.push(cls);
        }
    }
    return result;
}

// Get full inheritance chain for a class (going up)
function getInheritanceChain(className: string): string[] {
    const chain: string[] = [];
    let current = classIndex.get(className);
    while (current && current.baseClass) {
        chain.push(current.baseClass);
        current = classIndex.get(current.baseClass);
    }
    return chain;
}

// Get all transitive subclasses
function getAllSubclasses(baseName: string): ClassInfo[] {
    const result: ClassInfo[] = [];
    const directSubs = findSubclasses(baseName);
    for (const sub of directSubs) {
        result.push(sub);
        result.push(...getAllSubclasses(sub.name));
    }
    return result;
}

// Common base class methods for base. completion
const BASE_COMPLETIONS: { name: string; signature: string; doc: string }[] = [
    { name: 'new', signature: 'func new(...) -> Base', doc: 'Call base class constructor' },
    { name: 'to_string', signature: 'func to_string(this) -> String', doc: 'Base class string representation' },
    { name: 'equals', signature: 'func equals(this, other: ref This) -> Bool', doc: 'Base class equality check' },
    { name: 'hash', signature: 'func hash(this) -> U64', doc: 'Base class hash value' },
    { name: 'clone', signature: 'func clone(this) -> This', doc: 'Base class clone method' },
    { name: 'drop', signature: 'func drop(mut this)', doc: 'Base class destructor' },
];

// ============================================================================
// Diagnostic Validation
// ============================================================================

// Server settings interface
interface TMLSettings {
    maxNumberOfProblems: number;
    compilerPath: string;
    buildArgs: string[];
    enableDiagnostics: boolean;
}

// Default settings
const defaultSettings: TMLSettings = {
    maxNumberOfProblems: 100,
    compilerPath: 'tml',
    buildArgs: [],
    enableDiagnostics: true
};

let globalSettings: TMLSettings = defaultSettings;

// Cache of document settings
const documentSettings: Map<string, Promise<TMLSettings>> = new Map();

// Flag to track if we have configuration capability
let hasConfigurationCapability = false;

// Diagnostic JSON output interface from compiler
interface CompilerDiagnostic {
    severity: 'error' | 'warning' | 'note' | 'help';
    code: string;
    message: string;
    span: {
        file: string;
        start: { line: number; column: number };
        end: { line: number; column: number };
    };
    labels: Array<{
        message: string;
        is_primary: boolean;
        span: {
            file: string;
            start: { line: number; column: number };
            end: { line: number; column: number };
        };
    }>;
    notes: string[];
    help: string[];
    fixes: Array<{
        description: string;
        replacement: string;
        span: {
            file: string;
            start: { line: number; column: number };
            end: { line: number; column: number };
        };
    }>;
}

// Convert compiler severity to LSP severity
function toLspSeverity(severity: string): DiagnosticSeverity {
    switch (severity) {
        case 'error': return DiagnosticSeverity.Error;
        case 'warning': return DiagnosticSeverity.Warning;
        case 'note': return DiagnosticSeverity.Information;
        case 'help': return DiagnosticSeverity.Hint;
        default: return DiagnosticSeverity.Error;
    }
}

// Validate a TML document using the compiler
async function validateTextDocument(textDocument: TextDocument): Promise<void> {
    // Get document settings
    const settings = await getDocumentSettings(textDocument.uri);

    if (!settings.enableDiagnostics) {
        // Clear diagnostics if disabled
        connection.sendDiagnostics({ uri: textDocument.uri, diagnostics: [] });
        return;
    }

    const diagnostics: Diagnostic[] = [];
    const text = textDocument.getText();

    // Write content to a temporary file for compiler validation
    const tempDir = os.tmpdir();
    const tempFile = path.join(tempDir, `tml_check_${Date.now()}.tml`);

    try {
        // Write the document content to temp file
        fs.writeFileSync(tempFile, text, 'utf8');

        // Run the TML compiler with --error-format=json
        const compilerPath = settings.compilerPath || 'tml';
        const args = ['check', tempFile, '--error-format=json'];

        try {
            await execAsync(`"${compilerPath}" ${args.join(' ')}`, {
                timeout: 10000, // 10 second timeout
                maxBuffer: 1024 * 1024 // 1MB buffer
            });
            // If command succeeds, no errors
        } catch (error: unknown) {
            // Parse errors from stderr
            const execError = error as { stderr?: string; stdout?: string };
            const output = execError.stderr || execError.stdout || '';

            // Parse JSON diagnostics (one per line)
            const lines = output.split('\n').filter(line => line.trim().startsWith('{'));

            for (const line of lines) {
                try {
                    const diag = JSON.parse(line) as CompilerDiagnostic;

                    // Convert to LSP diagnostic
                    const lspDiag: Diagnostic = {
                        severity: toLspSeverity(diag.severity),
                        range: {
                            start: {
                                line: Math.max(0, diag.span.start.line - 1),
                                character: Math.max(0, diag.span.start.column - 1)
                            },
                            end: {
                                line: Math.max(0, diag.span.end.line - 1),
                                character: Math.max(0, diag.span.end.column - 1)
                            }
                        },
                        message: diag.message,
                        source: 'tml'
                    };

                    // Add error code if present
                    if (diag.code) {
                        lspDiag.code = diag.code;
                    }

                    // Add notes and help as related information
                    if (diag.notes.length > 0 || diag.help.length > 0) {
                        lspDiag.message += '\n' + [...diag.notes, ...diag.help].join('\n');
                    }

                    diagnostics.push(lspDiag);

                    // Limit number of problems
                    if (diagnostics.length >= settings.maxNumberOfProblems) {
                        break;
                    }
                } catch {
                    // Skip invalid JSON lines
                }
            }

            // If no JSON output, try to parse simple error format
            if (diagnostics.length === 0 && output.trim()) {
                // Try simple format: file:line:column: error: message
                const simpleMatch = output.match(/^.+:(\d+):(\d+):\s*(error|warning):\s*(.+)$/m);
                if (simpleMatch) {
                    diagnostics.push({
                        severity: simpleMatch[3] === 'error' ? DiagnosticSeverity.Error : DiagnosticSeverity.Warning,
                        range: {
                            start: { line: parseInt(simpleMatch[1]) - 1, character: parseInt(simpleMatch[2]) - 1 },
                            end: { line: parseInt(simpleMatch[1]) - 1, character: parseInt(simpleMatch[2]) }
                        },
                        message: simpleMatch[4],
                        source: 'tml'
                    });
                }
            }
        }
    } catch (err) {
        // File system error - log but don't report to user
        connection.console.error(`Error validating document: ${err}`);
    } finally {
        // Clean up temp file
        try {
            if (fs.existsSync(tempFile)) {
                fs.unlinkSync(tempFile);
            }
        } catch {
            // Ignore cleanup errors
        }
    }

    // Send diagnostics to client
    connection.sendDiagnostics({ uri: textDocument.uri, diagnostics });
}

// Get document settings
function getDocumentSettings(resource: string): Promise<TMLSettings> {
    if (!hasConfigurationCapability) {
        return Promise.resolve(globalSettings);
    }
    let result = documentSettings.get(resource);
    if (!result) {
        result = connection.workspace.getConfiguration({
            scopeUri: resource,
            section: 'tml'
        }) as Promise<TMLSettings>;
        documentSettings.set(resource, result);
    }
    return result;
}

// Debounce validation to avoid too many compiler calls
// eslint-disable-next-line @typescript-eslint/no-explicit-any
const validationQueue: Map<string, any> = new Map();
const VALIDATION_DELAY = 500; // ms

function queueValidation(document: TextDocument): void {
    const uri = document.uri;

    // Clear existing timeout
    const existing = validationQueue.get(uri);
    if (existing) {
        clearTimeoutNode(existing);
    }

    // Queue new validation
    const timeout = setTimeoutNode(() => {
        validationQueue.delete(uri);
        validateTextDocument(document);
    }, VALIDATION_DELAY);

    validationQueue.set(uri, timeout);
}

connection.onInitialize((params: InitializeParams): InitializeResult => {
    const capabilities = params.capabilities;

    // Check if client supports configuration
    hasConfigurationCapability = !!(
        capabilities.workspace && !!capabilities.workspace.configuration
    );
    return {
        capabilities: {
            textDocumentSync: TextDocumentSyncKind.Incremental,
            completionProvider: {
                resolveProvider: true,
                triggerCharacters: ['.', ':', '@']
            },
            hoverProvider: true,
            semanticTokensProvider: {
                legend,
                full: true,
                range: false
            },
            definitionProvider: true,
            implementationProvider: true,
            typeHierarchyProvider: true
        }
    };
});

// Provide semantic tokens
connection.languages.semanticTokens.on((params: SemanticTokensParams): SemanticTokens => {
    const document = documents.get(params.textDocument.uri);
    if (!document) {
        return { data: [] };
    }

    const builder = new SemanticTokensBuilder();
    const text = document.getText();
    const lines = text.split('\n');

    for (let lineNum = 0; lineNum < lines.length; lineNum++) {
        const line = lines[lineNum];

        // Find decorators (@test, @bench, etc.)
        const decoratorRegex = /@([a-zA-Z_][a-zA-Z0-9_]*)/g;
        let match;
        while ((match = decoratorRegex.exec(line)) !== null) {
            builder.push(
                lineNum,
                match.index,
                match[0].length,
                tokenTypes.indexOf('decorator'),
                0
            );
        }

        // Find words and classify them
        const wordRegex = /\b([a-zA-Z_][a-zA-Z0-9_]*)\b/g;
        while ((match = wordRegex.exec(line)) !== null) {
            const word = match[1];
            const col = match.index;

            // Check if it's after 'func' keyword - it's a function declaration
            const beforeWord = line.substring(0, col).trim();
            if (beforeWord.endsWith('func')) {
                builder.push(
                    lineNum,
                    col,
                    word.length,
                    tokenTypes.indexOf('function'),
                    tokenModifiers.indexOf('declaration')
                );
                continue;
            }

            // Check if it's after 'type' or 'class' keyword - it's a type declaration
            if (beforeWord.endsWith('type') || beforeWord.endsWith('class') || beforeWord.endsWith('interface') || beforeWord.endsWith('behavior')) {
                builder.push(
                    lineNum,
                    col,
                    word.length,
                    tokenTypes.indexOf('class'),
                    tokenModifiers.indexOf('declaration')
                );
                continue;
            }

            // Check if it's a builtin type
            if (typeSet.has(word)) {
                builder.push(
                    lineNum,
                    col,
                    word.length,
                    tokenTypes.indexOf('type'),
                    tokenModifiers.indexOf('defaultLibrary')
                );
                continue;
            }

            // Check if it's an enum variant
            if (variantSet.has(word)) {
                builder.push(
                    lineNum,
                    col,
                    word.length,
                    tokenTypes.indexOf('enumMember'),
                    0
                );
                continue;
            }

            // Check if it's a builtin function
            if (builtinSet.has(word)) {
                builder.push(
                    lineNum,
                    col,
                    word.length,
                    tokenTypes.indexOf('function'),
                    tokenModifiers.indexOf('defaultLibrary')
                );
                continue;
            }

            // Check if it's followed by '(' - it's a function call
            const afterWord = line.substring(col + word.length);
            if (afterWord.match(/^\s*\(/)) {
                builder.push(
                    lineNum,
                    col,
                    word.length,
                    tokenTypes.indexOf('function'),
                    0
                );
                continue;
            }

            // Check if it's a type (PascalCase starting with uppercase)
            if (/^[A-Z][a-zA-Z0-9]*$/.test(word) && !keywordSet.has(word)) {
                builder.push(
                    lineNum,
                    col,
                    word.length,
                    tokenTypes.indexOf('type'),
                    0
                );
                continue;
            }

            // Check if it's an effect (after 'with' keyword)
            if (effectSet.has(word)) {
                builder.push(
                    lineNum,
                    col,
                    word.length,
                    tokenTypes.indexOf('modifier'),
                    0
                );
                continue;
            }

            // Check if it's a capability
            if (capabilitySet.has(word)) {
                builder.push(
                    lineNum,
                    col,
                    word.length,
                    tokenTypes.indexOf('interface'),
                    0
                );
                continue;
            }

            // Check if it's a contract keyword
            if (contractSet.has(word)) {
                builder.push(
                    lineNum,
                    col,
                    word.length,
                    tokenTypes.indexOf('keyword'),
                    0
                );
                continue;
            }
        }

        // Highlight effect annotations (with effect1, effect2)
        const withMatch = line.match(/\bwith\s+([\w,\s]+)(?:\s*\{|$)/);
        if (withMatch) {
            const effects = withMatch[1].split(',').map((e: string) => e.trim());
            for (const eff of effects) {
                if (effectSet.has(eff)) {
                    const effIndex = line.indexOf(eff, line.indexOf('with'));
                    if (effIndex >= 0) {
                        builder.push(
                            lineNum,
                            effIndex,
                            eff.length,
                            tokenTypes.indexOf('modifier'),
                            0
                        );
                    }
                }
            }
        }
    }

    return builder.build();
});

// Helper to detect import context
interface ImportContext {
    afterUse: boolean;
    currentPath: string;  // e.g., "std::" or "std::io::"
}

function getImportContext(document: TextDocument, position: { line: number; character: number }): ImportContext {
    const lines = document.getText().split('\n');
    const currentLine = lines[position.line] || '';
    const lineUpToCursor = currentLine.substring(0, position.character);

    // Check if we're in a use statement
    const useMatch = lineUpToCursor.match(/\buse\s+([\w:.]*?)$/);
    if (useMatch) {
        return { afterUse: true, currentPath: useMatch[1] };
    }

    return { afterUse: false, currentPath: '' };
}

// Helper to detect OOP context from document
interface OOPContext {
    inClass: boolean;
    inInterface: boolean;
    className: string | null;
    baseClass: string | null;
    interfaces: string[];
    afterBase: boolean;  // true if cursor is after "base."
}

function getOOPContext(document: TextDocument, position: { line: number; character: number }): OOPContext {
    const text = document.getText();
    const lines = text.split('\n');
    const currentLine = lines[position.line] || '';

    // Check if we're after "base."
    const lineUpToCursor = currentLine.substring(0, position.character);
    const afterBase = /\bbase\.\s*$/.test(lineUpToCursor);

    // Find the enclosing class/interface
    let inClass = false;
    let inInterface = false;
    let className: string | null = null;
    let baseClass: string | null = null;
    let interfaces: string[] = [];

    let braceDepth = 0;
    let foundDecl = false;

    // Scan backwards from current position to find class/interface declaration
    for (let lineNum = position.line; lineNum >= 0; lineNum--) {
        const line = lines[lineNum];

        // Count braces (simplified - doesn't handle strings/comments perfectly)
        for (let i = line.length - 1; i >= 0; i--) {
            if (lineNum === position.line && i >= position.character) {continue;}
            if (line[i] === '}') {braceDepth++;}
            else if (line[i] === '{') {braceDepth--;}
        }

        // Look for class/interface declaration when we're at the opening brace level
        if (braceDepth < 0 && !foundDecl) {
            // Check for class declaration
            const classMatch = line.match(/\b(abstract\s+|sealed\s+)?class\s+(\w+)(?:\s+extends\s+(\w+))?(?:\s+implements\s+([\w,\s]+))?/);
            if (classMatch) {
                inClass = true;
                className = classMatch[2];
                baseClass = classMatch[3] || null;
                if (classMatch[4]) {
                    interfaces = classMatch[4].split(',').map(s => s.trim());
                }
                foundDecl = true;
                break;
            }

            // Check for interface declaration
            const interfaceMatch = line.match(/\binterface\s+(\w+)/);
            if (interfaceMatch) {
                inInterface = true;
                className = interfaceMatch[1];
                foundDecl = true;
                break;
            }
        }
    }

    return { inClass, inInterface, className, baseClass, interfaces, afterBase };
}

// Provide completion items
connection.onCompletion((textDocumentPosition: TextDocumentPositionParams): CompletionItem[] => {
    const completions: CompletionItem[] = [];

    const document = documents.get(textDocumentPosition.textDocument.uri);

    // Check for import context first
    const importContext = document
        ? getImportContext(document, textDocumentPosition.position)
        : { afterUse: false, currentPath: '' };

    // Handle import completions
    if (importContext.afterUse) {
        const path = importContext.currentPath;

        // If path is empty or just started, suggest top-level modules
        if (path === '' || !path.includes('::')) {
            for (const mod of TML_MODULES.filter(m => !m.name.includes('::'))) {
                completions.push({
                    label: mod.name,
                    kind: CompletionItemKind.Module,
                    detail: 'module',
                    documentation: mod.doc,
                    insertText: mod.name + '::',
                    command: { title: 'Trigger Suggest', command: 'editor.action.triggerSuggest' }
                });
            }
        } else {
            // Find matching module and suggest members or submodules
            const basePath = path.replace(/::$/, '');
            const matchingModule = TML_MODULES.find(m => m.name === basePath);

            if (matchingModule) {
                // Add members
                for (const member of matchingModule.members) {
                    // Check if it's a submodule
                    const submodulePath = `${basePath}::${member}`;
                    const isSubmodule = TML_MODULES.some(m => m.name === submodulePath);

                    completions.push({
                        label: member,
                        kind: isSubmodule ? CompletionItemKind.Module : CompletionItemKind.Reference,
                        detail: isSubmodule ? 'submodule' : 'export',
                        insertText: isSubmodule ? member + '::' : member,
                        command: isSubmodule ? { title: 'Trigger Suggest', command: 'editor.action.triggerSuggest' } : undefined
                    });
                }

                // Add wildcard import option
                completions.push({
                    label: '*',
                    kind: CompletionItemKind.Reference,
                    detail: 'import all',
                    documentation: `Import all public items from ${basePath}`
                });
            }
        }

        return completions;
    }

    const oopContext = document
        ? getOOPContext(document, textDocumentPosition.position)
        : { inClass: false, inInterface: false, className: null, baseClass: null, interfaces: [], afterBase: false };

    // Handle "base." completions
    if (oopContext.afterBase && oopContext.baseClass) {
        // Add base class method completions
        for (const method of BASE_COMPLETIONS) {
            completions.push({
                label: method.name,
                kind: CompletionItemKind.Method,
                detail: method.signature,
                documentation: `${method.doc}\n\nFrom base class: ${oopContext.baseClass}`,
                sortText: '0' + method.name
            });
        }
        // Return only base completions when after "base."
        return completions;
    }

    // Add keywords
    for (const kw of TML_KEYWORDS) {
        completions.push({
            label: kw.name,
            kind: kw.kind,
            detail: 'keyword',
            documentation: kw.doc
        });
    }

    // Add types
    for (const t of TML_TYPES) {
        completions.push({
            label: t.name,
            kind: CompletionItemKind.Class,
            detail: 'primitive type',
            documentation: t.doc
        });
    }

    for (const t of TML_COLLECTION_TYPES) {
        completions.push({
            label: t.name,
            kind: CompletionItemKind.Class,
            detail: 'collection type',
            documentation: t.doc
        });
    }

    for (const t of TML_WRAPPER_TYPES) {
        completions.push({
            label: t.name,
            kind: CompletionItemKind.Class,
            detail: 'wrapper type',
            documentation: t.doc
        });
    }

    // Add variants
    for (const v of TML_VARIANTS) {
        completions.push({
            label: v.name,
            kind: CompletionItemKind.EnumMember,
            detail: 'variant',
            documentation: v.doc
        });
    }

    // Add builtin functions
    for (const fn of TML_BUILTINS) {
        completions.push({
            label: fn.name,
            kind: CompletionItemKind.Function,
            detail: fn.signature,
            documentation: fn.doc
        });
    }

    // Add snippets
    for (const snippet of TML_SNIPPETS) {
        completions.push({
            label: snippet.label,
            kind: CompletionItemKind.Snippet,
            insertText: snippet.insertText,
            insertTextFormat: 2, // Snippet format
            detail: 'snippet',
            documentation: snippet.doc
        });
    }

    // Add effects
    for (const effect of TML_EFFECTS) {
        completions.push({
            label: effect.name,
            kind: CompletionItemKind.TypeParameter,
            detail: 'effect',
            documentation: effect.doc
        });
    }

    // Add capabilities
    for (const cap of TML_CAPABILITIES) {
        completions.push({
            label: cap.name,
            kind: CompletionItemKind.Interface,
            detail: 'capability',
            documentation: cap.doc
        });
    }

    // Add contracts
    for (const contract of TML_CONTRACTS) {
        completions.push({
            label: contract.name,
            kind: CompletionItemKind.Keyword,
            detail: 'contract',
            documentation: `${contract.doc}\n\nSyntax: \`${contract.syntax}\``
        });
    }

    // Add import snippets
    completions.push({
        label: 'use',
        kind: CompletionItemKind.Snippet,
        insertText: 'use ${1:std::${2:io}}',
        insertTextFormat: 2,
        detail: 'import snippet',
        documentation: 'Import a module'
    });

    completions.push({
        label: 'use (wildcard)',
        kind: CompletionItemKind.Snippet,
        insertText: 'use ${1:std::${2:io}}::*',
        insertTextFormat: 2,
        detail: 'import snippet',
        documentation: 'Import all items from a module'
    });

    completions.push({
        label: 'use (alias)',
        kind: CompletionItemKind.Snippet,
        insertText: 'use ${1:std::collections::HashMap} as ${2:Map}',
        insertTextFormat: 2,
        detail: 'import snippet',
        documentation: 'Import with alias'
    });

    // Add OOP snippets when in class/interface context
    if (oopContext.inClass || oopContext.inInterface) {
        for (const snippet of TML_OOP_SNIPPETS) {
            // Filter snippets based on context
            const isOverride = snippet.label.includes('override');
            const isVirtual = snippet.label.includes('virtual');
            const isAbstract = snippet.label.includes('abstract');
            const isBaseCall = snippet.label.includes('base');

            // Skip override/base snippets if no base class
            if ((isOverride || isBaseCall) && !oopContext.baseClass) {
                continue;
            }

            // Skip virtual/abstract in interfaces (methods are implicitly abstract)
            if (oopContext.inInterface && (isVirtual || isAbstract)) {
                continue;
            }

            completions.push({
                label: snippet.label,
                kind: CompletionItemKind.Snippet,
                insertText: snippet.insertText,
                insertTextFormat: 2,
                detail: 'OOP snippet',
                documentation: snippet.doc,
                sortText: snippet.sortText || ('1' + snippet.label)
            });
        }

        // Add context-specific hint completions
        if (oopContext.inClass && oopContext.baseClass) {
            completions.push({
                label: `base (${oopContext.baseClass})`,
                kind: CompletionItemKind.Keyword,
                insertText: 'base.',
                detail: `Access parent class: ${oopContext.baseClass}`,
                documentation: 'Type "base." to access methods and properties from the parent class',
                sortText: '0base'
            });
        }

        // Add interface implementation hints
        if (oopContext.inClass && oopContext.interfaces.length > 0) {
            for (const iface of oopContext.interfaces) {
                completions.push({
                    label: `implement ${iface}`,
                    kind: CompletionItemKind.Snippet,
                    insertText: `// Implement ${iface}\nfunc \${1:method}(\${2:this}) -> \${3:ReturnType} {\n\t$0\n}`,
                    insertTextFormat: 2,
                    detail: `Implement interface: ${iface}`,
                    documentation: `Add methods required by the ${iface} interface`,
                    sortText: '0implement_' + iface
                });
            }
        }
    }

    return completions;
});

// Resolve additional information for completion item
connection.onCompletionResolve((item: CompletionItem): CompletionItem => {
    return item;
});

// Provide hover information
connection.onHover((params: TextDocumentPositionParams): Hover | null => {
    const document = documents.get(params.textDocument.uri);
    if (!document) {
        return null;
    }

    const text = document.getText();
    const offset = document.offsetAt(params.position);

    // Find the word at the current position
    const word = getWordAtPosition(text, offset);
    if (!word) {
        return null;
    }

    // Check indexed classes
    const classInfo = classIndex.get(word);
    if (classInfo) {
        const chain = getInheritanceChain(word);
        const subclasses = findSubclasses(word);
        let value = `**${classInfo.name}** (class)`;
        if (classInfo.isAbstract) value = `**${classInfo.name}** (abstract class)`;
        if (classInfo.isSealed) value = `**${classInfo.name}** (sealed class)`;

        if (classInfo.baseClass) {
            value += `\n\nExtends: \`${classInfo.baseClass}\``;
        }
        if (classInfo.interfaces.length > 0) {
            value += `\n\nImplements: ${classInfo.interfaces.map(i => `\`${i}\``).join(', ')}`;
        }
        if (chain.length > 1) {
            value += `\n\nInheritance chain: ${chain.map(c => `\`${c}\``).join(' → ')}`;
        }
        if (subclasses.length > 0) {
            value += `\n\nDirect subclasses: ${subclasses.map(s => `\`${s.name}\``).join(', ')}`;
        }
        if (classInfo.methods.length > 0) {
            const publicMethods = classInfo.methods.filter(m => !m.isStatic).slice(0, 5);
            if (publicMethods.length > 0) {
                value += `\n\nMethods: ${publicMethods.map(m => `\`${m.name}()\``).join(', ')}`;
                if (classInfo.methods.length > 5) value += '...';
            }
        }
        return { contents: { kind: MarkupKind.Markdown, value } };
    }

    // Check indexed interfaces
    const ifaceInfo = interfaceIndex.get(word);
    if (ifaceInfo) {
        const implementations = findImplementations(word);
        let value = `**${ifaceInfo.name}** (interface)`;

        if (ifaceInfo.extends.length > 0) {
            value += `\n\nExtends: ${ifaceInfo.extends.map(i => `\`${i}\``).join(', ')}`;
        }
        if (implementations.length > 0) {
            value += `\n\nImplemented by: ${implementations.slice(0, 5).map(c => `\`${c.name}\``).join(', ')}`;
            if (implementations.length > 5) value += '...';
        }
        if (ifaceInfo.methods.length > 0) {
            value += `\n\nMethods: ${ifaceInfo.methods.slice(0, 5).map(m => `\`${m.name}()\``).join(', ')}`;
            if (ifaceInfo.methods.length > 5) value += '...';
        }
        return { contents: { kind: MarkupKind.Markdown, value } };
    }

    // Check keywords
    const keyword = TML_KEYWORDS.find(k => k.name === word);
    if (keyword) {
        return {
            contents: {
                kind: MarkupKind.Markdown,
                value: `**${keyword.name}** (keyword)\n\n${keyword.doc}`
            }
        };
    }

    // Check types
    const type = allTypes.find(t => t.name === word);
    if (type) {
        return {
            contents: {
                kind: MarkupKind.Markdown,
                value: `**${type.name}** (type)\n\n${type.doc}`
            }
        };
    }

    // Check variants
    const variant = TML_VARIANTS.find(v => v.name === word);
    if (variant) {
        return {
            contents: {
                kind: MarkupKind.Markdown,
                value: `**${variant.name}** (variant)\n\n${variant.doc}`
            }
        };
    }

    // Check builtins
    const builtin = TML_BUILTINS.find(f => f.name === word);
    if (builtin) {
        return {
            contents: {
                kind: MarkupKind.Markdown,
                value: `\`\`\`tml\n${builtin.signature}\n\`\`\`\n\n${builtin.doc}`
            }
        };
    }

    // Check effects
    const effect = TML_EFFECTS.find(e => e.name === word);
    if (effect) {
        return {
            contents: {
                kind: MarkupKind.Markdown,
                value: `**${effect.name}** (effect)\n\n${effect.doc}\n\n**Usage:**\n\`\`\`tml\nfunc example() -> T with ${effect.name} { ... }\n\`\`\``
            }
        };
    }

    // Check capabilities
    const capability = TML_CAPABILITIES.find(c => c.name === word);
    if (capability) {
        return {
            contents: {
                kind: MarkupKind.Markdown,
                value: `**${capability.name}** (capability)\n\n${capability.doc}\n\n**Usage:**\n\`\`\`tml\nfunc example(cap: ${capability.name}) { ... }\n\`\`\``
            }
        };
    }

    // Check contracts
    const contract = TML_CONTRACTS.find(c => c.name === word);
    if (contract) {
        return {
            contents: {
                kind: MarkupKind.Markdown,
                value: `**${contract.name}** (contract)\n\n${contract.doc}\n\n**Syntax:**\n\`\`\`tml\n${contract.syntax}\n\`\`\``
            }
        };
    }

    // Check modules
    const module = TML_MODULES.find(m => {
        const parts = m.name.split('::');
        return parts[parts.length - 1] === word;
    });
    if (module) {
        return {
            contents: {
                kind: MarkupKind.Markdown,
                value: `**${module.name}** (module)\n\n${module.doc}\n\n**Exports:** ${module.members.join(', ')}`
            }
        };
    }

    return null;
});

// Helper function to get word at position
function getWordAtPosition(text: string, offset: number): string | null {
    // Find word boundaries
    let start = offset;
    let end = offset;

    // Move start back to beginning of word
    while (start > 0 && isWordChar(text.charAt(start - 1))) {
        start--;
    }

    // Move end forward to end of word
    while (end < text.length && isWordChar(text.charAt(end))) {
        end++;
    }

    if (start === end) {
        return null;
    }

    return text.substring(start, end);
}

function isWordChar(char: string): boolean {
    return /[a-zA-Z0-9_]/.test(char);
}

// ============================================================================
// Go to Definition
// ============================================================================

connection.onDefinition((params: TextDocumentPositionParams): Location | null => {
    const document = documents.get(params.textDocument.uri);
    if (!document) {
        return null;
    }

    const text = document.getText();
    const offset = document.offsetAt(params.position);
    const word = getWordAtPosition(text, offset);

    if (!word) {
        return null;
    }

    // Check if it's a class
    const classInfo = classIndex.get(word);
    if (classInfo) {
        return Location.create(classInfo.uri, Range.create(
            Position.create(classInfo.range.start.line, classInfo.range.start.character),
            Position.create(classInfo.range.start.line, classInfo.range.start.character + word.length)
        ));
    }

    // Check if it's an interface
    const interfaceInfo = interfaceIndex.get(word);
    if (interfaceInfo) {
        return Location.create(interfaceInfo.uri, Range.create(
            Position.create(interfaceInfo.range.start.line, interfaceInfo.range.start.character),
            Position.create(interfaceInfo.range.start.line, interfaceInfo.range.start.character + word.length)
        ));
    }

    // Check if we're on "base" keyword - go to base class
    if (word === 'base') {
        const oopContext = getOOPContext(document, params.position);
        if (oopContext.baseClass) {
            const baseInfo = classIndex.get(oopContext.baseClass);
            if (baseInfo) {
                return Location.create(baseInfo.uri, Range.create(
                    Position.create(baseInfo.range.start.line, baseInfo.range.start.character),
                    Position.create(baseInfo.range.start.line, baseInfo.range.start.character + oopContext.baseClass.length)
                ));
            }
        }
    }

    return null;
});

// ============================================================================
// Find Implementations
// ============================================================================

connection.onImplementation((params: TextDocumentPositionParams): Location[] => {
    const document = documents.get(params.textDocument.uri);
    if (!document) {
        return [];
    }

    const text = document.getText();
    const offset = document.offsetAt(params.position);
    const word = getWordAtPosition(text, offset);

    if (!word) {
        return [];
    }

    const locations: Location[] = [];

    // If it's an interface, find all implementing classes
    if (interfaceIndex.has(word)) {
        const implementations = findImplementations(word);
        for (const cls of implementations) {
            locations.push(Location.create(cls.uri, Range.create(
                Position.create(cls.range.start.line, cls.range.start.character),
                Position.create(cls.range.start.line, cls.range.start.character + cls.name.length)
            )));
        }
    }

    // If it's a class, find all subclasses
    if (classIndex.has(word)) {
        const subclasses = getAllSubclasses(word);
        for (const sub of subclasses) {
            locations.push(Location.create(sub.uri, Range.create(
                Position.create(sub.range.start.line, sub.range.start.character),
                Position.create(sub.range.start.line, sub.range.start.character + sub.name.length)
            )));
        }
    }

    return locations;
});

// ============================================================================
// Type Hierarchy
// ============================================================================

connection.languages.typeHierarchy.onPrepare((params: TypeHierarchyPrepareParams): TypeHierarchyItem[] | null => {
    const document = documents.get(params.textDocument.uri);
    if (!document) {
        return null;
    }

    const text = document.getText();
    const offset = document.offsetAt(params.position);
    const word = getWordAtPosition(text, offset);

    if (!word) {
        return null;
    }

    // Check if it's a class
    const classInfo = classIndex.get(word);
    if (classInfo) {
        return [{
            name: classInfo.name,
            kind: SymbolKind.Class,
            uri: classInfo.uri,
            range: Range.create(
                Position.create(classInfo.range.start.line, classInfo.range.start.character),
                Position.create(classInfo.range.end.line, classInfo.range.end.character)
            ),
            selectionRange: Range.create(
                Position.create(classInfo.range.start.line, classInfo.range.start.character),
                Position.create(classInfo.range.start.line, classInfo.range.start.character + classInfo.name.length)
            ),
            data: { type: 'class', name: classInfo.name }
        }];
    }

    // Check if it's an interface
    const interfaceInfo = interfaceIndex.get(word);
    if (interfaceInfo) {
        return [{
            name: interfaceInfo.name,
            kind: SymbolKind.Interface,
            uri: interfaceInfo.uri,
            range: Range.create(
                Position.create(interfaceInfo.range.start.line, interfaceInfo.range.start.character),
                Position.create(interfaceInfo.range.end.line, interfaceInfo.range.end.character)
            ),
            selectionRange: Range.create(
                Position.create(interfaceInfo.range.start.line, interfaceInfo.range.start.character),
                Position.create(interfaceInfo.range.start.line, interfaceInfo.range.start.character + interfaceInfo.name.length)
            ),
            data: { type: 'interface', name: interfaceInfo.name }
        }];
    }

    return null;
});

connection.languages.typeHierarchy.onSupertypes((params: TypeHierarchySupertypesParams): TypeHierarchyItem[] => {
    const data = params.item.data as { type: string; name: string } | undefined;
    if (!data) {
        return [];
    }

    const results: TypeHierarchyItem[] = [];

    if (data.type === 'class') {
        const classInfo = classIndex.get(data.name);
        if (classInfo) {
            // Add base class
            if (classInfo.baseClass) {
                const baseInfo = classIndex.get(classInfo.baseClass);
                if (baseInfo) {
                    results.push({
                        name: baseInfo.name,
                        kind: SymbolKind.Class,
                        uri: baseInfo.uri,
                        range: Range.create(
                            Position.create(baseInfo.range.start.line, baseInfo.range.start.character),
                            Position.create(baseInfo.range.end.line, baseInfo.range.end.character)
                        ),
                        selectionRange: Range.create(
                            Position.create(baseInfo.range.start.line, baseInfo.range.start.character),
                            Position.create(baseInfo.range.start.line, baseInfo.range.start.character + baseInfo.name.length)
                        ),
                        data: { type: 'class', name: baseInfo.name }
                    });
                }
            }

            // Add implemented interfaces
            for (const ifaceName of classInfo.interfaces) {
                const ifaceInfo = interfaceIndex.get(ifaceName);
                if (ifaceInfo) {
                    results.push({
                        name: ifaceInfo.name,
                        kind: SymbolKind.Interface,
                        uri: ifaceInfo.uri,
                        range: Range.create(
                            Position.create(ifaceInfo.range.start.line, ifaceInfo.range.start.character),
                            Position.create(ifaceInfo.range.end.line, ifaceInfo.range.end.character)
                        ),
                        selectionRange: Range.create(
                            Position.create(ifaceInfo.range.start.line, ifaceInfo.range.start.character),
                            Position.create(ifaceInfo.range.start.line, ifaceInfo.range.start.character + ifaceInfo.name.length)
                        ),
                        data: { type: 'interface', name: ifaceInfo.name }
                    });
                }
            }
        }
    } else if (data.type === 'interface') {
        const ifaceInfo = interfaceIndex.get(data.name);
        if (ifaceInfo) {
            // Add extended interfaces
            for (const extName of ifaceInfo.extends) {
                const extInfo = interfaceIndex.get(extName);
                if (extInfo) {
                    results.push({
                        name: extInfo.name,
                        kind: SymbolKind.Interface,
                        uri: extInfo.uri,
                        range: Range.create(
                            Position.create(extInfo.range.start.line, extInfo.range.start.character),
                            Position.create(extInfo.range.end.line, extInfo.range.end.character)
                        ),
                        selectionRange: Range.create(
                            Position.create(extInfo.range.start.line, extInfo.range.start.character),
                            Position.create(extInfo.range.start.line, extInfo.range.start.character + extInfo.name.length)
                        ),
                        data: { type: 'interface', name: extInfo.name }
                    });
                }
            }
        }
    }

    return results;
});

connection.languages.typeHierarchy.onSubtypes((params: TypeHierarchySubtypesParams): TypeHierarchyItem[] => {
    const data = params.item.data as { type: string; name: string } | undefined;
    if (!data) {
        return [];
    }

    const results: TypeHierarchyItem[] = [];

    if (data.type === 'class') {
        // Find direct subclasses
        const subclasses = findSubclasses(data.name);
        for (const sub of subclasses) {
            results.push({
                name: sub.name,
                kind: SymbolKind.Class,
                uri: sub.uri,
                range: Range.create(
                    Position.create(sub.range.start.line, sub.range.start.character),
                    Position.create(sub.range.end.line, sub.range.end.character)
                ),
                selectionRange: Range.create(
                    Position.create(sub.range.start.line, sub.range.start.character),
                    Position.create(sub.range.start.line, sub.range.start.character + sub.name.length)
                ),
                data: { type: 'class', name: sub.name }
            });
        }
    } else if (data.type === 'interface') {
        // Find implementing classes
        const implementations = findImplementations(data.name);
        for (const impl of implementations) {
            results.push({
                name: impl.name,
                kind: SymbolKind.Class,
                uri: impl.uri,
                range: Range.create(
                    Position.create(impl.range.start.line, impl.range.start.character),
                    Position.create(impl.range.end.line, impl.range.end.character)
                ),
                selectionRange: Range.create(
                    Position.create(impl.range.start.line, impl.range.start.character),
                    Position.create(impl.range.start.line, impl.range.start.character + impl.name.length)
                ),
                data: { type: 'class', name: impl.name }
            });
        }

        // Find extending interfaces
        for (const [, iface] of interfaceIndex) {
            if (iface.extends.includes(data.name)) {
                results.push({
                    name: iface.name,
                    kind: SymbolKind.Interface,
                    uri: iface.uri,
                    range: Range.create(
                        Position.create(iface.range.start.line, iface.range.start.character),
                        Position.create(iface.range.end.line, iface.range.end.character)
                    ),
                    selectionRange: Range.create(
                        Position.create(iface.range.start.line, iface.range.start.character),
                        Position.create(iface.range.start.line, iface.range.start.character + iface.name.length)
                    ),
                    data: { type: 'interface', name: iface.name }
                });
            }
        }
    }

    return results;
});

// ============================================================================
// Document Event Handlers for Validation
// ============================================================================

// After initialization, register for configuration changes
connection.onInitialized(() => {
    if (hasConfigurationCapability) {
        // Register for configuration changes
        connection.client.register(DidChangeConfigurationNotification.type, undefined);
    }
});

// Handle configuration changes
connection.onDidChangeConfiguration((change: DidChangeConfigurationParams) => {
    if (hasConfigurationCapability) {
        // Reset cached settings
        documentSettings.clear();
    } else {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const settings = (change.settings as any)?.tml || {};
        globalSettings = {
            ...defaultSettings,
            ...settings
        };
    }

    // Re-validate all open documents
    documents.all().forEach(queueValidation);
});

// Validate document when opened
documents.onDidOpen((event: TextDocumentChangeEvent<TextDocument>) => {
    indexDocument(event.document);  // Index for navigation
    queueValidation(event.document);
});

// Validate document when content changes
documents.onDidChangeContent((event: TextDocumentChangeEvent<TextDocument>) => {
    indexDocument(event.document);  // Re-index for navigation
    queueValidation(event.document);
});

// Clear diagnostics when document is closed
documents.onDidClose((event: TextDocumentChangeEvent<TextDocument>) => {
    // Clear any pending validation
    const timeout = validationQueue.get(event.document.uri);
    if (timeout) {
        clearTimeoutNode(timeout);
        validationQueue.delete(event.document.uri);
    }

    // Clear settings cache
    documentSettings.delete(event.document.uri);

    // Clear diagnostics
    connection.sendDiagnostics({ uri: event.document.uri, diagnostics: [] });
});

// Make the text document manager listen on the connection
documents.listen(connection);

// Listen on the connection
connection.listen();
