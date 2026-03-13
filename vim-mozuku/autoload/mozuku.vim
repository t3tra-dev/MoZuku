let s:semantic_types = [
      \ 'noun',
      \ 'verb',
      \ 'adjective',
      \ 'adverb',
      \ 'particle',
      \ 'aux',
      \ 'conjunction',
      \ 'symbol',
      \ 'interj',
      \ 'prefix',
      \ 'suffix',
      \ 'unknown',
      \ ]

function! mozuku#init() abort
  call mozuku#ensure_config()
  call mozuku#ensure_highlights()

  if has('nvim')
    call mozuku#nvim_setup()
  elseif exists('*lsp#register_server')
    call mozuku#vim_lsp_setup()
  else
    if !exists('g:mozuku_warned')
      echohl WarningMsg
      echom '[mozuku] Neovim 0.5+ または vim-lsp が必要です。Vim組み込みLSPへの対応は未確定です。'
      echohl None
      let g:mozuku_warned = 1
    endif
  endif
endfunction

function! mozuku#ensure_config() abort
  if !exists('g:mozuku_server_path')
    let g:mozuku_server_path = 'mozuku-lsp'
  endif
  if !exists('g:mozuku_mecab_dicdir')
    let g:mozuku_mecab_dicdir = ''
  endif
  if !exists('g:mozuku_mecab_charset')
    let g:mozuku_mecab_charset = 'UTF-8'
  endif
  if !exists('g:mozuku_analysis_enable_cabocha')
    let g:mozuku_analysis_enable_cabocha = 1
  endif
  if !exists('g:mozuku_analysis_grammar_check')
    let g:mozuku_analysis_grammar_check = 1
  endif
  if !exists('g:mozuku_analysis_min_japanese_ratio')
    let g:mozuku_analysis_min_japanese_ratio = 0.1
  endif
  if !exists('g:mozuku_analysis_warning_min_severity')
    let g:mozuku_analysis_warning_min_severity = 2
  endif

  if !exists('g:mozuku_warnings')
    let g:mozuku_warnings = {}
  endif
  call s:default_bool(g:mozuku_warnings, 'particleDuplicate', 1)
  call s:default_bool(g:mozuku_warnings, 'particleSequence', 1)
  call s:default_bool(g:mozuku_warnings, 'particleMismatch', 1)
  call s:default_bool(g:mozuku_warnings, 'sentenceStructure', 0)
  call s:default_bool(g:mozuku_warnings, 'styleConsistency', 0)
  call s:default_bool(g:mozuku_warnings, 'redundancy', 0)

  if !exists('g:mozuku_rules')
    let g:mozuku_rules = {}
  endif
  call s:default_bool(g:mozuku_rules, 'commaLimit', 1)
  call s:default_bool(g:mozuku_rules, 'adversativeGa', 1)
  call s:default_bool(g:mozuku_rules, 'duplicateParticleSurface', 1)
  call s:default_bool(g:mozuku_rules, 'adjacentParticles', 1)
  call s:default_bool(g:mozuku_rules, 'conjunctionRepeat', 1)
  call s:default_bool(g:mozuku_rules, 'raDropping', 1)
  call s:default_num(g:mozuku_rules, 'commaLimitMax', 3)
  call s:default_num(g:mozuku_rules, 'adversativeGaMax', 1)
  call s:default_num(g:mozuku_rules, 'duplicateParticleSurfaceMaxRepeat', 1)
  call s:default_num(g:mozuku_rules, 'adjacentParticlesMaxRepeat', 1)
  call s:default_num(g:mozuku_rules, 'conjunctionRepeatMax', 1)
endfunction

function! s:default_bool(dict, key, value) abort
  if !has_key(a:dict, a:key)
    let a:dict[a:key] = a:value
  endif
endfunction

function! s:default_num(dict, key, value) abort
  if !has_key(a:dict, a:key)
    let a:dict[a:key] = a:value
  endif
endfunction

