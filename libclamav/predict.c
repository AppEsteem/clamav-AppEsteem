// NOTE idk what this does
#if HAVE_CONFIG_H
#include "clamav-config.h"
#endif
#ifndef _WIN32
#include <dlfcn.h>
#endif

// for engine and LoadLibrary
#include "others.h"

static LogPredict_t g_logfunc = NULL;

// memory management calling back to internal funcs fmap_need_off and fmap_unneed_off
// we need these because predict threads can timeout and return empty results, leaving the
// thread still running and using the mapped memory. this lets the thread grab/release the
// memory when it starts/completes, separate from the timeout
void *cl_predict_grab_map(void *ref, size_t len)
{
    return fmap_need_off((fmap_t *) ref, 0, len);
}

void cl_predict_release_map(void *ref, size_t len)
{
    fmap_unneed_off((fmap_t *) ref, 0, len);
}

cl_error_t cl_set_predict_funcs(struct cl_engine* engine, Predict_t predict_handle, DisposePredictionResult_t dispose_handle, LogPredict_t log_handle)
{
    // cli_errmsg("cl_set_predict_funcs engine 0x%x predict_handle 0x%x\n");

    if(log_handle) {
        g_logfunc = log_handle;
        // send messages to func below
        cl_set_clcb_msg(predict_log);
    }

    if(engine) {
        engine->predict_handle = predict_handle;
        engine->dispose_prediction_result_handle = dispose_handle;
        return CL_SUCCESS;
    }

    return CL_ERROR;
}

void predict_log(enum cl_msg severity, const char *fullmsg, const char *msg, void *context)
{
    if(g_logfunc) {
        (g_logfunc)("LibScan", msg);
    }

    // else swallow the messages :-)
}

// this uses the memory mapped file. we pass the filepath only for debugging/reference
cl_error_t call_predict(cli_ctx *ctx) {
    uint32_t retval = CL_SUCCESS;
    // cli_errmsg("call_predict: ctx->target_filepath: %s\n", ctx->target_filepath);
 
    if(!ctx->engine->predict_handle || !ctx->engine->dispose_prediction_result_handle) {
        cli_errmsg("call_predict: call cl_set_predict_funcs first\n");
        return CL_ERROR;
    }

    const char* filename = ctx->target_filepath;
    if(ctx && ctx->sub_filepath)
    {
        filename = ctx->sub_filepath;
    }

    // map contains memory-mapped file... thread will have to grab/release this memory
    const void *buf = NULL;
    size_t len = 0;
    if(ctx && ctx->fmap && ctx->fmap->len) {
        len = ctx->fmap->len;
        buf = ctx->fmap;
    }

    // note that this may timeout - this is why we have moved the fmap memory need/unneding into the mlpredict code
    PredictionResult *result = ctx->engine->predict_handle(filename, buf, len);

    if (result && result->shouldcheck) {
        // note that the virus name must be some static string - nobody frees it later
        // also note: cli_append_virus will return CL_SUCCESS if this was an fp, and CL_VIRUS if not
        //      we need to trust the retval so we can honor the fp check
        const char *virname = PREDICT_VIRNAME;
        switch(result->confidence)
        {
            case 'H':
                virname = PREDICT_VIRNAME_H;
                break;
            case 'M':
                virname = PREDICT_VIRNAME_M;
                break;
        }
        retval = cli_append_virus(ctx, virname);
        // if(retval == CL_VIRUS) {
        //     cli_errmsg("filename [%s] confidence [%c] virname [%s]\n", filename, result->confidence, virname);
        // }
    }

    // free up prediction results
    if(result) {
        ctx->engine->dispose_prediction_result_handle(result);
        result = NULL;
    }

    return retval;
}

