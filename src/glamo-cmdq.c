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
#include <unistd.h>

#include "glamo-log.h"
#include "glamo.h"
#include "glamo-regs.h"
#include "glamo-cmdq.h"
#include "glamo-draw.h"

static void GLAMOCMDQResetCP(GlamoPtr pGlamo);
static void GLAMODumpRegs(GlamoPtr pGlamo, CARD16 from, CARD16 to);

#define CQ_LEN 255
#define CQ_MASK ((CQ_LEN + 1) * 1024 - 1)
#define CQ_MASKL (CQ_MASK & 0xffff)
#define CQ_MASKH (CQ_MASK >> 16)

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
	volatile char *mmio = pGlamo->reg_base;

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
    sleep(1);
    MMIOSetBitMask(mmio, reg, mask, 0);
    sleep(1);
}

void
GLAMOEngineDisable(GlamoPtr pGlamo, enum GLAMOEngine engine)
{
	volatile char *mmio = pGlamo->reg_base;

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
}

void
GLAMOEngineEnable(GlamoPtr pGlamo, enum GLAMOEngine engine)
{
	volatile char *mmio = pGlamo->reg_base;

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
	volatile char *mmio = pGlamo->reg_base;
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
	volatile char *mmio = pGlamo->reg_base;
	CARD16 status, mask, val;

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

	do {
		status = MMIO_IN16(mmio, GLAMO_REG_CMDQ_STATUS);
    } while ((status & mask) != val);
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
	volatile char *mmio = pGlamo->reg_base;
	char *addr;
	size_t count, ring_count;
    size_t rest_size;
    size_t ring_read;
    size_t old_ring_write = pGlamo->ring_write;

    if (!buf->used)
        return;

    addr = ((char *)buf->address);
	count = buf->used;
	ring_count = pGlamo->ring_len;


    pGlamo->ring_write = (((pGlamo->ring_write + count) & CQ_MASK) + 1) & ~1;

    /* Wait until there is enough space to queue the cmd buffer */
    if (pGlamo->ring_write > old_ring_write) {
        do {
	        ring_read = MMIO_IN16(mmio, GLAMO_REG_CMDQ_READ_ADDRL) & CQ_MASKL;
        	ring_read |= ((MMIO_IN16(mmio, GLAMO_REG_CMDQ_READ_ADDRH) & CQ_MASKH) << 16);
        } while(ring_read > old_ring_write && ring_read < pGlamo->ring_write);
    } else {
        do {
	        ring_read = MMIO_IN16(mmio, GLAMO_REG_CMDQ_READ_ADDRL) & CQ_MASKL;
        	ring_read |= ((MMIO_IN16(mmio, GLAMO_REG_CMDQ_READ_ADDRH) & CQ_MASKH) << 16);
        } while(ring_read > old_ring_write || ring_read < pGlamo->ring_write);
    }

    /* Wrap around */
    if (old_ring_write >= pGlamo->ring_write) {
        rest_size = (ring_count - old_ring_write);
        memcpy((char*)(pGlamo->ring_addr) + old_ring_write, addr, rest_size);
        memcpy((char*)(pGlamo->ring_addr), addr+rest_size, count - rest_size);

        /* ring_write being 0 will result in a deadlock because the cmdq read
         * will never stop. To avoid such an behaviour insert an empty
         * instruction. */
        if(pGlamo->ring_write == 0) {
            memset((char*)(pGlamo->ring_addr), 0, 4);
            pGlamo->ring_write = 4;
        }

        /* Before changing write read has to stop */
        GLAMOEngineWaitReal(pGlamo, GLAMO_ENGINE_CMDQ, FALSE);

        /* The write position has to change to trigger a read */
        if(old_ring_write == pGlamo->ring_write) {
            memset((char*)(pGlamo->ring_addr + pGlamo->ring_write), 0, 4);
            pGlamo->ring_write += 4;
/*            MMIO_OUT16(mmio, GLAMO_REG_CMDQ_WRITE_ADDRH,
                       ((pGlamo->ring_write-4) >> 16) & CQ_MASKH);
            MMIO_OUT16(mmio, GLAMO_REG_CMDQ_WRITE_ADDRL,
                       (pGlamo->ring_write-4) & CQ_MASKL);*/
        }
    } else {
        memcpy((char*)(pGlamo->ring_addr) + old_ring_write, addr, count);
        GLAMOEngineWaitReal(pGlamo, GLAMO_ENGINE_CMDQ, FALSE);
    }
    MMIOSetBitMask(mmio, GLAMO_REG_CLOCK_2D,
					GLAMO_CLOCK_2D_EN_M6CLK,
					0);

	MMIO_OUT16(mmio, GLAMO_REG_CMDQ_WRITE_ADDRH,
			   (pGlamo->ring_write >> 16) & CQ_MASKH);
	MMIO_OUT16(mmio, GLAMO_REG_CMDQ_WRITE_ADDRL,
			   pGlamo->ring_write & CQ_MASKL);

    MMIOSetBitMask(mmio, GLAMO_REG_CLOCK_2D,
                GLAMO_CLOCK_2D_EN_M6CLK,
					0xffff);
    buf->used = 0;


    GLAMOEngineWaitReal(pGlamo, GLAMO_ENGINE_ALL, FALSE);
}

void
GLAMOFlushCMDQCache(GlamoPtr pGlamo, Bool discard)
{
	GLAMODispatchCMDQCache(pGlamo);
}

static void
GLAMOCMDQResetCP(GlamoPtr pGlamo)
{
	volatile char *mmio = pGlamo->reg_base;
	int cq_len = CQ_LEN;
	CARD32 queue_offset = 0;

	/* make the decoder happy? */
	memset((char*)pGlamo->ring_addr, 0, pGlamo->ring_len);

    pGlamo->ring_write = 0;

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
	int cq_len = CQ_LEN;

	if (!force && pGlamo->exa_cmd_queue)
		return TRUE;

	pGlamo->ring_len = (cq_len + 1) * 1024;

	pGlamo->exa_cmd_queue =
		exaOffscreenAlloc(pGlamo->pScreen, pGlamo->ring_len,
				  pGlamo->exa->pixmapOffsetAlign,
				  TRUE, NULL, NULL);

	if (!pGlamo->exa_cmd_queue)
		return FALSE;
	pGlamo->ring_addr =
		(CARD16 *) (pGlamo->fbstart +
				pGlamo->exa_cmd_queue->offset);

	GLAMOEngineEnable(pGlamo, GLAMO_ENGINE_CMDQ);

	GLAMOCMDQResetCP(pGlamo);

	return TRUE;
}

void
GLAMOCMDQCacheSetup(GlamoPtr pGlamo)
{
	GLAMOCMDQInit(pGlamo, TRUE);
	if (pGlamo->cmd_queue_cache)
		return;
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
    if(0)
        GLAMODumpRegs(pGlamo, 0, 0);
}

static void
GLAMODumpRegs(GlamoPtr pGlamo,
              CARD16 from,
              CARD16 to)
{
	int i=0;
	for (i=from; i <= to; i += 2) {
	    ErrorF("reg:%p, val:%#x\n",
		pGlamo->reg_base+i,
		*(VOL16*)(pGlamo->reg_base+i));
	}
}
