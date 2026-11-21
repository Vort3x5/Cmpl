# Regenerates / tests the grammar without a global npm install.
#   nix-shell
#   tree-sitter generate
#   tree-sitter parse ../examples/BasicJai/*.jai
#   tree-sitter test
{ pkgs ? import <nixpkgs> { } }:

pkgs.mkShell {
  packages = with pkgs; [
    nodejs   # tree-sitter CLI still shells out to node for grammar.js
    tree-sitter
  ];

  shellHook = ''
    echo "tree-sitter-jai: $(command -v tree-sitter)  node=$(command -v node)"
    echo "  tree-sitter generate && tree-sitter test"
  '';
}
