#pragma once

#include "fixed_containers/algorithm.hpp"
#include "fixed_containers/concepts.hpp"
#include "fixed_containers/consteval_compare.hpp"
#include "fixed_containers/iterator_utils.hpp"
#include "fixed_containers/max_size.hpp"
#include "fixed_containers/optional_storage.hpp"
#include "fixed_containers/preconditions.hpp"
#include "fixed_containers/random_access_iterator_transformer.hpp"
#include "fixed_containers/sequence_container_checking.hpp"
#include "fixed_containers/source_location.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <type_traits>

namespace fixed_containers::fixed_vector_detail
{
template <class T, class FixedVectorType>
class FixedVectorBuilder
{
public:
    constexpr FixedVectorBuilder() {}

    constexpr FixedVectorBuilder& push_back(const T& key) & noexcept
    {
        vector_.push_back(key);
        return *this;
    }
    constexpr FixedVectorBuilder&& push_back(const T& key) && noexcept
    {
        return std::move(push_back(key));
    }

    constexpr FixedVectorBuilder& push_back_all(std::initializer_list<T> list) & noexcept
    {
        push_back_all(list.begin(), list.end());
        return *this;
    }
    constexpr FixedVectorBuilder&& push_back_all(std::initializer_list<T> list) && noexcept
    {
        return std::move(push_back_all(list));
    }

    template <InputIterator InputIt>
    constexpr FixedVectorBuilder& push_back_all(InputIt first, InputIt last) & noexcept
    {
        vector_.insert(vector_.end(), first, last);
        return *this;
    }
    template <InputIterator InputIt>
    constexpr FixedVectorBuilder&& push_back_all(InputIt first, InputIt last) && noexcept
    {
        return std::move(push_back_all(first, last));
    }

    template <class Container>
    constexpr FixedVectorBuilder& push_back_all(const Container& container) & noexcept
    {
        push_back_all(container.cbegin(), container.cend());
        return *this;
    }
    template <class Container>
    constexpr FixedVectorBuilder&& push_back_all(const Container& container) && noexcept
    {
        return std::move(push_back_all(container.cbegin(), container.cend()));
    }

    constexpr FixedVectorType build() const& { return vector_; }
    constexpr FixedVectorType&& build() && { return std::move(vector_); }

private:
    FixedVectorType vector_;
};

// FixedVector<T> should carry the properties of T. For example, if T fulfils
// std::is_trivially_copy_assignable<T>, then so should FixedVector<T>.
// This is done with concepts. However, at the time of writing there is a compiler bug
// that is preventing usage of concepts for destructors: https://bugs.llvm.org/show_bug.cgi?id=46269
// [WORKAROUND-1] due to destructors: manually do the split with template specialization.
// FixedVectorBase is only used for avoiding too much duplication for the destructor split
template <typename T, std::size_t MAXIMUM_SIZE, customize::SequenceContainerChecking CheckingType>
class FixedVectorBase
{
    /*
     * Use OptionalStorageTransparent, to properly support constexpr .data() for simple types.
     *
     * In order to operate on a pointer at compile-time (e.g. increment), it needs to be a
     * consecutive block of T's. If we use the OptionalStorage wrapper (non-transparent), we instead
     * have a consecutive block of OptionalStorage<T>. The compiler would figure that out at
     * compile time and reject it even though they have the same layout. In that case,
     * vector.data()[0] would be accessible at constexpr, but vector.data()[1] would be rejected.
     */
    using OptionalT = optional_storage_detail::OptionalStorageTransparent<T>;
    static_assert(consteval_compare::equal<sizeof(OptionalT), sizeof(T)>);
    // std::vector has the following restrictions too
    static_assert(IsNotReference<T>, "References are not allowed");
    static_assert(std::same_as<std::remove_cv_t<T>, T>,
                  "Vector must have a non-const, non-volatile value_type");
    using Checking = CheckingType;
    using Array = std::array<OptionalT, MAXIMUM_SIZE>;

