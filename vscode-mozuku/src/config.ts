import * as vscode from "vscode";
import type { LanguageClientOptions } from "vscode-languageclient/node";

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

export function buildInitializationOptions() {
  const config = vscode.workspace.getConfiguration("mozuku");

  return {
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
}

export function buildDocumentSelector(): NonNullable<
  LanguageClientOptions["documentSelector"]
> {
  return [
    ...supportedLanguages.map((language) => ({ language })),
    { scheme: "file", pattern: "**/*.ja.txt" },
    { scheme: "file", pattern: "**/*.ja.md" },
  ];
}
