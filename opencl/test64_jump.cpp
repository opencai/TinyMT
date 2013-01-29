/**
 * Test program for OpenCL
 * using 1 parameter for all generators
 * using jump function
 */
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#define __CL_ENABLE_EXCEPTIONS

#include <cstddef>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <float.h>
#include <stdint.h>
#include <inttypes.h>
typedef uint32_t uint;
typedef uint64_t ulong;
#include "opencl_tools.hpp"

#include "tinymt64def.h"
#include "test_common.h"
#include "parse_opt.h"
extern "C" {
#include "tinymt64.h"
#include "jump64.h"
}
using namespace std;
using namespace cl;

/* ================== */
/* OpenCL information */
/* ================== */
std::vector<cl::Platform> platforms;
std::vector<cl::Device> devices;
cl::Context context;
std::string programBuffer;
cl::Program program;
cl::Program::Sources source;
cl::CommandQueue queue;
std::string errorMessage;

/* =========================
   declaration
   ========================= */
static tinymt64_t * tinymt64;
static const uint64_t tinymt64j_mag = UINT64_C(12157665459056928801);
static const char * tinymt64j_characteristic = "945e0ad4a30ec19432dfa9d5959e5d5d";

static int init_check_data(tinymt64_t tinymt64[],
			   int total_num,
			   uint64_t seed);
static int init_check_data_array(tinymt64_t tinymt64[],
				 int total_num,
				 uint64_t seed_array[],
				 int size);
static void check_data(uint64_t * h_data,
		       int num_data,
		       int total_num);
static void check_data12(double * h_data,
			 int num_data,
			 int total_num);
static void check_data01(double * h_data,
			 int num_data,
			 int total_num);
static void check_status(tinymt64j_t * h_status,
			 int total_num);
static void initialize_by_seed(Buffer& tinymt_status,
			       int total_num,
			       int local_num,
			       uint64_t seed);
static void initialize_by_array(Buffer& tinymt_status,
				int group,
				int local_num,
				uint64_t seed_array[],
				int seed_size);
static void make_tinymt(int total_num);
static Buffer get_status_buff(int total_num);
static void generate_uint64(Buffer& tinymt_status,
			    int total_num,
			    int local_num,
			    int data_size);
static void generate_double12(Buffer& tinymt_status,
			      int total_num,
			      int local_num,
			      int data_size);
static void generate_double01(Buffer& tinymt_status,
			      int total_num,
			      int local_num,
			      int data_size);
static int test(int argc, char * argv[]);

/* ========================= */
/* tinymt64 jump test code        */
/* ========================= */
/**
 * main
 * catch errors
 *@param argc number of arguments
 *@param argv array of arguments
 *@return 0 normal, -1 error
 */
int main(int argc, char * argv[])
{
    try {
	return test(argc, argv);
    } catch (Error e) {
	cerr << "Error Code:" << e.err() << endl;
	cerr << e.what() << endl;
    }
}

/**
 * sample main
 *@param argc number of arguments
 *@param argv array of arguments
 *@return 0 normal, -1 error
 */
static int test(int argc, char * argv[])
{
#if defined(DEBUG)
    cout << "test start" << endl;
#endif
    options opt;
    if (!parse_opt(opt, argc, argv)) {
	return -1;
    }
    // OpenCL setup
#if defined(DEBUG)
    cout << "openCL setup start" << endl;
#endif
    platforms = getPlatforms();
    devices = getDevices();
    context = getContext();
#if defined(APPLE) || defined(__MACOSX) || defined(__APPLE__)
    source = getSource("test64_jump.cl");
#else
    source = getSource("test64_jump.cl");
#endif
    std::string option = "";
    bool double_extension = false;
    if (hasDoubleExtension()) {
        double_extension = true;
        option += "-DHAVE_DOUBLE ";
    }
#if defined(DEBUG)
    option += "-DDEBUG";
#endif
    program = getProgram(option.c_str());
    queue = getCommandQueue();
#if defined(DEBUG)
    cout << "openCL setup end" << endl;
#endif
    int total_num = opt.group_num * opt.local_num;
    int max_group_size = getMaxGroupSize();
    if (opt.group_num > max_group_size) {
	cout << "group_num greater than max value("
	     << max_group_size << ")"
	     << endl;
	return -1;
    }
    Buffer tinymt_status = get_status_buff(total_num);
    // initialize by seed
    // generate uint64_t
    tinymt64 = new tinymt64_t[total_num];
    make_tinymt(total_num);
    init_check_data(tinymt64, total_num, 1234);
    initialize_by_seed(tinymt_status, total_num, opt.local_num, 1234);
    for (int i = 0; i < 2; i++) {
	generate_uint64(tinymt_status, total_num,
			opt.local_num, opt.data_count);
    }

    // initialize by array
    // generate double float
    if (double_extension) {
	uint64_t seed_array[5] = {1, 2, 3, 4, 5};
	make_tinymt(total_num);
	init_check_data_array(tinymt64, total_num, seed_array, 5);
	initialize_by_array(tinymt_status, total_num,
			    opt.local_num, seed_array, 5);
	for (int i = 0; i < 1; i++) {
	    generate_double12(tinymt_status, total_num,
			      opt.local_num, opt.data_count);
	    generate_double01(tinymt_status, total_num,
			      opt.local_num, opt.data_count);
	}
    }
    delete[] tinymt64;
    return 0;
}

