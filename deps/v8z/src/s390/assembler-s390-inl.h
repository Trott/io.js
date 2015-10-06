// Copyright (c) 1994-2006 Sun Microsystems Inc.
// All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// - Redistribution in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the
// distribution.
//
// - Neither the name of Sun Microsystems or the names of contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.

// The original source code covered by the above license above has been modified
// significantly by Google Inc.
// Copyright 2012 the V8 project authors. All rights reserved.

//
// Copyright IBM Corp. 2012, 2015. All rights reserved.
//

#ifndef V8_S390_ASSEMBLER_S390_INL_H_
#define V8_S390_ASSEMBLER_S390_INL_H_

#include "src/s390/assembler-s390.h"

#include "src/assembler.h"
#include "src/debug.h"


namespace v8 {
namespace internal {


bool CpuFeatures::SupportsCrankshaft() { return true; }


// The modes possibly affected by apply must be in kApplyMask.
void RelocInfo::apply(intptr_t delta, ICacheFlushMode icache_flush_mode) {
  // Might need to re-implement this once we have BRASL
  // absolute code pointer inside code object moves with the code object.
  if (IsInternalReference(rmode_)) {
    // Jump table entry
    Address target = Memory::Address_at(pc_);
    Memory::Address_at(pc_) = target + delta;
  } else if (IsCodeTarget(rmode_)) {
    bool flush_icache = icache_flush_mode != SKIP_ICACHE_FLUSH;
    SixByteInstr instr = Instruction::InstructionBits(
                                 reinterpret_cast<const byte*>(pc_));
    int32_t dis = static_cast<int32_t>(instr & 0xFFFFFFFF) * 2  // halfwords
                         - static_cast<int32_t>(delta);
    instr >>= 32;
    instr <<= 32;
    instr |= static_cast<uint32_t>(dis/2);
    Instruction::SetInstructionBits<SixByteInstr>(
                         reinterpret_cast<byte*>(pc_), instr);
    if (flush_icache) CpuFeatures::FlushICache(pc_, 6);
  } else {
    // mov sequence
    DCHECK(IsInternalReferenceEncoded(rmode_));
    Address target = Assembler::target_address_at(pc_, host_);
    Assembler::set_target_address_at(pc_, host_, target + delta,
                                     icache_flush_mode);
  }
}


Address RelocInfo::target_internal_reference() {
  if (IsInternalReference(rmode_)) {
    // Jump table entry
    return Memory::Address_at(pc_);
  } else {
    // mov sequence
    DCHECK(IsInternalReferenceEncoded(rmode_));
    return Assembler::target_address_at(pc_, host_);
  }
}


Address RelocInfo::target_internal_reference_address() {
  DCHECK(IsInternalReference(rmode_) || IsInternalReferenceEncoded(rmode_));
  return reinterpret_cast<Address>(pc_);
}


Address RelocInfo::target_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_));
  return Assembler::target_address_at(pc_, host_);
}


Address RelocInfo::target_address_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_) ||
         rmode_ == EMBEDDED_OBJECT || rmode_ == EXTERNAL_REFERENCE);

  // Read the address of the word containing the target_address in an
  // instruction stream.
  // The only architecture-independent user of this function is the serializer.
  // The serializer uses it to find out how many raw bytes of instruction to
  // output before the next target.
  // For an instruction like LIS/ORI where the target bits are mixed into the
  // instruction bits, the size of the target will be zero, indicating that the
  // serializer should not step forward in memory after a target is resolved
  // and written.
  return reinterpret_cast<Address>(pc_);
}


Address RelocInfo::constant_pool_entry_address() {
  UNREACHABLE();
  return NULL;
}


int RelocInfo::target_address_size() { return Assembler::kSpecialTargetSize; }


void RelocInfo::set_target_address(Address target,
                                   WriteBarrierMode write_barrier_mode,
                                   ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_));
  Assembler::set_target_address_at(pc_, host_, target, icache_flush_mode);
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && host() != NULL &&
      IsCodeTarget(rmode_)) {
    Object* target_code = Code::GetCodeFromTargetAddress(target);
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(
        host(), this, HeapObject::cast(target_code));
  }
}


Address Assembler::break_address_from_return_address(Address pc) {
  return target_address_from_return_address(pc);
}


