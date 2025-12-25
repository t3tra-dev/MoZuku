{
  partitionedAttrs.devShells = "dev";
  partitions.dev.module = devPartition: {
    imports = [ devPartition.inputs.git-hooks.flakeModule ];

    perSystem =
      { config, pkgs, ... }:
      {
          devenv.shells.default = {
            packages = with pkgs; [
              nil

              mold
              cmake
              ninja
              curlFull

              # dependencies
              mecab
              nativePackages.crfpp
              nativePackages.cabocha
              nlohmann_json

              tree-sitter
              tree-sitter-grammars.tree-sitter-c
              tree-sitter-grammars.tree-sitter-cpp
              tree-sitter-grammars.tree-sitter-html
              tree-sitter-grammars.tree-sitter-javascript
              tree-sitter-grammars.tree-sitter-latex
              tree-sitter-grammars.tree-sitter-python
              tree-sitter-grammars.tree-sitter-rust
              tree-sitter-grammars.tree-sitter-typescript
              tree-sitter-grammars.tree-sitter-tsx

              nativePackages.crfpp
              nativePackages.cabocha
              nativePackages.mozuku-lsp
            ];
          };
      };
  };
}
