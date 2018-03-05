$input v_texcoord0
/*
 * Copyright 2011-2018 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include "../common/common.sh"
SAMPLER2D(s_texColor, 0);
uniform vec4 s_center;
uniform vec4 s_scale;
uniform vec4 s_iter;
void main()
{
    vec2 z, c;
    vec2 pos;
	s_scale.x = 1/s_scale.x;
	s_scale.y = 1/s_scale.y;
	
	s_center.x = -s_center.x;
	s_center.y = -s_center.y;
    c.x = 1.3333 * (v_texcoord0.x - 0.5) * s_scale.x - s_center.x;
    c.y = (v_texcoord0.y - 0.5) * s_scale.xy - s_center.y;

    int i;
    z = c;
    for(i=0; i<s_iter.x; i++) {
        float x = (z.x * z.x - z.y * z.y) + c.x;
        float y = (z.y * z.x + z.x * z.y) + c.y;

        if((x * x + y * y) > 4.0) break;
        z.x = x;
        z.y = y;
    }
	pos.y = 0;
	pos.x =  (i == s_iter ? 0.0 : float(i)) / 100.0;
	gl_FragColor = texture2D(s_texColor, pos);
}
