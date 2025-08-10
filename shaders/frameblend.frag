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
uniform sampler2D last_tex;
uniform int odd_frame;
uniform bool interlacing;
uniform bool frameblending;

void main()
{
	vec4 old = texture(last_tex, Texcoord);
	vec4 new = texture(tex, Texcoord);

	if (interlacing) {
		float darken = floor(mod(Texcoord.y * 144 + odd_frame, 2));
		new *= darken * 0.5 + 0.5;
		darken = floor(mod(Texcoord.y * 144 + odd_frame + 1, 2));
		old *= darken * 0.5 + 0.5;
	}
	if (frameblending) {
		out_colour = mix(new, old, 0.5);
	} else {
		out_colour = new;
	}
}
