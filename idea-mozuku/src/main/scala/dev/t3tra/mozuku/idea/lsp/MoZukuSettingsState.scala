package dev.t3tra.mozuku.idea.lsp

final class MoZukuSettingsState:
  var serverPath: String = "mozuku-lsp"
  var mecab: MecabSettings = new MecabSettings()
  var analysis: AnalysisSettings = new AnalysisSettings()

final class MecabSettings:
  var dicdir: String = ""
  var charset: String = "UTF-8"

final class AnalysisSettings:
  var enableCaboCha: Boolean = true
  var grammarCheck: Boolean = true
  var minJapaneseRatio: Double = 0.1d
  var warningMinSeverity: Int = 2
  var warnings: WarningSettings = new WarningSettings()
  var rules: RuleSettings = new RuleSettings()

final class WarningSettings:
  var particleDuplicate: Boolean = true
  var particleSequence: Boolean = true
  var particleMismatch: Boolean = true
  var sentenceStructure: Boolean = false
  var styleConsistency: Boolean = false
  var redundancy: Boolean = false

final class RuleSettings:
  var commaLimit: Boolean = true
  var adversativeGa: Boolean = true
  var duplicateParticleSurface: Boolean = true
  var adjacentParticles: Boolean = true
  var conjunctionRepeat: Boolean = true
  var raDropping: Boolean = true
  var commaLimitMax: Int = 3
  var adversativeGaMax: Int = 1
  var duplicateParticleSurfaceMaxRepeat: Int = 1
  var adjacentParticlesMaxRepeat: Int = 1
  var conjunctionRepeatMax: Int = 1

object MoZukuSettingsState:
  def default(): MoZukuSettingsState = new MoZukuSettingsState()
