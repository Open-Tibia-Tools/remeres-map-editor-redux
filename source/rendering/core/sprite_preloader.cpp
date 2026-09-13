//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "rendering/core/sprite_preloader.h"
#include "rendering/core/graphics.h"
#include "rendering/core/normal_image.h"
#include "rendering/core/sprite_archive.h"
#include "ui/gui.h"

#include <algorithm>
#include <cassert>
#include <span>

namespace {
	constexpr size_t RGBA_COMPONENTS = 4;

	[[nodiscard]] size_t resultByteSize(const ImageDimensions& dimensions) {
		return dimensions.pixelCount() * RGBA_COMPONENTS;
	}
}

SpritePreloader& SpritePreloader::get() {
	static SpritePreloader instance;
	return instance;
}

SpritePreloader::SpritePreloader() : stopping(false) {
	unsigned int num_threads = std::clamp(std::thread::hardware_concurrency(), MIN_WORKER_THREADS, MAX_WORKER_THREADS);
	workers.reserve(num_threads);
	for (unsigned int i = 0; i < num_threads; ++i) {
		workers.emplace_back([this](std::stop_token stop_token) {
			this->workerLoop(stop_token);
		});
	}
}

SpritePreloader::~SpritePreloader() {
	shutdown();
}

void SpritePreloader::shutdown() {
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		if (stopping) {
			return;
		}
		stopping = true;
	}
	for (auto& worker : workers) {
		worker.request_stop(); // Correctly signaled transition for jthread's stop_token
	}
	cv.notify_all();
}

void SpritePreloader::clear() {
	std::lock_guard<std::mutex> lock(queue_mutex);
	// Bump the epoch so any in-flight worker result becomes stale.
	++active_epoch;
	task_queue = std::queue<Task>();
	result_queue = std::queue<Result>();
	pending_ids.clear();
	queued_result_bytes = 0;
}

void SpritePreloader::preload(GameSprite* spr, int pattern_x, int pattern_y, int pattern_z, int frame) {
	if (!spr) {
		return;
	}

	const auto archive = g_gui.gfx.getSpriteArchive();
	const bool has_transparency = g_gui.gfx.hasTransparency();
	if (!archive) {
		return;
	}

	struct PendingTask {
		NormalImage* img = nullptr;
		ArchiveSpriteKey key;
		uint32_t generation_id = 0;
	};

	static thread_local std::vector<PendingTask> ids_to_enqueue;
	ids_to_enqueue.clear();

	// Reserve for typical sprite sizes (1x1, 2x2, max layers etc) to minimize allocations
	if (ids_to_enqueue.capacity() < 64) {
		ids_to_enqueue.reserve(64);
	}

	for (int cx = 0; cx < spr->width; ++cx) {
		for (int cy = 0; cy < spr->height; ++cy) {
			for (int cf = 0; cf < spr->layers; ++cf) {
				int idx = spr->getIndex(cx, cy, cf, pattern_x, pattern_y, pattern_z, frame);

				if (idx >= static_cast<int>(spr->numsprites)) {
					if (spr->numsprites == 1) {
						idx = 0;
					} else {
						idx %= spr->numsprites;
					}
				}

				if (idx < 0 || static_cast<size_t>(idx) >= spr->spriteList.size()) {
					continue;
				}

				NormalImage* img = spr->spriteList[idx];
				if (img && !img->isGLLoaded && !img->is_preloading) {
					// Ensure parent is set so GC can invalidate cached_default_region
					// when evicting this sprite later (prevents stale cache -> wrong sprite)
					img->addParent(spr);
					ids_to_enqueue.push_back({ img, { archive.get(), img->id }, img->generation_id });
				}
			}
		}
	}

	if (!ids_to_enqueue.empty()) {
		std::lock_guard<std::mutex> lock(queue_mutex);
		if (task_queue.size() >= MAX_QUEUE_SIZE) {
			return; // Drop requests if queue is slammed
		}

		for (const auto& pending : ids_to_enqueue) {
			if (task_queue.size() >= MAX_QUEUE_SIZE) {
				break;
			}
			const PendingSpriteKey pending_key {
				.key = pending.key,
				.generation_id = pending.generation_id,
				.epoch = active_epoch,
			};
			if (pending_ids.insert(pending_key).second) {
				task_queue.push({ pending_key, archive, has_transparency });
				if (pending.img) {
					pending.img->is_preloading = true;
				}
			}
		}
		cv.notify_all();
	}
}

