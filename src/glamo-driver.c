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

#include "glamo.h"

static Bool debug = 0;

#define TRACE_ENTER(str) \
    do { if (debug) ErrorF("Glamo: " str " %d\n",pScrn->scrnIndex); } while (0)
#define TRACE_EXIT(str) \
    do { if (debug) ErrorF("Glamo: " str " done\n"); } while (0)
#define TRACE(str) \
    do { if (debug) ErrorF("Glamo trace: " str "\n"); } while (0)

/* -------------------------------------------------------------------- */
/* prototypes                                                           */

static const OptionInfoRec * GlamoAvailableOptions(int chipid, int busid);
static void	GlamoIdentify(int flags);
static Bool	GlamoProbe(DriverPtr drv, int flags);
static Bool	GlamoPreInit(ScrnInfoPtr pScrn, int flags);
static Bool	GlamoScreenInit(int Index, ScreenPtr pScreen, int argc,
				char **argv);
static Bool	GlamoCloseScreen(int scrnIndex, ScreenPtr pScreen);
/*static void *	GlamoWindowLinear(ScreenPtr pScreen, CARD32 row, CARD32 offset, int mode,
				  CARD32 *size, void *closure);*/
static void	GlamoPointerMoved(int index, int x, int y);
static Bool	GlamoDriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op,
				pointer ptr);


enum { Glamo_ROTATE_NONE=0, Glamo_ROTATE_CW=270, Glamo_ROTATE_UD=180, Glamo_ROTATE_CCW=90 };


/* -------------------------------------------------------------------- */

/*
 * This is intentionally screen-independent.  It indicates the binding
 * choice made in the first PreInit.
 */
static int pix24bpp = 0;

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
	GlamoDriverFunc

};

/* Supported "chipsets" */
static SymTabRec GlamoChipsets[] = {
    { 0, "Glamo" },
    {-1, NULL }
};

/* Supported options */
typedef enum {
	OPTION_SHADOW_FB,
	OPTION_ROTATE,
	OPTION_DEBUG
} GlamoOpts;

static const OptionInfoRec GlamoOptions[] = {
	{ OPTION_SHADOW_FB,	"ShadowFB",	OPTV_BOOLEAN,	{0},	FALSE },
	{ OPTION_ROTATE,	"Rotate",	OPTV_STRING,	{0},	FALSE },
	{ OPTION_DEBUG,		"debug",	OPTV_BOOLEAN,	{0},	FALSE },
	{ -1,			NULL,		OPTV_NONE,	{0},	FALSE }
};

/* -------------------------------------------------------------------- */

