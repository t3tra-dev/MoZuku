import * as vscode from "vscode";
import * as fs from "fs";
import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
  TransportKind,
  State,
} from "vscode-languageclient/node";
import { buildDocumentSelector, buildInitializationOptions } from "./config";
import { resolveServerPath as discoverServerPath } from "./server-discovery";

type CommentHighlightMessage = {
  uri: string;
  ranges: Array<{
    start: { line: number; character: number };
    end: { line: number; character: number };
  }>;
};

type ContentHighlightMessage = {
  uri: string;
  ranges: Array<{
    start: { line: number; character: number };
    end: { line: number; character: number };
  }>;
};

type SemanticHighlightMessage = {
  uri: string;
  tokens: Array<{
    range: {
      start: { line: number; character: number };
      end: { line: number; character: number };
    };
    type: string;
    modifiers: number;
  }>;
};

export async function startClient(
  ctx: vscode.ExtensionContext,
  serverPath: string,
) {
  const isDebug =
    process.env.VSCODE_DEBUG_MODE === "true" ||
    ctx.extensionMode === vscode.ExtensionMode.Development;

  const resolved = discoverServerPath(ctx, serverPath, isDebug);
  console.log("[MoZuku] 最終的に解決されたサーバーパス:", resolved);

  if (!fs.existsSync(resolved)) {
    const msg = `MoZuku LSPサーバーが見つかりません: ${resolved}。先にLSPサーバーをビルドしてください。`;
    console.error("[MoZuku]", msg);
    vscode.window.showErrorMessage(msg);
    throw new Error(msg);
  }

  const serverOptions: ServerOptions = {
    run: {
      command: resolved,
      transport: TransportKind.stdio,
      options: {
        env: isDebug ? { ...process.env, MOZUKU_DEBUG: "1" } : process.env,
      },
    },
    debug: {
      command: resolved,
      transport: TransportKind.stdio,
      options: { env: { ...process.env, MOZUKU_DEBUG: "1" } },
    },
  };

  const initOptions = buildInitializationOptions();

  if (isDebug) {
    console.log(
      "[MoZuku] LSP初期化オプション:",
      JSON.stringify(initOptions, null, 2),
    );
  }

  const clientOptions: LanguageClientOptions = {
    documentSelector: buildDocumentSelector(),
    synchronize: {
      fileEvents: vscode.workspace.createFileSystemWatcher("**/*"),
    },
    initializationOptions: initOptions,
    middleware: {},
  };

  const client = new LanguageClient(
    "mozuku",
    "MoZuku LSP",
    serverOptions,
    clientOptions,
  );

  const semanticHighlights = new Map<string, Map<string, vscode.Range[]>>();
  const commentHighlights = new Map<string, vscode.Range[]>();
  const contentHighlights = new Map<string, vscode.Range[]>();

  const semanticColors: Record<string, string> = {
    noun: "#c8c8c8",
    verb: "#569cd6",
    adjective: "#4fc1ff",
    adverb: "#9cdcfe",
    particle: "#d16969",
    aux: "#87ceeb",
    conjunction: "#d7ba7d",
    symbol: "#808080",
    interj: "#b5cea8",
    prefix: "#c8c8c8",
    suffix: "#c8c8c8",
    unknown: "#aaaaaa",
  };

  const semanticDecorationTypes = new Map<
    string,
    vscode.TextEditorDecorationType
  >();
  const commentDecorationType = vscode.window.createTextEditorDecorationType(
    {},
  );
  const contentDecorationType = vscode.window.createTextEditorDecorationType(
    {},
  );
  ctx.subscriptions.push(commentDecorationType, contentDecorationType);

  const getSemanticDecorationType = (tokenType: string) => {
    if (!semanticDecorationTypes.has(tokenType)) {
      const color = semanticColors[tokenType] ?? "#cccccc";
      const decoration = vscode.window.createTextEditorDecorationType({
        color,
      });
      semanticDecorationTypes.set(tokenType, decoration);
      ctx.subscriptions.push(decoration);
    }
    return semanticDecorationTypes.get(tokenType)!;
  };

  const applyDecorationsToEditor = (editor: vscode.TextEditor | undefined) => {
    if (!editor) {
      return;
    }
    const uri = editor.document.uri.toString();

    const semanticByType = semanticHighlights.get(uri);
    if (semanticByType) {
      for (const [tokenType, ranges] of semanticByType) {
        const decoration = getSemanticDecorationType(tokenType);
        editor.setDecorations(decoration, ranges);
      }
    }
    for (const [tokenType, decoration] of semanticDecorationTypes) {
      if (!semanticByType || !semanticByType.has(tokenType)) {
        editor.setDecorations(decoration, []);
      }
    }

    const commentRanges = commentHighlights.get(uri) ?? [];
    editor.setDecorations(commentDecorationType, commentRanges);

    const contentRanges = contentHighlights.get(uri) ?? [];
    const hasSemantic = semanticByType && semanticByType.size > 0;
    if (contentRanges.length > 0 && !hasSemantic) {
      editor.setDecorations(contentDecorationType, contentRanges);
    } else {
      editor.setDecorations(contentDecorationType, []);
    }
  };

  const applyDecorationsForUri = (uri: string) => {
    for (const editor of vscode.window.visibleTextEditors) {
      if (editor.document.uri.toString() === uri) {
        applyDecorationsToEditor(editor);
      }
    }
  };

  const applyDecorationsToVisibleEditors = () => {
    for (const editor of vscode.window.visibleTextEditors) {
      applyDecorationsToEditor(editor);
    }
  };

  client.onDidChangeState((event) => {
    if (isDebug) {
      console.log(
        `[MoZuku] クライアント状態変更: ${State[event.oldState]} -> ${State[event.newState]}`,
      );
    }
    if (event.newState === State.Running) {
      console.log("[MoZuku] LSPクライアントが起動しました");
    } else if (event.newState === State.Stopped) {
      console.error("[MoZuku] LSPクライアントが停止しました");
      if (event.oldState === State.Running) {
        vscode.window.showErrorMessage(
          "MoZuku LSPサーバーが予期せず停止しました。サーバー実行ファイルを確認してください。",
        );
      }
    }
  });

  client.onNotification(
    "mozuku/commentHighlights",
    (payload: CommentHighlightMessage) => {
      const { uri, ranges = [] } = payload;
      const vsRanges = ranges.map((r) => {
        const start = new vscode.Position(r.start.line, r.start.character);
        const end = new vscode.Position(r.end.line, r.end.character);
        return new vscode.Range(start, end);
      });
      if (vsRanges.length === 0) {
        commentHighlights.delete(uri);
      } else {
        commentHighlights.set(uri, vsRanges);
      }
      applyDecorationsForUri(uri);
    },
  );

  client.onNotification(
    "mozuku/contentHighlights",
    (payload: ContentHighlightMessage) => {
      const { uri, ranges = [] } = payload;
      const vsRanges = ranges.map((r) => {
        const start = new vscode.Position(r.start.line, r.start.character);
        const end = new vscode.Position(r.end.line, r.end.character);
        return new vscode.Range(start, end);
      });
      if (vsRanges.length === 0) {
        contentHighlights.delete(uri);
      } else {
        contentHighlights.set(uri, vsRanges);
      }
      applyDecorationsForUri(uri);
    },
  );

  client.onNotification(
    "mozuku/semanticHighlights",
    (payload: SemanticHighlightMessage) => {
      const { uri, tokens = [] } = payload;
      if (tokens.length === 0) {
        semanticHighlights.delete(uri);
        applyDecorationsForUri(uri);
        return;
      }

      const perType = new Map<string, vscode.Range[]>();
      for (const token of tokens) {
        const start = new vscode.Position(
          token.range.start.line,
          token.range.start.character,
        );
        const end = new vscode.Position(
          token.range.end.line,
          token.range.end.character,
        );
        const range = new vscode.Range(start, end);

        const decoration = getSemanticDecorationType(token.type);
        if (!perType.has(token.type)) {
          perType.set(token.type, []);
        }
        perType.get(token.type)!.push(range);

        void decoration;
      }

      semanticHighlights.set(uri, perType);
      applyDecorationsForUri(uri);
    },
  );

  if (isDebug) {
    client.outputChannel.show();
    console.log("[MoZuku] デバッグのためLSPクライアント出力チャンネルを表示");
  }

  ctx.subscriptions.push(client);

  try {
    await client.start();
    if (isDebug) {
      console.log("[MoZuku] LSPクライアントの起動に成功しました");
    }

    applyDecorationsToVisibleEditors();

    const openDisposable = vscode.workspace.onDidOpenTextDocument((doc) => {
      console.log("[MoZuku] ドキュメントを開きました:", {
        uri: doc.uri.toString(),
        languageId: doc.languageId,
        fileName: doc.fileName,
      });
      applyDecorationsForUri(doc.uri.toString());
    });

    const activeEditorDisposable = vscode.window.onDidChangeActiveTextEditor(
      (editor) => {
        if (editor) {
          console.log("[MoZuku] アクティブエディタが変更されました:", {
            uri: editor.document.uri.toString(),
            languageId: editor.document.languageId,
            fileName: editor.document.fileName,
          });
        }
        applyDecorationsToEditor(editor ?? undefined);
      },
    );

    const visibleEditorsDisposable =
      vscode.window.onDidChangeVisibleTextEditors(() => {
        applyDecorationsToVisibleEditors();
      });

    const closeDisposable = vscode.workspace.onDidCloseTextDocument((doc) => {
      const uri = doc.uri.toString();
      semanticHighlights.delete(uri);
      commentHighlights.delete(uri);
      contentHighlights.delete(uri);
      applyDecorationsForUri(uri);
    });

    ctx.subscriptions.push(
      openDisposable,
      activeEditorDisposable,
      visibleEditorsDisposable,
      closeDisposable,
    );
  } catch (error) {
    console.error("[MoZuku] LSPクライアントの起動に失敗しました:", error);
    vscode.window.showErrorMessage(`MoZuku LSPの起動に失敗: ${error}`);
    throw error;
  }

  return client;
}

export function resolveServerPath(
  ctx: vscode.ExtensionContext,
  configured: string,
): string {
  const isDebug =
    process.env.VSCODE_DEBUG_MODE === "true" ||
    ctx.extensionMode === vscode.ExtensionMode.Development;
  return discoverServerPath(ctx, configured, isDebug);
}
