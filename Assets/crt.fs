#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec2 resolution;

void main() {
    vec3 color = texture(texture0, fragTexCoord).rgb;
    float scanline = sin(fragTexCoord.y * resolution.y * 3.14159);
    color *= 1.0 + 0.05 * scanline;
    finalColor = vec4(color, 1.0) * fragColor;
}