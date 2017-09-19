/*
(C) 2011-2016 by Przemyslaw Skibinski (inikep@gmail.com)

    LICENSE

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 3 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details at
    Visit <http://www.gnu.org/copyleft/gpl.html>.

*/

#include "lzbench.h"
#include "util.h"
// #include <cmath>
#include <numeric>
#include <algorithm> // sort
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifndef BENCH_REMOVE_FASTPFOR
    #include "fastpfor/deltautil.h"  // for simd delta preproc
#endif

int istrcmp(const char *str1, const char *str2)
{
    int c1, c2;
    while (1) {
        c1 = tolower((unsigned char)(*str1++));
        c2 = tolower((unsigned char)(*str2++));
        if (c1 == 0 || c1 != c2) return c1 == c2 ? 0 : c1 > c2 ? 1 : -1;
    }
}

void format(std::string& s,const char* formatstring, ...)
{
   char buff[1024];
   va_list args;
   va_start(args, formatstring);

#ifdef WIN32
   _vsnprintf( buff, sizeof(buff), formatstring, args);
#else
   vsnprintf( buff, sizeof(buff), formatstring, args);
#endif

   va_end(args);

   s=buff;
}


std::vector<std::string> split(const std::string &text, char sep)
{
  std::vector<std::string> tokens;
  std::size_t start = 0, end = 0;
  while (text[start] == sep) start++;
  while ((end = text.find(sep, start)) != std::string::npos) {
    tokens.push_back(text.substr(start, end - start));
    start = end + 1;
  }
  tokens.push_back(text.substr(start));
  return tokens;
}


void print_header(lzbench_params_t *params)
{
    switch (params->textformat)
    {
        case CSV:
            if (params->show_speed)
                printf("Compressor name,Compression speed,Decompression speed,Original size,Compressed size,Ratio,Filename\n");
            else
                printf("Compressor name,Compression time in us,Decompression time in us,Original size,Compressed size,Ratio,Filename\n"); break;
            break;
        case TURBOBENCH:
            printf("  Compressed  Ratio   Cspeed   Dspeed         Compressor name Filename\n"); break;
        case TEXT:
            printf("Compressor name         Compress. Decompress. Compr. size  Ratio Filename\n"); break;
        case TEXT_FULL:
            printf("Compressor name         Compress. Decompress.  Orig. size  Compr. size  Ratio Filename\n"); break;
        case MARKDOWN:
            printf("| Compressor name         | Compression| Decompress.| Compr. size | Ratio | Filename |\n");
            printf("| ---------------         | -----------| -----------| ----------- | ----- | -------- |\n");
            break;
        case MARKDOWN2:
            printf("| Compressor name         | Ratio | Compression| Decompress.|\n");
            printf("| ---------------         | ------| -----------| ---------- |\n");
            break;
    }
}


void print_speed(lzbench_params_t *params, string_table_t& row)
{
    float cspeed, dspeed, ratio;
    cspeed = row.col5_origsize * 1000.0 / row.col2_ctime;
    dspeed = (!row.col3_dtime) ? 0 : (row.col5_origsize * 1000.0 / row.col3_dtime);
    ratio = row.col4_comprsize * 100.0 / row.col5_origsize;

    switch (params->textformat)
    {
        case CSV:
            printf("%s,%.2f,%.2f,%llu,%llu,%.2f,%s\n", row.col1_algname.c_str(), cspeed, dspeed, (unsigned long long)row.col5_origsize, (unsigned long long)row.col4_comprsize, ratio, row.col6_filename.c_str()); break;
        case TURBOBENCH:
            printf("%12llu %6.1f%9.2f%9.2f  %22s %s\n", (unsigned long long)row.col4_comprsize, ratio, cspeed, dspeed, row.col1_algname.c_str(), row.col6_filename.c_str()); break;
        case TEXT:
        case TEXT_FULL:
            printf("%-23s", row.col1_algname.c_str());
            if (cspeed < 10) printf("%6.2f MB/s", cspeed); else printf("%6d MB/s", (int)cspeed);
            if (!dspeed)
                printf("      ERROR");
            else
                if (dspeed < 10) printf("%6.2f MB/s", dspeed); else printf("%6d MB/s", (int)dspeed);
            if (params->textformat == TEXT_FULL)
                printf("%12llu %12llu %6.2f %s\n", (unsigned long long) row.col5_origsize, (unsigned long long)row.col4_comprsize, ratio, row.col6_filename.c_str());
            else
                printf("%12llu %6.2f %s\n", (unsigned long long)row.col4_comprsize, ratio, row.col6_filename.c_str());
            break;
        case MARKDOWN:
            printf("| %-23s ", row.col1_algname.c_str());
            if (cspeed < 10) printf("|%6.2f MB/s ", cspeed); else printf("|%6d MB/s ", (int)cspeed);
            if (!dspeed)
                printf("|      ERROR ");
            else
                if (dspeed < 10) printf("|%6.2f MB/s ", dspeed); else printf("|%6d MB/s ", (int)dspeed);
            printf("|%12llu |%6.2f | %-s|\n", (unsigned long long)row.col4_comprsize, ratio, row.col6_filename.c_str());
            break;
        case MARKDOWN2:
            ratio = 1.0*row.col5_origsize / row.col4_comprsize;
            printf("| %-23s |%6.3f ", row.col1_algname.c_str(), ratio);
            if (cspeed < 10) printf("|%6.2f MB/s ", cspeed); else printf("|%6d MB/s ", (int)cspeed);
            if (!dspeed)
                printf("|      ERROR ");
            else
                if (dspeed < 10) printf("|%6.2f MB/s ", dspeed); else printf("|%6d MB/s ", (int)dspeed);
            printf("|\n");
            break;
    }
}


