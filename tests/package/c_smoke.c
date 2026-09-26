/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/* Links the C API from an installed bundle with any C compiler: writes a
 * LAZ file with the parallel compressor and reads it back.
 * Usage: c_smoke <output.laz> */

#include <lasrs/lasrs.h>
#include <stdio.h>
#include <string.h>

static int fail(const char *step)
{
    LasrsStr message = lasrs_last_error();
    fprintf(stderr, "%s failed: %.*s\n", step, (int)message.len, (const char *)message.ptr);
    return 1;
}

int main(int argc, char **argv)
{
    enum
    {
        POINTS = 100000
    };
    LasrsStr path;
    LasrsHeader *header;
    LasrsWriter *writer = NULL;
    LasrsReader *reader = NULL;
    LasrsPointData *points = NULL;
    LasrsPoint point;
    size_t i;
    size_t read;

    if (argc != 2)
    {
        fprintf(stderr, "usage: c_smoke <output.laz>\n");
        return 2;
    }
    path.ptr = (const uint8_t *)argv[1];
    path.len = strlen(argv[1]);

    header = lasrs_header_default();
    if (lasrs_writer_from_path(path, header, &writer) != LASRS_STATUS_OK)
    {
        return fail("open writer");
    }
    lasrs_header_free(header);

    memset(&point, 0, sizeof(point));
    for (i = 0; i < POINTS; ++i)
    {
        point.x = (double)i;
        point.intensity = (uint16_t)i;
        if (lasrs_writer_write_point(writer, &point, NULL) != LASRS_STATUS_OK)
        {
            return fail("write point");
        }
    }
    if (lasrs_writer_close(writer) != LASRS_STATUS_OK || lasrs_writer_free(writer) != LASRS_STATUS_OK)
    {
        return fail("close writer");
    }

    if (lasrs_reader_from_path(path, &reader) != LASRS_STATUS_OK)
    {
        return fail("open reader");
    }
    if (lasrs_reader_read_all(reader, &points) != LASRS_STATUS_OK)
    {
        return fail("read points");
    }
    read = lasrs_point_data_len(points);
    lasrs_point_data_free(points);
    lasrs_reader_free(reader);

    printf("lasrs ABI %u: wrote and read %u of %u points\n", (unsigned)lasrs_abi_version(), (unsigned)read,
           (unsigned)POINTS);
    return read == POINTS ? 0 : 1;
}
