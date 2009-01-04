/*
 * Copyright  2007 OpenMoko, Inc.
 *
 * This driver is based on Xati,
 * Copyright  2004 Eric Anholt
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

#include <sys/time.h>

#include "glamo-log.h"
#include "glamo.h"
#include "glamo-regs.h"
#include "glamo-cmdq.h"
#include "glamo-draw.h"

static void GLAMOCMDQResetCP(GlamoPtr pGlamo);
#ifndef NDEBUG
static void
GLAMODebugFifo(GlamoPtr pGlamo)
{
	GLAMOCardInfo *glamoc = pGlamo->glamoc;
	char *mmio = glamoc->reg_base;
	CARD32 offset;

	ErrorF("GLAMO_REG_CMDQ_STATUS: 0x%04x\n",
	    MMIO_IN16(mmio, GLAMO_REG_CMDQ_STATUS));

	offset = MMIO_IN16(mmio, GLAMO_REG_CMDQ_WRITE_ADDRL);
	offset |= (MMIO_IN16(mmio, GLAMO_REG_CMDQ_WRITE_ADDRH) << 16) & 0x7;
	ErrorF("GLAMO_REG_CMDQ_WRITE_ADDR: 0x%08x\n", (unsigned int) offset);

	offset = MMIO_IN16(mmio, GLAMO_REG_CMDQ_READ_ADDRL);
	offset |= (MMIO_IN16(mmio, GLAMO_REG_CMDQ_READ_ADDRH) << 16) & 0x7;
	ErrorF("GLAMO_REG_CMDQ_READ_ADDR: 0x%08x\n", (unsigned int) offset);
}
#endif

void
GLAMOEngineReset(GlamoPtr pGlamo, enum GLAMOEngine engine)
{
	CARD32 reg;
	CARD16 mask;
	char *mmio = pGlamo->reg_base;

	if (!mmio)
		return;

	switch (engine) {
		case GLAMO_ENGINE_CMDQ:
			reg = GLAMO_REG_CLOCK_2D;
			mask = GLAMO_CLOCK_2D_CMDQ_RESET;
			break;
		case GLAMO_ENGINE_ISP:
			reg = GLAMO_REG_CLOCK_ISP;
			mask = GLAMO_CLOCK_ISP2_RESET;
			break;
		case GLAMO_ENGINE_2D:
			reg = GLAMO_REG_CLOCK_2D;
			mask = GLAMO_CLOCK_2D_RESET;
			break;
		default:
			return;
			break;
	}
	MMIOSetBitMask(mmio, reg, mask, 0xffff);
	usleep(5);
	MMIOSetBitMask(mmio, reg, mask, 0);
	usleep(5);

}

void
GLAMOEngineDisable(GlamoPtr pGlamo, enum GLAMOEngine engine)
{
	char *mmio = pGlamo->reg_base;

	if (!mmio)
		return;
	switch (engine) {
		case GLAMO_ENGINE_CMDQ:
			MMIOSetBitMask(mmio, GLAMO_REG_CLOCK_2D,
					GLAMO_CLOCK_2D_EN_M6CLK,
					0);
			MMIOSetBitMask(mmio, GLAMO_REG_HOSTBUS(2),
					GLAMO_HOSTBUS2_MMIO_EN_CMDQ,
					0);
			MMIOSetBitMask(mmio, GLAMO_REG_CLOCK_GEN5_1,
					GLAMO_CLOCK_GEN51_EN_DIV_MCLK,
					0);
			break;
		case GLAMO_ENGINE_ISP:
			MMIOSetBitMask(mmio, GLAMO_REG_CLOCK_ISP,
					GLAMO_CLOCK_ISP_EN_M2CLK |
					GLAMO_CLOCK_ISP_EN_I1CLK,
					0);
			MMIOSetBitMask(mmio, GLAMO_REG_CLOCK_GEN5_2,
					GLAMO_CLOCK_GEN52_EN_DIV_ICLK,
					0);
			MMIOSetBitMask(mmio, GLAMO_REG_CLOCK_GEN5_1,
					GLAMO_CLOCK_GEN51_EN_DIV_JCLK,
					0);
			MMIOSetBitMask(mmio, GLAMO_REG_HOSTBUS(2),
					GLAMO_HOSTBUS2_MMIO_EN_ISP,
					0);
			break;
		case GLAMO_ENGINE_2D:
			MMIOSetBitMask(mmio, GLAMO_REG_CLOCK_2D,
					GLAMO_CLOCK_2D_EN_M7CLK |
					GLAMO_CLOCK_2D_EN_GCLK |
					GLAMO_CLOCK_2D_DG_M7CLK |
					GLAMO_CLOCK_2D_DG_GCLK,
					0);
			MMIOSetBitMask(mmio, GLAMO_REG_HOSTBUS(2),
					GLAMO_HOSTBUS2_MMIO_EN_2D,
					0);
			MMIOSetBitMask(mmio, GLAMO_REG_CLOCK_GEN5_1,
					GLAMO_CLOCK_GEN51_EN_DIV_GCLK,
					0);
			break;
		default:
			break;
	}
	return;
}

void
GLAMOEngineEnable(GlamoPtr pGlamo, enum GLAMOEngine engine)
{
	char *mmio = pGlamo->reg_base;

	if (!mmio)
		return;

	switch (engine) {
		case GLAMO_ENGINE_CMDQ:
			MMIOSetBitMask(mmio, GLAMO_REG_CLOCK_2D,
					GLAMO_CLOCK_2D_EN_M6CLK,
					0xffff);
			MMIOSetBitMask(mmio, GLAMO_REG_HOSTBUS(2),
					GLAMO_HOSTBUS2_MMIO_EN_CMDQ,
					0xffff);
			MMIOSetBitMask(mmio, GLAMO_REG_CLOCK_GEN5_1,
					GLAMO_CLOCK_GEN51_EN_DIV_MCLK,
					0xffff);
			break;
		case GLAMO_ENGINE_ISP:
			MMIOSetBitMask(mmio, GLAMO_REG_CLOCK_ISP,
					GLAMO_CLOCK_ISP_EN_M2CLK |
					GLAMO_CLOCK_ISP_EN_I1CLK,
					0xffff);
			MMIOSetBitMask(mmio, GLAMO_REG_CLOCK_GEN5_2,
					GLAMO_CLOCK_GEN52_EN_DIV_ICLK,
					0xffff);
			MMIOSetBitMask(mmio, GLAMO_REG_CLOCK_GEN5_1,
					GLAMO_CLOCK_GEN51_EN_DIV_JCLK,
					0xffff);
			MMIOSetBitMask(mmio, GLAMO_REG_HOSTBUS(2),
					GLAMO_HOSTBUS2_MMIO_EN_ISP,
					0xffff);
			break;
		case GLAMO_ENGINE_2D:
			MMIOSetBitMask(mmio, GLAMO_REG_CLOCK_2D,
					GLAMO_CLOCK_2D_EN_M7CLK |
					GLAMO_CLOCK_2D_EN_GCLK |
					GLAMO_CLOCK_2D_DG_M7CLK |
					GLAMO_CLOCK_2D_DG_GCLK,
					0xffff);
			MMIOSetBitMask(mmio, GLAMO_REG_HOSTBUS(2),
					GLAMO_HOSTBUS2_MMIO_EN_2D,
					0xffff);
			MMIOSetBitMask(mmio, GLAMO_REG_CLOCK_GEN5_1,
					GLAMO_CLOCK_GEN51_EN_DIV_GCLK,
					0xffff);
			break;
		default:
			break;
	}
}

int
GLAMOEngineBusy(GlamoPtr pGlamo, enum GLAMOEngine engine)
{
	char *mmio = pGlamo->reg_base;
	CARD16 status, mask, val;

	if (!mmio)
		return FALSE;

	if (pGlamo->cmd_queue_cache != NULL)
		GLAMOFlushCMDQCache(pGlamo, 0);

	switch (engine)
	{
		case GLAMO_ENGINE_CMDQ:
			mask = 0x3;
			val  = mask;
			break;
		case GLAMO_ENGINE_ISP:
			mask = 0x3 | (1 << 8);
			val  = 0x3;
			break;
		case GLAMO_ENGINE_2D:
			mask = 0x3 | (1 << 4);
			val  = 0x3;
			break;
		case GLAMO_ENGINE_ALL:
		default:
			mask = 1 << 2;
			val  = mask;
			break;
	}

	status = MMIO_IN16(mmio, GLAMO_REG_CMDQ_STATUS);

	return !((status & mask) == val);
}

static void
GLAMOEngineWaitReal(GlamoPtr pGlamo,
		   enum GLAMOEngine engine,
		   Bool do_flush)
{
	char *mmio = pGlamo->reg_base;
	CARD16 status, mask, val;
	TIMEOUT_LOCALS;

	if (!mmio)
		return;

	if (pGlamo->cmd_queue_cache != NULL && do_flush)
		GLAMOFlushCMDQCache(pGlamo, 0);

	switch (engine)
	{
		case GLAMO_ENGINE_CMDQ:
			mask = 0x3;
			val  = mask;
			break;
		case GLAMO_ENGINE_ISP:
			mask = 0x3 | (1 << 8);
			val  = 0x3;
			break;
		case GLAMO_ENGINE_2D:
			mask = 0x3 | (1 << 4);
			val  = 0x3;
			break;
		case GLAMO_ENGINE_ALL:
		default:
			mask = 1 << 2;
			val  = mask;
			break;
	}

	WHILE_NOT_TIMEOUT(5) {
		status = MMIO_IN16(mmio, GLAMO_REG_CMDQ_STATUS);
		if ((status & mask) == val)
			break;
	}
	if (TIMEDOUT()) {
		GLAMO_LOG_ERROR("Timeout idling accelerator "
				"(0x%x), resetting...\n",
				status);
		GLAMODumpRegs(pGlamo, 0x1600, 0x1612);
		GLAMOEngineReset(pGlamo, GLAMO_ENGINE_CMDQ);
		GLAMOEngineEnable(pGlamo, GLAMO_ENGINE_2D);
        GLAMOEngineReset(pGlamo, GLAMO_ENGINE_2D);
	}
}

void
GLAMOEngineWait(GlamoPtr pGlamo,
		enum GLAMOEngine engine)
{
	GLAMOEngineWaitReal(pGlamo, engine, TRUE);
}

MemBuf *
GLAMOCreateCMDQCache(GlamoPtr pGlamo)
{
	MemBuf *buf;

	buf = (MemBuf *)xcalloc(1, sizeof(MemBuf));
	if (buf == NULL)
		return NULL;

	/*buf->size = glamos->ring_len / 2;*/
	buf->size = pGlamo->ring_len;
	buf->address = xcalloc(1, buf->size);
	if (buf->address == NULL) {
		xfree(buf);
		return NULL;
	}
	buf->used = 0;

	return buf;
}

