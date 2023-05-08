#pragma once

#include "fixed_containers/concepts.hpp"
#include "fixed_containers/enum_utils.hpp"
#include "fixed_containers/erase_if.hpp"
#include "fixed_containers/index_range_predicate_iterator.hpp"
#include "fixed_containers/optional_storage.hpp"
#include "fixed_containers/preconditions.hpp"
#include "fixed_containers/source_location.hpp"
#include "fixed_containers/type_name.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <type_traits>

namespace fixed_containers::enum_map_customize
{
template <class T, class K>
concept EnumMapChecking =
    requires(K key, std::size_t size, const std_transition::source_location& loc) {
        T::missing_enum_entries(loc);
        T::out_of_range(key, size, loc);  // ~ std::out_of_range
    };

template <class K, class V>
struct AbortChecking
{
    static constexpr auto KEY_TYPE_NAME = fixed_containers::type_name<K>();
    static constexpr auto VALUE_TYPE_NAME = fixed_containers::type_name<V>();

    [[noreturn]] static void missing_enum_entries(const std_transition::source_location& /*loc*/)
    {
        std::abort();
    }

    [[noreturn]] static void duplicate_enum_entries(const std_transition::source_location& /*loc*/)
    {
        std::abort();
    }

    [[noreturn]] static constexpr void out_of_range(const K& /*key*/,
                                                    const std::size_t /*size*/,
                                                    const std_transition::source_location& /*loc*/)
    {
        std::abort();
    }
};

}  // namespace fixed_containers::enum_map_customize

namespace fixed_containers::enum_map_detail
{
template <class K, class V, class EnumMapType>
class EnumMapBuilder
{
    using value_type = std::pair<const K, V>;

public:
    constexpr EnumMapBuilder() {}

    constexpr EnumMapBuilder& insert(const value_type& key) & noexcept
    {
        enum_map_.insert(key);
        return *this;
    }
    constexpr EnumMapBuilder&& insert(const value_type& key) && noexcept
    {
        enum_map_.insert(key);
        return std::move(*this);
    }
    constexpr EnumMapBuilder& insert(value_type&& key) & noexcept
    {
        enum_map_.insert(std::move(key));
        return *this;
    }
    constexpr EnumMapBuilder&& insert(value_type&& key) && noexcept
    {
        enum_map_.insert(std::move(key));
        return std::move(*this);
    }

    constexpr EnumMapBuilder& insert(std::initializer_list<value_type> list) & noexcept
    {
        enum_map_.insert(list);
        return *this;
    }
    constexpr EnumMapBuilder&& insert(std::initializer_list<value_type> list) && noexcept
    {
        enum_map_.insert(list);
        return std::move(*this);
    }

    template <InputIterator InputIt>
    constexpr EnumMapBuilder& insert(InputIt first, InputIt last) & noexcept
    {
        enum_map_.insert(first, last);
        return *this;
    }
    template <InputIterator InputIt>
    constexpr EnumMapBuilder&& insert(InputIt first, InputIt last) && noexcept
    {
        enum_map_.insert(first, last);
        return std::move(*this);
    }

    constexpr EnumMapType build() const& { return enum_map_; }
    constexpr EnumMapType build() && { return std::move(enum_map_); }

private:
    EnumMapType enum_map_;
};

template <class K, class V, enum_map_customize::EnumMapChecking<K> CheckingType>
class EnumMapBase
{
public:
    using key_type = K;
    using mapped_type = V;
    using value_type = std::pair<const K, V>;
    using reference = std::pair<const K&, V&>;
    using const_reference = std::pair<const K&, const V&>;
    using pointer = std::add_pointer_t<reference>;
    using const_pointer = std::add_pointer_t<const_reference>;

protected:  // [WORKAROUND-1] - Needed by the non-trivially-copyable flavor of EnumMap
    using Checking = CheckingType;
    using EnumAdapterType = rich_enums::EnumAdapter<K>;
    static constexpr std::size_t ENUM_COUNT = EnumAdapterType::count();
    using KeyArrayType = std::array<K, ENUM_COUNT>;
    using OptionalV = optional_storage_detail::OptionalStorage<V>;
    using ValueArrayType = std::array<OptionalV, ENUM_COUNT>;
    static constexpr const KeyArrayType& ENUM_VALUES = EnumAdapterType::values();

private:
    template <bool IS_CONST>
    struct PairProvider
    {
        using ConstOrMutableValueArray =
            std::conditional_t<IS_CONST, const ValueArrayType, ValueArrayType>;
        using ConstOrMutablePair =
            std::conditional_t<IS_CONST, std::pair<const K&, const V&>, std::pair<const K&, V&>>;

