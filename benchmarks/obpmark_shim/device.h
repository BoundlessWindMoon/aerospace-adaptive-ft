/*
 * 最小化 device.h 替身（仅供单翻译单元编译 OBPMark 顺序 C 版内核使用）
 *
 * OBPMark 上游的 src/1.1-image/include/device.h 会经 util_prints.h -> benchmark.h
 * 引入 <string> 等 C++ 头，并声明重载的 init()，无法用 C 编译器处理。
 * 本替身只复刻其中"顺序 C 版本"（#else 分支）的数据类型定义，逐字对应上游：
 *   struct image_data_t { frames, num_frames, offsets, gains, bad_pixels,
 *                         scrub_mask, binned_frame, image_output }
 *   typedef struct { time_t t_test; } image_time_t;
 * 上游计算内核源码 (ports/cpu/processing.c) 未作任何修改。
 */
#ifndef DEVICE_H_
#define DEVICE_H_

#include <stdbool.h>
#include <time.h>
#include "obpmark.h"
#include "obpmark_image.h"

struct image_data_t
{
	frame16_t *frames;
	unsigned int num_frames;

	frame16_t offsets;
	frame16_t gains;
	frame8_t bad_pixels;

	frame8_t scrub_mask;

	frame32_t binned_frame;

	frame32_t image_output;
};
typedef struct image_data_t image_data_t;

typedef struct {
	time_t t_test;
} image_time_t;

/* ---- Benchmark 2.1 (CCSDS 121.0 data compression) 顺序 C 版类型，逐字对应
       third_party/obpmark/src/2.1-data_compression/include/device.h 的 #else 分支 ---- */
struct OutputBitStream; /* 仅前向声明，2.1 基准接入留待多翻译单元构建 */
struct compression_data_t
{
	unsigned int *InputDataBlock;
	unsigned int *OutputPreprocessedValue;
	struct OutputBitStream *OutputDataBlock;
	unsigned int n_bits;
	unsigned int j_blocksize;
	unsigned int r_samplesInterval;
	unsigned int steps;
	bool preprocessor_active;
	unsigned int TotalSamples;
	unsigned int TotalSamplesStep;
	bool debug_mode;
};
typedef struct compression_data_t compression_data_t;

typedef struct {
	time_t t_test;
} compression_time_t;

#endif // DEVICE_H_
