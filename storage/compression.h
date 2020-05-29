#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <libdeflate.h>
#include <lz4hc.h>
#include <zstd.h>

template<typename T>
class CtxManager {
public:
	typedef std::function<T()> create_ctx;
	typedef std::function<void(T&)> delete_ctx;

	CtxManager(create_ctx create, delete_ctx delete_) :
		CreateCtx(create),
		DeleteCtx(delete_) { }

	~CtxManager() {
		for (auto& ctx : Ctx) {
			DeleteCtx(ctx);
		}
	}

	T& GetCtx(std::unique_lock<std::mutex>& lock) {
		std::unique_lock<std::mutex> lck(Mutex);

		auto mtxIt = Mutexes.begin();
		for (auto ctxIt = Ctx.begin(); ctxIt != Ctx.end(); ctxIt++, mtxIt++) {
			lock = std::unique_lock<std::mutex>(*mtxIt, std::try_to_lock);
			if (lock.owns_lock()) {
				return *ctxIt;
			}
		}

		lock = std::unique_lock<std::mutex>(Mutexes.emplace_back());
		return Ctx.emplace_back(CreateCtx());
	}

private:
	create_ctx CreateCtx;
	delete_ctx DeleteCtx;

	std::mutex Mutex;
	std::deque<T> Ctx;
	std::deque<std::mutex> Mutexes;
};

class Compressor {
public:
	using buffer_value = std::pair<std::shared_ptr<char[]>, size_t>;

	Compressor(uint32_t storageFlags);
	~Compressor();

	buffer_value StorageCompress(std::shared_ptr<char[]> buffer, size_t buffer_size);

	buffer_value ZlibDecompress(FILE* File, size_t& inBufSize);
	buffer_value ZstdDecompress(FILE* File, size_t& inBufSize);
	buffer_value LZ4Decompress(FILE* File, size_t& inBufSize);

private:
	std::function<buffer_value(std::shared_ptr<char[]>, size_t)> CompressFunc;

	int CLevel;
	std::unique_ptr<CtxManager<void*>> CCtx;
	std::unique_ptr<CtxManager<libdeflate_decompressor*>> ZlibDCtx;
	std::unique_ptr<CtxManager<ZSTD_DCtx*>> ZstdDCtx;
	//std::unique_ptr<CtxManager<void*>> LZ4DCtx; lz4 decompression doesn't use a ctx
	uint32_t StorageFlags;
};