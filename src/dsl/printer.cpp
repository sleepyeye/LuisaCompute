//
// Created by Mike Smith on 2022/2/13.
//

#include <runtime/device.h>
#include <runtime/stream.h>
#include <dsl/printer.h>

namespace luisa::compute {

Printer::Printer(Device &device, size_t capacity) noexcept
    : _buffer{device.create_buffer<uint>(next_pow2(capacity) + 1u)},
      _host_buffer(next_pow2(capacity) + 1u) {
    std::iota(_host_buffer.begin(), _host_buffer.end(), 0u);
}

void Printer::reset(Stream &stream) noexcept {
    _reset(stream);
}

void Printer::reset(CommandBuffer &command_buffer) noexcept {
    _reset(command_buffer);
}

template<class T>
void Printer::_reset(T &stream) noexcept {
    uint zero = 0u;
    stream << _buffer.view(_buffer.size() - 1u, 1u).copy_from(&zero);
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, CommandBuffer>) {
        stream << commit();
    }
    _reset_called = true;
}

luisa::string_view Printer::retrieve(CommandBuffer &command_buffer) noexcept {
    return _retrieve(command_buffer);
}

luisa::string_view Printer::retrieve(Stream &stream) noexcept {
    return _retrieve(stream);
}

template<class T>
luisa::string_view Printer::_retrieve(T &stream) noexcept {
    if (!_reset_called) [[unlikely]] {
        LUISA_ERROR_WITH_LOCATION(
            "Printer results cannot be "
            "retrieved if never reset.");
    }
    stream << _buffer.copy_to(_host_buffer.data());
    _reset(stream);
    stream << synchronize();
    auto size = std::min(
        static_cast<uint>(_buffer.size() - 1u),
        _host_buffer.back());
    _scratch.clear();
    for (auto offset = 0u; offset < size;) {
        auto desc_id = _host_buffer[offset++];
        auto desc = _descriptors[desc_id];
        if (offset + desc.size() > size) {
            break;
        }
        for (auto &&tag : desc) {
            auto record = _host_buffer[offset++];
            switch (tag) {
                case Descriptor::Tag::INT:
                    _scratch.append(luisa::format(
                        "{}", static_cast<int>(record)));
                    break;
                case Descriptor::Tag::UINT:
                    _scratch.append(luisa::format(
                        "{}", record));
                    break;
                case Descriptor::Tag::FLOAT:
                    _scratch.append(luisa::format(
                        "{}", luisa::bit_cast<float>(record)));
                    break;
                case Descriptor::Tag::BOOL:
                    _scratch.append(luisa::format(
                        "{}", static_cast<bool>(record)));
                    break;
                case Descriptor::Tag::STRING:
                    _scratch.append(_strings[record]);
                    break;
            }
        }
        _scratch.append("\n");
    }
    return _scratch;
}

}// namespace luisa::compute