    struct Mapper
    {
        constexpr T& operator()(OptionalT& opt_storage) const noexcept
        {
            return optional_storage_detail::get(opt_storage);
        }
        constexpr const T& operator()(const OptionalT& opt_storage) const noexcept
        // requires(not std::is_const_v<OptionalT>)
        {
            return optional_storage_detail::get(opt_storage);
        }
    };

    template <IteratorConstness CONSTNESS>
    using IteratorImpl = RandomAccessIteratorTransformer<typename Array::const_iterator,
                                                         typename Array::iterator,
                                                         Mapper,
                                                         Mapper,
                                                         CONSTNESS>;

public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using const_iterator = IteratorImpl<IteratorConstness::CONSTANT_ITERATOR>;
    using iterator = IteratorImpl<IteratorConstness::MUTABLE_ITERATOR>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    [[nodiscard]] static constexpr std::size_t static_max_size() noexcept { return MAXIMUM_SIZE; }

private:
    static constexpr void check_target_size(size_type target_size,
                                            const std_transition::source_location& loc)
    {
        if (preconditions::test(target_size <= MAXIMUM_SIZE))
        {
            Checking::length_error(target_size, loc);
        }
    }

public:  // Public so this type is a structural type and can thus be used in template parameters
    std::size_t IMPLEMENTATION_DETAIL_DO_NOT_USE_size_;
    Array IMPLEMENTATION_DETAIL_DO_NOT_USE_array_;

public:
    constexpr FixedVectorBase() noexcept
      : IMPLEMENTATION_DETAIL_DO_NOT_USE_size_{0}
      , IMPLEMENTATION_DETAIL_DO_NOT_USE_array_{}
    {
        // A constexpr context requires everything to be initialized.
        // The OptionalStorage wrapper takes care of that, but for unwrapped objects
        // while also being in a constexpr context, initialize array.
        if constexpr (!std::same_as<OptionalT, optional_storage_detail::OptionalStorage<T>>)
        {
            if (std::is_constant_evaluated())
            {
                std::construct_at(&array());
            }
        }
    }

    constexpr FixedVectorBase(std::size_t count,
                              const T& value,
                              const std_transition::source_location& loc =
                                  std_transition::source_location::current()) noexcept
      : FixedVectorBase()
    {
        check_target_size(count, loc);
        set_size(count);
        for (std::size_t i = 0; i < count; i++)
        {
            place_at(i, value);
        }
    }

    constexpr explicit FixedVectorBase(std::size_t count,
                                       const std_transition::source_location& loc =
                                           std_transition::source_location::current()) noexcept
      : FixedVectorBase(count, T(), loc)
    {
    }

    template <InputIterator InputIt>
    constexpr FixedVectorBase(InputIt first,
                              InputIt last,
                              const std_transition::source_location& loc =
                                  std_transition::source_location::current()) noexcept
      : FixedVectorBase()
    {
        insert(cend(), first, last, loc);
    }

    constexpr FixedVectorBase(std::initializer_list<T> list,
                              const std_transition::source_location& loc =
                                  std_transition::source_location::current()) noexcept
      : FixedVectorBase(list.begin(), list.end(), loc)
    {
    }

    /**
     * Resizes the container to contain `count` elements.
     * If the current size is greater than count, the container is reduced to its first count
     * elements. If the current size is less than count, additional elements are appended
     * (default/copy initialized).
     */
    constexpr void resize(
        size_type count,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        this->resize(count, T{}, loc);
    }
    constexpr void resize(
        size_type count,
        const value_type& v,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        check_target_size(count, loc);

        // Reinitialize the new members if we are enlarging
        while (size() < count)
        {
            place_at(end_index(), v);
            increment_size();
        }
        // Destroy extras if we are making it smaller.
        while (size() > count)
        {
            destroy_at(back_index());
            decrement_size();
        }
    }

