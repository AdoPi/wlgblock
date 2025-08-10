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
#include "bit_utils.h"
#include "debug.h"
#include "mbc.h"
#include "time_diff.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static void set_mbc_banks(struct gbcc_core *gbc);
static void eeprom_write(struct gbcc_core *gbc, uint8_t val);
static void eeprom_reset(struct gbcc_eeprom *eeprom);

void set_mbc_banks(struct gbcc_core *gbc)
{
	struct gbcc_mbc *mbc = &gbc->cart.mbc;
	/*
	 * Perform some sanity checks on selected values,
	 * discarding bits that don't make sense.
	 */
	if (mbc->rom0_bank >= gbc->cart.rom_banks) {
		gbcc_log_debug("Invalid rom0 bank %u.\n", mbc->rom0_bank);
		mbc->rom0_bank &= (gbc->cart.rom_banks - 1);
	}
	if (mbc->romx_bank >= gbc->cart.rom_banks) {
		gbcc_log_debug("Invalid romx bank %u.\n", mbc->romx_bank);
		mbc->romx_bank &= (gbc->cart.rom_banks - 1);
	}
	if (mbc->sram_bank >= gbc->cart.ram_banks && gbc->cart.ram_banks > 0) {
		gbcc_log_debug("Invalid ram bank %u.\n", mbc->sram_bank);
		mbc->sram_bank &= (gbc->cart.ram_banks - 1);
	}

	/* And finally update the actual banks */
	gbc->memory.rom0 = gbc->cart.rom + mbc->rom0_bank * ROM0_SIZE;
	gbc->memory.romx = gbc->cart.rom + mbc->romx_bank * ROMX_SIZE;
	if (gbc->cart.ram != NULL) {
		gbc->memory.sram = gbc->cart.ram + mbc->sram_bank * SRAM_SIZE;
	}
}

uint8_t gbcc_mbc_none_read(struct gbcc_core *gbc, uint16_t addr)
{
	if (addr < ROMX_START) {
		return gbc->memory.rom0[addr];
	}
	if (addr >= ROMX_START && addr < ROMX_END) {
		return gbc->memory.romx[addr - ROMX_START];
	}
	if (addr >= SRAM_START && addr < SRAM_END) {
		if (gbc->cart.ram_size == 0) {
			gbcc_log_debug("Trying to read SRAM when there isn't any!\n");
			return 0xFFu;
		}
		return gbc->memory.sram[addr - SRAM_START];
	}
	gbcc_log_error("Reading memory address 0x%04X out of bounds.\n", addr);
	return 0xFFu;
}

void gbcc_mbc_none_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	if (gbc->cart.ram_size == 0) {
		gbcc_log_debug("Trying to write to SRAM when there isn't any!\n");
	}
	else if (addr >= SRAM_START && addr < SRAM_END && addr - SRAM_START < gbc->cart.ram_size) {
		gbc->memory.sram[addr - SRAM_START] = val;
	} else {
		gbcc_log_error("Writing memory address 0x%04X out of bounds.\n", addr);
	}
}

uint8_t gbcc_mbc_mbc1_read(struct gbcc_core *gbc, uint16_t addr)
{
	if (addr < ROMX_START) {
		return gbc->memory.rom0[addr];
	}
	if (addr >= ROMX_START && addr < ROMX_END) {
		return gbc->memory.romx[addr - ROMX_START];
	}
	if (addr >= SRAM_START && addr < SRAM_END) {
		if (gbc->cart.ram_size == 0) {
			gbcc_log_debug("Trying to read SRAM when there isn't any!\n");
			return 0xFFu;
		}
		if (gbc->cart.mbc.sram_enable) {
			return gbc->memory.sram[addr - SRAM_START];
		}
		gbcc_log_debug("SRAM not enabled!\n");
		return 0xFFu;
	}
	gbcc_log_error("Reading memory address 0x%04X out of bounds.\n", addr);
	return 0xFFu;
}

void gbcc_mbc_mbc1_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	struct gbcc_mbc *mbc = &gbc->cart.mbc;

	if (addr >= SRAM_START && addr < SRAM_END) {
		if (gbc->cart.ram_size == 0) {
			gbcc_log_debug("Trying to write to SRAM when there isn't any!\n");
		} else if (mbc->sram_enable) {
			gbc->memory.sram[addr - SRAM_START] = val;
		} else {
			gbcc_log_debug("SRAM not enabled!\n");
		}
	} else if (addr < 0x2000u) {
		mbc->ramg = val & 0x0Fu;
	} else if (addr < 0x4000u) {
		mbc->romb0 = val & 0x1Fu;
	} else if (addr < 0x6000u) {
		mbc->romb1 = val & 0x03u;
	} else if (addr < 0x8000u) {
		mbc->ramb = val & 0x01u;
	} else {
		gbcc_log_error("Writing memory address %04X out of bounds.\n", addr);
		return;
	}

	/* RAMG switches on SRAM when it has 0x0A in the lower 4 bits. */
	mbc->sram_enable = (mbc->ramg == 0x0Au);

	/*
	 * ROMB0 selects lower 5 bits of ROMX bank number.
	 * 0 maps to 1, so banks 0x00, 0x20, 0x40 & 0x60 can
	 * never be mapped to ROMX.
	 */
	mbc->romx_bank = mbc->romb0;
	mbc->romx_bank += !mbc->romx_bank;

	/*
	 * ROMB1 selects either upper 2 bits of ROMX bank number
	 * or RAM bank number, depending on bit 0 of RAMB.
	 */
	if (!mbc->ramb) {
		/* Mode 0 (ROM banking mode) */
		mbc->romx_bank |= mbc->romb1 << 5u;
		mbc->sram_bank = 0;
		mbc->rom0_bank = 0;
	} else {
		/* Mode 1 (RAM banking mode) */
		mbc->sram_bank = mbc->romb1;
		/*
		 * Some weirdness here; in RAM banking mode, as well as
		 * controlling the RAM bank, ROMB1 moves ROM0 around!
		 */
		mbc->rom0_bank = (uint16_t)(mbc->romb1 << 5u);
		mbc->romx_bank |= mbc->romb1 << 5u;
	}

	set_mbc_banks(gbc);
}

