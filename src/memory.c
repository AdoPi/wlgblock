/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "core.h"
#include "apu.h"
#include "bit_utils.h"
#include "debug.h"
#include "gbcc.h"
#include "hdma.h"
#include "mbc.h"
#include "memory.h"
#include "ppu.h"
#include "printer.h"
#include <stdio.h>

static const uint8_t ioreg_read_masks[0x80] = {
/* 0xFF00 */	0x3F, 0xFF, 0x83, 0x00, 0xFF, 0xFF, 0xFF, 0x07,
/* 0xFF08 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F,
/* 0xFF10 */	0x7F, 0xC0, 0xFF, 0x00, 0x40, 0x00, 0xC0, 0xFF,
/* 0xFF18 */	0x00, 0x40, 0x80, 0x00, 0x60, 0x00, 0x40, 0x00,
/* 0xFF20 */	0x00, 0xFF, 0xFF, 0x40, 0xFF, 0xFF, 0x8F, 0x00,
/* 0xFF28 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF30 */	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0xFF38 */	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0xFF40 */	0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0xFF48 */	0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x81, 0x00, 0x01,
/* 0xFF50 */	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC3, 0x00,
/* 0xFF58 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF60 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF68 */	0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
/* 0xFF70 */	0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF78 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t ioreg_write_masks[0x80] = {
/* 0xFF00 */	0x30, 0xFF, 0x83, 0x00, 0xFF, 0xFF, 0xFF, 0x07,
/* 0xFF08 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F,
/* 0xFF10 */	0x7F, 0xFF, 0xFF, 0xFF, 0xC7, 0x00, 0xFF, 0xFF,
/* 0xFF18 */	0xFF, 0xC7, 0x80, 0xFF, 0x60, 0xFF, 0xC7, 0x00,
/* 0xFF20 */	0x3F, 0xFF, 0xFF, 0xC0, 0xFF, 0xFF, 0x80, 0x00,
/* 0xFF28 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF30 */	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0xFF38 */	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0xFF40 */	0xFF, 0x78, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF,
/* 0xFF48 */	0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01, 0x00, 0x01,
/* 0xFF50 */	0x00, 0xFF, 0xF0, 0x1F, 0xF0, 0xFF, 0xC2, 0x00,
/* 0xFF58 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF60 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF68 */	0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
/* 0xFF70 */	0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF78 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t ioreg_dmg_masks[0x80] = {
/* 0xFF00 */	0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF08 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF10 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF18 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF20 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF28 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF30 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF38 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF40 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF48 */	0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF,
/* 0xFF50 */	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
/* 0xFF58 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF60 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF68 */	0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
/* 0xFF70 */	0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0xFF78 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t vram_read(struct gbcc_core *gbc, uint16_t addr);
static void vram_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);

static uint8_t wram_read(struct gbcc_core *gbc, uint16_t addr);
static void wram_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);

static uint8_t echo_read(struct gbcc_core *gbc, uint16_t addr);
static void echo_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);

static uint8_t oam_read(struct gbcc_core *gbc, uint16_t addr);
static void oam_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);

static uint8_t unused_read(struct gbcc_core *gbc, uint16_t addr);
static void unused_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);

static uint8_t ioreg_read(struct gbcc_core *gbc, uint16_t addr);
static void ioreg_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);

static uint8_t hram_read(struct gbcc_core *gbc, uint16_t addr);
static void hram_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);

void gbcc_memory_increment(struct gbcc_core *gbc, uint16_t addr)
{
	gbcc_memory_write(gbc, addr, gbcc_memory_read(gbc, addr) + 1);
}

void gbcc_memory_copy(struct gbcc_core *gbc, uint16_t src, uint16_t dest)
{
	gbcc_memory_write(gbc, dest, gbcc_memory_read(gbc, src));
}

void gbcc_memory_set_bit(struct gbcc_core *gbc, uint16_t addr, uint8_t b)
{
	gbcc_memory_write(gbc, addr, gbcc_memory_read(gbc, addr) | bit(b));
}