/**
 * initialize tinymt status in device global memory
 * using 1 parameter for 1 generator.
 *@param tinymt_status device global memories
 *@param group number of group
 *@param seed seed for initialization
 */
static void initialize_by_seed(Buffer& tinymt_status,
			       int total,
			       int local_item,
			       uint64_t seed)
{
#if defined(DEBUG)
    cout << "initialize_by_seed start" << endl;
#endif
    Kernel init_kernel(program, "tinymt_init_seed_kernel");
    init_kernel.setArg(0, tinymt_status);
    init_kernel.setArg(1, seed);
    NDRange global(total);
    NDRange local(local_item);
    Event event;
#if defined(DEBUG)
    cout << "global:" << dec << total << endl;
    cout << "group:" << dec << (total / local_item) << endl;
    cout << "local:" << dec << local_item << endl;
#endif
    queue.enqueueNDRangeKernel(init_kernel,
			       NullRange,
			       global,
			       local,
			       NULL,
			       &event);
    double time = get_time(event);
    tinymt64j_t status[total];
    queue.enqueueReadBuffer(tinymt_status,
			    CL_TRUE,
			    0,
			    sizeof(tinymt64j_t) * total,
			    status);
    cout << "initializing time = " << time * 1000 << "ms" << endl;
#if defined(DEBUG)
    cout << "status[0].s0:" << hex << status[0].s0 << endl;
    cout << "status[0].s1:" << hex << status[0].s1 << endl;
#endif
    check_status(status, total);
#if defined(DEBUG)
    cout << "initialize_by_seed end" << endl;
#endif
}

/**
 * initialize tinymt status in device global memory
 * using 1 parameter for 1 generator.
 *@param tinymt_status device global memories
 *@param group number of group
 *@param seed_array seeds for initialization
 *@param seed_size size of seed_array
 */
static void initialize_by_array(Buffer& tinymt_status,
				int total,
				int local_item,
				uint64_t seed_array[],
				int seed_size)
{
#if defined(DEBUG)
    cout << "initialize_by_array start" << endl;
#endif
    Buffer seed_array_buffer(context,
			     CL_MEM_READ_WRITE,
			     seed_size * sizeof(uint64_t));
    queue.enqueueWriteBuffer(seed_array_buffer,
			     CL_TRUE,
			     0,
			     seed_size * sizeof(uint64_t),
			     seed_array);
    Kernel init_kernel(program, "tinymt_init_array_kernel");
    init_kernel.setArg(0, tinymt_status);
    init_kernel.setArg(1, seed_array_buffer);
    init_kernel.setArg(2, seed_size);
    NDRange global(total);
    NDRange local(local_item);
    Event event;
    queue.enqueueNDRangeKernel(init_kernel,
			       NullRange,
			       global,
			       local,
			       NULL,
			       &event);
    double time = get_time(event);
    tinymt64j_t status[total];
    queue.enqueueReadBuffer(tinymt_status,
			    CL_TRUE,
			    0,
			    sizeof(tinymt64j_t) * total,
			    status);
    cout << "initializing time = " << time * 1000 << "ms" << endl;
    check_status(status, total);
#if defined(DEBUG)
    cout << "initialize_by_array end" << endl;
#endif
}

