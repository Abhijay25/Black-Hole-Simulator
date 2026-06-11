#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec2 bhPos;      // BH screen position, normalized [0,1]
uniform float strength;  // lensing strength

void main() {
    vec2 uv = fragTexCoord;
    vec2 diff = uv - bhPos;
    float dist = length(diff);

    if (dist > 0.001) {
        // Deflection proportional to 1/dist (Schwarzschild approximation)
        float deflection = strength / (dist * dist);
        deflection = min(deflection, 0.08);
        uv -= normalize(diff) * deflection;
        uv = clamp(uv, 0.0, 1.0);
    }

    finalColor = texture(texture0, uv);
}
