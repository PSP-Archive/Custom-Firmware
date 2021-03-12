#ifndef __CONF_H__
#define __CONF_H__

typedef struct
{
	/* Load normal pbp's (no-kxploited) */
	int nokxploit;	
	/* Hide corrupt icons (it can cause slowdown) */
	int hidecorrupt;
	/* Skip $CE logo */
	int skiplogo;
	/* File to autoexecute at startup (Note: recovery has prioirity over this) */
	char autoboot[64];
	/* Modules to load at startup */
	char loadmodule[9][64];

} CONFIGFILE;

void read_config(const char *file, CONFIGFILE *config);

#endif 
