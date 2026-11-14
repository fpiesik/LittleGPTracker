
#include "WavFile.h"
#include "Application/Model/Config.h"
#include "Foundation/Types/Types.h"
#include "Services/Time/TimeService.h"
#include "System/Console/Trace.h"
#include <limits.h>
#include <stdlib.h>

int WavFile::bufferChunkSize_ = -1;
bool WavFile::initChunkSize_ = true;

short Swap16(short from) {
#ifdef __ppc__
    short result;
    ((char *)&result)[0] = ((char *)&from)[1];
    ((char *)&result)[1] = ((char *)&from)[0];
    return result;
#else
    return from;
#endif
}

int Swap32(int from) {
#ifdef __ppc__
    int result;
    ((char *)&result)[0] = ((char *)&from)[3];
    ((char *)&result)[1] = ((char *)&from)[2];
    ((char *)&result)[2] = ((char *)&from)[1];
    ((char *)&result)[3] = ((char *)&from)[0];
    return result;
#else
    return from;
#endif
}

WavFile::WavFile(I_File *file) {
    if (initChunkSize_) {
        const char *size =
            Config::GetInstance()->GetValue("SAMPLELOADCHUNKSIZE");
        if (size) {
            bufferChunkSize_ = atoi(size);
        }
        initChunkSize_ = false;
    }
    samples_ = 0;
    size_ = 0;
    readBuffer_ = 0;
    readBufferSize_ = 0;
    sampleBufferSize_ = 0;
    file_ = file;
};

WavFile::~WavFile() {
    if (file_) {
        file_->Close();
        delete file_;
    }
    SAFE_FREE(samples_);
    SAFE_FREE(readBuffer_);
};

// Read integers explicitly: RIFF is little endian on every target.
static unsigned int ReadLE(const unsigned char *p, int bytes) {
    unsigned int value = 0;
    for (int i = 0; i < bytes; ++i)
        value |= static_cast<unsigned int>(p[i]) << (8 * i);
    return value;
}

WavFile *WavFile::Open(const char *path) {
    I_File *file = FileSystem::GetInstance()->Open(path, "r");
    if (!file)
        return 0;
    WavFile *wav = new WavFile(file);
    file->Seek(0, SEEK_END);
    long length = file->Tell();
    bool format = false, data = false;
    unsigned int dataSize = 0;
    unsigned char header[16];
    file->Seek(0, SEEK_SET);
    if (length < 12 || file->Read(header, 1, 12) != 12 ||
        memcmp(header, "RIFF", 4) || memcmp(header + 8, "WAVE", 4))
        goto invalid;
    {
        unsigned int riffSize = ReadLE(header + 4, 4);
        if (riffSize < 4 || riffSize > static_cast<unsigned long>(length - 8))
            goto invalid;
        long end = static_cast<long>(riffSize) + 8;
        long position = 12;
        while (position < end) {
            if (end - position < 8)
                goto invalid;
            file->Seek(position, SEEK_SET);
            if (file->Read(header, 1, 8) != 8)
                goto invalid;
            unsigned int size = ReadLE(header + 4, 4);
            position += 8;
            if (size > static_cast<unsigned long>(end - position))
                goto invalid;
            if (!memcmp(header, "fmt ", 4)) {
                if (format || size < 16 || file->Read(header, 1, 16) != 16)
                    goto invalid;
                unsigned int channels = ReadLE(header + 2, 2);
                unsigned int rate = ReadLE(header + 4, 4);
                unsigned int bits = ReadLE(header + 14, 2);
                if (ReadLE(header, 2) != 1 ||
                    (channels != 1 && channels != 2) || !rate ||
                    rate > INT_MAX || (bits != 8 && bits != 16) ||
                    ReadLE(header + 12, 2) != channels * (bits / 8) ||
                    static_cast<unsigned long long>(rate) * channels *
                            (bits / 8) !=
                        ReadLE(header + 8, 4))
                    goto invalid;
                wav->channelCount_ = channels;
                wav->sampleRate_ = rate;
                wav->bytePerSample_ = bits / 8;
                format = true;
            } else if (!memcmp(header, "data", 4)) {
                if (data || position > INT_MAX)
                    goto invalid;
                wav->dataPosition_ = position;
                dataSize = size;
                data = true;
            }
            position += size;
            if (size & 1) {
                if (position == end)
                    goto invalid;
                ++position;
            }
        }
    }
    if (!format || !data || !dataSize ||
        dataSize % (wav->channelCount_ * wav->bytePerSample_))
        goto invalid;
    // Both input offsets and the expanded PCM buffer must fit our int API.
    if (dataSize > static_cast<unsigned int>(INT_MAX - wav->dataPosition_) ||
        dataSize / wav->bytePerSample_ > INT_MAX / 2)
        goto invalid;
    wav->size_ = dataSize / wav->channelCount_ / wav->bytePerSample_;
    return wav;
invalid:
    Trace::Error("Invalid or unsupported PCM WAV: %s", path);
    delete wav;
    return 0;
}

