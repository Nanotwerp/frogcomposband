{
  description = "C++ development environment";

  inputs = {
    nixpkgs.url = "nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];

      forAllSystems = f: nixpkgs.lib.genAttrs supportedSystems (system: f {
        pkgs = import nixpkgs { inherit system; };
        system = system;
      });
    in
    {
      devShell = forAllSystems ({ system, pkgs }:
        pkgs.mkShell {
          packages = [
            pkgs.autoconf
            pkgs.automake
            pkgs.clang-tools_17
            pkgs.llvmPackages_17.clangUseLLVM
            pkgs.mold
            pkgs.lldb_17
          ];
        });
    };
}
