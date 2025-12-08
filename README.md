# MoZuku - A Japanese Language Server

MoZuku は、MeCab・CaboCha を活用した日本語文章の解析・校正を行う LSP サーバーです。

## 特徴

- **形態素解析**: MeCab による高精度な日本語トークン化
- **文法チェック**: 二重助詞、助詞連続、動詞-助詞不整合の検出
- **セマンティックハイライト**: 品詞ごとの色分け表示
- **コメント内解析**: C/C++/Python/JavaScript/TypeScript/Rust のコメント内日本語を解析
- **HTML/LaTeX サポート**: ドキュメント本文も解析
- **ホバー情報**: 単語の原形、読み、品詞情報、Wikipedia のサマリーを表示

## 必須依存

- MeCab + 辞書 (mecab-ipadic など)
- CaboCha (オプション : 係り受け解析 [WIP] 用)
- CURL
- tree-sitter CLI (ビルド時)