void gbcc_memory_clear_bit(struct gbcc_core *gbc, uint16_t addr, uint8_t b)
{
	gbcc_memory_write(gbc, addr, gbcc_memory_read(gbc, addr) & (uint8_t)~bit(b));
}

ANDROID_INLINE
uint8_t gbcc_memory_read_force(struct gbcc_core *gbc, uint16_t addr) {
	if (addr >= OAM_START && addr < OAM_END) {
		return gbc->memory.oam[addr - OAM_START];
	} else if (addr >= IOREG_START && addr < IOREG_END) {
		return gbc->memory.ioreg[addr - IOREG_START];
	}
	return gbcc_memory_read(gbc, addr);
}

uint8_t gbcc_memory_read(struct gbcc_core *gbc, uint16_t addr)
{
	if (addr < ROMX_END || (addr >= SRAM_START && addr < SRAM_END)) {
		uint8_t ret;
		switch (gbc->cart.mbc.type) {
			case NONE:
				ret = gbcc_mbc_none_read(gbc, addr);
				break;
			case MBC1:
				ret = gbcc_mbc_mbc1_read(gbc, addr);
				break;
			case MBC2:
				ret = gbcc_mbc_mbc2_read(gbc, addr);
				break;
			case MBC3:
				ret = gbcc_mbc_mbc3_read(gbc, addr);
				break;
			case MBC5:
				ret = gbcc_mbc_mbc5_read(gbc, addr);
				break;
			case MBC6:
				ret = gbcc_mbc_mbc6_read(gbc, addr);
				break;
			case MBC7:
				ret = gbcc_mbc_mbc7_read(gbc, addr);
				break;
			case HUC1:
				ret = gbcc_mbc_huc1_read(gbc, addr);
				break;
			case HUC3:
				ret = gbcc_mbc_huc3_read(gbc, addr);
				break;
			case MMM01:
				ret = gbcc_mbc_mmm01_read(gbc, addr);
				break;
			case CAMERA:
				ret = gbcc_mbc_cam_read(gbc, addr);
				break;
		}
		if (addr < ROMX_END && gbc->cheats.enabled) {
			return gbcc_cheats_gamegenie_read(gbc, addr, ret);
		}
		return ret;
	}
	if (addr >= VRAM_START && addr < VRAM_END) {
		return vram_read(gbc, addr);
	}
	if (addr >= WRAM0_START && addr < WRAMX_END) {
		return wram_read(gbc, addr);
	}
	if (addr >= ECHO_START && addr < ECHO_END) {
		return echo_read(gbc, addr);
	}
	if (addr >= OAM_START && addr < OAM_END) {
		return oam_read(gbc, addr);
	}
	if (addr >= UNUSED_START && addr < UNUSED_END) {
		return unused_read(gbc, addr);
	}
	if (addr >= IOREG_START && addr < IOREG_END) {
		return ioreg_read(gbc, addr);
	}
	if (addr >= HRAM_START && addr < HRAM_END) {
		return hram_read(gbc, addr);
	}
	if (addr == IE) {
		return gbc->memory.iereg;
	}
	gbcc_log_error("Reading from unknown memory address %04X.\n", addr);
	return 0;
}

ANDROID_INLINE
void gbcc_memory_write_force(struct gbcc_core *gbc, uint16_t addr, uint8_t val) {
	if (addr >= OAM_START && addr < OAM_END) {
		gbc->memory.oam[addr - OAM_START] = val;
	} else if (addr >= IOREG_START && addr < IOREG_END) {
		gbc->memory.ioreg[addr - IOREG_START] = val;
	} else {
		gbcc_memory_write(gbc, addr, val);
	}
}

