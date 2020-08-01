/*
 * Copyright (c) 2020 Graphene
 */

#include <psp2/kernel/clib.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/iofilemgr.h>
#include <taihen.h>

static SceUID g_hooks[3];
static tai_hook_ref_t g_ref_hooks[3];

void _start() __attribute__((weak, alias("module_start")));

static unsigned int image_count = 0;
static SceByte savespace = 0, loaded = 0;
static SceUID currentUID = 0;
static char currentBook[256];

SceUID sceIoOpen_patched(const char *filename, int flag, SceIoMode mode)
{
	SceUID ret = TAI_CONTINUE(SceUID, g_ref_hooks[1], filename, flag, mode);

	char* ext = sceClibStrrchr(filename, '.');
	if (!sceClibStrncasecmp(ext, ".MNH", 4)) {

		if (!loaded) {
			loaded = 1;
			SceUID fdi = sceIoOpen("ux0:book/HonRipper.ini", SCE_O_RDONLY, 0777);
			if (fdi > 0) {
				sceIoRead(fdi, &savespace, 1);
				savespace = savespace - '0';
				sceClibPrintf("Read: 0x%x\n", savespace);
				sceIoClose(fdi);
			}
		}

		char* book = sceClibStrrchr(filename, '/');
		book = book + 1;
		int name_len = ext - book;
		sceClibMemset(&currentBook, 0, 256);
		sceClibStrncpy(currentBook, book, name_len);
		char path[100];
		sceClibMemset(&path, 0, 100);
		switch (savespace) {
		case 0:
			sceClibSnprintf(path, 100, "ux0:HonRipper/%s", currentBook);
			break;
		case 1:
			sceClibSnprintf(path, 100, "uma0:HonRipper/%s", currentBook);
			break;
		}
		sceIoMkdir(path, 0777);
		sceClibPrintf("HonRipper: opening book: %s\n", currentBook);
		currentUID = ret;
	}

	return ret;
}

int close_patched(int a1)
{
	if (currentUID > 0) {
		sceClibPrintf("HonRipper: closing book\n");
		currentUID = 0;
		image_count = 0;
	}

	return TAI_CONTINUE(int, g_ref_hooks[2], a1);
}

int sceJpegDecodeMJpegYCbCr_patched(
	const unsigned char *pJpeg,
	SceSize isize,
	unsigned char *pYCbCr,
	SceSize osize,
	int decodeMode,
	void *pCoefBuffer,
	SceSize coefBufferSize)
{
	if (currentUID != 0) {
		image_count++;
		sceClibPrintf("HonRipper: decoding in progress, image num: %d\n", image_count);

		char path[100];
		sceClibMemset(&path, 0, 100);
		switch (savespace) {
		case 0:
			sceClibSnprintf(path, 100, "ux0:HonRipper/%s/%d.jpg", currentBook, image_count);
			break;
		case 1:
			sceClibSnprintf(path, 100, "uma0:HonRipper/%s/%d.jpg", currentBook, image_count);
			break;
		}
		SceUID fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT, 0777);
		sceIoWrite(fd, pJpeg, isize);
		sceIoClose(fd);
	}

	return TAI_CONTINUE(
		int,
		g_ref_hooks[0],
		pJpeg,
		isize,
		pYCbCr,
		osize,
		decodeMode,
		pCoefBuffer,
		coefBufferSize);
}

int module_start(SceSize argc, const void *args) 
{
	sceClibPrintf("HonRipper: module start\n");

	tai_module_info_t info;
	info.size = sizeof(info);
	taiGetModuleInfo("ReaderForPSVita_release", &info);

	g_hooks[0] = taiHookFunctionImport(
		&g_ref_hooks[0],
		TAI_MAIN_MODULE,
		0x880BF710,
		0x2A769BD8,
		sceJpegDecodeMJpegYCbCr_patched);

	g_hooks[1] = taiHookFunctionImport(
		&g_ref_hooks[1],
		TAI_MAIN_MODULE,
		0xCAE9ACE6,
		0x6C60AC61,
		sceIoOpen_patched);

	g_hooks[2] = taiHookFunctionOffset(
		&g_ref_hooks[2],
		info.modid,
		0,
		0x10b4e8,
		1,
		close_patched);

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) 
{
	if (g_hooks[0] >= 0) taiHookRelease(g_hooks[0], g_ref_hooks[0]);
	if (g_hooks[1] >= 0) taiHookRelease(g_hooks[1], g_ref_hooks[1]);
	if (g_hooks[2] >= 0) taiHookRelease(g_hooks[2], g_ref_hooks[2]);
	return SCE_KERNEL_STOP_SUCCESS;
}