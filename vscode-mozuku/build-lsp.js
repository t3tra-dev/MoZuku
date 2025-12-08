#!/usr/bin/env node

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const isWindows = process.platform === 'win32';
const isLinux = process.platform === 'linux';
const isDarwin = process.platform === 'darwin';

// Define target platforms and architectures
const targets = [
  { platform: 'darwin', arch: 'arm64', enabled: isDarwin && process.arch === 'arm64' },
  { platform: 'darwin', arch: 'x64', enabled: isDarwin && process.arch === 'x64' },
  { platform: 'linux', arch: 'x64', enabled: isLinux },
  { platform: 'win32', arch: 'x64', enabled: isWindows },
];

function log(message) {
  console.log(`[Build LSP] ${message}`);
}

function ensureDir(dirPath) {
  if (!fs.existsSync(dirPath)) {
    fs.mkdirSync(dirPath, { recursive: true });
  }
}

function buildForCurrentPlatform() {
  const currentTarget = targets.find(t => t.enabled);
  if (!currentTarget) {
    throw new Error(`Unsupported platform: ${process.platform}-${process.arch}`);
  }

  log(`Building for current platform: ${currentTarget.platform}-${currentTarget.arch}`);
  log('⚠️  NOTE: This build requires system MeCab/CaboCha/CURL libraries to be installed');
  log('Installation instructions:');
  log('  macOS: brew install mecab mecab-ipadic cabocha curl');
  log('  Ubuntu: sudo apt install mecab libmecab-dev mecab-ipadic-utf8 cabocha libcabocha-dev libcurl4-openssl-dev');
  log('  Other: See MoZuku documentation');

  const lspSourceDir = path.join(__dirname, '..', 'mozuku-lsp');
  const binDir = path.join(__dirname, 'bin');
  
  // Clean previous build
  if (fs.existsSync(binDir)) {
    log('Cleaning previous bin directory...');
    fs.rmSync(binDir, { recursive: true, force: true });
  }

  // Ensure directories exist
  ensureDir(binDir);

  log('Building LSP server...');

  try {
    // Change to LSP source directory
    const originalCwd = process.cwd();
    process.chdir(lspSourceDir);

    // Configure and build
    if (!fs.existsSync('build')) {
      log('Creating build directory...');
      fs.mkdirSync('build');
    }

    // Configure CMake
    log('Configuring with CMake...');
    execSync('cmake -B build -DCMAKE_BUILD_TYPE=Release', { 
      stdio: 'inherit',
      cwd: lspSourceDir
    });

    // Build the project
    log('Building with CMake...');
    execSync('cmake --build build -j 4', { 
      stdio: 'inherit',
      cwd: lspSourceDir
    });

    // Package extension using CMake install target
    log('Packaging extension with CMake install target...');
    execSync('cmake --build build --target package-extension', { 
      stdio: 'inherit',
      cwd: lspSourceDir
    });

    // Copy the built executable
    const exeName = currentTarget.platform === 'win32' ? 'mozuku-lsp.exe' : 'mozuku-lsp';
    const builtExecutable = path.join(lspSourceDir, 'build', exeName);
    const targetExecutable = path.join(binDir, exeName);

    if (!fs.existsSync(builtExecutable)) {
      // Try alternative build location
      const altExecutable = path.join(lspSourceDir, exeName);
      if (fs.existsSync(altExecutable)) {
        log(`Copying executable from alternative location: ${altExecutable}`);
        fs.copyFileSync(altExecutable, targetExecutable);
      } else {
        throw new Error(`Built executable not found at: ${builtExecutable} or ${altExecutable}`);
      }
    } else {
      log(`Copying executable: ${builtExecutable} -> ${targetExecutable}`);
      fs.copyFileSync(builtExecutable, targetExecutable);
    }

    // Make executable on Unix-like systems
    if (currentTarget.platform !== 'win32') {
      fs.chmodSync(targetExecutable, 0o755);
    }

    // Restore original working directory
    process.chdir(originalCwd);

    log(`Successfully built LSP server for ${currentTarget.platform}-${currentTarget.arch}`);
    log(`Executable location: ${targetExecutable}`);
    log('');
    log('⚠️  IMPORTANT: System libraries required at runtime:');
    log('  - MeCab (with dictionary)');
    log('  - CaboCha (optional, for advanced features)');
    log('  - CRF++ (dependency of CaboCha)');
    log('  - CURL (for Wikipedia integration)');

  } catch (error) {
    log(`Build failed: ${error.message}`);
    throw error;
  }
}

// Create server metadata
function createServerMetadata() {
  const currentTarget = targets.find(t => t.enabled);
  const metadataPath = path.join(__dirname, 'metadata.json');
  
  const metadata = {
    buildTime: new Date().toISOString(),
    platform: currentTarget.platform,
    arch: currentTarget.arch,
    version: require('./package.json').version,
    systemLibraries: {
      required: ['MeCab', 'CURL'],
      optional: ['CaboCha', 'CRF++'],
      note: 'System libraries must be installed on target system'
    }
  };

  ensureDir(path.dirname(metadataPath));
  fs.writeFileSync(metadataPath, JSON.stringify(metadata, null, 2));
  log(`Created server metadata: ${metadataPath}`);
}

// Main execution
if (require.main === module) {
  try {
    log('Starting system-based LSP server build...');
    buildForCurrentPlatform();
    createServerMetadata();
    log('System-based LSP server build completed successfully!');
  } catch (error) {
    console.error(`Build failed: ${error.message}`);
    process.exit(1);
  }
}

module.exports = { buildForCurrentPlatform, createServerMetadata };