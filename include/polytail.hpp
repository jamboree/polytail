/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2019 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef POLYTAIL_HPP_INCLUDED
#define POLYTAIL_HPP_INCLUDED

#include <memory>
#include <type_traits>

namespace pltl
{
    template<class Expr, class T>
    struct enable_expr
    {
        using type = T;
    };

    template<class Expr, class T>
    using enable_expr_t = typename enable_expr<Expr, T>::type;

    namespace detail
    {
        template<class T>
        auto test_complete(T*) -> std::bool_constant<sizeof(T)>;
        auto test_complete(...) -> std::false_type;

        template<class T, class U>
        auto test_compare(T* a, U* b) -> enable_expr_t<decltype(a == b), std::true_type>;
        auto test_compare(...) -> std::false_type;

        template<class T>
        using is_complete = decltype(test_complete(std::declval<T*>()));

        template<class T>
        constexpr bool is_complete_v = is_complete<T>::value;

        template<class T, class U>
        using is_comparable = decltype(test_compare(std::declval<T*>(), std::declval<U*>()));

        template<class T, class U>
        constexpr bool is_comparable_v = is_comparable<T, U>::value;
    }

    template<class Trait, class T, class = void>
    struct impl_for;

    template<class Trait, class T, std::enable_if_t<detail::is_complete_v<impl_for<Trait, T>>, bool> = true>
    constexpr impl_for<Trait, T> get_impl(T const&) { return {}; }

    template<class Trait, class T>
    inline Trait const vtable = nullptr;

    template<class... Trait>
    struct composite;

    namespace detail
    {
        template<class T>
        struct is_composite : std::false_type {};

        template<class... Trait>
        struct is_composite<composite<Trait...>> : std::true_type {};

        template<class T>
        constexpr bool is_composite_v = is_composite<T>::value;

        template<class Trait, bool Implace>
        struct trait_object
        {
            constexpr explicit trait_object(Trait const* p) : _vptr(p) {}

            template<class... T>
            constexpr explicit trait_object(composite<T...> const* p) : _vptr(&static_cast<Trait const&>(*p)) {}
            
            template<class... T>
            constexpr explicit trait_object(composite<T...> const& p) : _vptr(&static_cast<Trait const&>(p)) {}

        private:
            friend Trait const* get_vdata(trait_object const* p) { return p->_vptr; }
            friend Trait const& get_vtable(trait_object const* p) { return *p->_vptr; }

            Trait const* _vptr;
        };

        template<class Trait>
        struct trait_object<Trait, true>
        {
            template<class Trait2>
            constexpr explicit trait_object(Trait2 const* p) : _vtable(*p) {}

            template<class... T>
            constexpr explicit trait_object(composite<T...> const& p) : _vtable(p) {}

        private:
            friend Trait const& get_vdata(trait_object const* p) { return p->_vtable; }
            friend Trait const& get_vtable(trait_object const* p) { return p->_vtable; }

            Trait _vtable;
        };

        template<class Trait, bool ForceImplace>
        using trait_base = trait_object<Trait, ForceImplace || sizeof(Trait) <= sizeof(void*)>;

        template<class Trait>
        struct indirect_trait_base : trait_base<Trait, false>
        {
            constexpr explicit indirect_trait_base(Trait const& p) : trait_base<Trait, false>(&p) {}
            operator Trait const&() const noexcept { return get_vtable(this); }
        };

        template<class Trait>
        using proxy_trait_base = trait_base<Trait, is_composite_v<Trait>>;

        template<bool Mut>
        struct indirect_data
        {
            std::uintptr_t data;
        };

        template<>
        struct indirect_data<true> : indirect_data<false> {};

        template<class Trait, bool Mut>
        struct proxy : indirect_data<Mut>, proxy_trait_base<Trait>
        {
        protected:
            proxy() noexcept : indirect_data<Mut>{0} {}

            template<class Vptr>
            proxy(std::uintptr_t data, Vptr vptr) noexcept
              : indirect_data<Mut>{data}
              , proxy_trait_base<Trait>{vptr}
            {}
        };

