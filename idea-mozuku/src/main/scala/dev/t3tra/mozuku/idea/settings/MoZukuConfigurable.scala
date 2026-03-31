package dev.t3tra.mozuku.idea.settings

import com.intellij.openapi.options.SearchableConfigurable
import com.intellij.openapi.project.ProjectManager
import com.intellij.platform.lsp.api.LspServerManager
import dev.t3tra.mozuku.idea.lsp.{
  MoZukuLspServerSupportProvider,
  MoZukuSettingsService,
  MoZukuSettingsState
}

import java.awt.{GridBagConstraints, GridBagLayout, Insets}
import javax.swing.*

final class MoZukuConfigurable extends SearchableConfigurable:
  private var panel: JPanel | Null = null
  private val serverPathField = new JTextField()
  private val mecabDicdirField = new JTextField()
  private val mecabCharsetBox =
    new JComboBox[String](Array("UTF-8", "EUC-JP", "Shift_JIS"))

  private val enableCaboChaBox = new JCheckBox("Enable CaboCha")
  private val grammarCheckBox = new JCheckBox("Enable grammar diagnostics")
  private val minJapaneseRatioSpinner = spinner(0.1d, 0.0d, 1.0d, 0.05d)
  private val warningMinSeveritySpinner = spinner(2, 1, 4, 1)

  private val particleDuplicateBox = new JCheckBox("particleDuplicate")
  private val particleSequenceBox = new JCheckBox("particleSequence")
  private val particleMismatchBox = new JCheckBox("particleMismatch")
  private val sentenceStructureBox = new JCheckBox("sentenceStructure")
  private val styleConsistencyBox = new JCheckBox("styleConsistency")
  private val redundancyBox = new JCheckBox("redundancy")

  private val commaLimitBox = new JCheckBox("commaLimit")
  private val adversativeGaBox = new JCheckBox("adversativeGa")
  private val duplicateParticleSurfaceBox = new JCheckBox(
    "duplicateParticleSurface"
  )
  private val adjacentParticlesBox = new JCheckBox("adjacentParticles")
  private val conjunctionRepeatBox = new JCheckBox("conjunctionRepeat")
  private val raDroppingBox = new JCheckBox("raDropping")
  private val commaLimitMaxSpinner = spinner(3, 1, 99, 1)
  private val adversativeGaMaxSpinner = spinner(1, 1, 99, 1)
  private val duplicateParticleSurfaceMaxRepeatSpinner = spinner(1, 1, 99, 1)
  private val adjacentParticlesMaxRepeatSpinner = spinner(1, 1, 99, 1)
  private val conjunctionRepeatMaxSpinner = spinner(1, 1, 99, 1)

  override def getId: String =
    "dev.t3tra.mozuku.idea.settings"

  override def getDisplayName: String =
    "MoZuku"

  override def createComponent(): JComponent =
    if panel == null then panel = buildPanel()
    panel.nn

  override def isModified: Boolean =
    val state = MoZukuSettingsService.getInstance().snapshot()
    serverPathField.getText != state.serverPath ||
    mecabDicdirField.getText != state.mecab.dicdir ||
    mecabCharsetBox.getSelectedItem != state.mecab.charset ||
    enableCaboChaBox.isSelected != state.analysis.enableCaboCha ||
    grammarCheckBox.isSelected != state.analysis.grammarCheck ||
    minJapaneseRatioSpinner.getValue != state.analysis.minJapaneseRatio ||
    warningMinSeveritySpinner.getValue != state.analysis.warningMinSeverity ||
    particleDuplicateBox.isSelected != state.analysis.warnings.particleDuplicate ||
    particleSequenceBox.isSelected != state.analysis.warnings.particleSequence ||
    particleMismatchBox.isSelected != state.analysis.warnings.particleMismatch ||
    sentenceStructureBox.isSelected != state.analysis.warnings.sentenceStructure ||
    styleConsistencyBox.isSelected != state.analysis.warnings.styleConsistency ||
    redundancyBox.isSelected != state.analysis.warnings.redundancy ||
    commaLimitBox.isSelected != state.analysis.rules.commaLimit ||
    adversativeGaBox.isSelected != state.analysis.rules.adversativeGa ||
    duplicateParticleSurfaceBox.isSelected != state.analysis.rules.duplicateParticleSurface ||
    adjacentParticlesBox.isSelected != state.analysis.rules.adjacentParticles ||
    conjunctionRepeatBox.isSelected != state.analysis.rules.conjunctionRepeat ||
    raDroppingBox.isSelected != state.analysis.rules.raDropping ||
    commaLimitMaxSpinner.getValue != state.analysis.rules.commaLimitMax ||
    adversativeGaMaxSpinner.getValue != state.analysis.rules.adversativeGaMax ||
    duplicateParticleSurfaceMaxRepeatSpinner.getValue != state.analysis.rules.duplicateParticleSurfaceMaxRepeat ||
    adjacentParticlesMaxRepeatSpinner.getValue != state.analysis.rules.adjacentParticlesMaxRepeat ||
    conjunctionRepeatMaxSpinner.getValue != state.analysis.rules.conjunctionRepeatMax

  override def apply(): Unit =
    val state = new MoZukuSettingsState()
    state.serverPath = serverPathField.getText.trim
    state.mecab.dicdir = mecabDicdirField.getText.trim
    state.mecab.charset = mecabCharsetBox.getSelectedItem.toString
    state.analysis.enableCaboCha = enableCaboChaBox.isSelected
    state.analysis.grammarCheck = grammarCheckBox.isSelected
    state.analysis.minJapaneseRatio =
      minJapaneseRatioSpinner.getValue.asInstanceOf[Double]
    state.analysis.warningMinSeverity =
      warningMinSeveritySpinner.getValue.asInstanceOf[Int]

    state.analysis.warnings.particleDuplicate = particleDuplicateBox.isSelected
    state.analysis.warnings.particleSequence = particleSequenceBox.isSelected
    state.analysis.warnings.particleMismatch = particleMismatchBox.isSelected
    state.analysis.warnings.sentenceStructure = sentenceStructureBox.isSelected
    state.analysis.warnings.styleConsistency = styleConsistencyBox.isSelected
    state.analysis.warnings.redundancy = redundancyBox.isSelected

    state.analysis.rules.commaLimit = commaLimitBox.isSelected
    state.analysis.rules.adversativeGa = adversativeGaBox.isSelected
    state.analysis.rules.duplicateParticleSurface =
      duplicateParticleSurfaceBox.isSelected
    state.analysis.rules.adjacentParticles = adjacentParticlesBox.isSelected
    state.analysis.rules.conjunctionRepeat = conjunctionRepeatBox.isSelected
    state.analysis.rules.raDropping = raDroppingBox.isSelected
    state.analysis.rules.commaLimitMax =
      commaLimitMaxSpinner.getValue.asInstanceOf[Int]
    state.analysis.rules.adversativeGaMax =
      adversativeGaMaxSpinner.getValue.asInstanceOf[Int]
    state.analysis.rules.duplicateParticleSurfaceMaxRepeat =
      duplicateParticleSurfaceMaxRepeatSpinner.getValue.asInstanceOf[Int]
    state.analysis.rules.adjacentParticlesMaxRepeat =
      adjacentParticlesMaxRepeatSpinner.getValue.asInstanceOf[Int]
    state.analysis.rules.conjunctionRepeatMax =
      conjunctionRepeatMaxSpinner.getValue.asInstanceOf[Int]

    MoZukuSettingsService.getInstance().update(state)
    ProjectManager.getInstance.getOpenProjects.foreach { project =>
      LspServerManager
        .getInstance(project)
        .stopAndRestartIfNeeded(classOf[MoZukuLspServerSupportProvider])
    }

  override def reset(): Unit =
    val state = MoZukuSettingsService.getInstance().snapshot()
    serverPathField.setText(state.serverPath)
    mecabDicdirField.setText(state.mecab.dicdir)
    mecabCharsetBox.setSelectedItem(state.mecab.charset)
    enableCaboChaBox.setSelected(state.analysis.enableCaboCha)
    grammarCheckBox.setSelected(state.analysis.grammarCheck)
    minJapaneseRatioSpinner.setValue(
      Double.box(state.analysis.minJapaneseRatio)
    )
    warningMinSeveritySpinner.setValue(
      Int.box(state.analysis.warningMinSeverity)
    )

    particleDuplicateBox.setSelected(state.analysis.warnings.particleDuplicate)
    particleSequenceBox.setSelected(state.analysis.warnings.particleSequence)
    particleMismatchBox.setSelected(state.analysis.warnings.particleMismatch)
    sentenceStructureBox.setSelected(state.analysis.warnings.sentenceStructure)
    styleConsistencyBox.setSelected(state.analysis.warnings.styleConsistency)
    redundancyBox.setSelected(state.analysis.warnings.redundancy)

    commaLimitBox.setSelected(state.analysis.rules.commaLimit)
    adversativeGaBox.setSelected(state.analysis.rules.adversativeGa)
    duplicateParticleSurfaceBox.setSelected(
      state.analysis.rules.duplicateParticleSurface
    )
    adjacentParticlesBox.setSelected(state.analysis.rules.adjacentParticles)
    conjunctionRepeatBox.setSelected(state.analysis.rules.conjunctionRepeat)
    raDroppingBox.setSelected(state.analysis.rules.raDropping)
    commaLimitMaxSpinner.setValue(Int.box(state.analysis.rules.commaLimitMax))
    adversativeGaMaxSpinner.setValue(
      Int.box(state.analysis.rules.adversativeGaMax)
    )
    duplicateParticleSurfaceMaxRepeatSpinner.setValue(
      Int.box(state.analysis.rules.duplicateParticleSurfaceMaxRepeat)
    )
    adjacentParticlesMaxRepeatSpinner.setValue(
      Int.box(state.analysis.rules.adjacentParticlesMaxRepeat)
    )
    conjunctionRepeatMaxSpinner.setValue(
      Int.box(state.analysis.rules.conjunctionRepeatMax)
    )

  private def buildPanel(): JPanel =
    val root = new JPanel(new GridBagLayout())
    val c = new GridBagConstraints()
    c.gridx = 0
    c.gridy = 0
    c.weightx = 1.0
    c.fill = GridBagConstraints.HORIZONTAL
    c.anchor = GridBagConstraints.NORTHWEST
    c.insets = new Insets(4, 4, 4, 4)

    root.add(section("Server", row("Server path", serverPathField)), c)
    c.gridy += 1
    root.add(
      section(
        "MeCab",
        row("Dictionary dir", mecabDicdirField),
        row("Charset", mecabCharsetBox)
      ),
      c
    )
    c.gridy += 1
    root.add(
      section(
        "Analysis",
        enableCaboChaBox,
        grammarCheckBox,
        row("Min Japanese ratio", minJapaneseRatioSpinner),
        row("Warning min severity", warningMinSeveritySpinner)
      ),
      c
    )
    c.gridy += 1
    root.add(
      section(
        "Warnings",
        particleDuplicateBox,
        particleSequenceBox,
        particleMismatchBox,
        sentenceStructureBox,
        styleConsistencyBox,
        redundancyBox
      ),
      c
    )
    c.gridy += 1
    root.add(
      section(
        "Rules",
        commaLimitBox,
        adversativeGaBox,
        duplicateParticleSurfaceBox,
        adjacentParticlesBox,
        conjunctionRepeatBox,
        raDroppingBox,
        row("commaLimitMax", commaLimitMaxSpinner),
        row("adversativeGaMax", adversativeGaMaxSpinner),
        row(
          "duplicateParticleSurfaceMaxRepeat",
          duplicateParticleSurfaceMaxRepeatSpinner
        ),
        row("adjacentParticlesMaxRepeat", adjacentParticlesMaxRepeatSpinner),
        row("conjunctionRepeatMax", conjunctionRepeatMaxSpinner)
      ),
      c
    )
    c.gridy += 1
    c.weighty = 1.0
    c.fill = GridBagConstraints.BOTH
    root.add(new JPanel(), c)
    reset()
    root

  private def section(title: String, components: JComponent*): JPanel =
    val panel = new JPanel()
    panel.setLayout(new BoxLayout(panel, BoxLayout.Y_AXIS))
    panel.setBorder(BorderFactory.createTitledBorder(title))
    components.foreach(panel.add)
    panel

  private def row(label: String, component: JComponent): JPanel =
    val panel = new JPanel(new GridBagLayout())
    val c = new GridBagConstraints()
    c.gridx = 0
    c.gridy = 0
    c.anchor = GridBagConstraints.WEST
    c.insets = new Insets(2, 2, 2, 8)
    panel.add(new JLabel(label), c)
    c.gridx = 1
    c.weightx = 1.0
    c.fill = GridBagConstraints.HORIZONTAL
    panel.add(component, c)
    panel

  private def spinner(
      value: Double,
      min: Double,
      max: Double,
      step: Double
  ): JSpinner =
    new JSpinner(new SpinnerNumberModel(value, min, max, step))

  private def spinner(value: Int, min: Int, max: Int, step: Int): JSpinner =
    new JSpinner(new SpinnerNumberModel(value, min, max, step))
