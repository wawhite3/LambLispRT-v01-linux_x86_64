#include "LambLisp.h"

#if LL_CUDA
#include "cuda_runtime.h"

//kernel examples are based on the book by Jamie Flux.

__global__ void cudakernel_vectorAdd(const float *A, const float *B, float *C, int n)
{
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) C[i] = A[i] + B[i];
}

__global__ void kernel1(float *data, int n)
{
  int i = blockDim.x * blockIdx.x + threadIdx.x;
  if (i < n) data[i] *= 2.0;
}

cudaError_t launch_kernel1(int nblocks, int thrPerBlk, int shared, cudaStream_t stream,
			   float *data, int n
			   )
{
  kernel1<<<nblocks, thrPerBlk, shared, stream>>>(data, n);
  return cudaGetLastError();
}

__global__ void kernel2(float *data, int n)
{
  int i = blockDim.x * blockIdx.x + threadIdx.x;
  if (i < n) data[i] += 1.0;
}

Sexpr_t mop3_cudaLaunch_kernel1(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("mop3_cudaLaunch_kernel1()");
  ll_try {
    Sexpr_t stream = lamb.car(sexpr);
    cudaStream_t cudaStream;
    if ((stream->type() == Cell::T_INT) && (stream->as_Int_t() == 0)) cudaStream = 0;
    else {
      cudaStream_t *cs = (cudaStream_t *) stream->mustbe_cppobj_t()->prechecked_cppobj_get_ptr();
      cudaStream = *cs;
    }
	
    Sexpr_t vec = lamb.car(sexpr);
    Int_t nbytes;
    ByteVec_t bytes;
    vec->any_bvec_get_info(nbytes, bytes);

    Int_t n = nbytes / sizeof(Real_t);
    Real_t *elems = (Real_t *) bytes;

    const Int_t threadsPerBlock = 256;
    Int_t nblocks = (n + threadsPerBlock - 1) / threadsPerBlock;
  
    cudaError_t err = launch_kernel1(nblocks, threadsPerBlock, 0, cudaStream, elems, n);
    if (err != cudaSuccess) throw NIL->mk_error("%s cuda error %d\n", me, err);
    return OBJ_UNDEF;
  }
  ll_catch();
}

Sexpr_t mop3_cudakernel_vectorAdd(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("mop3_cudakernel_vectorAdd()");
  ll_try {
    
    Sexpr_t dA_sx = lamb.car(sexpr);
    Sexpr_t dB_sx = lamb.cadr(sexpr);
    Sexpr_t dC_sx = lamb.caddr(sexpr);
    Int_t n       = lamb.cadddr(sexpr)->mustbe_Int_t();

    Int_t nbytes;
    ByteVec_t bytes;
    dA_sx->any_bvec_get_info(nbytes, bytes);
    Int_t nA   = nbytes / sizeof(Real_t);
    Real_t *eA = (Real_t *) bytes;
    
    dB_sx->any_bvec_get_info(nbytes, bytes);
    Int_t nB   = nbytes / sizeof(Real_t);
    Real_t *eB = (Real_t *) bytes;
    
    dC_sx->any_bvec_get_info(nbytes, bytes);
    Int_t nC   = nbytes / sizeof(Real_t);
    Real_t *eC = (Real_t *) bytes;
    
    Bool_t bad = (nA < n) || (nB < n) || (nC < n);
    if (bad) throw NIL->mk_error("%s Index out of range %d\n", me, n);

    const Int_t threadsPerBlock = 256;
    Int_t nblocks = (n + threadsPerBlock - 1) / threadsPerBlock;
  
    cudakernel_vectorAdd<<<nblocks, threadsPerBlock>>>(eA, eB, eC, n);
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) throw NIL->mk_error("%s cuda error %d\n", me, err);
    return OBJ_UNDEF;
  }
  ll_catch();
}

Sexpr_t mop3_cudaMalloc(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("mop3_cudaMalloc()");
  ll_try {
    Int_t n = lamb.car(sexpr)->mustbe_Int_t();
    void *devicePtr = 0;
    cudaError_t err = cudaMalloc(&devicePtr, n);
    if (err != cudaSuccess) throw NIL->mk_error("%s error %d\n", me, err);
    return lamb.mk_bytevector_ext(n, (Bytest_t) devicePtr, env_exec);
  }
  ll_catch();
}