        template<class Trait>
        struct boxed_trait : Trait
        {
            std::uintptr_t(*data)(void const*) noexcept;
            void(*destruct)(void*) noexcept;
        };

        template<class Trait, class Trait2>
        using deduce_t = decltype(Trait::deduce(std::declval<Trait2>()));

        template<class T, class... Trait>
        constexpr bool all_implemented_v = std::conjunction_v<is_complete<impl_for<Trait, T>>...>;
    }

    template<class... Trait>
    struct composite : detail::indirect_trait_base<Trait>...
    {
        constexpr composite(Trait const&... vtable) : detail::indirect_trait_base<Trait>{vtable}... {}

        template<class... Trait2>
        constexpr composite(composite<Trait2...> const& other) noexcept : detail::indirect_trait_base<Trait>(other)... {}
    };

    template<class... Trait, class T>
    inline composite<Trait...> const vtable<composite<Trait...>, T>{vtable<Trait, T>...};

    template<class... Trait, class T>
    struct impl_for<composite<Trait...>, T, std::enable_if_t<detail::all_implemented_v<T, Trait...>>> {};

    namespace detail
    {
        template<class T, class U>
        struct match_trait_no_const : std::is_convertible<U const&, T const&> {};

        template<class... T, class U>
        struct match_trait_no_const<composite<T...>, U> : std::conjunction<match_trait_no_const<T, U>...> {};

        template<class T, class U, int Flag = std::is_const_v<T> << 1 | std::is_const_v<U>>
        struct match_trait : match_trait_no_const<T, U> {};

        template<class T, class U>
        struct match_trait<T, U, 1> : std::false_type {};

        template<class T, class U>
        constexpr bool match_trait_v = match_trait<T, U>::value;
    }

    template<class Trait, class Trait2, bool I, std::enable_if_t<detail::match_trait_v<Trait, Trait2>, bool> = true>
    inline Trait const& get_impl(detail::trait_object<Trait2, I> const& p) { return get_vtable(&p); }

    template<class Trait, class Trait2, bool I>
    inline detail::deduce_t<Trait, Trait2> const& get_impl(detail::trait_object<Trait2, I> const& p) { return get_vtable(&p); }

    template<class Trait, class T>
    using impl_t = std::decay_t<decltype(get_impl<Trait>(std::declval<T>()))>;

    template<class Trait>
    struct boxed : detail::trait_object<detail::boxed_trait<Trait>, false>
    {
        explicit boxed(detail::boxed_trait<Trait> const* vptr)
          : detail::trait_object<detail::boxed_trait<Trait>, false>{vptr}
        {}

        std::uintptr_t data() const noexcept { return get_vtable(this).data(this); }
        friend void destruct(boxed* self) noexcept { get_vtable(self).destruct(self); }
    };

    template<class Trait>
    struct dyn_ptr : detail::proxy<Trait, true>
    {
        using base_t = detail::proxy<Trait, true>;

        dyn_ptr() = default;

        dyn_ptr(std::nullptr_t) noexcept {}

        template<class T, std::enable_if_t<detail::is_complete_v<impl_for<Trait, T>>, bool> = true>
        dyn_ptr(T* p) noexcept : base_t(reinterpret_cast<std::uintptr_t>(p), &vtable<Trait, T>) {}

        template<class Trait2, std::enable_if_t<detail::match_trait_v<Trait, Trait2>, bool> = true>
        dyn_ptr(dyn_ptr<Trait2> p) noexcept : base_t(p.data, get_vdata(&p)) {}

        template<class Trait2, std::enable_if_t<detail::match_trait_v<Trait, Trait2>, bool> = true>
        dyn_ptr(boxed<Trait2>* p) noexcept : base_t(p->data(), get_vdata(p)) {}

        explicit dyn_ptr(base_t base) : base_t(base) {}

        explicit operator bool() const noexcept { return !!this->data; }
    };

    template<class Trait>
    struct dyn_ptr<Trait const> : detail::proxy<Trait, false>
    {
        using base_t = detail::proxy<Trait, false>;

        dyn_ptr() = default;

        dyn_ptr(std::nullptr_t) noexcept {}