        ConstOrMutableValueArray* values_;
        std::size_t current_index_;

        constexpr PairProvider() noexcept
          : PairProvider{nullptr}
        {
        }

        constexpr PairProvider(ConstOrMutableValueArray* const values) noexcept
          : values_{values}
          , current_index_{}
        {
        }

        constexpr PairProvider(const PairProvider&) = default;
        constexpr PairProvider(PairProvider&&) noexcept = default;
        constexpr PairProvider& operator=(const PairProvider& other) = default;
        constexpr PairProvider& operator=(PairProvider&&) noexcept = default;

        // https://github.com/llvm/llvm-project/issues/62555
        template <bool IS_CONST_2>
        constexpr PairProvider(const PairProvider<IS_CONST_2>& m) noexcept
            requires(IS_CONST and !IS_CONST_2)
          : PairProvider{m.values_}
        {
        }

        constexpr void update_to_index(const std::size_t i) noexcept { current_index_ = i; }

        constexpr const_reference get() const noexcept
            requires IS_CONST
        {
            return {ENUM_VALUES[current_index_], (*values_)[current_index_].get()};
        }
        constexpr reference get() const noexcept
            requires(not IS_CONST)
        {
            return {ENUM_VALUES[current_index_], (*values_)[current_index_].get()};
        }
    };

    struct IndexPredicate
    {
        const std::array<bool, ENUM_COUNT>* array_set_;
        constexpr bool operator()(const std::size_t i) const { return (*array_set_)[i]; }
    };

    template <IteratorConstness CONSTNESS, IteratorDirection DIRECTION>
    using IteratorImpl = IndexRangePredicateIterator<IndexPredicate,
                                                     PairProvider<true>,
                                                     PairProvider<false>,
                                                     CONSTNESS,
                                                     DIRECTION>;

public:
    using const_iterator =
        IteratorImpl<IteratorConstness::CONSTANT_ITERATOR, IteratorDirection::FORWARD>;
    using iterator = IteratorImpl<IteratorConstness::MUTABLE_ITERATOR, IteratorDirection::FORWARD>;
    using const_reverse_iterator =
        IteratorImpl<IteratorConstness::CONSTANT_ITERATOR, IteratorDirection::REVERSE>;
    using reverse_iterator =
        IteratorImpl<IteratorConstness::MUTABLE_ITERATOR, IteratorDirection::REVERSE>;
    using size_type = typename KeyArrayType::size_type;
    using difference_type = typename KeyArrayType::difference_type;

public:
    template <class Container, class EnumMapType>
    static constexpr EnumMapType create_with_keys(const Container& sp, const V& value)
    {
        EnumMapType output{};
        for (auto&& k : sp)
        {
            output[k] = value;
        }
        return output;
    }

    template <class EnumMapType>
    static constexpr EnumMapType create_with_all_entries(std::initializer_list<value_type> pairs,
                                                         const std_transition::source_location& loc)
    {
        EnumMapType output{};
        for (const auto& pair : pairs)
        {
            auto [_, was_inserted] = output.insert(pair);
            if (preconditions::test(was_inserted))
            {
                Checking::duplicate_enum_entries(loc);
            }
        }

        if (preconditions::test(output.size() == ENUM_VALUES.size()))
        {
            Checking::missing_enum_entries(loc);
        }

        return output;
    }

