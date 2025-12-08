import * as vscode from 'vscode';
import { startClient } from './client';

export async function activate(context: vscode.ExtensionContext) {
  console.log('[MoZuku] Extension activation started...');

  const serverPath =
    vscode.workspace.getConfiguration('mozuku').get<string>('serverPath') ||
    'mozuku-lsp';
  console.log('[MoZuku] LSP client starting: server path =', serverPath);
  const client = await startClient(context, serverPath);

  console.log('[MoZuku] Extension activation completed');
}

export function deactivate() { }
