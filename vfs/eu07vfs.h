#ifndef EU07VFS_H
#define EU07VFS_H

#ifdef __cplusplus
extern "C" {
#endif
	struct _eu07vfs_instance;
	struct _eu07vfs_file_ctx;
	typedef struct _eu07vfs_instance* eu07vfs_instance;
	typedef uint64_t eu07vfs_file_handle;
	typedef struct _eu07vfs_file_ctx* eu07vfs_file_ctx;

	eu07vfs_instance eu07vfs_init(const char *path);
	eu07vfs_file_handle eu07vfs_lookup_file(eu07vfs_instance instance, const char *file, size_t filelen);
	eu07vfs_file_ctx eu07vfs_open_file(eu07vfs_instance instance, eu07vfs_file_handle handle);
	uint64_t eu07vfs_get_file_size(eu07vfs_instance instance, eu07vfs_file_ctx ctx);
	size_t eu07vfs_read_file(eu07vfs_instance instance, eu07vfs_file_ctx ctx, void *output, size_t size);
	void eu07vfs_close_file(eu07vfs_instance instance, eu07vfs_file_ctx ctx);
	void eu07vfs_destroy(eu07vfs_instance instance);

	//const char* eu07vfs_runtimedir_get_path(eu07vfs_instance *instance, const char *file, size_t filelen);
	//void eu07vfs_runtimedir_rescan_file(eu07vfs_instance *instance, const char *file, size_t filelen);

#define EU07VFS_NULL_HANDLE ((size_t)-1)
#ifdef __cplusplus
}
#endif
#endif