void SpritePreloader::workerLoop(std::stop_token stop_token) {
	while (!stop_token.stop_requested()) {
		Task task;
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			cv.wait(lock, [this, &stop_token] {
				return stop_token.stop_requested() || (!task_queue.empty()
					&& result_queue.size() < MAX_RESULT_QUEUE_SIZE
					&& queued_result_bytes < MAX_RESULT_QUEUE_BYTES);
			});
			if (stop_token.stop_requested()) {
				break;
			}
			task = std::move(task_queue.front());
			task_queue.pop();
		}
		std::unique_ptr<uint8_t[]> rgba;
		ImageDimensions dimensions;
		if (!task.archive || !task.archive->readRGBA(task.pending.key.id, task.has_transparency, rgba, dimensions)) {
			rgba.reset();
		}
		const size_t result_bytes = resultByteSize(dimensions);

		{
			std::lock_guard<std::mutex> lock(queue_mutex);
			if (rgba && result_queue.size() < MAX_RESULT_QUEUE_SIZE && queued_result_bytes + result_bytes <= MAX_RESULT_QUEUE_BYTES) {
				queued_result_bytes += result_bytes;
				result_queue.push({ task.pending, std::move(rgba), dimensions, std::move(task.archive) });
			} else {
				pending_ids.erase(task.pending);
			}
		}
	}
}

void SpritePreloader::update() {
	// CRITICAL: This method MUST only be called from the main GUI/OpenGL thread.
	assert(wxIsMainThread());

	// Move results to a local queue under lock to minimize holding time.
	std::queue<Result> results;
	uint64_t current_epoch = 0;
	size_t result_count = 0;
	size_t upload_bytes = 0;
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		if (result_queue.empty()) {
			return;
		}
		current_epoch = active_epoch;
		while (!result_queue.empty() && result_count < MAX_UPLOADS_PER_FRAME) {
			const size_t next_result_bytes = resultByteSize(result_queue.front().dimensions);
			if (result_count > 0 && upload_bytes + next_result_bytes > MAX_UPLOAD_BYTES_PER_FRAME) {
				break;
			}

			upload_bytes += next_result_bytes;
			queued_result_bytes = queued_result_bytes > next_result_bytes ? queued_result_bytes - next_result_bytes : 0;
			results.push(std::move(result_queue.front()));
			result_queue.pop();
			++result_count;
		}
	}

	thread_local std::vector<PendingSpriteKey> keys_processed;
	keys_processed.clear();
	keys_processed.reserve(result_count);

	const auto current_archive = g_gui.gfx.getSpriteArchive();
	const bool graphics_unloaded = g_gui.gfx.isUnloaded();

	while (!results.empty()) {
		Result res = std::move(results.front());
		results.pop();

		const auto pending = res.pending;
		const auto id = pending.key.id;
		keys_processed.push_back(pending);

		if (pending.epoch != current_epoch) {
			continue;
		}

		// Check if GraphicManager is loaded, for the correct sprite file, and ID is valid
		if (res.archive == current_archive && !graphics_unloaded && id < g_gui.gfx.image_space.size()) {
			auto& img_ptr = g_gui.gfx.image_space[id];
			if (img_ptr && img_ptr->isNormalImage()) {
				// Use static_cast for performance, as we know the type from loaders
				auto* img = static_cast<NormalImage*>(img_ptr.get());

				// Validate Sprite Identity & Generation
				// Check ID match, Generation match, and GLLoaded state
				if (img->id == id && img->generation_id == pending.generation_id && !img->isGLLoaded) {
					if (img->pixel_width != res.dimensions.width || img->pixel_height != res.dimensions.height) {
						img->pixel_width = res.dimensions.width;
						img->pixel_height = res.dimensions.height;
						for (GameSprite* parent : img->parents) {
							if (parent) {
								parent->invalidateMetricCaches();
							}
						}
					}
					img->fulfillPreload(std::move(res.data));
				} else {
					img->is_preloading = false;
				}
			}
		}
	}

	if (!keys_processed.empty()) {
		std::lock_guard<std::mutex> lock(queue_mutex);
		for (const auto& pending : keys_processed) {
			pending_ids.erase(pending);
		}
	}
	cv.notify_all();
}

namespace rme {
	void collectTileSprites(GameSprite* spr, int pattern_x, int pattern_y, int pattern_z, int frame) {
		SpritePreloader::get().preload(spr, pattern_x, pattern_y, pattern_z, frame);
	}
}
