// tile.frag — 瓦片片元着色器
//
// 精度：mediump（兼顾性能与质量）
//
// 输入：
//   v_texCoord — 插值的纹理坐标
//
// Uniforms：
//   u_texture  — 瓦片纹理采样器（2D）
//
// 输出：
//   gl_FragColor — 像素最终颜色

precision mediump float;

varying vec2 v_texCoord;
uniform sampler2D u_texture;

void main() {
    gl_FragColor = texture2D(u_texture, v_texCoord);
}
