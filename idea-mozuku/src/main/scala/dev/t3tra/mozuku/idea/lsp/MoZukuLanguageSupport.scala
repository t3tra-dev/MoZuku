package dev.t3tra.mozuku.idea.lsp

import com.intellij.openapi.vfs.VirtualFile

object MoZukuLanguageSupport:
  private val explicitSuffixes = Map(
    ".ja.txt" -> "japanese",
    ".ja.md" -> "japanese"
  )

  private val extensionToLanguageId = Map(
    "c" -> "c",
    "h" -> "c",
    "cpp" -> "cpp",
    "cc" -> "cpp",
    "cxx" -> "cpp",
    "c++" -> "cpp",
    "hpp" -> "cpp",
    "hh" -> "cpp",
    "hxx" -> "cpp",
    "py" -> "python",
    "js" -> "javascript",
    "mjs" -> "javascript",
    "cjs" -> "javascript",
    "jsx" -> "javascriptreact",
    "ts" -> "typescript",
    "tsx" -> "typescriptreact",
    "rs" -> "rust",
    "html" -> "html",
    "htm" -> "html",
    "tex" -> "latex",
    "ltx" -> "latex"
  )

  def languageIdFor(file: VirtualFile): Option[String] =
    explicitSuffixes
      .collectFirst {
        case (suffix, languageId) if file.getName.endsWith(suffix) => languageId
      }
      .orElse(
        Option(file.getExtension)
          .flatMap(ext => extensionToLanguageId.get(ext.toLowerCase))
      )

  def isSupported(file: VirtualFile): Boolean =
    languageIdFor(file).isDefined
