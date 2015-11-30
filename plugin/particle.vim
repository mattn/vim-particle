let s:exe = fnamemodify(expand('<sfile>:h:h') . '\particle.exe', ':p')

let s:seed = 0
function! s:srand(seed) abort
  let s:seed = a:seed
endfunction

function! s:rand() abort
  let s:seed = s:seed * 214013 + 2531011
  return (s:seed < 0 ? s:seed - 0x80000000 : s:seed) / 0x10000 % 0x8000
endfunction

call s:srand(localtime())

function! s:particle()
  let [x, y] = [getwinposx(), getwinposy()]
  let x += s:rand() % 10 - 5
  let y += s:rand() % 10 - 5
  exe 'winpos' x y
  let c = synIDattr(synIDtrans(synID(line("."), col(".")-1, 1)), "fg")
  if c =~ '^#'
    silent exe "!start" printf("%s %d %s", s:exe, v:windowid, c[1:])
  endif
endfunction

function! s:install(flag)
  augroup ParticleVim
    au!
    if a:flag
      au TextChangedI * call s:particle()
    endif
  augroup END
endfunction

command! -nargs=0 ParticleOn call <SID>install(1)
command! -nargs=0 ParticleOff call <SID>install(0)
