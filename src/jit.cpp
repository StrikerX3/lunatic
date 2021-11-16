/*
 * Copyright (C) 2021 fleroviux
 */

#include <lunatic/cpu.hpp>

#include "frontend/state.hpp"
#include "frontend/translator/translator.hpp"
#include "backend/x86_64/backend.hpp"

using namespace lunatic::frontend;
using namespace lunatic::backend;

namespace lunatic {

struct JIT final : CPU {
  JIT(CPU::Descriptor const& descriptor)
      : exception_base(descriptor.exception_base)
      , memory(descriptor.memory)
      , translator(descriptor)
      , backend(descriptor, state, block_cache, irq_line) {
  }

  void Reset() override {
    irq_line = false;
    wait_for_irq = false;
    cycles_to_run = 0;
    state.Reset();
    block_cache.Flush();
  }

  bool& IRQLine() override {
    return irq_line;
  }

  void WaitForIRQ() override {
    wait_for_irq = true;
  }

  void CancelIRQWait() override {
    wait_for_irq = false;
  }

  auto IsWaitingForIRQ() -> bool override {
    return wait_for_irq;
  }

  void ClearICache() override {
    block_cache.Flush();
  }

  void ClearICacheRange(u32 address_lo, u32 address_hi) override {
    block_cache.Flush(address_lo, address_hi);
  }

  int Run(int cycles) override {
    if (IsWaitingForIRQ() && !IRQLine()) {
      return 0;
    }

    cycles_to_run += cycles;
    int start_cycles = cycles_to_run;

    while (cycles_to_run > 0) {
      if (IRQLine()) {
        SignalIRQ();
      }

      auto block_key = BasicBlock::Key{state};
      auto basic_block = block_cache.Get(block_key);

      if (basic_block == nullptr) {
        basic_block = Compile(block_key, 0);
      }

      cycles_to_run = backend.Call(*basic_block, cycles_to_run);

      if (IsWaitingForIRQ()) {
        int cycles_executed = start_cycles - cycles_to_run;
        cycles_to_run = 0;
        return cycles_executed;
      }
    }
    return start_cycles - cycles_to_run;
  }

  auto GetGPR(GPR reg) const -> u32 override {
    return GetGPR(reg, GetCPSR().f.mode);
  }

  auto GetGPR(GPR reg, Mode mode) const -> u32 override {
    return const_cast<JIT*>(this)->GetGPR(reg, mode);
  }

  auto GetCPSR() const -> StatusRegister override {
    return const_cast<JIT*>(this)->GetCPSR();
  }

  auto GetSPSR(Mode mode) const -> StatusRegister override {
    return const_cast<JIT*>(this)->GetSPSR(mode);
  }

  void SetGPR(GPR reg, u32 value) override {
    SetGPR(reg, state.GetCPSR().f.mode, value);
  }

  void SetGPR(GPR reg, Mode mode, u32 value) override {
    state.GetGPR(mode, reg) = value;

    if (reg == GPR::PC) {
      if (GetCPSR().f.thumb) {
        state.GetGPR(mode, GPR::PC) += sizeof(u16) * 2;
      } else {
        state.GetGPR(mode, GPR::PC) += sizeof(u32) * 2;
      }
    }
  }

  void SetCPSR(StatusRegister value) override {
    state.GetCPSR() = value;
  }

  void SetSPSR(Mode mode, StatusRegister value) override {
    *state.GetPointerToSPSR(mode) = value;
  }

private:
  auto Compile(BasicBlock::Key block_key, int depth) -> BasicBlock* {
    auto basic_block = new BasicBlock{block_key};

    translator.Translate(*basic_block);
    for (auto &micro_block : basic_block->micro_blocks) {
      micro_block.emitter.Optimize();
    }

    if (depth <= 8) {
      auto branch_target_key = basic_block->branch_target.key;
      if (branch_target_key.value != 0 && !block_cache.Get(branch_target_key)) {
        Compile(branch_target_key, ++depth);
      }
    }

    backend.Compile(*basic_block);
    block_cache.Set(block_key, basic_block);
    return basic_block;
  }

  void SignalIRQ() {
    auto& cpsr = GetCPSR();

    wait_for_irq = false;

    if (!cpsr.f.mask_irq) {
      GetSPSR(Mode::IRQ) = cpsr;

      cpsr.f.mode = Mode::IRQ;
      cpsr.f.mask_irq = 1;
      if (cpsr.f.thumb) {
        GetGPR(GPR::LR) = GetGPR(GPR::PC);
      } else {
        GetGPR(GPR::LR) = GetGPR(GPR::PC) - 4;
      }
      cpsr.f.thumb = 0;

      GetGPR(GPR::PC) = exception_base + 0x18 + sizeof(u32) * 2;
    }
  }

  auto GetGPR(GPR reg) -> u32& {
    return GetGPR(reg, GetCPSR().f.mode);
  }

  auto GetGPR(GPR reg, Mode mode) -> u32& {
    return state.GetGPR(mode, reg);
  }

  auto GetCPSR() -> StatusRegister& {
    return state.GetCPSR();
  }

  auto GetSPSR(Mode mode) -> StatusRegister& {
    return *state.GetPointerToSPSR(mode);
  }

  bool irq_line = false;
  bool wait_for_irq = false;
  int cycles_to_run = 0;
  u32 exception_base;
  Memory& memory;
  State state;
  Translator translator;
  BasicBlockCache block_cache;
  X64Backend backend;
};

auto CreateCPU(CPU::Descriptor const& descriptor) -> std::unique_ptr<CPU> {
  return std::make_unique<JIT>(descriptor);
}

} // namespace lunatic
