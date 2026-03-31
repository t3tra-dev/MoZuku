package dev.t3tra.mozuku.idea.lsp

import com.intellij.ide.plugins.PluginManagerCore
import com.intellij.openapi.project.Project
import com.intellij.openapi.util.SystemInfo
import com.intellij.openapi.extensions.PluginId

import java.io.File
import java.nio.file.{Files, Path, Paths}
import scala.collection.mutable

object MoZukuServerPathResolver:
  def resolve(project: Project, configured: String): Path =
    val trimmedConfigured = Option(configured).getOrElse("").trim
    val envValue = Option(System.getenv("MOZUKU_LSP")).getOrElse("").trim
    val commandName = executableName("mozuku-lsp")
    val seen = mutable.LinkedHashSet.empty[Path]
    val candidates = mutable.ArrayBuffer.empty[Path]

    def add(candidate: Path | Null): Unit =
      Option(candidate)
        .map(_.normalize())
        .filterNot(seen.contains)
        .foreach { path =>
          seen += path
          candidates += path
        }

    def addResolved(raw: String): Unit =
      if raw.nonEmpty then
        val path = Paths.get(raw)
        if path.isAbsolute then add(path)
        else
          Option(project.getBasePath).foreach(base =>
            add(Paths.get(base).resolve(raw))
          )
          pluginBasePath().foreach(base => add(base.resolve(raw)))
          add(path.toAbsolutePath)

    def addCommandSearch(raw: String): Unit =
      if raw.nonEmpty && !hasPathSeparator(raw) then
        installDirectories().foreach(dir =>
          add(dir.resolve(executableName(raw)))
        )

    if trimmedConfigured.nonEmpty && hasPathSeparator(trimmedConfigured) then
      addResolved(trimmedConfigured)
    if envValue.nonEmpty && hasPathSeparator(envValue) then
      addResolved(envValue)

    pluginBasePath().foreach { base =>
      add(base.resolve("bin").resolve(commandName))
      add(
        base
          .resolve("server")
          .resolve("bin")
          .resolve(s"${SystemInfo.OS_NAME.toLowerCase}-${SystemInfo.OS_ARCH}")
          .resolve(commandName)
      )
    }

    Option(project.getBasePath).foreach { base =>
      val root = Paths.get(base)
      add(root.resolve("mozuku-lsp").resolve("build").resolve(commandName))
      add(
        root
          .resolve("mozuku-lsp")
          .resolve("build")
          .resolve("install")
          .resolve("bin")
          .resolve(commandName)
      )
      add(root.resolve("build").resolve(commandName))
      add(
        root
          .resolve("build")
          .resolve("install")
          .resolve("bin")
          .resolve(commandName)
      )
    }

    addCommandSearch(trimmedConfigured)
    addCommandSearch(envValue)
    addCommandSearch("mozuku-lsp")

    candidates
      .find(Files.isRegularFile(_))
      .getOrElse(
        Paths.get(
          if trimmedConfigured.nonEmpty then trimmedConfigured
          else if envValue.nonEmpty then envValue
          else "mozuku-lsp"
        )
      )

  private def pluginBasePath(): Option[Path] =
    Option(PluginManagerCore.getPlugin(PluginId.getId("dev.t3tra.mozuku.idea")))
      .map(_.getPluginPath)

  private def hasPathSeparator(value: String): Boolean =
    value.contains("/") || value.contains("\\")

  private def executableName(base: String): String =
    if SystemInfo.isWindows && !base.toLowerCase.endsWith(".exe") then
      s"$base.exe"
    else base

  private def installDirectories(): Seq[Path] =
    val dirs = mutable.ArrayBuffer.empty[Path]
    Option(System.getenv("PATH")).toSeq
      .flatMap(_.split(File.pathSeparator).toSeq)
      .filter(_.nonEmpty)
      .foreach(entry => dirs += Paths.get(entry))

    Option(System.getProperty("user.home")).foreach { home =>
      dirs += Paths.get(home, ".local", "bin")
      dirs += Paths.get(home, "bin")
    }

    if SystemInfo.isMac then
      dirs ++= Seq(
        "/usr/local/bin",
        "/usr/bin",
        "/opt/homebrew/bin",
        "/opt/local/bin"
      ).map(Paths.get(_))
    else if SystemInfo.isUnix then
      dirs ++= Seq("/usr/local/bin", "/usr/bin").map(Paths.get(_))
    else if SystemInfo.isWindows then
      Seq("LOCALAPPDATA", "ProgramFiles", "ProgramFiles(x86)")
        .flatMap(key => Option(System.getenv(key)))
        .map(Paths.get(_))
        .foreach { base =>
          dirs += base.resolve("MoZuku").resolve("bin")
          dirs += base.resolve("mozuku-lsp").resolve("bin")
        }

    dirs.toSeq.distinct