/**
 * generate 64 bit unsigned random numbers in device global memory
 *@param tinymt_status device global memories
 *@param total_num number of groups for execution
 *@param data_size number of data to generate
 */
static void generate_uint64(Buffer& tinymt_status,
			    int total_num,
			    int local_num,
			    int data_size)
{
#if defined(DEBUG)
    cout << "generate_uint64 start" << endl;
#endif
    int min_size = total_num;
    if (data_size % min_size != 0) {
	data_size = (data_size / min_size + 1) * min_size;
    }
    Kernel uint_kernel(program, "tinymt_uint64_kernel");
    Buffer output_buffer(context,
			 CL_MEM_READ_WRITE,
			 data_size * sizeof(uint64_t));
    uint_kernel.setArg(0, tinymt_status);
    uint_kernel.setArg(1, output_buffer);
    uint_kernel.setArg(2, data_size / total_num);
    NDRange global(total_num);
    NDRange local(local_num);
    Event generate_event;
#if defined(DEBUG)
    cout << "generate_uint64 enque kernel start" << endl;
#endif
    queue.enqueueNDRangeKernel(uint_kernel,
			       NullRange,
			       global,
			       local,
			       NULL,
			       &generate_event);
    uint64_t * output = new uint64_t[data_size];
    generate_event.wait();
    queue.enqueueReadBuffer(output_buffer,
			    CL_TRUE,
			    0,
			    data_size * sizeof(uint64_t),
			    output);
    check_data(output, data_size, total_num);
#if defined(DEBUG)
    print_uint64(output, data_size, total_num);
#endif
    double time = get_time(generate_event);
    cout << "generate time:" << time * 1000 << "ms" << endl;
    delete[] output;
#if defined(DEBUG)
    cout << "generate_uint64 end" << endl;
#endif
}

/**
 * generate double precision floating point numbers in the range [1, 2)
 * in device global memory
 *@param tinymt_status device global memories
 *@param total_num number of groups for execution
 *@param data_size number of data to generate
 */
static void generate_double12(Buffer& tinymt_status,
			      int total_num,
			      int local_num,
			      int data_size)
{
    int min_size = total_num;
    if (data_size % min_size != 0) {
	data_size = (data_size / min_size + 1) * min_size;
    }
    Kernel double_kernel(program, "tinymt_double12_kernel");
    Buffer output_buffer(context,
			 CL_MEM_READ_WRITE,
			 data_size * sizeof(double));
    double_kernel.setArg(0, tinymt_status);
    double_kernel.setArg(1, output_buffer);
    double_kernel.setArg(2, data_size / total_num);
    NDRange global(total_num);
    NDRange local(local_num);
    Event generate_event;
    queue.enqueueNDRangeKernel(double_kernel,
			       NullRange,
			       global,
			       local,
			       NULL,
			       &generate_event);
    double * output = new double[data_size];
    generate_event.wait();
    queue.enqueueReadBuffer(output_buffer,
			    CL_TRUE,
			    0,
			    data_size * sizeof(double),
			    &output[0]);
    check_data12(output, data_size, total_num);
#if defined(DEBUG)
    print_double(&output[0], data_size, total_num);
#endif
    double time = get_time(generate_event);
    delete[] output;
    cout << "generate time:" << time * 1000 << "ms" << endl;
}

/**
 * generate double precision floating point numbers in the range [0, 1)
 * in device global memory
 *@param tinymt_status device global memories
 *@param total_num number of groups for execution
 *@param data_size number of data to generate
 */
static void generate_double01(Buffer& tinymt_status,
			      int total_num,
			      int local_num,
			      int data_size)
{
    int min_size = total_num;
    if (data_size % min_size != 0) {
	data_size = (data_size / min_size + 1) * min_size;
    }
    Kernel double_kernel(program, "tinymt_double01_kernel");
    Buffer output_buffer(context,
			 CL_MEM_READ_WRITE,
			 data_size * sizeof(double));
    double_kernel.setArg(0, tinymt_status);
    double_kernel.setArg(1, output_buffer);
    double_kernel.setArg(2, data_size / total_num);
    NDRange global(total_num);
    NDRange local(local_num);
    Event generate_event;
    queue.enqueueNDRangeKernel(double_kernel,
			       NullRange,
			       global,
			       local,
			       NULL,
			       &generate_event);
    double * output = new double[data_size];
    generate_event.wait();
    queue.enqueueReadBuffer(output_buffer,
			    CL_TRUE,
			    0,
			    data_size * sizeof(double),
			    &output[0]);
    check_data01(output, data_size, total_num);
#if defined(DEBUG)
    print_double(&output[0], data_size, local_num);
#endif
    double time = get_time(generate_event);
    delete[] output;
    cout << "generate time:" << time * 1000 << "ms" << endl;
}