Address Assembler::target_address_from_return_address(Address pc) {
  // Returns the address of the call target from the return address that will
  // be returned to after a call.
  // Sequence is:
  //    BRASL r14, RI
  return pc - kCallTargetAddressOffset;
}


Address Assembler::return_address_from_call_start(Address pc) {
  // Sequence is:
  //    BRASL r14, RI
  return pc + kCallTargetAddressOffset;
}


Handle<Object> Assembler::code_target_object_handle_at(Address pc) {
  SixByteInstr instr = Instruction::InstructionBits(
                           reinterpret_cast<const byte*>(pc));
  int index = instr & 0xFFFFFFFF;
  return code_targets_[index];
}


Object* RelocInfo::target_object() {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return reinterpret_cast<Object*>(Assembler::target_address_at(pc_, host_));
}


Handle<Object> RelocInfo::target_object_handle(Assembler* origin) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  if (rmode_ == EMBEDDED_OBJECT) {
    return Handle<Object>(
        reinterpret_cast<Object**>(Assembler::target_address_at(pc_, host_)));
  } else {
    return origin->code_target_object_handle_at(pc_);
  }
}


void RelocInfo::set_target_object(Object* target,
                                  WriteBarrierMode write_barrier_mode,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  Assembler::set_target_address_at(
      pc_, host_, reinterpret_cast<Address>(target), icache_flush_mode);
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && host() != NULL &&
      target->IsHeapObject()) {
    host()->GetHeap()->incremental_marking()->RecordWrite(
        host(), &Memory::Object_at(pc_), HeapObject::cast(target));
  }
}


Address RelocInfo::target_external_reference() {
  DCHECK(rmode_ == EXTERNAL_REFERENCE);
  return Assembler::target_address_at(pc_, host_);
}


Address RelocInfo::target_runtime_entry(Assembler* origin) {
  DCHECK(IsRuntimeEntry(rmode_));
  return target_address();
}


void RelocInfo::set_target_runtime_entry(Address target,
                                         WriteBarrierMode write_barrier_mode,
                                         ICacheFlushMode icache_flush_mode) {
  DCHECK(IsRuntimeEntry(rmode_));
  if (target_address() != target)
    set_target_address(target, write_barrier_mode, icache_flush_mode);
}


Handle<Cell> RelocInfo::target_cell_handle() {
  DCHECK(rmode_ == RelocInfo::CELL);
  Address address = Memory::Address_at(pc_);
  return Handle<Cell>(reinterpret_cast<Cell**>(address));
}


Cell* RelocInfo::target_cell() {
  DCHECK(rmode_ == RelocInfo::CELL);
  return Cell::FromValueAddress(Memory::Address_at(pc_));
}


void RelocInfo::set_target_cell(Cell* cell, WriteBarrierMode write_barrier_mode,
                                ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::CELL);
  Address address = cell->address() + Cell::kValueOffset;
  Memory::Address_at(pc_) = address;
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && host() != NULL) {
    // TODO(1550) We are passing NULL as a slot because cell can never be on
    // evacuation candidate.
    host()->GetHeap()->incremental_marking()->RecordWrite(host(), NULL, cell);
  }
}

#if V8_TARGET_ARCH_S390X
    // NOP(2byte) + PUSH + MOV + BASR =
    // NOP + LAY + STG + IIHF + IILF + BASR
static const int kCodeAgingSequenceLength = 28;
static const int kCodeAgingTargetDelta = 14;  // Jump past NOP + PUSH to IIHF
    // LAY + 4 * STG + LA
static const int kNoCodeAgeSequenceLength = 34;
#else
#if (V8_HOST_ARCH_S390)
// NOP + NILH + LAY + ST + IILF + BASR
static const int kCodeAgingSequenceLength = 24;
static const int kCodeAgingTargetDelta = 16;  // Jump past NOP to IILF
// NILH + LAY + 4 * ST + LA
static const int kNoCodeAgeSequenceLength = 30;
#else
// NOP + LAY + ST + IILF + BASR
static const int kCodeAgingSequenceLength = 20;
static const int kCodeAgingTargetDelta = 12;  // Jump past NOP to IILF
// LAY + 4 * ST + LA
static const int kNoCodeAgeSequenceLength = 26;
#endif
#endif

