
#include "evx1.h"
#include "evx1enc.h"
#include "evx1dec.h"

namespace evx {

evx_status create_encoder(evx1_encoder **output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    (*output) = new evx1_encoder_impl;

    if (!(*output))
    {
        return evx_post_error(EVX_ERROR_OUTOFMEMORY);
    }

    return EVX_SUCCESS;
}

evx_status create_decoder(evx1_decoder **output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    (*output) = new evx1_decoder_impl;

    if (!(*output))
    {
        return evx_post_error(EVX_ERROR_OUTOFMEMORY);
    }

    return EVX_SUCCESS;
}

evx_status destroy_encoder(evx1_encoder *input)
{
    if (EVX_PARAM_CHECK)
    {
        if (!input)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    evx1_encoder_impl *internal = (evx1_encoder_impl *) input;

    delete internal;

    return EVX_SUCCESS;
}

evx_status destroy_decoder(evx1_decoder *input)
{
    if (EVX_PARAM_CHECK)
    {
        if (!input)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    evx1_decoder_impl *internal = (evx1_decoder_impl *) input;

    delete internal;

    return EVX_SUCCESS;
}

} // namespace evx