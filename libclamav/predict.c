// NOTE idk what this does
#if HAVE_CONFIG_H
#include "clamav-config.h"
#endif
#ifndef _WIN32
#include <dlfcn.h>
#endif

// for engine and LoadLibrary
#include "others.h"

cl_error_t cl_set_predict_funcs(struct cl_engine* engine, Predict_t predict_handle, DisposePredictionResult_t dispose_handle)
{
    // cli_errmsg("cl_set_predict_funcs engine 0x%x predict_handle 0x%x\n");

    if(engine) {
        engine->predict_handle = predict_handle;
        engine->dispose_prediction_result_handle = dispose_handle;
        return CL_SUCCESS;
    }

    return CL_ERROR;
}

/*
 * TODO/To investigate
 *
 * 1) see if we can use the ctx->map and not re-open the file
 */
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

    // map contains memory-mapped file... let's pass that in and hope it works :-)
    const void *buf = NULL;
    uint32_t len = 0;
    if(ctx && ctx->fmap && ctx->fmap->len) {
        len = ctx->fmap->len;
        if (!(buf = fmap_need_off(ctx->fmap, 0, ctx->fmap->len))) { // this was need_off_once
            cli_errmsg("call_predict: error reading map\n");
            return CL_EREAD;
        }
    }

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
            case 'L':
                virname = PREDICT_VIRNAME_L;
                break;
        }
        retval = cli_append_virus(ctx, virname);
    }

    // free up prediction results
    if(result) {
        ctx->engine->dispose_prediction_result_handle(result);
        result = NULL;
    }

    // un-need the memory mapping
    fmap_unneed_off(ctx->fmap, 0, ctx->fmap->len);

    return retval;
}

