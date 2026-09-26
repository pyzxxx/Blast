#include "material_codec.h"
#include "log.h"

#include <cstring>

bool material_decode(const uint8_t* data, uint64_t size, MaterialData& out) {
    if (size < sizeof(pak_format::MaterialHeader) + sizeof(uint64_t) * k_tex_count) {
        return false;
    }

    pak_format::MaterialHeader hdr;
    memcpy(&hdr, data, sizeof(hdr));
    if (hdr.magic != pak_format::k_material_magic) {
        return false;
    }

    memcpy(&out.block, hdr.block, sizeof(MaterialBlock));
    if (out.block.params.layer > k_layer_transparent || out.block.params.cutout > k_cutout_dither) {
        LOGE("material_codec: behavior field out of range");
        return false;
    }

    memcpy(out.texture_ids, data + sizeof(hdr), sizeof(out.texture_ids));
    return true;
}