    /**
     * Appends the given element value to the end of the container.
     * Calling push_back on a full container is undefined.
     */
    constexpr void push_back(
        const value_type& v,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        check_not_full(loc);
        this->push_back_internal(v);
    }
    constexpr void push_back(
        value_type&& v,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        check_not_full(loc);
        this->push_back_internal(std::move(v));
    }
    /**
     * Emplace the given element at the end of the container.
     * Calling emplace_back on a full container is undefined.
     */
    template <class... Args>
    constexpr reference emplace_back(Args&&... args)
    {
        check_not_full(std_transition::source_location::current());
        emplace_at(end_index(), std::forward<Args>(args)...);
        increment_size();
        return this->back();
    }

    /**
     * Removes the last element of the container.
     * Calling pop_back on an empty container is undefined.
     */
    constexpr void pop_back(
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        check_not_empty(loc);
        destroy_at(back_index());
        decrement_size();
    }

    /**
     * Inserts elements at the iterator-specified location in the container.
     * Calling insert on a full container is undefined.
     */
    constexpr iterator insert(
        const_iterator it,
        const value_type& v,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        check_not_full(loc);
        auto entry_it = advance_all_after_iterator_by_n(it, 1);
        std::construct_at(&*entry_it, v);
        return entry_it;
    }
    constexpr iterator insert(
        const_iterator it,
        value_type&& v,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        check_not_full(loc);
        auto entry_it = advance_all_after_iterator_by_n(it, 1);
        std::construct_at(&*entry_it, std::move(v));
        return entry_it;
    }
    template <InputIterator InputIt>
    constexpr iterator insert(
        const_iterator it,
        InputIt first,
        InputIt last,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        return insert_internal(
            typename std::iterator_traits<InputIt>::iterator_category{}, it, first, last, loc);
    }
    constexpr iterator insert(
        const_iterator it,
        std::initializer_list<T> ilist,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        return insert_internal(
            std::random_access_iterator_tag{}, it, ilist.begin(), ilist.end(), loc);
    }

    /**
     * Emplace element at the iterator-specified location in the container.
     * Calling emplace on a full container is undefined.
     */
    template <class... Args>
    constexpr iterator emplace(const_iterator it, Args&&... args)
    {
        check_not_full(std_transition::source_location::current());
        auto entry_it = advance_all_after_iterator_by_n(it, 1);
        std::construct_at(&*entry_it, std::forward<Args>(args)...);
        return entry_it;
    }

    /**
     * Replaces the contents with count copies of a given value
     */
    constexpr void assign(
        size_type count,
        const value_type& v,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        check_target_size(count, loc);
        this->clear();
        this->resize(count, v);
    }

    /**
     * Replaces the contents with copies of those in range [first, last)
     */
    template <InputIterator InputIt>
    constexpr void assign(
        InputIt first,
        InputIt last,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        this->clear();
        this->insert(cend(), first, last, loc);
    }

    constexpr void assign(
        std::initializer_list<T> ilist,
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        this->clear();
        this->insert(cend(), ilist, loc);
    }

    /**
     * Erases the specified range of elements from the container.
     */
    constexpr iterator erase(const_iterator first,
                             const_iterator last,
                             const std_transition::source_location& loc =
                                 std_transition::source_location::current()) noexcept
    {
        if (preconditions::test(first <= last))
        {
            Checking::invalid_argument("first > last, range is invalid", loc);
        }
        if (preconditions::test(first >= cbegin() && last <= cend()))
        {
            Checking::invalid_argument("iterators exceed container range", loc);
        }

        const auto entry_count_to_move = std::distance(last, cend());
        const auto entry_count_to_remove = std::distance(first, last);

        iterator read_start_it = const_to_mutable_it(last);
        iterator read_end_it = std::next(read_start_it, entry_count_to_move);
        iterator write_start_it = const_to_mutable_it(first);

        // Clean out the gap
        destroy_range(write_start_it, std::next(write_start_it, entry_count_to_remove));

        // Do the move
        std::move(read_start_it, read_end_it, write_start_it);

        decrement_size(static_cast<std::size_t>(entry_count_to_remove));
        return write_start_it;
    }

