/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#version 150 core

in vec2 Texcoord;

out vec4 out_colour;

uniform sampler2D tex;

const vec3 background = vec3(99, 149, 50) / 255.0;
const vec3 foreground = vec3(28, 66, 13) / 255.0;

float luma(vec3 col)
{
	return 0.2162 * col.r + 0.7152 * col.g + 0.0722 * col.b;
}

void main()
{
	float src = luma(texture(tex, Texcoord).rgb);
	int x = int(mod(Texcoord.x * 160 * 7, 7));
	int y = int(mod(Texcoord.y * 144 * 7, 7));

        if (x >= 3) {
                x -= 3;
        } else if (x < 3) {
                x = 3 - x;
        }

        if (y >= 3) {
                y -= 3;
        } else if (y < 3) {
                y = 3 - y;
        }

	float alpha = 1;
	if (x == 2 && y == 2) {
		alpha = 0.959;
	} else if (x == 3) {
                if (y == 3) {
                        alpha = 0.529;
                } else if (y == 2) {
                        alpha = 0.793;
                } else if (y == 1) {
                        alpha = 0.893;
                }
	} else if (y == 3) {
                if (x == 2) {
                        alpha = 0.793;
                } else if (x == 1) {
                        alpha = 0.893;
                }
	}
	out_colour = vec4(mix(background, foreground, (1.0 - src) * alpha), 1.0);
}
