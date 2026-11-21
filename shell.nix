# Dev shell for Cmpl (NixOS / nix-on-other-distros).
# Usage:
#   nix-shell
#   nix develop   # via flake.nix
#
# Inside the shell, -lvulkan / -lwayland-client resolve via NIX_LDFLAGS.
{ pkgs ? import <nixpkgs> { } }:

pkgs.mkShell {
  packages = with pkgs; [
    gcc
    fasm
    pkg-config
    vulkan-loader
    vulkan-headers
    wayland
    wayland-scanner
    wayland-protocols
    # ICD providers for headless / real GPU at runtime
    mesa
  ];

  LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath [
    pkgs.vulkan-loader
    pkgs.mesa
    pkgs.wayland
  ];

  shellHook = ''
    echo "Cmpl nix shell: fasm=$(command -v fasm)"
    echo "  vulkan=$(pkg-config --libs vulkan 2>/dev/null || echo '-lvulkan')"
    echo "  wayland=$(pkg-config --libs wayland-client 2>/dev/null || echo '-lwayland-client')"
  '';
}
