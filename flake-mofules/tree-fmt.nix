{
  partitionedAttrs.devShells = "dev";
  partitions.dev.module = devPartition: {
    imports = [ devPartition.inputs.git-hooks.flakeModule ];

    perSystem =
      { config, pkgs, ... }:
      {

          treefmt = {
            projectRootFile = "flake.nix";
            programs = {
              nixfmt.enable = true;
              clang-format.enable = true;
            };
          };
      };
  };
}
