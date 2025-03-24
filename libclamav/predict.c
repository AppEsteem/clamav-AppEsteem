// NOTE idk what this does
#if HAVE_CONFIG_H
#include "clamav-config.h"
#endif
#ifndef _WIN32
#include <dlfcn.h>
#endif


// for engine and LoadLibrary
#include "others.h"

cl_error_t engine_load_predict(struct cl_engine* engine)
{
    cl_error_t retval = CL_ERROR;

    do
    {
        // open libs and get addresses
#ifdef _WIN32
        engine->aepredict_handle = LoadLibrary("AePredict.dll");
        if (!engine->aepredict_handle) {
            cli_errmsg("cli_load_predict: LoadLibrary failed\n");
            retval = CL_ERROR;
            break;
        }
        engine->predict_handle = (Predict_t) GetProcAddress((HMODULE) engine->aepredict_handle, "AePredict");
        if(!engine->predict_handle) {
            cli_errmsg("cli_load_predict: GetProcAddress failed for AePredict\n");
            retval = CL_ERROR;
            break;
        }
        engine->dispose_prediction_result_handle = (DisposePredictionResult_t) GetProcAddress((HMODULE) engine->aepredict_handle, "DisposePredictionResult");
        if(!engine->dispose_prediction_result_handle) {
            cli_errmsg("cli_load_predict: GetProcAddress failed for DisposePredictionResult\n");
            retval = CL_ERROR;
            break;
        }
#else
        /*g_aepredict_handle = dlopen("libAePredict.so", RTLD_LAZY);*/
        engine->aepredict_handle = dlopen("libAePredict.so", RTLD_LAZY);
        if (!engine->aepredict_handle) {
            cli_errmsg("cli_load_predict: dlopen failed: %s\n", dlerror());
            retval = CL_ERROR;
            break;
        }
        engine->predict_handle = (Predict_t) dlsym(engine->aepredict_handle, "AePredict");
        if(!engine->predict_handle) {
            cli_errmsg("cli_load_predict: dlsym failed for AePredict: %s\n", dlerror());
            retval = CL_ERROR;
            break;
        }
        engine->dispose_prediction_result_handle = (DisposePredictionResult_t) dlsym(engine->aepredict_handle, "DisposePredictionResult");
        if(!engine->dispose_prediction_result_handle) {
            cli_errmsg("cli_load_predict: dlsym failed for DisposePredictionResult: %s\n", dlerror());
            retval = CL_ERROR;
            break;
        }
#endif
    } while(0);

    return retval;
}

cl_error_t engine_unload_predict(struct cl_engine *engine)
{
    cl_error_t retval = CL_ERROR;

    do
    {
        if(engine->aepredict_handle) {
#ifdef _WIN32
            FreeLibrary(engine->aepredict_handle);
#else
            dlclose(engine->aepredict_handle);
#endif
        }
        engine->aepredict_handle = NULL;
        engine->predict_handle = NULL;
        engine->dispose_prediction_result_handle = NULL;
    } while(0);

    return retval;
}

/*
 * TODO/To investigate
 *
 * 1) see if we can use the ctx->map and not re-open the file
 */
cl_error_t call_predict(cli_ctx *ctx) {
    uint32_t retval = CL_SUCCESS;
    /*cli_errmsg("call_predict: ctx->target_filepath: %s\n", ctx->target_filepath);*/
 
    if(!ctx->engine->aepredict_handle || !ctx->engine->predict_handle || !ctx->engine->dispose_prediction_result_handle) {
        cli_errmsg("call_predict: call cli_load_predict first\n");
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
        retval = cli_append_virus(ctx, PREDICT_VIRNAME);
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

