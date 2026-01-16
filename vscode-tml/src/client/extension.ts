import * as path from 'path';
import { workspace, ExtensionContext } from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind
} from 'vscode-languageclient/node';
import { registerCommands } from './commands';
import { registerTaskProvider } from './taskProvider';

let client: LanguageClient;

export function activate(context: ExtensionContext) {
    // Register commands (build, run, test, clean)
    registerCommands(context);

    // Register task provider
    registerTaskProvider(context);

    // The server is implemented in node
    const serverModule = context.asAbsolutePath(
        path.join('out', 'server', 'server.js')
    );

    // Server options - run the server in Node
    const serverOptions: ServerOptions = {
        run: {
            module: serverModule,
            transport: TransportKind.ipc
        },
        debug: {
            module: serverModule,
            transport: TransportKind.ipc,
            options: {
                execArgv: ['--nolazy', '--inspect=6009']
            }
        }
    };

    // Options to control the language client
    const clientOptions: LanguageClientOptions = {
        // Register the server for TML documents
        documentSelector: [{ scheme: 'file', language: 'tml' }],
        synchronize: {
            // Notify the server about file changes to '.tml' files in the workspace
            fileEvents: workspace.createFileSystemWatcher('**/*.tml')
        }
    };

    // Create the language client and start it
    client = new LanguageClient(
        'tmlLanguageServer',
        'TML Language Server',
        serverOptions,
        clientOptions
    );

    // Start the client (also launches the server)
    client.start();
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