    static constexpr std::size_t max_size() noexcept { return ENUM_COUNT; }

public:  // Public so this type is a structural type and can thus be used in template parameters
    ValueArrayType IMPLEMENTATION_DETAIL_DO_NOT_USE_values_;
    std::array<bool, ENUM_COUNT> IMPLEMENTATION_DETAIL_DO_NOT_USE_array_set_;
    std::size_t IMPLEMENTATION_DETAIL_DO_NOT_USE_size_;

public:
    constexpr EnumMapBase() noexcept
      : IMPLEMENTATION_DETAIL_DO_NOT_USE_values_{}
      , IMPLEMENTATION_DETAIL_DO_NOT_USE_array_set_{}
      , IMPLEMENTATION_DETAIL_DO_NOT_USE_size_{}
    {
    }

    template <InputIterator InputIt>
    constexpr EnumMapBase(InputIt first, InputIt last)
      : EnumMapBase()
    {
        insert(first, last);
    }

    constexpr EnumMapBase(std::initializer_list<value_type> list) noexcept
      : EnumMapBase()
    {
        this->insert(list);
    }

public:
    [[nodiscard]] constexpr V& at(const K& key,
                                  const std_transition::source_location& loc =
                                      std_transition::source_location::current()) noexcept
    {
        const std::size_t ordinal = EnumAdapterType::ordinal(key);
        if (preconditions::test(array_set_unchecked_at(ordinal)))
        {
            CheckingType::out_of_range(key, size(), loc);
        }
        return unchecked_at(ordinal);
    }
    [[nodiscard]] constexpr const V& at(
        const K& key,
        const std_transition::source_location& loc =
            std_transition::source_location::current()) const noexcept
    {
        const std::size_t ordinal = EnumAdapterType::ordinal(key);
        if (preconditions::test(array_set_unchecked_at(ordinal)))
        {
            CheckingType::out_of_range(key, size(), loc);
        }
        return unchecked_at(ordinal);
    }
    constexpr V& operator[](const K& key) noexcept
    {
        const std::size_t ordinal = EnumAdapterType::ordinal(key);
        touch_if_not_present(ordinal);
        return unchecked_at(ordinal);
    }
    constexpr V& operator[](K&& key) noexcept
    {
        const std::size_t ordinal = EnumAdapterType::ordinal(key);
        touch_if_not_present(ordinal);
        return unchecked_at(ordinal);
    }

    constexpr const_iterator cbegin() const noexcept { return create_const_iterator(0); }
    constexpr const_iterator cend() const noexcept { return create_const_iterator(ENUM_COUNT); }
    constexpr const_iterator begin() const noexcept { return cbegin(); }
    constexpr iterator begin() noexcept { return create_iterator(0); }
    constexpr const_iterator end() const noexcept { return cend(); }
    constexpr iterator end() noexcept { return create_iterator(ENUM_COUNT); }

    constexpr reverse_iterator rbegin() noexcept { return create_reverse_iterator(ENUM_COUNT); }
    constexpr const_reverse_iterator rbegin() const noexcept { return crbegin(); }
    constexpr const_reverse_iterator crbegin() const noexcept
    {
        return create_const_reverse_iterator(ENUM_COUNT);
    }
    constexpr reverse_iterator rend() noexcept { return create_reverse_iterator(0); }
    constexpr const_reverse_iterator rend() const noexcept { return crend(); }
    constexpr const_reverse_iterator crend() const noexcept
    {
        return create_const_reverse_iterator(0);
    }

    [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_size_;
    }

    constexpr void clear() noexcept
    {
        const std::size_t sz = values().size();
        for (std::size_t i = 0; i < sz; i++)
        {
            if (array_set_unchecked_at(i))
            {
                reset_at(i);
            }
        }
    }
    constexpr std::pair<iterator, bool> insert(const value_type& value) noexcept
    {
        const std::size_t ordinal = EnumAdapterType::ordinal(value.first);
        if (array_set_unchecked_at(ordinal))
        {
            return {create_iterator(ordinal), false};
        }

        increment_size();
        array_set_unchecked_at(ordinal) = true;
        std::construct_at(&values_unchecked_at(ordinal), value.second);
        return {create_iterator(ordinal), true};
    }
    constexpr std::pair<iterator, bool> insert(value_type&& value) noexcept
    {
        const std::size_t ordinal = EnumAdapterType::ordinal(value.first);
        if (array_set_unchecked_at(ordinal))
        {
            return {create_iterator(ordinal), false};
        }

        increment_size();
        array_set_unchecked_at(ordinal) = true;
        std::construct_at(&values_unchecked_at(ordinal), std::move(value.second));
        return {create_iterator(ordinal), true};
    }

