if has('win32')
  let s:exe = fnamemodify(expand('<sfile>:h:h') . '\particle.exe', ':p')
else
  let s:exe = fnamemodify(expand('<sfile>:h:h') . '/particle', ':p')
endif

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
let s:rainbow = [
\ 'ffb3b3', 'ffd9b3', 'ffffb3', 'b3ffb3', 'b3ffff', 'b3b3ff', 'ffb3ff'
\]
let s:rainbow_idx = 0
let [s:oldx, s:oldy] = [0, 0]
let s:job = v:null
let s:mode = ''

function! s:ensure_running() abort
  if s:job is v:null || job_status(s:job) != 'run'
    let n = get(g:, 'particle_count', 3)
    let cmd = [s:exe, '-w', string(v:windowid), '-n', string(n)]
    if s:mode == 'star' || s:mode == 'unko'
      let cmd += ['-mode', s:mode]
    endif
    let s:job = job_start(cmd, {'mode': 'raw'})
  endif
endfunction

function! s:get_color() abort
  if s:mode == 'rainbow' || s:mode == 'star'
    let c = s:rainbow[s:rainbow_idx]
    let s:rainbow_idx = (s:rainbow_idx + 1) % len(s:rainbow)
    return [c, 140]
  endif
  if s:mode == 'unko'
    return ['6B4226', 200]
  endif
  let c = synIDattr(synIDtrans(synID(line("."), col(".")-1, 1)), "fg")
  if c =~ '^#'
    return [c[1:], 70]
  elseif c =~ '^[0-9]\+$'
    return [s:ctb[c], 70]
  endif
  return ['ffffff', 70]
endfunction

function! s:particle()
  call s:ensure_running()
  let [x, y] = [getwinposx(), getwinposy()]
  let x += (abs(s:rand()) % 11 - 5)
  let y += (abs(s:rand()) % 11 - 5)
  exe 'winpos' x y
  let [c, a] = s:get_color()

  let [x, y] = [screencol(), screenrow()]
  if x == 10000 || y == 10000
    let [x, y] = [s:oldx, s:oldy]
  endif
  call ch_sendraw(job_getchannel(s:job), printf("%d,%d,%d,%d %s %d\n", x, y, &columns, &lines, c, a))
  let [s:oldx, s:oldy] = [x, y]
endfunction

function! s:install(mode)
  augroup ParticleVim
    au!
    if a:mode != ''
      if s:job isnot v:null && job_status(s:job) == 'run'
        call job_stop(s:job)
        let s:job = v:null
      endif
      let s:mode = a:mode
      let s:rainbow_idx = 0
      call s:ensure_running()
      au TextChangedI * call s:particle()
    else
      let s:mode = ''
      if s:job isnot v:null && job_status(s:job) == 'run'
        call job_stop(s:job)
      endif
      let s:job = v:null
    endif
  augroup END
endfunction

command! -nargs=0 ParticleOn call <SID>install('syntax')
command! -nargs=0 ParticleRainbow call <SID>install('rainbow')
command! -nargs=0 ParticleStar call <SID>install('star')
command! -nargs=0 ParticleUnko call <SID>install('unko')
command! -nargs=0 ParticleOff call <SID>install('')

let s:autostart_modes = {'syntax': 1, 'rainbow': 1, 'star': 1, 'unko': 1}
let s:autostart = get(g:, 'particle_autostart', '')
if has_key(s:autostart_modes, s:autostart)
  call s:install(s:autostart)
endif
