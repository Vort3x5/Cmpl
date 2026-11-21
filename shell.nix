# Dev shell for Cmpl (NixOS / nix-on-other-distros).
# Usage:
#   nix-shell
#   nix develop   # if you use the flake
#
# Inside the shell, -lvulkan resolves via vulkan-loader in NIX_LDFLAGS.
{ pkgs ? import <nixpkgs> { } }:

pkgs.mkShell {
  packages = with pkgs; [
    gcc
    fasm
    pkg-config
    vulkan-loader
    # ICD providers for headless / real GPU at runtime
    mesa
  ];

  # Runtime discovery of libvulkan + Mesa ICDs
  LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath [
    pkgs.vulkan-loader
    pkgs.mesa
  ];

  shellHook = ''
    echo "Cmpl nix shell: fasm=$(command -v fasm)  vulkan=$(pkg-config --libs vulkan 2>/dev/null || echo '-lvulkan')"
  '';
}