    /**
     * Erases the specified element from the container.
     */
    constexpr iterator erase(const_iterator it,
                             const std_transition::source_location& loc =
                                 std_transition::source_location::current()) noexcept
    {
        return erase(it, std::next(it), loc);
    }

    /**
     * Erases all elements from the container. After this call, size() returns zero.
     */
    constexpr void clear() noexcept
    {
        destroy_range(begin(), end());
        set_size(0);
    }

    /**
     * Regular accessors.
     */
    constexpr reference operator[](size_type i) noexcept
    {
        // Cannot capture real source_location for operator[]
        // This operator should not range-check according to the spec, but we want the extra safety.
        return at(i, std_transition::source_location::current());
    }
    constexpr const_reference operator[](size_type i) const noexcept
    {
        // Cannot capture real source_location for operator[]
        // This operator should not range-check according to the spec, but we want the extra safety.
        return at(i, std_transition::source_location::current());
    }

    constexpr reference at(size_type i,
                           const std_transition::source_location& loc =
                               std_transition::source_location::current()) noexcept
    {
        if (preconditions::test(i < size()))
        {
            Checking::out_of_range(i, size(), loc);
        }
        return unchecked_at(i);
    }
    constexpr const_reference at(size_type i,
                                 const std_transition::source_location& loc =
                                     std_transition::source_location::current()) const noexcept
    {
        if (preconditions::test(i < size()))
        {
            Checking::out_of_range(i, size(), loc);
        }
        return unchecked_at(i);
    }

    constexpr reference front(
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        check_not_empty(loc);
        return unchecked_at(front_index());
    }
    constexpr const_reference front(const std_transition::source_location& loc =
                                        std_transition::source_location::current()) const
    {
        check_not_empty(loc);
        return unchecked_at(front_index());
    }
    constexpr reference back(
        const std_transition::source_location& loc = std_transition::source_location::current())
    {
        check_not_empty(loc);
        return unchecked_at(back_index());
    }
    constexpr const_reference back(const std_transition::source_location& loc =
                                       std_transition::source_location::current()) const
    {
        check_not_empty(loc);
        return unchecked_at(back_index());
    }

    constexpr value_type* data() noexcept { return &optional_storage_detail::get(*array().data()); }
    constexpr const value_type* data() const noexcept
    {
        return &optional_storage_detail::get(*array().data());
    }

    /**
     * Iterators
     */
    constexpr iterator begin() noexcept { return create_iterator(front_index()); }
    constexpr const_iterator begin() const noexcept { return cbegin(); }
    constexpr const_iterator cbegin() const noexcept
    {
        return create_const_iterator(front_index());
    }
    constexpr iterator end() noexcept { return create_iterator(end_index()); }
    constexpr const_iterator end() const noexcept { return cend(); }
    constexpr const_iterator cend() const noexcept { return create_const_iterator(end_index()); }

    constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    constexpr const_reverse_iterator rbegin() const noexcept { return crbegin(); }
    constexpr const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }
    constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    constexpr const_reverse_iterator rend() const noexcept { return crend(); }
    constexpr const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(cbegin());
    }

    /**
     * Size
     */
    [[nodiscard]] constexpr std::size_t max_size() const noexcept { return static_max_size(); }
    [[nodiscard]] constexpr std::size_t capacity() const noexcept { return max_size(); }
    constexpr void reserve(const std::size_t new_capacity,
                           const std_transition::source_location& loc =
                               std_transition::source_location::current()) noexcept
    {
        if (preconditions::test(new_capacity <= MAXIMUM_SIZE))
        {
            Checking::length_error(new_capacity, loc);
        }
        // Do nothing
    }
    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return IMPLEMENTATION_DETAIL_DO_NOT_USE_size_;
    }
    [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

    /**
     * Equality.
     */
    template <std::size_t MAXIMUM_SIZE_2, customize::SequenceContainerChecking CheckingType2>
    constexpr bool operator==(const FixedVectorBase<T, MAXIMUM_SIZE_2, CheckingType2>& other) const
    {
        if constexpr (MAXIMUM_SIZE == MAXIMUM_SIZE_2)
        {
            if (this == &other)
            {
                return true;
            }
        }

        return std::ranges::equal(*this, other);
    }

    template <std::size_t MAXIMUM_SIZE_2, customize::SequenceContainerChecking CheckingType2>
    constexpr auto operator<=>(const FixedVectorBase<T, MAXIMUM_SIZE_2, CheckingType2>& other) const
    {
        return std::lexicographical_compare_three_way(
            cbegin(), cend(), other.cbegin(), other.cend());
    }

private:
    constexpr iterator advance_all_after_iterator_by_n(const const_iterator it, const std::size_t n)
    {
        const std::ptrdiff_t value_count_to_move = std::distance(it, cend());
        increment_size(n);  // Increment now so iterators are all within valid range

        auto read_start_it = const_to_mutable_it(it);
        auto read_end_it = std::next(read_start_it, value_count_to_move);
        auto write_end_it =
            std::next(read_start_it, static_cast<std::ptrdiff_t>(n) + value_count_to_move);
        algorithm::emplace_move_backward(read_start_it, read_end_it, write_end_it);

        return read_start_it;
    }

    template <InputIterator InputIt>
    constexpr iterator insert_internal(std::forward_iterator_tag,
                                       const_iterator it,
                                       InputIt first,
                                       InputIt last,
                                       const std_transition::source_location& loc)
    {
        const auto entry_count_to_add = static_cast<std::size_t>(std::distance(first, last));
        check_target_size(size() + entry_count_to_add, loc);

        auto write_it = advance_all_after_iterator_by_n(it, entry_count_to_add);
        for (auto w_it = write_it; first != last; std::advance(first, 1), std::advance(w_it, 1))
        {
            std::construct_at(&*w_it, *first);
        }
        return write_it;
    }

    template <InputIterator InputIt>
    constexpr iterator insert_internal(std::input_iterator_tag,
                                       const_iterator it,
                                       InputIt first,
                                       InputIt last,
                                       const std_transition::source_location& loc)
    {
        auto first_it = const_to_mutable_it(it);
        auto middle_it = end();

        // Place everything at the end
        for (; first != last && size() < max_size(); ++first)
        {
            push_back_internal(*first);
        }

        if (first != last)  // Reached capacity
        {
            std::size_t excess_element_count = 0;
            for (; first != last; ++first)
            {
                excess_element_count++;
            }

            Checking::length_error(MAXIMUM_SIZE + excess_element_count, loc);
        }

        // Rotate into the correct places
        std::rotate(first_it, middle_it, end());

        return first_it;
    }

    constexpr iterator create_iterator(const std::size_t offset_from_start) noexcept
    {
        auto array_it =
            std::next(std::begin(array()), static_cast<difference_type>(offset_from_start));
        return iterator{array_it, Mapper{}};
    }

    constexpr const_iterator create_const_iterator(
        const std::size_t offset_from_start) const noexcept
    {
        auto array_it =
            std::next(std::begin(array()), static_cast<difference_type>(offset_from_start));
        return const_iterator{array_it, Mapper{}};
    }

private:
    constexpr iterator const_to_mutable_it(const_iterator it)
    {
        return std::next(begin(), std::distance(cbegin(), it));
    }

    constexpr void check_not_full(const std_transition::source_location& loc) const
    {
        if (preconditions::test(size() < MAXIMUM_SIZE))
        {
            Checking::length_error(MAXIMUM_SIZE + 1, loc);
        }
    }
    constexpr void check_not_empty(const std_transition::source_location& loc) const
    {
        if (preconditions::test(!empty()))
        {
            Checking::empty_container_access(loc);
        }
    }

    [[nodiscard]] constexpr std::size_t front_index() const { return 0; }
    [[nodiscard]] constexpr std::size_t back_index() const { return end_index() - 1; }
    [[nodiscard]] constexpr std::size_t end_index() const { return size(); }

    constexpr const Array& array() const { return IMPLEMENTATION_DETAIL_DO_NOT_USE_array_; }
    constexpr Array& array() { return IMPLEMENTATION_DETAIL_DO_NOT_USE_array_; }

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
    constexpr const OptionalT& array_unchecked_at(const std::size_t i) const { return array()[i]; }
    constexpr OptionalT& array_unchecked_at(const std::size_t i) { return array()[i]; }
    constexpr const T& unchecked_at(const std::size_t i) const
    {
        return optional_storage_detail::get(array_unchecked_at(i));
    }
    constexpr T& unchecked_at(const std::size_t i)
    {
        return optional_storage_detail::get(array_unchecked_at(i));
    }

    constexpr void destroy_at(std::size_t)
        requires TriviallyDestructible<T>
    {
    }
    constexpr void destroy_at(std::size_t i)
        requires NotTriviallyDestructible<T>
    {
        std::destroy_at(&array_unchecked_at(i).value);
    }

    constexpr void destroy_range(iterator /*first*/, iterator /*last*/)
        requires TriviallyDestructible<T>
    {
    }
    constexpr void destroy_range(iterator first, iterator last)
        requires NotTriviallyDestructible<T>
    {
        for (; first != last; ++first)
        {
            std::destroy_at(&*first);
        }
    }

    constexpr void place_at(const std::size_t i, const value_type& v)
    {
        std::construct_at(&array_unchecked_at(i), v);
    }
    constexpr void place_at(const std::size_t i, value_type&& v)
    {
        std::construct_at(&array_unchecked_at(i), std::move(v));
    }

    template <class... Args>
    constexpr void emplace_at(const std::size_t i, Args&&... args)
    {
        optional_storage_detail::construct_at(&array_unchecked_at(i), std::forward<Args>(args)...);
    }

    // [WORKAROUND-1] - Needed by the non-trivially-copyable flavor of FixedVector
protected:
    constexpr void push_back_internal(const value_type& v)
    {
        place_at(end_index(), v);
        increment_size();
    }

    constexpr void push_back_internal(value_type&& v)
    {
        place_at(end_index(), std::move(v));
        increment_size();
    }
};

}  // namespace fixed_containers::fixed_vector_detail