uint8_t gbcc_mbc_mbc2_read(struct gbcc_core *gbc, uint16_t addr)
{
	if (addr < ROMX_START) {
		return gbc->memory.rom0[addr];
	}
	if (addr >= ROMX_START && addr < ROMX_END) {
		return gbc->memory.romx[addr - ROMX_START];
	}
	/*
	 * MBC2 only has 512 4-bit ram locations, however this is mirrored
	 * throughout the full SRAM address space.
	 */
	if (addr >= SRAM_START && addr < SRAM_END) {
		if (gbc->cart.mbc.sram_enable) {
			return gbc->memory.sram[(addr - SRAM_START) & 0x1FFu];
		}
		gbcc_log_debug("SRAM not enabled!\n");
		return 0xFFu;
	}
	gbcc_log_error("Reading memory address 0x%04X out of bounds.\n", addr);
	return 0xFFu;
}

void gbcc_mbc_mbc2_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	struct gbcc_mbc *mbc = &gbc->cart.mbc;

	/* MBC2 only has 512 4-bit ram locations */
	if (addr >= SRAM_START && addr < SRAM_START + 0x200u) {
		if (mbc->sram_enable) {
			gbc->memory.sram[addr - SRAM_START] = (val & 0x0Fu) | 0xF0u;
		} else {
			gbcc_log_debug("SRAM not enabled!\n");
		}
	} else if (addr < 0x4000u) {
		if (addr & bit16(8)) {
			/* bit 8 of address must be set to change rom bank */
			mbc->romb0 = val;
		} else {
			/* bit 8 of address must be cleared to enable/disable ram */
			mbc->ramg = val;
		}
	} else {
		gbcc_log_error("Writing memory address %04X out of bounds.\n", addr);
		return;
	}

	/* RAMG switches on SRAM when it has 0x0A in the lower 4 bits. */
	mbc->sram_enable = ((mbc->ramg & 0x0Fu) == 0x0Au);

	/*
	 * ROMB0 selects 4 bit ROMX bank number.
	 * 0 maps to 1, so banks 0x00, 0x20, 0x40 & 0x60 can
	 * never be mapped to ROMX.
	 * TODO: is this 0->1 note true for MBC2?
	 */
	mbc->romx_bank = mbc->romb0 & 0x0Fu;
	mbc->romx_bank += !mbc->romx_bank;

	set_mbc_banks(gbc);
}

uint8_t gbcc_mbc_mbc3_read(struct gbcc_core *gbc, uint16_t addr)
{
	struct gbcc_rtc *rtc = &gbc->cart.mbc.rtc;
	if (addr < ROM0_END) {
		return gbc->memory.rom0[addr];
	}
	if (addr >= ROMX_START && addr < ROMX_END) {
		return gbc->memory.romx[addr - ROMX_START];
	}
	if (addr >= SRAM_START && addr < SRAM_END) {
		if (!gbc->cart.mbc.sram_enable) {
			gbcc_log_debug("SRAM/RTC not enabled!\n");
		}
		if (rtc->mapped) {
			switch (rtc->cur_reg) {
				case 0:
					return rtc->seconds;
				case 1:
					return rtc->minutes;
				case 2:
					return rtc->hours;
				case 3:
					return rtc->day_low;
				case 4:
					return rtc->day_high;
				default:
					gbcc_log_error("Invalid rtc reg %u\n", rtc->cur_reg);
					return 0;
			}
		}
		if (gbc->cart.ram_size == 0) {
			gbcc_log_debug("Trying to read SRAM when there isn't any!\n");
			return 0xFFu;
		}
		return gbc->memory.sram[addr - SRAM_START];
	}
	gbcc_log_error("Reading memory address 0x%04X out of bounds.\n", addr);
	return 0xFFu;
}

