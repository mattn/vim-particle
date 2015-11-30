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

let s:ctb = [
\ '000000', 'aa0000', '00aa00', '0000aa', 'aa5500', 'aa00aa', '00aaaa', 'aaaaaa',
\ '555555', 'ff5555', '55ff55', 'ffff55', '5555ff', 'ff55ff', '55ffff', 'ffffff'
\]
let [s:oldx, s:oldy] = [0, 0]
function! s:particle()
  let [x, y] = [getwinposx(), getwinposy()]
  let x += s:rand() % 10 - 5
  let y += s:rand() % 10 - 5
  exe 'winpos' x y
  let c = synIDattr(synIDtrans(synID(line("."), col(".")-1, 1)), "fg")
  if c =~ '^#'
    let c = c[1:]
  elseif c =~ '^[0-9]\+$'
    let c = s:ctb[c]
  else
    let c = 'ffffff'
  endif

  let [x, y] = [screencol(), screenrow()]
  if x == 10000 || y == 10000
    let [x, y] = [s:oldx, s:oldy]
  endif
  silent exe "!start" printf("%s %d,%d,%d,%d,%d %s", s:exe, v:windowid, x, y, &columns, &lines, c)
  let [s:oldx, s:oldy] = [x, y]
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
