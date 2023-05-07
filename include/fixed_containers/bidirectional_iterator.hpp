#pragma once

#include "fixed_containers/arrow_proxy.hpp"
#include "fixed_containers/iterator_utils.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>

namespace fixed_containers
{
template <class P>
concept NextAndPreviousProvider = requires(P p, P other) {
    p.advance();
    p.get();
    p.recede();
    p == other;
};

template <NextAndPreviousProvider ConstReferenceProvider,
          NextAndPreviousProvider MutableReferenceProvider,
          IteratorConstness CONSTNESS,
          IteratorDirection DIRECTION>
class BidirectionalIterator
{
    static constexpr IteratorConstness NEGATED_CONSTNESS = IteratorConstness(!bool(CONSTNESS));
    static constexpr IteratorReturnTypeOwnership ITERATOR_RETURN_TYPE_OWNERSHIP =
        CONSTNESS == IteratorConstness::CONSTANT_ITERATOR
            ? ConstReferenceProvider::ITERATOR_RETURN_TYPE_OWNERSHIP
            : MutableReferenceProvider::ITERATOR_RETURN_TYPE_OWNERSHIP;
    static constexpr bool SAFE_LIFETIME =
        ITERATOR_RETURN_TYPE_OWNERSHIP == IteratorReturnTypeOwnership::COLLECTION_OWNED;

    // Sibling has the same parameters, but different const-ness
    using Sibling = BidirectionalIterator<ConstReferenceProvider,
                                          MutableReferenceProvider,
                                          NEGATED_CONSTNESS,
                                          DIRECTION>;

    // Give Sibling access to private members
    friend class BidirectionalIterator<ConstReferenceProvider,
                                       MutableReferenceProvider,
                                       NEGATED_CONSTNESS,
                                       DIRECTION>;

    using ReverseBase = BidirectionalIterator<ConstReferenceProvider,
                                              MutableReferenceProvider,
                                              CONSTNESS,
                                              IteratorDirection(!bool(DIRECTION))>;

    using ReferenceProvider = std::conditional_t<CONSTNESS == IteratorConstness::CONSTANT_ITERATOR,
                                                 ConstReferenceProvider,
                                                 MutableReferenceProvider>;

public:
    using reference = decltype(std::declval<ReferenceProvider>().get());
    using value_type = std::remove_cvref_t<reference>;
    using pointer =
        std::conditional_t<SAFE_LIFETIME, std::add_pointer_t<reference>, ArrowProxy<reference>>;
    using iterator = BidirectionalIterator;
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;

private:
    ReferenceProvider reference_provider_;

public:
    constexpr BidirectionalIterator() noexcept
      : BidirectionalIterator{ReferenceProvider{}}
    {
    }

    explicit constexpr BidirectionalIterator(const ReferenceProvider& reference_provider) noexcept
      : reference_provider_(reference_provider)
    {
        if constexpr (DIRECTION == IteratorDirection::REVERSE)
        {
            advance();
        }
    }

    // Mutable iterator needs to be convertible to const iterator
    constexpr BidirectionalIterator(const Sibling& other) noexcept
        requires(CONSTNESS == IteratorConstness::CONSTANT_ITERATOR)
      : BidirectionalIterator{other.reference_provider_}
    {
    }

    constexpr reference operator*() const& noexcept { return reference_provider_.get(); }
    constexpr std::conditional_t<SAFE_LIFETIME, reference, value_type> operator*() && noexcept
    {
        return reference_provider_.get();
    }
    constexpr pointer operator->() const noexcept
    {
        if constexpr (SAFE_LIFETIME)
        {
            return &reference_provider_.get();
        }
        else
        {
            return {reference_provider_.get()};
        }
    }

    constexpr BidirectionalIterator& operator++() noexcept
    {
        advance();
        return *this;
    }

    constexpr BidirectionalIterator operator++(int) & noexcept
    {
        BidirectionalIterator tmp = *this;
        advance();
        return tmp;
    }

    constexpr BidirectionalIterator& operator--() noexcept
    {
        recede();
        return *this;
    }

    constexpr BidirectionalIterator operator--(int) & noexcept
    {
        BidirectionalIterator tmp = *this;
        recede();
        return tmp;
    }

    constexpr bool operator==(const BidirectionalIterator& other) const noexcept
    {
        return reference_provider_ == other.reference_provider_;
    }

    constexpr bool operator==(const Sibling& other) const noexcept
    {
        return reference_provider_ == other.reference_provider_;
    }

    constexpr ReverseBase base() const noexcept
        requires(DIRECTION == IteratorDirection::REVERSE)
    {
        ReverseBase out{reference_provider_};
        ++out;
        return out;
    }

private:
    constexpr void advance() noexcept
    {
        if constexpr (DIRECTION == IteratorDirection::FORWARD)
        {
            reference_provider_.advance();
        }
        else
        {
            reference_provider_.recede();
        }
    }
    constexpr void recede() noexcept
    {
        if constexpr (DIRECTION == IteratorDirection::FORWARD)
        {
            reference_provider_.recede();
        }
        else
        {
            reference_provider_.advance();
        }
    }
};

}  // namespace fixed_containers
