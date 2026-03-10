#!/usr/bin/env node

const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");

const isWindows = process.platform === "win32";
const isLinux = process.platform === "linux";
const isDarwin = process.platform === "darwin";

// Define target platforms and architectures
const targets = [
  {
    platform: "darwin",
    arch: "arm64",
    enabled: isDarwin && process.arch === "arm64",
  },
  {
    platform: "darwin",
    arch: "x64",
    enabled: isDarwin && process.arch === "x64",
  },
  {
    platform: "linux",
    arch: "arm64",
    enabled: isLinux && process.arch === "arm64",
  },
  {
    platform: "linux",
    arch: "x64",
    enabled: isLinux && process.arch === "x64",
  },
  { platform: "win32", arch: "x64", enabled: isWindows },
];

function log(message) {
  console.log(`[Build LSP] ${message}`);
}

function ensureDir(dirPath) {
  if (!fs.existsSync(dirPath)) {
    fs.mkdirSync(dirPath, { recursive: true });
  }
}

function runCMake(args, cwd) {
  execFileSync("cmake", args, {
    stdio: "inherit",
    cwd,
  });
}

function buildMoZukuBinary(lspSourceDir, installPrefix) {
  log("Building LSP server...");

  try {
    if (!fs.existsSync(path.join(lspSourceDir, "build"))) {
      log("Creating build directory...");
      fs.mkdirSync(path.join(lspSourceDir, "build"), { recursive: true });
    }

    log("Configuring with CMake...");
    runCMake(["-B", "build", "-DCMAKE_BUILD_TYPE=Release"], lspSourceDir);

    log("Building with CMake...");
    runCMake(
      ["--build", "build", "--config", "Release", "--parallel", "4"],
      lspSourceDir,
    );

    log(`Installing with CMake into: ${installPrefix}`);
    runCMake(
      ["--install", "build", "--config", "Release", "--prefix", installPrefix],
      lspSourceDir,
    );
  } catch (error) {
    log(`Build failed: ${error.message}`);
    throw error;
  }
}

function buildForCurrentPlatform() {
  const currentTarget = targets.find((t) => t.enabled);
  const originalCwd = process.cwd();

  if (!currentTarget) {
    throw new Error(
      `Unsupported platform: ${process.platform}-${process.arch}`,
    );
  }

  log(
    `Building for current platform: ${currentTarget.platform}-${currentTarget.arch}`,
  );
  log(
    "⚠️  NOTE: This build requires system MeCab/CaboCha/CURL libraries to be installed",
  );
  log("Installation instructions:");
  log("  macOS: brew install mecab mecab-ipadic cabocha curl");
  log(
    "  Ubuntu: sudo apt install mecab libmecab-dev mecab-ipadic-utf8 cabocha libcabocha-dev libcurl4-openssl-dev",
  );
  log("  Other: See MoZuku documentation");

  const lspSourceDir = path.join(__dirname, "..", "mozuku-lsp");
  const binDir = path.join(__dirname, "bin");
  const installPrefix = __dirname;

  // Clean previous build
  if (fs.existsSync(binDir)) {
    log("Cleaning previous bin directory...");
    fs.rmSync(binDir, { recursive: true, force: true });
  }

  // ensure directories exist
  ensureDir(binDir);

  // Nixによるビルド成果物がない場合はビルドを行う
  if (fs.existsSync(path.join(__dirname, "..", "result", "bin"))) {
    const exeName =
      currentTarget.platform === "win32" ? "mozuku-lsp.exe" : "mozuku-lsp";
    const nixExecutable = path.join(__dirname, "..", "result", "bin", exeName);
    const targetExecutable = path.join(binDir, exeName);
    log(`Copying Nix executable: ${nixExecutable} -> ${targetExecutable}`);
    fs.copyFileSync(nixExecutable, targetExecutable);
  } else {
    buildMoZukuBinary(lspSourceDir, installPrefix);
  }

  const exeName =
    currentTarget.platform === "win32" ? "mozuku-lsp.exe" : "mozuku-lsp";
  const targetExecutable = path.join(binDir, exeName);
  if (!fs.existsSync(targetExecutable)) {
    throw new Error(`Installed executable not found at: ${targetExecutable}`);
  }

  // Make executable on Unix-like systems
  if (currentTarget.platform !== "win32") {
    fs.chmodSync(targetExecutable, 0o755);
  }

  // Restore original working directory
  process.chdir(originalCwd);

  log(
    `Successfully built LSP server for ${currentTarget.platform}-${currentTarget.arch}`,
  );
  log(`Executable location: ${targetExecutable}`);
  log("");
  log("⚠️  IMPORTANT: System libraries required at runtime:");
  log("  - MeCab (with dictionary)");
  log("  - CaboCha (optional, for advanced features)");
  log("  - CRF++ (dependency of CaboCha)");
  log("  - CURL (for Wikipedia integration)");
}

// Create server metadata
function createServerMetadata() {
  const currentTarget = targets.find((t) => t.enabled);
  const metadataPath = path.join(__dirname, "metadata.json");

  const metadata = {
    buildTime: new Date().toISOString(),
    platform: currentTarget.platform,
    arch: currentTarget.arch,
    version: require("./package.json").version,
    systemLibraries: {
      required: ["MeCab", "CURL"],
      optional: ["CaboCha", "CRF++"],
      note: "System libraries must be installed on target system",
    },
  };

  ensureDir(path.dirname(metadataPath));
  fs.writeFileSync(metadataPath, JSON.stringify(metadata, null, 2));
  log(`Created server metadata: ${metadataPath}`);
}

// Main execution
if (require.main === module) {
  try {
    log("Starting system-based LSP server build...");
    buildForCurrentPlatform();
    createServerMetadata();
    log("System-based LSP server build completed successfully!");
  } catch (error) {
    console.error(`Build failed: ${error.message}`);
    process.exit(1);
  }
}

module.exports = { buildForCurrentPlatform, createServerMetadata };