function! mozuku#ensure_highlights() abort
  highlight default MozukuNoun guifg=#c8c8c8 ctermfg=250
  highlight default MozukuVerb guifg=#569cd6 ctermfg=75
  highlight default MozukuAdjective guifg=#4fc1ff ctermfg=81
  highlight default MozukuAdverb guifg=#9cdcfe ctermfg=117
  highlight default MozukuParticle guifg=#d16969 ctermfg=167
  highlight default MozukuAux guifg=#87ceeb ctermfg=117
  highlight default MozukuConjunction guifg=#d7ba7d ctermfg=180
  highlight default MozukuSymbol guifg=#808080 ctermfg=244
  highlight default MozukuInterj guifg=#b5cea8 ctermfg=151
  highlight default MozukuPrefix guifg=#c8c8c8 ctermfg=250
  highlight default MozukuSuffix guifg=#c8c8c8 ctermfg=250
  highlight default MozukuUnknown guifg=#aaaaaa ctermfg=248
  highlight default MozukuComment gui=NONE cterm=NONE
  highlight default MozukuContent gui=NONE cterm=NONE

  if exists('*prop_type_add')
    call s:ensure_prop_types()
  endif
endfunction

function! s:ensure_prop_types() abort
  if exists('g:mozuku_prop_types_initialized')
    return
  endif
  let g:mozuku_prop_types_initialized = 1

  call prop_type_add('MozukuComment', {'highlight': 'MozukuComment', 'priority': 200})
  call prop_type_add('MozukuContent', {'highlight': 'MozukuContent', 'priority': 200})

  for l:type in s:semantic_types
    call prop_type_add('Mozuku' . s:capitalize(l:type), {'highlight': 'Mozuku' . s:capitalize(l:type), 'priority': 200})
  endfor
endfunction

function! s:capitalize(word) abort
  return toupper(strpart(a:word, 0, 1)) . strpart(a:word, 1)
endfunction

function! s:path_has_sep(path) abort
  return a:path =~# '[/\\]'
endfunction

function! s:is_windows() abort
  return has('win32') || has('win64')
endfunction

function! s:exe_name(command_name) abort
  if s:is_windows() && a:command_name !~? '\.exe$'
    return a:command_name . '.exe'
  endif
  return a:command_name
endfunction

function! s:add_unique(list, value) abort
  if empty(a:value)
    return
  endif
  if index(a:list, a:value) < 0
    call add(a:list, a:value)
  endif
endfunction

function! s:repo_root() abort
  return fnamemodify(expand('<sfile>:p'), ':h:h:h')
endfunction

function! s:resolve_explicit_path(path_value) abort
  if empty(a:path_value)
    return ''
  endif
  return fnamemodify(a:path_value, ':p')
endfunction

function! s:command_candidates(command_name) abort
  let l:candidates = []
  if empty(a:command_name)
    return l:candidates
  endif

  let l:exe = s:exe_name(a:command_name)
  let l:repo_root = s:repo_root()

  " 開発中バイナリを優先: ワークスペース出力 -> 拡張同梱 -> システムPATH
  call s:add_unique(l:candidates, l:repo_root . '/mozuku-lsp/build/' . l:exe)
  call s:add_unique(l:candidates, l:repo_root . '/mozuku-lsp/build/install/bin/' . l:exe)
  call s:add_unique(l:candidates, l:repo_root . '/vscode-mozuku/bin/' . l:exe)
  call s:add_unique(l:candidates, l:repo_root . '/build/' . l:exe)
  call s:add_unique(l:candidates, l:repo_root . '/build/install/bin/' . l:exe)

  for l:name in [a:command_name, l:exe]
    let l:resolved = exepath(l:name)
    if !empty(l:resolved)
      call s:add_unique(l:candidates, l:resolved)
    endif
  endfor

  if exists('$HOME')
    call s:add_unique(l:candidates, expand('~/.local/bin/' . l:exe))
    call s:add_unique(l:candidates, expand('~/bin/' . l:exe))
  endif

  if has('macunix')
    for l:dir in ['/usr/local/bin', '/usr/bin', '/opt/homebrew/bin', '/opt/local/bin']
      call s:add_unique(l:candidates, l:dir . '/' . l:exe)
    endfor
  elseif has('unix')
    for l:dir in ['/usr/local/bin', '/usr/bin']
      call s:add_unique(l:candidates, l:dir . '/' . l:exe)
    endfor
  endif

  if s:is_windows()
    for l:base in [expand('$LOCALAPPDATA'), expand('$ProgramFiles'), expand('$ProgramFiles(x86)')]
      if empty(l:base)
        continue
      endif
      for l:name in ['MoZuku', 'mozuku-lsp']
        call s:add_unique(l:candidates, l:base . '/' . l:name . '/bin/' . l:exe)
      endfor
    endfor
  endif

  return l:candidates
