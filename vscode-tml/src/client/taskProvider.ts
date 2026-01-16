import * as vscode from 'vscode';
import * as path from 'path';

interface TmlTaskDefinition extends vscode.TaskDefinition {
    task: string;
    file?: string;
    release?: boolean;
    args?: string[];
}

/**
 * Task provider for TML build tasks
 */
export class TmlTaskProvider implements vscode.TaskProvider {
    static TmlType = 'tml';
    private tasks: vscode.Task[] | undefined;

    constructor(private workspaceRoot: string | undefined) {}

    public async provideTasks(): Promise<vscode.Task[]> {
        return this.getTasks();
    }

    public resolveTask(task: vscode.Task): vscode.Task | undefined {
        const definition = task.definition as TmlTaskDefinition;
        if (definition.task) {
            return this.getTask(
                definition.task,
                definition.file,
                definition.release,
                definition.args,
                definition
            );
        }
        return undefined;
    }

    private getTasks(): vscode.Task[] {
        if (this.tasks !== undefined) {
            return this.tasks;
        }

        this.tasks = [];

        // Add default tasks
        this.tasks.push(this.getTask('build', undefined, false, [], {
            type: TmlTaskProvider.TmlType,
            task: 'build'
        }));

        this.tasks.push(this.getTask('build', undefined, true, [], {
            type: TmlTaskProvider.TmlType,
            task: 'build',
            release: true
        }));

        this.tasks.push(this.getTask('run', undefined, false, [], {
            type: TmlTaskProvider.TmlType,
            task: 'run'
        }));

        this.tasks.push(this.getTask('test', undefined, false, [], {
            type: TmlTaskProvider.TmlType,
            task: 'test'
        }));

        this.tasks.push(this.getTask('clean', undefined, false, [], {
            type: TmlTaskProvider.TmlType,
            task: 'clean'
        }));

        return this.tasks;
    }

    private getTask(
        taskName: string,
        file: string | undefined,
        release: boolean | undefined,
        args: string[] | undefined,
        definition: TmlTaskDefinition
    ): vscode.Task {
        const config = vscode.workspace.getConfiguration('tml');
        const compiler = config.get<string>('compilerPath', 'tml');
        const extraArgs = config.get<string[]>('buildArgs', []);

        // Build command arguments
        const cmdArgs: string[] = [taskName];

        if (file) {
            cmdArgs.push(file);
        } else if (taskName === 'build' || taskName === 'run') {
            // Use ${file} placeholder for current file
            cmdArgs.push('${file}');
        }

        if (release && (taskName === 'build' || taskName === 'run')) {
            cmdArgs.push('--release');
        }

        if (args && args.length > 0) {
            cmdArgs.push(...args);
        }

        cmdArgs.push(...extraArgs);

        // Create task name
        let displayName = `TML: ${taskName}`;
        if (release) {
            displayName += ' (release)';
        }
        if (file) {
            displayName += ` - ${path.basename(file)}`;
        }

        // Create shell execution
        const shellExecution = new vscode.ShellExecution(compiler, cmdArgs, {
            cwd: this.workspaceRoot
        });

        const task = new vscode.Task(
            definition,
            vscode.TaskScope.Workspace,
            displayName,
            'tml',
            shellExecution,
            ['$tml', '$tml-simple']
        );

        task.group = taskName === 'build'
            ? vscode.TaskGroup.Build
            : taskName === 'test'
                ? vscode.TaskGroup.Test
                : undefined;

        task.presentationOptions = {
            reveal: vscode.TaskRevealKind.Always,
            panel: vscode.TaskPanelKind.Shared,
            clear: true
        };

        return task;
    }
}

/**
 * Register the TML task provider
 */
export function registerTaskProvider(context: vscode.ExtensionContext): void {
    const workspaceRoot = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;

    const taskProvider = vscode.tasks.registerTaskProvider(
        TmlTaskProvider.TmlType,
        new TmlTaskProvider(workspaceRoot)
    );

    context.subscriptions.push(taskProvider);
}
