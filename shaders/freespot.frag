#include <common>
#include <tonemapping.glsl>

in vec4 f_clip_pos;
in vec4 f_clip_future_pos;

layout(location = 0) out vec4 out_color;
#if MOTIONBLUR_ENABLED
layout(location = 1) out vec4 out_motion;
#endif

void main()
{
	float x = (gl_PointCoord.x - 0.5f) * 2.0f;
	float y = (gl_PointCoord.y - 0.5f) * 2.0f;
	float dist = sqrt(x * x + y * y);
	if (dist > 0.5f)
		discard;
	vec4 color = vec4(param[0].rgb * emission, mix(param[0].a, 0.0f, dist * 2.0f));
#if POSTFX_ENABLED
    out_color = color;
#else
    out_color = tonemap(color);
#endif
#if MOTIONBLUR_ENABLED
	{
        vec2 a = (f_clip_future_pos.xy / f_clip_future_pos.w) * 0.5 + 0.5;;
        vec2 b = (f_clip_pos.xy / f_clip_pos.w) * 0.5 + 0.5;;
        
        out_motion = vec4(a - b, 0.0f, 0.0f);
	}
#endif
}