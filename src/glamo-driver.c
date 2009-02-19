/*
 * Authors:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *		 Michel Dänzer, <michel@tungstengraphics.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

/* all driver need this */
#include "xf86.h"
#include "xf86_OSproc.h"

#include "mipointer.h"
#include "mibstore.h"
#include "micmap.h"
#include "colormapst.h"
#include "xf86cmap.h"
#include "shadow.h"

/* for visuals */
#include "fb.h"

#include "xf86Resources.h"
#include "xf86RAC.h"

#include "fbdevhw.h"

#include "xf86xv.h"

#include "xf86i2c.h"
#include "xf86Modes.h"
#include "xf86Crtc.h"
#include "xf86RandR12.h"

#include "glamo.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

static Bool debug = 0;

#define TRACE_ENTER(str) \
    do { if (debug) ErrorF("Glamo: " str " %d\n",pScrn->scrnIndex); } while (0)
#define TRACE_EXIT(str) \
    do { if (debug) ErrorF("Glamo: " str " done\n"); } while (0)
#define TRACE(str) \
    do { if (debug) ErrorF("Glamo trace: " str "\n"); } while (0)

/* -------------------------------------------------------------------- */
/* prototypes                                                           */
static const OptionInfoRec *
GlamoAvailableOptions(int chipid, int busid);

static void
GlamoIdentify(int flags);

static Bool
GlamoProbe(DriverPtr drv, int flags);

static Bool
GlamoPreInit(ScrnInfoPtr pScrn, int flags);

static Bool
GlamoScreenInit(int Index, ScreenPtr pScreen, int argc, char **argv);

static Bool
GlamoCloseScreen(int scrnIndex, ScreenPtr pScreen);

static Bool
GlamoCrtcResize(ScrnInfoPtr scrn, int width, int height);

static Bool
GlamoInitFramebufferDevice(GlamoPtr pGlamo, const char *fb_device);
/* -------------------------------------------------------------------- */

static const xf86CrtcConfigFuncsRec glamo_crtc_config_funcs = {
    .resize = GlamoCrtcResize
};

#define GLAMO_VERSION		1000
#define GLAMO_NAME		"Glamo"
#define GLAMO_DRIVER_NAME	"Glamo"

_X_EXPORT DriverRec Glamo = {
	GLAMO_VERSION,
	GLAMO_DRIVER_NAME,
#if 0
	"driver for glamo devices",
#endif
	GlamoIdentify,
	GlamoProbe,
	GlamoAvailableOptions,
	NULL,
	0,
	NULL
};

/* Supported "chipsets" */
static SymTabRec GlamoChipsets[] = {
    { 0, "Glamo" },
    {-1, NULL }
};

/* Supported options */
typedef enum {
	OPTION_SHADOW_FB,
	OPTION_DEBUG
} GlamoOpts;

static const OptionInfoRec GlamoOptions[] = {
	{ OPTION_SHADOW_FB,	"ShadowFB",	OPTV_BOOLEAN,	{0},	FALSE },
	{ OPTION_DEBUG,		"debug",	OPTV_BOOLEAN,	{0},	FALSE },
	{ -1,			NULL,		OPTV_NONE,	{0},	FALSE }
};

/* -------------------------------------------------------------------- */

static const char *fbSymbols[] = {
	"fbScreenInit",
	"fbPictureInit",
	NULL
};

static const char *shadowSymbols[] = {
	"shadowAdd",
	"shadowInit",
	"shadowSetup",
	"shadowUpdatePacked",
	"shadowUpdatePackedWeak",
	"shadowUpdateRotatePacked",
	"shadowUpdateRotatePackedWeak",
	NULL
};

