////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2016, Advanced Micro Devices, Inc. All rights reserved.
//
// Developed by:
//
//                 AMD Research and AMD HSA Software Development
//
//                 Advanced Micro Devices, Inc.
//
//                 www.amd.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
//  - Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimers.
//  - Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimers in
//    the documentation and/or other materials provided with the distribution.
//  - Neither the names of Advanced Micro Devices, Inc,
//    nor the names of its contributors may be used to endorse or promote
//    products derived from this Software without specific prior written
//    permission.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

#include <sstream>
#include <cassert>
#include "hsa.h"
#include <string>
#include <cstring>
#include <fstream>
#include <cstdlib>
#include <iostream>

namespace amd {
namespace dispatch {

class Buffer {
private:
  size_t size;
  void *local_ptr, *system_ptr;

public:
  Buffer(size_t size_, void *local_ptr_, void *system_ptr_)
    : size(size_), local_ptr(local_ptr_), system_ptr(system_ptr_) { }
  Buffer(size_t size_, void *system_ptr_)
    : size(size_), local_ptr(system_ptr_), system_ptr(system_ptr_) { }
  void *LocalPtr() const { return local_ptr; }
  void *SystemPtr() { return system_ptr; }
  template <typename T>
  T* Ptr() { return (T*) system_ptr; }
  template <typename T>
  const T& Data(size_t i) const { return ((const T*) system_ptr)[i]; }
  template <typename T>
  T& Data(size_t i) { return ((T*) system_ptr)[i]; }
  bool IsLocal() const { return local_ptr != system_ptr; }
  size_t Size() const { return size; }
};

class Dispatch {
private:
  hsa_agent_t agent;
  hsa_agent_t cpu_agent;
  uint32_t queue_size;
  hsa_queue_t* queue;
  hsa_signal_t signal;
  hsa_region_t system_region;
  hsa_region_t kernarg_region;
  hsa_region_t local_region;
  hsa_kernel_dispatch_packet_t* aql;
  uint64_t packet_index;
  void *kernarg;
  size_t kernarg_offset;
  hsa_code_object_t code_object;
  hsa_executable_t executable;

  bool Init();
  bool InitDispatch();
  bool RunDispatch();
  bool Wait();

protected:
  std::ostringstream output;
  bool Error(const char *msg);
  bool HsaError(const char *msg, hsa_status_t status = HSA_STATUS_SUCCESS);

public:
  Dispatch(int argc, const char** argv);

  void SetAgent(hsa_agent_t agent) { assert(!this->agent.handle); this->agent = agent; }
  bool HasAgent() const { return agent.handle != 0; }
  void SetCpuAgent(hsa_agent_t agent) { assert(!this->cpu_agent.handle); this->cpu_agent = agent; }
  bool HasCpuAgent() const { return cpu_agent.handle != 0; }
  void SetWorkgroupSize(uint16_t sizeX, uint16_t sizeY = 1, uint16_t sizeZ = 1);
  void SetGridSize(uint32_t sizeX, uint32_t sizeY = 1, uint32_t sizeZ = 1);
  void SetSystemRegion(hsa_region_t region);
  void SetKernargRegion(hsa_region_t region);
  void SetLocalRegion(hsa_region_t region);
  bool AllocateKernarg(uint32_t size);
  bool Run();
  int RunMain();
  virtual bool SetupExecutable();
  virtual bool SetupCodeObject();
  bool LoadCodeObjectFromFile(const std::string& filename);
  void* AllocateLocalMemory(size_t size);
  void* AllocateSystemMemory(size_t size);
  bool CopyToLocal(void* dest, void* src, size_t size);
  bool CopyFromLocal(void* dest, void* src, size_t size);
  Buffer* AllocateBuffer(size_t size);
  bool CopyTo(Buffer* buffer);
  bool CopyFrom(Buffer* buffer);
  virtual bool Setup() { return true; }
  virtual bool Verify() { return true; }
  void KernargRaw(const void* ptr, size_t size, size_t align);

  template <typename T>
  void Kernarg(const T* ptr, size_t size = sizeof(T), size_t align = sizeof(T)) {
    KernargRaw(ptr, size, align);
  }

