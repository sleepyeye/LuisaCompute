//
// Created by Mike Smith on 2021/6/23.
//

#include <iostream>

#include <runtime/context.h>
#include <runtime/device.h>
#include <runtime/stream.h>
#include <runtime/event.h>
#include <dsl/syntax.h>
#include <tests/fake_device.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tests/stb_image_write.h>

using namespace luisa;
using namespace luisa::compute;

int main(int argc, char *argv[]) {

    log_level_verbose();

    Context context{argv[0]};

#if defined(LUISA_BACKEND_METAL_ENABLED)
    auto device = context.create_device("metal");
#elif defined(LUISA_BACKEND_DX_ENABLED)
    auto device = context.create_device("dx");
#else
    auto device = FakeDevice::create(context);
#endif

    auto buffer = device.create_buffer<uint>(1u);
    Kernel1D count = [&buffer] {
        Var x = buffer.as_atomic(0).fetch_add(1u);
    };
    device.compile(count);

    auto host_buffer = 0u;
    auto stream = device.create_stream();
    stream << count().launch(1024u)
           << buffer.copy_to(&host_buffer);
    stream.synchronize();

    std::cout << "Count: " << host_buffer << std::endl;
}
