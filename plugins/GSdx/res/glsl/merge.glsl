//#version 420 // Keep it for editor detection

in SHADER
{
    vec4 p;
    vec2 t;
} PSin;

#ifdef FRAGMENT_SHADER

layout(location = 0) out vec4 SV_Target0;

void ps_main0()
{
    vec4 c = texture(TextureSampler, PSin.t);
    // Note: clamping will be done by fixed unit
    c.a *= 2.0f;
    SV_Target0 = c;
}

void ps_main1()
{
    vec4 c = texture(TextureSampler, PSin.t);
    c.a = BGColor.a;
    SV_Target0 = c;
}

#endif