static void
GLAMODispatchCMDQCache(GlamoPtr pGlamo)
{
	MemBuf *buf = pGlamo->cmd_queue_cache;
	char *mmio = pGlamo->reg_base;
	CARD16 *addr;
	int count, ring_count;
	TIMEOUT_LOCALS;

	addr = (CARD16 *)((char *)buf->address + pGlamo->cmd_queue_cache_start);
	count = (buf->used - pGlamo->cmd_queue_cache_start) / 2;
	ring_count = pGlamo->ring_len / 2;
	if (count + pGlamo->ring_write >= ring_count) {
		GLAMOCMDQResetCP(pGlamo);
		pGlamo->ring_write = 0;
	}

	WHILE_NOT_TIMEOUT(.5) {
		if (count <= 0)
			break;

		pGlamo->ring_addr[pGlamo->ring_write] = *addr;
		pGlamo->ring_write++; addr++;
		if (pGlamo->ring_write >= ring_count) {
			GLAMO_LOG_ERROR("wrapped over ring_write\n");
			GLAMODumpRegs(pGlamo, 0x1600, 0x1612);
			pGlamo->ring_write = 0;
		}
		count--;
	}
	if (TIMEDOUT()) {
		GLAMO_LOG_ERROR("Timeout submitting packets, "
				"resetting...\n");
		GLAMODumpRegs(pGlamo, 0x1600, 0x1612);
		GLAMOEngineReset(pGlamo, GLAMO_ENGINE_CMDQ);
		GLAMODrawSetup(pGlamo);
	}

	MMIO_OUT16(mmio, GLAMO_REG_CMDQ_WRITE_ADDRH,
			 (pGlamo->ring_write >> 15) & 0x7);
	MMIO_OUT16(mmio, GLAMO_REG_CMDQ_WRITE_ADDRL,
			 (pGlamo->ring_write <<  1) & 0xffff);
	GLAMOEngineWaitReal(pGlamo,
			    GLAMO_ENGINE_CMDQ, FALSE);
}

