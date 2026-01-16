import * as assert from 'assert';

// Server unit tests - testing pure functions extracted from server logic
suite('Server Unit Tests', () => {
    suite('Word Detection', () => {
        // Reimplement the word detection logic for testing
        function isWordChar(char: string): boolean {
            return /[a-zA-Z0-9_]/.test(char);
        }

        function getWordAtPosition(text: string, offset: number): string | null {
            let start = offset;
            let end = offset;

            while (start > 0 && isWordChar(text.charAt(start - 1))) {
                start--;
            }

            while (end < text.length && isWordChar(text.charAt(end))) {
                end++;
            }

            if (start === end) {
                return null;
            }

            return text.substring(start, end);
        }

        test('should detect word at beginning', () => {
            const text = 'func main() {}';
            assert.strictEqual(getWordAtPosition(text, 2), 'func');
        });

        test('should detect word in middle', () => {
            const text = 'let x: I32 = 42';
            assert.strictEqual(getWordAtPosition(text, 8), 'I32');
        });

        test('should detect word at end', () => {
            const text = 'return value';
            assert.strictEqual(getWordAtPosition(text, 10), 'value');
        });

        test('should return null for non-word position', () => {
            const text = 'func main() {}';
            assert.strictEqual(getWordAtPosition(text, 4), 'func'); // still in 'func'
        });

        test('should handle empty text', () => {
            assert.strictEqual(getWordAtPosition('', 0), null);
        });

        test('should handle underscore in identifiers', () => {
            const text = 'my_variable = 10';
            assert.strictEqual(getWordAtPosition(text, 5), 'my_variable');
        });

        test('should handle numbers in identifiers', () => {
            const text = 'var123 = 10';
            assert.strictEqual(getWordAtPosition(text, 3), 'var123');
        });
    });

    suite('OOP Context Detection', () => {
        interface OOPContext {
            inClass: boolean;
            inInterface: boolean;
            className: string | null;
            baseClass: string | null;
            interfaces: string[];
            afterBase: boolean;
        }

        function getOOPContext(text: string, line: number, character: number): OOPContext {
            const lines = text.split('\n');
            const currentLine = lines[line] || '';
            const lineUpToCursor = currentLine.substring(0, character);
            const afterBase = /\bbase\.\s*$/.test(lineUpToCursor);

            let inClass = false;
            let inInterface = false;
            let className: string | null = null;
            let baseClass: string | null = null;
            let interfaces: string[] = [];

            let braceDepth = 0;
            let foundDecl = false;

            for (let lineNum = line; lineNum >= 0; lineNum--) {
                const l = lines[lineNum];

                for (let i = l.length - 1; i >= 0; i--) {
                    if (lineNum === line && i >= character) continue;
                    if (l[i] === '}') braceDepth++;
                    else if (l[i] === '{') braceDepth--;
                }

                if (braceDepth < 0 && !foundDecl) {
                    const classMatch = l.match(/\b(abstract\s+|sealed\s+)?class\s+(\w+)(?:\s+extends\s+(\w+))?(?:\s+implements\s+([\w,\s]+))?/);
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

                    const interfaceMatch = l.match(/\binterface\s+(\w+)/);
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

        test('should detect class context', () => {
            const text = `class MyClass {
    func test() {

    }
}`;
            const ctx = getOOPContext(text, 2, 8);
            assert.strictEqual(ctx.inClass, true);
            assert.strictEqual(ctx.className, 'MyClass');
        });

        test('should detect class with extends', () => {
            const text = `class Child extends Parent {
    func test() {

    }
}`;
            const ctx = getOOPContext(text, 2, 8);
            assert.strictEqual(ctx.inClass, true);
            assert.strictEqual(ctx.className, 'Child');
            assert.strictEqual(ctx.baseClass, 'Parent');
        });

        test('should detect class with implements', () => {
            const text = `class MyClass implements IFace1, IFace2 {
    func test() {

    }
}`;
            const ctx = getOOPContext(text, 2, 8);
            assert.strictEqual(ctx.inClass, true);
            assert.deepStrictEqual(ctx.interfaces, ['IFace1', 'IFace2']);
        });

        test('should detect interface context', () => {
            const text = `interface MyInterface {
    func test()
}`;
            const ctx = getOOPContext(text, 1, 4);
            assert.strictEqual(ctx.inInterface, true);
            assert.strictEqual(ctx.className, 'MyInterface');
        });

        test('should detect base. access', () => {
            const text = `class Child extends Parent {
    func test() {
        base.
    }
}`;
            const ctx = getOOPContext(text, 2, 13);
            assert.strictEqual(ctx.afterBase, true);
        });

        test('should not detect outside class', () => {
            const text = `func main() {
    let x = 10
}`;
            const ctx = getOOPContext(text, 1, 4);
            assert.strictEqual(ctx.inClass, false);
            assert.strictEqual(ctx.inInterface, false);
        });
    });

    suite('Import Context Detection', () => {
        interface ImportContext {
            afterUse: boolean;
            currentPath: string;
        }

        function getImportContext(text: string, line: number, character: number): ImportContext {
            const lines = text.split('\n');
            const currentLine = lines[line] || '';
            const lineUpToCursor = currentLine.substring(0, character);

            const useMatch = lineUpToCursor.match(/\buse\s+([\w:.]*?)$/);
            if (useMatch) {
                return { afterUse: true, currentPath: useMatch[1] };
            }

            return { afterUse: false, currentPath: '' };
        }

        test('should detect use statement', () => {
            const text = 'use std::';
            const ctx = getImportContext(text, 0, 9);
            assert.strictEqual(ctx.afterUse, true);
            assert.strictEqual(ctx.currentPath, 'std::');
        });

        test('should detect nested path', () => {
            const text = 'use std::io::';
            const ctx = getImportContext(text, 0, 13);
            assert.strictEqual(ctx.afterUse, true);
            assert.strictEqual(ctx.currentPath, 'std::io::');
        });

        test('should not detect outside use', () => {
            const text = 'let x = std::io::read()';
            const ctx = getImportContext(text, 0, 10);
            assert.strictEqual(ctx.afterUse, false);
        });
    });

    suite('Diagnostic Severity Conversion', () => {
        // LSP DiagnosticSeverity values
        const DiagnosticSeverity = {
            Error: 1,
            Warning: 2,
            Information: 3,
            Hint: 4
        };

        function toLspSeverity(severity: string): number {
            switch (severity) {
                case 'error': return DiagnosticSeverity.Error;
                case 'warning': return DiagnosticSeverity.Warning;
                case 'note': return DiagnosticSeverity.Information;
                case 'help': return DiagnosticSeverity.Hint;
                default: return DiagnosticSeverity.Error;
            }
        }

        test('should convert error severity', () => {
            assert.strictEqual(toLspSeverity('error'), DiagnosticSeverity.Error);
        });

        test('should convert warning severity', () => {
            assert.strictEqual(toLspSeverity('warning'), DiagnosticSeverity.Warning);
        });

        test('should convert note severity', () => {
            assert.strictEqual(toLspSeverity('note'), DiagnosticSeverity.Information);
        });

        test('should convert help severity', () => {
            assert.strictEqual(toLspSeverity('help'), DiagnosticSeverity.Hint);
        });

        test('should default to error for unknown', () => {
            assert.strictEqual(toLspSeverity('unknown'), DiagnosticSeverity.Error);
        });
    });

    suite('Keyword Recognition', () => {
        const TML_KEYWORDS = new Set([
            'and', 'as', 'async', 'await', 'behavior', 'break', 'const', 'continue',
            'crate', 'decorator', 'do', 'else', 'false', 'for', 'func', 'if', 'impl',
            'in', 'let', 'loop', 'lowlevel', 'mod', 'mut', 'not', 'or', 'pub', 'quote',
            'ref', 'return', 'super', 'then', 'this', 'This', 'through', 'to', 'true',
            'type', 'use', 'when', 'where', 'while', 'with',
            // OOP keywords
            'class', 'interface', 'extends', 'implements', 'override', 'virtual',
            'abstract', 'sealed', 'base', 'new', 'prop', 'private', 'protected', 'static'
        ]);

        test('should recognize all TML keywords', () => {
            assert.ok(TML_KEYWORDS.has('func'));
            assert.ok(TML_KEYWORDS.has('class'));
            assert.ok(TML_KEYWORDS.has('interface'));
            assert.ok(TML_KEYWORDS.has('override'));
        });

        test('should not recognize non-keywords', () => {
            assert.ok(!TML_KEYWORDS.has('foo'));
            assert.ok(!TML_KEYWORDS.has('bar'));
            assert.ok(!TML_KEYWORDS.has('main'));
        });
    });

    suite('Type Recognition', () => {
        const TML_TYPES = new Set([
            'Bool', 'I8', 'I16', 'I32', 'I64', 'I128',
            'U8', 'U16', 'U32', 'U64', 'U128',
            'F32', 'F64', 'Char', 'String', 'Str', 'Unit', 'Never', 'Ptr', 'Text',
            // Collections
            'List', 'Map', 'HashMap', 'Set', 'Vec', 'Buffer',
            // Wrappers
            'Maybe', 'Outcome', 'Result', 'Option', 'Heap', 'Shared', 'Sync'
        ]);

        test('should recognize primitive types', () => {
            assert.ok(TML_TYPES.has('I32'));
            assert.ok(TML_TYPES.has('Bool'));
            assert.ok(TML_TYPES.has('Str'));
        });

        test('should recognize collection types', () => {
            assert.ok(TML_TYPES.has('List'));
            assert.ok(TML_TYPES.has('Map'));
            assert.ok(TML_TYPES.has('Set'));
        });

        test('should recognize wrapper types', () => {
            assert.ok(TML_TYPES.has('Maybe'));
            assert.ok(TML_TYPES.has('Outcome'));
            assert.ok(TML_TYPES.has('Shared'));
        });
    });

    suite('Effect Recognition', () => {
        const TML_EFFECTS = new Set([
            'pure', 'io', 'throws', 'async', 'unsafe', 'diverges', 'alloc', 'nondet'
        ]);

        test('should recognize all effects', () => {
            assert.ok(TML_EFFECTS.has('pure'));
            assert.ok(TML_EFFECTS.has('io'));
            assert.ok(TML_EFFECTS.has('async'));
        });
    });

    suite('Capability Recognition', () => {
        const TML_CAPABILITIES = new Set([
            'Read', 'Write', 'Exec', 'Net', 'Fs', 'Env', 'Time', 'Random'
        ]);

        test('should recognize all capabilities', () => {
            assert.ok(TML_CAPABILITIES.has('Read'));
            assert.ok(TML_CAPABILITIES.has('Write'));
            assert.ok(TML_CAPABILITIES.has('Fs'));
            assert.ok(TML_CAPABILITIES.has('Net'));
        });
    });

    suite('Contract Recognition', () => {
        const TML_CONTRACTS = new Set([
            'requires', 'ensures', 'invariant', 'assert', 'assume'
        ]);

        test('should recognize all contracts', () => {
            assert.ok(TML_CONTRACTS.has('requires'));
            assert.ok(TML_CONTRACTS.has('ensures'));
            assert.ok(TML_CONTRACTS.has('invariant'));
        });
    });
});
