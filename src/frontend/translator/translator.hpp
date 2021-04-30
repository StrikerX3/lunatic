/*
 * Copyright (C) 2021 fleroviux
 */

#pragma once

#include <lunatic/memory.hpp>

#include "frontend/decode/arm.hpp"
#include "frontend/basic_block.hpp"

namespace lunatic {
namespace frontend {

struct Translator final : ARMDecodeClient<bool> {
  auto Translate(BasicBlock& block, Memory& memory) -> bool;

  auto Handle(ARMDataProcessing const& opcode) -> bool override;
  auto Undefined(u32 opcode) -> bool override;

  void EmitUpdateNZCV();

  Mode mode;
  IREmitter* emitter = nullptr;
};

} // namespace lunatic::frontend
} // namespace lunatic
