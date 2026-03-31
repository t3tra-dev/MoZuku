package dev.t3tra.mozuku.idea.lsp

import com.intellij.execution.ExecutionException
import com.intellij.execution.configurations.GeneralCommandLine
import com.intellij.openapi.project.Project
import com.intellij.openapi.vfs.VirtualFile
import com.intellij.platform.lsp.api.{
  Lsp4jClient,
  LspServerDescriptor,
  LspServerNotificationsHandler,
  LspServerSupportProvider,
  ProjectWideLspServerDescriptor
}
import org.eclipse.lsp4j.jsonrpc.services.JsonNotification

final class MoZukuLspServerSupportProvider extends LspServerSupportProvider:
  override def fileOpened(
      project: Project,
      file: VirtualFile,
      serverStarter: LspServerSupportProvider.LspServerStarter
  ): Unit =
    if MoZukuLanguageSupport.isSupported(file) then
      serverStarter.ensureServerStarted(new MoZukuLspServerDescriptor(project))

final class MoZukuLspServerDescriptor(project: Project)
    extends ProjectWideLspServerDescriptor(project, "MoZuku"):
  override def isSupportedFile(file: VirtualFile): Boolean =
    MoZukuLanguageSupport.isSupported(file)

  @throws[ExecutionException]
  override def createCommandLine(): GeneralCommandLine =
    val settings = MoZukuSettingsService.getInstance().snapshot()
    val command =
      MoZukuServerPathResolver.resolve(getProject, settings.serverPath)
    new GeneralCommandLine(command.toAbsolutePath.toString)

  override def getLanguageId(file: VirtualFile): String =
    MoZukuLanguageSupport.languageIdFor(file).orNull

  override def createInitializationOptions(): Object =
    MoZukuSettingsService.getInstance().initializationOptions()

  override def createLsp4jClient(
      handler: LspServerNotificationsHandler
  ): Lsp4jClient =
    new MoZukuLsp4jClient(handler, getProject)

final class MoZukuLsp4jClient(
    handler: LspServerNotificationsHandler,
    project: Project
) extends Lsp4jClient(handler):
  private val highlightService =
    project.getService(classOf[MoZukuHighlightService])

  @JsonNotification("mozuku/commentHighlights")
  def commentHighlights(payload: java.util.Map[String, Object]): Unit =
    highlightService.updateComment(
      MoZukuLspProtocol.parseContentRanges(payload)
    )

  @JsonNotification("mozuku/contentHighlights")
  def contentHighlights(payload: java.util.Map[String, Object]): Unit =
    highlightService.updateContent(
      MoZukuLspProtocol.parseContentRanges(payload)
    )

  @JsonNotification("mozuku/semanticHighlights")
  def semanticHighlights(payload: java.util.Map[String, Object]): Unit =
    highlightService.updateSemantic(
      MoZukuLspProtocol.parseSemanticHighlights(payload)
    )
