/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "gbcc.h"
#include "input.h"
#include "memory.h"
#include "nelem.h"

void gbcc_input_process_key(struct gbcc *gbc, enum gbcc_key key, bool pressed)
{
	if (gbc->menu.show) {
		if (pressed) {
			gbcc_menu_process_key(gbc, key);
		}
		return;
	}
	switch(key) {
		case GBCC_KEY_A:
			gbc->core.keys.a = pressed;
			gbc->core.keys.interrupt = true;
			break;
		case GBCC_KEY_B:
			gbc->core.keys.b = pressed;
			gbc->core.keys.interrupt = true;
			break;
		case GBCC_KEY_START:
			gbc->core.keys.start = pressed;
			gbc->core.keys.interrupt = true;
			break;
		case GBCC_KEY_SELECT:
			gbc->core.keys.select = pressed;
			gbc->core.keys.interrupt = true;
			break;
		case GBCC_KEY_UP:
			gbc->core.keys.dpad.up = pressed;
			gbc->core.keys.interrupt = true;
			break;
		case GBCC_KEY_DOWN:
			gbc->core.keys.dpad.down = pressed;
			gbc->core.keys.interrupt = true;
			break;
		case GBCC_KEY_LEFT:
			gbc->core.keys.dpad.left = pressed;
			gbc->core.keys.interrupt = true;
			break;
		case GBCC_KEY_RIGHT:
			gbc->core.keys.dpad.right = pressed;
			gbc->core.keys.interrupt = true;
			break;
		case GBCC_KEY_TURBO:
			gbc->core.keys.turbo ^= pressed;
			gbc->audio.index *= pressed;
			break;
		case GBCC_KEY_SCREENSHOT:
			gbc->window.screenshot ^= pressed;
			break;
		case GBCC_KEY_RAW_SCREENSHOT:
			gbc->window.raw_screenshot ^= pressed;
			break;
		case GBCC_KEY_PAUSE:
			gbc->pause ^= pressed;
			break;
		case GBCC_KEY_PRINTER:
			if (pressed) {
				if (gbc->core.link_cable.state == GBCC_LINK_CABLE_STATE_PRINTER) {
					gbc->core.link_cable.state = GBCC_LINK_CABLE_STATE_DISCONNECTED;
				} else{
					gbc->core.link_cable.state = GBCC_LINK_CABLE_STATE_PRINTER;
				}
			}
			if (gbc->core.link_cable.state == GBCC_LINK_CABLE_STATE_PRINTER) {
				gbcc_window_show_message(gbc, "Printer connected", 1, true);
			} else {
				gbcc_window_show_message(gbc, "Printer disconnected", 1, true);
			}
			break;
		case GBCC_KEY_FPS:
			gbc->show_fps ^= pressed;
			break;
		case GBCC_KEY_FRAME_BLENDING:
			gbc->frame_blending ^= pressed;
			if (gbc->frame_blending) {
				gbcc_window_show_message(gbc, "Frame blending enabled", 1, true);
			} else {
				gbcc_window_show_message(gbc, "Frame blending disabled", 1, true);
			}
			break;
		case GBCC_KEY_VSYNC:
			gbc->core.sync_to_video ^= pressed;
			if (!pressed) {
				break;
			}
			gbc->audio.index = 0;
			if (gbc->core.sync_to_video) {
				gbcc_window_show_message(gbc, "Vsync enabled", 1, true);
			} else {
				gbcc_window_show_message(gbc, "Vsync disabled", 1, true);
			}
			break;
		case GBCC_KEY_VRAM:
			gbc->vram_display ^= pressed;
			break;
		case GBCC_KEY_DISPLAY_BACKGROUND:
			gbc->core.hide_background ^= pressed;
			if (!pressed) {
				break;
			}
			if (gbc->core.hide_background) {
				gbcc_window_show_message(gbc, "Background disabled", 1, true);
			} else {
				gbcc_window_show_message(gbc, "Background enabled", 1, true);
			}
			break;
		case GBCC_KEY_DISPLAY_WINDOW:
			gbc->core.hide_window ^= pressed;
			if (!pressed) {
				break;
			}
			if (gbc->core.hide_window) {
				gbcc_window_show_message(gbc, "Window disabled", 1, true);
			} else {
				gbcc_window_show_message(gbc, "Window enabled", 1, true);
			}
			break;
		case GBCC_KEY_DISPLAY_SPRITES:
			gbc->core.hide_sprites ^= pressed;
			if (!pressed) {
				break;
			}
			if (gbc->core.hide_sprites) {
				gbcc_window_show_message(gbc, "Sprites disabled", 1, true);
			} else {
				gbcc_window_show_message(gbc, "Sprites enabled", 1, true);
			}
			break;
		case GBCC_KEY_LINK_CABLE:
			if (pressed) {
				if (gbc->core.link_cable.state == GBCC_LINK_CABLE_STATE_LOOPBACK) {
					gbc->core.link_cable.state = GBCC_LINK_CABLE_STATE_DISCONNECTED;
				} else{
					gbc->core.link_cable.state = GBCC_LINK_CABLE_STATE_LOOPBACK;
				}
			}
			if (gbc->core.link_cable.state == GBCC_LINK_CABLE_STATE_LOOPBACK) {
				gbcc_window_show_message(gbc, "Link cable connected", 1, true);
			} else {
				gbcc_window_show_message(gbc, "Link cable disconnected", 1, true);
			}
			break;
		case GBCC_KEY_AUTOSAVE:
			gbc->autosave ^= pressed;
			if (!pressed) {
				break;
			}
			if (gbc->autosave) {
				gbcc_window_show_message(gbc, "Autosave enabled", 1, true);
			} else {
				gbcc_window_show_message(gbc, "Autosave disabled", 1, true);
			}
			break;
		case GBCC_KEY_BACKGROUND_PLAY:
			gbc->background_play ^= pressed;
			if (!pressed) {
				break;
			}
			if (gbc->background_play) {
				gbcc_window_show_message(gbc, "Background playback enabled", 1, true);
			} else {
				gbcc_window_show_message(gbc, "Background playback disabled", 1, true);
			}
			break;
		case GBCC_KEY_MENU:
			gbc->menu.show = pressed;
			if (pressed) {
				gbcc_menu_update(gbc);
			}
			break;
		case GBCC_KEY_INTERLACE:
			gbc->interlacing ^= pressed;
			if (!pressed) {
				break;
			}
			if (gbc->interlacing) {
				gbcc_window_show_message(gbc, "Interlacing enabled", 1, true);
			} else {
				gbcc_window_show_message(gbc, "Interlacing disabled", 1, true);
			}
			break;
		case GBCC_KEY_SHADER:
			if (pressed) {
				gbc->window.gl.cur_shader++;
				gbc->window.gl.cur_shader %= N_ELEM(gbc->window.gl.shaders);
				gbcc_window_show_message(gbc, gbc->window.gl.shaders[gbc->window.gl.cur_shader].name, 1, true);
			}
			break;
		case GBCC_KEY_CHEATS:
			gbc->core.cheats.enabled ^= pressed;
			if (!pressed) {
				break;
			}
			if (gbc->core.cheats.enabled) {
				gbcc_window_show_message(gbc, "Cheats enabled", 1, true);
			} else {
				gbcc_window_show_message(gbc, "Cheats disabled", 1, true);
			}
			break;
		case GBCC_KEY_ACCELEROMETER_UP:
			gbc->core.cart.mbc.accelerometer.tilt.up = pressed;
			break;
		case GBCC_KEY_ACCELEROMETER_DOWN:
			gbc->core.cart.mbc.accelerometer.tilt.down = pressed;
			break;
		case GBCC_KEY_ACCELEROMETER_LEFT:
			gbc->core.cart.mbc.accelerometer.tilt.left = pressed;
			break;
		case GBCC_KEY_ACCELEROMETER_RIGHT:
			gbc->core.cart.mbc.accelerometer.tilt.right = pressed;
			break;
		case GBCC_KEY_ACCELEROMETER_MAX_UP:
			if (pressed) {
				gbc->core.cart.mbc.accelerometer.real_y = 0x81D0u + 0x70u;
			}
			break;
		case GBCC_KEY_ACCELEROMETER_MAX_DOWN:
			if (pressed) {
				gbc->core.cart.mbc.accelerometer.real_y = 0x81D0u - 0x70u;
			}
			break;
		case GBCC_KEY_ACCELEROMETER_MAX_LEFT:
			if (pressed) {
				gbc->core.cart.mbc.accelerometer.real_x = 0x81D0u + 0x70u;
			}
			break;
		case GBCC_KEY_ACCELEROMETER_MAX_RIGHT:
			if (pressed) {
				gbc->core.cart.mbc.accelerometer.real_x = 0x81D0u - 0x70u;
			}
			break;
		case GBCC_KEY_SAVE_STATE_1:
		case GBCC_KEY_SAVE_STATE_2:
		case GBCC_KEY_SAVE_STATE_3:
		case GBCC_KEY_SAVE_STATE_4:
		case GBCC_KEY_SAVE_STATE_5:
		case GBCC_KEY_SAVE_STATE_6:
		case GBCC_KEY_SAVE_STATE_7:
		case GBCC_KEY_SAVE_STATE_8:
		case GBCC_KEY_SAVE_STATE_9:
			if (!pressed) {
				break;
			}
			gbc->save_state = (int8_t)(key - GBCC_KEY_SAVE_STATE_1 + 1);
			break;
		case GBCC_KEY_LOAD_STATE_1:
		case GBCC_KEY_LOAD_STATE_2:
		case GBCC_KEY_LOAD_STATE_3:
		case GBCC_KEY_LOAD_STATE_4:
		case GBCC_KEY_LOAD_STATE_5:
		case GBCC_KEY_LOAD_STATE_6:
		case GBCC_KEY_LOAD_STATE_7:
		case GBCC_KEY_LOAD_STATE_8:
		case GBCC_KEY_LOAD_STATE_9:
			if (!pressed) {
				break;
			}
			gbc->load_state = (int8_t)(key - GBCC_KEY_LOAD_STATE_1 + 1);
			break;
	}
}