/* ==============
 * check programs
 * ==============*/
static void make_tinymt(int total_num)
{
    tinymt64 = new tinymt64_t[total_num];
    uint32_t mat1 = 0xfa051f40U;
    uint32_t mat2 = 0xffd0fff4U;
    uint64_t tmat = UINT64_C(0x58d02ffeffbfffbc);
    for (int i = 0; i < total_num; i++) {
	tinymt64[i].mat1 = mat1;
	tinymt64[i].mat2 = mat2;
	tinymt64[i].tmat = tmat;
    }
}

static int init_check_data(tinymt64_t tinymt64[],
			   int total_num,
			   uint64_t seed)
{
#if defined(DEBUG)
    cout << "init_check_data start" << endl;
#endif

    tinymt64_init(&tinymt64[0], seed);
    for (int i = 1; i < total_num; i++) {
	tinymt64[i] = tinymt64[i - 1];
	tinymt64_jump(&tinymt64[i],
		      tinymt64j_mag,
		      0,
		      tinymt64j_characteristic);
    }
#if defined(DEBUG)
    cout << "init_check_data end" << endl;
#endif
    return 0;
}

static int init_check_data_array(tinymt64_t tinymt64[],
				 int total_num,
				 uint64_t seed_array[],
				 int size)
{
#if defined(DEBUG)
    cout << "init_check_data_array start" << endl;
#endif
    tinymt64_init_by_array(&tinymt64[0], seed_array, size);
    for (int i = 1; i < total_num; i++) {
	tinymt64[i] = tinymt64[i - 1];
	tinymt64_jump(&tinymt64[i],
		      tinymt64j_mag,
		      0,
		      tinymt64j_characteristic);
    }
#if defined(DEBUG)
    cout << "init_check_data_array end" << endl;
#endif
    return 0;
}

static void check_data(uint64_t * h_data,
		       int num_data,
		       int total_num)
{
#if defined(DEBUG)
    cout << "check_data start" << endl;
#endif
    int size = num_data / total_num;
#if defined(DEBUG)
    cout << "size = " << dec << size << endl;
#endif
    bool error = false;
    for (int i = 0; i < total_num; i++) {
	bool disp_flg = true;
	int count = 0;
	for (int j = 0; j < size; j++) {
	    uint64_t r = tinymt64_generate_uint64(&tinymt64[i]);
	    //if ((h_data[i * size + j] != r) && disp_flg) {
	    if ((h_data[j * total_num + i] != r) && disp_flg) {
		cout << "mismatch i = " << dec << i
		     << " j = " << dec << j
		     << " data = " << hex << h_data[j * total_num + i]
		     << " r = " << hex << r << endl;
		cout << "check_data check N.G!" << endl;
		count++;
		error = true;
	    }
	    if (count > 10) {
		disp_flg = false;
	    }
	}
    }
    if (!error) {
	cout << "check_data check O.K!" << endl;
    } else {
	throw cl::Error(-1, "tinymt64 check_data error!");
    }
#if defined(DEBUG)
    cout << "check_data end" << endl;
#endif
}

