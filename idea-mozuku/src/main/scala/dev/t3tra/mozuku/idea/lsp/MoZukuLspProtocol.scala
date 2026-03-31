package dev.t3tra.mozuku.idea.lsp

import scala.jdk.CollectionConverters.*

final case class LspPosition(line: Int, character: Int)
final case class LspRange(start: LspPosition, end: LspPosition)
final case class ContentRangesPayload(uri: String, ranges: Seq[LspRange])
final case class SemanticTokenPayload(
    range: LspRange,
    tokenType: String,
    modifiers: Int
)
final case class SemanticHighlightsPayload(
    uri: String,
    tokens: Seq[SemanticTokenPayload]
)

object MoZukuLspProtocol:
  def parseContentRanges(
      payload: java.util.Map[String, Object]
  ): ContentRangesPayload =
    val uri = stringValue(payload.get("uri"))
    val ranges = listValue(payload.get("ranges")).flatMap(parseRange)
    ContentRangesPayload(uri, ranges)

  def parseSemanticHighlights(
      payload: java.util.Map[String, Object]
  ): SemanticHighlightsPayload =
    val uri = stringValue(payload.get("uri"))
    val tokens = listValue(payload.get("tokens")).flatMap(parseToken)
    SemanticHighlightsPayload(uri, tokens)

  private def parseToken(value: Object): Option[SemanticTokenPayload] =
    mapValue(value).flatMap { token =>
      for
        range <- parseRange(token.get("range"))
        tokenType = stringValue(token.get("type"))
        modifiers = numberValue(token.get("modifiers")).toInt
      yield SemanticTokenPayload(range, tokenType, modifiers)
    }

  private def parseRange(value: Object): Option[LspRange] =
    mapValue(value).flatMap { range =>
      for
        start <- parsePosition(range.get("start"))
        end <- parsePosition(range.get("end"))
      yield LspRange(start, end)
    }

  private def parsePosition(value: Object): Option[LspPosition] =
    mapValue(value).map { position =>
      LspPosition(
        numberValue(position.get("line")).toInt,
        numberValue(position.get("character")).toInt
      )
    }

  private def mapValue(value: Object): Option[java.util.Map[String, Object]] =
    value match
      case map: java.util.Map[?, ?] =>
        Some(map.asInstanceOf[java.util.Map[String, Object]])
      case _ => None

  private def listValue(value: Object): Seq[Object] =
    value match
      case list: java.util.List[?] =>
        list.asInstanceOf[java.util.List[Object]].asScala.toSeq
      case _ => Seq.empty

  private def stringValue(value: Object): String =
    Option(value).map(_.toString).getOrElse("")

  private def numberValue(value: Object): Double =
    value match
      case number: java.lang.Number => number.doubleValue()
      case string if string != null =>
        string.toString.toDoubleOption.getOrElse(0.0d)
      case _ => 0.0d