void gbcc_input_accelerometer_update(struct gbcc_accelerometer *acc)
{
	int dx = 0;
	int dy = 0;

	if (acc->tilt.up) {
		dy += 8;
	}
	if (acc->tilt.down) {
		dy -= 8;
	}
	if (acc->tilt.left) {
		dx += 8;
	}
	if (acc->tilt.right) {
		dx -= 8;
	}

	if (dx == 0) {
		if (acc->real_x > 0x81D0u) {
			dx = -8;
		} else if (acc->real_x < 0x81D0u) {
			dx = 8;
		}
	}
	if (dy == 0) {
		if (acc->real_y > 0x81D0u) {
			dy = -8;
		} else if (acc->real_y < 0x81D0u) {
			dy = 8;
		}
	}
	acc->real_x += dx;
	acc->real_y += dy;

	if (acc->real_x > 0x81D0u + 0x70u) {
		acc->real_x = 0x81D0u + 0x70u;
	} else if (acc->real_x < 0x81D0u - 0x70u) {
		acc->real_x = 0x81D0u - 0x70u;
	}
	if (acc->real_y > 0x81D0u + 0x70u) {
		acc->real_y = 0x81D0u + 0x70u;
	} else if (acc->real_y < 0x81D0u - 0x70u) {
		acc->real_y = 0x81D0u - 0x70u;
	}
}
