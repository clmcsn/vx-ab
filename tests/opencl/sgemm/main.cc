#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <CL/opencl.h>
#include <string.h>
#include <time.h>
#include <unistd.h> 
#include <chrono>
#include <vector>
#include "common.h"

#define KERNEL_NAME "sgemm"

#define CL_CHECK(_expr)                                                \
   do {                                                                \
     cl_int _err = _expr;                                              \
     if (_err == CL_SUCCESS)                                           \
       break;                                                          \
     printf("OpenCL Error: '%s' returned %d!\n", #_expr, (int)_err);   \
	 cleanup();			                                                     \
     exit(-1);                                                         \
   } while (0)

#define CL_CHECK2(_expr)                                               \
   ({                                                                  \
     cl_int _err = CL_INVALID_VALUE;                                   \
     decltype(_expr) _ret = _expr;                                     \
     if (_err != CL_SUCCESS) {                                         \
       printf("OpenCL Error: '%s' returned %d!\n", #_expr, (int)_err); \
	   cleanup();			                                                   \
       exit(-1);                                                       \
     }                                                                 \
     _ret;                                                             \
   })

static int read_kernel_file(const char* filename, uint8_t** data, size_t* size) {
  if (nullptr == filename || nullptr == data || 0 == size)
    return -1;

  FILE* fp = fopen(filename, "r");
  if (NULL == fp) {
    fprintf(stderr, "Failed to load kernel.");
    return -1;
  }
  fseek(fp , 0 , SEEK_END);
  long fsize = ftell(fp);
  rewind(fp);

  *data = (uint8_t*)malloc(fsize);
  *size = fread(*data, 1, fsize, fp);
  
  fclose(fp);
  
  return 0;
}

/*static void matmul(TYPE *C, const TYPE* A, const TYPE *B, int M, int N, int K) {
  for (int m = 0; m < M; ++m) {
    for (int n = 0; n < N; ++n) {
      TYPE acc = 0;
      for (int k = 0; k < K; ++k) {
          acc += A[k * M + m] * B[n * K + k];
      }
      C[n * M + m] = acc;
    }
  }
}*/

#ifdef USE_FLOAT
static bool compare_equal(float a, float b, int ulp = 21) {
  union fi_t { int i; float f; };
  fi_t fa, fb;
  fa.f = a;
  fb.f = b;
  return std::abs(fa.i - fb.i) <= ulp;
}
#else
static bool compare_equal(int a, int b, int ulp = 21) {
  return (a == b);
}
#endif

cl_device_id device_id = NULL;
cl_context context = NULL;
cl_command_queue commandQueue = NULL;
cl_program program = NULL;
cl_kernel kernel = NULL;
cl_mem a_memobj = NULL;
cl_mem b_memobj = NULL;
cl_mem c_memobj = NULL;  
TYPE *h_a = NULL;
TYPE *h_b = NULL;
TYPE *h_c = NULL;
uint8_t *kernel_bin = NULL;

static void cleanup() {
  if (commandQueue) clReleaseCommandQueue(commandQueue);
  if (kernel) clReleaseKernel(kernel);
  if (program) clReleaseProgram(program);
  if (a_memobj) clReleaseMemObject(a_memobj);
  if (b_memobj) clReleaseMemObject(b_memobj);
  if (c_memobj) clReleaseMemObject(c_memobj);  
  if (context) clReleaseContext(context);
  if (device_id) clReleaseDevice(device_id);
  
  if (kernel_bin) free(kernel_bin);
  if (h_a) free(h_a);
  if (h_b) free(h_b);
  if (h_c) free(h_c);
}

int size = 32;

static void show_usage() {
  printf("Usage: [-n size] [-h: help]\n");
}

static void parse_args(int argc, char **argv) {
  int c;
  while ((c = getopt(argc, argv, "n:h?")) != -1) {
    switch (c) {
    case 'n':
      size = atoi(optarg);
      break;
    case 'h':
    case '?': {
      show_usage();
      exit(0);
    } break;
    default:
      show_usage();
      exit(-1);
    }
  }

  if (size < 2) {
    printf("Error: invalid size!\n");
    exit(-1);
  }

  printf("Workload size=%d\n", size);
}

int main (int argc, char **argv) {
  // parse command arguments
  parse_args(argc, argv);

  cl_platform_id platform_id;
  size_t kernel_size;
  cl_int binary_status;

  srand(50);

  // read kernel binary from file  
  if (0 != read_kernel_file("kernel.pocl", &kernel_bin, &kernel_size))
    return -1;
  
  // Getting platform and device information
  CL_CHECK(clGetPlatformIDs(1, &platform_id, NULL));
  CL_CHECK(clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 1, &device_id, NULL));

  printf("Create context\n");
  context = CL_CHECK2(clCreateContext(NULL, 1, &device_id, NULL, NULL,  &_err));

  // Allocate device buffers
  size_t nbytes = size * size * sizeof(TYPE);
  a_memobj = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_ONLY, nbytes, NULL, &_err));
  b_memobj = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_ONLY, nbytes, NULL, &_err));
  c_memobj = CL_CHECK2(clCreateBuffer(context, CL_MEM_WRITE_ONLY, nbytes, NULL, &_err));

  printf("Create program from kernel source\n");
  program = CL_CHECK2(clCreateProgramWithBinary(
    context, 1, &device_id, &kernel_size, (const uint8_t**)&kernel_bin, &binary_status, &_err));
  if (program == NULL) {
    cleanup();
    return -1;
  }

  // Build program
  CL_CHECK(clBuildProgram(program, 1, &device_id, NULL, NULL, NULL));
  
  // Create kernel
  kernel = CL_CHECK2(clCreateKernel(program, KERNEL_NAME, &_err));

  // Set kernel arguments
  int width = size;
  CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&a_memobj));	
  CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&b_memobj));	
  CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&c_memobj));
  CL_CHECK(clSetKernelArg(kernel, 3, sizeof(width), (void*)&width));

  // Allocate memories for input arrays and output arrays.    
  h_a = (TYPE*)malloc(nbytes);
  h_b = (TYPE*)malloc(nbytes);
  h_c = (TYPE*)malloc(nbytes);	
	
  // Initialize values for array members.  
  for (int i = 0; i < (size * size); ++i) {
  #ifdef USE_FLOAT
    h_a[i] = (float)rand() / (float)RAND_MAX;
    h_b[i] = (float)rand() / (float)RAND_MAX;
  #else
    h_a[i] = rand();
    h_b[i] = rand();
  #endif
    h_c[i] = 0xdeadbeef;
  }

  size_t global_offset[2] = {0, 0};
  size_t global_work_size[2] = {size, size};
  size_t local_work_size[2] = {1, 1};

  std::vector<float> ref_vec(size * size);

  // reference generation
  size_t num_groups_y = global_work_size[1] / local_work_size[1];
  size_t num_groups_x = global_work_size[0] / local_work_size[0];    
  for (size_t workgroup_id_y = 0; workgroup_id_y < num_groups_y; ++workgroup_id_y) {
    for (size_t workgroup_id_x = 0; workgroup_id_x < num_groups_x; ++workgroup_id_x) {
      for (size_t local_id_y = 0; local_id_y < local_work_size[1]; ++local_id_y) {
        for (size_t local_id_x = 0; local_id_x < local_work_size[0]; ++local_id_x) {
          // Calculate global ID for the work-item
          int global_id_x = global_offset[0] + local_work_size[0] * workgroup_id_x + local_id_x;
          int global_id_y = global_offset[1] + local_work_size[1] * workgroup_id_y + local_id_y;
          // kernel operation
          int r = global_id_x;
          int c = global_id_y;
          TYPE acc = 0;
          for (int k = 0; k < width; k++) {
            acc += h_a[k * width + r] * h_b[c * width + k];
          }
        /*#ifdef USE_FLOAT
          printf("*** r=%d, c=%d, v=%f\n", r, c, acc);
        #else
          printf("*** r=%d, c=%d, v=%d\n", r, c, acc);
        #endif*/                   
          ref_vec[c * width + r] = acc;         
        }
      }
    }
  }

  // Creating command queue
  commandQueue = CL_CHECK2(clCreateCommandQueue(context, device_id, 0, &_err));  

	printf("Upload source buffers\n");
  CL_CHECK(clEnqueueWriteBuffer(commandQueue, a_memobj, CL_TRUE, 0, nbytes, h_a, 0, NULL, NULL));
  CL_CHECK(clEnqueueWriteBuffer(commandQueue, b_memobj, CL_TRUE, 0, nbytes, h_b, 0, NULL, NULL));

  printf("Execute the kernel\n");  
  auto time_start = std::chrono::high_resolution_clock::now();
  CL_CHECK(clEnqueueNDRangeKernel(commandQueue, kernel, 2, global_offset, global_work_size, local_work_size, 0, NULL, NULL));
  CL_CHECK(clFinish(commandQueue));
  auto time_end = std::chrono::high_resolution_clock::now();
  double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start).count();
  printf("Elapsed time: %lg ms\n", elapsed);

  printf("Download destination buffer\n");
  CL_CHECK(clEnqueueReadBuffer(commandQueue, c_memobj, CL_TRUE, 0, nbytes, h_c, 0, NULL, NULL));

  printf("Verify result\n");
  int errors = 0;
  for (int i = 0; i < (size * size); i++) {
    if (!compare_equal(h_c[i], ref_vec[i])) {
      if (errors < 100) 
      #ifdef USE_FLOAT
        printf("*** error: [%d] expected=%f, actual=%f\n", i, ref_vec[i], h_c[i]);
      #else
        printf("*** error: [%d] expected=%d, actual=%d\n", i, ref_vec[i], h_c[i]);
      #endif
      ++errors;
    }
  }
  if (errors != 0) {
    printf("FAILED! - %d errors\n", errors);    
  } else {
    printf("PASSED!\n");
  }
  // Clean up		
  cleanup();  

  return errors;
}
