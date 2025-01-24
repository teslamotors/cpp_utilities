#pragma once

#include "fixed_containers/assert_or_abort.hpp"
#include "fixed_containers/bidirectional_iterator.hpp"
#include "fixed_containers/concepts.hpp"
#include "fixed_containers/enum_utils.hpp"
#include "fixed_containers/fixed_bitset.hpp"
#include "fixed_containers/erase_if.hpp"
#include "fixed_containers/filtered_integer_range_iterator.hpp"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <memory>

namespace fixed_containers::enum_set_detail
{
template <class K, class EnumSetType>
class EnumSetBuilder
{
public:
    constexpr EnumSetBuilder() = default;

    constexpr EnumSetBuilder& insert(const K& key) & noexcept
    {
        enum_set_.insert(key);
        return *this;
    }
    constexpr EnumSetBuilder&& insert(const K& key) && noexcept
    {
        enum_set_.insert(key);
        return std::move(*this);
    }

    constexpr EnumSetBuilder& insert(std::initializer_list<K> list) & noexcept
    {
        enum_set_.insert(list);
        return *this;
    }
    constexpr EnumSetBuilder&& insert(std::initializer_list<K> list) && noexcept
    {
        enum_set_.insert(list);
        return std::move(*this);
    }

    template <InputIterator InputIt>
    constexpr EnumSetBuilder& insert(InputIt first, InputIt last) & noexcept
    {
        enum_set_.insert(first, last);
        return *this;
    }
    template <InputIterator InputIt>
    constexpr EnumSetBuilder&& insert(InputIt first, InputIt last) && noexcept
    {
        enum_set_.insert(first, last);
        return std::move(*this);
    }

    template <class Container>
    constexpr EnumSetBuilder& insert(const Container& container) & noexcept
    {
        enum_set_.insert(container.cbegin(), container.cend());
        return *this;
    }
    template <class Container>
    constexpr EnumSetBuilder&& insert(const Container& container) && noexcept
    {
        enum_set_.insert(container.cbegin(), container.cend());
        return std::move(*this);
    }

    constexpr EnumSetBuilder& erase(const K& key) & noexcept
    {
        enum_set_.erase(key);
        return *this;
    }
    constexpr EnumSetBuilder&& erase(const K& key) && noexcept
    {
        enum_set_.erase(key);
        return std::move(*this);
    }

    [[nodiscard]] constexpr EnumSetType build() const& { return enum_set_; }
    constexpr EnumSetType build() && { return std::move(enum_set_); }

private:
    EnumSetType enum_set_;
};
}  // namespace fixed_containers::enum_set_detail

namespace fixed_containers
{
/**
 * Fixed-capacity set for enum keys. Properties:
 *  - constexpr
 *  - retains the properties of V (e.g. if T is trivially copyable, then so is EnumMap<K, V>)
 *  - no pointers stored (data layout is purely self-referential and can be serialized directly)
 *  - no dynamic allocations
 *
 * Note that despite what the underlying implementation might suggest, this is NOT a bitset. EnumSet
 * is a set for the special case when the keys are enum values, so the API matches that of std::set.
 */
template <class K>
class EnumSet
{
    using Self = EnumSet<K>;

public:
    using key_type = K;
    using value_type = K;
    using const_reference = const value_type&;
    using reference = const_reference;
    using const_pointer = std::add_pointer_t<const_reference>;
    using pointer = const_pointer;

private:
    using EnumAdapterType = rich_enums::EnumAdapter<K>;
    static constexpr std::size_t ENUM_COUNT = EnumAdapterType::count();
    using KeyArrayType = std::array<K, ENUM_COUNT>;
    using StorageType = FixedBitSet<ENUM_COUNT>;
    static constexpr const KeyArrayType& ENUM_VALUES = EnumAdapterType::values();

    struct IndexPredicate
    {
        const StorageType* array_set_;
        constexpr bool operator()(const std::size_t index) const { return (*array_set_)[index]; }
        constexpr bool operator==(const IndexPredicate&) const = default;
    };

    class ReferenceProvider
    {
        FilteredIntegerRangeEntryProvider<IndexPredicate, CompileTimeIntegerRange<0, ENUM_COUNT>>
            present_indices_;

    public:
        constexpr ReferenceProvider()
          : ReferenceProvider(nullptr, ENUM_COUNT)
        {
        }

        constexpr ReferenceProvider(const StorageType* array_set,
                                    const std::size_t current_index)
          : present_indices_{
                CompileTimeIntegerRange<0, ENUM_COUNT>{}, current_index, IndexPredicate{array_set}}
        {
        }