        template<class T, std::enable_if_t<detail::is_complete_v<impl_for<Trait, T>>, bool> = true>
        dyn_ptr(T const* p) noexcept : base_t(reinterpret_cast<std::uintptr_t>(p), &vtable<Trait, T>) {}

        template<class Trait2, std::enable_if_t<detail::match_trait_v<Trait const, Trait2>, bool> = true>
        dyn_ptr(dyn_ptr<Trait2> p) noexcept : base_t(p.data, get_vdata(&p)) {}

        template<class Trait2, std::enable_if_t<detail::match_trait_v<Trait const, Trait2>, bool> = true>
        dyn_ptr(boxed<Trait2> const* p) noexcept : base_t(p->data(), get_vdata(p)) {}

        explicit dyn_ptr(base_t base) : base_t(base) {}

        explicit operator bool() const noexcept { return !!this->data; }
    };

    template<class T, class U, std::enable_if_t<detail::is_comparable_v<T, U>, bool> = true>
    inline bool operator==(dyn_ptr<T> a, dyn_ptr<U> b) noexcept
    {
        return a.data == b.data;
    }

    template<class T>
    inline bool operator==(dyn_ptr<T> a, std::nullptr_t) noexcept
    {
        return !!a.data;
    }

    template<class T, class U, std::enable_if_t<detail::is_comparable_v<T, U>, bool> = true>
    inline bool operator!=(dyn_ptr<T> a, dyn_ptr<U> b) noexcept
    {
        return a.data != b.data;
    }

    template<class T>
    inline bool operator!=(dyn_ptr<T> a, std::nullptr_t) noexcept
    {
        return !a.data;
    }

    template<class T, class U, std::enable_if_t<detail::is_comparable_v<T, U>, bool> = true>
    inline bool operator<(dyn_ptr<T> a, dyn_ptr<U> b) noexcept
    {
        return a.data < b.data;
    }

    template<class Trait>
    struct dyn_ref : detail::proxy<Trait, true>
    {
        using base_t = detail::proxy<Trait, true>;

        template<class T, std::enable_if_t<detail::is_complete_v<impl_for<Trait, T>>, bool> = true>
        dyn_ref(T& r) noexcept : base_t(reinterpret_cast<std::uintptr_t>(&r), &vtable<Trait, T>) {}

        template<class Trait2, std::enable_if_t<detail::match_trait_v<Trait, Trait2>, bool> = true>
        dyn_ref(dyn_ref<Trait2> r) noexcept : base_t(r.data, get_vdata(&r)) {}

        template<class Trait2, std::enable_if_t<detail::match_trait_v<Trait, Trait2>, bool> = true>
        dyn_ref(boxed<Trait2>& r) noexcept : base_t(r.data(), get_vdata(&r)) {}

        dyn_ptr<Trait> get_ptr() const noexcept { return dyn_ptr<Trait>(*this); }
    };

    template<class Trait>
    struct dyn_ref<Trait const> : detail::proxy<Trait, false>
    {
        using base_t = detail::proxy<Trait, false>;

        template<class T, std::enable_if_t<detail::is_complete_v<impl_for<Trait, T>>, bool> = true>
        dyn_ref(T const& r) noexcept : base_t(reinterpret_cast<std::uintptr_t>(&r), &vtable<Trait, T>) {}

        template<class Trait2, std::enable_if_t<detail::match_trait_v<Trait const, Trait2>, bool> = true>
        dyn_ref(dyn_ref<Trait2> r) noexcept : base_t(r.data, get_vdata(&r)) {}

        template<class Trait2, std::enable_if_t<detail::match_trait_v<Trait const, Trait2>, bool> = true>
        dyn_ref(boxed<Trait2> const& r) noexcept : base_t(r.data(), get_vdata(&r)) {}

        dyn_ptr<Trait const> get_ptr() const noexcept { return dyn_ptr<Trait const>(*this); }
    };

    namespace detail
    {
        template<class Trait, class T>
        struct boxer : boxed<Trait>
        {
            explicit boxer(T&& val) : boxed<Trait>(&boxed_vtable), _data(std::move(val)) {}

