#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include "pspvshbridge.h"

#include <string.h>

PSP_MODULE_INFO("daxRecovery_app", 0x1000, 1, 0);

PSP_MAIN_THREAD_ATTR(0);

#define printf pspDebugScreenPrintf

char buf[16384];

#define PROGRAM "ms0:/PSP/GAME/UPDATE/EBOOT.PBP"

int copy_file(char *input, char *output)
{
	SceUID i = sceIoOpen(input, PSP_O_RDONLY, 0777);	

	if (i < 0)
		return -1;

	sceIoRemove(output);

	SceUID o = sceIoOpen(output, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

	if (o < 0)
		return -1;

	int read;

	while ((read = sceIoRead(i, buf, 16384)) > 0)
	{
		sceIoWrite(o, buf, read);
	}

	sceIoClose(i);
	sceIoClose(o);

	return 0;
}

int usbStarted=0;

void start_usb()
{
	pspSdkLoadStartModule("flash0:/kd/semawm.prx", PSP_MEMORY_PARTITION_KERNEL);
	pspSdkLoadStartModule("flash0:/kd/usbstor.prx", PSP_MEMORY_PARTITION_KERNEL);
	pspSdkLoadStartModule("flash0:/kd/usbstormgr.prx", PSP_MEMORY_PARTITION_KERNEL);
	pspSdkLoadStartModule("flash0:/kd/usbstorms.prx", PSP_MEMORY_PARTITION_KERNEL);
	pspSdkLoadStartModule("flash0:/kd/usbstorboot.prx", PSP_MEMORY_PARTITION_KERNEL);

	sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, 0);
	sceUsbStart(PSP_USBSTOR_DRIVERNAME, 0, 0);
	sceUsbstorBootSetCapacity(0x800000);

	sceUsbActivate(0x1c8);

	usbStarted = 1;
}

void stop_usb()
{
	if (usbStarted)
	{
		sceUsbDeactivate(0);
		sceUsbStop(PSP_USBSTOR_DRIVERNAME, 0, 0);
		sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, 0);

		usbStarted = 0;
	}
}

int main()
{
	pspDebugScreenInit();
	
	pspDebugScreenSetBackColor(0xFF0000);
	pspDebugScreenSetTextColor(0x00FFFF);
	pspDebugScreenClear();

	printf("Recovery Mode.\n\n\n\n");
	printf("Press start to activate USB Mass.\n");
	printf("Press triangle to flash ms0:/index.dat to flash0.\n");
	printf("Press cross to start the program under ms0:/PSP/GAME/UPDATE/EBOOT.PBP\n");
	printf("Press home to exit.\n\n\n\n");

	sceIoUnassign("flash0:");
	sceIoAssign("flash0:", "lflash0:0,0", "flashfat0:", IOASSIGN_RDWR, NULL, 0);

	while (1)
	{
		SceCtrlData pad;

		int keyprocessed = 0;

		sceCtrlReadBufferPositive(&pad, 1);

		if (pad.Buttons & PSP_CTRL_START)
		{
			start_usb();
			printf("Usb started.\n");
			keyprocessed = 1;
		}
		else if (pad.Buttons & PSP_CTRL_TRIANGLE)
		{
			if (copy_file("ms0:/index.dat", "flash0:/vsh/etc/index.dat") < 0)
				printf("Cannot copy file. (file missing?).\n");
			else
				printf("File copied succesfully.\n");

			keyprocessed = 1;
		}
		else if (pad.Buttons & PSP_CTRL_CROSS)
		{
			struct SceKernelLoadExecVSHParam param;

			memset(&param, 0, sizeof(param));

			param.size = sizeof(param);
			param.args = strlen(PROGRAM)+1;
			param.argp = PROGRAM;
			param.key = "updater";

			printf("Starting program...\n");
			vshKernelLoadExecVSHMs1(PROGRAM, &param);

			keyprocessed = 1;
		}
		else if (pad.Buttons & PSP_CTRL_HOME)
		{
			break;
		}

		sceKernelDelayThread((keyprocessed) ? 200000 : 50000);
	}

	vshKernelExitVSHVSH(NULL);

	return 0;
}