Handle<Object> RelocInfo::code_age_stub_handle(Assembler* origin) {
  UNREACHABLE();  // This should never be reached on S390.
  return Handle<Object>();
}


Code* RelocInfo::code_age_stub() {
  DCHECK(rmode_ == RelocInfo::CODE_AGE_SEQUENCE);
  return Code::GetCodeFromTargetAddress(
      Assembler::target_address_at(pc_ + kCodeAgingTargetDelta, host_));
}


void RelocInfo::set_code_age_stub(Code* stub,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::CODE_AGE_SEQUENCE);
  Assembler::set_target_address_at(pc_ + kCodeAgingTargetDelta, host_,
                                   stub->instruction_start(),
                                   icache_flush_mode);
}


Address RelocInfo::call_address() {
  DCHECK((IsJSReturn(rmode()) && IsPatchedReturnSequence()) ||
         (IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence()));
  // The pc_ offset of 0 assumes patched return sequence per
  // BreakLocation::SetDebugBreakAtReturn(), or debug break
  // slot per BreakLocation::SetDebugBreakAtSlot().
  return Assembler::target_address_at(pc_, host_);
}


void RelocInfo::set_call_address(Address target) {
  DCHECK((IsJSReturn(rmode()) && IsPatchedReturnSequence()) ||
         (IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence()));
  Assembler::set_target_address_at(pc_, host_, target);
  if (host() != NULL) {
    Object* target_code = Code::GetCodeFromTargetAddress(target);
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(
        host(), this, HeapObject::cast(target_code));
  }
}


Object* RelocInfo::call_object() { return *call_object_address(); }


void RelocInfo::set_call_object(Object* target) {
  *call_object_address() = target;
}


Object** RelocInfo::call_object_address() {
  DCHECK((IsJSReturn(rmode()) && IsPatchedReturnSequence()) ||
         (IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence()));
  return reinterpret_cast<Object**>(pc_ + 2 * Assembler::kInstrSize);
}


void RelocInfo::WipeOut() {
  DCHECK(IsEmbeddedObject(rmode_) || IsCodeTarget(rmode_) ||
         IsRuntimeEntry(rmode_) || IsExternalReference(rmode_) ||
         IsInternalReference(rmode_) || IsInternalReferenceEncoded(rmode_));
  if (IsInternalReference(rmode_)) {
    // Jump table entry
    Memory::Address_at(pc_) = NULL;
  } else if (IsInternalReferenceEncoded(rmode_)) {
    // mov sequence
    // Currently used only by deserializer, no need to flush.
    Assembler::set_target_address_at(pc_, host_, NULL, SKIP_ICACHE_FLUSH);
  } else {
    Assembler::set_target_address_at(pc_, host_, NULL);
  }
}


bool RelocInfo::IsPatchedReturnSequence() {
  //
  // The patched return sequence is defined by
  // BreakLocation::SetDebugBreakAtReturn()
  // FIXED_SEQUENCE

  bool patched_return = true;
#if V8_TARGET_ARCH_S390X
  Opcode instr0 =
      Instruction::S390OpcodeValue(reinterpret_cast<const byte*>(pc_));
  Opcode instr1 =
      Instruction::S390OpcodeValue(reinterpret_cast<const byte*>(pc_+6));
  Opcode basr =
      Instruction::S390OpcodeValue(reinterpret_cast<const byte*>(pc_ + 12));
  Opcode bkpt =
      Instruction::S390OpcodeValue(reinterpret_cast<const byte*>(pc_ + 14));
  patched_return = (IIHF == instr0);
#else
  Opcode instr1 =
      Instruction::S390OpcodeValue(reinterpret_cast<const byte*>(pc_));
  Opcode basr =
      Instruction::S390OpcodeValue(reinterpret_cast<const byte*>(pc_ + 6));
  Opcode bkpt =
      Instruction::S390OpcodeValue(reinterpret_cast<const byte*>(pc_ + 8));
#endif
  patched_return =
      patched_return && (IILF == instr1) && (BASR == basr) && (BKPT == bkpt);

  return patched_return;
}


