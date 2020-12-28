#define _FILE_OFFSET_BITS 64
#include <inttypes.h>
#include <stdio.h>
#include <assert.h>
#include "hashmap.h"
#include <string.h>
#include <lz4frame.h>
#include <stdlib.h>
#include <inttypes.h>
#include "eu07vfs.h"

#pragma pack(push, 1)

struct vfs_header
{
	uint64_t magic;
	uint32_t version;
	uint64_t data_size;
	uint64_t entry_list_size;
	uint64_t meta_list_size;
};

struct entry_header
{
	uint64_t entry_size;
	uint32_t id;
	uint32_t storetype;
	uint64_t original_size;
};

#pragma pack(pop)

struct _eu07vfs_instance
{
	FILE *storage;
	struct hashmap *hm;
};

struct _eu07vfs_file_ctx
{
	uint64_t pos;
	uint64_t left;
	int lz4_enabled;
	uint64_t original_size;

	LZ4F_dctx *lz4_ctx;
	LZ4F_decompressOptions_t lz4_opts;
	char *buffer;
	size_t leftover;
};

struct hash_entry {
	char *name;
	size_t namelen;
	uint64_t offset;
};

static uint64_t user_hash(const void *item, uint64_t seed0, uint64_t seed1) {
	const struct hash_entry *entry = item;
	return hashmap_sip(entry->name, entry->namelen, seed0, seed1);
}

static int user_compare(const void *a, const void *b, void *udata) {
	const struct hash_entry *ua = a;
	const struct hash_entry *ub = b;
	if (ua->namelen < ub->namelen)
		return 1;
	if (ua->namelen > ub->namelen)
		return -1;
	return memcmp(ua->name, ub->name, ua->namelen);
}

static bool user_iter(const void *item, void *udata) {
	const struct hash_entry *user = item;
	free(user->name);
	return true;
}

static inline void seek(FILE *file, uint64_t position)
{
#ifdef _WIN32
	_fseeki64(file, position, SEEK_SET);
#else
	fseeko(file, position, SEEK_SET);
#endif
}

eu07vfs_instance eu07vfs_init(const char *path)
{
	FILE *base = fopen(path, "rb");
	struct vfs_header header;
	fread(&header, sizeof(header), 1, base);
	if (header.magic != 0x5346565F37305545 || header.version != 1)
		return NULL;

	seek(base, (uint64_t)sizeof(header) + header.data_size);

	eu07vfs_instance instance = malloc(sizeof(struct _eu07vfs_instance));
	instance->hm = hashmap_new(sizeof(struct hash_entry), 0, 0, 0, user_hash, user_compare, NULL);
	instance->storage = base;

	size_t processed = 0;
	while (processed < header.entry_list_size) {
		uint32_t entry_h[2];

		fread(entry_h, 8, 1, base);
		processed += entry_h[0];

		uint32_t filenamelen = entry_h[1];
		char* filename = malloc(filenamelen + 1);
		filename[filenamelen] = 0;
		fread(filename, filenamelen, 1, base);

		fread(entry_h, 4, 1, base);
		if (entry_h[0] != 0x4C504552)
			return NULL;

		uint64_t offset;
		fread(&offset, 8, 1, base);

		hashmap_set(instance->hm, &(struct hash_entry){ .name = filename, .namelen = filenamelen, .offset = offset });

		//free(filename);
	}

	return instance;
}

eu07vfs_file_handle eu07vfs_lookup_file(eu07vfs_instance instance, const char *file, size_t filelen)
{
	struct hash_entry *entry = hashmap_get(instance->hm, &(struct hash_entry){ .name = (char*)file, .namelen = filelen });
	if (!entry)
        return -1;
	return entry->offset;
}

eu07vfs_file_ctx eu07vfs_open_file(eu07vfs_instance instance, eu07vfs_file_handle handle)
{
	seek(instance->storage, (uint64_t)sizeof(struct vfs_header) + handle);
	struct entry_header header;
	fread(&header, sizeof(header), 1, instance->storage);

	eu07vfs_file_ctx ctx = malloc(sizeof(struct _eu07vfs_file_ctx));

	ctx->pos = (uint64_t)sizeof(struct vfs_header) + handle + (uint64_t)sizeof(struct entry_header);
	ctx->left = header.entry_size - (uint64_t)sizeof(struct entry_header);
	ctx->buffer = NULL;
	ctx->original_size = header.original_size;

	if (header.storetype == 0x5F345A4C) { // LZ4
		ctx->lz4_enabled = 1;
		memset(&ctx->lz4_opts, 0, sizeof(ctx->lz4_opts));
		ctx->lz4_opts.stableDst = 0;
		ctx->leftover = 0;
		LZ4F_createDecompressionContext(&ctx->lz4_ctx, LZ4F_VERSION);
	}
	else if (header.storetype == 0x59504F43) { // COPY
		ctx->lz4_enabled = 0;
	}
	else {
		free(ctx);
		return NULL;
	}

	return ctx;
}

uint64_t eu07vfs_get_file_size(eu07vfs_instance instance, eu07vfs_file_ctx ctx)
{
	return ctx->original_size;
}

size_t eu07vfs_read_file(eu07vfs_instance instance, eu07vfs_file_ctx ctx, void *output, size_t size)
{
	if (!ctx->lz4_enabled) {
		if (size > ctx->left)
			size = ctx->left;
		if (!size)
			return 0;

		seek(instance->storage, ctx->pos);

		size_t read = fread(output, 1, size, instance->storage);
		ctx->pos += read;
		ctx->left -= read;

		return read;
	}
	else {
		const size_t BUFSIZE = 1024 * 128;
		if (!ctx->buffer) {
			ctx->buffer = malloc(BUFSIZE);
		}

		int seekreq = 1;
		size_t decomp_bytes = 0;
		while (decomp_bytes < size)
		{
			size_t src_size = BUFSIZE - ctx->leftover;
			if (src_size > ctx->left)
				src_size = ctx->left;

			size_t read = 0;
			if (src_size) {
				assert(ctx->buffer + ctx->leftover + src_size <= ctx->buffer + BUFSIZE);
				if (seekreq)
					seek(instance->storage, ctx->pos);
				read = fread(ctx->buffer + ctx->leftover, 1, src_size, instance->storage);
				ctx->pos += read;
				ctx->left -= read;
				seekreq = 0;
			}

			src_size = ctx->leftover + read;

			if (!src_size)
				break;

			size_t dst_size = size - decomp_bytes;

            size_t ret = LZ4F_decompress(ctx->lz4_ctx, (void*)((uintptr_t)output + decomp_bytes), &dst_size, ctx->buffer, &src_size, &ctx->lz4_opts);
            if (LZ4F_isError(ret))
                ret = 0;

			decomp_bytes += dst_size;


			ctx->leftover = (ctx->leftover + read) - src_size;
			memmove(ctx->buffer, ctx->buffer + src_size, ctx->leftover);

			if (!ret)
				break;
		}

		return decomp_bytes;
	}
	return 0;
}

void eu07vfs_close_file(eu07vfs_instance instance, eu07vfs_file_ctx ctx)
{
	if (ctx->lz4_enabled)
		LZ4F_freeDecompressionContext(ctx->lz4_ctx);
	if (ctx->buffer)
		free(ctx->buffer);
	free(ctx);
}

void eu07vfs_destroy(eu07vfs_instance instance)
{
	hashmap_scan(instance->hm, user_iter, NULL);
	hashmap_free(instance->hm);
	free(instance);
}
