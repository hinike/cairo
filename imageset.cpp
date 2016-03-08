
#include "imageset.h"

namespace evx {
  
image_set::image_set() {}

image_set::~image_set()
{
    deinitialize();
}

image *image_set::query_y_image() const
{
    return const_cast<image *>(&y_plane);
}

image *image_set::query_u_image() const
{
    return const_cast<image *>(&u_plane);
}

image *image_set::query_v_image() const
{
    return const_cast<image *>(&v_plane);
}

EVX_IMAGE_FORMAT image_set::query_image_format() const
{
    return y_plane.query_image_format();
}

evx_status image_set::initialize(EVX_IMAGE_FORMAT format, uint16 width, uint16 height)
{
    if (EVX_PARAM_CHECK)
    {
        if (0 == width || 0 == height)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    deinitialize();

    if (EVX_SUCCESS != create_image(format, width, height, &y_plane) ||
        EVX_SUCCESS != create_image(format, width >> 1, height >> 1, &u_plane) ||
        EVX_SUCCESS != create_image(format, width >> 1, height >> 1, &v_plane))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    return EVX_SUCCESS;
}

evx_status image_set::deinitialize()
{
    if (EVX_SUCCESS != destroy_image(&y_plane))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    } 

    if (EVX_SUCCESS != destroy_image(&u_plane))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (EVX_SUCCESS != destroy_image(&v_plane))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    return EVX_SUCCESS;
}

uint16 image_set::query_width() const
{
    return y_plane.query_width();
}

uint16 image_set::query_height() const
{
    return y_plane.query_height();
}

} // namespace evx