void gbcc_mbc_mbc3_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	/* NB: For MBC3, sram_enable controls BOTH SRAM and RTC registers */
	/* TODO: Handle halt flag */
	struct gbcc_mbc *mbc = &gbc->cart.mbc;
	struct gbcc_rtc *rtc = &mbc->rtc;
	if (addr >= SRAM_START && addr < SRAM_END) {
		if (!mbc->sram_enable) {
			gbcc_log_debug("SRAM/RTC not enabled!\n");
			return;
		} else if (rtc->mapped) {
			switch (rtc->cur_reg) {
				case 0:
					rtc->base_time.tv_sec += rtc->seconds;
					rtc->base_time.tv_sec -= val;
					break;
				case 1:
					rtc->base_time.tv_sec += rtc->minutes * (MINUTE / SECOND);
					rtc->base_time.tv_sec -= val * (MINUTE / SECOND);
					break;
				case 2:
					rtc->base_time.tv_sec += rtc->hours * (HOUR / SECOND);
					rtc->base_time.tv_sec -= val * (HOUR / SECOND);
					break;
				case 3:
					rtc->base_time.tv_sec += rtc->day_low * (DAY / SECOND);
					rtc->base_time.tv_sec -= val * (DAY / SECOND);
					break;
				case 4:
					rtc->base_time.tv_sec += check_bit(rtc->day_high, 0) * 256 * (DAY / SECOND);
					rtc->base_time.tv_sec -= check_bit(val, 0) * 256 * (DAY / SECOND);
					rtc->halt = check_bit(val, 6);
					break;
				default:
					gbcc_log_error("Invalid rtc reg %u\n", rtc->cur_reg);
			}
		} else if (gbc->cart.ram_size == 0) {
			gbcc_log_debug("Trying to write to SRAM when there isn't any!\n");
		} else {
			gbc->memory.sram[addr - SRAM_START] = val;
		}
	} else if (addr < 0x2000u) {
		mbc->ramg = val;
	} else if (addr < 0x4000u) {
		mbc->romb0 = val;
	} else if (addr < 0x6000u) {
		mbc->ramb = val;
	} else if (addr < 0x8000u) {
		if (rtc->latch != 0  || val != 0x01u) {
			rtc->latch = val;
			return;
		}
		rtc->latch = val;
		uint64_t diff;
		struct timespec cur_time;
		clock_gettime(CLOCK_REALTIME, &cur_time);
		diff = gbcc_time_diff(&cur_time, &rtc->base_time);
		rtc->seconds = (diff / SECOND) % (MINUTE / SECOND);
		rtc->minutes = (diff / MINUTE) % (HOUR / MINUTE);
		rtc->hours = (diff / HOUR) % (DAY / HOUR);
		rtc->day_low = (diff / DAY) & 0xFFu;
		rtc->day_high &= (uint8_t)(~bit(0));
		rtc->day_high |= ((diff / DAY) >> 8u) & bit(0);
		if (diff / DAY > 0x100u) {
			rtc->day_high |= bit(7);
		}
	} else {
		gbcc_log_error("Writing memory address %04X out of bounds.\n", addr);
	}

	/* RAMG switches on SRAM when it has 0x0A in the lower 4 bits. */
	mbc->sram_enable = ((mbc->ramg & 0x0Fu) == 0x0Au);

	/* ROMB0 selects 7 bit ROMX bank number. 0 maps to 1. */
	mbc->romx_bank = mbc->romb0 & 0x7Fu;
	mbc->romx_bank += !mbc->romx_bank;

	/* RAMB selects RAM bank number or RTC register, depending on specific ranges */
	if (mbc->ramb >= 0x08u && mbc->ramb <= 0x0Cu) {
		rtc->mapped = true;
		rtc->cur_reg = mbc->ramb - 0x08u;
	} else if (mbc->ramb < 0x04u) {
		rtc->mapped = false;
		mbc->sram_bank = mbc->ramb;
	}

	set_mbc_banks(gbc);
}

uint8_t gbcc_mbc_mbc5_read(struct gbcc_core *gbc, uint16_t addr)
{
	if (addr < ROMX_START) {
		return gbc->memory.rom0[addr];
	}
	if (addr >= ROMX_START && addr < ROMX_END) {
		return gbc->memory.romx[addr - ROMX_START];
	}
	if (addr >= SRAM_START && addr < SRAM_END) {
		if (gbc->cart.ram_size == 0) {
			gbcc_log_debug("Trying to read SRAM when there isn't any!\n");
			return 0xFFu;
		}
		if (gbc->cart.mbc.sram_enable) {
			return gbc->memory.sram[addr - SRAM_START];
		}
		gbcc_log_debug("SRAM not enabled!\n");
	}
	gbcc_log_error("Reading memory address 0x%04X out of bounds.\n", addr);
	return 0xFFu;
}