void print_time(lzbench_params_t *params, string_table_t& row)
{
    float ratio = row.col4_comprsize * 100.0 / row.col5_origsize;
    uint64_t ctime = row.col2_ctime / 1000;
    uint64_t dtime = row.col3_dtime / 1000;

    switch (params->textformat)
    {
        case CSV:
            printf("%s,%llu,%llu,%llu,%llu,%.2f,%s\n", row.col1_algname.c_str(), (unsigned long long)ctime, (unsigned long long)dtime,  (unsigned long long) row.col5_origsize, (unsigned long long)row.col4_comprsize, ratio, row.col6_filename.c_str()); break;
        case TURBOBENCH:
            printf("%12llu %6.1f%9llu%9llu  %22s %s\n", (unsigned long long)row.col4_comprsize, ratio, (unsigned long long)ctime, (unsigned long long)dtime, row.col1_algname.c_str(), row.col6_filename.c_str()); break;
        case TEXT:
        case TEXT_FULL:
            printf("%-23s", row.col1_algname.c_str());
            printf("%8llu us", (unsigned long long)ctime);
            if (!dtime)
                printf("      ERROR");
            else
                printf("%8llu us", (unsigned long long)dtime);
            if (params->textformat == TEXT_FULL)
                printf("%12llu %12llu %6.2f %s\n", (unsigned long long) row.col5_origsize, (unsigned long long)row.col4_comprsize, ratio, row.col6_filename.c_str());
            else
                printf("%12llu %6.2f %s\n", (unsigned long long)row.col4_comprsize, ratio, row.col6_filename.c_str());
            break;
        case MARKDOWN:
            printf("| %-23s ", row.col1_algname.c_str());
            printf("|%8llu us ", (unsigned long long)ctime);
            if (!dtime)
                printf("|      ERROR ");
            else
                printf("|%8llu us ", (unsigned long long)dtime);
            printf("|%12llu |%6.2f | %-s|\n", (unsigned long long)row.col4_comprsize, ratio, row.col6_filename.c_str());
            break;
        case MARKDOWN2:
            printf("MARKDOWN2 not supported!\n");
            break;
    }
}


void print_stats(lzbench_params_t *params, const compressor_desc_t* desc, int level, std::vector<uint64_t> &ctime, std::vector<uint64_t> &dtime, size_t insize, size_t outsize, bool decomp_error)
{
    std::string col1_algname;
    std::sort(ctime.begin(), ctime.end());
    std::sort(dtime.begin(), dtime.end());
    uint64_t best_ctime, best_dtime;

    switch (params->timetype)
    {
        default:
        case FASTEST:
            best_ctime = ctime.empty()?0:ctime[0];
            best_dtime = dtime.empty()?0:dtime[0];
            break;
        case AVERAGE:
            best_ctime = ctime.empty()?0:std::accumulate(ctime.begin(),ctime.end(),(uint64_t)0) / ctime.size();
            best_dtime = dtime.empty()?0:std::accumulate(dtime.begin(),dtime.end(),(uint64_t)0) / dtime.size();
            break;
        case MEDIAN:
            best_ctime = ctime.empty()?0:(ctime[(ctime.size()-1)/2] + ctime[ctime.size()/2]) / 2;
            best_dtime = dtime.empty()?0:(dtime[(dtime.size()-1)/2] + dtime[dtime.size()/2]) / 2;
            break;
    }

    if (desc->first_level == 0 && desc->last_level==0)
        format(col1_algname, "%s %s", desc->name, desc->version);
    else
        format(col1_algname, "%s %s -%d", desc->name, desc->version, level);

    params->results.push_back(string_table_t(col1_algname, best_ctime, (decomp_error)?0:best_dtime, outsize, insize, params->in_filename));
    if (params->show_speed)
        print_speed(params, params->results[params->results.size()-1]);
    else
        print_time(params, params->results[params->results.size()-1]);

    ctime.clear();
    dtime.clear();
}


size_t common(uint8_t *p1, uint8_t *p2)
{
    size_t size = 0;

    while (*(p1++) == *(p2++))
        size++;

    return size;
}


// size_t round_up_to_multiple_of(size_t x, size_t multipleof) {
//     size_t remainder = x % multipleof;
//     return remainder ? (multipleof - remainder) : x;
// }

/* Allocate aligned memory in a portable way.
 *
 * Memory allocated with aligned alloc *MUST* be freed using aligned_free.
 *
 * @param alignment The number of bytes to which memory must be aligned. This
 *  value *must* be <= 255.
 * @param bytes The number of bytes to allocate.
 * @param zero If true, the returned memory will be zeroed. If false, the
 *  contents of the returned memory are undefined.
 * @returns A pointer to `size` bytes of memory, aligned to an `alignment`-byte
 *  boundary.
 */
void *aligned_alloc(size_t alignment, size_t size, bool zero) {
    size_t request_size = size + alignment;
    char* buf = (char*)(zero ? calloc(1, request_size) : malloc(request_size));

    size_t remainder = ((size_t)buf) % alignment;
    size_t offset = alignment - remainder;
    char* ret = buf + (unsigned char)offset;

    // store how many extra bytes we allocated in the byte just before the
    // pointer we return
    *(unsigned char*)(ret - 1) = offset;

    return (void*)ret;
}

/* Free memory allocated with aligned_alloc */
void aligned_free(void* aligned_ptr) {
    int offset = *(((char*)aligned_ptr) - 1);
    free(((char*)aligned_ptr) - offset);
}

uint8_t* alloc_data_buffer(size_t size) {
    void* buf;
    if (ALIGN_BYTES > 1) {
        buf = aligned_alloc(ALIGN_BYTES, size, true);
    } else {
        buf = calloc(1, size);
    }
    if (buf != NULL) {
        volatile char zero = 0;
        for (size_t i = 0; i < size; i += MIN_PAGE_SIZE) {
            static_cast<char * volatile>(buf)[i] = zero;
        }
    }
    return reinterpret_cast<uint8_t*>(buf);
}

void free_data_buffer(void* ptr) {
    if (ALIGN_BYTES > 1) {
        aligned_free(ptr);
    } else {
        free(ptr);
    }
}


