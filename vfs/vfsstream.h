#pragma once

#include <streambuf>
#include "vfs/eu07vfs.h"
#include "Globals.h"

class ivfsbuf : public std::streambuf {
    eu07vfs_file_ctx vfs_ctx;
    eu07vfs_file_handle handle;
    char *buffer;
    const size_t BUFSIZE = 1024 * 64;

    size_t virtual_pos;
    size_t vfs_pos;
    size_t buffer_pos;
    size_t buffer_size;

    size_t file_size;

    void update_virtualpos() {
        virtual_pos += this->gptr() - this->eback();
        this->setg(this->gptr(), this->gptr(), this->egptr());
    }

public:
    ivfsbuf(const std::string &filename, std::ios_base::openmode mode = std::ios_base::in) {
        virtual_pos = 0;
        vfs_pos = 0;
        buffer_pos = 0;
        buffer_size = 0;
        buffer = nullptr;
        vfs_ctx = nullptr;
        file_size = 0;

        handle = eu07vfs_lookup_file(Global.vfs, filename.data(), filename.size());

        if (handle == EU07VFS_NULL_HANDLE)
            return;

        vfs_ctx = eu07vfs_open_file(Global.vfs, handle);
        if (!vfs_ctx)
            return;

        file_size = eu07vfs_get_file_size(Global.vfs, vfs_ctx);
        buffer = new char[BUFSIZE];
        this->setg(nullptr, nullptr, nullptr);

        if (mode & std::ios_base::ate)
            seekoff(0, std::ios_base::end, std::ios_base::in);
    }

    ~ivfsbuf() {
        if (vfs_ctx)
            eu07vfs_close_file(Global.vfs, vfs_ctx);
        if (buffer)
            delete[] buffer;
    }

    int underflow() override {
        update_virtualpos();

        if (this->gptr() < this->egptr()) { // return if more data is already available
            return *this->gptr();
        }

        // no more data, but let's check within our buffer
        if (virtual_pos >= buffer_pos && virtual_pos < (buffer_pos + buffer_size)) {
            // we have data, return it
            size_t offset = virtual_pos - buffer_pos;
            this->setg(this->buffer + offset, this->buffer + offset, this->buffer + buffer_size);
            return *this->gptr();
        }

        if (virtual_pos >= file_size)
            return std::char_traits<char>::eof();

        // too bad, we don't have data in our buffer, we need to fill it somehow

        if (virtual_pos < vfs_pos) {
            // we need to seek backwards
            // we cannot do that, reopen file
            eu07vfs_close_file(Global.vfs, vfs_ctx);
            vfs_ctx = eu07vfs_open_file(Global.vfs, handle);
            vfs_pos = 0;
        }
        if (virtual_pos > vfs_pos) {
            // we need to seek forward
            size_t ignore_bytes = virtual_pos - vfs_pos;
            while (ignore_bytes) {
                size_t read = ignore_bytes;
                if (read > BUFSIZE)
                    read = BUFSIZE;

                read = eu07vfs_read_file(Global.vfs, vfs_ctx, buffer, read);
                buffer_pos = vfs_pos;
                buffer_size = read;
                vfs_pos += read;

                ignore_bytes -= read;
            }
        }

        // now at proper position, just read the data
        assert(vfs_pos == virtual_pos);

        size_t size = eu07vfs_read_file(Global.vfs, vfs_ctx, buffer, BUFSIZE);
        buffer_pos = vfs_pos;
        buffer_size = size;
        vfs_pos += size;

        // and set necessary pointers
        this->setg(this->buffer, this->buffer, this->buffer + buffer_size);
        return *this->gptr();
    }

    std::streamsize showmanyc() override {
        update_virtualpos();

        return file_size - virtual_pos;
    }

    std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way, std::ios_base::openmode which) override {
        update_virtualpos();

        size_t pos;

        if (way == std::ios_base::beg)
            pos = off;
        else if (way == std::ios_base::cur)
            pos = virtual_pos + off;
        else if (way == std::ios_base::end)
            pos = file_size + off;

        if (pos < 0)
            pos = 0;
        if (pos > file_size)
            pos = file_size;

        virtual_pos = pos;
        this->setg(nullptr, nullptr, nullptr);

        return pos;
    }

    std::streampos seekpos(std::streampos pos, std::ios_base::openmode which) override {
        return seekoff(pos, std::ios_base::beg, which);
    }

    int pbackfail(int c) override {
        seekoff(-1, std::ios_base::cur, std::ios_base::in);
        underflow();

        if (this->gptr() < this->egptr()) {
            if (c != std::char_traits<char>::eof())
                *this->gptr() = c;
            return c;
        }
        else
            return std::char_traits<char>::eof();
    }

    std::streamsize xsgetn(char *target, std::streamsize size) override {
        std::streamsize data_left = showmanyc();
        if (size > data_left)
            size = data_left;

        if (virtual_pos >= file_size || !size)
            return 0;

        char *ori_target = target;

        if (size < BUFSIZE / 4) // small read, let's fetch data to our buffer
            underflow();

        { // some data should be available in buffer, copy it
            size_t data_avail = this->egptr() - this->gptr();
            if (data_avail > size)
                data_avail = size;

            memcpy(target, this->gptr(), data_avail);
            size -= data_avail;
            target += data_avail;

            this->setg(this->eback(), this->gptr() + data_avail, this->egptr());
            update_virtualpos();
        }

        if (virtual_pos >= file_size || !size)
            return target - ori_target;

        // still something to read, do it directly
        size_t read = eu07vfs_read_file(Global.vfs, vfs_ctx, target, size);
        target += read;
        vfs_pos += read;
        virtual_pos += read;

        return target - ori_target;
    }
};

struct ivfsstream_base {
    ivfsbuf sbuf_;
    ivfsstream_base(const std::string &filename, std::ios_base::openmode mode = std::ios_base::in): sbuf_(filename, mode) {}
};

class ivfsstream : virtual ivfsstream_base, public std::istream {
public:
    ivfsstream(const std::string &filename, std::ios_base::openmode mode = std::ios_base::in) : ivfsstream_base(filename, mode), std::ios(&this->sbuf_), std::istream(&this->sbuf_) {}
};