namespace fixed_containers::fixed_vector_detail::specializations
{
template <typename T, std::size_t MAXIMUM_SIZE, customize::SequenceContainerChecking CheckingType>
class FixedVector : public fixed_vector_detail::FixedVectorBase<T, MAXIMUM_SIZE, CheckingType>
{
    using Base = fixed_vector_detail::FixedVectorBase<T, MAXIMUM_SIZE, CheckingType>;

public:
    using Builder =
        fixed_vector_detail::FixedVectorBuilder<T, FixedVector<T, MAXIMUM_SIZE, CheckingType>>;

    constexpr FixedVector() noexcept
      : Base()
    {
    }
    constexpr FixedVector(std::size_t count,
                          const T& value,
                          const std_transition::source_location& loc =
                              std_transition::source_location::current()) noexcept
      : Base(count, value, loc)
    {
    }
    constexpr explicit FixedVector(std::size_t count,
                                   const std_transition::source_location& loc =
                                       std_transition::source_location::current()) noexcept
      : Base(count, loc)
    {
    }
    template <InputIterator InputIt>
    constexpr FixedVector(InputIt first,
                          InputIt last,
                          const std_transition::source_location& loc =
                              std_transition::source_location::current()) noexcept
      : Base(first, last, loc)
    {
    }
    constexpr FixedVector(std::initializer_list<T> list,
                          const std_transition::source_location& loc =
                              std_transition::source_location::current()) noexcept
      : Base(list, loc)
    {
    }

