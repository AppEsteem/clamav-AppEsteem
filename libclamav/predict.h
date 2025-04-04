#ifndef __PREDICT_H
#define __PREDICT_H

#include "others.h"

typedef struct PredictionResult_t {
    char** keys; // Pointer to an array of strings
    double* values; // Pointer to an array of doubles
    int count; // Number of items
    char* verdict; // pointer to a string for the verdict
    bool shouldcheck; // true if we should flag this file for checking (deceptorheavy)
    int confidence; // how confident we are with this verdit (H/M/L) (cast to char)
} PredictionResult;

#define PREDICT_VIRNAME_H "Request_Inspection_HIGH.AE"
#define PREDICT_VIRNAME_M "Request_Inspection_MED.AE"
#define PREDICT_VIRNAME "Request_Inspection.AE"

// callbacks into aescan: do the prediction, dispose of the results, and log things
typedef PredictionResult* (*Predict_t)(const char *filename, const void *buf, size_t len);
typedef void (*DisposePredictionResult_t)(PredictionResult* result);
typedef void (*LogPredict_t)(const char *level, const char *msg);

cl_error_t call_predict(struct cli_ctx_tag *ctx);
void predict_log(enum cl_msg severity, const char *fullmsg, const char *msg, void *context);

#endif