/*static const char *afbSymbols[] = {
	"afbScreenInit",
	"afbCreateDefColormap",
	NULL
};*/

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
	"fbdevHWSetVideoModes",
	"fbdevHWUseBuildinMode",

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
	"fbdevHWModeInit",
	"fbdevHWRestore",
	"fbdevHWSave",
	"fbdevHWSaveScreen",
	"fbdevHWSaveScreenWeak",
	"fbdevHWSwitchModeWeak",
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
		xf86AddDriver(&Glamo, module, HaveDriverFuncs);
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
		dev = xf86FindOptionValue(devSections[i]->options,"Glamo");
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
				pScrn->SwitchMode    = fbdevHWSwitchModeWeak();
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
	GlamoPtr fPtr;
	int default_depth, fbbpp;
	const char /**mod = NULL,*/ *s;
	/*const char **syms = NULL;*/

	if (flags & PROBE_DETECT) return FALSE;

	TRACE_ENTER("PreInit");

	/* Check the number of entities, and fail if it isn't one. */
	if (pScrn->numEntities != 1)
		return FALSE;

	pScrn->monitor = pScrn->confScreen->monitor;

	GlamoGetRec(pScrn);
	fPtr = GlamoPTR(pScrn);

	fPtr->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

	pScrn->racMemFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;
	/* XXX Is this right?  Can probably remove RAC_FB */
	pScrn->racIoFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;

	/* open device */
	if (!fbdevHWInit(pScrn,NULL,xf86FindOptionValue(fPtr->pEnt->device->options,"Glamo")))
		return FALSE;
	default_depth = fbdevHWGetDepth(pScrn,&fbbpp);
	if (!xf86SetDepthBpp(pScrn, default_depth, default_depth, fbbpp,
				 Support24bppFb | Support32bppFb | SupportConvert32to24 | SupportConvert24to32))
		return FALSE;
	xf86PrintDepthBpp(pScrn);

	/* Get the depth24 pixmap format */
	if (pScrn->depth == 24 && pix24bpp == 0)
		pix24bpp = xf86GetBppFromDepth(pScrn, 24);

	/* color weight */
	if (pScrn->depth > 8) {
		rgb zeros = { 0, 0, 0 };
		if (!xf86SetWeight(pScrn, zeros, zeros))
			return FALSE;
	}

	/* visual init */
	if (!xf86SetDefaultVisual(pScrn, -1))
		return FALSE;

	/* We don't currently support DirectColor at > 8bpp */
	if (pScrn->depth > 8 && pScrn->defaultVisual != TrueColor) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "requested default visual"
			   " (%s) is not supported at depth %d\n",
			   xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
		return FALSE;
	}

	{
		Gamma zeros = {0.0, 0.0, 0.0};

		if (!xf86SetGamma(pScrn,zeros)) {
			return FALSE;
		}
	}

	pScrn->progClock = TRUE;
	pScrn->rgbBits   = 8;
	pScrn->chipset   = "Glamo";
	pScrn->videoRam  = fbdevHWGetVidmem(pScrn);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "hardware: %s (video memory:"
		   " %dkB)\n", fbdevHWGetName(pScrn), pScrn->videoRam/1024);

	/* handle options */
	xf86CollectOptions(pScrn, NULL);
	if (!(fPtr->Options = xalloc(sizeof(GlamoOptions))))
		return FALSE;
	memcpy(fPtr->Options, GlamoOptions, sizeof(GlamoOptions));
	xf86ProcessOptions(pScrn->scrnIndex, fPtr->pEnt->device->options, fPtr->Options);

	/* use shadow framebuffer by default */
	fPtr->shadowFB = xf86ReturnOptValBool(fPtr->Options, OPTION_SHADOW_FB, TRUE);

	debug = xf86ReturnOptValBool(fPtr->Options, OPTION_DEBUG, FALSE);

	/* rotation */
	fPtr->rotate = Glamo_ROTATE_NONE;
	if ((s = xf86GetOptValString(fPtr->Options, OPTION_ROTATE)))
	{
	  if(!xf86NameCmp(s, "CW"))
	  {
		fPtr->shadowFB = TRUE;
		fPtr->rotate = Glamo_ROTATE_CW;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			   "rotating screen clockwise\n");
	  }
	  else if(!xf86NameCmp(s, "CCW"))
	  {
		fPtr->shadowFB = TRUE;
		fPtr->rotate = Glamo_ROTATE_CCW;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			   "rotating screen counter-clockwise\n");
	  }
	  else if(!xf86NameCmp(s, "UD"))
	  {
		fPtr->shadowFB = TRUE;
		fPtr->rotate = Glamo_ROTATE_UD;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			   "rotating screen upside-down\n");
	  }
	  else
	  {
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			   "\"%s\" is not a valid value for Option \"Rotate\"\n", s);
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "valid options are \"CW\", \"CCW\" and \"UD\"\n");
	  }
	}

	/* select video modes */

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "checking modes against framebuffer device...\n");
	fbdevHWSetVideoModes(pScrn);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "checking modes against monitor...\n");
	{
		DisplayModePtr mode, first = mode = pScrn->modes;

		if (mode != NULL) do {
			mode->status = xf86CheckModeForMonitor(mode, pScrn->monitor);
			mode = mode->next;
		} while (mode != NULL && mode != first);

		xf86PruneDriverModes(pScrn);
	}

	if (NULL == pScrn->modes)
		fbdevHWUseBuildinMode(pScrn);
	pScrn->currentMode = pScrn->modes;

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
	GlamoPtr fPtr = GlamoPTR(pScrn);
	VisualPtr visual;
	int init_picture = 0;
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

	if (NULL == (fPtr->fbmem = fbdevHWMapVidmem(pScrn))) {
			xf86DrvMsg(scrnIndex,X_ERROR,"mapping of video memory"
			   " failed\n");
		return FALSE;
	}
	fPtr->fboff = fbdevHWLinearOffset(pScrn);

	fbdevHWSave(pScrn);

	if (!fbdevHWModeInit(pScrn, pScrn->currentMode)) {
		xf86DrvMsg(scrnIndex,X_ERROR,"mode initialization failed\n");
		return FALSE;
	}
	fbdevHWSaveScreen(pScreen, SCREEN_SAVER_ON);
	fbdevHWAdjustFrame(scrnIndex,0,0,0);

	/* mi layer */
	miClearVisualTypes();
	if (pScrn->bitsPerPixel > 8) {
		if (!miSetVisualTypes(pScrn->depth, TrueColorMask, pScrn->rgbBits, TrueColor)) {
			xf86DrvMsg(scrnIndex,X_ERROR,"visual type setup failed"
				   " for %d bits per pixel [1]\n",
				   pScrn->bitsPerPixel);
			return FALSE;
		}
	} else {
		if (!miSetVisualTypes(pScrn->depth,
					  miGetDefaultVisualMask(pScrn->depth),
					  pScrn->rgbBits, pScrn->defaultVisual)) {
			xf86DrvMsg(scrnIndex,X_ERROR,"visual type setup failed"
				   " for %d bits per pixel [2]\n",
				   pScrn->bitsPerPixel);
			return FALSE;
		}
	}
	if (!miSetPixmapDepths()) {
	  xf86DrvMsg(scrnIndex,X_ERROR,"pixmap depth setup failed\n");
	  return FALSE;
	}

	if(fPtr->rotate==Glamo_ROTATE_CW || fPtr->rotate==Glamo_ROTATE_CCW)
	{
	  int tmp = pScrn->virtualX;
	  pScrn->virtualX = pScrn->displayWidth = pScrn->virtualY;
	  pScrn->virtualY = tmp;
	} else {
		/* FIXME: this doesn't work for all cases, e.g. when each scanline
			has a padding which is independent from the depth (controlfb) */
		pScrn->displayWidth = fbdevHWGetLineLength(pScrn) /
					  (pScrn->bitsPerPixel / 8);

		if (pScrn->displayWidth != pScrn->virtualX) {
			xf86DrvMsg(scrnIndex, X_INFO,
				   "Pitch updated to %d after ModeInit\n",
				   pScrn->displayWidth);
		}
	}

	if(fPtr->rotate && !fPtr->PointerMoved) {
		fPtr->PointerMoved = pScrn->PointerMoved;
		pScrn->PointerMoved = GlamoPointerMoved;
	}

	fPtr->fbstart = fPtr->fbmem + fPtr->fboff;

    ret = fbScreenInit(pScreen, fPtr->fbstart, pScrn->virtualX,
					   pScrn->virtualY, pScrn->xDpi,
					   pScrn->yDpi, pScrn->displayWidth,
					   pScrn->bitsPerPixel);
	init_picture = 1;

	if (!ret)
		return FALSE;

	if (pScrn->bitsPerPixel > 8) {
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
	}

	/* must be after RGB ordering fixed */
	if (init_picture && !fbPictureInit(pScreen, NULL, 0))
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "Render extension initialisation failed\n");

	if (fPtr->rotate) {
	  xf86DrvMsg(scrnIndex, X_INFO, "using driver rotation; disabling "
							"XRandR\n");
	  xf86DisableRandR();
	  if (pScrn->bitsPerPixel == 24)
		xf86DrvMsg(scrnIndex, X_WARNING, "rotation might be broken at 24 "
                                             "bits per pixel\n");
	}

	/* map in the registers */
	fPtr->reg_base = xf86MapVidMem(pScreen->myNum, VIDMEM_MMIO, 0x8000000, 0x2400);

	fPtr->pScreen = pScreen;

	/*fPtr->cmd_queue_cache = GLAMOCreateCMDQCache(fPtr);*/

	/*GLAMOCMDQCacheSetup(fPtr);*/

	xf86LoadSubModule(pScrn, "exa");
	xf86LoaderReqSymLists(exaSymbols, NULL);

	if(!GLAMODrawExaInit(pScreen, pScrn)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"EXA hardware acceleration initialization failed\n");
		return FALSE;
	}

	GLAMODrawEnable(fPtr);

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
	if(!xf86HandleColormaps(pScreen, 256, 8, fbdevHWLoadPaletteWeak(),
				NULL, flags))
		return FALSE;

	xf86DPMSInit(pScreen, fbdevHWDPMSSetWeak(), 0);

	pScreen->SaveScreen = fbdevHWSaveScreenWeak();

	/* Wrap the current CloseScreen function */
	fPtr->CloseScreen = pScreen->CloseScreen;
	pScreen->CloseScreen = GlamoCloseScreen;

	{
		XF86VideoAdaptorPtr *ptr;

		int n = xf86XVListGenericAdaptors(pScrn,&ptr);
		if (n) {
		xf86XVScreenInit(pScreen,ptr,n);
		}
	}

	TRACE_EXIT("GlamoScreenInit");

	return TRUE;
}