endfunction

function! mozuku#config() abort
  return {
        \ 'server_path': mozuku#server_cmd(),
        \ 'init_options': mozuku#build_init_options(),
        \ }
endfunction

function! mozuku#vim_allowlist() abort
  let l:allowlist = copy(g:mozuku_filetypes)
  for l:filetype in ['text', 'markdown']
    if index(l:allowlist, l:filetype) < 0
      call add(l:allowlist, l:filetype)
    endif
  endfor
  return l:allowlist
endfunction

function! mozuku#is_ja_document(bufnr) abort
  let l:path = bufname(a:bufnr)
  return l:path =~# '\.ja\.\(txt\|md\)$'
endfunction

function! mozuku#vim_language_id(server_info) abort
  let l:filetype = getbufvar(bufnr('%'), '&filetype')
  if l:filetype ==# 'tex' || l:filetype ==# 'plaintex'
    return 'latex'
  endif
  return l:filetype
endfunction

function! mozuku#vim_server_cmd(server_info) abort
  let l:bufnr = bufnr('%')
  let l:filetype = getbufvar(l:bufnr, '&filetype')

  if index(['text', 'markdown'], l:filetype) >= 0 && !mozuku#is_ja_document(l:bufnr)
    return []
  endif

  return mozuku#server_cmd()
endfunction

function! mozuku#build_init_options() abort
  return {
        \ 'mozuku': {
        \   'mecab': {
        \     'dicdir': g:mozuku_mecab_dicdir,
        \     'charset': g:mozuku_mecab_charset,
        \   },
        \   'analysis': {
        \     'enableCaboCha': g:mozuku_analysis_enable_cabocha,
        \     'grammarCheck': g:mozuku_analysis_grammar_check,
        \     'minJapaneseRatio': g:mozuku_analysis_min_japanese_ratio,
        \     'warningMinSeverity': g:mozuku_analysis_warning_min_severity,
        \     'warnings': copy(g:mozuku_warnings),
        \     'rules': copy(g:mozuku_rules),
        \   },
        \ }
        \ }
endfunction

function! mozuku#maybe_start(force) abort
  if exists('b:mozuku_attached') && b:mozuku_attached
    return
  endif
  if !a:force && index(g:mozuku_filetypes, &filetype) < 0
    return
  endif

  if has('nvim')
    let l:config = mozuku#config()
    call luaeval("require('mozuku').start(_A.bufnr, _A.config)", {'bufnr': bufnr('%'), 'config': l:config})
    let b:mozuku_attached = 1
    return
  endif

  if exists('*lsp#register_server')
    call mozuku#vim_lsp_setup()
    if exists('*lsp#enable')
      call lsp#enable()
    endif
    let b:mozuku_attached = 1
    return
  endif
endfunction

function! mozuku#apply_current() abort
  if has('nvim')
    call luaeval("require('mozuku').apply(_A)", bufnr('%'))
    return
  endif
  call mozuku#vim_apply_from_state(bufnr('%'))
endfunction

function! mozuku#nvim_setup() abort
  if exists('g:mozuku_nvim_initialized')
    return
  endif
  let g:mozuku_nvim_initialized = 1
  call luaeval("require('mozuku').setup()")
endfunction

function! mozuku#vim_lsp_setup() abort
  if exists('g:mozuku_vim_lsp_registered')
    return
  endif
  let g:mozuku_vim_lsp_registered = 1

  if !exists('*lsp#register_server')
    return
  endif

  let l:init = mozuku#build_init_options()

  call lsp#register_server({
        \ 'name': 'mozuku',
        \ 'cmd': function('mozuku#vim_server_cmd'),
        \ 'allowlist': mozuku#vim_allowlist(),
        \ 'initialization_options': l:init,
        \ 'languageId': function('mozuku#vim_language_id'),
        \ })

  if exists('*lsp#register_notification_handler')
    call lsp#register_notification_handler('mozuku/commentHighlights', function('mozuku#vim_on_comment'))
    call lsp#register_notification_handler('mozuku/contentHighlights', function('mozuku#vim_on_content'))
    call lsp#register_notification_handler('mozuku/semanticHighlights', function('mozuku#vim_on_semantic'))
  endif
endfunction

