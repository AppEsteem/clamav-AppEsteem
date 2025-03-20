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

