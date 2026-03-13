import * as fs from "fs";
import * as path from "path";
import * as vscode from "vscode";

export function resolveServerPath(
  ctx: vscode.ExtensionContext,
  configured: string,
  isDebug: boolean,
): string {
  const isWindows = process.platform === "win32";
  const exeName = isWindows ? "mozuku-lsp.exe" : "mozuku-lsp";
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
  const add = (type: string, candidatePath: string | undefined) => {
    if (!candidatePath || candidatePath.trim().length === 0) {
      return;
    }
    const normalized = path.normalize(candidatePath);
    if (seen.has(normalized)) {
      return;
    }
    seen.add(normalized);
    candidates.push({ type, path: candidatePath });
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

  add(
    "パッケージ済み",
    vscode.Uri.joinPath(ctx.extensionUri, "bin", exeName).fsPath,
  );

  add(
    "パッケージ済み",
    vscode.Uri.joinPath(
      ctx.extensionUri,
      "server",
      "bin",
      `${process.platform}-${process.arch}`,
      exeName,
    ).fsPath,
  );

  if (workspaceRoot) {
    // 開発ワークスペースでは nested `mozuku-lsp/build` が最新になりやすいので優先
    add(
      "ワークスペース-build",
      path.join(workspaceRoot, "mozuku-lsp", "build", exeName),
    );
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
      "ワークスペース-install",
      path.join(workspaceRoot, "build", "install", "bin", exeName),
    );
    add("ワークスペース-build", path.join(workspaceRoot, "build", exeName));
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

  // PATH 探索は最後: システムにインストール済みの古いバイナリより、
  // ワークスペース/拡張同梱の開発中バイナリを優先する
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

  for (const candidate of candidates) {
    if (fs.existsSync(candidate.path)) {
      if (isDebug) {
        console.log(`[MoZuku] ${candidate.type}パスを使用:`, candidate.path);
      }
      return candidate.path;
    }
    if (isDebug) {
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

function hasPathSep(candidate: string): boolean {
  return candidate.includes("/") || candidate.includes("\\");
}
