#include <pspsdk.h>
#include <pspkernel.h>
#include <psputils.h>
#include <pspctrl.h>

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

PSP_MODULE_INFO("daxFlasher_app", 0x0800, 1, 0);

PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VSH);

#define printf pspDebugScreenPrintf

void ErrorExit(int milisecs, char *fmt, ...)
{
	va_list list;
	char msg[256];	

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	printf(msg);

	sceKernelDelayThread(milisecs*1000);
	sceKernelExitGame();
}

#define VSHMAIN_SIZE		30368
#define VSHMAIN_REAL_SIZE	72032
#define RECOVERY_SIZE		40144	

char vshmain_buffer[VSHMAIN_SIZE];
char vshmain_real_buffer[VSHMAIN_REAL_SIZE];
char recovery_buffer[RECOVERY_SIZE];

u8 vshmain_md5[16] =
{
	0x00, 0x10, 0xB5, 0xF2, 0xFF, 0x38, 0x15, 0xAC,
	0xBD, 0x21, 0xE6, 0x87, 0xA9, 0xF1, 0x3C, 0xDE
};

u8 vshmain_real_md5[16] =
{
	0x37, 0xF7, 0x4E, 0x1E, 0x4B, 0x26, 0xD2, 0x57,
	0x93, 0x07, 0xCB, 0x8D, 0xCD, 0xDB, 0xFF, 0x1C
};

u8 recovery_md5[16] =
{
	0xBF, 0xAF, 0x54, 0x14, 0x19, 0x36, 0xC6, 0x43,
	0x0B, 0x33, 0x99, 0x96, 0x92, 0xA3, 0xD1, 0x78
};

char buf[16536];

void copy_vshmain()
{
	SceUID i = sceIoOpen("flash0:/vsh/module/vshmain.prx", PSP_O_RDONLY, 0777);

	if (i < 0)
	{
		ErrorExit(4000, "Cannot copy vshmain.prx: input error.\n");
	}

	SceUID o = sceIoOpen("vshmain_real.prx", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

	if (o < 0)
	{
		ErrorExit(4000, "Cannot copy vshmain.prx: output error.\n");
	}

	int read;

	while ((read = sceIoRead(i, buf, 16536)) > 0)
	{
		sceIoWrite(o, buf, read);
	}

	sceIoClose(i);
	sceIoClose(o);
}

void read_file(char *file, u8 *md5, char *buf, int size)
{
	SceUID fd;
	int read;
	u8 digest[16];
	
	fd= sceIoOpen(file, PSP_O_RDONLY, 0777);

	if (fd < 0)
	{
		ErrorExit(4000, "Cannot read file %s.\n", file);
	}
	
	read = sceIoRead(fd, buf, size);

	if (read != size)
	{
		ErrorExit(4000, "File size of %s doesn't match.\n", file);
	}

	if (sceKernelUtilsMd5Digest((u8 *)buf, size, digest) < 0)
	{
		ErrorExit(4000, "Error calculating md5.\n");
	}

	if (memcmp(digest, md5, 16) != 0)
	{
		ErrorExit(4000, "MD5 of %s doesn't match.\n", file);
	}
}

void flash_file(char *file, char *buf, int size)
{
	SceUID fd = sceIoOpen(file, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

	if (fd >= 0)
	{
		sceIoWrite(fd, buf, size);
		sceIoClose(fd);
	}
	// else { better not to think we are going end here :P }
}

int main()
{
	pspDebugScreenInit();
	pspDebugScreenClear();
	pspDebugScreenSetTextColor(0x0000FF00);

	printf("Copying vshmain.prx as vshmain_real.prx.\n");
	copy_vshmain();

	printf("Copying files to buffers...\n");
	read_file("vshmain.prx", vshmain_md5, vshmain_buffer, VSHMAIN_SIZE);
	read_file("vshmain_real.prx", vshmain_real_md5, vshmain_real_buffer, VSHMAIN_REAL_SIZE);
	read_file("recovery.elf", recovery_md5, recovery_buffer, RECOVERY_SIZE);

	if (sceIoUnassign("flash0:") < 0)
		ErrorExit(4000, "Cannot unassign flash0.");

	if (sceIoAssign("flash0:", "lflash0:0,0", "flashfat0:", IOASSIGN_RDWR, NULL, 0) < 0)
	{
		ErrorExit(4000, "Error assigning flash0 in write mode.");
	}

	pspDebugScreenSetTextColor(0x000000FF);
	printf("Preparation done. Press X to flash the proof of concept at your own risk.\n");

	while (1)
	{
		SceCtrlData pad;

		sceCtrlReadBufferPositive(&pad, 1);

		if (pad.Buttons & PSP_CTRL_CROSS)
			break;

		sceKernelDelayThread(20000);
	}

	printf("Flashing files...\n");

	sceIoRemove("flash0:/vsh/module/vshmain.prx");

	flash_file("flash0:/vsh/module/vshmain.prx", vshmain_buffer, VSHMAIN_SIZE);
	flash_file("flash0:/vsh/module/vshmain_real.prx", vshmain_real_buffer, VSHMAIN_REAL_SIZE);
	flash_file("flash0:/kd/recovery.elf", recovery_buffer, RECOVERY_SIZE);


	ErrorExit(4000, "Done.\n");

	return 0;
}

