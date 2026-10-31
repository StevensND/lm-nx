struct Uniformes { color: vec4f, }
struct Inmediatos { palabras0: vec4u, palabras1: vec4u, palabras2: vec4u, palabras3: vec4u, }
@group(0) @binding(0) var<storage, read> posiciones: array<vec4f>;
@group(1) @binding(0) var<uniform> uniformes: Uniformes;
@group(2) @binding(0) var imagen: texture_2d<f32>;
@group(2) @binding(1) var muestreo: sampler;
var<immediate> inmediatos: Inmediatos;

@vertex fn vs_main(@builtin(vertex_index) indice: u32) -> @builtin(position) vec4f {
    let posicion = posiciones[indice + inmediatos.palabras0.x];
    return vec4f(posicion.xy, 0.5, 1.0);
}

@fragment fn fs_main() -> @location(0) vec4f {
    return textureSample(imagen, muestreo, vec2f(0.5, 0.5)) * uniformes.color;
}
