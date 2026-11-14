#include "Application/Instruments/SampleEditBuffer.h"
#include <cassert>
#include <cstdio>

static SampleEditBuffer stereo() {
    SampleEditBuffer b;
    b.channels = 2;
    const short values[] = {100, -200, 1000, -2000, 3000, -4000, 5000, -6000};
    b.pcm.assign(values, values + 8);
    return b;
}
int main() {
    SampleEditBuffer b = stereo();
    assert(b.Process(SampleEditBuffer::CROP, 1, 3));
    assert(b.GetSize(-1) == 2 && b.pcm[0] == 1000 && b.pcm[3] == -4000);
    b = stereo();
    assert(b.Process(SampleEditBuffer::DELETE, 1, 3));
    assert(b.GetSize(-1) == 2 && b.pcm[0] == 100 && b.pcm[2] == 5000);
    assert(!b.Process(SampleEditBuffer::DELETE, 0, 2));
    assert(!b.Process(SampleEditBuffer::CROP, 1, 1));
    assert(!b.Process(SampleEditBuffer::SILENCE, -1, 1));
    assert(!b.Process(SampleEditBuffer::SILENCE, 0, 3));
    b = stereo();
    assert(b.Process(SampleEditBuffer::SILENCE, 1, 3));
    assert(b.pcm[0] == 100 && b.pcm[2] == 0 && b.pcm[5] == 0 &&
           b.pcm[7] == -6000);
    b = stereo();
    assert(b.Process(SampleEditBuffer::NORMALIZE, 1, 3));
    assert(b.pcm[2] == 8191 && b.pcm[5] == -32767 && b.pcm[7] == -6000);
    b.pcm.assign(4, -32768);
    assert(b.Process(SampleEditBuffer::NORMALIZE, 0, 2));
    assert(b.pcm[0] == -32767);
    b.pcm.assign(4, 0);
    assert(b.Process(SampleEditBuffer::NORMALIZE, 0, 2));
    assert(b.pcm[0] == 0);
    b = stereo();
    assert(b.Process(SampleEditBuffer::FADE_IN, 1, 4));
    assert(b.pcm[0] == 100 && b.pcm[2] == 0 && b.pcm[4] == 1500 &&
           b.pcm[7] == -6000);
    b = stereo();
    assert(b.Process(SampleEditBuffer::FADE_OUT, 1, 4));
    assert(b.pcm[2] == 1000 && b.pcm[4] == 1500 && b.pcm[7] == 0);
    assert(b.Process(SampleEditBuffer::FADE_IN, 0, 1));
    assert(b.pcm[0] == 0 && b.pcm[1] == 0);
    b = stereo();
    FILE *f = tmpfile();
    assert(f && b.Write(f, 1, 3));
    assert(ftell(f) == 44 + 8);
    rewind(f);
    unsigned char data[52];
    assert(fread(data, 1, sizeof(data), f) == sizeof(data));
    assert(data[22] == 2 && data[24] == 0x44 && data[25] == 0xac);
    assert(data[40] == 8 && data[44] == 0xe8 && data[45] == 3);
    assert(data[50] == 0x60 && data[51] == 0xf0);
    fclose(f);
    b.channels = 1;
    b.rate = 22050;
    f = tmpfile();
    assert(f && b.Write(f, 0, 2, 44100));
    assert(ftell(f) == 44 + 8);
    fclose(f);
    puts("Sample editor PCM and WAV tests passed");
}