void gbcc_memory_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	if (addr < ROMX_END || (addr >= SRAM_START && addr < SRAM_END)) {
		if (addr >= SRAM_START && addr < SRAM_END) {
			gbc->cart.mbc.last_save_time = time(NULL);
			gbc->cart.mbc.sram_changed = true;
		}
		switch (gbc->cart.mbc.type) {
			case NONE:
				gbcc_mbc_none_write(gbc, addr, val);
				break;
			case MBC1:
				gbcc_mbc_mbc1_write(gbc, addr, val);
				break;
			case MBC2:
				gbcc_mbc_mbc2_write(gbc, addr, val);
				break;
			case MBC3:
				gbcc_mbc_mbc3_write(gbc, addr, val);
				break;
			case MBC5:
				gbcc_mbc_mbc5_write(gbc, addr, val);
				break;
			case MBC6:
				gbcc_mbc_mbc6_write(gbc, addr, val);
				break;
			case MBC7:
				gbcc_mbc_mbc7_write(gbc, addr, val);
				break;
			case HUC1:
				gbcc_mbc_huc1_write(gbc, addr, val);
				break;
			case HUC3:
				gbcc_mbc_huc3_write(gbc, addr, val);
				break;
			case MMM01:
				gbcc_mbc_mmm01_write(gbc, addr, val);
				break;
			case CAMERA:
				gbcc_mbc_cam_write(gbc, addr, val);
				break;
		}
		return;
	}
	if (addr >= VRAM_START && addr < VRAM_END) {
		vram_write(gbc, addr, val);
	} else if (addr >= WRAM0_START && addr < WRAMX_END) {
		wram_write(gbc, addr, val);
	} else if (addr >= ECHO_START && addr < ECHO_END) {
		echo_write(gbc, addr, val);
	} else if (addr >= OAM_START && addr < OAM_END) {
		oam_write(gbc, addr, val);
	} else if (addr >= UNUSED_START && addr < UNUSED_END) {
		unused_write(gbc, addr, val);
	} else if (addr >= IOREG_START && addr < IOREG_END) {
		ioreg_write(gbc, addr, val);
	} else if (addr >= HRAM_START && addr < HRAM_END) {
		hram_write(gbc, addr, val);
	} else if (addr == IE) {
		gbc->memory.iereg = val;
	} else {
		gbcc_log_error("Writing to unknown memory address %04X.\n", addr);
	}
}

uint8_t vram_read(struct gbcc_core *gbc, uint16_t addr)
{
	return gbc->memory.vram[addr - VRAM_START];
}

void vram_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	gbc->memory.vram[addr - VRAM_START] = val;
}

uint8_t wram_read(struct gbcc_core *gbc, uint16_t addr)
{
	if (addr < WRAMX_START) {
		return gbc->memory.wram0[addr - WRAM0_START];
	}
	return gbc->memory.wramx[addr - WRAMX_START];
}

void wram_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	if (addr < WRAMX_START) {
		gbc->memory.wram0[addr - WRAM0_START] = val;
	} else {
		gbc->memory.wramx[addr - WRAMX_START] = val;
	}
}

uint8_t echo_read(struct gbcc_core *gbc, uint16_t addr)
{
	return wram_read(gbc, addr - WRAMX_SIZE - WRAM0_SIZE);
}

void echo_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	wram_write(gbc, addr - WRAMX_SIZE - WRAM0_SIZE, val);
}

uint8_t oam_read(struct gbcc_core *gbc, uint16_t addr)
{
	uint8_t stat = gbc->memory.ioreg[STAT - IOREG_START];
	if (stat & 0x02u || gbc->cpu.dma.running) {
		 /* CPU cannot access oam during dma or STAT modes 2 & 3 */
		return 0xFFu;
	}
	return gbc->memory.oam[addr - OAM_START];
}

void oam_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	uint8_t stat = gbc->memory.ioreg[STAT - IOREG_START];
	if (stat & 0x02u || gbc->cpu.dma.running) {
		 /* CPU cannot access oam during dma or STAT modes 2 & 3 */
		return;
	}
	gbc->memory.oam[addr - OAM_START] = val;
}

uint8_t unused_read(struct gbcc_core *gbc, uint16_t addr)
{
	/*
	 * According to Antonio's TCAGBD, the CGB rev. D repeats the 16 bytes
	 * from 0xFEC0-0xFECF throughout the rest of this memory area.
	 */
	if (addr > 0xFED0u) {
		addr = 0xFED0u + (addr % 0x10u);
	}
	return gbc->memory.unused[addr - UNUSED_START];
}