void apply_preprocessors(const std::vector<int64_t>& preprocessors, uint8_t* inbuf,
                         size_t size, int element_sz, uint8_t* outbuf)
{
    // printf("using %lu preprocessors; size=%lu, element_sz=%d\n", preprocessors.size(), size, element_sz);

    if (preprocessors.size() < 1) { return; }

    int sz = element_sz;
    if (sz < 1) {
        sz = 1;
    }
    int64_t nelements = size / sz;

    // printf("size=%lu, sz=%lu, element_sz=%d\n", size, sz, element_sz);
    // printf("size=%lu, element_sz=%lu, nelements=%lld\n", size, sz, element_sz, nelements);


    for (auto preproc : preprocessors) {
        // printf("applying preproc: %lld with nelements=%lld, element_sz=%d\n", preproc, nelements, sz);
        // continue;

        if ((preproc < 1) || (preproc > 4)) {
            printf("WARNING: ignoring unrecognized preprocessor number '%lld'\n", preproc);
            continue;
        }

        // memcpy(outbuf, inbuf, size); continue;  // TODO rm

#ifndef BENCH_REMOVE_FASTPFOR   // use simd delta if available
        if (sz == 4 && preproc == DELTA4)  {
            memcpy(outbuf, inbuf, size);
            FastPForLib::Delta::deltaSIMD((uint32_t*)outbuf, nelements);
            continue;
        }
#endif
        int offset = preproc;  // simplifying hack based on enum values

        memcpy(outbuf, inbuf, offset * sz);
        if (sz == 1) {
            auto in = (uint8_t*)inbuf;
            auto out = (uint8_t*)outbuf;
            for (int i = offset; i < nelements; i++) {
                out[i] = in[i] - in[i-offset];
            }
        }
        else if (sz == 2) {
            auto in = (uint16_t*)inbuf;
            auto out = (uint16_t*)outbuf;
            for (int i = offset; i < nelements; i++) {
                out[i] = in[i] - in[i-offset];
            }
        } else if (sz == 4) {
            auto in = (uint32_t*)inbuf;
            auto out = (uint32_t*)outbuf;
            for (int i = offset; i < nelements; i++) {
                out[i] = in[i] - in[i-offset];
            }
        } else if (sz == 8) {
            auto in = (uint64_t*)inbuf;
            auto out = (uint64_t*)outbuf;
            for (int i = offset; i < nelements; i++) {
                out[i] = in[i] - in[i-offset];
            }
        } else {
            printf("WARNING: ignoring invalid element size '%d'; must be in {1,2,4,8}\n", element_sz);
            // memcpy(outbuf, inbuf, size);
        }

        // offset = 1; // TODO rm
        // if (offset < 0) {
        //     offset = 1;
        // }
        // size_t end = nelements - 1;

        // printf("---- end = %lu\n", end);
        // continue; // TODO rm

        // printf("applying preproc: %lld with element sz %lu\n", preproc, sz);

        // if (sz <= 1) {
        //     // printf("really gonna use preproc: %lld with element sz 1\n", preproc);
        //     auto in = inbuf;
        //     for (size_t i = end; i >= offset; i--) { in[i] -= in[i-offset]; }
        //     // for (size_t i = end; i >= offset; i--) { in[i] -= in[i-offset]; }
        //     // printf("used preproc: %lld on input of size %lu\n", preproc, end);
        // } else if (sz == 2) {
        //     // printf("actually applying preproc: %lld with element sz %lu\n", preproc, sz);
        //     uint16_t* in = (uint16_t*)inbuf;
        //     for (size_t i = end; i >= offset; i--) { in[i] -= in[i-offset]; }
        // } else if (sz == 4) {
        //     // printf("actually applying preproc: %lld with element sz %lu\n", preproc, sz);
        //     uint32_t* in = (uint32_t*)inbuf;
        //     for (size_t i = end; i >= offset; i--) { in[i] -= in[i-offset]; }
        // } else {
        //     // printf("mysteriously didn't apply preproc...\n");
        // }
    }
}

void undo_preprocessors(const std::vector<int64_t>& preprocessors, uint8_t* inbuf,
                        size_t size, int element_sz, uint8_t* outbuf)
{
    // printf("using %lu preprocessors; size=%lu, element_sz=%d\n", preprocessors.size(), size, element_sz);

    if (preprocessors.size() < 1) { return; }

    int sz = element_sz;
    if (sz < 1) {
        sz = 1;
    }
    int64_t nelements = size / sz;

    // printf("size=%lu, sz=%lu, element_sz=%d\n", size, sz, element_sz);
    // printf("size=%lu, element_sz=%lu, nelements=%lld\n", size, sz, element_sz, nelements);


    for (auto preproc : preprocessors) {
        // printf("applying preproc: %lld with nelements=%lld, element_sz=%d\n", preproc, nelements, sz);
        // continue;

        if ((preproc < 1) || (preproc > 4)) {
            printf("WARNING: ignoring unrecognized preprocessor number '%lld'\n", preproc);
            continue;
        }

        // memcpy(outbuf, inbuf, size); continue;  // TODO rm

#ifndef BENCH_REMOVE_FASTPFOR   // use simd delta if available
        if (sz == 4 && preproc == DELTA4)  {
            memcpy(outbuf, inbuf, size);
            FastPForLib::Delta::inverseDeltaSIMD((uint32_t*)outbuf, nelements);
            continue;
        }
#endif
        int offset = preproc;  // simplifying hack based on enum values

        memcpy(outbuf, inbuf, offset * sz);
        if (sz <= 1) {
            auto in = (uint8_t*)inbuf;
            auto out = (uint8_t*)outbuf;
            for (int i = offset; i < nelements; i++) {
                out[i] = in[i] + out[i-offset];
            }
        }
        else if (sz == 2) {
            auto in = (uint16_t*)inbuf;
            auto out = (uint16_t*)outbuf;
            for (int i = offset; i < nelements; i++) {
                out[i] = in[i] + out[i-offset];
            }
        } else if (sz == 4) {
            auto in = (uint32_t*)inbuf;
            auto out = (uint32_t*)outbuf;
            for (int i = offset; i < nelements; i++) {
                out[i] = in[i] + out[i-offset];
            }
        } else if (sz == 8) {
            auto in = (uint64_t*)inbuf;
            auto out = (uint64_t*)outbuf;
            for (int i = offset; i < nelements; i++) {
                out[i] = in[i] + out[i-offset];
            }
        } else {
            printf("WARNING: ignoring invalid element size '%d'; must be in {1,2,4,8}\n", element_sz);
        }
    }
}

inline int64_t lzbench_compress(lzbench_params_t *params,
    std::vector<size_t>& chunk_sizes, compress_func compress,
    std::vector<size_t> &compr_sizes, uint8_t *inbuf, uint8_t *outbuf,
    uint8_t* tmpbuf, size_t outsize, size_t param1, size_t param2,
    char* workmem)
{
    int64_t clen;
    size_t outpart, part, sum = 0;
    uint8_t *start = inbuf;
    int cscount = chunk_sizes.size();

    compr_sizes.resize(cscount);

    for (int i=0; i<cscount; i++)
    {
        part = chunk_sizes[i];
        outpart = GET_COMPRESS_BOUND(part);
        if (outpart > outsize) outpart = outsize;

        uint8_t* inptr = inbuf;
        if (params->preprocessors.size() > 0) {
            apply_preprocessors(params->preprocessors, inbuf, part, params->element_sz, tmpbuf);
            inptr = tmpbuf;
        }

        clen = compress((char*)inptr, part, (char*)outbuf, outpart, param1, param2, workmem);
        LZBENCH_PRINT(9, "ENC part=%d clen=%d in=%d\n", (int)part, (int)clen, (int)(inbuf-start));

        if (clen <= 0 || clen == part)
        {
            if (part > outsize) return 0;
            memcpy(outbuf, inbuf, part);
            clen = part;
        }

        inbuf += part;
        outbuf += clen;
        outsize -= clen;
        compr_sizes[i] = clen;
        sum += clen;
    }
    return sum;
}