    template <InputIterator InputIt>
    constexpr void insert(InputIt first, InputIt last) noexcept
    {
        for (; first != last; std::advance(first, 1))
        {
            this->insert(*first);
        }
    }
    constexpr void insert(std::initializer_list<value_type> list) noexcept
    {
        this->insert(list.begin(), list.end());
    }

    template <class M>
    constexpr std::pair<iterator, bool> insert_or_assign(const K& key, M&& obj) noexcept
        requires std::is_assignable_v<mapped_type&, M&&>
    {
        const std::size_t ordinal = EnumAdapterType::ordinal(key);
        const bool is_insertion = !array_set_unchecked_at(ordinal);
        if (is_insertion)
        {
            increment_size();
            array_set_unchecked_at(ordinal) = true;
        }
        values_unchecked_at(ordinal) = OptionalV(std::forward<M>(obj));
        return {create_iterator(ordinal), is_insertion};
    }
    template <class M>
    constexpr iterator insert_or_assign(const_iterator /*hint*/, const K& key, M&& obj) noexcept
        requires std::is_assignable_v<mapped_type&, M&&>
    {
        return insert_or_assign(key, std::forward<M>(obj)).first;
    }

    template <class... Args>
    constexpr std::pair<iterator, bool> try_emplace(const K& key, Args&&... args) noexcept
    {
        const std::size_t ordinal = EnumAdapterType::ordinal(key);
        if (array_set_unchecked_at(ordinal))
        {
            return {create_iterator(ordinal), false};
        }

        increment_size();
        array_set_unchecked_at(ordinal) = true;
        std::construct_at(
            &values_unchecked_at(ordinal), std::in_place, std::forward<Args>(args)...);
        return {create_iterator(ordinal), true};
    }
    template <class... Args>
    constexpr std::pair<iterator, bool> try_emplace(const_iterator /*hint*/,
                                                    const K& key,
                                                    Args&&... args) noexcept
    {
        return try_emplace(key, std::forward<Args>(args)...);
    }

    template <class... Args>
    constexpr std::pair<iterator, bool> emplace(Args&&... args) noexcept
    {
        std::pair<K, V> as_pair{std::forward<Args>(args)...};
        return try_emplace(as_pair.first, std::move(as_pair.second));
    }
    template <class... Args>
    constexpr std::pair<iterator, bool> emplace_hint(const_iterator /*hint*/,
                                                     Args&&... args) noexcept
    {
        return emplace(std::forward<Args>(args)...);
    }

    constexpr iterator erase(const_iterator pos) noexcept
    {
        assert(pos != cend());
        const std::size_t i = EnumAdapterType::ordinal(pos->first);
        assert(contains_at(i));
        reset_at(i);
        return create_iterator(i);
    }
    constexpr iterator erase(iterator pos) noexcept
    {
        assert(pos != end());
        const std::size_t i = EnumAdapterType::ordinal(pos->first);
        assert(contains_at(i));
        reset_at(i);
        return create_iterator(i);
    }

    constexpr iterator erase(const_iterator first, const_iterator last) noexcept
    {
        const std::size_t from =
            first == cend() ? ENUM_COUNT : EnumAdapterType::ordinal(first->first);
        const std::size_t to = last == cend() ? ENUM_COUNT : EnumAdapterType::ordinal(last->first);
        assert(from <= to);

        for (std::size_t i = from; i < to; i++)
        {
            if (contains_at(i))
            {
                reset_at(i);
            }
        }

        return create_iterator(to);
    }

    constexpr size_type erase(const K& key) noexcept
    {
        const std::size_t i = EnumAdapterType::ordinal(key);
        if (!contains_at(i))
        {
            return 0;
        }

        reset_at(i);
        return 1;
    }