static const char *fbdevHWSymbols[] = {
	"fbdevHWInit",
	"fbdevHWProbe",

	"fbdevHWGetDepth",
	"fbdevHWGetLineLength",
	"fbdevHWGetName",
	"fbdevHWGetType",
	"fbdevHWGetVidmem",
	"fbdevHWLinearOffset",
	"fbdevHWLoadPalette",
	"fbdevHWMapVidmem",
	"fbdevHWUnmapVidmem",

	/* colormap */
	"fbdevHWLoadPalette",
	"fbdevHWLoadPaletteWeak",

	/* ScrnInfo hooks */
	"fbdevHWAdjustFrameWeak",
	"fbdevHWEnterVTWeak",
	"fbdevHWLeaveVTWeak",
	"fbdevHWRestore",
	"fbdevHWSave",
	"fbdevHWSaveScreen",
	"fbdevHWSaveScreenWeak",
	"fbdevHWValidModeWeak",

	"fbdevHWDPMSSet",
	"fbdevHWDPMSSetWeak",

	NULL
};

static const char *exaSymbols[] = {
    "exaDriverAlloc",
    "exaDriverInit",
    "exaDriverFini",
    NULL
};


#ifdef XFree86LOADER

MODULESETUPPROTO(GlamoSetup);

static XF86ModuleVersionInfo GlamoVersRec =
{
	"Glamo",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XORG_VERSION_CURRENT,
	PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR, PACKAGE_VERSION_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,
	ABI_VIDEODRV_VERSION,
	NULL,
	{0,0,0,0}
};

_X_EXPORT XF86ModuleData glamoModuleData = { &GlamoVersRec, GlamoSetup, NULL };

pointer
GlamoSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
	static Bool setupDone = FALSE;

	if (!setupDone) {
		setupDone = TRUE;
		xf86AddDriver(&Glamo, module, 0);
		LoaderRefSymLists(fbSymbols,
				  shadowSymbols, fbdevHWSymbols, exaSymbols, NULL);
		return (pointer)1;
	} else {
		if (errmaj) *errmaj = LDR_ONCEONLY;
		return NULL;
	}
}

#endif /* XFree86LOADER */

static Bool
GlamoGetRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate != NULL)
		return TRUE;

	pScrn->driverPrivate = xnfcalloc(sizeof(GlamoRec), 1);
	return TRUE;
}

static void
GlamoFreeRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate == NULL)
		return;
	xfree(pScrn->driverPrivate);
	pScrn->driverPrivate = NULL;
}

/* -------------------------------------------------------------------- */
static Bool
GlamoSwitchMode(int scrnIndex, DisplayModePtr mode, int flags) {
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR (pScrn);
    xf86OutputPtr output = config->output[config->compat_output];
    Rotation rotation;

    if (output && output->crtc)
        rotation = output->crtc->rotation;
    else
        rotation = RR_Rotate_0;

    return xf86SetSingleMode(pScrn, mode, rotation);
}

static const OptionInfoRec *
GlamoAvailableOptions(int chipid, int busid)
{
	return GlamoOptions;
}

static void
GlamoIdentify(int flags)
{
	xf86PrintChipsets(GLAMO_NAME, "driver for glamo", GlamoChipsets);
}

static Bool
GlamoProbe(DriverPtr drv, int flags)
{
	int i;
	ScrnInfoPtr pScrn;
	GDevPtr *devSections;
	int numDevSections;
	char *dev;
	Bool foundScreen = FALSE;

	TRACE("probe start");

	/* For now, just bail out for PROBE_DETECT. */
	if (flags & PROBE_DETECT)
		return FALSE;

	if ((numDevSections = xf86MatchDevice(GLAMO_DRIVER_NAME, &devSections)) <= 0)
		return FALSE;

	if (!xf86LoadDrvSubModule(drv, "fbdevhw"))
		return FALSE;

	xf86LoaderReqSymLists(fbdevHWSymbols, NULL);

	for (i = 0; i < numDevSections; i++) {
		dev = xf86FindOptionValue(devSections[i]->options, "Glamo");
		if (fbdevHWProbe(NULL, dev, NULL)) {
			int entity;
			pScrn = NULL;

			entity = xf86ClaimFbSlot(drv, 0, devSections[i], TRUE);
			pScrn = xf86ConfigFbEntity(pScrn,0,entity,
			                           NULL, NULL, NULL, NULL);

			if (pScrn) {
				foundScreen = TRUE;

				pScrn->driverVersion = GLAMO_VERSION;
				pScrn->driverName    = GLAMO_DRIVER_NAME;
				pScrn->name          = GLAMO_NAME;
				pScrn->Probe         = GlamoProbe;
				pScrn->PreInit       = GlamoPreInit;
				pScrn->ScreenInit    = GlamoScreenInit;
				pScrn->SwitchMode    = GlamoSwitchMode;
				pScrn->AdjustFrame   = fbdevHWAdjustFrameWeak();
				pScrn->EnterVT       = fbdevHWEnterVTWeak();
				pScrn->LeaveVT       = fbdevHWLeaveVTWeak();
				pScrn->ValidMode     = fbdevHWValidModeWeak();

				xf86DrvMsg(pScrn->scrnIndex, X_INFO,
					   "using %s\n", dev ? dev : "default device");
			}
		}
	}
	xfree(devSections);
	TRACE("probe done");
	return foundScreen;
}