bool RelocInfo::IsPatchedDebugBreakSlotSequence() {
  SixByteInstr current_instr = Assembler::instr_at(pc_);
  return !Assembler::IsNop(current_instr, Assembler::DEBUG_BREAK_NOP);
}


void RelocInfo::Visit(Isolate* isolate, ObjectVisitor* visitor) {
  RelocInfo::Mode mode = rmode();
  if (mode == RelocInfo::EMBEDDED_OBJECT) {
    visitor->VisitEmbeddedPointer(this);
  } else if (RelocInfo::IsCodeTarget(mode)) {
    visitor->VisitCodeTarget(this);
  } else if (mode == RelocInfo::CELL) {
    visitor->VisitCell(this);
  } else if (mode == RelocInfo::EXTERNAL_REFERENCE) {
    visitor->VisitExternalReference(this);
  } else if (mode == RelocInfo::INTERNAL_REFERENCE ||
             mode == RelocInfo::INTERNAL_REFERENCE_ENCODED) {
    visitor->VisitInternalReference(this);
  } else if (RelocInfo::IsCodeAgeSequence(mode)) {
    visitor->VisitCodeAgeSequence(this);
  } else if (((RelocInfo::IsJSReturn(mode) && IsPatchedReturnSequence()) ||
              (RelocInfo::IsDebugBreakSlot(mode) &&
               IsPatchedDebugBreakSlotSequence())) &&
             isolate->debug()->has_break_points()) {
    visitor->VisitDebugTarget(this);
  } else if (IsRuntimeEntry(mode)) {
    visitor->VisitRuntimeEntry(this);
  }
}


template <typename StaticVisitor>
void RelocInfo::Visit(Heap* heap) {
  RelocInfo::Mode mode = rmode();
  if (mode == RelocInfo::EMBEDDED_OBJECT) {
    StaticVisitor::VisitEmbeddedPointer(heap, this);
  } else if (RelocInfo::IsCodeTarget(mode)) {
    StaticVisitor::VisitCodeTarget(heap, this);
  } else if (mode == RelocInfo::CELL) {
    StaticVisitor::VisitCell(heap, this);
  } else if (mode == RelocInfo::EXTERNAL_REFERENCE) {
    StaticVisitor::VisitExternalReference(this);
  } else if (mode == RelocInfo::INTERNAL_REFERENCE ||
             mode == RelocInfo::INTERNAL_REFERENCE_ENCODED) {
    StaticVisitor::VisitInternalReference(this);
  } else if (RelocInfo::IsCodeAgeSequence(mode)) {
    StaticVisitor::VisitCodeAgeSequence(heap, this);
  } else if (heap->isolate()->debug()->has_break_points() &&
             ((RelocInfo::IsJSReturn(mode) && IsPatchedReturnSequence()) ||
              (RelocInfo::IsDebugBreakSlot(mode) &&
               IsPatchedDebugBreakSlotSequence()))) {
    StaticVisitor::VisitDebugTarget(heap, this);
  } else if (IsRuntimeEntry(mode)) {
    StaticVisitor::VisitRuntimeEntry(this);
  }
}

// Operand constructors
Operand::Operand(intptr_t immediate, RelocInfo::Mode rmode) {
  rm_ = no_reg;
  imm_ = immediate;
  rmode_ = rmode;
}

Operand::Operand(const ExternalReference& f) {
  rm_ = no_reg;
  imm_ = reinterpret_cast<intptr_t>(f.address());
  rmode_ = RelocInfo::EXTERNAL_REFERENCE;
}

Operand::Operand(Smi* value) {
  rm_ = no_reg;
  imm_ = reinterpret_cast<intptr_t>(value);
  rmode_ = kRelocInfo_NONEPTR;
}

Operand::Operand(Register rm) {
  rm_ = rm;
  rmode_ = kRelocInfo_NONEPTR;  // S390 -why doesn't ARM do this?
}

void Assembler::CheckBuffer() {
  if (buffer_space() <= kGap) {
    GrowBuffer();
  }
}

