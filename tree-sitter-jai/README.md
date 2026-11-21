# tree-sitter-jai

Grammar for Cmpl’s Jai subset. **No global npm.**

## In this repo (regenerate / test)

Root or local nix shell provides `tree-sitter` + `node` (CLI still loads `grammar.js` via node):

```bash
cd tree-sitter-jai
nix-shell                 # or: nix-shell ..  from repo root
tree-sitter generate
tree-sitter parse ../examples/BasicJai/*.jai   # no (ERROR)
tree-sitter test
```

Commit `grammar.js`, `queries/`, `src/` (generated), `corpus/`.

## On your NixOS machine (Neovim)

Config: `~/nixos-config/home/neovim.nix` builds the parser with

`pkgs.tree-sitter.buildGrammar { src = ./nvim/plugin/tree-sitter-jai; }`

(no npm — only compiles committed `src/parser.c`).

After updating the grammar in Cmpl:

```fish
# sync into home-manager plugin tree
rsync -a --delete \
  --exclude node_modules --exclude package-lock.json \
  --exclude bindings --exclude binding.gyp --exclude Cargo.toml \
  ~/Mvsc/Cmpl/tree-sitter-jai/ \
  ~/nixos-config/home/nvim/plugin/tree-sitter-jai/

cd ~/nixos-config
home-manager switch --flake ./home#vortex
```

(If your active attr is `vortex-colemak` / `vortex-qwerty`, use that instead of `vortex`.)

## Highlights

`queries/jai/highlights.scm` — `#library` / `#foreign`, `defer`, types, `.*`, calls in `if`, etc.
