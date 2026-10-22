#ifndef OPENTAPE_WIRE_H
#define OPENTAPE_WIRE_H
#include "openaspi/types.h"

static void ot_put_u16(oa_u8 *out, oa_u16 value)
{
    out[0] = (oa_u8)(value & 0xffu);
    out[1] = (oa_u8)((value >> 8) & 0xffu);
}
static void ot_put_u32(oa_u8 *out, oa_u32 value)
{
    out[0] = (oa_u8)(value & 0xfful);
    out[1] = (oa_u8)((value >> 8) & 0xfful);
    out[2] = (oa_u8)((value >> 16) & 0xfful);
    out[3] = (oa_u8)((value >> 24) & 0xfful);
}
static oa_u16 ot_get_u16(const oa_u8 *in)
{
    return (oa_u16)((oa_u16)in[0] | ((oa_u16)in[1] << 8));
}
static oa_u32 ot_get_u32(const oa_u8 *in)
{
    return (oa_u32)in[0] | ((oa_u32)in[1] << 8) |
           ((oa_u32)in[2] << 16) | ((oa_u32)in[3] << 24);
}
#endif
