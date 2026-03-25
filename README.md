# vim-particle

https://github.com/user-attachments/assets/32e2178c-1e45-4009-a4b3-840ac4c73acf

https://github.com/user-attachments/assets/86bf5417-b378-4e41-af1c-1a97744e94df

https://github.com/user-attachments/assets/46cd3529-4aae-4dbe-9b65-29c81a016676

## Usage

```vim
:ParticleOn       " Syntax color particles
:ParticleRainbow  " Pastel rainbow particles
:ParticleUnko     " Poops particles
:ParticleOff      " Stop
```

### Options

```vim
let g:particle_count = 3  " Number of particles per keystroke (1-10, default: 3)
```

## Requirements

* Windows or Linux (X11)
* Linux requires a compositor (picom, compton, etc.) for transparency

## Installation

```
make
```

Pre-built binaries are available on the [Releases](../../releases) page.
Rename to `particle.exe` (Windows) or `particle` (Linux) and place in the plugin directory.

## License

MIT

## Author

Yasuhiro Matsumoto (a.k.a mattn)
