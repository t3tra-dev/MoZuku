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
exports.activate = activate;
exports.deactivate = deactivate;
// VS Code拡張APIを使用したMoZuku拡張のエントリポイント
const vscode = __importStar(require("vscode"));
const client_1 = require("./client");
/**
 * 拡張機能のアクティベーション処理
 * 初回コマンド実行時またはstartupFinished時に呼び出される
 */
async function activate(context) {
    console.log('[MoZuku] 拡張機能をアクティベート中...');
    // LSPクライアントを起動
    const serverPath = vscode.workspace.getConfiguration('mozuku').get('serverPath') ||
        'mozuku-lsp';
    console.log('[MoZuku] LSPクライアント起動: サーバーパス =', serverPath);
    const client = await (0, client_1.startClient)(context, serverPath);
    console.log('[MoZuku] 拡張機能のアクティベートが完了しました');
    // デモコマンドを保持（将来削除予定）
    const disposable = vscode.commands.registerCommand('mozuku.helloWorld', () => {
        vscode.window.showInformationMessage('MoZukuからこんにちは！');
    });
    context.subscriptions.push(disposable);
}
/**
 * 拡張機能の非アクティベーション処理
 */
function deactivate() {
    // クリーンアップ処理（必要に応じて追加）
}
//# sourceMappingURL=extension.js.map