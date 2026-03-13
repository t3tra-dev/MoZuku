local M = {}

local state = {
  comment = {},
  content = {},
  semantic = {},
}

local ns_comment = vim.api.nvim_create_namespace('mozuku_comment')
local ns_content = vim.api.nvim_create_namespace('mozuku_content')
local ns_semantic = vim.api.nvim_create_namespace('mozuku_semantic')

local token_groups = {
  noun = 'MozukuNoun',
  verb = 'MozukuVerb',
  adjective = 'MozukuAdjective',
  adverb = 'MozukuAdverb',
  particle = 'MozukuParticle',
  aux = 'MozukuAux',
  conjunction = 'MozukuConjunction',
  symbol = 'MozukuSymbol',
  interj = 'MozukuInterj',
  prefix = 'MozukuPrefix',
  suffix = 'MozukuSuffix',
  unknown = 'MozukuUnknown',
}

local function language_id_for(bufnr, filetype)
  if filetype == 'tex' or filetype == 'plaintex' then
    return 'latex'
  end
  if filetype == 'text' or filetype == 'markdown' then
    local name = vim.api.nvim_buf_get_name(bufnr)
    if name:match('%.ja%.[^.]+$') then
      return 'japanese'
    end
  end
  return filetype
end

local function to_byte_col(line, utf16_col)
  return vim.fn['mozuku#utf16_to_byte'](line, utf16_col)
end

local function buf_line(bufnr, line)
  local lines = vim.api.nvim_buf_get_lines(bufnr, line, line + 1, true)
  return lines[1] or ''
end

local function apply_ranges(bufnr, ranges, ns, group, priority)
  for _, range in ipairs(ranges) do
    local s = range.start
    local e = range['end']
    if not s or not e then
      goto continue
    end
    local start_line = buf_line(bufnr, s.line)
    local end_line = buf_line(bufnr, e.line)
    local start_col = to_byte_col(start_line, s.character)
    local end_col = to_byte_col(end_line, e.character)
    vim.api.nvim_buf_set_extmark(bufnr, ns, s.line, start_col, {
      end_line = e.line,
      end_col = end_col,
      hl_group = group,
      priority = priority,
    })
    ::continue::
  end
end

local function apply_semantic(bufnr, payload)
  vim.api.nvim_buf_clear_namespace(bufnr, ns_semantic, 0, -1)
  local tokens = payload.tokens or {}
  for _, token in ipairs(tokens) do
    local range = token.range
    if range then
      local group = token_groups[token.type] or 'MozukuUnknown'
      apply_ranges(bufnr, { range }, ns_semantic, group, 200)
    end
  end
end

local function apply_comment(bufnr, payload)
  vim.api.nvim_buf_clear_namespace(bufnr, ns_comment, 0, -1)
  local ranges = payload.ranges or {}
  apply_ranges(bufnr, ranges, ns_comment, 'MozukuComment', 200)
end

local function apply_content(bufnr, payload, has_semantic)
  if has_semantic then
    vim.api.nvim_buf_clear_namespace(bufnr, ns_content, 0, -1)
    return
  end
  vim.api.nvim_buf_clear_namespace(bufnr, ns_content, 0, -1)
  local ranges = payload.ranges or {}
  apply_ranges(bufnr, ranges, ns_content, 'MozukuContent', 200)
end

local function apply_for_uri(uri)
  local bufnr = vim.uri_to_bufnr(uri)
  if not vim.api.nvim_buf_is_loaded(bufnr) then
    return
  end
  local semantic = state.semantic[uri]
  local comment = state.comment[uri]
  local content = state.content[uri]

  if semantic then
    apply_semantic(bufnr, semantic)
  else
    vim.api.nvim_buf_clear_namespace(bufnr, ns_semantic, 0, -1)
  end

  if comment then
    apply_comment(bufnr, comment)
  else
    vim.api.nvim_buf_clear_namespace(bufnr, ns_comment, 0, -1)
  end

  if content then
    apply_content(bufnr, content, semantic and (#(semantic.tokens or {}) > 0))
  else
    vim.api.nvim_buf_clear_namespace(bufnr, ns_content, 0, -1)
  end
end

function M.setup()
  vim.lsp.handlers['mozuku/commentHighlights'] = function(_, result)
    if not result or not result.uri then
      return
    end
    state.comment[result.uri] = result
    apply_for_uri(result.uri)
  end

  vim.lsp.handlers['mozuku/contentHighlights'] = function(_, result)
    if not result or not result.uri then
      return
    end
    state.content[result.uri] = result
    apply_for_uri(result.uri)
  end

  vim.lsp.handlers['mozuku/semanticHighlights'] = function(_, result)
    if not result or not result.uri then
      return
    end
    state.semantic[result.uri] = result
    apply_for_uri(result.uri)
  end
end

function M.start(bufnr, config)
  if vim.b[bufnr].mozuku_attached then
    return
  end

  local path = vim.api.nvim_buf_get_name(bufnr)
  local root = nil
  local git = vim.fs.find('.git', { path = vim.fs.dirname(path), upward = true })
  if git and git[1] then
    root = vim.fs.dirname(git[1])
  end
  if not root or root == '' then
    root = vim.fs.dirname(path)
  end

  local server_path = config.server_path or 'mozuku-lsp'
  local cmd
  if type(server_path) == 'table' then
    cmd = server_path
  else
    cmd = { server_path }
  end

  vim.lsp.start({
    name = 'mozuku',
    cmd = cmd,
    root_dir = root,
    init_options = config.init_options or {},
    get_language_id = language_id_for,
    on_init = function(client)
      -- Disable built-in semantic tokens; mozuku uses custom notifications instead
      client.server_capabilities.semanticTokensProvider = nil
    end,
    on_attach = function(_, b)
      vim.b[b].mozuku_attached = true
    end,
  }, {
    bufnr = bufnr,
  })
end

function M.apply(bufnr)
  local uri = vim.uri_from_bufnr(bufnr)
  if uri then
    apply_for_uri(uri)
  end
end

return M