void gbcc_mbc_mbc5_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	struct gbcc_mbc *mbc = &gbc->cart.mbc;

	if (addr >= SRAM_START && addr < SRAM_END) {
		if (gbc->cart.ram_size == 0) {
			gbcc_log_debug("Trying to write to SRAM when there isn't any!\n");
		} else if (mbc->sram_enable) {
			gbc->memory.sram[addr - SRAM_START] = val;
		} else {
			gbcc_log_debug("SRAM not enabled!\n");
		}
	} else if (addr < 0x2000u) {
		mbc->ramg = val;
	} else if (addr < 0x3000u) {
		mbc->romb0 = val;
	} else if (addr < 0x4000u) {
		mbc->romb1 = val;
	} else if (addr < 0x6000u) {
		mbc->ramb = val;
	} else {
		gbcc_log_error("Writing memory address %04X out of bounds.\n", addr);
	}

	/* RAMG switches on SRAM when it has 0x0A in the lower 4 bits. */
	mbc->sram_enable = ((mbc->ramg & 0x0Fu) == 0x0Au);

	/* ROMB0 selects lower 8 bits of ROMX bank number. */
	mbc->romx_bank = mbc->romb0;

	/* ROMB1 selects upper bit of ROMX bank number */
	mbc->romx_bank |= (uint16_t)(check_bit(mbc->romb1, 0) << 8u);

	/* Lower 4 bits of RAMB select SRAM bank number */
	mbc->sram_bank = mbc->ramb & 0x0Fu;

	/*
	 * With the rumble pak, bit 3 of RAMB is repurposed to control the
	 * rumble. From how the rumble strength works, it seems that the
	 * cartridge has direct control over the rumble motor, and this bit
	 * controls the position of the motor.
	 */
	/* N.B.: this is not confirmed, but it works */
	if (gbc->cart.rumble) {
		mbc->sram_bank = mbc->ramb & 0x07u;
		gbc->cart.rumble_state = (mbc->ramb & 0x08u) >> 3u;
	}

	set_mbc_banks(gbc);
}

uint8_t gbcc_mbc_mbc6_read(struct gbcc_core *gbc, uint16_t addr)
{
	(void)gbc;
	(void)addr;
	gbcc_log_debug("Stubbed function gbcc_mbc_mbc6_read() called\n");
	return 0xFFu;
}

void gbcc_mbc_mbc6_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	(void)gbc;
	(void)addr;
	(void)val;
	gbcc_log_debug("Stubbed function gbcc_mbc_mbc6_write() called\n");
}

uint8_t gbcc_mbc_mbc7_read(struct gbcc_core *gbc, uint16_t addr)
{
	struct gbcc_mbc *mbc = &gbc->cart.mbc;
	if (addr < ROM0_END) {
		return gbc->memory.rom0[addr];
	}
	if (addr >= ROMX_START && addr < ROMX_END) {
		return gbc->memory.romx[addr - ROMX_START];
	}
	if (addr >= 0xA000u && addr < 0xB000u) {
		switch ((addr & 0x00F0u) >> 4u) {
			case 0:
				return 0xFFu;
			case 1:
				return 0xFFu;
			case 2:
				return mbc->accelerometer.x & 0xFFu;
			case 3:
				return mbc->accelerometer.x >> 8u;
			case 4:
				return mbc->accelerometer.y & 0xFFu;
			case 5:
				return mbc->accelerometer.y >> 8u;
			case 6:
				return 0x00u;
			case 7:
				return 0xFFu;
			case 8:
				{
					uint8_t ret = 0;
					ret = cond_bit(ret, 0, mbc->eeprom.DO);
					ret = cond_bit(ret, 1, mbc->eeprom.DI);
					ret = cond_bit(ret, 6, mbc->eeprom.CLK);
					ret = cond_bit(ret, 7, mbc->eeprom.CS);
					return ret;
				}
			default:
				return 0xFFu;
		}
	}
	gbcc_log_error("Reading memory address 0x%04X out of bounds.\n", addr);
	return 0xFFu;
}

void gbcc_mbc_mbc7_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	struct gbcc_mbc *mbc = &gbc->cart.mbc;

	if (addr >= 0xA000u && addr < 0xB000u) {
		switch ((addr & 0x00F0u) >> 4u) {
			case 0:
				if (val == 0x55u) {
					mbc->accelerometer.x = 0x8000u;
					mbc->accelerometer.y = 0x8000u;
					mbc->accelerometer.latch = false;
				}
				break;
			case 1:
				if (val == 0xAAu && !mbc->accelerometer.latch) {
					mbc->accelerometer.x = mbc->accelerometer.real_x;
					mbc->accelerometer.y = mbc->accelerometer.real_y;
					mbc->accelerometer.latch = true;
				}
				break;
			case 8:
				eeprom_write(gbc, val);
				break;
		}
	} else if (addr < 0x2000u) {
		mbc->ramg = val;
	} else if (addr < 0x4000u) {
		mbc->romb0 = val;
	} else if (addr < 0x6000u) {
		mbc->ramb = val;
	} else {
		gbcc_log_error("Writing memory address %04X out of bounds.\n", addr);
	}

	/* RAMG switches on SRAM when it has 0x0A in the lower 4 bits. */
	mbc->sram_enable = ((mbc->ramg & 0x0Fu) == 0x0Au);

	/* ROMB0 selects lower 8 bits of ROMX bank number. */
	mbc->romx_bank = mbc->romb0;

	/* ROMB1 selects upper bit of ROMX bank number */
	mbc->romx_bank |= (uint16_t)(check_bit(mbc->romb1, 0) << 8u);

	/* Lower 4 bits of RAMB select SRAM bank number */
	mbc->sram_bank = mbc->ramb & 0x0Fu;

	set_mbc_banks(gbc);
}