void unused_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	if (addr > 0xFED0u) {
		addr = 0xFED0u + (addr % 0x10u);
	}
	gbc->memory.unused[addr - UNUSED_START] = val;
}

uint8_t ioreg_read(struct gbcc_core *gbc, uint16_t addr)
{
	uint8_t mask = ioreg_read_masks[addr - IOREG_START];
	uint8_t ret = gbc->memory.ioreg[addr - IOREG_START];

	/* Ignore GBC-specific registers when in DMG mode */
	if (gbc->mode == DMG) {
		mask &= ~ioreg_dmg_masks[addr - IOREG_START];
	}


	if (addr >= WAVE_START && addr < WAVE_END) {
		/*
		 * When the wave channel is enabled, accessing any wave RAM
		 * accesses the current byte.
		 */
		if (gbc->apu.ch3.enabled) {
			return gbc->memory.ioreg[gbc->apu.wave.addr - IOREG_START];
		}
	}

	switch (addr) {
		case LY:
			if (gbc->ppu.lcd_disable) {
				return 0;
			}
			break;
		case JOYP:
			/* Only update the keys when we actually want to read from them */
			ret |= 0x0Fu;
			if (!check_bit(ret, 5)) {
				ret &= (uint8_t)~(uint8_t)(gbc->keys.start << 3u);
				ret &= (uint8_t)~(uint8_t)(gbc->keys.select << 2u);
				ret &= (uint8_t)~(uint8_t)(gbc->keys.b << 1u);
				ret &= (uint8_t)~(uint8_t)(gbc->keys.a << 0u);
			}
			if (!check_bit(ret, 4)) {
				ret &= (uint8_t)~(uint8_t)(gbc->keys.dpad.down << 3u);
				ret &= (uint8_t)~(uint8_t)(gbc->keys.dpad.up << 2u);
				ret &= (uint8_t)~(uint8_t)(gbc->keys.dpad.left << 1u);
				ret &= (uint8_t)~(uint8_t)(gbc->keys.dpad.right << 0u);
			}
			gbc->memory.ioreg[addr - IOREG_START] = ret;
			break;
		case DIV:
			return high_byte(gbc->cpu.div_timer);
		case NR52:
			ret &= 0xF0u;
			ret |= (uint8_t)(gbc->apu.ch1.enabled << 0u);
			ret |= (uint8_t)(gbc->apu.ch2.enabled << 1u);
			ret |= (uint8_t)(gbc->apu.ch3.enabled << 2u);
			ret |= (uint8_t)(gbc->apu.ch4.enabled << 3u);
			ret |= (uint8_t)(!gbc->apu.disabled << 7u);
			break;
		case BGPD:
			ret = gbc->ppu.bgp[gbc->memory.ioreg[BGPI - IOREG_START] & 0x3Fu];
			break;
		case OBPD:
			ret = gbc->ppu.obp[gbc->memory.ioreg[OBPI - IOREG_START] & 0x3Fu];
			break;
		default:
			ret = gbc->memory.ioreg[addr - IOREG_START];
			break;
	}
	return ret | (uint8_t)~mask;
}