void
GLAMOFlushCMDQCache(GlamoPtr pGlamo, Bool discard)
{
	MemBuf *buf = pGlamo->cmd_queue_cache;

	if ((pGlamo->cmd_queue_cache_start == buf->used) && !discard)
		return;
	GLAMODispatchCMDQCache(pGlamo);

	buf->used = 0;
	pGlamo->cmd_queue_cache_start = 0;
}

#define CQ_LEN 255
static void
GLAMOCMDQResetCP(GlamoPtr pGlamo)
{
	char *mmio = pGlamo->reg_base;
	int cq_len = CQ_LEN;
	CARD32 queue_offset = 0;

	/* make the decoder happy? */
	memset((char*)pGlamo->ring_addr, 0, pGlamo->ring_len+2);

	GLAMOEngineReset(pGlamo, GLAMO_ENGINE_CMDQ);

	queue_offset = pGlamo->exa_cmd_queue->offset;

	MMIO_OUT16(mmio, GLAMO_REG_CMDQ_BASE_ADDRL,
		   queue_offset & 0xffff);
	MMIO_OUT16(mmio, GLAMO_REG_CMDQ_BASE_ADDRH,
		   (queue_offset >> 16) & 0x7f);
	MMIO_OUT16(mmio, GLAMO_REG_CMDQ_LEN, cq_len);

	MMIO_OUT16(mmio, GLAMO_REG_CMDQ_WRITE_ADDRH, 0);
	MMIO_OUT16(mmio, GLAMO_REG_CMDQ_WRITE_ADDRL, 0);
	MMIO_OUT16(mmio, GLAMO_REG_CMDQ_READ_ADDRH, 0);
	MMIO_OUT16(mmio, GLAMO_REG_CMDQ_READ_ADDRL, 0);
	MMIO_OUT16(mmio, GLAMO_REG_CMDQ_CONTROL,
			 1 << 12 |
			 5 << 8 |
			 8 << 4);
	GLAMOEngineWaitReal(pGlamo, GLAMO_ENGINE_ALL, FALSE);
}

