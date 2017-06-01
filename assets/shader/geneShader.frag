varying highp vec2 outTextCoord;
varying lowp vec4 outColor;
uniform lowp bool selected;

// bandpass smooth filter   __/  \__
float smoothband(float lo, float hi, float e, float t) {
    return (lo < hi) ?
    smoothstep(lo-e, lo+e, t) - smoothstep(hi-e, hi+e, t) :
    1.0 - (smoothstep(hi-e, hi+e, t) - smoothstep(lo-e, lo+e, t));
}

void main(void)
{
    // helper colors
    vec4 cNone = vec4(0.0,0.0,0.0,0.0);
    vec4 cWhite = vec4(1.0,1.0,1.0,1.0);

    // derive color
    vec4 fragColor = outColor;
    vec2 pos = mod(outTextCoord.xy, vec2(1.0)) - vec2(0.5);
    float dist = length(pos);

    // radii of circle
    float radii = (selected) ? 0.3 : 0.5;

    fragColor = mix(fragColor, cNone, smoothstep(radii - 0.02, radii, dist));
    if (selected) {
        fragColor = mix(fragColor, cWhite, smoothband(radii + 0.02, 0.49, 0.01, dist));
    }

    gl_FragColor = fragColor;
}