static Bool
GlamoPreInit(ScrnInfoPtr pScrn, int flags)
{
    GlamoPtr pGlamo;
    int default_depth, fbbpp;
    rgb weight_defaults = {0, 0, 0};
    Gamma gamma_defaults = {0.0, 0.0, 0.0};
    char *fb_device;

    if (flags & PROBE_DETECT)
        return FALSE;

    TRACE_ENTER("PreInit");

    /* Check the number of entities, and fail if it isn't one. */
    if (pScrn->numEntities != 1)
        return FALSE;

    pScrn->monitor = pScrn->confScreen->monitor;

    GlamoGetRec(pScrn);
    pGlamo = GlamoPTR(pScrn);

    pGlamo->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

    pScrn->racMemFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;
    /* XXX Is this right?  Can probably remove RAC_FB */
    pScrn->racIoFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;

    fb_device = xf86FindOptionValue(pGlamo->pEnt->device->options, "Glamo");

    /* open device */
    if (!fbdevHWInit(pScrn, NULL, fb_device))
            return FALSE;

	/* FIXME: Replace all fbdev functionality with our own code, so we only have
	 * to open the fb devic only once. */
    if (!GlamoInitFramebufferDevice(pGlamo, fb_device))
        return FALSE;

    default_depth = fbdevHWGetDepth(pScrn, &fbbpp);

    if (!xf86SetDepthBpp(pScrn, default_depth, default_depth, fbbpp, 0))
        return FALSE;

    xf86PrintDepthBpp(pScrn);

	/* color weight */
    if (!xf86SetWeight(pScrn, weight_defaults, weight_defaults))
        return FALSE;

    /* visual init */
    if (!xf86SetDefaultVisual(pScrn, -1))
        return FALSE;

    /* We don't currently support DirectColor at > 8bpp */
    if (pScrn->defaultVisual != TrueColor) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "requested default visual"
			   " (%s) is not supported at depth %d\n",
			   xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
        return FALSE;
    }

    if (!xf86SetGamma(pScrn, gamma_defaults)) {
        return FALSE;
    }

    xf86CrtcConfigInit(pScrn, &glamo_crtc_config_funcs);
    xf86CrtcSetSizeRange(pScrn, 240, 320, 480, 640);
    GlamoCrtcInit(pScrn);
    GlamoOutputInit(pScrn);

    if (!xf86InitialConfiguration(pScrn, TRUE)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes.\n");
        return FALSE;
    }

    pScrn->progClock = TRUE;
    pScrn->chipset   = "Glamo";
    pScrn->videoRam  = fbdevHWGetVidmem(pScrn);

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "hardware: %s (video memory:"
		   " %dkB)\n", fbdevHWGetName(pScrn), pScrn->videoRam/1024);

    /* handle options */
    xf86CollectOptions(pScrn, NULL);
    if (!(pGlamo->Options = xalloc(sizeof(GlamoOptions))))
        return FALSE;
    memcpy(pGlamo->Options, GlamoOptions, sizeof(GlamoOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pGlamo->pEnt->device->options, pGlamo->Options);

    /* use shadow framebuffer by default */
    pGlamo->shadowFB = xf86ReturnOptValBool(pGlamo->Options, OPTION_SHADOW_FB, TRUE);

    debug = xf86ReturnOptValBool(pGlamo->Options, OPTION_DEBUG, FALSE);

    /* First approximation, may be refined in ScreenInit */
    pScrn->displayWidth = pScrn->virtualX;

    xf86PrintModes(pScrn);

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);

    if (xf86LoadSubModule(pScrn, "fb") == NULL) {
        GlamoFreeRec(pScrn);
        return FALSE;
    }
    xf86LoaderReqSymLists(fbSymbols, NULL);

    TRACE_EXIT("PreInit");
    return TRUE;
}