    [[nodiscard]] constexpr iterator find(const K& key) noexcept
    {
        const std::size_t ordinal = EnumAdapterType::ordinal(key);
        if (!this->contains_at(ordinal))
        {
            return this->end();
        }

        return create_iterator(ordinal);
    }

    [[nodiscard]] constexpr const_iterator find(const K& key) const noexcept
    {
        const std::size_t ordinal = EnumAdapterType::ordinal(key);
        if (!this->contains_at(ordinal))
        {
            return this->cend();
        }

        return create_const_iterator(ordinal);
    }

    [[nodiscard]] constexpr bool contains(const K& key) const noexcept
    {
        return contains_at(EnumAdapterType::ordinal(key));
    }

    [[nodiscard]] constexpr std::size_t count(const K& key) const noexcept
    {
        return static_cast<std::size_t>(contains(key));
    }

    template <enum_map_customize::EnumMapChecking<K> CheckingType2>
    [[nodiscard]] constexpr bool operator==(const EnumMapBase<K, V, CheckingType2>& other) const
    {
        for (std::size_t i = 0; i < ENUM_COUNT; i++)
        {
            if (this->array_set_unchecked_at(i) != other.array_set_unchecked_at(i))
            {
                return false;
            }

            if (!this->array_set_unchecked_at(i))
            {
                continue;
            }

            if (this->unchecked_at(i) != other.unchecked_at(i))
            {
                return false;
            }
        }

        return true;
    }

private:
    constexpr void touch_if_not_present(const std::size_t ordinal) noexcept
    {
        if (contains_at(ordinal))
        {
            return;
        }

        increment_size();
        array_set_unchecked_at(ordinal) = true;
        std::construct_at(&values_unchecked_at(ordinal), std::in_place);
    }

    constexpr iterator create_iterator(const std::size_t start_index) noexcept
    {
        return iterator{
            IndexPredicate{&array_set()}, PairProvider<false>{&values()}, start_index, ENUM_COUNT};
    }

    constexpr const_iterator create_const_iterator(const std::size_t start_index) const noexcept
    {
        return const_iterator{
            IndexPredicate{&array_set()}, PairProvider<true>{&values()}, start_index, ENUM_COUNT};
    }

    constexpr reverse_iterator create_reverse_iterator(const std::size_t start_index) noexcept
    {
        return reverse_iterator{
            IndexPredicate{&array_set()}, PairProvider<false>{&values()}, start_index, ENUM_COUNT};
    }

    constexpr const_reverse_iterator create_const_reverse_iterator(
        const std::size_t start_index) const noexcept
    {
        return const_reverse_iterator{
            IndexPredicate{&array_set()}, PairProvider<true>{&values()}, start_index, ENUM_COUNT};
    }

    constexpr void reset_at(const std::size_t i) noexcept
    {
        assert(contains_at(i));
        if constexpr (NotTriviallyDestructible<V>)  // if-check needed by clang
        {
            std::destroy_at(&unchecked_at(i));
        }
        array_set_unchecked_at(i) = false;
        decrement_size();
    }

protected:  // [WORKAROUND-1]
    constexpr const std::array<bool, ENUM_COUNT>& array_set() const
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_array_set_;
    }
    constexpr std::array<bool, ENUM_COUNT>& array_set()
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_array_set_;
    }
    constexpr const bool& array_set_unchecked_at(const std::size_t i) const
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_array_set_[i];
    }
    constexpr bool& array_set_unchecked_at(const std::size_t i)
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_array_set_[i];
    }

    constexpr const ValueArrayType& values() const
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_values_;
    }
    constexpr ValueArrayType& values() { return IMPLEMENTATION_DETAIL_DO_NOT_USE_values_; }
    constexpr const OptionalV& values_unchecked_at(const std::size_t i) const
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_values_[i];
    }
    constexpr OptionalV& values_unchecked_at(const std::size_t i)
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_values_[i];
    }
    constexpr const V& unchecked_at(const std::size_t i) const
    {
        return optional_storage_detail::get(IMPLEMENTATION_DETAIL_DO_NOT_USE_values_[i]);
    }
    constexpr V& unchecked_at(const std::size_t i)
    {
        return optional_storage_detail::get(IMPLEMENTATION_DETAIL_DO_NOT_USE_values_[i]);
    }
    [[nodiscard]] constexpr bool contains_at(const std::size_t i) const noexcept
    {
        return array_set_unchecked_at(i);
    }

    constexpr void increment_size(const std::size_t n = 1)
    {
        IMPLEMENTATION_DETAIL_DO_NOT_USE_size_ += n;
    }
    constexpr void decrement_size(const std::size_t n = 1)
    {
        IMPLEMENTATION_DETAIL_DO_NOT_USE_size_ -= n;
    }
    constexpr void set_size(const std::size_t size)
    {
        IMPLEMENTATION_DETAIL_DO_NOT_USE_size_ = size;
    }
};
}  // namespace fixed_containers::enum_map_detail

