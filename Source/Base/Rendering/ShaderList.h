REGISTER_SHADER(ModelVert, "Assets/Shaders/Model.vert")
REGISTER_SHADER(ModelFrag, "Assets/Shaders/Model.frag")
REGISTER_VARIANT(ModelFrag, AlphaMask, "ALPHA_MASK")
REGISTER_VARIANT(ModelFrag, AlphaBlend, "ALPHA_BLEND")

REGISTER_SHADER(CompositeVert, "Assets/Shaders/Composite.vert")
REGISTER_SHADER(CompositeFrag, "Assets/Shaders/Composite.frag")

REGISTER_SHADER(ClusterAABB, "Assets/Shaders/ClusterAABB.comp")
REGISTER_SHADER(Clustering, "Assets/Shaders/Clustering.comp")
REGISTER_SHADER(ClusterDebug, "Assets/Shaders/ClusterDebug.comp")

REGISTER_SHADER(BVHDebugVert, "Assets/Shaders/BVHDebug.vert")
REGISTER_SHADER(BVHDebugFrag, "Assets/Shaders/BVHDebug.frag")

REGISTER_SHADER(ImguiVert, "Assets/Shaders/Imgui.vert")
REGISTER_SHADER(ImguiFrag, "Assets/Shaders/Imgui.frag")