Sexpr_t mop3_cudaMallocHost(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("mop3_cudaMalloc()");
  ll_try {
    Int_t n = lamb.car(sexpr)->mustbe_Int_t();
    void *hostPtr = 0;
    cudaError_t err = cudaMallocHost(&hostPtr, n);
    if (err != cudaSuccess) throw NIL->mk_error("%s error %d\n", me, err);
    return lamb.tcons(Cell::T_BVEC_HEAP, (Word_t) n, (Word_t) hostPtr, env_exec);
  }
  ll_catch();

}

Sexpr_t mop3_cudaFree(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("mop3_cudaFree()");
  ll_try {
    while (sexpr != NIL) {
      Sexpr_t bv      = lamb.car(sexpr);
      void *devPtr    = (void *) bv->any_bvec_get_elems();
      cudaError_t err = cudaFree(devPtr);
      if (err != cudaSuccess) throw NIL->mk_error("%s error %d\n", me, err);
      sexpr = lamb.cdr(sexpr);
    }
    return OBJ_UNDEF;
  }
  ll_catch();
}

//!Combines cudaMemcpy() and cudaMemcpyAsync().  If the stream argument is provided then the asynchronous copy will be used.
Sexpr_t mop3_cudaMemcpy(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("mop3_cudaMemcpy()");
  ll_try {
    static Sexpr_t sym_h2d = NIL;
    static Sexpr_t sym_d2h = NIL;
    static Sexpr_t sym_d2d = NIL;
    once({
	sym_h2d = lamb.mk_symbol("cudaMemcpyHostToDevice", env_exec);
	sym_d2h = lamb.mk_symbol("cudaMemcpyDeviceToHost", env_exec);
	sym_d2d = lamb.mk_symbol("cudaMemcpyDeviceToDevice", env_exec);
      });

    Int_t dlen, slen;
    ByteVec_t dst, src;
    lamb.car(sexpr)->any_bvec_get_info(dlen, dst);
    lamb.cadr(sexpr)->any_bvec_get_info(slen, src);
    sexpr = lamb.cddr(sexpr);

    Int_t count = lamb.car(sexpr)->mustbe_Int_t();
    if ((dlen < count) || (slen < count)) throw NIL->mk_error("%s Bad index %d\n", me, count);
    sexpr = lamb.cdr(sexpr);

    Sexpr_t dir = lamb.car(sexpr);
    cudaMemcpyKind idir;
    if (dir == sym_h2d) idir = cudaMemcpyHostToDevice;
    else if (dir == sym_d2h) idir = cudaMemcpyDeviceToHost;
    else if (dir == sym_d2d) idir = cudaMemcpyDeviceToDevice;
    else throw NIL->mk_error("%s Bad xfer direction %d\n", me, idir);
    sexpr = lamb.cdr(sexpr);

    cudaStream_t *cppstream = 0;
    if (sexpr != NIL) {
      Sexpr_t item = lamb.car(sexpr);
      if (item != NIL) cppstream = ((cudaStream_t *) item->mustbe_cppobj_t()->prechecked_cppobj_get_ptr());
    }
    
    cudaError_t err = cppstream
      ? cudaMemcpyAsync(dst, src, count, idir, *cppstream)
      : cudaMemcpy(dst, src, count, idir);
    if (err != cudaSuccess) throw NIL->mk_error("%s error %d\n", me, err);
  
    return OBJ_UNDEF;
  }
  ll_catch();
}

Sexpr_t mop3_cudaMemset(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Sexpr_t bv  = lamb.car(sexpr);
  Int_t val   = lamb.cadr(sexpr)->mustbe_Int_t();
  Int_t count = lamb.caddr(sexpr)->mustbe_Int_t();

  void *devPtr = (void *) bv->any_bvec_get_elems();
  int err = cudaMemset(devPtr, val, count);
  return OBJ_UNDEF;
}

void cudaStreamDestroyer(Ptr_t p) { cudaStream_t *s = (cudaStream_t *) p;  cudaStreamDestroy(*s);   delete s; }

Sexpr_t mop3_cudaStreamCreate(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  cudaStream_t *s = new cudaStream_t;
  cudaStreamCreate(s);

  CPPDeleterPtr p = &cudaStreamDestroyer;
  return lamb.tcons(Cell::T_CPP_HEAP, (Word_t) p, (Word_t) s, env_exec);
}

Sexpr_t mop3_cudaStreamSynchronize(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Sexpr_t sx = lamb.car(sexpr);
  cudaStream_t *s = (cudaStream_t *) sx->mustbe_cppobj_t()->prechecked_cppobj_get_ptr();
  cudaStreamSynchronize(*s);
  return OBJ_UNDEF;
}

