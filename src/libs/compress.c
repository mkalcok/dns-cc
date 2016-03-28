#include "compress.h"
#include <assert.h>
#include <zlib.h>

int deflate_data(FILE *source, int fd_dest) {
    int ret, flush;
    unsigned have;
    z_stream strm;
    ssize_t check;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];
    //FILE *output = fopen("/tmp/deflate.msg", "w");

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, LEVEL);
    if (ret != Z_OK) { return ret; }


    /* compress until end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void) deflateEnd(&strm);
            return Z_ERRNO;
        }
        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;
        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.next_out =  out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */

            have = CHUNK - strm.avail_out;

            check = write(fd_dest, out, have);
            //printf("%d\n,%s\n",e, strerror(errno));
            /*if ((e = write(&fd, out, have)) != 1) {
                printf("error\n%d\n",e);
                (void)deflateEnd(&strm);
                return Z_ERRNO;
            }*/


        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);     /* all input will be used */

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */
    /* clean up and return */
    (void) deflateEnd(&strm);
    return Z_OK;
}

int inflate_data(void **args) {
    int dest_fd = (int) args[0];
    int source_fd = (int) args[1];
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    int i = 0, check = 0;
    if (ret != Z_OK)
        return ret;

    /* decompress until deflate stream ends or end of file */
    do {
        for (i = 0; i < CHUNK; i++) {
            strm.avail_in++;
            check = read(source_fd, &in[i], 1);
            if (check == 0) { break; }
        }
        /*if (ferror(source_fd)) {
            (void)inflateEnd(&strm);
            return Z_ERRNO;
        }*/
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
                case Z_NEED_DICT:
                    ret = Z_DATA_ERROR;     /* and fall through */
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                    (void) inflateEnd(&strm);
                    return ret;
                default:
                    break;
            }
            have = CHUNK - strm.avail_out;
            if (write(dest_fd, out, have) != have) {
                (void) inflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void) inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

