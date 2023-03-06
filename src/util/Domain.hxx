// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef DOMAIN_HXX
#define DOMAIN_HXX

class Domain {
	const char *const name;

public:
	constexpr explicit Domain(const char *_name) noexcept
		:name(_name) {}

	Domain(const Domain &) = delete;
	Domain &operator=(const Domain &) = delete;

	constexpr const char *GetName() const noexcept {
		return name;
	}

	bool operator==(const Domain &other) const noexcept {
		return this == &other;
	}

	bool operator!=(const Domain &other) const noexcept {
		return !(*this == other);
	}
};

#endif
