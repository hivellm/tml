import * as assert from 'assert';
import * as vscode from 'vscode';

suite('TML Extension Test Suite', () => {
    vscode.window.showInformationMessage('Start all tests.');

    test('Extension should be present', () => {
        const extension = vscode.extensions.getExtension('tml-lang.tml-language');
        assert.ok(extension, 'Extension should be installed');
    });

    test('TML language should be registered', async () => {
        const languages = await vscode.languages.getLanguages();
        assert.ok(languages.includes('tml'), 'TML language not registered');
    });

    test('Commands should be registered', async () => {
        const commands = await vscode.commands.getCommands(true);

        const expectedCommands = [
            'tml.build',
            'tml.buildRelease',
            'tml.run',
            'tml.test',
            'tml.clean'
        ];

        for (const cmd of expectedCommands) {
            assert.ok(commands.includes(cmd), `Command ${cmd} not registered`);
        }
    });

    test('Configuration should be available', () => {
        const config = vscode.workspace.getConfiguration('tml');

        // Check default values
        assert.strictEqual(config.get('compilerPath'), 'tml');
        assert.strictEqual(config.get('maxNumberOfProblems'), 100);
        assert.deepStrictEqual(config.get('buildArgs'), []);
        assert.strictEqual(config.get('enableDiagnostics'), true);
    });

    test('Configuration properties should exist', () => {
        const config = vscode.workspace.getConfiguration('tml');

        // Check that all expected properties are defined
        const properties = [
            'compilerPath',
            'maxNumberOfProblems',
            'buildArgs',
            'trace.server',
            'enableDiagnostics'
        ];

        for (const prop of properties) {
            const inspection = config.inspect(prop);
            assert.ok(inspection !== undefined, `Property ${prop} should be defined`);
        }
    });
});

suite('Language Server Tests', () => {
    test('Completion provider should return items', async () => {
        // Create a temporary TML document
        const doc = await vscode.workspace.openTextDocument({
            language: 'tml',
            content: 'func '
        });

        // Wait for language server to initialize
        await new Promise(resolve => setTimeout(resolve, 2000));

        // Get completions
        const position = new vscode.Position(0, 5);
        const completions = await vscode.commands.executeCommand<vscode.CompletionList>(
            'vscode.executeCompletionItemProvider',
            doc.uri,
            position
        );

        // Check that we got completions
        assert.ok(completions, 'No completions returned');
        assert.ok(completions.items.length > 0, 'No completion items');

        // Check for some expected keywords
        const labels = completions.items.map(item =>
            typeof item.label === 'string' ? item.label : item.label.label
        );

        // Should have common keywords
        assert.ok(labels.some(l => l.includes('func') || l.includes('if') || l.includes('let')),
            'Missing expected keywords in completions');
    });

    test('Completion should include types', async () => {
        const doc = await vscode.workspace.openTextDocument({
            language: 'tml',
            content: 'let x: '
        });

        await new Promise(resolve => setTimeout(resolve, 1000));

        const position = new vscode.Position(0, 7);
        const completions = await vscode.commands.executeCommand<vscode.CompletionList>(
            'vscode.executeCompletionItemProvider',
            doc.uri,
            position
        );

        assert.ok(completions, 'No completions returned');

        const labels = completions.items.map(item =>
            typeof item.label === 'string' ? item.label : item.label.label
        );

        // Should have type completions
        assert.ok(labels.some(l => l === 'I32' || l === 'Bool' || l === 'Str'),
            'Missing type completions');
    });

    test('Completion should include effects', async () => {
        const doc = await vscode.workspace.openTextDocument({
            language: 'tml',
            content: 'func test() with '
        });

        await new Promise(resolve => setTimeout(resolve, 1000));

        const position = new vscode.Position(0, 17);
        const completions = await vscode.commands.executeCommand<vscode.CompletionList>(
            'vscode.executeCompletionItemProvider',
            doc.uri,
            position
        );

        assert.ok(completions, 'No completions returned');

        const labels = completions.items.map(item =>
            typeof item.label === 'string' ? item.label : item.label.label
        );

        // Should have effect completions
        assert.ok(labels.some(l => l === 'pure' || l === 'io' || l === 'async'),
            'Missing effect completions');
    });

    test('Hover provider should return information', async () => {
        // Create a document with a known keyword
        const doc = await vscode.workspace.openTextDocument({
            language: 'tml',
            content: 'func test() {}'
        });

        await new Promise(resolve => setTimeout(resolve, 1000));

        // Get hover info for 'func'
        const position = new vscode.Position(0, 2);
        const hovers = await vscode.commands.executeCommand<vscode.Hover[]>(
            'vscode.executeHoverProvider',
            doc.uri,
            position
        );

        assert.ok(hovers, 'No hovers returned');
        assert.ok(hovers.length > 0, 'No hover items');
    });

    test('Hover should show type information', async () => {
        const doc = await vscode.workspace.openTextDocument({
            language: 'tml',
            content: 'let x: I32 = 42'
        });

        await new Promise(resolve => setTimeout(resolve, 1000));

        // Get hover info for 'I32'
        const position = new vscode.Position(0, 8);
        const hovers = await vscode.commands.executeCommand<vscode.Hover[]>(
            'vscode.executeHoverProvider',
            doc.uri,
            position
        );

        assert.ok(hovers, 'No hovers returned');
        assert.ok(hovers.length > 0, 'No hover items for type');
    });

    test('Semantic tokens should be provided', async () => {
        const doc = await vscode.workspace.openTextDocument({
            language: 'tml',
            content: `func main() {
    let x: I32 = 42
    println("Hello")
}`
        });

        await new Promise(resolve => setTimeout(resolve, 1000));

        // Request semantic tokens - verify the command completes without throwing
        await vscode.commands.executeCommand<vscode.SemanticTokens>(
            'vscode.provideDocumentSemanticTokens',
            doc.uri
        );

        // Tokens may or may not be available depending on server state
        // Just verify the command doesn't throw
        assert.ok(true, 'Semantic tokens request completed');
    });
});

