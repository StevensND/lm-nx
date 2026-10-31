#version 460
layout(std140, binding = 0) uniform Uniformes { vec4 escala; };
layout(std430, binding = 0) readonly buffer Vertices { vec4 posiciones[]; };
layout(std140, binding = 15) uniform Inmediatos { uvec4 palabras[4]; };
void main() { gl_Position = posiciones[gl_VertexID + int(palabras[0].x)] * escala; }
