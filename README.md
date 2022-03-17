# QUASI88-SDL2

A port of QUASI88 to the SDL2.

based on [original version 0.6.4](https://www.eonet.ne.jp/~showtime/quasi88/) and [SDL2 patch](http://kameya-z.way-nifty.com/blog/2021/08/post-bf1e89.html)

<img alt="2022-03-17 17 59 43" src="https://user-images.githubusercontent.com/493908/158774286-bcc22dc7-1f82-4eb9-b966-fb3647252955.png">
PC-8801mkII demo (that i vacuumed, this version may not have been pirated bec i have never seen this on the web.)

## Install

 * on Mac

```
$ brew install sdl2
$ vi Makefile
$ make
```

## Copyright

QUASI88 is an emulator by Showzoh Fukunaga licensed under the BSD 3-Clause license. This libretro port is distributed in the same way.

The sound processing portion of QUASI88 uses source code from MAME and XMAME. The copyright to this source code belongs to its corresponding authors. Please refer to license/MAME.TXT for licensing information.

The sound processing portion of QUASI88 also uses source code from the FM audio generator "fmgen". The copyright to this source code belongs to cisc. Please refer to license/FMGEN.TXT (in Japanese) for licensing information.

SDL2 part by [kameya-z](http://kameya-z.way-nifty.com/blog/2021/08/post-bf1e89.html)
