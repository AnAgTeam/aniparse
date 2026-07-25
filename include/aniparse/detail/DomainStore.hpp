/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/detail/DomainScanner.hpp"
#include "aniparse/utility/AtomicSharedPtr.hpp"
#include "aniparse/utility/Attributes.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aniparse::detail {

/**
 * @brief Immutable-snapshot domain registry for parser-like source stores.
 *
 * Readers load one complete snapshot. Writers make an Edit, mutate only its
 * local maps, then commit one newly built DomainScanner with an atomic swap.
 * A stale concurrent edit is rejected rather than silently overwriting a newer
 * registry revision.
 */
template <class T, class DomainContext, class DomainEmitter>
class DomainStore {
public:
	using ValuePtr = std::shared_ptr<T>;
	using DomainOverrides = std::map<std::string, std::vector<std::string>, std::less<>>;

	class Edit;

	DomainStore()
	    : snapshot_(make_snapshot({}, {})) {}

	[[nodiscard]] Edit begin_edit() ANIPARSE_LIFETIMEBOUND;

	[[nodiscard]] bool contains(std::string_view identifier) const {
		return snapshot_.load()->entries.contains(identifier);
	}

	[[nodiscard]] ValuePtr find_by_key(std::string_view identifier) const {
		auto snapshot = snapshot_.load();
		auto it       = snapshot->entries.find(identifier);
		return it == snapshot->entries.end() ? nullptr : it->second.value;
	}

	[[nodiscard]] std::vector<ValuePtr> values() const {
		auto snapshot = snapshot_.load();
		std::vector<ValuePtr> result;
		result.reserve(snapshot->entries.size());
		for (const auto& [identifier, entry] : snapshot->entries) {
			result.push_back(entry.value);
		}
		return result;
	}

	template <class Predicate>
	[[nodiscard]] ValuePtr find_by_host(std::string_view host, Predicate&& accepts) const {
		auto snapshot = snapshot_.load();
		auto labels   = split_domains(host);
		auto found    = snapshot->scanner->search_all(labels, [&accepts](ValuePtr& value) {
			return accepts(*value);
		});
		return found ? *found : nullptr;
	}

private:
	using Scanner = DomainScanner<ValuePtr>;

	struct Entry {
		ValuePtr value;
	};

	class DomainAdder final : public DomainContext {
	public:
		DomainAdder(Scanner& scanner, ValuePtr value)
		    : scanner_(scanner), value_(std::move(value)) {}

		void add_domain(std::string_view domain) override {
			scanner_.add_domain_parser(split_domains(domain), value_);
		}

	private:
		Scanner& scanner_;
		ValuePtr value_;
	};

	struct Snapshot {
		std::map<std::string, Entry, std::less<>> entries;
		DomainOverrides volatile_domains;
		std::shared_ptr<Scanner> scanner;
	};

	template <std::ranges::viewable_range Range>
	static constexpr auto split_domains(Range&& range) {
		// clang-format off
		return std::forward<Range>(range)
		       | std::views::reverse
		       | std::views::split('.')
		       | std::views::transform(std::views::reverse);
		// clang-format on
	}

	using DomainLabels = decltype(split_domains(std::string_view{}));
	using DomainLabel  = std::ranges::range_value_t<DomainLabels>;
	static_assert(std::ranges::contiguous_range<DomainLabel>);

	std::shared_ptr<const Snapshot> make_snapshot(
	    std::map<std::string, Entry, std::less<>> entries,
	    DomainOverrides volatile_domains) {
		auto scanner = std::make_shared<Scanner>();
		for (const auto& [identifier, entry] : entries) {
			DomainAdder adder(*scanner, entry.value);
			emitter_(*entry.value, adder);
			auto add_domains = [&scanner, &entry](const std::vector<std::string>& domains) {
				for (const std::string& domain : domains) {
					scanner->add_domain_parser(split_domains(domain), entry.value);
				}
			};
			if (auto it = volatile_domains.find(identifier); it != volatile_domains.end()) {
				add_domains(it->second);
			}
		}
		return std::make_shared<const Snapshot>(Snapshot{
		    .entries = std::move(entries),
		    .volatile_domains = std::move(volatile_domains),
		    .scanner = std::move(scanner),
		});
	}

	void commit(std::shared_ptr<const Snapshot> base,
	            std::map<std::string, Entry, std::less<>> entries,
	            DomainOverrides volatile_domains) {
		std::lock_guard lock(writer_mutex_);
		auto live = snapshot_.load();
		if (live != base) {
			throw std::logic_error("DomainStore edit is stale");
		}
		snapshot_.store(make_snapshot(std::move(entries), std::move(volatile_domains)));
	}

	mutable std::mutex writer_mutex_;
	DomainEmitter emitter_;
	AtomicSharedPtr<const Snapshot> snapshot_;

public:
	/** A local mutable draft of one DomainStore snapshot. */
	class Edit {
	public:
		Edit(const Edit&) = delete;
		Edit& operator=(const Edit&) = delete;
		Edit(Edit&&) noexcept = default;
		Edit& operator=(Edit&&) noexcept = default;

		ValuePtr add(std::string identifier, ValuePtr value) {
			auto [it, inserted] = entries_.emplace(
			    std::move(identifier), Entry{ .value = std::move(value) });
			return inserted ? it->second.value : nullptr;
		}

		[[nodiscard]] bool contains(std::string_view identifier) const {
			return entries_.contains(identifier);
		}

		bool remove(std::string_view identifier) {
			auto it = entries_.find(identifier);
			if (it == entries_.end()) {
				return false;
			}
			entries_.erase(it);
			return true;
		}

		void set_volatile_domains(DomainOverrides volatile_domains) {
			volatile_domains_ = std::move(volatile_domains);
		}

		void commit() {
			if (!store_) {
				throw std::logic_error("DomainStore edit was already committed");
			}
			store_->commit(base_, std::move(entries_), std::move(volatile_domains_));
			store_ = nullptr;
		}

	private:
		friend class DomainStore;

		explicit Edit(DomainStore& store)
		    : store_(&store), base_(store.snapshot_.load())
		    , entries_(base_->entries), volatile_domains_(base_->volatile_domains) {}

		DomainStore* store_;
		std::shared_ptr<const Snapshot> base_;
		std::map<std::string, Entry, std::less<>> entries_;
		DomainOverrides volatile_domains_;
	};

};

template <class T, class DomainContext, class DomainEmitter>
typename DomainStore<T, DomainContext, DomainEmitter>::Edit
DomainStore<T, DomainContext, DomainEmitter>::begin_edit() {
	return Edit(*this);
}

} // namespace aniparse::detail