static Bool
GlamoCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	GlamoPtr fPtr = GlamoPTR(pScrn);

	fbdevHWRestore(pScrn);
	fbdevHWUnmapVidmem(pScrn);
	if (fPtr->shadow) {
		xfree(fPtr->shadow);
		fPtr->shadow = NULL;
	}
	pScrn->vtSema = FALSE;

	pScreen->CreateScreenResources = fPtr->CreateScreenResources;
	pScreen->CloseScreen = fPtr->CloseScreen;
	return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}



/***********************************************************************
 * Shadow stuff
 ***********************************************************************/
#if 0
static void *
GlamoWindowLinear(ScreenPtr pScreen, CARD32 row, CARD32 offset, int mode,
		 CARD32 *size, void *closure)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    GlamoPtr fPtr = GlamoPTR(pScrn);

    if (!pScrn->vtSema)
      return NULL;

    if (fPtr->lineLength)
      *size = fPtr->lineLength;
    else
      *size = fPtr->lineLength = fbdevHWGetLineLength(pScrn);

    return ((CARD8 *)fPtr->fbstart + row * fPtr->lineLength + offset);
}
#endif
static void
GlamoPointerMoved(int index, int x, int y)
{
    ScrnInfoPtr pScrn = xf86Screens[index];
    GlamoPtr fPtr = GlamoPTR(pScrn);
    int newX, newY;

    switch (fPtr->rotate)
    {
    case Glamo_ROTATE_CW:
	/* 90 degrees CW rotation. */
	newX = pScrn->pScreen->height - y - 1;
	newY = x;
	break;

    case Glamo_ROTATE_CCW:
	/* 90 degrees CCW rotation. */
	newX = y;
	newY = pScrn->pScreen->width - x - 1;
	break;

    case Glamo_ROTATE_UD:
	/* 180 degrees UD rotation. */
	newX = pScrn->pScreen->width - x - 1;
	newY = pScrn->pScreen->height - y - 1;
	break;

    default:
	/* No rotation. */
	newX = x;
	newY = y;
	break;
    }

    /* Pass adjusted pointer coordinates to wrapped PointerMoved function. */
    (*fPtr->PointerMoved)(index, newX, newY);
}