  void Kernarg(Buffer* buffer);
  uint64_t GetTimestampFrequency();
};


Dispatch::Dispatch(int argc, const char** argv)
  : queue_size(0),
    queue(0)
{
  agent.handle = 0;
  cpu_agent.handle = 0;
  signal.handle = 0;
  kernarg_region.handle = 0;
  system_region.handle = 0;
  local_region.handle = 0;
}

hsa_status_t find_gpu_device(hsa_agent_t agent, void *data)
{
  if (data == NULL) { return HSA_STATUS_ERROR_INVALID_ARGUMENT; }

  hsa_device_type_t hsa_device_type;
  hsa_status_t hsa_error_code = hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &hsa_device_type);
  if (hsa_error_code != HSA_STATUS_SUCCESS) { return hsa_error_code; }

  if (hsa_device_type == HSA_DEVICE_TYPE_GPU) {
    Dispatch* dispatch = static_cast<Dispatch*>(data);
    if (!dispatch->HasAgent()) {
      dispatch->SetAgent(agent);
    }
  }

  if (hsa_device_type == HSA_DEVICE_TYPE_CPU) {
    Dispatch* dispatch = static_cast<Dispatch*>(data);
    if (!dispatch->HasCpuAgent()) {
      dispatch->SetCpuAgent(agent);
    }
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t FindRegions(hsa_region_t region, void* data)
{
  hsa_region_segment_t segment_id;
  hsa_region_get_info(region, HSA_REGION_INFO_SEGMENT, &segment_id);

  if (segment_id != HSA_REGION_SEGMENT_GLOBAL) {
    return HSA_STATUS_SUCCESS;
  }

  hsa_region_global_flag_t flags;
  hsa_region_get_info(region, HSA_REGION_INFO_GLOBAL_FLAGS, &flags);

  Dispatch* dispatch = static_cast<Dispatch*>(data);

  if (flags & HSA_REGION_GLOBAL_FLAG_FINE_GRAINED) {
    dispatch->SetSystemRegion(region);
  }

  if (flags & HSA_REGION_GLOBAL_FLAG_COARSE_GRAINED) {
    dispatch->SetLocalRegion(region);
  }

  if (flags & HSA_REGION_GLOBAL_FLAG_KERNARG) {
    dispatch->SetKernargRegion(region);
  }

  return HSA_STATUS_SUCCESS;
}

bool Dispatch::HsaError(const char* msg, hsa_status_t status)
{
  const char* err = 0;
  if (status != HSA_STATUS_SUCCESS) {
    hsa_status_string(status, &err);
  }
  output << msg << ": " << (err ? err : "unknown error") << std::endl;
  return false;
}

bool Dispatch::Init()
{
  hsa_status_t status;
  status = hsa_init();
  if (status != HSA_STATUS_SUCCESS) { return HsaError("hsa_init failed", status); }

  // Find GPU
  status = hsa_iterate_agents(find_gpu_device, this);
  assert(status == HSA_STATUS_SUCCESS);

  char agent_name[64];
  status = hsa_agent_get_info(agent, HSA_AGENT_INFO_NAME, agent_name);
  if (status != HSA_STATUS_SUCCESS) { return HsaError("hsa_agent_get_info(HSA_AGENT_INFO_NAME) failed", status); }
  output << "Using agent: " << agent_name << std::endl;

  status = hsa_agent_get_info(agent, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &queue_size);
  if (status != HSA_STATUS_SUCCESS) { return HsaError("hsa_agent_get_info(HSA_AGENT_INFO_QUEUE_MAX_SIZE) failed", status); }

  status = hsa_queue_create(agent, queue_size, HSA_QUEUE_TYPE_MULTI, NULL, NULL, UINT32_MAX, UINT32_MAX, &queue);
  if (status != HSA_STATUS_SUCCESS) { return HsaError("hsa_queue_create failed", status); }

  status = hsa_signal_create(1, 0, NULL, &signal);
  if (status != HSA_STATUS_SUCCESS) { return HsaError("hsa_signal_create failed", status); }

  status = hsa_agent_iterate_regions(agent, FindRegions, this);
  if (status != HSA_STATUS_SUCCESS) { return HsaError("Failed to iterate memory regions", status); }
  if (!kernarg_region.handle) { return HsaError("Failed to find kernarg memory region"); }

  return true;
}

bool Dispatch::InitDispatch()
{
  const uint32_t queue_mask = queue->size - 1;
  packet_index = hsa_queue_add_write_index_relaxed(queue, 1);
  aql = (hsa_kernel_dispatch_packet_t*) (hsa_kernel_dispatch_packet_t*)(queue->base_address) + (packet_index & queue_mask);
  memset((uint8_t*)aql + 4, 0, sizeof(*aql) - 4);
  aql->completion_signal = signal;
  aql->workgroup_size_x = 1;
  aql->workgroup_size_y = 1;
  aql->workgroup_size_z = 1;
  aql->grid_size_x = 1;
  aql->grid_size_y = 1;
  aql->grid_size_z = 1;
  aql->group_segment_size = 0;
  aql->private_segment_size = 0;
  return true;
}

bool Dispatch::RunDispatch()
{
  std::cout << "wg=" << aql->workgroup_size_x << ", "
                     << aql->workgroup_size_y << ", "
                     << aql->workgroup_size_z << std::endl;
  std::cout << "gr=" << aql->grid_size_x << ", "
                     << aql->grid_size_y << ", "
                     << aql->grid_size_z << std::endl;
  uint16_t header =
    (HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE) |
    (1 << HSA_PACKET_HEADER_BARRIER) |
    (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE) |
    (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE);
  uint16_t setup = 2 << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;
  uint32_t header32 = header | (setup << 16);
  #if defined(_WIN32) || defined(_WIN64)  // Windows
    _InterlockedExchange(aql, header32);
  #else // Linux
    __atomic_store_n((uint32_t*)aql, header32, __ATOMIC_RELEASE);
  #endif
  // Ring door bell
  hsa_signal_store_relaxed(queue->doorbell_signal, packet_index);

  return true;
}

void Dispatch::SetWorkgroupSize(uint16_t sizeX, uint16_t sizeY, uint16_t sizeZ)
{
  aql->workgroup_size_x = sizeX;
  aql->workgroup_size_y = sizeY;
  aql->workgroup_size_z = sizeZ;
}

void Dispatch::SetGridSize(uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ)
{
  aql->grid_size_x = sizeX;
  aql->grid_size_y = sizeY;
  aql->grid_size_z = sizeZ;
}

void Dispatch::SetSystemRegion(hsa_region_t region)
{
  system_region = region;
}

void Dispatch::SetKernargRegion(hsa_region_t region)
{
  kernarg_region = region;
}

void Dispatch::SetLocalRegion(hsa_region_t region)
{
  local_region = region;
}

bool Dispatch::AllocateKernarg(uint32_t size)
{
  hsa_status_t status;
  status = hsa_memory_allocate(kernarg_region, size, &kernarg);
  if (status != HSA_STATUS_SUCCESS) { return HsaError("Failed to allocate kernarg", status); }
  aql->kernarg_address = kernarg;
  kernarg_offset = 0;
  return true;
}

bool Dispatch::LoadCodeObjectFromFile(const std::string& filename)
{
  std::ifstream in(filename.c_str(), std::ios::binary | std::ios::ate);
  if (!in) { output << "Error: failed to load " << filename << std::endl; return false; }
  size_t size = std::string::size_type(in.tellg());
  char *ptr = (char*) AllocateSystemMemory(size);
  if (!ptr) {
    output << "Error: failed to allocate memory for code object." << std::endl;
    return false;
  }
  in.seekg(0, std::ios::beg);
  std::copy(std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>(),
            ptr);
/*
  res.assign((std::istreambuf_iterator<char>(in)),
              std::istreambuf_iterator<char>());

*/
  hsa_status_t status = hsa_code_object_deserialize(ptr, size, NULL, &code_object);
  if (status != HSA_STATUS_SUCCESS) { return HsaError("Failed to deserialize code object", status); }
  return true;
}

bool Dispatch::SetupCodeObject()
{
  return false;
}

bool Dispatch::SetupExecutable()
{
  hsa_status_t status;
  hsa_executable_symbol_t kernel_symbol;

  if (!SetupCodeObject()) { return false; }
  status = hsa_executable_create(HSA_PROFILE_FULL, HSA_EXECUTABLE_STATE_UNFROZEN,
                                 NULL, &executable);
  if (status != HSA_STATUS_SUCCESS) { return HsaError("hsa_executable_create failed", status); }

  // Load code object
  status = hsa_executable_load_code_object(executable, agent, code_object, NULL);
  if (status != HSA_STATUS_SUCCESS) { return HsaError("hsa_executable_load_code_object failed", status); }

  // Freeze executable
  status = hsa_executable_freeze(executable, NULL);
  if (status != HSA_STATUS_SUCCESS) { return HsaError("hsa_executable_freeze failed", status); }

  // Get symbol handle
  //status = hsa_executable_get_symbol(executable, NULL, "mad2d", agent, 0, &kernel_symbol);
  status = hsa_executable_get_symbol(executable, NULL, "ZN12_GLOBAL__N_113mad2d_functor19__cxxamp_trampolineEiiiiiiPfPKfS3_ffi", agent, 0, &kernel_symbol);

  if (status != HSA_STATUS_SUCCESS) { return HsaError("hsa_executable_get_symbol failed", status); }

  // Get code handle
  uint64_t code_handle;
  status = hsa_executable_symbol_get_info(kernel_symbol,
                                          HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT,
                                          &code_handle);
  if (status != HSA_STATUS_SUCCESS) { return HsaError("hsa_executable_symbol_get_info failed", status); }

  aql->kernel_object = code_handle;

  return true;
}

uint64_t TIMEOUT = 120;

bool Dispatch::Wait()
{
  clock_t beg = clock();
  hsa_signal_value_t result;
  do {
    result = hsa_signal_wait_acquire(signal,
      HSA_SIGNAL_CONDITION_EQ, 0, ~0ULL, HSA_WAIT_STATE_ACTIVE);
    clock_t clocks = clock() - beg;
    if (clocks > (clock_t) TIMEOUT * CLOCKS_PER_SEC) {
      output << "Kernel execution timed out, elapsed time: " << (long) clocks << std::endl;
      return false;
    }
  } while (result != 0);
  return true;
}

void* Dispatch::AllocateLocalMemory(size_t size)
{
  assert(local_region.handle != 0);
  void *p = NULL;

  hsa_status_t status = hsa_memory_allocate(local_region, size, (void **)&p);
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_memory_allocate(local_region) failed", status); return 0; }
  //status = hsa_memory_assign_agent(p, agent, HSA_ACCESS_PERMISSION_RW);
  //if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_memory_assign_agent failed", status); return 0; }
  return p;
}

void* Dispatch::AllocateSystemMemory(size_t size)
{
  void *p = NULL;
  hsa_status_t status = hsa_memory_allocate(system_region, size, (void **)&p);
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_memory_allocate(system_region) failed", status); return 0; }
  return p;
}

bool Dispatch::CopyToLocal(void* dest, void* src, size_t size)
{
  hsa_status_t status;
  status = hsa_memory_copy(dest, src, size);
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_memory_copy failed", status); return false; }
  //status = hsa_memory_assign_agent(dest, agent, HSA_ACCESS_PERMISSION_RW);
  //if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_memory_assign_agent failed", status); return false; }
  return true;
}

bool Dispatch::CopyFromLocal(void* dest, void* src, size_t size)
{
  hsa_status_t status;
  status = hsa_memory_assign_agent(src, cpu_agent, HSA_ACCESS_PERMISSION_RW);
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_memory_assign_agent failed", status); return false; }
  status = hsa_memory_copy(dest, src, size);
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_memory_copy failed", status); return false; }
  return true;
}

Buffer* Dispatch::AllocateBuffer(size_t size)
{
  if (local_region.handle != 0) {
    void* system_ptr = AllocateSystemMemory(size);
    if (!system_ptr) { return 0; }
    void* local_ptr = AllocateLocalMemory(size);
    if (!local_ptr) { free(system_ptr); return 0; }
    return new Buffer(size, local_ptr, system_ptr);
  } else {
    void* system_ptr = AllocateSystemMemory(size);
    if (!system_ptr) { return 0; }
    return new Buffer(size, system_ptr);
  }
}

bool Dispatch::CopyTo(Buffer* buffer)
{
  if (buffer->IsLocal()) {
    return CopyToLocal(buffer->LocalPtr(), buffer->SystemPtr(), buffer->Size());
  }
  return true;
}

bool Dispatch::CopyFrom(Buffer* buffer)
{
  if (buffer->IsLocal()) {
    return CopyFromLocal(buffer->SystemPtr(), buffer->LocalPtr(), buffer->Size());
  }
  return true;
}

void Dispatch::KernargRaw(const void* ptr, size_t size, size_t align)
{
  assert((align & (align - 1)) == 0);
  kernarg_offset = ((kernarg_offset + align - 1) / align) * align;
  memcpy((char*) kernarg + kernarg_offset, ptr, size);
  kernarg_offset += size;
}

void Dispatch::Kernarg(Buffer* buffer)
{
  void* localPtr = buffer->LocalPtr();
  Kernarg(&localPtr);
}

bool Dispatch::Run()
{
  bool res =
    Init() &&
    InitDispatch() &&
    SetupExecutable() &&
    Setup() &&
    RunDispatch() &&
    Wait() &&
    Verify();
  std::string out = output.str();
  if (!out.empty()) {
    std::cout << out << std::endl;
  }
  std::cout << (res ? "Success" : "Failed") << std::endl;
  return res;
}

int Dispatch::RunMain()
{
  return Run() ? 0 : 1;
}

uint64_t Dispatch::GetTimestampFrequency()
{
  uint64_t frequency;
  hsa_status_t status;
  status = hsa_system_get_info(HSA_SYSTEM_INFO_TIMESTAMP_FREQUENCY, &frequency);
  if (status != HSA_STATUS_SUCCESS) {
    HsaError("hsa_system_get_info(HSA_SYSTEM_INFO_TIMESTAMP_FREQUENCY) failed", status);
    return 0;
  }

  return frequency;
}

} // namespace dispatch
} // namespace amd


using namespace amd::dispatch;

class AsmKernelDispatch : public Dispatch {
private:
  Buffer* c;
  Buffer* a;
  Buffer* b;
  const unsigned int workGroup[2] = {16, 16};
  const unsigned int microTile[2] = {8, 8};
  float vA, vB, vC;
  float alpha;
  float beta;
  unsigned int M, N, K;
  unsigned int numElementsC, numElementsA, numElementsB;
  unsigned int sizeC, sizeA, sizeB;
  unsigned int offsetC, offsetA, offsetB;
  unsigned int strideCJ, strideAK, strideBK;
  unsigned int size0I, size1J, sizeK;

public:
  AsmKernelDispatch(int argc, const char **argv)
    : Dispatch(argc, argv),
    vA(1),
    vB(1),
    vC(1),
    alpha(1),
    beta(1),
    M(128),
    N(128),
    K(128),
    numElementsC(M*N),
    numElementsA(M*K),
    numElementsB(N*K),
    sizeC(numElementsC*sizeof(float)),
    sizeA(numElementsA*sizeof(float)),
    sizeB(numElementsB*sizeof(float)),
    offsetC(0),
    offsetA(0),
    offsetB(0),
    strideCJ(N),
    strideAK(K),
    strideBK(K),
    size0I(M),
    size1J(N),
    sizeK(K)
  {
  }