namespace fixed_containers::enum_map_detail::specializations
{
template <class K, class V, enum_map_customize::EnumMapChecking<K> CheckingType>
class EnumMap : public enum_map_detail::EnumMapBase<K, V, CheckingType>
{
    using Self = EnumMap<K, V, CheckingType>;
    using Base = enum_map_detail::EnumMapBase<K, V, CheckingType>;

public:
    using key_type = typename Base::key_type;
    using mapped_type = typename Base::mapped_type;
    using value_type = typename Base::value_type;
    using reference = typename Base::reference;
    using const_reference = typename Base::const_reference;
    using pointer = typename Base::pointer;
    using const_pointer = typename Base::const_pointer;
    using const_iterator = typename Base::const_iterator;
    using iterator = typename Base::iterator;
    using const_reverse_iterator = typename Base::const_reverse_iterator;
    using reverse_iterator = typename Base::reverse_iterator;
    using size_type = typename Base::size_type;
    using difference_type = typename Base::difference_type;

public:
    using Builder = enum_map_detail::EnumMapBuilder<K, V, EnumMap<K, V, CheckingType>>;

    template <class Container, class EnumMapType = Self>
    static constexpr EnumMapType create_with_keys(const Container& sp, const V& value = V())
    {
        return Base::template create_with_keys<Container, EnumMapType>(sp, value);
    }

    template <class EnumMapType = Self>
    static constexpr EnumMapType create_with_all_entries(
        std::initializer_list<value_type> pairs,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        return Base::template create_with_all_entries<EnumMapType>(pairs, loc);
    }

    constexpr EnumMap() noexcept
      : Base()
    {
    }
    template <InputIterator InputIt>
    constexpr EnumMap(InputIt first, InputIt last)
      : Base(first, last)
    {
    }
    constexpr EnumMap(std::initializer_list<value_type> list) noexcept
      : Base(list)
    {
    }

    constexpr EnumMap(const EnumMap& other)
        requires TriviallyCopyConstructible<V>
    = default;
    constexpr EnumMap(EnumMap&& other) noexcept
        requires TriviallyMoveConstructible<V>
    = default;
    constexpr EnumMap& operator=(const EnumMap& other)
        requires TriviallyCopyAssignable<V>
    = default;
    constexpr EnumMap& operator=(EnumMap&& other) noexcept
        requires TriviallyMoveAssignable<V>
    = default;

