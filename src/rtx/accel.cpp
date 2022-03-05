//
// Created by Mike Smith on 2021/6/24.
//

#include <ast/function_builder.h>
#include <runtime/shader.h>
#include <rtx/accel.h>

namespace luisa::compute {

namespace detail {

ShaderInvokeBase &ShaderInvokeBase::operator<<(const Accel &accel) noexcept {
    _encode_pending_bindings();
    if (auto t = _kernel.arguments()[_argument_index].type();
        !t->is_accel()) {
        LUISA_ERROR_WITH_LOCATION(
            "Expected {} but got accel for argument {}.",
            t->description(), _argument_index);
    }
    auto v = _kernel.arguments()[_argument_index++].uid();
    _dispatch_command()->encode_accel(v, accel.handle());
    return *this;
}

}// namespace detail

Accel Device::create_accel(AccelBuildHint hint) noexcept { return _create<Accel>(hint); }

Accel::Accel(Device::Interface *device, AccelBuildHint hint) noexcept
    : Resource{device, Resource::Tag::ACCEL, device->create_accel(hint)},
      _rebuild_observer{luisa::make_unique<RebuildObserver>()} {}

Command *Accel::update() noexcept {
    if (_rebuild_observer->requires_rebuild()) [[unlikely]] {
        LUISA_WARNING_WITH_LOCATION(
            "Accel #{} requires rebuild rather than update. "
            "Automatically replacing with AccelBuildCommand.",
            handle());
        return build();
    }
    return AccelUpdateCommand::create(handle());
}

Var<Hit> Accel::trace_closest(Expr<Ray> ray) const noexcept {
    return Expr<Accel>{*this}.trace_closest(ray);
}

Var<bool> Accel::trace_any(Expr<Ray> ray) const noexcept {
    return Expr<Accel>{*this}.trace_any(ray);
}

Var<float4x4> Accel::instance_to_world(Expr<int> instance_id) const noexcept {
    return Expr<Accel>{*this}.instance_to_world(instance_id);
}

Var<float4x4> Accel::instance_to_world(Expr<uint> instance_id) const noexcept {
    return Expr<Accel>{*this}.instance_to_world(instance_id);
}
void Accel::set_transform(Expr<int> instance_id, Expr<float4x4> mat) const noexcept {
    Expr<Accel>{*this}.set_transform(instance_id, mat);
}
void Accel::set_transform(Expr<uint> instance_id, Expr<float4x4> mat) const noexcept {
    Expr<Accel>{*this}.set_transform(instance_id, mat);
}
void Accel::set_transform_vis(Expr<int> instance_id, Expr<float4x4> mat, Expr<bool> vis) const noexcept {
    Expr<Accel>{*this}.set_transform_vis(instance_id, mat, vis);
}
void Accel::set_transform_vis(Expr<uint> instance_id, Expr<float4x4> mat, Expr<bool> vis) const noexcept {
    Expr<Accel>{*this}.set_transform_vis(instance_id, mat, vis);
}
void Accel::set_vis(Expr<int> instance_id, Expr<bool> vis) const noexcept {
    Expr<Accel>{*this}.set_vis(instance_id, vis);
}
void Accel::set_vis(Expr<uint> instance_id, Expr<bool> vis) const noexcept {
    Expr<Accel>{*this}.set_vis(instance_id, vis);
}
Accel &Accel::emplace_back(
    Mesh const &mesh,
    float4x4 transform,
    bool visible) noexcept {
    _rebuild_observer->notify();
    device()->emplace_back_instance_in_accel(handle(), mesh.handle(), transform, visible);
    _rebuild_observer->emplace_back(mesh.shared_subject());
    return *this;
}

Command *Accel::build() noexcept {
    _rebuild_observer->clear();
    return AccelBuildCommand::create(handle());
}

Accel &Accel::pop_back() noexcept {
    _rebuild_observer->notify();
    _rebuild_observer->pop_back();
    device()->pop_back_instance_from_accel(handle());
    return *this;
}

Accel& Accel::set_mesh(size_t index, const Mesh& mesh) noexcept {
    if (index >= size()) [[unlikely]] {
        LUISA_ERROR_WITH_LOCATION(
            "Invalid index {} in accel #{}.",
            index, handle());
    }
    _rebuild_observer->notify();
    _rebuild_observer->set(index, mesh.shared_subject());
    device()->set_instance_mesh_in_accel(handle(), index, mesh.handle());
    return *this;
}
}// namespace luisa::compute
