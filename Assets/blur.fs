#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;
uniform float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
uniform vec2 direction;

void main()
{
    vec3 result = texture(texture0, fragTexCoord).rgb * weight[0]; // current fragment's contribution
    for(int i = 1; i < 5; ++i) {
        vec2 offset = direction * float(i);
        result += texture(texture0, fragTexCoord + offset).rgb * weight[i];
        result += texture(texture0, fragTexCoord - offset).rgb * weight[i];
    }
    finalColor = vec4(result, 1);
}