inline int64_t lzbench_decompress(lzbench_params_t *params,
    std::vector<size_t>& chunk_sizes, compress_func decompress,
    std::vector<size_t> &compr_sizes, uint8_t *inbuf, uint8_t *outbuf,
    uint8_t* tmpbuf, size_t param1, size_t param2, char* workmem)
{
    int64_t dlen;
    size_t part, sum = 0;
    uint8_t *outstart = outbuf;
    int cscount = compr_sizes.size();

    bool has_preproc = params->preprocessors.size() > 0;

    LZBENCH_PRINT(9, "---- Decompressing %d chunks\n", (int)chunk_sizes.size());
    for (int i = 0; i < chunk_sizes.size(); i++) {
        LZBENCH_PRINT(9, "Chunk %d: orig size, compressed size = %d, %d\n",
            (int)i, (int)chunk_sizes[i], (int)compr_sizes[i]);
    }
    for (int i=0; i<cscount; i++)
    {
        part = compr_sizes[i];
        if (part == chunk_sizes[i]) // uncompressed
        {
            memcpy(outbuf, inbuf, part);
            dlen = part;
        }
        else
        {
            uint8_t* outptr = has_preproc ? tmpbuf : outbuf;
            LZBENCH_PRINT(9, "chunk %d: about to decompress\n", i);
            // printf("decompress func: %p, %p, %p\n", decompress, inbuf, outbuf);
            // uint8_t* outptr = outbuf; // TODO rm
            // uint8_t* outptr = tmpbuf; // TODO rm
            dlen = decompress((char*)inbuf, part, (char*)outptr, chunk_sizes[i], param1, param2, workmem);

            if (has_preproc) {
                undo_preprocessors(params->preprocessors, tmpbuf, dlen, params->element_sz, outbuf);
            }
        }

        LZBENCH_PRINT(9, "chunk %d: DEC part=%d dlen=%d out=%d\n",
            i, (int)part, (int)dlen, (int)(outbuf - outstart));
        if (dlen <= 0) return dlen;

        inbuf += part;
        outbuf += dlen;
        sum += dlen;
    }

    return sum;
}


void lzbench_test(lzbench_params_t *params, std::vector<size_t> &file_sizes,
    const compressor_desc_t* desc, int level, uint8_t *inbuf, size_t insize,
    uint8_t *compbuf, size_t comprsize, uint8_t *decomp, bench_rate_t rate,
    size_t param1)
{
    float speed;
    int i, total_c_iters, total_d_iters;
    bench_timer_t loop_ticks, start_ticks, end_ticks, timer_ticks;
    int64_t complen=0, decomplen;
    uint64_t nanosec, total_nanosec;
    std::vector<uint64_t> ctime, dtime;
    std::vector<size_t> compr_sizes, chunk_sizes;
    bool decomp_error = false;
    char* workmem = NULL;
    size_t param2 = desc->additional_param;
    size_t chunk_size = (params->chunk_size > insize) ? insize : params->chunk_size;

    uint8_t* tmpbuf = alloc_data_buffer(insize);
    // memcpy(tmpbuf, inbuf, insize);

    LZBENCH_PRINT(5, "*** trying %s insize=%d comprsize=%d chunk_size=%d\n", desc->name, (int)insize, (int)comprsize, (int)chunk_size);

    if (desc->max_block_size != 0 && chunk_size > desc->max_block_size) chunk_size = desc->max_block_size;
    if (!desc->compress || !desc->decompress) goto done;
    if (desc->init) workmem = desc->init(chunk_size, param1, param2);

    if (params->cspeed > 0)
    {
        size_t part = MIN(100*1024, chunk_size);
        GetTime(start_ticks);
        int64_t clen = desc->compress((char*)inbuf, part, (char*)compbuf, comprsize, param1, param2, workmem);
        GetTime(end_ticks);
        nanosec = GetDiffTime(rate, start_ticks, end_ticks)/1000;
        if (clen>0 && nanosec>=1000)
        {
            part = (part / nanosec); // speed in MB/s
            if (part < params->cspeed) { LZBENCH_PRINT(7, "%s (100K) slower than %d MB/s nanosec=%d\n", desc->name, (uint32_t)part, (uint32_t)nanosec); goto done; }
        }
    }

    for (int i=0; i<file_sizes.size(); i++) {
        size_t tmpsize = file_sizes[i];
        while (tmpsize > 0)
        {
            chunk_sizes.push_back(MIN(tmpsize, chunk_size));
            tmpsize -= MIN(tmpsize, chunk_size);
        }
    }

    LZBENCH_PRINT(5, "%s using %d chunks\n", desc->name, (int)chunk_sizes.size());

    total_c_iters = 0;
    GetTime(timer_ticks);
    do
    {
        i = 0;
        uni_sleep(1); // give processor to other processes
        GetTime(loop_ticks);
        do
        {
            // if (!params->time_preproc) {
            //     apply_preprocessors(params->preprocessors, inbuf, part, params->element_sz);
            // }
            GetTime(start_ticks);
            complen = lzbench_compress(params, chunk_sizes, desc->compress,
                compr_sizes, inbuf, compbuf, tmpbuf, comprsize, param1, param2, workmem);
            GetTime(end_ticks);
            nanosec = GetDiffTime(rate, start_ticks, end_ticks);
            if (nanosec >= 10000) { ctime.push_back(nanosec); }
            i++;
        }
        while (GetDiffTime(rate, loop_ticks, end_ticks) < params->cloop_time);

        nanosec = GetDiffTime(rate, loop_ticks, end_ticks);
        ctime.push_back(nanosec/i);
        speed = (float)insize*i*1000/nanosec;
        LZBENCH_PRINT(8, "%s nanosec=%d\n", desc->name, (int)nanosec);

        if ((uint32_t)speed < params->cspeed) { LZBENCH_PRINT(7, "%s slower than %d MB/s\n", desc->name, (uint32_t)speed); return; }

        total_nanosec = GetDiffTime(rate, timer_ticks, end_ticks);
        total_c_iters += i;
        if ((total_c_iters >= params->c_iters) && (total_nanosec > ((uint64_t)params->cmintime*1000000))) break;
        LZBENCH_PRINT(2, "%s compr iter=%d time=%.2fs speed=%.2f MB/s     \r", desc->name, total_c_iters, total_nanosec/1000000000.0, speed);
    }
    while (true);


    total_d_iters = 0;
    GetTime(timer_ticks);
    if (!params->compress_only)
    do
    {
        i = 0;
        uni_sleep(1); // give processor to other processes
        GetTime(loop_ticks);
        do
        {
            GetTime(start_ticks);
            decomplen = lzbench_decompress(params, chunk_sizes, desc->decompress, compr_sizes, compbuf, decomp, tmpbuf, param1, param2, workmem);
            GetTime(end_ticks);
            nanosec = GetDiffTime(rate, start_ticks, end_ticks);
            if (nanosec >= 10000) dtime.push_back(nanosec);
            i++;
        }
        while (GetDiffTime(rate, loop_ticks, end_ticks) < params->dloop_time);

        nanosec = GetDiffTime(rate, loop_ticks, end_ticks);
        dtime.push_back(nanosec/i);
        LZBENCH_PRINT(9, "%s dnanosec=%d\n", desc->name, (int)nanosec);

        if (insize != decomplen)
        {
            decomp_error = true;
            LZBENCH_PRINT(5, "ERROR: input length (%d) != decompressed length (%d)\n", (int32_t)insize, (int32_t)decomplen);
        }

        if (memcmp(inbuf, decomp, insize) != 0)
        {
            decomp_error = true;

            size_t cmn = common(inbuf, decomp);
            LZBENCH_PRINT(5, "ERROR in %s: only = %d / %d decompressed bytes were correct\n", desc->name, (int32_t)cmn, (int32_t)insize);

            if (params->verbose >= 10)
            {
                char text[256];
                snprintf(text, sizeof(text), "%s_failed", desc->name);
                cmn /= chunk_size;
                size_t err_size = MIN(insize, (cmn+1)*chunk_size);
                err_size -= cmn*chunk_size;
                printf("ERROR: fwrite %d-%d to %s\n", (int32_t)(cmn*chunk_size), (int32_t)(cmn*chunk_size+err_size), text);
                FILE *f = fopen(text, "wb");
                if (f) fwrite(inbuf+cmn*chunk_size, 1, err_size, f), fclose(f);
                exit(1);
            }
        }

        memset(decomp, 0, insize); // clear output buffer

        if (decomp_error) break;

        total_nanosec = GetDiffTime(rate, timer_ticks, end_ticks);
        total_d_iters += i;
        if ((total_d_iters >= params->d_iters) && (total_nanosec > ((uint64_t)params->dmintime*1000000))) break;
        LZBENCH_PRINT(2, "%s decompr iter=%d time=%.2fs speed=%.2f MB/s     \r", desc->name, total_d_iters, total_nanosec/1000000000.0, (float)insize*i*1000/nanosec);
    }
    while (true);

 //   printf("total_c_iters=%d total_d_iters=%d            \n", total_c_iters, total_d_iters);
    print_stats(params, desc, level, ctime, dtime, insize, complen, decomp_error);

done:
    if (desc->deinit) desc->deinit(workmem);
}


