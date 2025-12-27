{
  description = "An LSP server for parsing and proofreading Japanese text.";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixpkgs-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    systems.url = "github:nix-systems/default";
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
	inputs.flake-parts.flakeModules.partitions
      ];

      systems = import inputs.systems;

      partitions.dev.extraInputsFlake = ./dev-flake;

      perSystem =
        {
          config,
          pkgs,
          system,
          ...
        }:
        let
          crossTargets = [
            "musl64" # x86_64-linux with musl
            "aarch64-multiplatform" # aarch64-linux
	    "aarch64-multiplatform-musl" # aarch64-unknown- linux-musl
          ];

          mkPackagesFor =
            targetPkgs:
            let
              stdenv = targetPkgs.stdenv;

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

                buildInputs = with targetPkgs; [
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

                cmakeFlags = [
                  "-DCABOCHA_INCLUDE_HINT=${cabocha}/include"
                  "-DCABOCHA_LIBRARY_HINT=${cabocha}/lib"
                  "-DCRFPP_INCLUDE_HINT=${crfpp}/include"
                  "-DCRFPP_LIBRARY_HINT=${crfpp}/lib"
                ];

                PKG_CONFIG_PATH = "${targetPkgs.tree-sitter}/lib/pkgconfig";
              };

              crfpp = stdenv.mkDerivation {
                pname = "crfpp";
                version = "0.58";
                src = inputs.crfpp;

                configurePlatforms = [
                  "build"
                  "host"
                ];

                nativeBuildInputs = with pkgs; [
                  autoconf
                  automake
                  libtool
                  pkg-config
                ];

                patchPhase = ''
                  runHook prePatch

                  cat > winmain.h << 'EOF'
                  // winmain.h - Empty header for Unix/Linux compatibility
                  #ifndef WINMAIN_H_
                  #define WINMAIN_H_
                  #endif
                  EOF

                  rm -f configure

                  runHook postPatch
                '';

                preConfigure = ''
                  rm -f configure
                  autoreconf -vfi
                '';

                enableParallelBuilding = true;
              };

              cabocha = stdenv.mkDerivation {
                pname = "cabocha";
                version = "0.69";
                src = inputs.cabocha;

                configurePlatforms = [
                  "build"
                  "host"
                ];

                nativeBuildInputs = with pkgs; [
                  pkg-config
                  automake
                  autoconf
                  libtool
                  gettext
                ];

                buildInputs = with targetPkgs; [
                  mecab
                  crfpp
                  libiconv
                ];

                configureFlags = [
                  "--with-charset=UTF8"
                  "--with-posset=IPA"
                ];

                preConfigure = ''
                  # install-sh がない場合のみコピー
                  if [ ! -f install-sh ]; then
                    cp ${pkgs.automake}/share/automake-*/install-sh .
                  fi
                  # configure スクリプトが存在しない場合のみ autoreconf を実行
                  if [ ! -f configure ]; then
                    export ACLOCAL_PATH="${pkgs.gettext}/share/aclocal:$ACLOCAL_PATH"
                    autoreconf -vfi
                  fi
                '';

                makeFlags =
                  if (stdenv.buildPlatform != stdenv.hostPlatform) then
                    [
                      "SUBDIRS=src doc"
                    ]
                  else
                    [ ];

                enableParallelBuilding = true;
              };
            in
            {
              inherit mozuku-lsp crfpp cabocha;
            };

          nativePackages = mkPackagesFor pkgs;

          mkMoZuKuLSP =
            crossTarget:
            let
              crossPkgs = pkgs.pkgsCross.${crossTarget};
              crossPackages = mkPackagesFor crossPkgs;
            in
            crossPackages.mozuku-lsp;
        in
        {
          packages = {
            default = nativePackages.mozuku-lsp;
          }
          // builtins.listToAttrs (
            map (target: {
              name = "mozuku-lsp-${target}";
              value = mkMoZuKuLSP target;
            }) crossTargets
          );
        };
    };
}
