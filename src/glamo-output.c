/*
 * Copyright © 2009 Lars-Peter Clausen <lars@metafoo.de>
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "xf86_OSproc.h"

#include "xf86i2c.h"
#include "xf86Crtc.h"

#include "fbdevhw.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "glamo.h"

static const char *display_state_switch_path = "/sys/bus/spi/devices/spi2.0/state";
static const char *display_state_vga = "normal";
static const char *display_state_qvga = "qvga-normal";

static void GlamoOutputDPMS(xf86OutputPtr output, int mode) {}
static xf86OutputStatus GlamoOutputDetect(xf86OutputPtr output);
static Bool GlamoOutputModeFixup(xf86OutputPtr output, DisplayModePtr mode,
                     DisplayModePtr mode_adjusted);
static void GlamoOutputPrepare(xf86OutputPtr output);
static void GlamoOutputModeSet(xf86OutputPtr output, DisplayModePtr mode,
                   DisplayModePtr adjusted_mode);
static int GlamoOutputModeValid(xf86OutputPtr output, DisplayModePtr mode);
static Bool GlamoOutputModeFixup(xf86OutputPtr output, DisplayModePtr mode,
                  DisplayModePtr mode_adjusted);
static void GlamoOutputPrepare(xf86OutputPtr output);
static void GlamoOutputModeSet(xf86OutputPtr output, DisplayModePtr mode,
                  DisplayModePtr adjusted_mode);
static void GlamoOutputCommit(xf86OutputPtr output);
static void GlamoOutputDestroy(xf86OutputPtr output);
static DisplayModePtr GlamoOutputGetModes(xf86OutputPtr output);

static const xf86OutputFuncsRec glamo_output_funcs = {
    .create_resources = NULL,
    .dpms = GlamoOutputDPMS,
    .save = NULL,
    .restore = NULL,
    .mode_valid = GlamoOutputModeValid,
    .mode_fixup = GlamoOutputModeFixup,
    .prepare = GlamoOutputPrepare,
    .commit = GlamoOutputCommit,
    .mode_set = GlamoOutputModeSet,
    .detect = GlamoOutputDetect,
    .get_modes = GlamoOutputGetModes,
#ifdef RANDR_12_INTERFACE
    .set_property = NULL,
#endif
    .destroy = GlamoOutputDestroy
};

void
GlamoOutputInit(ScrnInfoPtr pScrn) {
    xf86OutputPtr output;
    output = xf86OutputCreate(pScrn, &glamo_output_funcs, "LCD");
    output->possible_crtcs = 1;
    output->possible_clones = 0;
}

static xf86OutputStatus
GlamoOutputDetect(xf86OutputPtr output) {
    return XF86OutputStatusConnected;
}

static int
GlamoOutputModeValid(xf86OutputPtr output, DisplayModePtr mode) {
    return MODE_OK;
    /*return fbdevHWValidMode(output->scrn->scrnIndex, mode, FALSE, 0);*/
}

static Bool
GlamoOutputModeFixup(xf86OutputPtr output, DisplayModePtr mode,
                  DisplayModePtr mode_adjusted) {
    return TRUE;
}

static void
GlamoOutputPrepare(xf86OutputPtr output) {
}

static void
GlamoOutputModeSet(xf86OutputPtr output, DisplayModePtr mode,
                  DisplayModePtr adjusted_mode) {
}

static void
GlamoOutputCommit(xf86OutputPtr output) {
    int fd = open(display_state_switch_path, O_WRONLY);
    if (fd != -1) {
        if(output->crtc->mode.HDisplay == 240 && output->crtc->mode.VDisplay == 320)
            write(fd, display_state_qvga, strlen(display_state_qvga));
        else
            write(fd, display_state_vga, strlen(display_state_vga));
        close(fd);
    } else {
        xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
                   "Couldn't open %s to change display resolution: %s\n",
                   display_state_switch_path, strerror(errno));
    }
}

static void GlamoOutputDestroy(xf86OutputPtr output) {
}

static DisplayModePtr GlamoOutputGetModes(xf86OutputPtr output) {
    GlamoPtr pGlamo = GlamoPTR(output->scrn);

    output->mm_width = pGlamo->fb_var.width;
    output->mm_height = pGlamo->fb_var.height;

    return NULL;
    /*return fbdevHWGetBuildinMode(output->scrn);*/
}