    constexpr EnumMap(const EnumMap& other)
      : EnumMap()
    {
        this->array_set() = other.array_set();
        this->set_size(other.size());
        for (std::size_t i = 0; i < Base::ENUM_COUNT; i++)
        {
            if (this->contains_at(i))
            {
                std::construct_at(&this->values_unchecked_at(i), other.values_unchecked_at(i));
            }
        }
    }
    constexpr EnumMap(EnumMap&& other) noexcept
      : EnumMap()
    {
        this->array_set() = other.array_set();
        this->set_size(other.size());
        for (std::size_t i = 0; i < Base::ENUM_COUNT; i++)
        {
            if (this->contains_at(i))
            {
                std::construct_at(&this->values_unchecked_at(i),
                                  std::move(other.values_unchecked_at(i)));
            }
        }
        // Clear the moved-out-of-map. This is consistent with both std::map
        // as well as the trivial move constructor of this class.
        other.clear();
    }
    constexpr EnumMap& operator=(const EnumMap& other)
    {
        if (this == &other)
        {
            return *this;
        }

        this->clear();
        this->array_set() = other.array_set();
        this->set_size(other.size());
        for (std::size_t i = 0; i < Base::ENUM_COUNT; i++)
        {
            if (this->contains_at(i))
            {
                std::construct_at(&this->values_unchecked_at(i), other.values_unchecked_at(i));
            }
        }
        return *this;
    }
    constexpr EnumMap& operator=(EnumMap&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        this->clear();
        this->array_set() = other.array_set();
        this->set_size(other.size());
        for (std::size_t i = 0; i < Base::ENUM_COUNT; i++)
        {
            if (this->contains_at(i))
            {
                std::construct_at(&this->values_unchecked_at(i),
                                  std::move(other.values_unchecked_at(i)));
            }
        }
        // The trivial assignment operator does not `other.clear()`, so don't do it here either for
        // consistency across EnumMaps. std::map<T> does clear it, so behavior is different.
        // Both choices are fine, because the state of a moved object is intentionally unspecified
        // as per the standard and use-after-move is undefined behavior.
        return *this;
    }

    constexpr ~EnumMap() noexcept { this->clear(); }
};

template <class K, TriviallyCopyable V, enum_map_customize::EnumMapChecking<K> CheckingType>
class EnumMap<K, V, CheckingType> : public enum_map_detail::EnumMapBase<K, V, CheckingType>
{
    using Self = EnumMap<K, V, CheckingType>;
    using Base = enum_map_detail::EnumMapBase<K, V, CheckingType>;

public:
    using key_type = typename Base::key_type;
    using mapped_type = typename Base::mapped_type;
    using value_type = typename Base::value_type;
    using reference = typename Base::reference;
    using const_reference = typename Base::const_reference;
    using pointer = typename Base::pointer;
    using const_pointer = typename Base::const_pointer;
    using const_iterator = typename Base::const_iterator;
    using iterator = typename Base::iterator;
    using const_reverse_iterator = typename Base::const_reverse_iterator;
    using reverse_iterator = typename Base::reverse_iterator;
    using size_type = typename Base::size_type;
    using difference_type = typename Base::difference_type;

public:
    using Builder = enum_map_detail::EnumMapBuilder<K, V, Self>;

    template <class Container, class EnumMapType = Self>
    static constexpr EnumMapType create_with_keys(const Container& sp, const V& value = V())
    {
        return Base::template create_with_keys<Container, EnumMapType>(sp, value);
    }

    template <class EnumMapType = Self>
    static constexpr EnumMapType create_with_all_entries(
        std::initializer_list<value_type> pairs,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        return Base::template create_with_all_entries<EnumMapType>(pairs, loc);
    }

    constexpr EnumMap() noexcept
      : Base()
    {
    }
    template <InputIterator InputIt>
    constexpr EnumMap(InputIt first, InputIt last)
      : Base(first, last)
    {
    }
    constexpr EnumMap(std::initializer_list<value_type> list) noexcept
      : Base(list)
    {
    }
};
}  // namespace fixed_containers::enum_map_detail::specializations

namespace fixed_containers
{
/**
 * Fixed-capacity map for enum keys. Properties:
 *  - constexpr
 *  - retains the properties of V (e.g. if T is trivially copyable, then so is EnumMapBase<K, V>)
 *  - no pointers stored (data layout is purely self-referential and can be serialized directly)
 *  - no dynamic allocations
 */
template <class K,
          class V,
          enum_map_customize::EnumMapChecking<K> CheckingType =
              enum_map_customize::AbortChecking<K, V>>
using EnumMap = enum_map_detail::specializations::EnumMap<K, V, CheckingType>;

template <class K, class V, enum_map_customize::EnumMapChecking<K> CheckingType, class Predicate>
constexpr typename EnumMap<K, V, CheckingType>::size_type erase_if(EnumMap<K, V, CheckingType>& c,
                                                                   Predicate predicate)
{
    return erase_if_detail::erase_if_impl(c, predicate);
}

}  // namespace fixed_containers
