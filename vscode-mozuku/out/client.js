"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.startClient = startClient;
const vscode = __importStar(require("vscode"));
const fs = __importStar(require("fs"));
const path = __importStar(require("path"));
const node_1 = require("vscode-languageclient/node");
const supportedLanguages = [
    'japanese',
    'c',
    'cpp',
    'html',
    'python',
    'javascript',
    'javascriptreact',
    'typescript',
    'typescriptreact',
    'rust',
    'html',
    'latex',
];
/**
 * LSPクライアントを起動し、設定を初期化
 * @param ctx 拡張機能コンテキスト
 * @param serverPath サーバーバイナリのパス
 * @returns 起動されたLanguageClientインスタンス
 */
async function startClient(ctx, serverPath) {
    // 開発モード時のみデバッグログを有効化
    const isDebug = process.env.VSCODE_DEBUG_MODE === 'true' || ctx.extensionMode === vscode.ExtensionMode.Development;
    // サーバーパスを解決
    const resolved = resolveServerPath(ctx, serverPath);
    console.log('[MoZuku] 最終的に解決されたサーバーパス:', resolved);
    // サーバーバイナリの存在チェック
    if (!fs.existsSync(resolved)) {
        const msg = `MoZuku LSPサーバーが見つかりません: ${resolved}。先にLSPサーバーをビルドしてください。`;
        console.error('[MoZuku]', msg);
        vscode.window.showErrorMessage(msg);
        throw new Error(msg);
    }
    // サーバー起動オプションの設定
    const serverOptions = {
        run: {
            command: resolved,
            transport: node_1.TransportKind.stdio,
            options: { env: isDebug ? { ...process.env, MOZUKU_DEBUG: '1' } : process.env }
        },
        debug: {
            command: resolved,
            transport: node_1.TransportKind.stdio,
            options: { env: { ...process.env, MOZUKU_DEBUG: '1' } }
        },
    };
    // LSP初期化用の設定を取得
    const config = vscode.workspace.getConfiguration('mozuku');
    const initOptions = {
        mozuku: {
            mecab: {
                dicdir: config.get('mecab.dicdir', ''),
                charset: config.get('mecab.charset', 'UTF-8')
            },
            analysis: {
                enableCaboCha: config.get('analysis.enableCaboCha', true),
                grammarCheck: config.get('analysis.grammarCheck', true),
                minJapaneseRatio: config.get('analysis.minJapaneseRatio', 0.1),
                warningMinSeverity: config.get('analysis.warningMinSeverity', 2),
                warnings: {
                    particleDuplicate: config.get('analysis.warnings.particleDuplicate', true),
                    particleSequence: config.get('analysis.warnings.particleSequence', true),
                    particleMismatch: config.get('analysis.warnings.particleMismatch', true),
                    sentenceStructure: config.get('analysis.warnings.sentenceStructure', false),
                    styleConsistency: config.get('analysis.warnings.styleConsistency', false),
                    redundancy: config.get('analysis.warnings.redundancy', false)
                }
            }
        }
    };
    if (isDebug) {
        console.log('[MoZuku] LSP初期化オプション:', JSON.stringify(initOptions, null, 2));
    }
    // クライアントオプションの設定
    const documentSelector = [
        ...supportedLanguages.map((language) => ({ language })),
        { scheme: 'file', pattern: '**/*.ja.txt' },
        { scheme: 'file', pattern: '**/*.ja.md' },
    ];
    const clientOptions = {
        documentSelector,
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*'),
        },
        initializationOptions: initOptions,
        middleware: {},
    };
    // Language Clientインスタンスを作成
    const client = new node_1.LanguageClient('mozuku', 'MoZuku LSP', serverOptions, clientOptions);
    const semanticHighlights = new Map();
    const commentHighlights = new Map();
    const contentHighlights = new Map();
    const semanticColors = {
        noun: '#c8c8c8',
        verb: '#569cd6',
        adjective: '#4fc1ff',
        adverb: '#9cdcfe',
        particle: '#d16969',
        aux: '#87ceeb',
        conjunction: '#d7ba7d',
        symbol: '#808080',
        interj: '#b5cea8',
        prefix: '#c8c8c8',
        suffix: '#c8c8c8',
        unknown: '#aaaaaa',
    };
    const semanticDecorationTypes = new Map();
    const commentDecorationType = vscode.window.createTextEditorDecorationType({
        color: '#f59e0b',
    });
    const contentDecorationType = vscode.window.createTextEditorDecorationType({});
    ctx.subscriptions.push(commentDecorationType, contentDecorationType);
    const getSemanticDecorationType = (tokenType) => {
        if (!semanticDecorationTypes.has(tokenType)) {
            const color = semanticColors[tokenType] ?? '#cccccc';
            const decoration = vscode.window.createTextEditorDecorationType({
                color,
            });
            semanticDecorationTypes.set(tokenType, decoration);
            ctx.subscriptions.push(decoration);
        }
        return semanticDecorationTypes.get(tokenType);
    };
    const applyDecorationsToEditor = (editor) => {
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
        }
        else {
            editor.setDecorations(contentDecorationType, []);
        }
    };
    const applyDecorationsForUri = (uri) => {
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
    // クライアント状態変化のハンドリング
    client.onDidChangeState((event) => {
        if (isDebug) {
            console.log(`[MoZuku] クライアント状態変更: ${node_1.State[event.oldState]} -> ${node_1.State[event.newState]}`);
        }
        if (event.newState === node_1.State.Running) {
            console.log('[MoZuku] LSPクライアントが起動しました');
        }
        else if (event.newState === node_1.State.Stopped) {
            console.error('[MoZuku] LSPクライアントが停止しました');
            if (event.oldState === node_1.State.Running) {
                vscode.window.showErrorMessage('MoZuku LSPサーバーが予期せず停止しました。サーバー実行ファイルを確認してください。');
            }
        }
    });
    client.onNotification('mozuku/commentHighlights', (payload) => {
        const { uri, ranges = [] } = payload;
        const vsRanges = ranges.map((r) => {
            const start = new vscode.Position(r.start.line, r.start.character);
            const end = new vscode.Position(r.end.line, r.end.character);
            return new vscode.Range(start, end);
        });
        if (vsRanges.length === 0) {
            commentHighlights.delete(uri);
        }
        else {
            commentHighlights.set(uri, vsRanges);
        }
        applyDecorationsForUri(uri);
    });
    client.onNotification('mozuku/contentHighlights', (payload) => {
        const { uri, ranges = [] } = payload;
        const vsRanges = ranges.map((r) => {
            const start = new vscode.Position(r.start.line, r.start.character);
            const end = new vscode.Position(r.end.line, r.end.character);
            return new vscode.Range(start, end);
        });
        if (vsRanges.length === 0) {
            contentHighlights.delete(uri);
        }
        else {
            contentHighlights.set(uri, vsRanges);
        }
        applyDecorationsForUri(uri);
    });
    client.onNotification('mozuku/semanticHighlights', (payload) => {
        const { uri, tokens = [] } = payload;
        if (tokens.length === 0) {
            semanticHighlights.delete(uri);
            applyDecorationsForUri(uri);
            return;
        }
        const perType = new Map();
        for (const token of tokens) {
            const start = new vscode.Position(token.range.start.line, token.range.start.character);
            const end = new vscode.Position(token.range.end.line, token.range.end.character);
            const range = new vscode.Range(start, end);
            const decoration = getSemanticDecorationType(token.type);
            if (!perType.has(token.type)) {
                perType.set(token.type, []);
            }
            perType.get(token.type).push(range);
            // Ensure decoration is kept alive
            void decoration;
        }
        semanticHighlights.set(uri, perType);
        applyDecorationsForUri(uri);
    });
    // デバッグ用にサーバー出力を表示
    if (isDebug) {
        client.outputChannel.show();
        console.log('[MoZuku] デバッグのためLSPクライアント出力チャンネルを表示');
    }
    ctx.subscriptions.push(client);
    // クライアントを起動
    try {
        await client.start();
        if (isDebug) {
            console.log('[MoZuku] LSPクライアントの起動に成功しました');
        }
        applyDecorationsToVisibleEditors();
        // デバッグ用のドキュメントイベントリスナーを追加
        const openDisposable = vscode.workspace.onDidOpenTextDocument((doc) => {
            console.log('[MoZuku] ドキュメントを開きました:', {
                uri: doc.uri.toString(),
                languageId: doc.languageId,
                fileName: doc.fileName
            });
            applyDecorationsForUri(doc.uri.toString());
        });
        const activeEditorDisposable = vscode.window.onDidChangeActiveTextEditor((editor) => {
            if (editor) {
                console.log('[MoZuku] アクティブエディタが変更されました:', {
                    uri: editor.document.uri.toString(),
                    languageId: editor.document.languageId,
                    fileName: editor.document.fileName
                });
            }
            applyDecorationsToEditor(editor ?? undefined);
        });
        const visibleEditorsDisposable = vscode.window.onDidChangeVisibleTextEditors(() => {
            applyDecorationsToVisibleEditors();
        });
        const closeDisposable = vscode.workspace.onDidCloseTextDocument((doc) => {
            const uri = doc.uri.toString();
            semanticHighlights.delete(uri);
            commentHighlights.delete(uri);
            contentHighlights.delete(uri);
            applyDecorationsForUri(uri);
        });
        ctx.subscriptions.push(openDisposable, activeEditorDisposable, visibleEditorsDisposable, closeDisposable);
    }
    catch (error) {
        console.error('[MoZuku] LSPクライアントの起動に失敗しました:', error);
        vscode.window.showErrorMessage(`MoZuku LSPの起動に失敗: ${error}`);
        throw error;
    }
    return client;
}
/**
 * サーバーバイナリのパスを複数の候補から解決
 * @param ctx 拡張機能コンテキスト
 * @param configured ユーザー設定のパス
 * @returns 解決されたサーバーバイナリの絶対パス
 */