uint8_t gbcc_mbc_huc1_read(struct gbcc_core *gbc, uint16_t addr)
{
	if (addr < ROM0_END) {
		return gbc->memory.rom0[addr];
	}
	if (addr >= ROMX_START && addr < ROMX_END) {
		return gbc->memory.romx[addr - ROMX_START];
	}
	if (addr >= SRAM_START && addr < SRAM_END) {
		if (gbc->cart.mbc.sram_enable) {
			if (gbc->cart.ram_size == 0) {
				gbcc_log_debug("Trying to read SRAM when there isn't any!\n");
				return 0xFFu;
			}
			return gbc->memory.sram[addr - SRAM_START];
		}
		/* TODO: Implement IR receiver */
		return 0xC0u;
	}
	gbcc_log_error("Reading memory address 0x%04X out of bounds.\n", addr);
	return 0xFFu;
}

void gbcc_mbc_huc1_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	struct gbcc_mbc *mbc = &gbc->cart.mbc;

	if (addr >= SRAM_START && addr < SRAM_END) {
		if (mbc->sram_enable) {
			if (gbc->cart.ram_size == 0) {
				gbcc_log_debug("Trying to write to SRAM when there isn't any!\n");
				return;
			}
			gbc->memory.sram[addr - SRAM_START] = val;
		} else {
			/* TODO: Implement IR transmitter */
			return;
		}
	} else if (addr < 0x2000u) {
		mbc->ramg = val;
	} else if (addr < 0x4000u) {
		mbc->romb0 = val;
	} else if (addr < 0x6000u) {
		mbc->ramb = val;
	} else if (addr < 0x8000u) {
		/* Do nothing */
	} else {
		gbcc_log_error("Writing memory address %04X out of bounds.\n", addr);
	}

	/* RAMG toggles between SRAM and IR when it equals 0x0E */
	mbc->sram_enable = !((mbc->ramg & 0x0Fu) == 0x0Eu);

	/* ROMB0 selects ROMX bank number. */
	mbc->romx_bank = mbc->romb0;

	/* Lower 2 bits of RAMB select SRAM bank number */
	mbc->sram_bank = mbc->ramb & 0x03u;

	set_mbc_banks(gbc);
}

uint8_t gbcc_mbc_huc3_read(struct gbcc_core *gbc, uint16_t addr)
{
	struct gbcc_rtc *rtc = &gbc->cart.mbc.rtc;
	if (addr < ROM0_END) {
		return gbc->memory.rom0[addr];
	}
	if (addr >= ROMX_START && addr < ROMX_END) {
		return gbc->memory.romx[addr - ROMX_START];
	}
	if (addr >= SRAM_START && addr < SRAM_END) {
		if (!gbc->cart.mbc.sram_enable) {
			/* TODO: Implement IR receiver */
			/* Apparently, the few HuC3 games in existence won't
			 * start unless bit 0 of this is 1 at some point */
			return 0xC1u;
		}
		if (rtc->mapped) {
			switch (rtc->cur_reg) {
				case 0:
					return rtc->seconds;
				case 1:
					return rtc->minutes;
				case 2:
					return rtc->hours;
				case 3:
					return rtc->day_low;
				case 4:
					return rtc->day_high;
				default:
					gbcc_log_error("Invalid rtc reg %u\n", rtc->cur_reg);
					return 0;
			}
		}
		if (gbc->cart.ram_size == 0) {
			gbcc_log_debug("Trying to read SRAM when there isn't any!\n");
			return 0xFFu;
		}
		return gbc->memory.sram[addr - SRAM_START];
	}
	gbcc_log_error("Reading memory address 0x%04X out of bounds.\n", addr);
	return 0xFFu;
}

