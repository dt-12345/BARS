#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define BYTES_PER_FRAME 8
#define SAMPLES_PER_FRAME 14
#define NIBBLES_PER_FRAME 16

#ifdef COMPILING_DLL 
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

typedef struct
{
	int16_t coef[16];
	uint16_t gain;
	uint16_t pred_scale;
	int16_t yn1;
	int16_t yn2;

	uint16_t loop_pred_scale;
	int16_t loop_yn1;
	int16_t loop_yn2;
} ADPCMINFO;

DLLEXPORT void dsptool_encode(int16_t* src, uint8_t* dst, ADPCMINFO* cxt, uint32_t samples, uint8_t loop);
DLLEXPORT void dsptool_decode(uint8_t* src, int16_t* dst, ADPCMINFO* cxt, uint32_t samples);
DLLEXPORT void dsptool_getLoopContext(uint8_t* src, ADPCMINFO* cxt, uint32_t samples);

DLLEXPORT void dsptool_encodeFrame(int16_t* src, uint8_t* dst, int16_t* coefs, uint8_t one);
DLLEXPORT void dsptool_correlateCoefs(int16_t* src, uint32_t samples, int16_t* coefsOut);

DLLEXPORT uint32_t dsptool_getBytesForAdpcmBuffer(uint32_t samples);
DLLEXPORT uint32_t dsptool_getBytesForAdpcmSamples(uint32_t samples);
DLLEXPORT uint32_t dsptool_getBytesForPcmBuffer(uint32_t samples);
DLLEXPORT uint32_t dsptool_getBytesForPcmSamples(uint32_t samples);
DLLEXPORT uint32_t dsptool_getNibbleAddress(uint32_t samples);
DLLEXPORT uint32_t dsptool_getNibblesForNSamples(uint32_t samples);
DLLEXPORT uint32_t dsptool_getSampleForAdpcmNibble(uint32_t nibble);
DLLEXPORT uint32_t dsptool_getBytesForAdpcmInfo(void);

#ifdef __cplusplus
}
#endif