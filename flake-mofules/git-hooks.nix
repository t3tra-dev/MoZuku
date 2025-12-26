{
  partitionedAttrs.devShells = "dev";
  partitions.dev.module = devPartition: {
    imports = [ devPartition.inputs.git-hooks.flakeModule ];

    perSystem =
      { config, pkgs, ... }:
      {
          pre-commit = {
            check.enable = true;
            settings = {
              hooks = {
                treefmt.enable = true;
                ripsecrets.enable = true;
              };
            };
          };
      };
  };
}
