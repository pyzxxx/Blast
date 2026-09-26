REGISTER_SHADER(ModelVert, "shaders/model.vert")

REGISTER_SHADER(ModelFrag, "shaders/model.frag")

REGISTER_SHADER(DepthVert, "shaders/depth.vert")

REGISTER_SHADER(DepthFrag, "shaders/depth.frag")
REGISTER_VARIANT(DepthFrag, Cutout, "VARIANT_CUTOUT")

REGISTER_SHADER(ShadowVert, "shaders/shadow.vert")

REGISTER_SHADER(ShadowFrag, "shaders/shadow.frag")
REGISTER_VARIANT(ShadowFrag, Cutout, "VARIANT_CUTOUT")

REGISTER_SHADER(CompositeVert, "shaders/composite.vert")
REGISTER_SHADER(CompositeFrag, "shaders/composite.frag")

REGISTER_SHADER(UiVert, "shaders/ui.vert")
REGISTER_SHADER(UiFrag, "shaders/ui.frag")
