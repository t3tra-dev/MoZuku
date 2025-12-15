{
  description = "A basic flake to with flake-parts";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixpkgs-unstable";
    treefmt-nix.url = "github:numtide/treefmt-nix";
    flake-parts.url = "github:hercules-ci/flake-parts";
    systems.url = "github:nix-systems/default";
    git-hooks-nix.url = "github:cachix/git-hooks.nix";
    devenv.url = "github:cachix/devenv";
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

          generated = import ./_sources/generated.nix;
          sources = generated {
            inherit (pkgs)
              fetchurl
              fetchgit
              fetchFromGitHub
              dockerTools
              ;
          };

          mozuku-lsp = stdenv.mkDerivation {
            pname = "mozuku";
            version = "0.0.1";
            src = ./mozuku-lsp;

	    nativeBuildInputs = with pkgs; [
	      cmake
	    ];

	    buildInputs = [
	      crfpp
	      cabocha
	    ];

            buildPhase = ''
	    '';

            installPhase = ''
	    '';
          };

          crfpp = stdenv.mkDerivation {
            pname = "crfpp";
            version = "0.58";
            src = sources.crfpp.src; 

            nativeBuildInputs = with pkgs; [
              autoconf
              automake
              libtool
              pkg-config
	      gettext
            ];

	    dontUseAutoreconf = true;

            buildPhase = ''
              ./configure --prefix=$out

              cat > winmain.h << 'EOF'
              // winmain.h - Empty header for Unix/Linux compatibility
              #ifndef WINMAIN_H_
              #define WINMAIN_H_
              #endif
              EOF

              make
            '';

            installPhase = ''
              make install
            '';
          };

          cabocha = stdenv.mkDerivation {
            pname = "cabocha";
            version = "0.69";
            src = sources.cabocha.src; 

            nativeBuildInputs = with pkgs; [
              autoconf
              automake
              libtool
              pkg-config
	      gettext
            ];

            buildInputs = with pkgs; [
              mecab
              crfpp
            ];

            configureFlags = [
              "--with-charset=UTF8"
              "--with-posset=IPA"
            ];

            preConfigure = ''
	      # rm -f aclocal.m4 configure config.h.in
              # autoreconf -fi
	      ./configure
            '';

            enableParallelBuilding = true;
          };

          git-secrets' = pkgs.writeShellApplication {
            name = "git-secrets";
            runtimeInputs = [ pkgs.git-secrets ];
            text = ''
              git secrets --scan
            '';
          };
        in
        {
          # When execute `nix fmt`, formatting your code.
          treefmt = {
            projectRootFile = "flake.nix";
            programs = {
              nixfmt.enable = true;
            };

            settings.formatter = { };
          };

          pre-commit = {
            check.enable = true;
            settings = {
              hooks = {
                treefmt.enable = true;
                ripsecrets.enable = true;
                git-secrets = {
                  enable = true;
                  name = "git-secrets";
                  entry = "${git-secrets'}/bin/git-secrets";
                  language = "system";
                  types = [ "text" ];
                };
              };
            };
          };

          devenv.shells.default = {
            packages = with pkgs; [
	      nil

	      cmake
	      curlFull

	      # dependencies
	      tree-sitter
	      mecab
	      crfpp
	      cabocha
	    ];

            languages = {
              cplusplus = {
                enable = true;
                # version = "8.4";
              };
            };

            enterShell = '''';
          };

          packages.default = mozuku-lsp;
        };
    };
}