Sexpr_t mop3_cudaDeviceSynchronize(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  cudaDeviceSynchronize();
  return OBJ_UNDEF;
}

Sexpr_t mop3_cudaGetDeviceCount(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("mop3_cudaGetDeviceCount()");
  ll_try {
    int n = -1;
    cudaError_t err = cudaGetDeviceCount(&n);
    if (err != cudaSuccess) throw NIL->mk_error("%s cuda returned %d\n", me, err);
    return lamb.mk_integer(n, env_exec);
  }
  ll_catch();
}

Sexpr_t mop3_cudaGetDeviceProp(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("mop3_cudaGetDeviceProp()");
  ll_try {
    Int_t dev = 0;
    if (sexpr != NIL) dev = lamb.car(sexpr)->mustbe_Int_t();

    cudaDeviceProp prop;
    cudaError_t err = cudaGetDeviceProperties(&prop, dev);
    if (err != cudaSuccess) throw NIL->mk_error("%s cuda returned %d\n", me, err);

    Sexpr_t alist = NIL;

    Sexpr_t sym = lamb.mk_symbol("name", env_exec);
    lamb.gc_root_push(sym);
    Sexpr_t vec = lamb.mk_string(sizeof(prop.name), prop.name, env_exec);
    Sexpr_t pair = lamb.cons(sym, vec, env_exec);
    lamb.gc_root_pop();

    alist = lamb.cons(pair, alist, env_exec);
    lamb.gc_root_push(alist);
  
    sym = lamb.mk_symbol("uuid", env_exec);
    lamb.gc_root_push(sym);
  
    vec   = lamb.mk_bytevector(sizeof(prop.uuid), (Bytest_t) &prop.uuid, env_exec);
    pair  = lamb.cons(sym, vec, env_exec);
    alist = lamb.cons(pair, alist, env_exec);
    lamb.gc_root_pop(2);
  
#define mk_int_field(__name) {					\
      lamb.gc_root_push(alist);					\
      Sexpr_t sym  = lamb.mk_symbol(#__name, env_exec);		\
      lamb.gc_root_push(sym);					\
      Sexpr_t ival = lamb.mk_integer(prop.__name, env_exec);	\
      Sexpr_t pair = lamb.cons(sym, ival, env_exec);		\
      alist = lamb.cons(pair, alist, env_exec);			\
      lamb.gc_root_pop(2);					\
    }								\
    //

#define mk_int2_field(__name) {						\
      lamb.gc_root_push(alist);						\
      Sexpr_t sym  = lamb.mk_symbol(#__name, env_exec);			\
      lamb.gc_root_push(sym);						\
      Sexpr_t vec  = lamb.mk_bytevector(2 * sizeof(int), (Bytest_t) prop.__name, env_exec); \
      Sexpr_t pair = lamb.cons(sym, vec, env_exec);			\
      alist = lamb.cons(pair, alist, env_exec);				\
      lamb.gc_root_pop(2);						\
    }									\
    //

#define mk_int3_field(__name) {						\
      lamb.gc_root_push(alist);						\
      Sexpr_t sym  = lamb.mk_symbol(#__name, env_exec);			\
      lamb.gc_root_push(sym);						\
      Sexpr_t vec  = lamb.mk_bytevector(3 * sizeof(int), (Bytest_t) prop.__name, env_exec); \
      Sexpr_t pair = lamb.cons(sym, vec, env_exec);			\
      alist = lamb.cons(pair, alist, env_exec);				\
      lamb.gc_root_pop(2);						\
    }									\
    //
    
    mk_int_field(totalGlobalMem);
    mk_int_field(sharedMemPerBlock);
    mk_int_field(regsPerBlock);
    mk_int_field(warpSize);
    mk_int_field(memPitch);
    mk_int_field(maxThreadsPerBlock);
    mk_int3_field(maxThreadsDim);
    mk_int3_field(maxGridSize);

    mk_int_field(clockRate);
    mk_int_field(totalConstMem);
    mk_int_field(major);
    mk_int_field(minor);
    mk_int_field(textureAlignment);
    mk_int_field(texturePitchAlignment);
    mk_int_field(deviceOverlap);
    mk_int_field(multiProcessorCount);
    mk_int_field(kernelExecTimeoutEnabled);
    mk_int_field(integrated);
    mk_int_field(canMapHostMemory);
    mk_int_field(computeMode);
    mk_int_field(maxTexture1D);
    mk_int_field(maxTexture1DMipmap);
    mk_int_field(maxTexture1DLinear);
    mk_int2_field(maxTexture2D);
    mk_int2_field(maxTexture2DMipmap);
    mk_int3_field(maxTexture2DLinear);
    mk_int2_field(maxTexture2DGather);
    mk_int3_field(maxTexture3D);
    mk_int3_field(maxTexture3DAlt);

    mk_int_field(maxTextureCubemap);
    mk_int2_field(maxTexture1DLayered);
    mk_int3_field(maxTexture2DLayered);
    mk_int2_field(maxTextureCubemapLayered);
    mk_int_field(maxSurface1D);
    mk_int2_field(maxSurface2D);
    mk_int3_field(maxSurface3D);
    mk_int2_field(maxSurface1DLayered);  
    mk_int3_field(maxSurface2DLayered);
    
    mk_int_field(maxSurfaceCubemap);
    mk_int2_field(maxSurfaceCubemapLayered);
    
    mk_int_field(surfaceAlignment);
    mk_int_field(concurrentKernels);
    mk_int_field(ECCEnabled);
    mk_int_field(pciBusID);
    mk_int_field(pciDeviceID);
    mk_int_field(pciDomainID);
    mk_int_field(tccDriver);
    mk_int_field(asyncEngineCount);
    mk_int_field(unifiedAddressing);
    mk_int_field(memoryClockRate);
    mk_int_field(memoryBusWidth);
    mk_int_field(l2CacheSize);
    mk_int_field(persistingL2CacheMaxSize);
    mk_int_field(maxThreadsPerMultiProcessor);
    mk_int_field(streamPrioritiesSupported);
    mk_int_field(globalL1CacheSupported);
    mk_int_field(localL1CacheSupported);
    mk_int_field(sharedMemPerMultiprocessor);
    mk_int_field(regsPerMultiprocessor);
    mk_int_field(managedMemory);
    mk_int_field(isMultiGpuBoard);
    mk_int_field(multiGpuBoardGroupID);
    mk_int_field(singleToDoublePrecisionPerfRatio);
    mk_int_field(pageableMemoryAccess);
    mk_int_field(concurrentManagedAccess);
    mk_int_field(computePreemptionSupported);
    mk_int_field(canUseHostPointerForRegisteredMem);
    mk_int_field(cooperativeLaunch);
    mk_int_field(cooperativeMultiDeviceLaunch);
    mk_int_field(pageableMemoryAccessUsesHostPageTables);
    mk_int_field(directManagedMemAccessFromHost);
    mk_int_field(accessPolicyMaxWindowSize);

#undef mk_int_field
#undef mk_int2_field
#undef mk_int3_field
  
    return alist;
  }
  ll_catch();
}