function! mozuku#server_cmd() abort
  if type(g:mozuku_server_path) == v:t_list
    return copy(g:mozuku_server_path)
  endif

  let l:configured = trim(g:mozuku_server_path)
  if !empty(l:configured) && s:path_has_sep(l:configured)
    return [s:resolve_explicit_path(l:configured)]
  endif

  let l:env = exists('$MOZUKU_LSP') ? trim($MOZUKU_LSP) : ''
  if !empty(l:env) && s:path_has_sep(l:env)
    return [s:resolve_explicit_path(l:env)]
  endif

  for l:command_name in [l:configured, l:env, 'mozuku-lsp']
    if empty(l:command_name)
      continue
    endif
    for l:candidate in s:command_candidates(l:command_name)
      if executable(l:candidate)
        return [fnamemodify(l:candidate, ':p')]
      endif
    endfor
  endfor

  if !empty(l:configured)
    return [l:configured]
  endif
  if !empty(l:env)
    return [l:env]
  endif
  return ['mozuku-lsp']
endfunction

function! mozuku#vim_on_comment(server, payload) abort
  call mozuku#vim_store_and_apply('comment', a:payload)
endfunction

function! mozuku#vim_on_content(server, payload) abort
  call mozuku#vim_store_and_apply('content', a:payload)
endfunction

function! mozuku#vim_on_semantic(server, payload) abort
  call mozuku#vim_store_and_apply('semantic', a:payload)
endfunction

function! mozuku#vim_store_and_apply(kind, payload) abort
  if type(a:payload) != v:t_dict || !has_key(a:payload, 'uri')
    return
  endif
  let l:uri = a:payload['uri']
  if !exists('g:mozuku_state')
    let g:mozuku_state = {}
  endif
  if !has_key(g:mozuku_state, a:kind)
    let g:mozuku_state[a:kind] = {}
  endif
  let g:mozuku_state[a:kind][l:uri] = a:payload

  let l:bufnr = mozuku#uri_to_bufnr(l:uri)
  if l:bufnr > 0
    call mozuku#vim_apply_from_state(l:bufnr)
  endif
endfunction

function! mozuku#vim_apply_from_state(bufnr) abort
  if !exists('g:mozuku_state')
    return
  endif
  let l:uri = mozuku#bufnr_to_uri(a:bufnr)
  if l:uri ==# ''
    return
  endif

  if has_key(g:mozuku_state, 'semantic') && has_key(g:mozuku_state['semantic'], l:uri)
    call mozuku#vim_apply_semantic(a:bufnr, g:mozuku_state['semantic'][l:uri])
  else
    call mozuku#vim_clear_semantic(a:bufnr)
    call setbufvar(a:bufnr, 'mozuku_has_semantic', 0)
  endif

  if has_key(g:mozuku_state, 'comment') && has_key(g:mozuku_state['comment'], l:uri)
    call mozuku#vim_apply_comment(a:bufnr, g:mozuku_state['comment'][l:uri])
  else
    call mozuku#vim_clear_comment(a:bufnr)
  endif

  if has_key(g:mozuku_state, 'content') && has_key(g:mozuku_state['content'], l:uri)
    if getbufvar(a:bufnr, 'mozuku_has_semantic', 0)
      call mozuku#vim_clear_content(a:bufnr)
    else
      call mozuku#vim_apply_content(a:bufnr, g:mozuku_state['content'][l:uri])
    endif
  else
    call mozuku#vim_clear_content(a:bufnr)
  endif
endfunction

function! mozuku#vim_apply_comment(bufnr, payload) abort
  call mozuku#vim_clear_comment(a:bufnr)
  if !has_key(a:payload, 'ranges')
    return
  endif
  call s:vim_apply_ranges(a:bufnr, a:payload['ranges'], 'MozukuComment')
endfunction

function! mozuku#vim_apply_content(bufnr, payload) abort
  call mozuku#vim_clear_content(a:bufnr)
  if !has_key(a:payload, 'ranges')
    return
  endif
  call s:vim_apply_ranges(a:bufnr, a:payload['ranges'], 'MozukuContent')
endfunction

