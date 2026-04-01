-- TML Language Server configuration for Neovim
--
-- Installation:
--   1. Build the language server:
--      cd vscode-tml && npm install && npm run compile
--   2. Copy this file to your Neovim config or source it
--   3. Requires nvim-lspconfig plugin
--
-- Usage:
--   Place this in your init.lua or lua/plugins/tml.lua

local lspconfig = require('lspconfig')
local configs = require('lspconfig.configs')

-- Register TML language server if not already registered
if not configs.tml then
    configs.tml = {
        default_config = {
            -- Path to the compiled server.js
            -- Adjust this path to where you cloned the tml repo
            cmd = { 'node', vim.fn.expand('~/tml/vscode-tml/out/server/server.js'), '--stdio' },
            filetypes = { 'tml' },
            root_dir = lspconfig.util.root_pattern('tml.toml', '.git'),
            settings = {},
            single_file_support = true,
        },
    }
end

-- Setup the TML LSP
lspconfig.tml.setup({
    capabilities = require('cmp_nvim_lsp').default_capabilities(),
    on_attach = function(client, bufnr)
        -- Keymaps for LSP features
        local opts = { buffer = bufnr, noremap = true, silent = true }

        vim.keymap.set('n', 'gd', vim.lsp.buf.definition, opts)
        vim.keymap.set('n', 'gr', vim.lsp.buf.references, opts)
        vim.keymap.set('n', 'K', vim.lsp.buf.hover, opts)
        vim.keymap.set('n', '<leader>rn', vim.lsp.buf.rename, opts)
        vim.keymap.set('n', 'gi', vim.lsp.buf.implementation, opts)
        vim.keymap.set('n', '<leader>ws', vim.lsp.buf.workspace_symbol, opts)
        vim.keymap.set('n', '<leader>ca', vim.lsp.buf.code_action, opts)
    end,
})

-- Register .tml filetype
vim.filetype.add({
    extension = {
        tml = 'tml',
    },
})

-- Basic TML syntax highlighting (when treesitter grammar is not available)
vim.api.nvim_create_autocmd('FileType', {
    pattern = 'tml',
    callback = function()
        vim.bo.commentstring = '// %s'
        vim.bo.tabstop = 4
        vim.bo.shiftwidth = 4
        vim.bo.expandtab = true
    end,
})
