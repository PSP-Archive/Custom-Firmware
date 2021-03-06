#include <pspsdk.h>
#include <pspkernel.h>
#include <psputilsforkernel.h>
#include <pspctrl.h>
#include "pspvshbridge.h"

#include <stdio.h>
#include <string.h>

#include "conf.h"


PSP_MODULE_INFO("daxVSH_boot", 0x1000, 1, 0);

PSP_MAIN_THREAD_ATTR(0);


#define J_OPCODE	0x08000000
#define NOP			0x00000000			

#define CHANGE_FUNC(o, n) _sw(J_OPCODE | (((u32)(n) & 0x3FFFFFFF) >> 2), o); _sw(NOP, o+4);

SceUID sceKernelSearchModuleByName(const char *name);

typedef int (* LOADEXEC_VSH)(const char *file, struct SceKernelLoadExecVSHParam *param);

#define PBP_SIGNATURE	0x50425000
#define ELF_SIGNATURE	0x464C457F

typedef struct 
{ 
	u32 signature; 
	int version; 
	int offsets[8]; 
} PBPHeader;

char buf[65536];

#define TEMP_ELF "ms0:/__TEMP__.ELF"

/* Temporal patch to allow unsigned PBP: extract the ELF */
/* Note for the future: patch the kernel instead of this */
int do_loadexecvsh(const char *file, struct SceKernelLoadExecVSHParam *param, LOADEXEC_VSH func)
{
	PBPHeader header;

	SceUID pbp = sceIoOpen(file, PSP_O_RDONLY, 0777);

	if (pbp > 0)
	{
		sceIoRead(pbp, &header, sizeof(header));

		if (header.signature == PBP_SIGNATURE)
		{
			sceIoLseek32(pbp, header.offsets[6], PSP_SEEK_SET);

			int size = header.offsets[7]-header.offsets[6];

			int read = sceIoRead(pbp, buf, 2048);

			if (((u32 *)buf)[0] == ELF_SIGNATURE)
			{
				SceUID elf = sceIoOpen(TEMP_ELF, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

				if (elf > 0)
				{
					sceIoWrite(elf, buf, read);
					size -= read;

					while (size > 0)
					{
						read = sceIoRead(pbp, buf, sizeof(buf));

						if (read <= 0)
							break;

						sceIoWrite(elf, buf, sizeof(buf));
						size -= read;
					}

					sceIoClose(elf);

					file = TEMP_ELF;
				}
			}
		}

		sceIoClose(pbp);
	}

	return func(file, param);
}

int vshms1_patch(const char *file, struct SceKernelLoadExecVSHParam *param)
{
	return do_loadexecvsh(file, param, vshKernelLoadExecVSHMs1);	
}

int vshms2_patch(const char *file, struct SceKernelLoadExecVSHParam *param)
{
	return do_loadexecvsh(file, param, vshKernelLoadExecVSHMs2);	
}

int vshms3_patch(const char *file, struct SceKernelLoadExecVSHParam *param)
{
	return do_loadexecvsh(file, param, vshKernelLoadExecVSHMs3);	
}

void clear_cache()
{
	sceKernelDcacheWritebackAll();
	sceKernelIcacheInvalidateAll();
}

void nokxploit_patch(SceUID vshmod)
{
	SceKernelModuleInfo info;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);

	if (sceKernelQueryModuleInfo(vshmod, &info) >= 0)
	{
		/* Patch the imports to the vshKernelLoadExecVSHMs* functions */
		
		/* vshms1 for memstick updaters */
		CHANGE_FUNC(info.text_addr+0x1B6E8, vshms1_patch);
		/* vshms2 for homebrew/memstick games */
		CHANGE_FUNC(info.text_addr+0x1B6F0, vshms2_patch);
		/* vshms3 for... ? */
		CHANGE_FUNC(info.text_addr+0x1B6F8, vshms3_patch);

		clear_cache();
	}
}

SceUID theuid=-1;

int dopen_patch(const char *dirname)
{
	int res = sceIoDopen(dirname);

	if (strcmp(dirname, "ms0:/PSP/GAME") == 0)
	{
		theuid = res;
	}

	return res;
}

int dread_patch(SceUID dfd, SceIoDirent *dir)
{
	char path[512];

	if (dfd != theuid)
		return sceIoDread(dfd, dir);

	while (1)
	{		
		int res = sceIoDread(dfd, dir);
			
		if (res <= 0)
			return res;

		snprintf(path, 512, "ms0:/PSP/GAME/%s/EBOOT.PBP", dir->d_name);

		SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);

		if (fd > 0)
		{
			u32 signature;

			sceIoRead(fd, &signature, 4);
			sceIoClose(fd);

			if (signature == PBP_SIGNATURE)
				return res;
		}		
	}

	return 0;
}