static Bool
GlamoScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    GlamoPtr pGlamo = GlamoPTR(pScrn);
    VisualPtr visual;
    int ret, flags;

    TRACE_ENTER("GlamoScreenInit");

#if DEBUG
	ErrorF("\tbitsPerPixel=%d, depth=%d, defaultVisual=%s\n"
		   "\tmask: %x,%x,%x, offset: %d,%d,%d\n",
		   pScrn->bitsPerPixel,
		   pScrn->depth,
		   xf86GetVisualName(pScrn->defaultVisual),
		   pScrn->mask.red,pScrn->mask.green,pScrn->mask.blue,
		   pScrn->offset.red,pScrn->offset.green,pScrn->offset.blue);
#endif

    if (NULL == (pGlamo->fbmem = fbdevHWMapVidmem(pScrn))) {
        xf86DrvMsg(scrnIndex, X_ERROR, "mapping of video memory failed\n");
        return FALSE;
    }
    pGlamo->fboff = fbdevHWLinearOffset(pScrn);

    fbdevHWSave(pScrn);
    fbdevHWSaveScreen(pScreen, SCREEN_SAVER_ON);
    fbdevHWAdjustFrame(scrnIndex,0,0,0);

    /* mi layer */
    miClearVisualTypes();
    if (!miSetVisualTypes(pScrn->depth, TrueColorMask, pScrn->rgbBits, TrueColor)) {
        xf86DrvMsg(scrnIndex, X_ERROR,
                   "visual type setup failed for %d bits per pixel [1]\n",
                   pScrn->bitsPerPixel);
        return FALSE;
    }
    if (!miSetPixmapDepths()) {
      xf86DrvMsg(scrnIndex, X_ERROR, "pixmap depth setup failed\n");
      return FALSE;
    }

    pScrn->displayWidth = fbdevHWGetLineLength(pScrn) /
					  (pScrn->bitsPerPixel / 8);

    pGlamo->fbstart = pGlamo->fbmem + pGlamo->fboff;

    ret = fbScreenInit(pScreen, pGlamo->fbstart, pScrn->virtualX,
                       pScrn->virtualY, pScrn->xDpi, pScrn->yDpi,
                       pScrn->displayWidth,  pScrn->bitsPerPixel);
    if (!ret)
        return FALSE;

    /* Fixup RGB ordering */
    visual = pScreen->visuals + pScreen->numVisuals;
    while (--visual >= pScreen->visuals) {
        if ((visual->class | DynamicClass) == DirectColor) {
            visual->offsetRed   = pScrn->offset.red;
            visual->offsetGreen = pScrn->offset.green;
            visual->offsetBlue  = pScrn->offset.blue;
            visual->redMask     = pScrn->mask.red;
            visual->greenMask   = pScrn->mask.green;
            visual->blueMask    = pScrn->mask.blue;
        }
    }

    /* must be after RGB ordering fixed */
    if (!fbPictureInit(pScreen, NULL, 0))
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "Render extension initialisation failed\n");

        /* map in the registers */
        pGlamo->reg_base = xf86MapVidMem(pScreen->myNum, VIDMEM_MMIO, 0x8000000, 0x2400);

        pGlamo->pScreen = pScreen;

        xf86LoadSubModule(pScrn, "exa");
        xf86LoaderReqSymLists(exaSymbols, NULL);

	if (!GLAMODrawExaInit(pScreen, pScrn)) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "EXA hardware acceleration initialization failed\n");
            return FALSE;
        }

        GLAMODrawEnable(pGlamo);

        xf86SetBlackWhitePixels(pScreen);
        miInitializeBackingStore(pScreen);
        xf86SetBackingStore(pScreen);

        /* software cursor */
        miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

        /* colormap */
        if (!miCreateDefColormap(pScreen)) {
            xf86DrvMsg(scrnIndex, X_ERROR,
                       "internal error: miCreateDefColormap failed "
                       "in GlamoScreenInit()\n");
            return FALSE;
        }

        flags = CMAP_PALETTED_TRUECOLOR;
        if (!xf86HandleColormaps(pScreen, 256, 8, fbdevHWLoadPaletteWeak(),
                                 NULL, flags))
            return FALSE;


    xf86CrtcScreenInit(pScreen);
    xf86RandR12SetRotations(pScreen, RR_Rotate_0 | RR_Rotate_90 |
                                     RR_Rotate_180 | RR_Rotate_270);

    xf86DPMSInit(pScreen, xf86DPMSSet, 0);

    pScreen->SaveScreen = xf86SaveScreen;

    xf86SetDesiredModes(pScrn);
    /* Wrap the current CloseScreen function */
    pGlamo->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = GlamoCloseScreen;

    TRACE_EXIT("GlamoScreenInit");

    return TRUE;
}

