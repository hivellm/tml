import * as vscode from 'vscode';

/**
 * Get the TML compiler path from settings
 */
function getCompilerPath(): string {
    const config = vscode.workspace.getConfiguration('tml');
    return config.get<string>('compilerPath', 'tml');
}

/**
 * Get additional build arguments from settings
 */
function getBuildArgs(): string[] {
    const config = vscode.workspace.getConfiguration('tml');
    return config.get<string[]>('buildArgs', []);
}

/**
 * Get the current TML file path
 */
function getCurrentFile(): string | undefined {
    const editor = vscode.window.activeTextEditor;
    if (editor && editor.document.languageId === 'tml') {
        return editor.document.fileName;
    }
    return undefined;
}

/**
 * Execute a TML command as a terminal task
 */
async function runTmlCommand(
    command: string,
    args: string[],
    name: string
): Promise<vscode.TaskExecution | undefined> {
    const workspaceFolder = vscode.workspace.workspaceFolders?.[0];

    const shellExecution = new vscode.ShellExecution(command, args, {
        cwd: workspaceFolder?.uri.fsPath
    });

    const task = new vscode.Task(
        { type: 'tml', task: name },
        workspaceFolder || vscode.TaskScope.Workspace,
        name,
        'tml',
        shellExecution,
        ['$tml', '$tml-simple']
    );

    task.presentationOptions = {
        reveal: vscode.TaskRevealKind.Always,
        panel: vscode.TaskPanelKind.Shared,
        clear: true
    };

    return vscode.tasks.executeTask(task);
}

/**
 * Build the current TML file
 */
export async function buildCommand(): Promise<void> {
    const file = getCurrentFile();
    if (!file) {
        vscode.window.showErrorMessage('No TML file is currently open.');
        return;
    }

    const compiler = getCompilerPath();
    const extraArgs = getBuildArgs();
    const args = ['build', file, ...extraArgs];

    await runTmlCommand(compiler, args, 'TML Build');
}

/**
 * Build the current TML file in release mode
 */
export async function buildReleaseCommand(): Promise<void> {
    const file = getCurrentFile();
    if (!file) {
        vscode.window.showErrorMessage('No TML file is currently open.');
        return;
    }

    const compiler = getCompilerPath();
    const extraArgs = getBuildArgs();
    const args = ['build', file, '--release', ...extraArgs];

    await runTmlCommand(compiler, args, 'TML Build (Release)');
}

/**
 * Run the current TML file
 */
export async function runCommand(): Promise<void> {
    const file = getCurrentFile();
    if (!file) {
        vscode.window.showErrorMessage('No TML file is currently open.');
        return;
    }

    const compiler = getCompilerPath();
    const extraArgs = getBuildArgs();
    const args = ['run', file, ...extraArgs];

    await runTmlCommand(compiler, args, 'TML Run');
}

/**
 * Run tests in the current workspace
 */
export async function testCommand(): Promise<void> {
    const file = getCurrentFile();
    const compiler = getCompilerPath();
    const extraArgs = getBuildArgs();

    const args = file
        ? ['test', file, ...extraArgs]
        : ['test', ...extraArgs];

    await runTmlCommand(compiler, args, 'TML Test');
}

/**
 * Clean build artifacts
 */
export async function cleanCommand(): Promise<void> {
    const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
    if (!workspaceFolder) {
        vscode.window.showErrorMessage('No workspace folder is open.');
        return;
    }

    const compiler = getCompilerPath();
    await runTmlCommand(compiler, ['clean'], 'TML Clean');
}

/**
 * Register all TML commands
 */
export function registerCommands(context: vscode.ExtensionContext): void {
    context.subscriptions.push(
        vscode.commands.registerCommand('tml.build', buildCommand),
        vscode.commands.registerCommand('tml.buildRelease', buildReleaseCommand),
        vscode.commands.registerCommand('tml.run', runCommand),
        vscode.commands.registerCommand('tml.test', testCommand),
        vscode.commands.registerCommand('tml.clean', cleanCommand)
    );
}
