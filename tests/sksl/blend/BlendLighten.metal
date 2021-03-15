struct Inputs {
    float4 src;
    float4 dst;
};
struct Outputs {
    float4 sk_FragColor [[color(0)]];
};
float4 blend_lighten(float4 src, float4 dst) {
    float4 result = src + (1.0 - src.w) * dst;
    result.xyz = max(result.xyz, (1.0 - dst.w) * src.xyz + dst.xyz);
    return result;
}

fragment Outputs fragmentMain(Inputs _in [[stage_in]], bool _frontFacing [[front_facing]], float4 _fragCoord [[position]]) {
    Outputs _out;
    (void)_out;
    _out.sk_FragColor = blend_lighten(_in.src, _in.dst);
    return _out;
}
