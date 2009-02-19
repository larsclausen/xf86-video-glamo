/*
 * Copyright  2007 OpenMoko, Inc.
 * Copyright © 2009 Lars-Peter Clausen <lars@metafoo.de>
 *
 * This driver is based on Xati,
 * Copyright  2003 Eric Anholt
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * Edited by:
 *   Dodji SEKETELI <dodji@openedhand.com>
 */

#ifndef _GLAMO_H_
#define _GLAMO_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "exa.h"
#include <linux/fb.h>

#define GLAMO_REG_BASE(c)		((c)->attr.address[0])
#define GLAMO_REG_SIZE(c)		(0x2400)

#if defined(__arm__) /* && !defined(__ARM_EABI__) */

static __inline__ void
MMIO_OUT16(__volatile__ void *base, const unsigned long offset,
       const unsigned short val)
{
    __asm__ __volatile__(
            "strh %0, [%1, +%2]"
            :
            : "r" (val), "r" (base), "r" (offset)
            : "memory" );
}

static __inline__ CARD16
MMIO_IN16(__volatile__ void *base, const unsigned long offset)
{
    unsigned short val;
    __asm__ __volatile__(
            "ldrh %0, [%1, +%2]"
            : "=r" (val)
            : "r" (base), "r" (offset)
            : "memory");
    return val;
}

#else

#define MMIO_OUT16(mmio, a, v) (*(VOL16 *)((mmio) + (a)) = (v))
#define MMIO_IN16(mmio, a)     (*(VOL16 *)((mmio) + (a)))

#endif

typedef volatile CARD16        VOL16;

typedef struct _MemBuf {
	int size;
	int used;
	void *address;
} MemBuf;

typedef struct {
	Bool					shadowFB;
	void					*shadow;
	CloseScreenProcPtr		CloseScreen;
	CreateScreenResourcesProcPtr CreateScreenResources;
	void					(*PointerMoved)(int index, int x, int y);
	EntityInfoPtr			pEnt;
	OptionInfoPtr			Options;

	ScreenPtr 				pScreen;

	ExaDriverPtr exa;
	ExaOffscreenArea *exa_cmd_queue;

	CARD16 *ring_addr; /* Beginning of ring buffer. */
	int ring_len;

	/*
	 * cmd queue cache in system memory
	 * It is to be flushed to cmd_queue_space
	 * "at once", when we are happy with it.
	 */
	MemBuf *cmd_queue_cache;

	/* What was GLAMOCardInfo */
	volatile char *reg_base;
	Bool is_3362;

    /* linux framebuffer */
    int fb_fd;
    struct fb_var_screeninfo fb_var;
    struct fb_fix_screeninfo fb_fix;
    unsigned char *fbstart;
	unsigned char *fbmem;
	int fboff;
} GlamoRec, *GlamoPtr;

#define GlamoPTR(p) ((GlamoPtr)((p)->driverPrivate))

static inline void
MMIOSetBitMask(volatile char *mmio, CARD32 reg, CARD16 mask, CARD16 val)
{
	CARD16 tmp;

	val &= mask;

	tmp = MMIO_IN16(mmio, reg);
	tmp &= ~mask;
	tmp |= val;

	MMIO_OUT16(mmio, reg, tmp);
}

/* glamo_draw.c */
Bool
GLAMODrawInit(ScreenPtr pScreen);

void
GLAMODrawSetup(GlamoPtr pGlamo);

void
GLAMODrawEnable(GlamoPtr pScreen);

void
GLAMODrawDisable(ScreenPtr pScreen);

void
GLAMODrawFini(ScreenPtr pScreen);

Bool
GLAMODrawExaInit(ScreenPtr pScreen, ScrnInfoPtr pScrn);

/* glamo-display.h */
Bool
GlamoCrtcInit(ScrnInfoPtr pScrn);

/* glamo-output.h */
void
GlamoOutputInit(ScrnInfoPtr pScrn);

#endif /* _GLAMO_H_ */
