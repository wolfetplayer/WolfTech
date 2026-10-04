textures/wt_alpha/mesh_c02
{
    qer_editorimage textures/wt_alpha/mesh_c02.tga
    qer_alphafunc greater 0.5
    qer_trans 0.99
    surfaceparm trans
    surfaceparm alphashadow
    surfaceparm metalsteps
    nomipmaps
    nopicmip
    cull disable
    {
        map textures/wt_alpha/mesh_c02.tga
        alphaFunc GE128
        depthWrite
        rgbGen vertex
    }
}