{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    scenefx = {
      url = "github:wlrfx/scenefx";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = {
    self,
    flake-parts,
    ...
  } @ inputs:
    flake-parts.lib.mkFlake {inherit inputs;} {
      imports = [
        inputs.flake-parts.flakeModules.easyOverlay
      ];

      flake = {
        hmModules.lemon = import ./nix/hm-modules.nix self;
        nixosModules.lemon = import ./nix/nixos-modules.nix self;
      };

      perSystem = {
        config,
        pkgs,
        ...
      }: let
        inherit (pkgs) callPackage ;
        lemon = callPackage ./nix {
          inherit (inputs.scenefx.packages.${pkgs.stdenv.hostPlatform.system}) scenefx;
        };
        shellOverride = old: {
          nativeBuildInputs = old.nativeBuildInputs ++ [];
          buildInputs = old.buildInputs ++ [];
        };
      in {
        packages.default = lemon;
        overlayAttrs = {
          inherit (config.packages) lemon;
        };
        packages = {
          inherit lemon;
          hm-options-json = pkgs.callPackage (import ./nix/generate-options.nix self) {
            module = ./nix/hm-modules.nix;
            optionPrefix = "wayland.windowManager.lemon.";
          };
          nixos-options-json = pkgs.callPackage (import ./nix/generate-options.nix self) {
            module = ./nix/nixos-modules.nix;
            optionPrefix = "programs.lemon.";
          };
        };
        devShells.default = lemon.overrideAttrs shellOverride;
        formatter = pkgs.alejandra;
      };
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
    };
}
