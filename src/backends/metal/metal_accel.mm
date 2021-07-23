//
// Created by Mike Smith on 2021/7/22.
//

#import <backends/metal/metal_device.h>
#import <backends/metal/metal_accel.h>

namespace luisa::compute::metal {

id<MTLCommandBuffer> MetalAccel::build(
    id<MTLCommandBuffer> command_buffer,
    AccelBuildHint hint,
    std::span<const uint64_t> mesh_handles,
    std::span<const float4x4> transforms) noexcept {

    // build instance buffer
    auto instance_buffer_size = mesh_handles.size() * sizeof(MTLAccelerationStructureInstanceDescriptor);
    _instance_buffer = [_device->handle() newBufferWithLength:instance_buffer_size
                                                      options:MTLResourceStorageModePrivate
                                                              | MTLResourceHazardTrackingModeUntracked];
    _instance_buffer_host = [_device->handle() newBufferWithLength:instance_buffer_size
                                                           options:MTLResourceStorageModeShared
                                                                   | MTLResourceHazardTrackingModeUntracked
                                                                   | MTLResourceOptionCPUCacheModeWriteCombined];
    auto instances = static_cast<MTLAccelerationStructureInstanceDescriptor *>(_instance_buffer_host.contents);
    for (auto i = 0u; i < mesh_handles.size(); i++) {
        instances[i].mask = 0xffffffffu;
        instances[i].accelerationStructureIndex = i;
        instances[i].intersectionFunctionTableOffset = 0u;
        instances[i].options = MTLAccelerationStructureInstanceOptionOpaque;
        auto t = transforms[i];
        for (auto c = 0; c < 4; c++) {
            instances[i].transformationMatrix[c] = {t[c].x, t[c].y, t[c].z};
        }
    }
    auto blit_encoder = [command_buffer blitCommandEncoder];
    [blit_encoder copyFromBuffer:_instance_buffer_host
                    sourceOffset:0u
                        toBuffer:_instance_buffer
               destinationOffset:0u
                            size:instance_buffer_size];
    [blit_encoder endEncoding];
    [command_buffer addCompletedHandler:^(id<MTLCommandBuffer>) { _semaphore.release(); }];
    [command_buffer commit];// to avoid dead locks...

    // build accel and (possibly) compact
    command_buffer = [[command_buffer commandQueue] commandBuffer];
    _descriptor = [MTLInstanceAccelerationStructureDescriptor descriptor];
    _descriptor.instancedAccelerationStructures = _device->mesh_handles(mesh_handles);
    _descriptor.instanceCount = mesh_handles.size();
    _descriptor.instanceDescriptorBuffer = _instance_buffer;
    switch (hint) {
        case AccelBuildHint::FAST_TRACE: _descriptor.usage = MTLAccelerationStructureUsageNone; break;
        case AccelBuildHint::FAST_UPDATE: _descriptor.usage = MTLAccelerationStructureUsageRefit; break;
        case AccelBuildHint::FAST_REBUILD: _descriptor.usage = MTLAccelerationStructureUsagePreferFastBuild; break;
    }
    _sizes = [_device->handle() accelerationStructureSizesWithDescriptor:_descriptor];
    _handle = [_device->handle() newAccelerationStructureWithSize:_sizes.accelerationStructureSize];
    auto scratch_buffer = [_device->handle() newBufferWithLength:_sizes.buildScratchBufferSize
                                                         options:MTLResourceStorageModePrivate
                                                                 | MTLResourceHazardTrackingModeUntracked];
    auto command_encoder = [command_buffer accelerationStructureCommandEncoder];
    [command_encoder buildAccelerationStructure:_handle
                                     descriptor:_descriptor
                                  scratchBuffer:scratch_buffer
                            scratchBufferOffset:0u];
    if (hint != AccelBuildHint::FAST_REBUILD) {
        auto compacted_size_buffer = [_device->handle() newBufferWithLength:sizeof(uint)
                                                                    options:MTLResourceStorageModeShared
                                                                            | MTLResourceHazardTrackingModeUntracked];
        [command_encoder writeCompactedAccelerationStructureSize:_handle
                                                        toBuffer:compacted_size_buffer
                                                          offset:0u];
        [command_encoder endEncoding];
        [command_buffer commit];
        [command_buffer waitUntilCompleted];
        auto compacted_size = static_cast<const uint *>(compacted_size_buffer.contents)[0];
        auto accel = _handle;
        _handle = [_device->handle() newAccelerationStructureWithSize:compacted_size];
        command_buffer = [[command_buffer commandQueue] commandBuffer];
        command_encoder = [command_buffer accelerationStructureCommandEncoder];
        [command_encoder copyAndCompactAccelerationStructure:accel
                                     toAccelerationStructure:_handle];
    }
    [command_encoder endEncoding];
    return command_buffer;
}

id<MTLCommandBuffer> MetalAccel::update(
    id<MTLCommandBuffer> command_buffer,
    bool should_update_transforms,
    std::span<const float4x4> transforms) noexcept {

    // update transforms
    if (should_update_transforms) {
        _semaphore.acquire();
        auto instances = static_cast<MTLAccelerationStructureInstanceDescriptor *>(_instance_buffer_host.contents);
        for (auto i = 0u; i < transforms.size(); i++) {
            auto t = transforms[i];
            for (auto c = 0; c < 4; c++) {
                instances[i].transformationMatrix[c] = {t[c].x, t[c].y, t[c].z};
            }
        }
        auto blit_encoder = [command_buffer blitCommandEncoder];
        [blit_encoder copyFromBuffer:_instance_buffer_host
                        sourceOffset:0u
                            toBuffer:_instance_buffer
                   destinationOffset:0u
                                size:_instance_buffer_host.length];
        [blit_encoder endEncoding];
        [command_buffer addCompletedHandler:^(id<MTLCommandBuffer>) { _semaphore.release(); }];
        [command_buffer commit];// to avoid dead locks
        command_buffer = [[command_buffer commandQueue] commandBuffer];
    }

    // update accel
    if (_update_buffer == nullptr || _update_buffer.length < _sizes.refitScratchBufferSize) {
        _update_buffer = [_device->handle() newBufferWithLength:_sizes.refitScratchBufferSize
                                                        options:MTLResourceStorageModePrivate];
    }
    auto command_encoder = [command_buffer accelerationStructureCommandEncoder];
    [command_encoder refitAccelerationStructure:_handle
                                     descriptor:_descriptor
                                    destination:_handle
                                  scratchBuffer:_update_buffer
                            scratchBufferOffset:0u];
    [command_encoder endEncoding];
    return command_buffer;
}

}
