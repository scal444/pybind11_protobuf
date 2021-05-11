// Copyright (c) 2019 The Pybind Development Team. All rights reserved.
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#ifndef THIRD_PARTY_PYBIND11_PROTOBUF_PROTO_CASTERS_H_
#define THIRD_PARTY_PYBIND11_PROTOBUF_PROTO_CASTERS_H_

#include <pybind11/cast.h>
#include <pybind11/pybind11.h>

#include <memory>
#include <stdexcept>
#include <type_traits>

#include "google/protobuf/message.h"
#include "third_party/pybind11_protobuf/proto_utils.h"

namespace pybind11 {
namespace google {

#define PYBIND11_PROTOBUF_MODULE_PATH third_party.pybind11_protobuf.proto

// Imports the bindings for the proto base types. This not thread safe and
// should only be called from a PYBIND11_MODULE definition.
inline module ImportProtoModule() {
  auto m = reinterpret_borrow<module>(
      PyImport_AddModule(PYBIND11_TOSTRING(PYBIND11_PROTOBUF_MODULE_PATH)));
  if (!IsProtoModuleImported()) RegisterProtoBindings(m);
  // else no-op because bindings are already loaded.
  return m;
}

// Registers the given concrete ProtoType with pybind11.
template <typename ProtoType>
void RegisterProtoMessageType(module m = module()) {
  CheckProtoModuleImported();
  // Drop the return value from ConcreteProtoMessageBindings.
  ConcreteProtoMessageBindings<ProtoType>(m);
}

}  // namespace google

// Specialize polymorphic_type_hook for proto message types.
// If ProtoType is a derived type (ie, not ::google::protobuf::Message), this registers
// it and adds a constructor and concrete fields, to avoid the need to call
// FindFieldByName for every field access.
template <typename ProtoType>
struct polymorphic_type_hook<ProtoType,
                             std::enable_if_t<google::is_proto_v<ProtoType>>> {
  static const void *get(const ProtoType *src, const std::type_info *&type) {
    google::CheckProtoModuleImported();

    // Use RTTI to get the concrete message type.
    const void *out = polymorphic_type_hook_base<ProtoType>::get(src, type);
    if (!out) return nullptr;

    if (!detail::get_type_info(*type)) {
      // Concrete message type is not registered, so cast as a ::google::protobuf::Message.
      out = static_cast<const ::google::protobuf::Message *>(src);
      type = &typeid(::google::protobuf::Message);
    }

    return out;
  }
};

namespace detail {

// Pybind11 type_caster to enable automatic wrapping and/or converting
// protocol messages with pybind11.
template <typename ProtoType>
struct type_caster<ProtoType, std::enable_if_t<google::is_proto_v<ProtoType>>>
    : public type_caster_base<ProtoType> {
  using IntrinsicProtoType = intrinsic_t<ProtoType>;

 public:
  // Convert Python->C++.
  bool load(handle src, bool convert) {
    google::CheckProtoModuleImported();

    if (!google::PyProtoCheckType<IntrinsicProtoType>(src)) return false;

    if (google::IsWrappedCProto(src)) {  // Just remove the wrapper.
      // Concrete ProtoType may not be registered, so load as a ::google::protobuf::Message.
      type_caster_base<::google::protobuf::Message> base_caster;
      if (!base_caster.load(src, convert))
        throw type_error(
            "Proto message passed type checks yet failed to be loaded as a "
            "::google::protobuf::Message base class. This should not be possible.");
      // Since we already checked the type, static cast is safe.
      type_caster_base<ProtoType>::value =
          static_cast<ProtoType *>(static_cast<::google::protobuf::Message *>(base_caster));
      return true;
    }

    // This is not a wrapped C proto and we are not allowed to do conversion.
    if (!convert) return false;

    // Convert this proto from a native python proto.
    owned_value_ =
        google::PyProtoAllocateAndCopyMessage<IntrinsicProtoType>(src);
    type_caster_base<ProtoType>::value = owned_value_.get();
    return true;
  }

 private:
  std::unique_ptr<::google::protobuf::Message> owned_value_;
};

// copybara:strip_begin(core pybind11 patch required)
// A specialization of move_only_holder_caster in proto_utils.h converts
// std::unique_ptrs to protobuffers into std::shared_ptrs automatically.
// copybara:strip_end

}  // namespace detail
}  // namespace pybind11

#endif  // THIRD_PARTY_PYBIND11_PROTOBUF_PROTO_CASTERS_H_
