# vim-mozuku

MoZuku LSP を Vim/Neovim で使うための軽量プラグインです。`vim-mozuku/` を runtimepath に追加することで有効化できます。既定では `mozuku-lsp` を `PATH`、標準的なインストール先、開発中の `build/install` から探索します。

## configure (e.g. lazy.nvim)

```lua
require("lazy").setup({
  spec = {
    {
      "t3tra-dev/MoZuku",
      name = "vim-mozuku",
      branch = "main",
      submodules = false,
      init = function(plugin)
        vim.opt.rtp:prepend(plugin.dir .. "/vim-mozuku")
      end,
      build = function(plugin)
        local dir = plugin.dir
        vim.fn.system({ "git", "-C", dir, "sparse-checkout", "init", "--cone" })
        vim.fn.system({ "git", "-C", dir, "sparse-checkout", "set", "vim-mozuku" })
      end,
    },
  },
}, {
  git = {
    filter = true, -- partial clone (blobless)
  },
})
```
