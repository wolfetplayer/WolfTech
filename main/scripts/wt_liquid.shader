textures/wt_liquid/siwa_water
{
    qer_editorimage textures/wt_liquid/siwa_water.tga
    qer_trans 0.5
    q3map_globaltexture
    surfaceparm trans
    surfaceparm nonsolid
    surfaceparm water
    surfaceparm nomarks
    surfaceparm nodlight
    cull disable
    {
        map textures/wt_liquid/siwa_water.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen identity
        tcMod scale 0.5 0.5
        tcMod scroll 0 0.15
    }
    {
        map textures/wt_liquid/siwa_water.tga
        blendFunc GL_DST_COLOR GL_ONE
        rgbGen identity
        tcMod scale 0.35 0.35
        tcMod turb 0 0.05 0 0.1
        tcMod scroll 0.02 0.25
    }
    {
        map $lightmap
        blendFunc filter
        rgbGen identity
        tcGen lightmap
    }
}