static Bool
GLAMOCMDQInit(GlamoPtr pGlamo,
	      Bool force)
{
	char *mmio = pGlamo->reg_base;
	int cq_len = CQ_LEN;

	xf86DrvMsg(0, X_WARNING,
			"GLAMOCMDQInit here1\n");

	if (!force && pGlamo->exa_cmd_queue)
		return TRUE;

	xf86DrvMsg(0, X_WARNING,
			"GLAMOCMDQInit here2\n");

	pGlamo->ring_len = (cq_len + 1) * 1024;

	pGlamo->exa_cmd_queue =
		exaOffscreenAlloc(pGlamo->pScreen, pGlamo->ring_len + 4,
				  pGlamo->exa->pixmapOffsetAlign,
				  TRUE, NULL, NULL);

	xf86DrvMsg(0, X_WARNING,
			"GLAMOCMDQInit here3\n");

	if (!pGlamo->exa_cmd_queue)
		return FALSE;
	pGlamo->ring_addr =
		(CARD16 *) (pGlamo->fbstart +
				pGlamo->exa_cmd_queue->offset);

	GLAMOEngineEnable(pGlamo, GLAMO_ENGINE_CMDQ);

	xf86DrvMsg(0, X_WARNING,
			"GLAMOCMDQInit here4\n");

	GLAMOCMDQResetCP(pGlamo);

	xf86DrvMsg(0, X_WARNING,
			"GLAMOCMDQInit here5\n");

	return TRUE;
}

void
GLAMOCMDQCacheSetup(GlamoPtr pGlamo)
{
	xf86DrvMsg(0, X_WARNING,
			"here1\n");
	GLAMOCMDQInit(pGlamo, TRUE);
	xf86DrvMsg(0, X_WARNING,
			"here2\n");
	if (pGlamo->cmd_queue_cache)
		return;
	xf86DrvMsg(0, X_WARNING,
			"here3\n");
	pGlamo->cmd_queue_cache = GLAMOCreateCMDQCache(pGlamo);
	if (pGlamo->cmd_queue_cache == FALSE)
		FatalError("Failed to allocate cmd queue cache buffer.\n");
}

void
GLAMOCMQCacheTeardown(GlamoPtr pGlamo)
{
	GLAMOEngineWait(pGlamo, GLAMO_ENGINE_ALL);

	xfree(pGlamo->cmd_queue_cache->address);
	xfree(pGlamo->cmd_queue_cache);
	pGlamo->cmd_queue_cache = NULL;
}
