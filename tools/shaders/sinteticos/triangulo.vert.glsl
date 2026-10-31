#version 460
void main() {
    vec2 p[3] = vec2[3](vec2(-0.5,-0.5), vec2(0.5,-0.5), vec2(0.0,0.5));
    gl_Position = vec4(p[gl_VertexID], 0.5, 1.0);
}
