package dev.t3tra.mozuku.idea.lsp

import com.intellij.openapi.application.ApplicationManager
import com.intellij.openapi.components.{
  PersistentStateComponent,
  Service,
  State,
  Storage
}

import java.util.{LinkedHashMap, Map as JMap}

@Service(Array(Service.Level.APP))
@State(name = "MoZukuSettings", storages = Array(new Storage("mozuku.xml")))
final class MoZukuSettingsService
    extends PersistentStateComponent[MoZukuSettingsState]:
  private var state: MoZukuSettingsState = MoZukuSettingsState.default()

  private def linkedMap(): LinkedHashMap[String, AnyRef] =
    new LinkedHashMap[String, AnyRef]()

  override def getState: MoZukuSettingsState =
    state

  override def loadState(loadedState: MoZukuSettingsState): Unit =
    state = loadedState

  def snapshot(): MoZukuSettingsState =
    MoZukuSettingsCloner.copy(state)

  def update(newState: MoZukuSettingsState): Unit =
    state = MoZukuSettingsCloner.copy(newState)

  def initializationOptions(): JMap[String, AnyRef] =
    val root = linkedMap()
    val mozuku = linkedMap()
    val mecab = linkedMap()
    val analysis = linkedMap()
    val warnings = linkedMap()
    val rules = linkedMap()

    mecab.put("dicdir", state.mecab.dicdir)
    mecab.put("charset", state.mecab.charset)

    warnings.put(
      "particleDuplicate",
      Boolean.box(state.analysis.warnings.particleDuplicate)
    )
    warnings.put(
      "particleSequence",
      Boolean.box(state.analysis.warnings.particleSequence)
    )
    warnings.put(
      "particleMismatch",
      Boolean.box(state.analysis.warnings.particleMismatch)
    )
    warnings.put(
      "sentenceStructure",
      Boolean.box(state.analysis.warnings.sentenceStructure)
    )
    warnings.put(
      "styleConsistency",
      Boolean.box(state.analysis.warnings.styleConsistency)
    )
    warnings.put("redundancy", Boolean.box(state.analysis.warnings.redundancy))

    rules.put("commaLimit", Boolean.box(state.analysis.rules.commaLimit))
    rules.put("adversativeGa", Boolean.box(state.analysis.rules.adversativeGa))
    rules.put(
      "duplicateParticleSurface",
      Boolean.box(state.analysis.rules.duplicateParticleSurface)
    )
    rules.put(
      "adjacentParticles",
      Boolean.box(state.analysis.rules.adjacentParticles)
    )
    rules.put(
      "conjunctionRepeat",
      Boolean.box(state.analysis.rules.conjunctionRepeat)
    )
    rules.put("raDropping", Boolean.box(state.analysis.rules.raDropping))
    rules.put("commaLimitMax", Int.box(state.analysis.rules.commaLimitMax))
    rules.put(
      "adversativeGaMax",
      Int.box(state.analysis.rules.adversativeGaMax)
    )
    rules.put(
      "duplicateParticleSurfaceMaxRepeat",
      Int.box(state.analysis.rules.duplicateParticleSurfaceMaxRepeat)
    )
    rules.put(
      "adjacentParticlesMaxRepeat",
      Int.box(state.analysis.rules.adjacentParticlesMaxRepeat)
    )
    rules.put(
      "conjunctionRepeatMax",
      Int.box(state.analysis.rules.conjunctionRepeatMax)
    )

    analysis.put("enableCaboCha", Boolean.box(state.analysis.enableCaboCha))
    analysis.put("grammarCheck", Boolean.box(state.analysis.grammarCheck))
    analysis.put(
      "minJapaneseRatio",
      Double.box(state.analysis.minJapaneseRatio)
    )
    analysis.put(
      "warningMinSeverity",
      Int.box(state.analysis.warningMinSeverity)
    )
    analysis.put("warnings", warnings)
    analysis.put("rules", rules)

    mozuku.put("mecab", mecab)
    mozuku.put("analysis", analysis)
    root.put("mozuku", mozuku)
    root

object MoZukuSettingsService:
  def getInstance(): MoZukuSettingsService =
    ApplicationManager.getApplication.getService(classOf[MoZukuSettingsService])

private object MoZukuSettingsCloner:
  def copy(source: MoZukuSettingsState): MoZukuSettingsState =
    val copied = MoZukuSettingsState.default()
    copied.serverPath = source.serverPath

    copied.mecab.dicdir = source.mecab.dicdir
    copied.mecab.charset = source.mecab.charset

    copied.analysis.enableCaboCha = source.analysis.enableCaboCha
    copied.analysis.grammarCheck = source.analysis.grammarCheck
    copied.analysis.minJapaneseRatio = source.analysis.minJapaneseRatio
    copied.analysis.warningMinSeverity = source.analysis.warningMinSeverity

    copied.analysis.warnings.particleDuplicate =
      source.analysis.warnings.particleDuplicate
    copied.analysis.warnings.particleSequence =
      source.analysis.warnings.particleSequence
    copied.analysis.warnings.particleMismatch =
      source.analysis.warnings.particleMismatch
    copied.analysis.warnings.sentenceStructure =
      source.analysis.warnings.sentenceStructure
    copied.analysis.warnings.styleConsistency =
      source.analysis.warnings.styleConsistency
    copied.analysis.warnings.redundancy = source.analysis.warnings.redundancy

    copied.analysis.rules.commaLimit = source.analysis.rules.commaLimit
    copied.analysis.rules.adversativeGa = source.analysis.rules.adversativeGa
    copied.analysis.rules.duplicateParticleSurface =
      source.analysis.rules.duplicateParticleSurface
    copied.analysis.rules.adjacentParticles =
      source.analysis.rules.adjacentParticles
    copied.analysis.rules.conjunctionRepeat =
      source.analysis.rules.conjunctionRepeat
    copied.analysis.rules.raDropping = source.analysis.rules.raDropping
    copied.analysis.rules.commaLimitMax = source.analysis.rules.commaLimitMax
    copied.analysis.rules.adversativeGaMax =
      source.analysis.rules.adversativeGaMax
    copied.analysis.rules.duplicateParticleSurfaceMaxRepeat =
      source.analysis.rules.duplicateParticleSurfaceMaxRepeat
    copied.analysis.rules.adjacentParticlesMaxRepeat =
      source.analysis.rules.adjacentParticlesMaxRepeat
    copied.analysis.rules.conjunctionRepeatMax =
      source.analysis.rules.conjunctionRepeatMax
    copied
