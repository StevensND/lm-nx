#version 460
layout(binding = 0) uniform sampler2D imagen;
layout(location = 0) out vec4 color;
void main() { color = texture(imagen, vec2(0.5, 0.5)); }
