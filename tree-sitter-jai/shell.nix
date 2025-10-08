
{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
	buildInputs = with pkgs; [
		nodejs
		tree-sitter
    ];

	shellHook = ''
		fish
	'';
}
