//
// Created by Mike Smith on 2021/3/17.
//

#pragma once

#import <vector>
#import <thread>
#import <future>

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#import <core/spin_mutex.h>
#import <runtime/device.h>
#import <backends/metal/metal_event.h>
#import <backends/metal/metal_compiler.h>
#import <backends/metal/metal_argument_buffer.h>

namespace luisa::compute::metal {

class MetalDevice : public Device {

private:
    id<MTLDevice> _handle{nullptr};
    std::unique_ptr<MetalCompiler> _compiler{nullptr};
    std::unique_ptr<MetalArgumentBufferPool> _argument_buffer_pool{nullptr};
    dispatch_queue_t _dispatch_queue{nullptr};
    MTLSharedEventListener *_event_listener{nullptr};
    
    // for buffers
    mutable spin_mutex _buffer_mutex;
    std::vector<id<MTLBuffer>> _buffer_slots;
    std::vector<size_t> _available_buffer_slots;
    
    // for streams
    mutable spin_mutex _stream_mutex;
    std::vector<id<MTLCommandQueue>> _stream_slots;
    std::vector<size_t> _available_stream_slots;
    
    // for textures
    mutable spin_mutex _texture_mutex;
    std::vector<id<MTLTexture>> _texture_slots;
    std::vector<size_t> _available_texture_slots;
    
    // for events
    mutable spin_mutex _event_mutex;
    std::vector<MetalEvent> _event_slots;
    std::vector<size_t> _available_event_slots;

public:
    explicit MetalDevice(const Context &ctx, uint32_t index) noexcept;
    ~MetalDevice() noexcept override;
    [[nodiscard]] id<MTLDevice> handle() const noexcept;
    [[nodiscard]] id<MTLBuffer> buffer(uint64_t handle) const noexcept;
    [[nodiscard]] id<MTLCommandQueue> stream(uint64_t handle) const noexcept;
    [[nodiscard]] id<MTLTexture> texture(uint64_t handle) const noexcept;
    [[nodiscard]] MetalEvent event(uint64_t handle) const noexcept;
    [[nodiscard]] MetalArgumentBufferPool *argument_buffer_pool() const noexcept;
    [[nodiscard]] MetalCompiler::PipelineState kernel(uint32_t uid) const noexcept;

public:
    uint64_t create_texture(PixelFormat format, uint dimension, uint width, uint height, uint depth, uint mipmap_levels, bool is_bindless) override;
    void dispose_texture(uint64_t handle) noexcept override;
    uint64_t create_buffer(size_t size_bytes) noexcept override;
    void dispose_buffer(uint64_t handle) noexcept override;
    uint64_t create_stream() noexcept override;
    void dispose_stream(uint64_t handle) noexcept override;
    void synchronize_stream(uint64_t stream_handle) noexcept override;
    void prepare_kernel(uint32_t uid) noexcept override;
    void dispatch(uint64_t stream_handle, CommandBuffer buffer, std::function<void()> function) noexcept override;
    uint64_t create_event() noexcept override;
    void dispose_event(uint64_t handle) noexcept override;
    void synchronize_event(uint64_t handle) noexcept override;
};

}
