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

        template<class Trait>
        struct direct_vptr_storage : Trait
        {
            direct_vptr_storage() = default;

            //template<class Trait2>
            constexpr direct_vptr_storage(Trait const* p) : Trait(*p) {}

            Trait const& operator*() const noexcept { return *this; }
        };

        template<class Trait, bool Implace>
        struct vptr_storage
        {
            using type = Trait const*;
        };

        template<class Trait>
        struct vptr_storage<Trait, true>
        {
            using type = direct_vptr_storage<Trait>;
        };

        template<class Trait, bool ForceImplace>
        struct trait_object
        {
            typename vptr_storage<Trait, ForceImplace || sizeof(Trait) <= sizeof(void*)>::type vptr;
        };

        template<bool Mut>
        struct indirect_data
        {
            std::uintptr_t data;
        };

        template<>
        struct indirect_data<true> : indirect_data<false> {};

        template<class Trait>
        struct boxed_trait : Trait
        {
            std::uintptr_t(*data)(void const*) noexcept;
            void(*destruct)(void*) noexcept;
        };

        template<class Trait, bool Mut>
        struct proxy : indirect_data<Mut>, trait_object<Trait, is_composite_v<Trait>>
        {
        protected:
            proxy() noexcept { this->data = 0; }

            proxy(std::uintptr_t data, Trait const* vptr) noexcept
            {
                this->data = data;
                this->vptr = vptr;
            }
        };

        template<class Trait, class Trait2>
        using deduce_t = decltype(Trait::deduce(std::declval<Trait2>()));

        template<class T, class... Trait>
        constexpr bool all_implemented_v = std::conjunction_v<is_complete<impl_for<Trait, T>>...>;

        template<class Trait>
        constexpr Trait const* get_vptr(Trait const& p) { return &p; }

        template<class Trait, class... T>
        constexpr Trait const* get_vptr(composite<T...> const& p)
        {
            return &*static_cast<trait_object<Trait, false> const&>(p).vptr;
        }
    }

    template<class... Trait>
    struct composite : detail::trait_object<Trait, false>... {};

    template<class... Trait, class T>
    inline composite<Trait...> const vtable<composite<Trait...>, T>{{&vtable<Trait, T>}...};

    template<class... Trait, class T>
    struct impl_for<composite<Trait...>, T, std::enable_if_t<detail::all_implemented_v<T, Trait...>>> {};

    namespace detail
    {
        template<class T, class U>
        struct is_subtrait_no_const : std::is_base_of<T, U> {};

        template<class T, class... U>
        struct is_subtrait_no_const<T, composite<U...>> : std::is_base_of<trait_object<T, false>, composite<U...>> {};

        template<class... T, class U>
        struct is_subtrait_no_const<composite<T...>, U> : std::conjunction<is_subtrait_no_const<T, U>...> {};

        template<class... T, class... U>
        struct is_subtrait_no_const<composite<T...>, composite<U...>> : std::conjunction<is_subtrait_no_const<T, U>...> {};

        template<class T, class U>
        struct is_subtrait : is_subtrait_no_const<T, U> {};

        template<class T, class U>
        struct is_subtrait<T const, U> : is_subtrait_no_const<T, U> {};

        template<class T, class U>
        struct is_subtrait<T, U const> : std::false_type {};

        template<class T, class U>
        struct is_subtrait<T const, U const> : is_subtrait_no_const<T, U> {};

        template<class T, class U>
        constexpr bool is_subtrait_v = is_subtrait<T, U>::value;
    }

    template<class Trait, class Trait2, bool FI, std::enable_if_t<detail::is_subtrait_v<Trait, Trait2>, bool> = true>
    inline Trait const& get_impl(detail::trait_object<Trait2, FI> const& p) { return *detail::get_vptr<Trait>(*p.vptr); }

    template<class Trait, class Trait2, bool FI>
    inline detail::deduce_t<Trait, Trait2> const& get_impl(detail::trait_object<Trait2, FI> const& p) { return *p.vptr; }

    template<class Trait, class T>
    using impl_t = std::decay_t<decltype(get_impl<Trait>(std::declval<T>()))>;

    template<class Trait>
    struct boxed : detail::trait_object<detail::boxed_trait<Trait>, false>
    {
        std::uintptr_t data() const noexcept { return (*this->vptr).data(this); }
        friend void destruct(boxed* self) noexcept { (*self->vptr).destruct(self); }
    };

    template<class Trait>
    struct dyn_ptr : detail::proxy<Trait, true>
    {
        using base_t = detail::proxy<Trait, true>;

        dyn_ptr() = default;

        dyn_ptr(std::nullptr_t) noexcept {}

        template<class T, std::enable_if_t<detail::is_complete_v<impl_for<Trait, T>>, bool> = true>
        dyn_ptr(T* p) noexcept : base_t(reinterpret_cast<std::uintptr_t>(p), &vtable<Trait, T>) {}

        template<class Trait2, std::enable_if_t<detail::is_subtrait_v<Trait, Trait2>, bool> = true>
        dyn_ptr(dyn_ptr<Trait2> p) noexcept : base_t(p.data, detail::get_vptr<Trait>(*p.vptr)) {}

        template<class Trait2, std::enable_if_t<detail::is_subtrait_v<Trait, Trait2>, bool> = true>
        dyn_ptr(boxed<Trait2>* p) noexcept : base_t(p->data(), detail::get_vptr<Trait>(*p->vptr)) {}

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

        template<class Trait2, std::enable_if_t<detail::is_subtrait_v<Trait, Trait2>, bool> = true>
        dyn_ptr(dyn_ptr<Trait2> p) noexcept : base_t(p.data, p.vptr) {}

        template<class Trait2, std::enable_if_t<detail::is_subtrait_v<Trait, Trait2>, bool> = true>
        dyn_ptr(boxed<Trait2> const* p) noexcept : base_t(p->data(), p->vptr) {}

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

        template<class Trait2, std::enable_if_t<detail::is_subtrait_v<Trait, Trait2>, bool> = true>
        dyn_ref(dyn_ref<Trait2> r) noexcept : base_t(r.data, detail::get_vptr<Trait>(*r.vptr)) {}

        template<class Trait2, std::enable_if_t<detail::is_subtrait_v<Trait, Trait2>, bool> = true>
        dyn_ref(boxed<Trait2>& r) noexcept : base_t(r.data(), detail::get_vptr<Trait>(*r.vptr)) {}

        dyn_ptr<Trait> get_ptr() const noexcept { return dyn_ptr<Trait>(*this); }
    };

    template<class Trait>
    struct dyn_ref<Trait const> : detail::proxy<Trait, false>
    {
        using base_t = detail::proxy<Trait, false>;

        template<class T, std::enable_if_t<detail::is_complete_v<impl_for<Trait, T>>, bool> = true>
        dyn_ref(T const& r) noexcept : base_t(reinterpret_cast<std::uintptr_t>(&r), &vtable<Trait, T>) {}

        template<class Trait2, std::enable_if_t<detail::is_subtrait_v<Trait, Trait2>, bool> = true>
        dyn_ref(dyn_ref<Trait2> r) noexcept : base_t(r.data, r.vptr) {}

        template<class Trait2, std::enable_if_t<detail::is_subtrait_v<Trait, Trait2>, bool> = true>
        dyn_ref(boxed<Trait2> const& r) noexcept : base_t(r.data(), r.vptr) {}

        dyn_ptr<Trait const> get_ptr() const noexcept { return dyn_ptr<Trait const>(*this); }
    };

    namespace detail
    {
        template<class Trait, class T>
        struct boxer : boxed<Trait>
        {
            boxer(T&& val) : _data(std::move(val)) { this->vptr = &boxed_vtable; }

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

        struct box_deleter
        {
            template<class Trait>
            void operator()(boxed<Trait>* p) const noexcept
            {
                destruct(p);
                ::operator delete(p);
            }
        };
    }

    template<class Trait, class T>
    inline std::unique_ptr<boxed<Trait>, detail::box_deleter> box_unique(T val)
    {
        return std::unique_ptr<boxed<Trait>, detail::box_deleter>(new detail::boxer<Trait, T>(std::move(val)));
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