#endif

Sexpr_t Cuda_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
#if LL_CUDA
  ME("::Cuda_install_mop3()");
  ll_try {

    static const struct { Lamb::Mop3st_t func;  const char *name; } std_procs[] = {
      { mop3_cudaGetDeviceCount, "cudaGetDeviceCount" },
      { mop3_cudaGetDeviceProp, "cudaGetDeviceProperties" },

      { mop3_cudaMalloc, "cudaMalloc" },
      { mop3_cudaMalloc, "cudaMallocHost" },
      { mop3_cudaFree, "cudaFree" },
      { mop3_cudaMemset, "cudaMemset" },
      { mop3_cudaMemcpy, "cudaMemcpy" },
      { mop3_cudaDeviceSynchronize, "cudaDeviceSynchronize" },
      { mop3_cudakernel_vectorAdd, "cudakernel-vectorAdd" },

      { Cuda_install_mop3, "Cuda-install-mop3" }
    };

    const int Nstd_procs = sizeof(std_procs)/sizeof(std_procs[0]);
  
    lamb.log("%s defining %d Mops\n", me, Nstd_procs);
    Sexpr_t env_target = lamb.car(sexpr);
    for (int i=0; i<Nstd_procs; i++) {
      Sexpr_t sym  = lamb.mk_symbol(std_procs[i].name, env_exec);
      lamb.gc_root_push(sym);
      Sexpr_t proc = lamb.mk_Mop3_procst_t(std_procs[i].func, env_exec);
      lamb.gc_root_pop();
      lamb.dict_bind_bang(env_target, sym, proc, env_exec);
    }
    
    return OBJ_UNDEF;
  }
  ll_catch();
#else
  return OBJ_UNDEF;
#endif
}