function! mozuku#vim_apply_semantic(bufnr, payload) abort
  call mozuku#vim_clear_semantic(a:bufnr)
  call setbufvar(a:bufnr, 'mozuku_has_semantic', 0)
  if !has_key(a:payload, 'tokens')
    return
  endif
  for l:token in a:payload['tokens']
    if !has_key(l:token, 'range') || !has_key(l:token, 'type')
      continue
    endif
    let l:group = 'Mozuku' . s:capitalize(l:token['type'])
    call s:vim_apply_ranges(a:bufnr, [l:token['range']], l:group)
    call setbufvar(a:bufnr, 'mozuku_has_semantic', 1)
  endfor
endfunction

function! mozuku#vim_clear_comment(bufnr) abort
  if !exists('*prop_remove')
    return
  endif
  call prop_remove({'type': 'MozukuComment', 'bufnr': a:bufnr})
endfunction

function! mozuku#vim_clear_content(bufnr) abort
  if !exists('*prop_remove')
    return
  endif
  call prop_remove({'type': 'MozukuContent', 'bufnr': a:bufnr})
endfunction

function! mozuku#vim_clear_semantic(bufnr) abort
  if !exists('*prop_remove')
    return
  endif
  for l:type in s:semantic_types
    let l:prop = 'Mozuku' . s:capitalize(l:type)
    call prop_remove({'type': l:prop, 'bufnr': a:bufnr})
  endfor
endfunction

function! s:vim_apply_ranges(bufnr, ranges, group) abort
  if !exists('*prop_add')
    return
  endif
  for l:range in a:ranges
    if !has_key(l:range, 'start') || !has_key(l:range, 'end')
      continue
    endif
    let l:start = l:range['start']
    let l:end = l:range['end']

    let l:start_lnum = l:start['line'] + 1
    let l:end_lnum = l:end['line'] + 1
    let l:start_list = getbufline(a:bufnr, l:start_lnum)
    let l:end_list = getbufline(a:bufnr, l:end_lnum)
    if empty(l:start_list) || empty(l:end_list)
      continue
    endif
    let l:start_line = l:start_list[0]
    let l:end_line = l:end_list[0]

    let l:start_col = mozuku#utf16_to_byte(l:start_line, l:start['character']) + 1
    let l:end_col = mozuku#utf16_to_byte(l:end_line, l:end['character']) + 1

    if l:start_col < 1
      let l:start_col = 1
    endif
    if l:end_col < 1
      let l:end_col = 1
    endif

    call prop_add(l:start_lnum, l:start_col, {
          \ 'end_lnum': l:end_lnum,
          \ 'end_col': l:end_col,
          \ 'type': a:group,
          \ 'bufnr': a:bufnr,
          \ })
  endfor
endfunction

function! mozuku#utf16_to_byte(line, utf16_col) abort
  if a:utf16_col <= 0
    return 0
  endif
  let l:chars = strchars(a:line)
  let l:acc = 0
  for l:i in range(0, l:chars - 1)
    if l:acc >= a:utf16_col
      return byteidx(a:line, l:i)
    endif
    let l:char = strcharpart(a:line, l:i, 1)
    let l:code = char2nr(l:char)
    if l:code > 0xFFFF
      let l:acc += 2
    else
      let l:acc += 1
    endif
  endfor
  return strlen(a:line)
endfunction

function! mozuku#uri_to_bufnr(uri) abort
  let l:path = mozuku#uri_to_path(a:uri)
  if l:path ==# ''
    return -1
  endif
  return bufnr(l:path)
endfunction

function! mozuku#bufnr_to_uri(bufnr) abort
  let l:path = bufname(a:bufnr)
  if l:path ==# ''
    return ''
  endif
  if exists('*lsp#utils#path_to_uri')
    return lsp#utils#path_to_uri(l:path)
  endif
  let l:path = substitute(fnamemodify(l:path, ':p'), '\\', '/', 'g')
  return 'file://' . l:path
endfunction

function! mozuku#uri_to_path(uri) abort
  if exists('*lsp#utils#uri_to_path')
    return lsp#utils#uri_to_path(a:uri)
  endif
  if a:uri !~# '^file://'
    return a:uri
  endif
  let l:path = substitute(a:uri, '^file://', '', '')
  let l:path = s:url_decode(l:path)
  if has('win32') && l:path =~# '^/[A-Za-z]:'
    let l:path = l:path[1:]
  endif
  return l:path
endfunction

function! s:url_decode(str) abort
  return substitute(a:str, '%\(\x\x\)', '\=nr2char("0x" . submatch(1))', 'g')
endfunction
