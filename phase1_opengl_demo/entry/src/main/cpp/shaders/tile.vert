// tile.vert — 瓦片顶点着色器
//
// 输入：
//   a_position  — 顶点位置 (NDC 空间, [-1, 1])
//   a_texCoord  — 纹理坐标 (UV, [0, 1])
//
// Uniforms：
//   u_mvpMatrix — 模型-视图-投影矩阵（正交投影 + 平移）
//
// 输出：
//   v_texCoord  — 插值后的纹理坐标（传递到片元着色器）

attribute vec2 a_position;
attribute vec2 a_texCoord;

varying vec2 v_texCoord;

uniform mat4 u_mvpMatrix;

void main() {
    gl_Position = u_mvpMatrix * vec4(a_position, 0.0, 1.0);
    v_texCoord = a_texCoord;
}