function resolveServerPath(ctx, configured) {
    const isWindows = process.platform === 'win32';
    const exeName = isWindows ? 'mozuku-lsp.exe' : 'mozuku-lsp';
    // 診断のため現在はデバッグログを常に有効化
    const isDebug = true;
    if (isDebug) {
        console.log('[MoZuku] サーバーパスを解決中:', {
            configured,
            extensionPath: ctx.extensionUri.fsPath,
            workspaceFolders: vscode.workspace.workspaceFolders?.map(f => f.uri.fsPath)
        });
    }
    const candidates = [];
    // 1) 設定されたパスが存在する場合はそれを使用
    if (configured && hasPathSep(configured)) {
        candidates.push({ type: '設定済み', path: configured });
    }
    // 2) 拡張機能にパッケージされたバイナリ: <extension>/bin/mozuku-lsp（新統一構造）
    const primaryPackaged = vscode.Uri.joinPath(ctx.extensionUri, 'bin', exeName).fsPath;
    candidates.push({ type: 'パッケージ済み-統一版', path: primaryPackaged });
    // 2b) 旧拡張機能パッケージバイナリへのフォールバック: <extension>/server/bin/<platform>-<arch>/mozuku-lsp
    const plat = process.platform;
    const arch = process.arch;
    const legacyPackaged = vscode.Uri.joinPath(ctx.extensionUri, 'server', 'bin', `${plat}-${arch}`, exeName).fsPath;
    candidates.push({ type: 'パッケージ済み-レガシー', path: legacyPackaged });
    // 3) ワークスペース相対パスを試行（ワークスペースフォルダが存在する場合）
    const workspaceRoot = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
    if (workspaceRoot) {
        candidates.push({ type: 'ワークスペース-build', path: path.join(workspaceRoot, 'build', exeName) });
        candidates.push({ type: 'ワークスペース-lsp', path: path.join(workspaceRoot, 'mozuku-lsp', 'build', exeName) });
    }
    // 4) 開発環境用フォールバック
    candidates.push({ type: '開発-ルート', path: path.join(ctx.extensionUri.fsPath, '..', 'build', exeName) });
    candidates.push({ type: '開発-兄弟', path: path.join(ctx.extensionUri.fsPath, '..', 'mozuku-lsp', 'build', exeName) });
    // 候補を順番にチェック
    for (const candidate of candidates) {
        if (fs.existsSync(candidate.path)) {
            if (isDebug) {
                console.log(`[MoZuku] ${candidate.type}パスを使用:`, candidate.path);
            }
            return candidate.path;
        }
        else if (isDebug) {
            console.log(`[MoZuku] ${candidate.type}パスが見つかりません:`, candidate.path);
        }
    }
    // 5) 最終手段: PATH上に存在することを期待
    const fallback = configured || exeName;
    if (isDebug) {
        console.log('[MoZuku] フォールバックパスを使用:', fallback);
    }
    return fallback;
}
function hasPathSep(p) {
    return p.includes('/') || p.includes('\\');
}
//# sourceMappingURL=client.js.map