void lzbench_test_with_params(lzbench_params_t *params,
    std::vector<size_t> &file_sizes, const char *namesWithParams,
    uint8_t *inbuf, size_t insize, uint8_t *compbuf, size_t comprsize,
    uint8_t *decomp, bench_rate_t rate)
{
    std::vector<std::string> cnames, cparams;

	if (!namesWithParams) return;

    cnames = split(namesWithParams, '/');

    for (int k=0; k<cnames.size(); k++)
        LZBENCH_PRINT(5, "cnames[%d] = %s\n", k, cnames[k].c_str());

    for (int k=0; k<cnames.size(); k++)
    {
        for (int i=0; i<LZBENCH_ALIASES_COUNT; i++)
        {
            if (istrcmp(cnames[k].c_str(), alias_desc[i].name)==0)
            {
                lzbench_test_with_params(params, file_sizes, alias_desc[i].params, inbuf, insize, compbuf, comprsize, decomp, rate);
                goto next_k;
            }
        }

        LZBENCH_PRINT(5, "params = %s\n", cnames[k].c_str());
        cparams = split(cnames[k].c_str(), ',');
        if (cparams.size() >= 1)
        {
            int j=1;
            do {
                bool found = false;
                for (int i=1; i<LZBENCH_COMPRESSOR_COUNT; i++)
                {
                    if (istrcmp(comp_desc[i].name, cparams[0].c_str()) == 0)
                    {
                        found = true;
                       // printf("%s %s %s\n", cparams[0].c_str(), comp_desc[i].version, cparams[j].c_str());
                        if (j >= cparams.size())
                        {
                            for (int level=comp_desc[i].first_level; level<=comp_desc[i].last_level; level++)
                                lzbench_test(params, file_sizes, &comp_desc[i], level, inbuf, insize, compbuf, comprsize, decomp, rate, level);
                        }
                        else
                            lzbench_test(params, file_sizes, &comp_desc[i], atoi(cparams[j].c_str()), inbuf, insize, compbuf, comprsize, decomp, rate, atoi(cparams[j].c_str()));
                        break;
                    }
                }
                if (!found) printf("NOT FOUND: %s %s\n", cparams[0].c_str(), (j<cparams.size()) ? cparams[j].c_str() : NULL);
                j++;
            }
            while (j < cparams.size());
        }
next_k:
        continue;
    }
}


