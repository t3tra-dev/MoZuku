# MoZuku for IntelliJ

<!-- Plugin description -->
IntelliJ IDEA から MoZuku LSP を利用するためのプラグインです。
既存の `mozuku-lsp` バイナリを起動し、日本語文書や各種ソースコード中の日本語コメントに対して診断とセマンティックハイライトを提供します。
<!-- Plugin description end -->

MoZuku LSP 本体はプラグインに同梱していないため、以下のいずれかで `mozuku-lsp` を参照できる必要があります。

- `Settings | Tools | MoZuku` の `Server path`
- `MOZUKU_LSP` 環境変数
- `PATH` や標準的なインストール先
