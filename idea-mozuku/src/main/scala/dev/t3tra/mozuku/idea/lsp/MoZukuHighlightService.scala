package dev.t3tra.mozuku.idea.lsp

import com.intellij.openapi.Disposable
import com.intellij.openapi.application.{ApplicationManager, ReadAction}
import com.intellij.openapi.components.Service
import com.intellij.openapi.editor.event.{
  EditorFactoryEvent,
  EditorFactoryListener
}
import com.intellij.openapi.editor.ex.EditorEx
import com.intellij.openapi.editor.markup.{
  HighlighterLayer,
  HighlighterTargetArea,
  RangeHighlighter,
  TextAttributes
}
import com.intellij.openapi.editor.{Document, Editor, EditorFactory}
import com.intellij.openapi.fileEditor.{
  FileEditorManager,
  FileEditorManagerEvent,
  FileEditorManagerListener
}
import com.intellij.openapi.project.Project
import com.intellij.openapi.util.Key
import com.intellij.openapi.vfs.{VfsUtil, VirtualFile}

import java.awt.Color
import java.awt.Font
import java.net.URI
import java.nio.file.Path
import scala.jdk.CollectionConverters.*
import scala.jdk.OptionConverters.*

@Service(Array(Service.Level.PROJECT))
final class MoZukuHighlightService(project: Project) extends Disposable:
  private val highlightersKey =
    Key.create[java.util.ArrayList[RangeHighlighter]]("mozuku.highlighters")
  private var semanticState = Map.empty[String, Seq[SemanticTokenOverlay]]

  private val connection = project.getMessageBus.connect(this)
  connection.subscribe(
    FileEditorManagerListener.FILE_EDITOR_MANAGER,
    new FileEditorManagerListener:
      override def fileOpened(
          source: FileEditorManager,
          file: VirtualFile
      ): Unit =
        refreshFile(file)

      override def selectionChanged(event: FileEditorManagerEvent): Unit =
        Option(event.getNewFile).foreach(refreshFile)
  )

  EditorFactory
    .getInstance()
    .addEditorFactoryListener(
      new EditorFactoryListener:
        override def editorCreated(event: EditorFactoryEvent): Unit =
          Option(
            com.intellij.openapi.fileEditor.FileDocumentManager
              .getInstance()
              .getFile(event.getEditor.getDocument)
          ).foreach(refreshFile)
      ,
      this
    )

  def updateSemantic(payload: SemanticHighlightsPayload): Unit =
    val key = stateKeyForUri(payload.uri)
    val highlights = payload.tokens.map(token =>
      SemanticTokenOverlay(token.range, token.tokenType)
    )
    semanticState =
      if highlights.isEmpty then semanticState - key
      else semanticState.updated(key, highlights)
    refreshUri(payload.uri)

  def updateComment(payload: ContentRangesPayload): Unit =
    refreshUri(payload.uri)

  def updateContent(payload: ContentRangesPayload): Unit =
    refreshUri(payload.uri)

  private def refreshUri(uri: String): Unit =
    ApplicationManager.getApplication.invokeLater(() =>
      if !project.isDisposed then fileForUri(uri).foreach(refreshFile)
    )

  private def refreshFile(file: VirtualFile): Unit =
    val document =
      ReadAction.compute[java.util.Optional[Document], RuntimeException](() =>
        java.util.Optional.ofNullable(
          com.intellij.openapi.fileEditor.FileDocumentManager
            .getInstance()
            .getDocument(file)
        )
      )
    Option(document.orElse(null)).foreach { doc =>
      EditorFactory.getInstance().getEditors(doc, project).foreach {
        case editor: EditorEx => applySemanticHighlights(file, doc, editor)
        case _                =>
      }
    }

  private def applySemanticHighlights(
      file: VirtualFile,
      document: Document,
      editor: EditorEx
  ): Unit =
    clear(editor)
    val highlights = semanticState.getOrElse(stateKeyForFile(file), Seq.empty)
    if highlights.nonEmpty then
      val created = new java.util.ArrayList[RangeHighlighter]()
      highlights.foreach { highlight =>
        toTextRange(document, highlight.range).foreach { textRange =>
          if textRange.getStartOffset < textRange.getEndOffset then
            val highlighter = editor.getMarkupModel.addRangeHighlighter(
              textRange.getStartOffset,
              textRange.getEndOffset,
              HighlighterLayer.SELECTION - 100,
              MoZukuSemanticColors.attributesFor(highlight.tokenType),
              HighlighterTargetArea.EXACT_RANGE
            )
            created.add(highlighter)
        }
      }
      editor.putUserData(highlightersKey, created)

  private def clear(editor: Editor): Unit =
    Option(editor.getUserData(highlightersKey)).foreach { items =>
      items.asScala.foreach(editor.getMarkupModel.removeHighlighter)
    }
    editor.putUserData(highlightersKey, null)

  private def fileForUri(uri: String): Option[VirtualFile] =
    ReadAction
      .compute[java.util.Optional[VirtualFile], RuntimeException](() =>
        try
          java.util.Optional
            .ofNullable(VfsUtil.findFile(Path.of(URI.create(uri)), true))
        catch
          case _: IllegalArgumentException =>
            java.util.Optional.empty[VirtualFile]()
      )
      .toScala

  private def stateKeyForUri(uri: String): String =
    try Path.of(URI.create(uri)).normalize().toString
    catch case _: IllegalArgumentException => uri

  private def stateKeyForFile(file: VirtualFile): String =
    file.getPath

  private def toTextRange(
      document: Document,
      range: LspRange
  ): Option[com.intellij.openapi.util.TextRange] =
    Option(
      new com.intellij.openapi.util.TextRange(
        offset(document, range.start),
        offset(document, range.end)
      )
    )

  private def offset(document: Document, position: LspPosition): Int =
    val line = math.max(0, math.min(position.line, document.getLineCount - 1))
    val lineStart = document.getLineStartOffset(line)
    val lineEnd = document.getLineEndOffset(line)
    val lineText = document.getText(
      new com.intellij.openapi.util.TextRange(lineStart, lineEnd)
    )
    lineStart + math.min(math.max(position.character, 0), lineText.length)

  override def dispose(): Unit =
    semanticState = Map.empty

private final case class SemanticTokenOverlay(
    range: LspRange,
    tokenType: String
)

private object MoZukuSemanticColors:
  private val attributes = Map(
    "noun" -> text("#c8c8c8"),
    "verb" -> text("#569cd6"),
    "adjective" -> text("#4fc1ff"),
    "adverb" -> text("#9cdcfe"),
    "particle" -> text("#d16969"),
    "aux" -> text("#87ceeb"),
    "conjunction" -> text("#d7ba7d"),
    "symbol" -> text("#808080"),
    "interj" -> text("#b5cea8"),
    "prefix" -> text("#c8c8c8"),
    "suffix" -> text("#c8c8c8"),
    "unknown" -> text("#aaaaaa")
  )

  def attributesFor(tokenType: String): TextAttributes =
    attributes.getOrElse(tokenType, attributes("unknown"))

  private def text(hex: String): TextAttributes =
    val base = Color.decode(hex)
    val overlay = new Color(base.getRed, base.getGreen, base.getBlue, 168)
    new TextAttributes(overlay, null, null, null, Font.PLAIN)
