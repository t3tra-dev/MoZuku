# Change Log

All notable changes to the "mozuku" extension will be documented in this file.

## [0.1.0] - 2025-12-09

### Added

- 日本語 LSP サーバー (MoZuku) との連携を提供し、`.ja.txt`/`.ja.md` 向けにセマンティックトークンと診断を表示
- 設定から MeCab/CaboCha のパス・文字コード、文法チェックの各ルール ON/OFF・閾値を切り替え可能に
- コメント/コンテンツ範囲のハイライトに対応 (HTML/LaTeX など)

### Changed

- Windows/macOS/Linux 向けのビルドフローを CI に追加