static Bool
GlamoCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    GlamoPtr pGlamo = GlamoPTR(pScrn);

    fbdevHWRestore(pScrn);
    fbdevHWUnmapVidmem(pScrn);
    pScrn->vtSema = FALSE;

    pScreen->CreateScreenResources = pGlamo->CreateScreenResources;
    pScreen->CloseScreen = pGlamo->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}

static Bool
GlamoCrtcResize(ScrnInfoPtr pScrn, int width, int height) {
    pScrn->virtualX = width;
    pScrn->virtualY = height;
    pScrn->displayWidth = width * (pScrn->bitsPerPixel / 8);
    pScrn->pScreen->GetScreenPixmap(pScrn->pScreen)->devKind = pScrn->displayWidth;

    return TRUE;
}


static Bool
GlamoInitFramebufferDevice(GlamoPtr pGlamo, const char *fb_device) {
    if (fb_device) {
        pGlamo->fb_fd = open(fb_device, O_RDWR, 0);
        if (pGlamo->fb_fd == -1) {
            ErrorF("Failed to open framebuffer device\n");
            goto fail2;
        }
    } else {
        fb_device = getenv("FRAMEBUFFER");
        if (fb_device != NULL) {
            pGlamo->fb_fd = open(fb_device, O_RDWR, 0);
        if (pGlamo->fb_fd != -1)
            fb_device = NULL;
        }
        if (fb_device == NULL) {
            fb_device = "/dev/fb0";
            pGlamo->fb_fd = open(fb_device, O_RDWR, 0);
            if (pGlamo->fb_fd == -1) {
                ErrorF("Failed to open framebuffer device\n");
                goto fail2;
            }
        }
    }

    /* retrive current setting */
    if (ioctl(pGlamo->fb_fd, FBIOGET_FSCREENINFO, (void*)(&pGlamo->fb_fix)) == -1) {
        ErrorF("FBIOGET_FSCREENINFO\n");
        goto fail1;
    }

    if (ioctl(pGlamo->fb_fd, FBIOGET_VSCREENINFO, (void*)(&pGlamo->fb_var)) == -1) {
        ErrorF("FBIOGET_VSCREENINFO\n");
        goto fail1;
    }
    return TRUE;
fail1:
    close(pGlamo->fb_fd);
    pGlamo->fb_fd = -1;
fail2:
    return FALSE;
}

