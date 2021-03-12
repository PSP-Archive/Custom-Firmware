Custom Firmware - The proof of concept by Dark_AleX

There has been lately some discussion about if custom firmware in the psp are posible
(note: custom in the sense of writing our own executable code in the system, and not only data 
like fonts, videos,...)

Even a lot of developers doubt that this is possible due to the protections in the psp.
Well, i'm here with this simple proof of concept to show the contrary :P

Instructions:
(Note: this thing only works for 1.50)

- Copy the PSP folder to the root of your memstick.
- Execute the program "Custom Firmware - The proof of concept"
  It will write to the flash some executable files.

Now you are done.

This program has ben tested on my psp. However, as all programs that write to the flash, there is 
always some risk of bricking the psp. Use it at your own risk.
IMPORTANT (to avoid confussion): the program includes a file called vshmain.prx. This file is
NOT of Sony, it comes from me, the source of everything is released.

The features of this custom firmware - proof of concept are:

- Execution of normal pbp's (1.00 ones, no-kxploited)
- Posibility of hiding corrupt icons. (note: by default it's not enabled, look at the file 
/PSP/SYSTEM/config.txt to see how to enable it)
- Posibility of skipping the SCE logo at the startup. (and in this way avoid the annoying auto-execution
of the UMD). Like the previous one, this is not enabled by default.
- "Recovery mode:". If you keep R pressed while starting the psp, it will enter in "Recovery mode".
Recovery mode is not more than a simple application that will let you to use the usb, and execute a 
program under ms0:/PSP/GAME/UPDATE/EBOOT.PBP, that could be, for example, a sony updater or the
1.50 -> 1.00 downdater.

This would allow to recover potential bricks caused by the writing of bad fonts/videos/sounds... to 
the flash.
(Note that this wouldn't allow to recover critical things like the overwriting of critical prx's)

- Autoexecution of a program at the startup. See the configuration file at /PSP/SYSTEM/config.txt
  to see how to use it.


Technical Details for Developers - Why and how this work.

Consider vshmain.prx like the executable of the firmware. 
You should know that either in 1.00 or 1.50 we CANNOT execute our own prx's.
However, you should know that we can execute our own elf's :)

Just creating an elf, renaming it to vshmain.prx and flashing the file does the job :)

However, notice a VERY IMPORTANT thing. By default, static elf's created by the pspsdk are 
linked to the address 0x08900000. This address is already taken by a prx of those that are loaded
before vshmain.prx.
It's necesary to relocate the elf to a different address not already taken.
This proof of concept use the address 0x09CD3000.