    constexpr FixedVector(const FixedVector& other)
        requires TriviallyCopyConstructible<T>
    = default;
    constexpr FixedVector(FixedVector&& other) noexcept
        requires TriviallyMoveConstructible<T>
    = default;
    constexpr FixedVector& operator=(const FixedVector& other)
        requires TriviallyCopyAssignable<T>
    = default;
    constexpr FixedVector& operator=(FixedVector&& other) noexcept
        requires TriviallyMoveAssignable<T>
    = default;

    constexpr FixedVector(const FixedVector& other)
      : FixedVector(other.begin(), other.end())
    {
    }
    constexpr FixedVector(FixedVector&& other) noexcept
      : FixedVector()
    {
        for (T& entry : other)
        {
            this->push_back_internal(std::move(entry));
        }

        // Clear the moved-out-of-vector. This is consistent with both std::vector
        // as well as the trivial move constructor of this class.
        other.clear();
    }
    constexpr FixedVector& operator=(const FixedVector& other)
    {
        if (this == &other)
        {
            return *this;
        }

        this->assign(other.begin(), other.end());
        return *this;
    }
    constexpr FixedVector& operator=(FixedVector&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        this->clear();
        for (T& entry : other)
        {
            this->push_back_internal(std::move(entry));
        }
        // The trivial assignment operator does not `other.clear()`, so don't do it here either for
        // consistency across FixedVectors. std::vector<T> does clear it, so behavior is different.
        // Both choices are fine, because the state of a moved object is intentionally unspecified
        // as per the standard and use-after-move is undefined behavior.
        return *this;
    }

    constexpr ~FixedVector() noexcept { this->clear(); }
};

template <TriviallyCopyable T,
          std::size_t MAXIMUM_SIZE,
          customize::SequenceContainerChecking CheckingType>
class FixedVector<T, MAXIMUM_SIZE, CheckingType>
  : public fixed_vector_detail::FixedVectorBase<T, MAXIMUM_SIZE, CheckingType>
{
    using Base = fixed_vector_detail::FixedVectorBase<T, MAXIMUM_SIZE, CheckingType>;

public:
    using Builder =
        fixed_vector_detail::FixedVectorBuilder<T, FixedVector<T, MAXIMUM_SIZE, CheckingType>>;

    constexpr FixedVector() noexcept
      : Base()
    {
    }
    constexpr FixedVector(std::size_t count,
                          const T& value,
                          const std_transition::source_location& loc =
                              std_transition::source_location::current()) noexcept
      : Base(count, value, loc)
    {
    }
    constexpr explicit FixedVector(std::size_t count,
                                   const std_transition::source_location& loc =
                                       std_transition::source_location::current()) noexcept
      : Base(count, loc)
    {
    }
    template <InputIterator InputIt>
    constexpr FixedVector(InputIt first,
                          InputIt last,
                          const std_transition::source_location& loc =
                              std_transition::source_location::current()) noexcept
      : Base(first, last, loc)
    {
    }
    constexpr FixedVector(std::initializer_list<T> list,
                          const std_transition::source_location& loc =
                              std_transition::source_location::current()) noexcept
      : Base(list, loc)
    {
    }
};

}  // namespace fixed_containers::fixed_vector_detail::specializations