int lzbench_join(lzbench_params_t* params, const char** inFileNames, unsigned ifnIdx, char* encoder_list)
{
    bench_rate_t rate;
    size_t comprsize, insize, inpos, totalsize;//, aligned_totalsize;
    uint8_t *inbuf, *compbuf, *decomp;
    std::vector<size_t> file_sizes;
    std::string text;
    FILE* in;
    const char* pch;

    totalsize = UTIL_getTotalFileSize(inFileNames, ifnIdx);
    if (totalsize == 0) {
        printf("Could not find input files\n");
        return 1;
    }

    size_t data_buf_size = totalsize + PAD_SIZE + (ALIGN_BYTES * ifnIdx);
    comprsize = GET_COMPRESS_BOUND(totalsize) + (ALIGN_BYTES * ifnIdx);
    inbuf = alloc_data_buffer(data_buf_size);
    compbuf = alloc_data_buffer(data_buf_size);
    // tmpbuf = alloc_data_buffer(data_buf_size);  // for preprocessing
    decomp = alloc_data_buffer(data_buf_size);

    if (!inbuf || !compbuf || !decomp)
    {
        printf("Not enough memory, please use -m option!\n");
        return 1;
    }

    InitTimer(rate);
    inpos = 0;

    for (int i=0; i<ifnIdx; i++)
    {
        if (UTIL_isDirectory(inFileNames[i])) {
            fprintf(stderr, "warning: use -r to process directories (%s)\n", inFileNames[i]);
            continue;
        }

        if (!(in=fopen(inFileNames[i], "rb"))) {
            perror(inFileNames[i]);
            continue;
        }

        fseeko(in, 0L, SEEK_END);
        insize = ftello(in);
        rewind(in);
        insize = fread(inbuf+inpos, 1, insize, in);

        // force even multiple of ALIGN_BYTES so start of next file
        // will be aligned properly
        size_t remainder = insize % ALIGN_BYTES;
        insize += remainder ? ALIGN_BYTES - remainder : 0;

        file_sizes.push_back(insize);
        inpos += insize;
        fclose(in);
    }

    if (file_sizes.size() == 0)
        goto _clean;

    format(text, "%d files", file_sizes.size());
    params->in_filename = text.c_str();

    LZBENCH_PRINT(5, "totalsize=%d inpos=%d\n", (int)totalsize, (int)inpos);
    totalsize = inpos;


    {
        std::vector<size_t> single_file;
        // lzbench_params_t params_memcpy;
        lzbench_params_t params_memcpy(*params);

        print_header(params);
        // memcpy(&params_memcpy, params, sizeof(lzbench_params_t));
        params_memcpy.cmintime = params_memcpy.dmintime = 0;
        params_memcpy.c_iters = params_memcpy.d_iters = 0;
        params_memcpy.cloop_time = params_memcpy.dloop_time = DEFAULT_LOOP_TIME;
        single_file.push_back(totalsize);
        lzbench_test(&params_memcpy, file_sizes, &comp_desc[0], 0, inbuf, totalsize, compbuf, totalsize, decomp, rate, 0);
    }

    lzbench_test_with_params(params, file_sizes, encoder_list?encoder_list:alias_desc[0].params, inbuf, totalsize, compbuf, comprsize, decomp, rate);

_clean:
    free_data_buffer(inbuf);
    free_data_buffer(compbuf);
    free_data_buffer(decomp);

    return 0;
}


int lzbench_main(lzbench_params_t* params, const char** inFileNames, unsigned ifnIdx, char* encoder_list)
{
    bench_rate_t rate;
    size_t comprsize, insize, real_insize;
    uint8_t *inbuf, *compbuf, *decomp;
    std::vector<size_t> file_sizes;
    FILE* in;
    const char* pch;

    for (int i=0; i<ifnIdx; i++)
    {
        if (UTIL_isDirectory(inFileNames[i])) {
            fprintf(stderr, "warning: use -r to process directories (%s)\n", inFileNames[i]);
            continue;
        }

        if (!(in=fopen(inFileNames[i], "rb"))) {
            perror(inFileNames[i]);
            continue;
        }

        pch = strrchr(inFileNames[i], '\\');
        params->in_filename = pch ? pch+1 : inFileNames[i];

        InitTimer(rate);

        fseeko(in, 0L, SEEK_END);
        real_insize = ftello(in);
        rewind(in);

        if (params->mem_limit && real_insize > params->mem_limit)
            insize = params->mem_limit;
        else
            insize = real_insize;

        comprsize = GET_COMPRESS_BOUND(insize) + ALIGN_BYTES;
        size_t data_buf_size = insize + PAD_SIZE + ALIGN_BYTES;
        inbuf = alloc_data_buffer(data_buf_size);
        compbuf = alloc_data_buffer(data_buf_size);
        // tmpbuf = alloc_data_buffer(data_buf_size);  // for preprocessing
        decomp = alloc_data_buffer(data_buf_size);

        if (!inbuf || !compbuf || !decomp)
        {
            printf("Not enough memory, please use -m option!");
            return 1;
        }

        if(params->random_read){
          long long unsigned pos = 0;
          if (params->chunk_size < real_insize){
            pos = (rand() % (real_insize / params->chunk_size)) * params->chunk_size;
            insize = params->chunk_size;
            fseeko(in, pos, SEEK_SET);
          }else{
            insize = real_insize;
          }
          printf("Seeking to: %llu %ld %ld\n", pos, (long)params->chunk_size, (long)insize);
        }

        insize = fread(inbuf, 1, insize, in);

        if (i == 0)
        {
            print_header(params);

            // lzbench_params_t params_memcpy;
            // memcpy(&params_memcpy, params, sizeof(lzbench_params_t));
            lzbench_params_t params_memcpy(*params);
            params_memcpy.cmintime = params_memcpy.dmintime = 0;
            params_memcpy.c_iters = params_memcpy.d_iters = 0;
            params_memcpy.cloop_time = params_memcpy.dloop_time = DEFAULT_LOOP_TIME;
            file_sizes.push_back(insize);
            lzbench_test(&params_memcpy, file_sizes, &comp_desc[0], 0, inbuf, insize, compbuf, insize, decomp, rate, 0);
            file_sizes.clear();
        }

        if (params->mem_limit && real_insize > params->mem_limit)
        {
            int i;
            std::string partname;
            const char* filename = params->in_filename;
            for (i=1; insize > 0; i++)
            {
                format(partname, "%s part %d", filename, i);
                params->in_filename = partname.c_str();
                file_sizes.push_back(insize);
                lzbench_test_with_params(params, file_sizes, encoder_list?encoder_list:alias_desc[0].params, inbuf, insize, compbuf, comprsize, decomp, rate);
                file_sizes.clear();
                insize = fread(inbuf, 1, insize, in);
            }
        }
        else
        {
            file_sizes.push_back(insize);
            lzbench_test_with_params(params, file_sizes, encoder_list?encoder_list:alias_desc[0].params, inbuf, insize, compbuf, comprsize, decomp, rate);
            file_sizes.clear();
        }

        fclose(in);
        free_data_buffer(inbuf);
        free_data_buffer(compbuf);
        free_data_buffer(decomp);
    }

    return 0;
}


