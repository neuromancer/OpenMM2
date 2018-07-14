#include "stdafx.h"
#include "Stream.h"

defnvar(0x6A3D68, Stream::sm_Streams);
defnvar(0x6A3EB8, Stream::sm_Buffers);

instvar(0x5CED80, int, MaxFilesOpenAtOnce);

int Stream::Read(void* dstBuf, int size)
{
    return stub<thiscall_t<int, Stream, void*, int>>(0x4C9AA0, this, dstBuf, size);
};

int Stream::Write(const void* srcBuf, int size)
{
    return stub<thiscall_t<int, Stream, const void*, int>>(0x4C9BF0, this, srcBuf, size);
};

int Stream::GetCh(void)
{
    uint8_t result;

    if (Read(&result, 1) == 1)
    {
        return result;
    }

    return -1;
};

int Stream::PutCh(unsigned char ch)
{
    if (Write(&ch, 1) == 1)
    {
        return ch;
    }

    return -1;
};

int Stream::Seek(int offset)
{
    if (Methods->Seek)
    {
        if (Flush() >= 0)
        {
            CurrentFileOffset = offset;
            return Methods->Seek(Handle, offset, 0);
        }
        else
        {
            return -1;
        }
    }

    CurrentBufferOffset = offset;

    return offset;
};

int Stream::Tell(void)
{
    return CurrentFileOffset + CurrentBufferOffset;
};

int Stream::Close(void)
{
    if (!CurrentBufferSize)
    {
        if (this->CurrentBufferOffset)
        {
            Flush();
        }
    }

    Methods->Close(Handle);

    Handle = -1;
    Methods = 0;

    return 0;
};

int Stream::Size(void)
{
    if (Methods->Size)
    {
        return Methods->Size(this->Handle);
    }

    if (Flush() >= 0)
    {
        int current = Methods->Seek(Handle, 0, SeekCurrent);
        int size = Methods->Seek(Handle, 0, SeekEnd);

        Methods->Seek(Handle, current, SeekBegin);

        return size;
    }

    return -1;
};

int Stream::Flush(void)
{
    int result = 0;

    if (CurrentBufferSize)
    {
        if (CurrentBufferSize == CurrentBufferOffset)
        {
            result = 0;
        }
        else
        {
            result = Methods->Seek(Handle, CurrentBufferOffset + CurrentFileOffset, 0);
        }
    }
    else if (CurrentBufferOffset)
    {
        result = Methods->Write(Handle, Buffer, CurrentBufferOffset);
    }

    int v5 = CurrentBufferOffset + CurrentFileOffset;

    CurrentBufferSize = 0;
    CurrentFileOffset = v5;
    CurrentBufferOffset = 0;

    if (Methods->Flush)
    {
        result = Methods->Flush(Handle);
    }

    return result;
}

Stream * Stream::Open(char const * fileName, coreFileMethods const * methods, bool readOnly)
{
    int handle = methods->Open(fileName, readOnly);

    if (handle != -1)
    {
        return AllocStream(fileName, handle, methods);
    }

    return nullptr;
}

Stream * Stream::AllocStream(char const * fileName, int handle, coreFileMethods const * methods)
{
    (void)fileName;

    for (int i = 0; i < MAX_STREAMS; ++i)
    {
        Stream* stream = &sm_Streams[i];

        if (stream->Methods == nullptr)
        {
            stream->Handle = handle;
            stream->Methods = methods;

            stream->CurrentFileOffset = 0;
            stream->CurrentBufferOffset = 0;
            stream->CurrentBufferSize = 0;

            stream->Buffer = sm_Buffers[i];
            stream->BufferSize = STREAM_BUFFER_SIZE;

            if (i > MaxFilesOpenAtOnce)
            {
                MaxFilesOpenAtOnce = i;
            }

            return stream;
        }
    }

    DumpOpenFiles();

    Errorf("Out of file handles.");

    return nullptr;
}

Stream * Stream::Open(char const * fileName, bool readOnly)
{
    coreFileMethods* fileMethods = readOnly ? ReadOnlyFileMethos : ReadWriteFileMethods;

    int fileHandle = fileMethods->Open(fileName, readOnly);

    if (fileHandle == -1)
    {
        return nullptr;
    }

    return Stream::AllocStream(fileName, fileHandle, fileMethods);
}

Stream * Stream::Create(const char * fileName)
{
    int handle = ReadWriteFileMethods->Create(fileName);

    if (handle == -1)
    {
        return nullptr;
    }

    return Stream::AllocStream(fileName, handle, ReadWriteFileMethods);
}

void Stream::DumpOpenFiles(void)
{
    for (int i = 0; i < MAX_STREAMS; ++i)
    {
        Stream* stream = &sm_Streams[i];

        if (stream->Methods)
        {
            Displayf("%d. HANDLE=%x", i, stream->Handle);
        }
    }
}

int fseek(Stream * stream, int position, seekWhence whence)
{
    switch (whence)
    {
    case SeekBegin:
    {
        stream->Seek(position);
    } break;

    case SeekCurrent:
    {
        stream->Seek(position + stream->Tell());
    } break;

    case SeekEnd:
    {
        stream->Seek(position + stream->Size());
    } break;
    }

    return -1;
}

int fgets(char * buffer, int length, Stream * stream)
{
    int total = 0;

    for (int last = length - 1; total < last; ++total)
    {
        int currentChar = -1;

        if (stream->CurrentBufferOffset >= stream->CurrentBufferSize)
        {
            currentChar = stream->GetCh();
        }
        else
        {
            currentChar = stream->Buffer[stream->CurrentBufferOffset++];
        }

        if (currentChar == -1)
        {
            break;
        }

        buffer[total] = (char) currentChar;

        if (currentChar == '\n')
        {
            break;
        }
    }

    buffer[total] = '\0';

    return total;
}

void fprintf(Stream * stream, char const * format, ...)
{
    char buffer[512]; // [sp+4h] [bp-200h]@1
    va_list va; // [sp+214h] [bp+10h]@1

    va_start(va, format);
    vsprintf_s(buffer, format, va);
    va_end(va);

    stream->Write(buffer, strlen(buffer));
}

int fscanf(Stream * stream, char const * format, ...)
{
    int currentChar = -1; // eax@1

    do
    {
        do
        {
            currentChar = stream->GetCh();
        } while (currentChar == '\t');
    } while (currentChar == '\n' || currentChar == ' ' || currentChar == '\r');

    char buffer[256];
    buffer[0] = (char) currentChar;

    if (fgets(&buffer[1], 255, stream))
    {
        va_list va;
        va_start(va, format);

        int result = vsscanf_s(buffer, format, va);

        va_end(va);

        if (!result)
        {
            Errorf("scan of '%s' == '%s' failed", format, buffer);
        }

        return result;
    }

    return 0;
}

defnvar(0x5CED7C, ReadWriteFileMethods);
defnvar(0x5CED78, ReadOnlyFileMethos);
defnvar(0x5CEE28, rawFileMethods);