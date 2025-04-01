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
const void *cl_predict_grab_map(void *ref, size_t len)
{
    return fmap_need_off((fmap_t *) ref, 0, len);
}

void cl_predict_release_map(void *ref, size_t len)
{
    fmap_t *m = (fmap_t *) ref;
    fmap_unneed_off(m, 0, len);

    // if we had timed out before and now we're waiting for release, we can unmap
    if(m->timed_out && m->waiting_for_release) {
        m->timed_out = false;
        m->waiting_for_release = false;
        funmap(m);
    }
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

// set tmmpdir. we put this code here because it uses an enum which may change over time
cl_error_t cl_predict_set_tempdir(struct cl_engine *engine, char *tmpdir)
{
    cl_error_t ret = CL_SUCCESS;
    if ((ret = cl_engine_set_str(engine, CL_ENGINE_TMPDIR, tmpdir))) {
        cli_errmsg("cli_engine_set_str(CL_ENGINE_TMPDIR) failed: %s\n", cl_strerror(ret));
        ret = CL_ERROR;
    } else {
        cli_errmsg("TemporaryDirectory set to %s\n", tmpdir);
    }
    return ret;
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

        // note that this may timeout - this is why we have moved the fmap memory need/unneding into the mlpredict code
        PredictionResult *result = NULL;
        
        // don't call prediction for any embedded files
        if(ctx && filename == ctx->target_filepath) {
            // don't pass buf and len for now
            result = ctx->engine->predict_handle(filename, NULL, 0);
            // result = ctx->engine->predict_handle(filename, buf, len);
        }

        if(result == (int) (-1)) { // special timeout
            // the only way we get here was after a timeout
            if(ctx->sub_filepath) {
                cli_errmsg("TIMEOUT filename [%s] on embedded [%s] len [%d]\n", ctx->target_filepath, filename, len);
            } else {
                cli_errmsg("TIMEOUT filename [%s] len [%d]\n", filename, ctx->target_filepath, len);
            }

            // set the map->timed_out to true so that funmap doesn't delete it while the thread continues to run
            ctx->fmap->timed_out = true;

            // TODO: trying to account for a race condition here where the timeed-out thread finishes after the line above -- still not perfect
            if(ctx->fmap->waiting_for_release) {
                ctx->fmap->timed_out = false;
                ctx->fmap->waiting_for_release = false;
                funmap(ctx->fmap);
            }
        }
        else if (result > 0 && result->shouldcheck) {
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

        // free up prediction results if we received a valid result
        if(result != (int) (-1) && result > 0) {
            ctx->engine->dispose_prediction_result_handle(result);
            result = NULL;
        }
    }

    return retval;
}