static Bool
GlamoRandRGetInfo(ScrnInfoPtr pScrn, Rotation *rotations)
{
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "GlamoRandRGetInfo got here!\n");

    *rotations = RR_Rotate_0 | RR_Rotate_90 | RR_Rotate_270;

    return TRUE;
}

static Bool
GlamoRandRSetConfig(ScrnInfoPtr pScrn, xorgRRConfig *config)
{
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "GlamoRandRSetConfig got here!\n");

    switch(config->rotation) {
        case RR_Rotate_0:
            break;

        case RR_Rotate_90:
            break;

        case RR_Rotate_270:
            break;

        default:
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                    "Unexpected rotation in GlamoRandRSetConfig!\n");
            return FALSE;
    }

    return TRUE;
}

static Bool
GlamoDriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op, pointer ptr)
{
    xorgHWFlags *flag;

    switch (op) {
	case GET_REQUIRED_HW_INTERFACES:
		flag = (CARD32*)ptr;
		(*flag) = 0;
		return TRUE;
	case RR_GET_INFO:
		return GlamoRandRGetInfo(pScrn, (Rotation*)ptr);
	case RR_SET_CONFIG:
        return GlamoRandRSetConfig(pScrn, (xorgRRConfig*)ptr);
	default:
		return FALSE;
    }
}
