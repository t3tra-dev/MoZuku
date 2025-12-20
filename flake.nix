{
  description = "A basic flake to with flake-parts";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixpkgs-unstable";
    treefmt-nix.url = "github:numtide/treefmt-nix";
    flake-parts.url = "github:hercules-ci/flake-parts";
    systems.url = "github:nix-systems/default";
    git-hooks-nix.url = "github:cachix/git-hooks.nix";
    devenv.url = "github:cachix/devenv";
    crfpp = {
      url = "github:taku910/crfpp/c603ea6fc2cc8e5317d2754971bc787da383f7e5";
      flake = false;
    };
    cabocha = {
      url = "github:taku910/cabocha/96e100a62f6dadceb490cdd61c8c15fb561f6674";
      flake = false;
    };
  };

  outputs =
    inputs@{
      self,
      systems,
      nixpkgs,
      flake-parts,
      ...
    }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      imports = [
        inputs.treefmt-nix.flakeModule
        inputs.git-hooks-nix.flakeModule
        inputs.devenv.flakeModule
      ];
      systems = import inputs.systems;

      perSystem =
        {
          config,
          pkgs,
          system,
          ...
        }:
        let
          stdenv = pkgs.stdenv;

          mozuku-lsp = stdenv.mkDerivation {
            pname = "mozuku-lsp";
            version = "0.0.1";
            src = ./mozuku-lsp;
            cmakeGenerator = "Ninja";

            nativeBuildInputs = with pkgs; [
              mold-wrapped
              cmake
              ninja
              pkg-config
              tree-sitter
            ];

            NIX_LDFLAGS = [
              "-fuse-ld=mold"
            ];

            buildInputs = with pkgs; [
              mecab
              crfpp
              cabocha
              curl
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
            ];
          };

          crfpp = stdenv.mkDerivation {
            pname = "crfpp";
            version = "0.58";
            src = inputs.crfpp;

            nativeBuildInputs = with pkgs; [
              autoconf
              automake
              libtool
              pkg-config
            ];

            patchPhase = ''
              ./configure --prefix=$out

              cat > winmain.h << 'EOF'
              // winmain.h - Empty header for Unix/Linux compatibility
              #ifndef WINMAIN_H_
              #define WINMAIN_H_
              #endif
              EOF
            '';

            enableParallelBuilding = true;
          };

          cabocha = stdenv.mkDerivation {
            pname = "cabocha";
            version = "0.69";
            src = inputs.cabocha;

            nativeBuildInputs = with pkgs; [
              pkg-config
              automake
            ];

            buildInputs = with pkgs; [
              mecab
              crfpp
              libiconv
              gettext
            ];

            configureFlags = [
              "--with-charset=UTF8"
              "--with-posset=IPA"
            ];

            preConfigure = ''
              cp ${pkgs.automake}/share/automake-*/install-sh .
            '';

            enableParallelBuilding = true;
          };
        in
        {
          treefmt = {
            projectRootFile = "flake.nix";
            programs = {
              nixfmt.enable = true;
              clang-format.enable = true;
            };
          };

          pre-commit = {
            check.enable = true;
            settings = {
              hooks = {
                treefmt.enable = true;
                ripsecrets.enable = true;
              };
            };
          };

          devenv.shells.default = {
            packages = with pkgs; [
              nil

              mold
              cmake
              ninja
              curlFull

              # dependencies
              mecab
              crfpp
              cabocha
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
            ];
          };

          packages.default = mozuku-lsp;
        };
    };
}