        constexpr void advance() noexcept { present_indices_.advance(); }
        constexpr void recede() noexcept { present_indices_.recede(); }
        [[nodiscard]] constexpr const_reference get() const noexcept
        {
            return ENUM_VALUES[present_indices_.get()];
        }
        constexpr bool operator==(const ReferenceProvider&) const = default;
    };

    template <IteratorDirection DIRECTION>
    using IteratorImpl = BidirectionalIterator<ReferenceProvider,
                                               ReferenceProvider,
                                               IteratorConstness::CONSTANT_ITERATOR,
                                               DIRECTION>;

public:
    using const_iterator = IteratorImpl<IteratorDirection::FORWARD>;
    using iterator = const_iterator;
    using const_reverse_iterator = IteratorImpl<IteratorDirection::REVERSE>;
    using reverse_iterator = const_reverse_iterator;
    using size_type = typename KeyArrayType::size_type;
    using difference_type = typename KeyArrayType::difference_type;

public:
    using Builder = enum_set_detail::EnumSetBuilder<K, Self>;

    template <class EnumSetType = Self>
    static constexpr EnumSetType all()
    {
        EnumSetType output{};
        output.insert(ENUM_VALUES.cbegin(), ENUM_VALUES.cend());
        return output;
    }

    template <class EnumSetType = Self>
    static constexpr EnumSetType none()
    {
        return {};
    }

    template <class Container, class EnumSetType = Self>
    static constexpr EnumSetType complement_of(const Container& container)
    {
        auto output = all<EnumSetType>();
        for (const K& key : container)
        {
            output.erase(key);
        }

        return output;
    }

    template <class Container, class EnumSetType = Self>
    static constexpr EnumSetType copy_of(const Container& container)
    {
        EnumSetType output{};
        output.insert(container.begin(), container.end());
        return output;
    }

    [[nodiscard]] static constexpr std::size_t static_max_size() noexcept { return ENUM_COUNT; }

public:  // Public so this type is a structural type and can thus be used in template parameters
    // std::bitset is not sufficiently constexpr to use here, using a std::array instead.
    StorageType IMPLEMENTATION_DETAIL_DO_NOT_USE_array_set_;
    std::size_t IMPLEMENTATION_DETAIL_DO_NOT_USE_size_;

public:
    constexpr EnumSet() noexcept
      : IMPLEMENTATION_DETAIL_DO_NOT_USE_array_set_()
      , IMPLEMENTATION_DETAIL_DO_NOT_USE_size_{}
    {
    }

    template <InputIterator InputIt>
    constexpr EnumSet(InputIt first, InputIt last) noexcept
      : EnumSet()
    {
        insert(first, last);
    }

    constexpr EnumSet(std::initializer_list<K> list) noexcept
      : EnumSet()
    {
        insert(list);
    }

public:
    [[nodiscard]] constexpr const_iterator cbegin() const noexcept
    {
        return create_const_iterator(0);
    }
    [[nodiscard]] constexpr const_iterator cend() const noexcept
    {
        return create_const_iterator(ENUM_COUNT);
    }
    [[nodiscard]] constexpr const_iterator begin() const noexcept { return cbegin(); }
    [[nodiscard]] constexpr const_iterator end() const noexcept { return cend(); }

