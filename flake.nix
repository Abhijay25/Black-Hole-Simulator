{
  description = "Black Hole Simulator dev environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        buildInputs = with pkgs; [
          # Build tools
          gcc
          gnumake
          pkg-config
          clang-tools  # provides clangd for editor LSP

          # Raylib + its windowing deps
          raylib
          libGL
          libxkbcommon
          wayland
          libx11
          libxrandr
          libxinerama
          libxcursor
          libxi
        ];

        LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath [
          pkgs.raylib
          pkgs.libGL
        ];

        shellHook = ''
          RAYLIB_INCLUDE=$(pkg-config --variable=includedir raylib)
          {
            echo "CompileFlags:"
            echo "  Add:"
            echo "    - -I$RAYLIB_INCLUDE"
          } > .clangd
        '';
      };
    };
}
