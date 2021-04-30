/*
 * Copyright (C) 2021 fleroviux
 */

#include "translator.hpp"

namespace lunatic {
namespace frontend {

auto Translator::Translate(BasicBlock& block, Memory& memory) -> bool {
  auto address = block.key.field.address;

  if (address & 1) {
    // Thumb mode is not supported right now.
    return false;
  }

  mode = block.key.field.mode;
  emitter = &block.emitter;

  auto instruction = memory.FastRead<u32, Memory::Bus::Code>(address);

  return decode_arm(instruction, *this);
}

auto Translator::Undefined(u32 opcode) -> bool {
  return false;
}

void Translator::EmitUpdateNZCV() {
  auto& cpsr_in  = emitter->CreateVar(IRDataType::UInt32, "cpsr_in");
  auto& cpsr_out = emitter->CreateVar(IRDataType::UInt32, "cpsr_out");

  emitter->LoadCPSR(cpsr_in);
  emitter->UpdateNZCV(cpsr_out, cpsr_in);
  emitter->StoreCPSR(cpsr_out);
}

} // namespace lunatic::frontend
} // namespace lunatic
