TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Global Caches
//!
//! This file implements the GlobalASTCache and GlobalLibraryIRCache
//! used by the code generation pipeline for caching parsed modules
//! and generated library IR across compilation units.

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "common.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"

#include <filesystem>
#include <iomanip>
#include <set>

namespace tml::codegen {

// ============================================================================
// GlobalASTCache Implementation
// ============================================================================

GlobalASTCache& GlobalASTCache::instance() {
    static GlobalASTCache cache;
    return cache;
}

bool GlobalASTCache::has(const std::string& module_path) const {
    std::shared_lock lock(mutex_);
    return cache_.find(module_path) != cache_.end();
}

const parser::Module* GlobalASTCache::get(const std::string& module_path) const {
    std::shared_lock lock(mutex_);
    auto it = cache_.find(module_path);
    if (it != cache_.end()) {
        ++hits_;
        return &it->second;
    }
    ++misses_;
    return nullptr;
}

void GlobalASTCache::put(const std::string& module_path, parser::Module module) {
    // Only cache library modules
    if (!should_cache(module_path)) {
        return;
    }

    std::unique_lock lock(mutex_);
    // Only insert if not already present (first parse wins)
    if (cache_.find(module_path) == cache_.end()) {
        cache_.emplace(module_path, std::move(module));
    }
}

void GlobalASTCache::clear() {
    std::unique_lock lock(mutex_);
    cache_.clear();
    hits_.store(0, std::memory_order_relaxed);
    misses_.store(0, std::memory_order_relaxed);
}

GlobalASTCache::Stats GlobalASTCache::get_stats() const {
    std::shared_lock lock(mutex_);
    return Stats{.total_entries = cache_.size(),
                 .cache_hits = hits_.load(std::memory_order_relaxed),
                 .cache_misses = misses_.load(std::memory_order_relaxed)};
}

bool GlobalASTCache::should_cache(const std::string& module_path) {
    // Cache library modules: core::*, std::*, test
    if (module_path.starts_with("core::") || module_path.starts_with("std::") ||
        module_path == "test" || module_path.starts_with("test::")) {
        return true;
    }
    return false;
}

// ============================================================================
// GlobalLibraryIRCache Implementation
// ============================================================================

GlobalLibraryIRCache& GlobalLibraryIRCache::instance() {
    static GlobalLibraryIRCache cache;
    return cache;
}

bool GlobalLibraryIRCache::has(const std::string& key) const {
    std::shared_lock lock(mutex_);
    return cache_.find(key) != cache_.end();
}

const CachedIREntry* GlobalLibraryIRCache::get(const std::string& key) const {
    std::shared_lock lock(mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        ++hits_;
        return &it->second;
    }
    ++misses_;
    return nullptr;
}

void GlobalLibraryIRCache::put(const std::string& key, CachedIREntry entry) {
    std::unique_lock lock(mutex_);
    // Only insert if not already present (first generation wins)
    if (cache_.find(key) == cache_.end()) {
        cache_.emplace(key, std::move(entry));
    }
}

std::vector<const CachedIREntry*> GlobalLibraryIRCache::get_by_type(CachedIRType type) const {
    std::shared_lock lock(mutex_);
    std::vector<const CachedIREntry*> result;
    for (const auto& [key, entry] : cache_) {
        if (entry.type == type) {
            result.push_back(&entry);
        }
    }
    return result;
}

std::vector<const CachedIREntry*> GlobalLibraryIRCache::get_all() const {
    std::shared_lock lock(mutex_);
    std::vector<const CachedIREntry*> result;
    result.reserve(cache_.size());
    for (const auto& [key, entry] : cache_) {
        result.push_back(&entry);
    }
    return result;
}

void GlobalLibraryIRCache::clear() {
    std::unique_lock lock(mutex_);
    cache_.clear();
    in_progress_.clear();
    hits_.store(0, std::memory_order_relaxed);
    misses_.store(0, std::memory_order_relaxed);
}

GlobalLibraryIRCache::Stats GlobalLibraryIRCache::get_stats() const {
    std::shared_lock lock(mutex_);
    Stats stats{};
    stats.total_entries = cache_.size();
    stats.cache_hits = hits_.load(std::memory_order_relaxed);
    stats.cache_misses = misses_.load(std::memory_order_relaxed);

    for (const auto& [key, entry] : cache_) {
        switch (entry.type) {
        case CachedIRType::StructDef:
            ++stats.struct_defs;
            break;
        case CachedIRType::EnumDef:
            ++stats.enum_defs;
            break;
        case CachedIRType::Function:
            ++stats.functions;
            break;
        case CachedIRType::ImplMethod:
            ++stats.impl_methods;
            break;
        case CachedIRType::GenericInst:
            ++stats.generic_insts;
            break;
        }
    }
    return stats;
}

bool GlobalLibraryIRCache::try_claim(const std::string& key) {
    std::unique_lock lock(mutex_);
    // If already cached or in progress, don't claim
    if (cache_.find(key) != cache_.end()) {
        return false;
    }
    if (in_progress_.find(key) != in_progress_.end()) {
        return false;
    }
    // Claim this key for generation
    in_progress_.insert(key);
    return true;
}

void GlobalLibraryIRCache::release_claim(const std::string& key) {
    std::unique_lock lock(mutex_);
    in_progress_.erase(key);
}

void GlobalLibraryIRCache::preload_library_definitions() {
    // TODO: Pre-generate common library instantiations
    // This will scan library modules and generate IR for:
    // - TryFrom/From for all numeric type pairs
    // - Common generic instantiations (List[I32], etc.)
    // - Frequently used library functions
    TML_DEBUG_LN("[IR_CACHE] Preload library definitions (not yet implemented)");
}

} // namespace tml::codegen