    [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept
    {
        return create_const_reverse_iterator(ENUM_COUNT);
    }
    [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept
    {
        return create_const_reverse_iterator(0);
    }
    [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return crbegin(); }
    [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return crend(); }

    [[nodiscard]] constexpr std::size_t max_size() const noexcept { return static_max_size(); }
    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_size_;
    }
    [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

    constexpr void clear() noexcept
    {
        const std::size_t max_sz = max_size();
        for (std::size_t i = 0; i < max_sz; i++)
        {
            if (contains_at(i))
            {
                reset_at(i);
            }
        }
    }
    constexpr std::pair<const_iterator, bool> insert(const K& key) noexcept
    {
        const std::size_t ordinal = EnumAdapterType::ordinal(key);
        if (contains_at(ordinal))
        {
            return {create_const_iterator(ordinal), false};
        }

        increment_size();
        array_set_unchecked_at(EnumAdapterType::ordinal(key)) = true;
        return {create_const_iterator(ordinal), true};
    }
    constexpr const_iterator insert(const_iterator /*hint*/, const K& key) noexcept
    {
        return insert(key).first;
    }
    constexpr void insert(std::initializer_list<K> list) noexcept
    {
        this->insert(list.begin(), list.end());
    }
    template <InputIterator InputIt>
    constexpr void insert(InputIt first, InputIt last) noexcept
    {
        for (; first != last; std::advance(first, 1))
        {
            this->insert(*first);
        }
    }

    template <class... Args>
    constexpr std::pair<const_iterator, bool> emplace(Args&&... args)
    {
        return insert(K{std::forward<Args>(args)...});
    }
    template <class... Args>
    constexpr iterator emplace_hint(const_iterator hint, Args&&... args)
    {
        return insert(hint, K{std::forward<Args>(args)...});
    }

    constexpr const_iterator erase(const_iterator pos) noexcept
    {
        assert_or_abort(pos != cend());
        const std::size_t index = EnumAdapterType::ordinal(*pos);
        assert_or_abort(contains_at(index));
        reset_at(index);
        return create_const_iterator(index);
    }

    constexpr const_iterator erase(const_iterator first, const_iterator last) noexcept
    {
        const std::size_t from_inclusive =
            first == end() ? ENUM_COUNT : EnumAdapterType::ordinal(*first);
        const std::size_t to_exclusive =
            last == end() ? ENUM_COUNT : EnumAdapterType::ordinal(*last);
        assert_or_abort(from_inclusive <= to_exclusive);

        for (std::size_t i = from_inclusive; i < to_exclusive; i++)
        {
            if (contains_at(i))
            {
                reset_at(i);
            }
        }

        return create_const_iterator(to_exclusive);
    }

    constexpr size_type erase(const K& key) noexcept
    {
        const std::size_t index = EnumAdapterType::ordinal(key);
        if (!contains_at(index))
        {
            return 0;
        }

        reset_at(index);
        return 1;
    }

    [[nodiscard]] constexpr bool contains(const K& key) const noexcept
    {
        return contains_at(EnumAdapterType::ordinal(key));
    }

    [[nodiscard]] constexpr bool contains(const EnumSet<K>& other) const noexcept
    {
        return ((other.array_set() & array_set()) == other.array_set());
    }

    constexpr bool operator==(const EnumSet<K>& other) const
    {
        return array_set() == other.array_set();
    }

private:
    [[nodiscard]] constexpr const StorageType& array_set() const
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_array_set_;
    }
    constexpr StorageType& array_set()
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_array_set_;
    }
    [[nodiscard]] constexpr bool array_set_unchecked_at(const std::size_t index) const
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_array_set_[index];
    }
    constexpr StorageType::reference array_set_unchecked_at(const std::size_t index)
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_array_set_[index];
    }
    constexpr void increment_size(const std::size_t n = 1)
    {
        IMPLEMENTATION_DETAIL_DO_NOT_USE_size_ += n;
    }
    constexpr void decrement_size(const std::size_t n = 1)
    {
        IMPLEMENTATION_DETAIL_DO_NOT_USE_size_ -= n;
    }
    constexpr void set_size(const std::size_t n)
    {
        IMPLEMENTATION_DETAIL_DO_NOT_USE_size_ = n;
    }
    [[nodiscard]] constexpr const_iterator create_const_iterator(
        const std::size_t start_index) const noexcept
    {
        return const_iterator{ReferenceProvider{std::addressof(array_set()), start_index}};
    }
    [[nodiscard]] constexpr const_reverse_iterator create_const_reverse_iterator(
        const std::size_t start_index) const noexcept
    {
        return const_reverse_iterator{ReferenceProvider{std::addressof(array_set()), start_index}};
    }

    [[nodiscard]] constexpr bool contains_at(const std::size_t index) const noexcept
    {
        return array_set_unchecked_at(index);
    }

    constexpr void reset_at(const std::size_t index) noexcept
    {
        assert_or_abort(contains_at(index));
        array_set_unchecked_at(index) = false;
        decrement_size();
    }

    constexpr EnumSet &operator+=(const K e) noexcept
    {
        insert(e);
        return *this;
    }

    constexpr EnumSet &operator-=(const K e) noexcept
    {
        erase(e);
        return *this;
    }

    constexpr EnumSet &operator+=(const EnumSet<K> &rhs) noexcept
    {
        array_set() |= rhs.array_set();
        set_size(array_set().count());
        return *this;
    }

    constexpr EnumSet &operator-=(const EnumSet<K> &rhs) noexcept
    {
        array_set() &= ~rhs.array_set();
        set_size(array_set().count());
        return *this;
    }
};

template <typename K>
[[nodiscard]] constexpr bool is_full(const EnumSet<K>& container)
{
    return container.size() >= container.max_size();
}

template <InputIterator InputIt>
EnumSet(InputIt first,
        InputIt last) noexcept -> EnumSet<typename std::iterator_traits<InputIt>::value_type>;

template <class K, class Predicate>
constexpr typename EnumSet<K>::size_type erase_if(EnumSet<K>& container, Predicate predicate)
{
    return erase_if_detail::erase_if_impl(container, predicate);
}

}  // namespace fixed_containers

// Specializations
namespace std
{
template <typename K>
struct tuple_size<fixed_containers::EnumSet<K>> : std::integral_constant<std::size_t, 0>
{
    // Implicit Structured Binding due to the fields being public is disabled
};
}  // namespace std
