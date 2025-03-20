#ifndef __PREDICT_H
#define __PREDICT_H

#include "others.h"

typedef struct PredictionResult_t {
    char** keys; // Pointer to an array of strings
    double* values; // Pointer to an array of doubles
    int count; // Number of items
    char* verdict; // pointer to a string for the verdict
    bool shouldcheck; // true if we should flag this file for checking (deceptorheavy)
    int confidence; // how confident we are with this verdit (H/M/L)
} PredictionResult;

typedef PredictionResult* (*Predict_t)(const char *filename, const void *buf, uint32_t len);
typedef void (*DisposePredictionResult_t)(PredictionResult* result);

cl_error_t engine_unload_predict(struct cl_engine* engine);
cl_error_t engine_load_predict(struct cl_engine* engine);
cl_error_t call_predict(struct cli_ctx_tag *ctx);

#endif