static void check_data12(double * h_data,
			 int num_data,
			 int total_num)
{
#if defined(DEBUG)
    cout << "check_data start" << endl;
#endif
    int size = num_data / total_num;
#if defined(DEBUG)
    cout << "size = " << dec << size << endl;
#endif
    bool error = false;
    for (int i = 0; i < total_num; i++) {
	bool disp_flg = true;
	int count = 0;
	for (int j = 0; j < size; j++) {
	    double r = tinymt64_generate_double12(&tinymt64[i]);
	    double d = h_data[j * total_num + i];
	    bool ok = (-FLT_EPSILON <= (r - d))
		&& ((r - d) <= FLT_EPSILON);
	    if (!ok && disp_flg) {
		cout << "mismatch i = " << dec << i
		     << " j = " << dec << j
		     << " data = " << hex << h_data[j * total_num + i]
		     << " r = " << hex << r << endl;
		cout << "check_data check N.G!" << endl;
		count++;
		error = true;
	    }
	    if (count > 10) {
		disp_flg = false;
	    }
	}
    }
    if (!error) {
	cout << "check_data check O.K!" << endl;
    } else {
	throw cl::Error(-1, "tinymt64 check_data error!");
    }
#if defined(DEBUG)
    cout << "check_data end" << endl;
#endif
}

static void check_data01(double * h_data,
			 int num_data,
			 int total_num)
{
#if defined(DEBUG)
    cout << "check_data start" << endl;
#endif
    int size = num_data / total_num;
#if defined(DEBUG)
    cout << "size = " << dec << size << endl;
#endif
    bool error = false;
    for (int i = 0; i < total_num; i++) {
	bool disp_flg = true;
	int count = 0;
	for (int j = 0; j < size; j++) {
	    double r = tinymt64_generate_double(&tinymt64[i]);
	    double d = h_data[j * total_num + i];
	    bool ok = (-FLT_EPSILON <= (r - d))
		&& ((r - d) <= FLT_EPSILON);
	    if (!ok && disp_flg) {
		cout << "mismatch i = " << dec << i
		     << " j = " << dec << j
		     << " data = " << hex << h_data[j * total_num + i]
		     << " r = " << hex << r << endl;
		cout << "check_data check N.G!" << endl;
		count++;
		error = true;
	    }
	    if (count > 10) {
		disp_flg = false;
	    }
	}
    }
    if (!error) {
	cout << "check_data check O.K!" << endl;
    } else {
	throw cl::Error(-1, "tinymt64 check_data error!");
    }
#if defined(DEBUG)
    cout << "check_data end" << endl;
#endif
}

static void check_status(tinymt64j_t * h_status,
			 int total_num)
{
#if defined(DEBUG)
    cout << "check_status start" << endl;
#endif
    typedef struct {
	uint64_t status[2];
    } sp;
    sp * dummy = (sp *)h_status;
    int counter = 0;
#if defined(DEBUG)
	cout << "device:" << endl;
	cout << "s0:" << hex << h_status[0].s0 << endl;
	cout << "s1:" << hex << h_status[0].s1 << endl;
	cout << "host:" << endl;
	cout << "s0:" << hex << tinymt64[0].status[0] << endl;
	cout << "s1:" << hex << tinymt64[0].status[1] << endl;
#endif
    for (int i = 0; i < total_num; i++) {
	for (int j = 0; j < 2; j++) {
	    uint64_t x = dummy[i].status[j];
	    uint64_t r = tinymt64[i].status[j];
	    if (j == 0) {
		x = x & TINYMT64_MASK;
		r = r & TINYMT64_MASK;
	    }
#if defined(DEBUG)
	    if (i == 0 && counter == 0) {
		cout << "i = " << dec << i
		     << " j = " << dec << j
		     << " device = " << hex << x
		     << " host = " << hex << r << endl;
	    }
#endif
	    if (x != r) {
		cout << "mismatch i = " << dec << i
		     << " j = " << dec << j
		     << " device = " << hex << x
		     << " host = " << hex << r << endl;
		cout << "check_status check N.G!" << endl;
		counter++;
	    }
	    if (counter > 10) {
		return;
	    }
	}
    }
    if (counter == 0) {
	cout << "check_status check O.K!" << endl;
    } else {
	throw cl::Error(-1, "tinymt64 check_status error!");
    }
#if defined(DEBUG)
    cout << "check_status end" << endl;
#endif
}

/* ==============
 * utility programs
 * ==============*/
static Buffer get_status_buff(int total_num)
{
#if defined(DEBUG)
    cout << "get_rec_buff start" << endl;
#endif
    Buffer status_buffer(context,
			 CL_MEM_READ_ONLY,
			 total_num * sizeof(tinymt64j_t));
#if defined(DEBUG)
    cout << "get_rec_buff end" << endl;
#endif
    return status_buffer;
}
