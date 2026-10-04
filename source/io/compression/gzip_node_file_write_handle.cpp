#include "io/compression/gzip_node_file_write_handle.h"

#include <zlib.h>

GzipNodeFileWriteHandle::GzipNodeFileWriteHandle(const std::filesystem::path& path, const std::string& identifier) {
	if (identifier.length() != 4) {
		error_code = FILE_INVALID_IDENTIFIER;
		return;
	}
#ifdef _WIN32
	gz = gzopen_w(path.c_str(), "wb6");
#else
	gz = gzopen(path.c_str(), "wb6");
#endif
	if (!gz) {
		error_code = FILE_COULD_NOT_OPEN;
		return;
	}
	gzbuffer(gz, 64 * 1024);

	if (gzwrite(gz, identifier.data(), 4) != 4) {
		error_code = FILE_WRITE_ERROR;
	}

	cache.resize(INITIAL_CACHE_SIZE + 1);
	local_write_index = 0;
}

GzipNodeFileWriteHandle::~GzipNodeFileWriteHandle() {
	close();
}

void GzipNodeFileWriteHandle::close() {
	if (!gz) {
		return;
	}
	renewCache();
	if (gzclose(gz) != Z_OK) {
		error_code = FILE_WRITE_ERROR;
	}
	gz = nullptr;
}

bool GzipNodeFileWriteHandle::renewCache() {
	const size_t pending = local_write_index;
	local_write_index = 0;
	if (cache.empty()) {
		cache.resize(INITIAL_CACHE_SIZE + 1);
		return true;
	}
	if (pending > 0 && gzwrite(gz, cache.data(), static_cast<unsigned>(pending)) != static_cast<int>(pending)) {
		error_code = FILE_WRITE_ERROR;
		return false;
	}
	return true;
}