void gbcc_mbc_huc3_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	/* NB: For HuC3, sram_enable controls BOTH SRAM and RTC registers */
	/* TODO: Handle halt flag */
	struct gbcc_mbc *mbc = &gbc->cart.mbc;
	struct gbcc_rtc *rtc = &mbc->rtc;
	if (addr >= SRAM_START && addr < SRAM_END) {
		if (!mbc->sram_enable) {
			/* TODO: Implement IR transmitter */
			return;
		} else if (rtc->mapped) {
			switch (rtc->cur_reg) {
				case 0:
					rtc->base_time.tv_sec += rtc->seconds;
					rtc->base_time.tv_sec -= val;
					break;
				case 1:
					rtc->base_time.tv_sec += rtc->minutes * (MINUTE / SECOND);
					rtc->base_time.tv_sec -= val * (MINUTE / SECOND);
					break;
				case 2:
					rtc->base_time.tv_sec += rtc->hours * (HOUR / SECOND);
					rtc->base_time.tv_sec -= val * (HOUR / SECOND);
					break;
				case 3:
					rtc->base_time.tv_sec += rtc->day_low * (DAY / SECOND);
					rtc->base_time.tv_sec -= val * (DAY / SECOND);
					break;
				case 4:
					rtc->base_time.tv_sec += check_bit(rtc->day_high, 0) * 256 * (DAY / SECOND);
					rtc->base_time.tv_sec -= check_bit(val, 0) * 256 * (DAY / SECOND);
					rtc->halt = check_bit(val, 6);
					break;
				default:
					gbcc_log_error("Invalid rtc reg %u\n", rtc->cur_reg);
			}
		} else if (gbc->cart.ram_size == 0) {
			gbcc_log_debug("Trying to write to SRAM when there isn't any!\n");
		} else {
			gbc->memory.sram[addr - SRAM_START] = val;
		}
	} else if (addr < 0x2000u) {
		mbc->ramg = val;
	} else if (addr < 0x4000u) {
		mbc->romb0 = val;
	} else if (addr < 0x6000u) {
		mbc->ramb = val;
	} else if (addr < 0x8000u) {
		if (rtc->latch != 0  || val != 0x01u) {
			rtc->latch = val;
			return;
		}
		rtc->latch = val;
		uint64_t diff;
		struct timespec cur_time;
		clock_gettime(CLOCK_REALTIME, &cur_time);
		diff = gbcc_time_diff(&cur_time, &rtc->base_time);
		rtc->seconds = (diff / SECOND) % (MINUTE / SECOND);
		rtc->minutes = (diff / MINUTE) % (HOUR / MINUTE);
		rtc->hours = (diff / HOUR) % (DAY / HOUR);
		rtc->day_low = (diff / DAY) & 0xFFu;
		rtc->day_high &= (uint8_t)(~bit(0));
		rtc->day_high |= ((diff / DAY) >> 8u) & bit(0);
		if (diff / DAY > 0x100u) {
			rtc->day_high |= bit(7);
		}
	} else {
		gbcc_log_error("Writing memory address %04X out of bounds.\n", addr);
	}

	/* RAMG switches on SRAM when it has 0x0A in the lower 4 bits. */
	mbc->sram_enable = ((mbc->ramg & 0x0Fu) == 0x0Au);

	/* ROMB0 selects 7 bit ROMX bank number. 0 maps to 1. */
	mbc->romx_bank = mbc->romb0 & 0x7Fu;
	mbc->romx_bank += !mbc->romx_bank;

	/* RAMB selects RAM bank number or RTC register, depending on specific ranges */
	if (mbc->ramb >= 0x08u && mbc->ramb <= 0x0Cu) {
		rtc->mapped = true;
		rtc->cur_reg = mbc->ramb - 0x08u;
	} else if (mbc->ramb < 0x04u) {
		rtc->mapped = false;
		mbc->sram_bank = mbc->ramb;
	}

	set_mbc_banks(gbc);
}

uint8_t gbcc_mbc_mmm01_read(struct gbcc_core *gbc, uint16_t addr)
{
	return gbcc_mbc_mbc1_read(gbc, addr);
}

void gbcc_mbc_mmm01_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	struct gbcc_mbc *mbc = &gbc->cart.mbc;

	if (addr >= SRAM_START && addr < SRAM_END) {
		if (gbc->cart.ram_size == 0) {
			gbcc_log_debug("Trying to write to SRAM when there isn't any!\n");
		} else if (mbc->sram_enable) {
			gbc->memory.sram[addr - SRAM_START] = val;
		} else {
			gbcc_log_debug("SRAM not enabled!\n");
		}
	} else if (addr < 0x2000u) {
		/*
		 * Bits 0-3: RAM enable
		 * Bits 4-5: Write mask for bits 0-1 in 0x4000 - 0x5FFF block
		 * Bit 6: Map enable
		 */
		uint8_t mask = 0x00u;
		if (mbc->unlocked) {
			mask |= 0xF0u;
		}
		mbc->ramg = (mbc->ramg & mask) | (val & ~mask);

		mbc->sram_enable = ((mbc->ramg & 0x0Fu) == 0x0Au);

		/* Map enable cannot be unset without a reboot */
		mbc->unlocked |= check_bit(mbc->ramg, 6);
	} else if (addr < 0x4000u) {
		/*
		 * Bits 0-4: ROM bank bits 0-4
		 * Bits 5-6: ROM bank bits 5-6 OR RAM bank bits 0-1
		 */
		uint8_t mask = (mbc->ramb & 0x3Cu) >> 1u;
		if (mbc->unlocked) {
			mask |= 0xE0u;
		}
		mbc->romb0 = (mbc->romb0 & mask) | (val & ~mask);
	} else if (addr < 0x6000u) {
		/*
		 * Bits 0-1: RAM bank bits 0-1 OR ROM bank bits 5-6
		 * Bits 2-3: RAM bank bits 2-3
		 * Bits 4-5: ROM bank bits 7-8
		 * Bit 6: Write mask for bit 0 in 0x6000-0x7FFF block
		 */
		uint8_t mask = (mbc->ramg & 0x30u) >> 4u;
		if (mbc->unlocked) {
			mask |= 0xFCu;
		}
		mbc->romb1 = (mbc->romb1 & mask) | (val & ~mask);
	} else if (addr < 0x8000u) {
		/*
		 * Bit 0: MBC1 banking mode
		 * Bit 1: Unknown
		 * Bits 2-5: Write mask for bits 1-4 in 0x2000 - 0x3FFF block
		 * Bit 6: Multiplexer, switches 0x2000 bits 5-6 with 0x6000 bits 0-1
		 */
		uint8_t mask = (mbc->romb1 & 0x40u) >> 6u;
		if (mbc->unlocked) {
			mask |= 0xFEu;
		}
		mbc->ramb = (mbc->ramb & mask) | (val & ~mask);
		mbc->ramb = val;
	}

	if (mbc->unlocked) {
		if (check_bit(mbc->ramb, 6) == check_bit(mbc->ramb, 0)) {
			mbc->romx_bank = mbc->romb0 & 0x1Fu;

			mbc->romx_bank |= (mbc->romb1 & 0x03u) << 5u;

			mbc->sram_bank = (mbc->romb0 >> 5u) & 0x03u;
			mbc->sram_bank |= mbc->romb1 & 0x0Cu;
		} else {
			mbc->romx_bank = mbc->romb0 & 0x7Fu;
			mbc->sram_bank = mbc->romb1 & 0x0Fu;
		}
		mbc->romx_bank |= (mbc->romb1 & 0x30u) << 3u;

		/* Bits 0-4 are zero-adjusted based on unmasked bits */
		mbc->romx_bank += !((mbc->romb0 & 0x1Fu) & ~((mbc->ramb & 0x3Cu) >> 1u));
	} else {
		if (check_bit(mbc->ramb, 6)) {
			mbc->rom0_bank = mbc->romb0 & 0x1Fu;
			mbc->rom0_bank |= (mbc->romb1 & 0x03u) << 5u;

			mbc->sram_bank = (mbc->romb0 >> 5u) & 0x03u;
			mbc->sram_bank |= mbc->romb1 & 0x0Cu;
		} else {
			mbc->rom0_bank = mbc->romb0 & 0x7Fu;
			mbc->sram_bank = mbc->romb1 & 0x0Fu;
		}
		mbc->rom0_bank |= (mbc->romb1 & 0x30u) << 3u;
	}

	if (mbc->unlocked) {
		set_mbc_banks(gbc);
	}
}