void *WavFile::GetSampleBuffer(int note) { return samples_; };

int WavFile::GetSize(int note) { return size_; };

int WavFile::GetChannelCount(int note) { return channelCount_; };

int WavFile::GetSampleRate(int note) { return sampleRate_; };

long WavFile::readBlock(long start, long size) {
    if (size > readBufferSize_) {
        SAFE_FREE(readBuffer_);
        readBuffer_ = SYS_MALLOC(size);
        readBufferSize_ = size;
    }
    if (!readBuffer_) {
        Trace::Error("Failed to allocate read buffer of size %ld", size);
        return 0;
    } else {
        file_->Seek(start, SEEK_SET);
        if (file_->Read(readBuffer_, 1, size) != size)
            return 0;
    }
    return size;
};

bool WavFile::GetBuffer(long start, long size) {

    if (!file_ || start < 0 || size <= 0 || start > size_ ||
        size > size_ - start)
        return false;

    // compute the sample buffer size we need,
    // allocate if needed

    int sampleBufferSize = 2 * channelCount_ * size;
    if (sampleBufferSize > sampleBufferSize_) {
        SAFE_FREE(samples_);
        samples_ = (short *)SYS_MALLOC(sampleBufferSize);
        sampleBufferSize_ = sampleBufferSize;
    }

    if (!samples_) {
        Trace::Error("Failed to allocate %d samples", sampleBufferSize);
        return false;
    }

    // compute the file buffer size we need to read

    int bufferSize = size * channelCount_ * bytePerSample_;
    int bufferStart = dataPosition_ + start * channelCount_ * bytePerSample_;

    // Read the buffer but in small chunk to let the system breathe
    // if the files are big

    int count = bufferSize;
    int offset = 0;
    char *ptr = (char *)samples_;
    int readSize = (bufferChunkSize_ > 0) ? bufferChunkSize_
                   : count > 4096         ? 4096
                                          : count;

    while (count > 0) {
        readSize = (count > readSize) ? readSize : count;
        if (readBlock(bufferStart, readSize) != readSize)
            return false;
        memcpy(ptr + offset, readBuffer_, readSize);
        bufferStart += readSize;
        count -= readSize;
        offset += readSize;
        if (bufferChunkSize_ > 0)
            TimeService::GetInstance()->Sleep(1);
    }

    // expand 8 bit data if needed

    unsigned char *src = (unsigned char *)samples_;
    short *dst = samples_;
    for (int i = size * channelCount_ - 1; i >= 0; --i) {
        if (bytePerSample_ == 1)
            dst[i] = (src[i] - 128) * 256;
        else
            dst[i] = Swap16(dst[i]);
    }

    return true;
};

void WavFile::Close() {
    file_->Close();
    SAFE_DELETE(file_);
    SAFE_FREE(readBuffer_);
    readBufferSize_ = 0;
};

int WavFile::GetRootNote(int note) { return 60; }
