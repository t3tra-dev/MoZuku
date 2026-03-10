import * as vscode from "vscode";
import * as fs from "fs";
import * as path from "path";
import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
  TransportKind,
  State,
} from "vscode-languageclient/node";

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

const supportedLanguages = [
  "japanese",
  "c",
  "cpp",
  "html",
  "python",
  "javascript",
  "javascriptreact",
  "typescript",
  "typescriptreact",
  "rust",
  "html",
  "latex",
];

export async function startClient(
  ctx: vscode.ExtensionContext,
  serverPath: string,
) {
  const isDebug =
    process.env.VSCODE_DEBUG_MODE === "true" ||
    ctx.extensionMode === vscode.ExtensionMode.Development;

  const resolved = resolveServerPath(ctx, serverPath);
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

  const config = vscode.workspace.getConfiguration("mozuku");
  const initOptions = {
    mozuku: {
      mecab: {
        dicdir: config.get<string>("mecab.dicdir", ""),
        charset: config.get<string>("mecab.charset", "UTF-8"),
      },
      analysis: {
        enableCaboCha: config.get<boolean>("analysis.enableCaboCha", true),
        grammarCheck: config.get<boolean>("analysis.grammarCheck", true),
        minJapaneseRatio: config.get<number>("analysis.minJapaneseRatio", 0.1),
        warningMinSeverity: config.get<number>(
          "analysis.warningMinSeverity",
          2,
        ),
        warnings: {
          particleDuplicate: config.get<boolean>(
            "analysis.warnings.particleDuplicate",
            true,
          ),
          particleSequence: config.get<boolean>(
            "analysis.warnings.particleSequence",
            true,
          ),
          particleMismatch: config.get<boolean>(
            "analysis.warnings.particleMismatch",
            true,
          ),
          sentenceStructure: config.get<boolean>(
            "analysis.warnings.sentenceStructure",
            false,
          ),
          styleConsistency: config.get<boolean>(
            "analysis.warnings.styleConsistency",
            false,
          ),
          redundancy: config.get<boolean>(
            "analysis.warnings.redundancy",
            false,
          ),
        },
        rules: {
          commaLimit: config.get<boolean>("analysis.rules.commaLimit", true),
          adversativeGa: config.get<boolean>(
            "analysis.rules.adversativeGa",
            true,
          ),
          duplicateParticleSurface: config.get<boolean>(
            "analysis.rules.duplicateParticleSurface",
            true,
          ),
          adjacentParticles: config.get<boolean>(
            "analysis.rules.adjacentParticles",
            true,
          ),
          conjunctionRepeat: config.get<boolean>(
            "analysis.rules.conjunctionRepeat",
            true,
          ),
          raDropping: config.get<boolean>("analysis.rules.raDropping", true),
          commaLimitMax: config.get<number>("analysis.rules.commaLimitMax", 3),
          adversativeGaMax: config.get<number>(
            "analysis.rules.adversativeGaMax",
            1,
          ),
          duplicateParticleSurfaceMaxRepeat: config.get<number>(
            "analysis.rules.duplicateParticleSurfaceMaxRepeat",
            1,
          ),
          adjacentParticlesMaxRepeat: config.get<number>(
            "analysis.rules.adjacentParticlesMaxRepeat",
            1,
          ),
          conjunctionRepeatMax: config.get<number>(
            "analysis.rules.conjunctionRepeatMax",
            1,
          ),
        },
      },
    },
  };

  if (isDebug) {
    console.log(
      "[MoZuku] LSP初期化オプション:",
      JSON.stringify(initOptions, null, 2),
    );
  }

  const documentSelector = [
    ...supportedLanguages.map((language) => ({ language })),
    { scheme: "file", pattern: "**/*.ja.txt" },
    { scheme: "file", pattern: "**/*.ja.md" },
  ];

  const clientOptions: LanguageClientOptions = {
    documentSelector,
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

function resolveServerPath(
  ctx: vscode.ExtensionContext,
  configured: string,
): string {
  const isWindows = process.platform === "win32";
  const exeName = isWindows ? "mozuku-lsp.exe" : "mozuku-lsp";
  const isDebug =
    process.env.VSCODE_DEBUG_MODE === "true" ||
    ctx.extensionMode === vscode.ExtensionMode.Development;
  const configuredValue = configured.trim();
  const envValue = process.env.MOZUKU_LSP?.trim() ?? "";
  const workspaceRoot = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
  const extensionRoot = ctx.extensionUri.fsPath;
  const seen = new Set<string>();

  if (isDebug) {
    console.log("[MoZuku] サーバーパスを解決中:", {
      configured: configuredValue,
      extensionPath: extensionRoot,
      workspaceFolders: vscode.workspace.workspaceFolders?.map(
        (f) => f.uri.fsPath,
      ),
    });
  }

  const candidates: { type: string; path: string }[] = [];
  const add = (type: string, p: string | undefined) => {
    if (!p || p.trim().length === 0) {
      return;
    }
    const normalized = path.normalize(p);
    if (seen.has(normalized)) {
      return;
    }
    seen.add(normalized);
    candidates.push({ type, path: p });
  };
  const addResolvedPath = (type: string, candidate: string | undefined) => {
    if (!candidate) {
      return;
    }
    if (path.isAbsolute(candidate)) {
      add(type, candidate);
      return;
    }
    if (workspaceRoot) {
      add(`${type} (workspace)`, path.join(workspaceRoot, candidate));
    }
    add(`${type} (extension)`, path.join(extensionRoot, candidate));
    add(`${type} (cwd)`, path.resolve(candidate));
  };
  const installDirs = (): string[] => {
    const dirs: string[] = [];
    const pathEnv = process.env.PATH || "";
    for (const dir of pathEnv.split(path.delimiter)) {
      if (dir) {
        dirs.push(dir);
      }
    }

    const home = process.env.HOME || process.env.USERPROFILE;
    if (home) {
      dirs.push(path.join(home, ".local", "bin"));
      dirs.push(path.join(home, "bin"));
    }

    if (isWindows) {
      const localAppData = process.env.LOCALAPPDATA;
      if (localAppData) {
        dirs.push(path.join(localAppData, "Programs", "MoZuku", "bin"));
        dirs.push(path.join(localAppData, "Programs", "mozuku-lsp", "bin"));
      }
      for (const base of [
        process.env.ProgramFiles,
        process.env["ProgramFiles(x86)"],
      ]) {
        if (!base) {
          continue;
        }
        dirs.push(path.join(base, "MoZuku", "bin"));
        dirs.push(path.join(base, "mozuku-lsp", "bin"));
      }
    } else {
      dirs.push("/usr/local/bin");
      dirs.push("/usr/bin");
      if (process.platform === "darwin") {
        dirs.push("/opt/homebrew/bin");
        dirs.push("/opt/local/bin");
      }
    }

    return dirs;
  };
  const addCommandSearch = (type: string, commandName: string | undefined) => {
    if (!commandName || hasPathSep(commandName)) {
      return;
    }
    const names =
      isWindows && !commandName.toLowerCase().endsWith(".exe")
        ? [commandName, `${commandName}.exe`]
        : [commandName];
    for (const dir of installDirs()) {
      for (const name of names) {
        add(type, path.join(dir, name));
      }
    }
  };

  if (configuredValue && hasPathSep(configuredValue)) {
    addResolvedPath("設定済み", configuredValue);
  }
  if (envValue && hasPathSep(envValue)) {
    addResolvedPath("環境変数 MOZUKU_LSP", envValue);
  }

  addCommandSearch(
    "設定済みコマンド",
    configuredValue && !hasPathSep(configuredValue)
      ? configuredValue
      : undefined,
  );
  addCommandSearch(
    "環境変数 MOZUKU_LSP",
    envValue && !hasPathSep(envValue) ? envValue : undefined,
  );
  addCommandSearch("デフォルトコマンド", exeName);

  add(
    "パッケージ済み",
    vscode.Uri.joinPath(ctx.extensionUri, "bin", exeName).fsPath,
  );

  const plat = process.platform;
  const arch = process.arch;
  add(
    "パッケージ済み",
    vscode.Uri.joinPath(
      ctx.extensionUri,
      "server",
      "bin",
      `${plat}-${arch}`,
      exeName,
    ).fsPath,
  );

  if (workspaceRoot) {
    add(
      "ワークスペース-install",
      path.join(workspaceRoot, "build", "install", "bin", exeName),
    );
    add("ワークスペース-build", path.join(workspaceRoot, "build", exeName));
    add(
      "ワークスペース-install",
      path.join(
        workspaceRoot,
        "mozuku-lsp",
        "build",
        "install",
        "bin",
        exeName,
      ),
    );
    add(
      "ワークスペース-build",
      path.join(workspaceRoot, "mozuku-lsp", "build", exeName),
    );
  }

  add(
    "開発-install",
    path.join(
      extensionRoot,
      "..",
      "mozuku-lsp",
      "build",
      "install",
      "bin",
      exeName,
    ),
  );
  add(
    "開発-build",
    path.join(extensionRoot, "..", "mozuku-lsp", "build", exeName),
  );

  for (const candidate of candidates) {
    if (fs.existsSync(candidate.path)) {
      if (isDebug) {
        console.log(`[MoZuku] ${candidate.type}パスを使用:`, candidate.path);
      }
      return candidate.path;
    } else if (isDebug) {
      console.log(
        `[MoZuku] ${candidate.type}パスが見つかりません:`,
        candidate.path,
      );
    }
  }

  const fallback = configuredValue || envValue || exeName;
  if (isDebug) {
    console.log("[MoZuku] フォールバックパスを使用:", fallback);
  }
  return fallback;
}

function hasPathSep(p: string): boolean {
  return p.includes("/") || p.includes("\\");
}