uint8_t gbcc_mbc_cam_read(struct gbcc_core *gbc, uint16_t addr)
{
	struct gbcc_mbc *mbc = &gbc->cart.mbc;
	struct gbcc_camera *cam = &mbc->camera;
	if (addr < ROM0_END) {
		return gbc->memory.rom0[addr];
	}
	if (addr >= ROMX_START && addr < ROMX_END) {
		return gbc->memory.romx[addr - ROMX_START];
	}
	if (addr >= SRAM_START && addr < SRAM_END) {
		/*
		 * Camera MBC always allows reading
		 * from SRAM or camera registers
		 * (although only 0xA000 returns non-zero)
		 */
		if (cam->mapped) {
			if (addr == 0xA000u) {
				return (cam->capture_timer > 0) | (uint8_t)(cam->filter_mode << 1u);
			}
			return 0x00u;
		}
		/* Can't read SRAM while capturing an image */
		if (cam->capture_timer > 0) {
			return 0x00u;
		}
		return gbc->memory.sram[addr - SRAM_START];
	}
	gbcc_log_error("Reading memory address 0x%04X out of bounds.\n", addr);
	return 0xFFu;
}

void gbcc_mbc_cam_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	struct gbcc_mbc *mbc = &gbc->cart.mbc;
	struct gbcc_camera *cam = &mbc->camera;
	if (addr >= SRAM_START && addr < SRAM_END) {
		if (!cam->mapped && !mbc->sram_enable) {
			gbcc_log_debug("SRAM not enabled!\n");
			return;
		} else if (cam->mapped) {
			if (addr == 0xA000u) {
				cam->filter_mode = (val >> 1) & 0x03u;
				switch (cam->filter_mode) {
					case 0:
						cam->reg4 = 0x00u;
						cam->reg5 = 0x01u;
						break;
					case 1:
						cam->reg4 = 0x01u;
						cam->reg5 = 0x00u;
						break;
					/*
					 * Cases 2 & 3 are deliberately
					 * identical
					 */
					case 2:
						cam->reg4 = 0x01u;
						cam->reg5 = 0x02u;
						break;
					case 3:
						cam->reg4 = 0x01u;
						cam->reg5 = 0x02u;
						break;
				}
				/* reg6 = reg.x is always 1 */
				cam->reg6 = 0x01u;
				if (check_bit(val, 0)) {
					cam->capture_request = true;
				}
			} else if (addr == 0xA001u) {
				cam->reg1 = val;
			} else if (addr == 0xA002u) {
				cam->reg2 = val;
			} else if (addr == 0xA003u) {
				cam->reg3 = val;
			} else if (addr == 0xA004u) {
				cam->reg7 = val;
			} else if (addr == 0xA005u) {
				cam->reg0 = val;
			} else if (addr < 0xA036u) {
				uint8_t idx = (uint8_t)(addr - 0xA006u);
				uint8_t n = idx % 3;
				uint8_t x = (idx / 3) % 4;
				uint8_t y = idx / (3 * 4);
				cam->matrix[y][x][n] = val;
			} else {
				gbcc_log_debug("Writing 0x%02X to invalid camera register 0x%04X\n", val, addr);
			}
		} else {
			gbc->memory.sram[addr - SRAM_START] = val;
		}
	} else if (addr < 0x2000u) {
		mbc->ramg = val;
	} else if (addr < 0x4000u) {
		mbc->romb0 = val;
	} else if (addr < 0x6000u) {
		cam->mapped = check_bit(val, 4);
		if (!cam->mapped) {
			mbc->ramb = val;
		}
	} else {
		gbcc_log_error("Writing memory address %04X out of bounds.\n", addr);
	}

	/* RAMG switches on SRAM when it has 0x0A in the lower 4 bits. */
	mbc->sram_enable = ((mbc->ramg & 0x0Fu) == 0x0Au);

	/* ROMB0 selects 7 bit ROMX bank number. 0 maps to 1. */
	mbc->romx_bank = mbc->romb0 & 0x7Fu;
	mbc->romx_bank += !mbc->romx_bank;

	/* RAMB selects RAM bank number */
	mbc->sram_bank = mbc->ramb;

	set_mbc_banks(gbc);
}

