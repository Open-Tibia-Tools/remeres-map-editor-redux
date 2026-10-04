#ifndef RME_IO_COMPRESSION_GZIP_NODE_FILE_WRITE_HANDLE_H_
#define RME_IO_COMPRESSION_GZIP_NODE_FILE_WRITE_HANDLE_H_

#include <filesystem>
#include <string>
#include "io/filehandle.h"

struct gzFile_s;

// Streams the node file through zlib's gzip writer (Canary / Crystal Server compressed map format).
// close() must be called explicitly and isOk() checked afterwards: gzip errors surface on flush.
class GzipNodeFileWriteHandle : public NodeFileWriteHandle {
public:
	GzipNodeFileWriteHandle(const std::filesystem::path& path, const std::string& identifier);
	~GzipNodeFileWriteHandle() override;

	void close() override;
	[[nodiscard]] bool isOpen() override {
		return gz != nullptr;
	}
	[[nodiscard]] bool isOk() override {
		return error_code == FILE_NO_ERROR;
	}

protected:
	bool renewCache() override;

private:
	gzFile_s* gz = nullptr;
};

#endif // RME_IO_COMPRESSION_GZIP_NODE_FILE_WRITE_HANDLE_H_