void usage(lzbench_params_t* params)
{
    fprintf(stderr, "usage: " PROGNAME " [options] input [input2] [input3]\n\nwhere [input] is a file or a directory and [options] are:\n");
    fprintf(stderr, " -b#   set block/chunk size to # KB (default = MIN(filesize,%d KB))\n", (int)(params->chunk_size>>10));
    fprintf(stderr, " -c#   sort results by column # (1=algname, 2=ctime, 3=dtime, 4=comprsize)\n");
    fprintf(stderr, " -e#   #=compressors separated by '/' with parameters specified after ',' (deflt=fast)\n");
    fprintf(stderr, " -iX,Y set min. number of compression and decompression iterations (default = %d, %d)\n", params->c_iters, params->d_iters);
    fprintf(stderr, " -j    join files in memory but compress them independently (for many small files)\n");
    fprintf(stderr, " -l    list of available compressors and aliases\n");
    fprintf(stderr, " -R    read block/chunk size from random blocks (to estimate for large files)\n");
    fprintf(stderr, " -m#   set memory limit to # MB (default = no limit)\n");
    fprintf(stderr, " -o#   output text format 1=Markdown, 2=text, 3=text+origSize, 4=CSV (default = %d)\n", params->textformat);
    fprintf(stderr, " -p#   print time for all iterations: 1=fastest 2=average 3=median (default = %d)\n", params->timetype);
#ifdef UTIL_HAS_CREATEFILELIST
    fprintf(stderr, " -r    operate recursively on directories\n");
#endif
    fprintf(stderr, " -s#   use only compressors with compression speed over # MB (default = %d MB)\n", params->cspeed);
    fprintf(stderr, " -tX,Y set min. time in seconds for compression and decompression (default = %.0f, %.0f)\n", params->cmintime/1000.0, params->dmintime/1000.0);
    fprintf(stderr, " -v    disable progress information\n");
    fprintf(stderr, " -x    disable real-time process priority\n");
    fprintf(stderr, " -z    show (de)compression times instead of speed\n");
    fprintf(stderr,"\nExample usage:\n");
    fprintf(stderr,"  " PROGNAME " -ezstd filename = selects all levels of zstd\n");
    fprintf(stderr,"  " PROGNAME " -ebrotli,2,5/zstd filename = selects levels 2 & 5 of brotli and zstd\n");
    fprintf(stderr,"  " PROGNAME " -t3 -u5 fname = 3 sec compression and 5 sec decompression loops\n");
    fprintf(stderr,"  " PROGNAME " -t0 -u0 -i3 -j5 -ezstd fname = 3 compression and 5 decompression iter.\n");
    fprintf(stderr,"  " PROGNAME " -t0u0i3j5 -ezstd fname = the same as above with aggregated parameters\n");
}