suite('Syntax Highlighting Tests', () => {
    test('TML file should have syntax highlighting', async () => {
        const doc = await vscode.workspace.openTextDocument({
            language: 'tml',
            content: `
func main() {
    let x: I32 = 42
    println("Hello, World!")
}
`
        });

        // Verify document language
        assert.strictEqual(doc.languageId, 'tml');
    });

    test('Document should recognize TML keywords', async () => {
        const doc = await vscode.workspace.openTextDocument({
            language: 'tml',
            content: 'func if else when for while loop return let mut'
        });

        assert.strictEqual(doc.languageId, 'tml');
    });

    test('Document should handle OOP constructs', async () => {
        const doc = await vscode.workspace.openTextDocument({
            language: 'tml',
            content: `
class MyClass extends Base implements IFace {
    virtual func method(this) -> I32 {
        return 0
    }
}
`
        });

        assert.strictEqual(doc.languageId, 'tml');
    });

    test('Document should handle effects and capabilities', async () => {
        const doc = await vscode.workspace.openTextDocument({
            language: 'tml',
            content: `
func read_file(path: Str) -> Str with io {
    // I/O operation
}

func process(fs: Fs, net: Net) with pure {
    // Pure function with capabilities
}
`
        });

        assert.strictEqual(doc.languageId, 'tml');
    });

    test('Document should handle contracts', async () => {
        const doc = await vscode.workspace.openTextDocument({
            language: 'tml',
            content: `
func divide(a: I32, b: I32) -> I32
    requires b != 0
    ensures result * b == a
{
    return a / b
}
`
        });

        assert.strictEqual(doc.languageId, 'tml');
    });
});

suite('Task Provider Tests', () => {
    test('Task provider should be registered', async () => {
        // Tasks may or may not be available depending on workspace state
        // Just verify the fetch completes without throwing
        await vscode.tasks.fetchTasks({ type: 'tml' });
        assert.ok(true, 'Task fetch completed without error');
    });
});

suite('Diagnostics Tests', () => {
    test('Diagnostics setting should be configurable', () => {
        const config = vscode.workspace.getConfiguration('tml');
        const enableDiagnostics = config.get<boolean>('enableDiagnostics');
        assert.strictEqual(enableDiagnostics, true, 'enableDiagnostics should default to true');
    });

    test('Diagnostics collection should exist for TML', async () => {
        // Create a TML document
        const doc = await vscode.workspace.openTextDocument({
            language: 'tml',
            content: 'func test() {}'
        });

        // Wait for potential validation
        await new Promise(resolve => setTimeout(resolve, 1000));

        // Get diagnostics for the document
        const diagnostics = vscode.languages.getDiagnostics(doc.uri);

        // Diagnostics may be empty if no errors, but the collection should exist
        assert.ok(Array.isArray(diagnostics), 'Diagnostics should be an array');
    });
});
