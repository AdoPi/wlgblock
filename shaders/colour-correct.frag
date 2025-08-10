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
uniform sampler3D lut;

void main()
{
	vec4 icol = texture(tex, Texcoord);
	// TODO: This factor is a hold-over from the old colour correction
	// code, when I was manually interpolating colours. Is it correct /
	// needed anymore?
	const float brightening = 35.0 / 31.0;
	out_colour = vec4(texture(lut, icol.rgb).rgb, 1) * brightening;
}