int main( int argc, char** argv)
{
    FILE *in;
    char* encoder_list = NULL;
    int result = 0, sort_col = 0, real_time = 1;
    lzbench_params_t lzparams;
    lzbench_params_t* params = &lzparams;
    const char** inFileNames = (const char**) calloc(argc, sizeof(char*));
    unsigned ifnIdx=0;
    bool join = false;
#ifdef UTIL_HAS_CREATEFILELIST
    const char** extendedFileList = NULL;
    char* fileNamesBuf = NULL;
    unsigned fileNamesNb, recursive=0;
#endif

    if (inFileNames==NULL) {
        LZBENCH_PRINT(2, "Allocation error : not enough memory%c\n", ' ');
        return 1;
    }

    memset(params, 0, sizeof(lzbench_params_t));
    params->timetype = FASTEST;
    params->textformat = TEXT;
    params->show_speed = 1;
    params->verbose = 2;
    params->chunk_size = (1ULL << 31) - (1ULL << 31)/6;
    params->cspeed = 0;
    params->c_iters = params->d_iters = 1;
    params->cmintime = 10*DEFAULT_LOOP_TIME/1000000; // 1 sec
    params->dmintime = 20*DEFAULT_LOOP_TIME/1000000; // 2 sec
    params->cloop_time = params->dloop_time = DEFAULT_LOOP_TIME;


    while ((argc>1) && (argv[1][0]=='-')) {
    char* argument = argv[1]+1;
    if (!strcmp(argument, "-compress-only")) params->compress_only = 1;
    else while (argument[0] != 0) {
        char* numPtr = argument + 1;
        unsigned number = 0;
        while ((*numPtr >='0') && (*numPtr <='9')) { number *= 10;  number += *numPtr - '0'; numPtr++; }

        // // parse number passed
        // int decimal_pos = -1;
        // int length = 0;
        // while (((*numPtr >='0') && (*numPtr <='9')) || *numPtr == '.') {
        //     if (*numPtr == '.') {
        //         decimal = true;
        //         decimal_pos = length;
        //         continue;
        //     }
        //     number *= 10;
        //     number += *numPtr - '0';
        //     numPtr++;
        //     length++;
        // }
        // if (length < 1) {
        //     LZBENCH_PRINT(1, 'Received malformed numeric argument: %s\n', argument);
        //     return 1;
        // }
        // bool decimal = decimal_pos >= 0;
        // bool ignored_decimal = decimal;
        // int64_t divide_decimal_by = std::pow(10, length - decimal_pos);

        switch (argument[0])
        {
        case 'a':
            encoder_list = strdup(argument + 1);
            numPtr += strlen(numPtr);
            break;
        case 'b':
            params->chunk_size = number << 10;
            break;
        case 'c':
            sort_col = number;
            break;
        case 'd':
            // break; // TODO rm
            params->preprocessors.push_back(number);
            // printf("params preprocessors current size: %lu\n", params->preprocessors.size());
            // params->preprocessors.push_back(1);
            // printf("params preprocessors new size: %lu\n", params->preprocessors.size());
            // params->preprocessors.clear();
            // printf("params preprocessors cleared size: %lu\n", params->preprocessors.size());
            break;
        case 'e':
            params->element_sz = number;
            break;
        // case 'g':
        //     params->time_preproc = true;
        //     break;
        // case 'e':
        //     encoder_list = strdup(argument + 1);
        //     numPtr += strlen(numPtr);
        //     break;
        case 'i':
            params->c_iters = number;
            if (*numPtr == ',')
            {
                numPtr++;
                number = 0;
                while ((*numPtr >='0') && (*numPtr <='9')) { number *= 10;  number += *numPtr - '0'; numPtr++; }
                params->d_iters = number;
            }
            break;
        case 'j':
            join = true;
            break;
        case 'm':
            params->mem_limit = number << 18; /*  total memory usage = mem_limit * 4  */
            if (params->textformat == TEXT) params->textformat = TEXT_FULL;
            break;
        case 'o':
            params->textformat = (textformat_e)number;
            if (params->textformat == CSV) params->verbose = 0;
            break;
        case 'p':
            params->timetype = (timetype_e)number;
            break;
        case 'r':
            recursive = 1;
            break;
        case 'R':
            params->random_read = 1;
            srand(time(NULL));
            break;
        case 's':
            params->cspeed = number;
            break;
        case 't':
            params->cmintime = 1000*number;
            params->cloop_time = (params->cmintime)?DEFAULT_LOOP_TIME:0;
            // if (decimal) {
            //     params->cmintime /= divide_decimal_by;
            //     params->cloop_time /= divide_decimal_by;
            //     ignored_decimal = false;
            // }
            if (*numPtr == ',')
            {
                numPtr++;
                number = 0;
                while ((*numPtr >='0') && (*numPtr <='9')) { number *= 10;  number += *numPtr - '0'; numPtr++; }
                params->dmintime = 1000*number;
                params->dloop_time = (params->dmintime)?DEFAULT_LOOP_TIME:0;
            }
            break;
        case 'u':
            params->dmintime = 1000*number;
            params->dloop_time = (params->dmintime)?DEFAULT_LOOP_TIME:0;
            break;
        case 'v':
            params->verbose = number;
            break;
        case 'x':
            real_time = 0;
            break;
        case 'z':
            params->show_speed = 0;
            break;
        case '-': // --help
        case 'h':
            usage(params);
            goto _clean;
        case 'l':
            printf("\nAvailable compressors for -e option:\n");
            printf("all - alias for all available compressors\n");
            printf("fast - alias for compressors with compression speed over 100 MB/s (default)\n");
            printf("opt - compressors with optimal parsing (slow compression, fast decompression)\n");
            printf("lzo / ucl - aliases for all levels of given compressors\n");
            for (int i=1; i<LZBENCH_COMPRESSOR_COUNT; i++)
            {
                if (comp_desc[i].compress)
                {
                    if (comp_desc[i].first_level < comp_desc[i].last_level)
                        printf("%s %s [%d-%d]\n", comp_desc[i].name, comp_desc[i].version, comp_desc[i].first_level, comp_desc[i].last_level);
                    else
                        printf("%s %s\n", comp_desc[i].name, comp_desc[i].version);
                }
            }
            return 0;
        default:
            fprintf(stderr, "unknown option: %s\n", argv[1]);
            result = 1; goto _clean;
        }

        // if (decimal and ignored_decimal) {
        //     LZBENCH_PRINT(2, "Argument does not except decimal input!");
        //     return 1;
        // }

        argument = numPtr;
    }
    argv++;
    argc--;
    }

    while (argc > 1) {
        inFileNames[ifnIdx++] = argv[1];
        argv++;
        argc--;
    }

    LZBENCH_PRINT(2, PROGNAME " " PROGVERSION " (%d-bit " PROGOS ")   Assembled by P.Skibinski\n", (uint32_t)(8 * sizeof(uint8_t*)));
    LZBENCH_PRINT(5, "params: chunk_size=%d c_iters=%d d_iters=%d cspeed=%d cmintime=%d dmintime=%d encoder_list=%s\n", (int)params->chunk_size, params->c_iters, params->d_iters, params->cspeed, params->cmintime, params->dmintime, encoder_list);

    if (ifnIdx < 1)  { usage(params); goto _clean; }

    if (real_time)
    {
        SET_HIGH_PRIORITY;
    } else {
        LZBENCH_PRINT(2, "The real-time process priority disabled%c\n", ' ');
    }


#ifdef UTIL_HAS_CREATEFILELIST
    if (recursive) {  /* at this stage, filenameTable is a list of paths, which can contain both files and directories */
        extendedFileList = UTIL_createFileList(inFileNames, ifnIdx, &fileNamesBuf, &fileNamesNb);
        if (extendedFileList) {
            unsigned u;
            for (u=0; u<fileNamesNb; u++) LZBENCH_PRINT(4, "%u %s\n", u, extendedFileList[u]);
            free((void*)inFileNames);
            inFileNames = extendedFileList;
            ifnIdx = fileNamesNb;
        }
    }
#endif

    /* Main function */
    if (join)
        result = lzbench_join(params, inFileNames, ifnIdx, encoder_list);
    else
        result = lzbench_main(params, inFileNames, ifnIdx, encoder_list);

    if (params->chunk_size > 10 * (1<<20)) {
        LZBENCH_PRINT(2, "done... (cIters=%d dIters=%d cTime=%.1f dTime=%.1f chunkSize=%dMB cSpeed=%dMB)\n", params->c_iters, params->d_iters, params->cmintime/1000.0, params->dmintime/1000.0, (int)(params->chunk_size >> 20), params->cspeed);
    } else {
        LZBENCH_PRINT(2, "done... (cIters=%d dIters=%d cTime=%.1f dTime=%.1f chunkSize=%dKB cSpeed=%dMB)\n", params->c_iters, params->d_iters, params->cmintime/1000.0, params->dmintime/1000.0, (int)(params->chunk_size >> 10), params->cspeed);
    }

    if (sort_col <= 0) goto _clean;

    printf("\nThe results sorted by column number %d:\n", sort_col);
    print_header(params);

    switch (sort_col)
    {
        default:
        case 1: std::sort(params->results.begin(), params->results.end(), less_using_1st_column()); break;
        case 2: std::sort(params->results.begin(), params->results.end(), less_using_2nd_column()); break;
        case 3: std::sort(params->results.begin(), params->results.end(), less_using_3rd_column()); break;
        case 4: std::sort(params->results.begin(), params->results.end(), less_using_4th_column()); break;
        case 5: std::sort(params->results.begin(), params->results.end(), less_using_5th_column()); break;
    }

    for (std::vector<string_table_t>::iterator it = params->results.begin(); it!=params->results.end(); it++)
    {
        if (params->show_speed)
            print_speed(params, *it);
        else
            print_time(params, *it);
    }

_clean:
    if (encoder_list) free(encoder_list);
#ifdef UTIL_HAS_CREATEFILELIST
    if (extendedFileList)
        UTIL_freeFileList(extendedFileList, fileNamesBuf);
    else
#endif
        free((void*)inFileNames);
    return result;
}
