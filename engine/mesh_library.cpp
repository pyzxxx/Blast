#include "mesh_library.h"
#include "mesh_codec.h"
#include "render_core.h"
#include "services.h"

#include <cstring>

bool MeshLibrary::init(uint32_t cap, const Services& services) {
    if (!store_.init(cap, static_cast<uint8_t>(HandleTag::Mesh))) {
        return false;
    }

    gpu_ = services.gpu;
    rcore_ = services.rcore;
    return true;
}

bool MeshLibrary::upload(const MeshData& data, Mesh& out) {
    auto cleanup = [&]() {
        if (out.vb != k_gpu_invalid) {
            rcore_->delay_delete_buffer(out.vb);
        }

        if (out.ib != k_gpu_invalid) {
            rcore_->delay_delete_buffer(out.ib);
        }
    };

    const uint32_t pos_size = static_cast<uint32_t>(data.positions.size() * sizeof(pak_format::VertexPos));
    const uint32_t attr_size = static_cast<uint32_t>(data.attrs.size() * sizeof(pak_format::VertexAttr));
    const uint32_t vb_size = pos_size + attr_size;
    const uint32_t ib_size = static_cast<uint32_t>(data.indices.size() * sizeof(uint32_t));
    out.attr_off = pos_size;
    GpuBufferDesc vb_desc = {vb_size,
                             GpuBufferUsage::Storage | GpuBufferUsage::Vertex | GpuBufferUsage::DeviceAddress |
                                 GpuBufferUsage::TransferDst,
                             GpuMemoryDomain::GpuOnly};
    GpuBufferDesc ib_desc = {ib_size,
                             GpuBufferUsage::Storage | GpuBufferUsage::Index | GpuBufferUsage::DeviceAddress |
                                 GpuBufferUsage::TransferDst,
                             GpuMemoryDomain::GpuOnly};
    out.vb = gpu_->create_buffer(vb_desc);
    out.ib = gpu_->create_buffer(ib_desc);
    if (out.vb == k_gpu_invalid || out.ib == k_gpu_invalid) {
        cleanup();
        return false;
    }

    const uint32_t vb_off = rcore_->upload_alloc(vb_size);
    const uint32_t ib_off = rcore_->upload_alloc(ib_size);
    if (vb_off == UINT32_MAX || ib_off == UINT32_MAX) {
        cleanup();
        return false;
    }

    memcpy(rcore_->upload_cpu_addr() + vb_off, data.positions.data(), pos_size);
    memcpy(rcore_->upload_cpu_addr() + vb_off + pos_size, data.attrs.data(), attr_size);
    memcpy(rcore_->upload_cpu_addr() + ib_off, data.indices.data(), ib_size);
    GpuCmd cmd = rcore_->new_upload_cmd();
    gpu_->cmd_copy_buffer(cmd, out.vb, 0, rcore_->upload_buffer(), vb_off, vb_size);
    gpu_->cmd_copy_buffer(cmd, out.ib, 0, rcore_->upload_buffer(), ib_off, ib_size);
    gpu_->cmd_buffer_barrier(cmd, out.vb, ResourceState::TransferDst, ResourceState::ShaderRead);
    gpu_->cmd_buffer_barrier(cmd, out.ib, ResourceState::TransferDst, ResourceState::ShaderRead);

    memcpy(out.bounds_min, data.bounds_min, sizeof(out.bounds_min));
    memcpy(out.bounds_max, data.bounds_max, sizeof(out.bounds_max));
    out.layout = MeshLayout::Static;
    out.lods[0] = {.first_index = 0,
                   .index_count = static_cast<uint32_t>(data.indices.size()),
                   .base_vertex = 0,
                   .screen_error = 0,
                   .resident = 1};
    return true;
}

void MeshLibrary::shutdown() {
    store_.shutdown([this](Mesh& m) {
        if (m.vb != k_gpu_invalid) {
            rcore_->delay_delete_buffer(m.vb);
        }

        if (m.ib != k_gpu_invalid) {
            rcore_->delay_delete_buffer(m.ib);
        }
    });
}

void MeshLibrary::commit(MeshHandle h, const Mesh& mesh) {
    store_.commit(h, mesh);
}

void MeshLibrary::release_ref(MeshHandle h) {
    store_.release_ref(h, [this](Mesh& m) {
        if (m.vb != k_gpu_invalid) {
            rcore_->delay_delete_buffer(m.vb);
        }

        if (m.ib != k_gpu_invalid) {
            rcore_->delay_delete_buffer(m.ib);
        }
    });
}