int dclose_patch(int dfd)
{
	if (dfd == theuid)
		theuid = -1;

	return sceIoDclose(dfd);
}

void corrupticons_patch(SceUID gpmod)
{
	SceKernelModuleInfo info;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);

	if (sceKernelQueryModuleInfo(gpmod, &info) >= 0)
	{
		/* Patch the imports to sceIoDopen/sceIoDread/sceIoDclose */
		
		CHANGE_FUNC(info.text_addr+0x10F68, dopen_patch);
		CHANGE_FUNC(info.text_addr+0x10F70, dread_patch);
		CHANGE_FUNC(info.text_addr+0x10F78, dclose_patch);
		
		clear_cache();
	}

}

CONFIGFILE config;

u8 vshmain_params[0x0400] =
{
   0x00, 0x04, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
   0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
}; 

#define RECOVERY_PROGRAM "flash0:/kd/recovery.elf"

int main_thread(SceSize args, void *argp)
{
	SceCtrlData pad;
	SceIoStat stat;
	SceUID mod;
	SceSize vshmain_args;
	void *vshmain_argp;
	int i;

	sceCtrlReadBufferPositive(&pad, 1);

	if (pad.Buttons & PSP_CTRL_RTRIGGER)
	{
		struct SceKernelLoadExecParam param;
		
		memset(&param, 0, sizeof(param));

		param.size = sizeof(param);
		param.args = strlen(RECOVERY_PROGRAM)+1;
		param.argp = RECOVERY_PROGRAM;
		param.key = "updater";

		sceKernelLoadExec(RECOVERY_PROGRAM, &param);
	}
	
	pspSdkInstallNoDeviceCheckPatch();
	pspSdkInstallNoPlainModuleCheckPatch();

	read_config("ms0:/PSP/SYSTEM/config.txt", &config);

	if (config.autoboot[0] && args != 0x0400)
	{		
		if (sceIoGetstat(config.autoboot, &stat) >= 0)
		{		
			struct SceKernelLoadExecVSHParam vsh_param;

			memset(&vsh_param, 0, sizeof(vsh_param));

			vsh_param.size = sizeof(vsh_param);
			vsh_param.args = strlen(config.autoboot)+1;
			vsh_param.argp = config.autoboot;
			vsh_param.key = "game";
			vsh_param.vshmain_args_size = 0x0400;
			vsh_param.vshmain_args = vshmain_params;

			do_loadexecvsh(config.autoboot, &vsh_param, vshKernelLoadExecVSHMs2);
		}
	}

	for (i = 0; i < 10; i++)
	{
		if (config.loadmodule[i][0])
		{
			mod = sceKernelLoadModule(config.loadmodule[i], 0, NULL);
			if (mod >= 0)
			{
				sceKernelStartModule(mod, strlen(config.loadmodule[i])+1, config.loadmodule[i], NULL, NULL);
			}
		}
	}

	if (config.skiplogo)
	{
		vshmain_args = 0x0400;
		vshmain_argp = vshmain_params;
	}
	else
	{
		vshmain_args = args;
		vshmain_argp = argp;
	}

	mod = sceKernelLoadModule("flash0:/vsh/module/vshmain_real.prx", 0, NULL);

	if (mod > 0)
	{
		sceKernelStartModule(mod, vshmain_args, vshmain_argp, NULL, NULL);
	}

	/* Delete temporal file of no-kxploit */
	sceIoRemove(TEMP_ELF);	
	
	if (config.nokxploit)
	{
		nokxploit_patch(mod);
	}

	if (config.hidecorrupt)
	{

		while ((mod = sceKernelSearchModuleByName("game_plugin_module")) < 0)
		{
			sceKernelDelayThread(50000);
		}	
	
		corrupticons_patch(mod);
	}
	
	sceKernelExitDeleteThread(0);
	
	return 0;
}

int module_start(SceSize args, void *argp) __attribute__((alias("_start")));

int _start(SceSize args, void *argp)
{
	SceUID th;

	u32 func = 0x80000000 | (u32)main_thread;

	th = sceKernelCreateThread("main_thread", (void *)func, 0x20, 0x10000, 0, NULL);

	if (th >= 0)
	{
		sceKernelStartThread(th, args, argp);
	}

	return 0;
}
