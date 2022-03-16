Part of SDL2 for quasi88.

folder [Contents]
for macOSⅩ build for double clickable aplication resource.

script [sdl2-config-mac]
install SDL2 for FrameWorks, but not include sdl2-config.
this script for --lib, --cflags LINKER option, for FrameWorks SDL2.

for use
edit MakeFile option for macOSⅩ.
 SDL2_VERSION	= 1
 ARCH = macosx
 SOUND_SDL2		= 1

and then .....
$ make
$ make mac

joy for your Mac life!

8/27/2021 from kameya.