        private:
            static std::uintptr_t data_impl(void const* self) noexcept
            {
                return reinterpret_cast<std::uintptr_t>(&static_cast<boxer const*>(self)->_data);
            }
            static void destruct_impl(void* self) noexcept
            {
                static_cast<boxer*>(self)->~boxer();
            }
            static inline boxed_trait<Trait> boxed_vtable{vtable<Trait, T>, data_impl, destruct_impl};

            T _data;
        };
    }

    struct box_deleter
    {
        template<class Trait>
        void operator()(boxed<Trait>* p) const noexcept
        {
            destruct(p);
            ::operator delete(p);
        }
    };

    template<class Trait, class T>
    inline std::unique_ptr<boxed<Trait>, box_deleter> box_unique(T val)
    {
        return std::unique_ptr<boxed<Trait>, box_deleter>(new detail::boxer<Trait, T>(std::move(val)));
    }

    template<class Trait, class T>
    inline std::shared_ptr<boxed<Trait>> box_shared(T val)
    {
        return std::make_shared<detail::boxer<Trait, T>>(std::move(val));
    }

    struct mut_this
    {
        std::uintptr_t self;

        mut_this(detail::indirect_data<true> h) noexcept : self(h.data) {}

        template<class Trait>
        mut_this(boxed<Trait>& b) noexcept : self(b.data()) {}

        template<class T>
        T& get() noexcept { return *reinterpret_cast<T*>(self); }
    };

    struct const_this
    {
        std::uintptr_t self;

        const_this(detail::indirect_data<false> h) noexcept : self(h.data) {}

        template<class Trait>
        const_this(boxed<Trait> const& b) noexcept : self(b.data()) {}

        template<class T>
        T const& get() noexcept { return *reinterpret_cast<T const*>(self); }
    };

    struct ignore_this
    {
        ignore_this() = default;
        template<class T>
        constexpr ignore_this(T const& b) {}
    };

    namespace detail
    {
        template<class Sig>
        using fn_ptr = Sig*;

        template<class FT>
        struct no_this : std::false_type {};

        template<class R, class... A>
        struct no_this<R(*)(ignore_this, A...)> : std::true_type {};

        // MSVC cannot deduce noexcept(B), so another a specialization.
        template<class R, class... A>
        struct no_this<R(*)(ignore_this, A...) noexcept> : std::true_type {};

        template<auto F>
        struct delegate_no_this
        {
            template<class R, class U, class... A>
            constexpr operator fn_ptr<R(U, A...)>() const
            {
                return [](U, A... a) -> R { return F({}, std::forward<A>(a)...); };
            }
        };

        template<class T, auto F, class = void>
        struct delegate_t
        {
            template<class R, class U, class... A>
            constexpr operator fn_ptr<R(U, A...)>() const
            {
                return [](U self, A... a) -> R { return F(self.template get<T>(), std::forward<A>(a)...); };
            }
        };

        template<class T, auto F>
        struct delegate_t<T, F, std::enable_if_t<no_this<decltype(F)>::value>> : delegate_no_this<F> {};
    }

    template<class T, auto F>
    constexpr typename detail::delegate_t<T, F> delegate{};
}

#define Zz_POLYTAIL_RM_PAREN(...) __VA_ARGS__
#define Zz_POLYTAIL_RET(T, expr) ::pltl::enable_expr_t<decltype(expr), T> { return expr; }
#define Zz_POLYTAIL_RET_PAREN(T, expr) ::pltl::enable_expr_t<decltype(expr), Zz_POLYTAIL_RM_PAREN T> { return expr; }
#define POLYTAIL_RET(T, expr) Zz_POLYTAIL_RET(T, ::pltl::get_impl<trait>(self).expr)
#define POLYTAIL_RET_PAREN(T, expr) Zz_POLYTAIL_RET_PAREN(T, ::pltl::get_impl<trait>(self).expr)
#define POLYTAIL_RET_TMP(T, trait, expr) Zz_POLYTAIL_RET(T, ::pltl::get_impl<trait>(self).expr)
#define POLYTAIL_RET_TMP_PAREN(T, trait, expr) Zz_POLYTAIL_RET_PAREN(T, ::pltl::get_impl<trait>(self).expr)

#endif