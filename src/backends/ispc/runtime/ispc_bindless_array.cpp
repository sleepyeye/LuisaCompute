#include <backends/ispc/runtime/ispc_bindless_array.h>

namespace lc::ispc {

ISPCBindlessArray::ISPCBindlessArray(size_t size) noexcept : 
    size(size), bufferVector(size, 0), bufferAddressVector(size, 0), tex2dVector(size, 0), tex3dVector(size, 0) {
    data.size = size;
    data.buffer = bufferAddressVector.data();
    data.tex2d = tex2dVector.data();
    data.tex3d = tex3dVector.data();
    LUISA_INFO("{} {} {}", (uint64_t)data.buffer, (uint64_t)data.tex2d, (uint64_t)data.tex3d);
}

void ISPCBindlessArray::emplace_buffer(size_t index, uint64_t buffer, size_t offset) noexcept {
    bufferVector[index] = buffer;
    bufferAddressVector[index] = buffer + offset;
    auto a = reinterpret_cast<uint32_t *>(buffer + offset)[0];
    LUISA_INFO("emplace {} = {}, a = {}", buffer, data.buffer[index], a);
}

void ISPCBindlessArray::emplace_tex2d(size_t index, uint64_t buffer, Sampler sampler) noexcept {

}

void ISPCBindlessArray::emplace_tex3d(size_t index, uint64_t buffer, Sampler sampler) noexcept {

}

void ISPCBindlessArray::remove_buffer(size_t index) noexcept {
    bufferVector[index] = 0;
}

void ISPCBindlessArray::remove_tex2d(size_t index) noexcept {
    tex2dVector[index] = 0;
}

void ISPCBindlessArray::remove_tex3d(size_t index) noexcept {
    tex3dVector[index] = 0;
}

[[nodiscard]] bool ISPCBindlessArray::uses_buffer(uint64_t handle) const noexcept {
    for(auto& item: bufferVector)
        if(item == handle) return true;
    return false;
}

[[nodiscard]] bool ISPCBindlessArray::uses_texture(uint64_t handle) const noexcept {
    for(auto& item: tex2dVector)
        if(item == handle) return true;
    for(auto& item: tex3dVector)
        if(item == handle) return true;
    return false;
}


}