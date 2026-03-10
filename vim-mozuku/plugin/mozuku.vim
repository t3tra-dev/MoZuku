if exists('g:loaded_mozuku')
  finish
endif
let g:loaded_mozuku = 1

if !exists('g:mozuku_filetypes')
  let g:mozuku_filetypes = [
        \ 'japanese',
        \ 'c',
        \ 'cpp',
        \ 'html',
        \ 'python',
        \ 'javascript',
        \ 'javascriptreact',
        \ 'typescript',
        \ 'typescriptreact',
        \ 'rust',
        \ 'tex',
        \ 'plaintex',
        \ 'latex',
        \ ]
endif

call mozuku#init()

augroup mozuku
  autocmd!
  execute 'autocmd FileType ' . join(g:mozuku_filetypes, ',') . ' call mozuku#maybe_start(0)'
  autocmd BufEnter *.ja.txt,*.ja.md call mozuku#maybe_start(1)
  autocmd BufEnter * call mozuku#apply_current()
augroup END

command! MozukuStart call mozuku#maybe_start(1)
command! MozukuRefresh call mozuku#apply_current()