  bool SetupCodeObject() override {
    return LoadCodeObjectFromFile("kernel.co");
  }

  bool Setup() override {
    if (!AllocateKernarg(3*8 + 11*4 + 0)) { return false; }


    c = AllocateBuffer(sizeC);
    a = AllocateBuffer(sizeA);
    b = AllocateBuffer(sizeB);
    for (unsigned int i = 0; i < numElementsC; i++) c->Data<float>(i) = vC*i;
    for (unsigned int i = 0; i < numElementsA; i++) a->Data<float>(i) = vA*i;
    for (unsigned int i = 0; i < numElementsB; i++) b->Data<float>(i) = vB*i;
    if (!CopyTo(c)) output << "Error: failed to copy to local" << std::endl;
    if (!CopyTo(a)) output << "Error: failed to copy to local" << std::endl;
    if (!CopyTo(b)) output << "Error: failed to copy to local" << std::endl;

    Kernarg(c);
    Kernarg(a);
    Kernarg(b);
    Kernarg(&alpha);
    Kernarg(&beta);
    Kernarg(&offsetC);
    Kernarg(&offsetA);
    Kernarg(&offsetB);
    Kernarg(&strideCJ);
    Kernarg(&strideAK);
    Kernarg(&strideBK);
    Kernarg(&size0I);
    Kernarg(&size1J);
    Kernarg(&sizeK);

    SetGridSize(size0I/microTile[0],size1J/microTile[1]);
    SetWorkgroupSize(workGroup[0], workGroup[1]);
    return true;
  }

  bool Verify() override {
    if (!CopyFrom(c)) {
      output << "Error: failed to copy from local" << std::endl;
      return false;
    }
    bool valid = true;
    for (unsigned int d1 = 0; d1 < size1J; d1++) {
      for (unsigned int d0 = 0; d0 < size0I; d0++) {
        unsigned int index = d1*strideCJ + d0;
        float correctC = alpha*(vA*index)*(vB*index)+beta*(vC*index);
        bool equal = c->Data<float>(index) == correctC;
        output << "c[" << d1 << "," << d0 << "] = "
            << c->Data<float>(index) << (equal ? " == " : " != ") << correctC << std::endl;
        if (!equal) valid = false;
      }
    }
    return valid;
  }
};

int main(int argc, const char** argv)
{
  return AsmKernelDispatch(argc, argv).RunMain();
}