namespace fixed_containers
{
/**
 * Fixed-capacity vector with maximum size that is declared at compile-time via
 * template parameter. Properties:
 *  - constexpr
 *  - retains the properties of T (e.g. if T is trivially copyable, then so is FixedVector<T>)
 *  - no pointers stored (data layout is purely self-referential and can be serialized directly)
 *  - no dynamic allocations
 */
template <typename T,
          std::size_t MAXIMUM_SIZE,
          customize::SequenceContainerChecking CheckingType =
              customize::SequenceContainerAbortChecking<T, MAXIMUM_SIZE>>
class FixedVector
  : public fixed_vector_detail::specializations::FixedVector<T, MAXIMUM_SIZE, CheckingType>
{
    using Base = fixed_vector_detail::specializations::FixedVector<T, MAXIMUM_SIZE, CheckingType>;

public:
    using Builder =
        fixed_vector_detail::FixedVectorBuilder<T, FixedVector<T, MAXIMUM_SIZE, CheckingType>>;

    constexpr FixedVector() noexcept
      : Base()
    {
    }
    constexpr FixedVector(std::initializer_list<T> list,
                          const std_transition::source_location& loc =
                              std_transition::source_location::current()) noexcept
      : Base(list, loc)
    {
    }
    constexpr FixedVector(std::size_t count,
                          const T& value,
                          const std_transition::source_location& loc =
                              std_transition::source_location::current()) noexcept
      : Base(count, value, loc)
    {
    }
    constexpr explicit FixedVector(std::size_t count,
                                   const std_transition::source_location& loc =
                                       std_transition::source_location::current()) noexcept
      : Base(count, loc)
    {
    }
    template <InputIterator InputIt>
    constexpr FixedVector(InputIt first,
                          InputIt last,
                          const std_transition::source_location& loc =
                              std_transition::source_location::current()) noexcept
      : Base(first, last, loc)
    {
    }
};

template <typename T, std::size_t MAXIMUM_SIZE, typename CheckingType>
[[nodiscard]] constexpr typename FixedVector<T, MAXIMUM_SIZE, CheckingType>::size_type is_full(
    const FixedVector<T, MAXIMUM_SIZE, CheckingType>& c)
{
    return c.size() >= c.max_size();
}

template <typename T, std::size_t MAXIMUM_SIZE, typename CheckingType, typename U>
constexpr typename FixedVector<T, MAXIMUM_SIZE, CheckingType>::size_type erase(
    FixedVector<T, MAXIMUM_SIZE, CheckingType>& c, const U& value)
{
    const auto original_size = c.size();
    c.erase(std::remove(c.begin(), c.end(), value), c.end());
    return original_size - c.size();
}

template <typename T, std::size_t MAXIMUM_SIZE, typename CheckingType, typename Predicate>
constexpr typename FixedVector<T, MAXIMUM_SIZE, CheckingType>::size_type erase_if(
    FixedVector<T, MAXIMUM_SIZE, CheckingType>& c, Predicate predicate)
{
    const auto original_size = c.size();
    c.erase(std::remove_if(c.begin(), c.end(), predicate), c.end());
    return original_size - c.size();
}

/**
 * Construct a FixedVector with its capacity being deduced from the number of items being passed.
 */
template <typename T,
          customize::SequenceContainerChecking CheckingType,
          std::size_t MAXIMUM_SIZE,
          // Exposing this as a template parameter is useful for customization (for example with
          // child classes that set the CheckingType)
          typename FixedVectorType = FixedVector<T, MAXIMUM_SIZE, CheckingType>>
[[nodiscard]] constexpr FixedVectorType make_fixed_vector(
    const T (&list)[MAXIMUM_SIZE],
    const std_transition::source_location& loc =
        std_transition::source_location::current()) noexcept
{
    return {std::begin(list), std::end(list), loc};
}

template <typename T, std::size_t MAXIMUM_SIZE>
[[nodiscard]] constexpr auto make_fixed_vector(
    const T (&list)[MAXIMUM_SIZE],
    const std_transition::source_location& loc =
        std_transition::source_location::current()) noexcept
{
    using CheckingType = customize::SequenceContainerAbortChecking<T, MAXIMUM_SIZE>;
    using FixedVectorType = FixedVector<T, MAXIMUM_SIZE, CheckingType>;
    return make_fixed_vector<T, CheckingType, MAXIMUM_SIZE, FixedVectorType>(list, loc);
}

}  // namespace fixed_containers

// Specializations
namespace std
{
template <typename T,
          std::size_t MAXIMUM_SIZE,
          fixed_containers::customize::SequenceContainerChecking CheckingType>
struct tuple_size<fixed_containers::FixedVector<T, MAXIMUM_SIZE, CheckingType>>
  : std::integral_constant<std::size_t, 0>
{
    // Implicit Structured Binding due to the fields being public is disabled
};
}  // namespace std
