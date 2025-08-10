/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 * I don't think you can really copyright this shader though :)
 */

#version 150 core

in vec2 Texcoord;

out vec4 out_colour;

uniform sampler2D tex;

void main()
{
	out_colour = texture(tex, Texcoord);
}