void eeprom_write(struct gbcc_core *gbc, uint8_t val)
{
	struct gbcc_eeprom *eeprom = &gbc->cart.mbc.eeprom;
	eeprom->last_DI = eeprom->DI;
	eeprom->last_CLK = eeprom->CLK;
	eeprom->last_CS = eeprom->CS;
	eeprom->DI = check_bit(val, 1);
	eeprom->CLK = check_bit(val, 6);
	eeprom->CS = check_bit(val, 7);
	if (eeprom->current_command == GBCC_EEPROM_NONE) {
		eeprom->DO = true;
	}

	if (!eeprom->CS) {
		eeprom_reset(eeprom);
		return;
	}

	/* 
	 * Start bit is detected when both CS & DI are held high, and CLK has a
	 * rising edge.
	 */
	if (!eeprom->start
			&& (eeprom->CS && eeprom->last_CS)
			&& (eeprom->DI && eeprom->last_DI)
			&& (eeprom->CLK && !eeprom->last_CLK)) {
		eeprom->start = true;
		return;
	}

	if (!eeprom->start) {
		return;
	}
	if (!(eeprom->CLK && !eeprom->last_CLK)) {
		return;
	}

	/* First check if we're currently running a command */
	switch (eeprom->current_command) {
		case GBCC_EEPROM_NONE:
			break;
		case GBCC_EEPROM_READ:
			eeprom->DO = check_bit16(eeprom->data[eeprom->address], 15 - eeprom->value_bit);
			eeprom->value <<= 1;
			eeprom->value |= eeprom->DO;
			eeprom->value_bit++;
			if (eeprom->value_bit == 16) {
				eeprom->value_bit = 0;
				eeprom->address++;
			}
			return;
		case GBCC_EEPROM_WRITE:
			eeprom->value <<= 1;
			eeprom->value |= eeprom->DI;
			eeprom->value_bit++;
			if (eeprom->value_bit == 16) {
				eeprom->data[eeprom->address] = eeprom->value;
				eeprom_reset(eeprom);
			}
			return;
		case GBCC_EEPROM_WRAL:
			eeprom->value <<= 1;
			eeprom->value |= eeprom->DI;
			eeprom->value_bit++;
			if (eeprom->value_bit == 16) {
				for (int i = 0; i < 128; i++) {
					eeprom->data[i] = eeprom->value;
				}
				eeprom_reset(eeprom);
			}
			return;
		default:
			gbcc_log_error("Impossible eeprom command.\n");
			return;
	}

	/* Actual reading of command */
	eeprom->command <<= 1;
	eeprom->command |= eeprom->DI;
	eeprom->command_bit++;

	if (eeprom->command_bit == 10) {
		if ((eeprom->command & 0x300u) == 0x200u) {
			/* READ */
			eeprom->address = eeprom->command & 0x7Fu;
			eeprom->current_command = GBCC_EEPROM_READ;
		} else if ((eeprom->command & 0x3C0u) == 0x0C0u) {
			/* EWEN */
			eeprom->write_enable = true;
			eeprom_reset(eeprom);
		} else if ((eeprom->command & 0x3C0u) == 0x000u) {
			/* EWDS */
			eeprom->write_enable = false;
			eeprom_reset(eeprom);
		} else if ((eeprom->command & 0x300u) == 0x100u) {
			/* WRITE */
			eeprom->address = eeprom->command & 0x7Fu;
			eeprom->current_command = GBCC_EEPROM_WRITE;
		} else if ((eeprom->command & 0x300u) == 0x300u) {
			/* ERASE */
			eeprom->data[eeprom->command & 0x7Fu] = 0xFFFFu;
			eeprom_reset(eeprom);
		} else if ((eeprom->command & 0x3C0u) == 0x080u) {
			/* ERAL */
			memset(eeprom->data, 0xFFu, 256);
			eeprom_reset(eeprom);
		} else if ((eeprom->command & 0x3C0u) == 0x040u) {
			/* WRAL */
			eeprom->current_command = GBCC_EEPROM_WRAL;
		} else {
			gbcc_log_error("Unknown EEPROM command\n");
			eeprom_reset(eeprom);
		}
	}
}

void eeprom_reset(struct gbcc_eeprom *eeprom)
{
	eeprom->start = false;
	eeprom->command = 0;
	eeprom->command_bit = 0;
	eeprom->current_command = GBCC_EEPROM_NONE;
	eeprom->value = 0;
	eeprom->value_bit = 0;
	eeprom->address = 0;
	eeprom->DO = true;
}