void ioreg_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	/*
	if (addr == LCDC) {
		printf("LCDC = 0b");
		for (int i = 7; i >= 0; i--) {
			printf("%u", check_bit(val, i));
		}
		printf("\n");
	}
	if (addr == STAT) {
		printf("STAT = 0b");
		val |= ~ioreg_read_masks[addr - IOREG_START];
		for (int i = 7; i >= 0; i--) {
			printf("%u", check_bit(val, i));
		}
		printf(" (0x%02X)\n", val);
	}
	*/

	uint8_t *dest = &gbc->memory.ioreg[addr - IOREG_START];
	uint8_t mask = ioreg_write_masks[addr - IOREG_START];
	/* Ignore GBC-specific registers when in DMG mode */
	if (gbc->mode == DMG) {
		mask &= ~ioreg_dmg_masks[addr - IOREG_START];
	}
	uint8_t tmp = *dest & (uint8_t)~mask;
	
	if (addr >= WAVE_START && addr < WAVE_END) {
		/*
		 * When the wave channel is enabled, accessing any wave RAM
		 * accesses the current byte.
		 */
		if (gbc->apu.ch3.enabled) {
			gbc->memory.ioreg[gbc->apu.wave.addr - IOREG_START] = val;
		}
	}

	if (addr >= NR10 && addr <= NR52) {
		if (addr != NR52 && gbc->apu.disabled) {
			return;
		}
		*dest = tmp | (uint8_t)(val & mask);
		gbcc_apu_memory_write(gbc, addr, val);
		return;
	}

	switch (addr) {
		case LY:
			*dest = 0;
			break;
		case JOYP:
			*dest &= 0x0Fu;
			*dest |= val;
			break;
		case SC:
			*dest = tmp | (val & mask);
			if (gbc->link_cable.state == GBCC_LINK_CABLE_STATE_LOOPBACK) {
				*dest = clear_bit(*dest, 7);
				gbcc_memory_set_bit(gbc, IF, 3);
				return;
			}
			if (check_bit(val, 1)) {
				gbc->link_cable.divider = 16;
			} else {
				gbc->link_cable.divider = 512;
			}
			if (check_bit(val, 7)) {
				if (!check_bit(val, 0)) {
					/*
					 * Externally clocked transfer,
					 * do nothing for now.
					 */
					return;
				}
				//fprintf(stderr, "%c\n", gbc->memory.ioreg[SB - IOREG_START]);
				//fprintf(stdout, "0x%02X\n", gbc->memory.ioreg[SB - IOREG_START]);
				/*
				 * If link_cable_loop is true, just receive SB.
				 * This means the gameboy acts like it's
				 * talking to an exact clone of itself.
				 */
				switch (gbc->link_cable.state) {
					case GBCC_LINK_CABLE_STATE_DISCONNECTED:
						gbc->link_cable.received = 0xFFu;
						break;
					case GBCC_LINK_CABLE_STATE_LOOPBACK:
						gbc->link_cable.received = gbc->memory.ioreg[SB - IOREG_START];
						break;
					case GBCC_LINK_CABLE_STATE_PRINTER:
						gbc->link_cable.received = gbcc_printer_parse_byte(&gbc->printer, gbc->memory.ioreg[SB - IOREG_START]);
						break;
					default:
						break;
				}
				return;
			}
			break;
		case DIV:
			//printf("DIV reset from %04X\n", gbc->div_timer);
			gbc->cpu.div_timer = 0;
			break;
		case LCDC:
			if (check_bit(val, 7)) {
				gbcc_enable_lcd(gbc);
			} else {
				gbcc_disable_lcd(gbc);
			}
			*dest = tmp | (uint8_t)(val & mask);
			break;
		case LYC:
			*dest = tmp | (uint8_t)(val & mask);
			gbc->ppu.lyc = val;
			break;
		case DMA:
			gbc->cpu.dma.new_source = (uint16_t)(val << 8u);
			if (gbc->cpu.dma.new_source > WRAMX_END) {
				/* Can't DMA from ECHO or IOREG areas */
				gbc->cpu.dma.new_source -= 0x2000u;
			}
			gbc->cpu.dma.requested = true;
			*dest = tmp | (uint8_t)(val & mask);
			break;
		case VBK:
			*dest = tmp | (uint8_t)(val & mask);
			gbc->memory.vram = gbc->memory.vram_bank[*dest];
			break;
		case HDMA1:
			if (val < 0x80u || (val >= 0xA0u && val < 0xE0u)) {
				*dest = val;
			}
			break;
		case HDMA2:
			/* Lower 4 bits of source are ignored */
			*dest = val & 0xF0u;
			break;
		case HDMA3:
			/*
			 * Upper 3 bits of destination are ignored
			 * (destination is always VRAM)
			 */
			*dest = (val & 0x1Fu) | 0x80u;
			break;
		case HDMA4:
			/* Lower 4 bits of destination are ignored */
			*dest = val & 0xF0u;
			break;
		case HDMA5:
			{
				if (!check_bit(val, 7)) {
					if (gbc->hdma.length > 0) {
						gbc->hdma.length = 0;
						gbc->hdma.to_copy = 0;
						*dest |= 0x80u;
						return;
					}
				}
				uint8_t src_hi = gbc->memory.ioreg[HDMA1 - IOREG_START];
				uint8_t src_lo = gbc->memory.ioreg[HDMA2 - IOREG_START];
				uint8_t dst_hi = gbc->memory.ioreg[HDMA3 - IOREG_START];
				uint8_t dst_lo = gbc->memory.ioreg[HDMA4 - IOREG_START];
				gbc->hdma.source = cat_bytes(src_lo, src_hi);
				gbc->hdma.dest = cat_bytes(dst_lo, dst_hi);
				gbc->hdma.length = ((val & 0x7Fu) + 1u) * 0x10u;
				/* Top bit is is set to 0 to indicate running */
				*dest = val & 0x7Fu;
				if (check_bit(val, 7)) {
					/* H-Blank DMA */
					gbc->hdma.hblank = true;
					/*
					 * When started in HBLANK or while the
					 * screen is off, one block is copied
					 * immediately.
					 */
					uint8_t stat = gbc->memory.ioreg[STAT - IOREG_START];
					if ((stat & 0x03u) == GBC_LCD_MODE_HBLANK) {
						gbc->hdma.to_copy = 0x10u;
					}
					if (gbc->ppu.lcd_disable) {
						gbc->hdma.to_copy = 0x10u;
					}
				} else {
					/* General DMA */
					gbc->hdma.to_copy = gbc->hdma.length;
					gbc->hdma.hblank = false;
				}
			}
			break;
		case RP:
			/* TODO: Handle */
			break;
		case BGPD:
			{
				uint8_t index = gbc->memory.ioreg[BGPI - IOREG_START];
				gbc->ppu.bgp[index & 0x3Fu] = val;
				if (check_bit(index, 7)) {
					index++;
					if ((index & 0x7Fu) == 0x40u) {
						index = bit(7);
					}
					gbc->memory.ioreg[BGPI - IOREG_START] = index;
				}
			}
			break;
		case OBPD:
			{
				uint8_t index = gbc->memory.ioreg[OBPI - IOREG_START];
				gbc->ppu.obp[index & 0x3Fu] = val;
				if (check_bit(index, 7)) {
					index++;
					if ((index & 0x7Fu) == 0x40u) {
						index = bit(7);
					}
					gbc->memory.ioreg[OBPI - IOREG_START] = index;
				}
			}
			break;
		case SVBK:
			{
				uint8_t bank = tmp | (val & mask);
				bank += !bank;
				*dest = bank;
				gbc->memory.wramx = gbc->memory.wram_bank[bank];
			}
			break;
		default:
			*dest = tmp | (uint8_t)(val & mask);
			break;
	}
}

uint8_t hram_read(struct gbcc_core *gbc, uint16_t addr)
{
	return gbc->memory.hram[addr - HRAM_START];
}

void hram_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	gbc->memory.hram[addr - HRAM_START] = val;
}

void gbcc_link_cable_clock(struct gbcc_core *gbc)
{
	uint8_t sc = gbcc_memory_read_force(gbc, SC);
	if (!check_bit(sc, 7) || !check_bit(sc, 0)) {
		return;
	}
	uint8_t sb = gbcc_memory_read_force(gbc, SB);

	gbc->link_cable.clock++;
	if (gbc->link_cable.clock < gbc->link_cable.divider) {
		return;
	}

	gbc->link_cable.clock = 0;
	sb <<= 1;
	sb |= check_bit(gbc->link_cable.received, 7 - gbc->link_cable.current_bit);
	gbcc_memory_write_force(gbc, SB, sb);
	gbc->link_cable.current_bit++;

	if (gbc->link_cable.current_bit == 8) {
		gbc->link_cable.current_bit = 0;
		uint8_t tmp = gbcc_memory_read_force(gbc, SC);
		gbcc_memory_write_force(gbc, SC, clear_bit(tmp, 7));
		gbcc_memory_set_bit(gbc, IF, 3);
	}
}