void Assembler::CheckTrampolinePoolQuick() {
  if (pc_offset() >= next_buffer_check_) {
    CheckTrampolinePool();
  }
}
int32_t Assembler::emit_code_target(Handle<Code> target,
                                    RelocInfo::Mode rmode,
                                    TypeFeedbackId ast_id) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  if (rmode == RelocInfo::CODE_TARGET && !ast_id.IsNone()) {
    SetRecordedAstId(ast_id);
    RecordRelocInfo(RelocInfo::CODE_TARGET_WITH_ID);
  } else {
    RecordRelocInfo(rmode);
  }

  int current = code_targets_.length();
  if (current > 0 && code_targets_.last().is_identical_to(target)) {
    // Optimization if we keep jumping to the same code target.
    current--;
  } else {
    code_targets_.Add(target);
  }
  return current;
}

// S390 specific emitting helpers
void Assembler::emit2bytes(uint16_t x) {
    CheckBuffer();
#if V8_TARGET_LITTLE_ENDIAN
    // We need to emit instructions in big endian format as disassembler /
    // simulator require the first byte of the instruction in order to decode
    // the instruction length.  Swap the bytes.
    x = ((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8);
#endif
    *reinterpret_cast<uint16_t*>(pc_) = x;
    pc_ += 2;
    CheckTrampolinePoolQuick();
}

void Assembler::emit4bytes(uint32_t x) {
    CheckBuffer();
#if V8_TARGET_LITTLE_ENDIAN
    // We need to emit instructions in big endian format as disassembler /
    // simulator require the first byte of the instruction in order to decode
    // the instruction length.  Swap the bytes.
    x = ((x & 0x000000FF) << 24) | ((x & 0x0000FF00) << 8) |
        ((x & 0x00FF0000) >>  8) | ((x & 0xFF000000) >> 24);
#endif
    *reinterpret_cast<uint32_t*>(pc_) = x;
    pc_ += 4;
    CheckTrampolinePoolQuick();
}

void Assembler::emit6bytes(uint64_t x) {
    CheckBuffer();
#if V8_TARGET_LITTLE_ENDIAN
    // We need to emit instructions in big endian format as disassembler /
    // simulator require the first byte of the instruction in order to decode
    // the instruction length.  Swap the bytes.
    x = (static_cast<uint64_t>(x & 0xFF) << 40) |
        (static_cast<uint64_t>((x >>  8) & 0xFF) << 32) |
        (static_cast<uint64_t>((x >> 16) & 0xFF) << 24) |
        (static_cast<uint64_t>((x >> 24) & 0xFF) << 16) |
        (static_cast<uint64_t>((x >> 32) & 0xFF) << 8) |
        (static_cast<uint64_t>((x >> 40) & 0xFF));
    x |= (*reinterpret_cast<uint64_t*>(pc_) >> 48) << 48;
#else
    // We need to pad two bytes of zeros in order to get the 6-bytes
    // stored from low address.
    x = x << 16;
    x |= *reinterpret_cast<uint64_t*>(pc_) & 0xFFFF;
#endif
    // It is safe to store 8-bytes, as CheckBuffer() guarantees we have kGap
    // space left over.
    *reinterpret_cast<uint64_t*>(pc_) = x;
    pc_ += 6;
}
// end of S390 specific emitting helpers

bool Operand::is_reg() const { return rm_.is_valid(); }


// Fetch the 32bit value from the FIXED_SEQUENCE IIHF / IILF
Address Assembler::target_address_at(Address pc, Address constant_pool) {
  // S390 Instruction!
  // We want to check for instructions generated by Asm::mov()
  Opcode op1 = Instruction::S390OpcodeValue(reinterpret_cast<const byte*>(pc));
  SixByteInstr instr_1 = Instruction::InstructionBits(
                                        reinterpret_cast<const byte*>(pc));

  if (BRASL == op1 || BRCL == op1) {
    int32_t dis = static_cast<int32_t>(instr_1 & 0xFFFFFFFF) * 2;
    return reinterpret_cast<Address>(reinterpret_cast<uint64_t>(pc) + dis);
  }

#if V8_TARGET_ARCH_S390X
  int instr1_length = Instruction::InstructionLength(
                                      reinterpret_cast<const byte*>(pc));
  Opcode op2 = Instruction::S390OpcodeValue(
                        reinterpret_cast<const byte*>(pc + instr1_length));
  SixByteInstr instr_2 = Instruction::InstructionBits(
                        reinterpret_cast<const byte*>(pc + instr1_length));
  // IIHF for hi_32, IILF for lo_32
  if (IIHF == op1 && IILF == op2) {
     return reinterpret_cast<Address>(((instr_1 & 0xFFFFFFFF) << 32) |
                                      ((instr_2 & 0xFFFFFFFF)));
  }
#else
  // IILF loads 32-bits
  if (IILF == op1 || CFI == op1) {
     return reinterpret_cast<Address>((instr_1 & 0xFFFFFFFF));
  }
#endif

  UNIMPLEMENTED();
  return (Address)0;
}

// This sets the branch destination (which gets loaded at the call address).
// This is for calls and branches within generated code.  The serializer
// has already deserialized the mov instructions etc.
// There is a FIXED_SEQUENCE assumption here
void Assembler::deserialization_set_special_target_at(
    Address instruction_payload, Code* code, Address target) {
  set_target_address_at(instruction_payload, code, target);
}


void Assembler::deserialization_set_target_internal_reference_at(
    Address pc, Address target, RelocInfo::Mode mode) {
  if (RelocInfo::IsInternalReferenceEncoded(mode)) {
    Code* code = NULL;
    set_target_address_at(pc, code, target, SKIP_ICACHE_FLUSH);
  } else {
    Memory::Address_at(pc) = target;
  }
}


// This code assumes the FIXED_SEQUENCE of IIHF/IILF
void Assembler::set_target_address_at(Address pc,
                                      Address constant_pool,
                                      Address target,
                                      ICacheFlushMode icache_flush_mode) {
  // S390 Instruction!
  // We want to check for instructions generated by Asm::mov()
  Opcode op1 = Instruction::S390OpcodeValue(reinterpret_cast<const byte*>(pc));
  SixByteInstr instr_1 = Instruction::InstructionBits(
                                            reinterpret_cast<const byte*>(pc));
  bool patched = false;

  if (BRASL == op1 || BRCL == op1) {
    instr_1 >>= 32;  // Zero out the lower 32-bits
    instr_1 <<= 32;
    int32_t halfwords = (target - pc)/2;  // number of halfwords
    instr_1 |= static_cast<uint32_t>(halfwords);
    Instruction::SetInstructionBits<SixByteInstr>(
                                     reinterpret_cast<byte*>(pc), instr_1);
    if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
      CpuFeatures::FlushICache(pc, 6);
    }
    patched = true;
  } else {
#if V8_TARGET_ARCH_S390X
    int instr1_length = Instruction::InstructionLength(
                                            reinterpret_cast<const byte*>(pc));
    Opcode op2 = Instruction::S390OpcodeValue(
                            reinterpret_cast<const byte*>(pc + instr1_length));
    SixByteInstr instr_2 = Instruction::InstructionBits(
                            reinterpret_cast<const byte*>(pc + instr1_length));
    // IIHF for hi_32, IILF for lo_32
    if (IIHF == op1 && IILF == op2) {
      // IIHF
      instr_1 >>= 32;  // Zero out the lower 32-bits
      instr_1 <<= 32;
      instr_1 |= reinterpret_cast<uint64_t>(target) >> 32;

      Instruction::SetInstructionBits<SixByteInstr>(
                                      reinterpret_cast<byte*>(pc), instr_1);

      // IILF
      instr_2 >>= 32;
      instr_2 <<= 32;
      instr_2 |= reinterpret_cast<uint64_t>(target) & 0xFFFFFFFF;

      Instruction::SetInstructionBits<SixByteInstr>(
                      reinterpret_cast<byte*>(pc + instr1_length), instr_2);
      if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
        CpuFeatures::FlushICache(pc, 12);
      }
      patched = true;
    }
#else
    // IILF loads 32-bits
    if (IILF == op1 || CFI == op1) {
      instr_1 >>= 32;  // Zero out the lower 32-bits
      instr_1 <<= 32;
      instr_1 |= reinterpret_cast<uint32_t>(target);

      Instruction::SetInstructionBits<SixByteInstr>(
                                     reinterpret_cast<byte*>(pc), instr_1);
      if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
        CpuFeatures::FlushICache(pc, 6);
      }
      patched = true;
    }
#endif
  }
  if (!patched)
    UNREACHABLE();
}
}
}  // namespace v8::internal

#endif  // V8_S390_ASSEMBLER_S390_INL_H_
