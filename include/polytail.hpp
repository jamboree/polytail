/*//////////////////////////////////////////////////////////////////////////////
    Copyright (c) 2019-2025 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////*/
#ifndef POLYTAIL_HPP_INCLUDED
#define POLYTAIL_HPP_INCLUDED

#include <memory>
#include <cstdint>
#include <type_traits>

namespace pltl {
    template<class Expr, class T>
    struct enable_expr {
        using type = T;
    };

    template<class Expr, class T>
    using enable_expr_t = typename enable_expr<Expr, T>::type;

    template<class T, class U>
    concept PtrComparable = requires(T* a, U* b) { a <=> b; };

    template<class... Trait>
    struct composite;

    namespace detail {
        template<class T>
        struct is_composite : std::false_type {};

        template<class... Trait>
        struct is_composite<composite<Trait...>> : std::true_type {};

        template<class T>
        constexpr bool is_composite_v = is_composite<T>::value;

        template<class Trait, bool Implace>
        struct trait_object {
            constexpr trait_object() : m_vptr() {}

            constexpr explicit trait_object(const Trait* p) : m_vptr(p) {}

            template<class... T>
            constexpr explicit trait_object(const composite<T...>* p)
                : m_vptr(&static_cast<const Trait&>(*p)) {}

            template<class... T>
            constexpr explicit trait_object(const composite<T...>& p)
                : m_vptr(&static_cast<const Trait&>(p)) {}

        private:
            friend const Trait* get_vdata(const trait_object* p) {
                return p->m_vptr;
            }
            friend const Trait& get_vtable(const trait_object* p) {
                return *p->m_vptr;
            }

            const Trait* m_vptr;
        };

        template<class Trait>
        struct trait_object<Trait, true> {
            constexpr trait_object() : m_vtable() {}

            template<class Trait2>
            constexpr explicit trait_object(const Trait2* p) : m_vtable(*p) {}

            template<class... T>
            constexpr explicit trait_object(const composite<T...>& p)
                : m_vtable(p) {}

        private:
            friend const Trait& get_vdata(const trait_object* p) {
                return p->m_vtable;
            }
            friend const Trait& get_vtable(const trait_object* p) {
                return p->m_vtable;
            }

            Trait m_vtable;
        };

        std::false_type is_trait_object(...);

        template<class Trait, bool I>
        std::true_type is_trait_object(const trait_object<Trait, I>*);

        template<class T>
        concept IsTraitObject =
            decltype(is_trait_object(std::declval<T*>()))::value;

        template<class Trait, bool ForceImplace>
        using trait_base =
            trait_object<Trait, ForceImplace || sizeof(Trait) <= sizeof(void*)>;

        template<class Trait>
        struct indirect_trait_base : trait_base<Trait, false> {
            constexpr explicit indirect_trait_base(const Trait& p)
                : trait_base<Trait, false>(&p) {}
            operator const Trait&() const noexcept { return get_vtable(this); }
        };

        template<class Trait>
        using proxy_trait_base = trait_base<Trait, is_composite_v<Trait>>;

        template<bool Mut>
        struct indirect_data {
            std::uintptr_t data;
        };

        template<>
        struct indirect_data<true> : indirect_data<false> {};

        template<class Trait, bool Mut>
        struct proxy : indirect_data<Mut>, proxy_trait_base<Trait> {
        protected:
            proxy() noexcept : indirect_data<Mut>{0} {}

            template<class Vptr>
            proxy(std::uintptr_t data, Vptr vptr) noexcept
                : indirect_data<Mut>{data}, proxy_trait_base<Trait>{vptr} {}
        };

        struct boxed_meta_trait {
            std::uintptr_t data_offset;
            void (*destruct)(void*) noexcept;
        };

        template<class Trait>
        struct boxed_trait : boxed_meta_trait, Trait {};

        template<class Trait>
        using boxed_trait_base = trait_object<boxed_trait<Trait>, false>;

        template<class Trait, class Trait2>
        using deduce_t = decltype(Trait::deduce(std::declval<Trait2>()));
    } // namespace detail

    template<class Trait, class T>
    struct impl_for;

    template<class T, class Trait>
    concept Impl =
        !detail::IsTraitObject<T> && requires { impl_for<Trait, T>{}; };

    template<auto... F>
    struct meta_list {};

    template<class T, class... Trait>
    concept AllImplemented = (... && Impl<T, Trait>);

    template<class... Trait>
    struct composite : detail::indirect_trait_base<Trait>... {
        constexpr composite(const Trait&... vtable)
            : detail::indirect_trait_base<Trait>{vtable}... {}

        template<class... Trait2>
        constexpr composite(const composite<Trait2...>& other) noexcept
            : detail::indirect_trait_base<Trait>(other)... {}
    };

    template<class... Trait, AllImplemented<Trait...> T>
    struct impl_for<composite<Trait...>, T> {};

    namespace detail {
        template<class T, class U>
        struct match_trait_no_const : std::is_convertible<const U&, const T&> {
        };

        template<class... T, class U>
        struct match_trait_no_const<composite<T...>, U>
            : std::conjunction<match_trait_no_const<T, U>...> {};

        template<class T, class U,
                 int Flag = std::is_const_v<T> << 1 | std::is_const_v<U>>
        struct match_trait : match_trait_no_const<T, U> {};

        template<class T, class U>
        struct match_trait<T, U, 1> : std::false_type {};

        template<class T, class U>
        constexpr bool match_trait_v = match_trait<T, U>::value;

        template<class Trait, Impl<Trait> T>
        constexpr impl_for<Trait, T> get_impl(const T&) {
            return {};
        }

        template<class Trait, class Trait2, bool I>
            requires match_trait_v<Trait, Trait2>
        inline const Trait& get_impl(const trait_object<Trait2, I>& p) {
            return get_vtable(&p);
        }

        template<class Trait, class Trait2, bool I>
        inline const deduce_t<Trait, Trait2>&
        get_impl(const trait_object<Trait2, I>& p) {
            return get_vtable(&p);
        }

        template<class Trait, class T, auto... F>
        constexpr Trait make_vtable(meta_list<F...>);

        template<class Trait, class T>
        inline const Trait vtable = make_vtable<Trait, T>(
            typename Trait::template meta_list<impl_for<Trait, T>>{});

        template<class... Trait, class T>
        inline const composite<Trait...> vtable<composite<Trait...>, T>{
            vtable<Trait, T>...};
    } // namespace detail

    template<class Trait>
    struct boxed : detail::boxed_trait_base<Trait> {
        explicit boxed(const detail::boxed_trait<Trait>* vptr)
            : detail::boxed_trait_base<Trait>{vptr} {}

        boxed(const boxed&) = delete;

        boxed& operator=(const boxed&) = delete;

        std::uintptr_t data() const noexcept {
            return reinterpret_cast<std::uintptr_t>(
                reinterpret_cast<const char*>(this) +
                get_vtable(this).data_offset);
        }

        friend void destruct(boxed* self) noexcept {
            get_vtable(self).destruct(self);
        }
    };

    template<class Trait>
    struct dyn_ptr : detail::proxy<Trait, true> {
        using base_t = detail::proxy<Trait, true>;

        dyn_ptr() = default;

        dyn_ptr(std::nullptr_t) noexcept {}

        template<Impl<Trait> T>
        dyn_ptr(T* p) noexcept
            : base_t(reinterpret_cast<std::uintptr_t>(p),
                     &detail::vtable<Trait, T>) {}

        template<class Trait2>
            requires detail::match_trait_v<Trait, Trait2>
        dyn_ptr(dyn_ptr<Trait2> p) noexcept : base_t(p.data, get_vdata(&p)) {}

        template<class Trait2>
            requires detail::match_trait_v<Trait, Trait2>
        dyn_ptr(boxed<Trait2>* p) noexcept : base_t(p->data(), get_vdata(p)) {}

        explicit dyn_ptr(base_t base) : base_t(base) {}

        explicit operator bool() const noexcept { return this->data != 0; }
    };

    template<class Trait>
    struct dyn_ptr<const Trait> : detail::proxy<Trait, false> {
        using base_t = detail::proxy<Trait, false>;

        dyn_ptr() = default;

        dyn_ptr(std::nullptr_t) noexcept {}

        template<Impl<Trait> T>
        dyn_ptr(const T* p) noexcept
            : base_t(reinterpret_cast<std::uintptr_t>(p),
                     &detail::vtable<Trait, T>) {}

        template<class Trait2>
            requires detail::match_trait_v<const Trait, Trait2>
        dyn_ptr(dyn_ptr<Trait2> p) noexcept : base_t(p.data, get_vdata(&p)) {}

        template<class Trait2>
            requires detail::match_trait_v<const Trait, Trait2>
        dyn_ptr(const boxed<Trait2>* p) noexcept
            : base_t(p->data(), get_vdata(p)) {}

        explicit dyn_ptr(base_t base) : base_t(base) {}

        explicit operator bool() const noexcept { return this->data != 0; }
    };

    template<class T, class U>
        requires PtrComparable<T, U>
    inline auto operator<=>(dyn_ptr<T> a, dyn_ptr<U> b) noexcept {
        return a.data <=> b.data;
    }

    template<class T, class U>
        requires PtrComparable<T, U>
    inline bool operator==(dyn_ptr<T> a, dyn_ptr<U> b) noexcept {
        return a.data == b.data;
    }

    template<class T>
    inline bool operator==(dyn_ptr<T> a, std::nullptr_t) noexcept {
        return !a.data;
    }

    template<class Trait>
    struct dyn_ref : detail::proxy<Trait, true> {
        using base_t = detail::proxy<Trait, true>;

        template<Impl<Trait> T>
        dyn_ref(T& r) noexcept
            : base_t(reinterpret_cast<std::uintptr_t>(&r),
                     &detail::vtable<Trait, T>) {}

        template<class Trait2>
            requires detail::match_trait_v<Trait, Trait2>
        dyn_ref(dyn_ref<Trait2> r) noexcept : base_t(r.data, get_vdata(&r)) {}

        template<class Trait2>
            requires detail::match_trait_v<Trait, Trait2>
        dyn_ref(boxed<Trait2>& r) noexcept : base_t(r.data(), get_vdata(&r)) {}

        dyn_ptr<Trait> get_ptr() const noexcept {
            return dyn_ptr<Trait>(*this);
        }
    };

    template<class Trait>
    struct dyn_ref<const Trait> : detail::proxy<Trait, false> {
        using base_t = detail::proxy<Trait, false>;

        template<Impl<Trait> T>
        dyn_ref(const T& r) noexcept
            : base_t(reinterpret_cast<std::uintptr_t>(&r),
                     &detail::vtable<Trait, T>) {}

        template<class Trait2>
            requires detail::match_trait_v<const Trait, Trait2>
        dyn_ref(dyn_ref<Trait2> r) noexcept : base_t(r.data, get_vdata(&r)) {}

        template<class Trait2>
            requires detail::match_trait_v<const Trait, Trait2>
        dyn_ref(const boxed<Trait2>& r) noexcept
            : base_t(r.data(), get_vdata(&r)) {}

        dyn_ptr<const Trait> get_ptr() const noexcept {
            return dyn_ptr<const Trait>(*this);
        }
    };

    namespace detail {
        template<class Trait, class T>
        struct boxer : boxed<Trait> {
            explicit boxer(T&& val)
                : boxed<Trait>(&boxed_vtable), m_data(std::move(val)) {}

        private:
            static constexpr std::uintptr_t calc_data_offset() {
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
                return offsetof(boxer, m_data);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
            }

            static void destruct_impl(void* self) noexcept {
                static_cast<boxer*>(self)->~boxer();
            }

            static const boxed_trait<Trait> boxed_vtable;

            T m_data;
        };

        template<class Trait, class T>
        inline const boxed_trait<Trait> boxer<Trait, T>::boxed_vtable{
            boxed_meta_trait{calc_data_offset(), destruct_impl},
            vtable<Trait, T>};
    } // namespace detail

    struct boxed_deleter {
        template<class Trait>
        void operator()(boxed<Trait>* p) const noexcept {
            destruct(p);
            ::operator delete(p);
        }
    };

    template<class Trait, class T>
    inline std::unique_ptr<boxed<Trait>, boxed_deleter> box_unique(T val) {
        return std::unique_ptr<boxed<Trait>, boxed_deleter>(
            new detail::boxer<Trait, T>(std::move(val)));
    }

    template<class Trait, class T>
    inline std::shared_ptr<boxed<Trait>> box_shared(T val) {
        return std::make_shared<detail::boxer<Trait, T>>(std::move(val));
    }

    struct mut_this {
        std::uintptr_t self;

        mut_this(detail::indirect_data<true> h) noexcept : self(h.data) {}

        template<class Trait>
        mut_this(boxed<Trait>& b) noexcept : self(b.data()) {}
    };

    struct const_this {
        std::uintptr_t self;

        const_this(detail::indirect_data<false> h) noexcept : self(h.data) {}

        template<class Trait>
        const_this(const boxed<Trait>& b) noexcept : self(b.data()) {}
    };

    struct ignore_this {
        ignore_this() = default;

        template<class T>
        constexpr ignore_this(const T& b) {}
    };

    namespace detail {
        template<class Sig>
        using fn_ptr = Sig*;

        template<class FT>
        struct no_this : std::false_type {};

        template<class R, class... A>
        struct no_this<R (*)(ignore_this, A...)> : std::true_type {};

        // MSVC cannot deduce noexcept(B), so another a specialization.
        template<class R, class... A>
        struct no_this<R (*)(ignore_this, A...) noexcept> : std::true_type {};

        template<class T>
        struct deref {
            static T& get(mut_this p) noexcept {
                return *reinterpret_cast<T*>(p.self);
            }

            static const T& get(const_this p) noexcept {
                return *reinterpret_cast<const T*>(p.self);
            }
        };

        template<auto F>
        struct delegate_no_this {
            template<class R, class U, class... A>
            constexpr operator fn_ptr<R(U, A...)>() const {
                return
                    [](U, A... a) -> R { return F({}, std::forward<A>(a)...); };
            }
        };

        template<class T, auto F>
        struct delegate_t {
            template<class R, class U, class... A>
            constexpr operator fn_ptr<R(U, A...)>() const {
                return [](U self, A... a) -> R {
                    return F(deref<T>::get(self), std::forward<A>(a)...);
                };
            }
        };

        template<class T, auto F>
            requires(no_this<decltype(F)>::value)
        struct delegate_t<T, F> : delegate_no_this<F> {};

        template<class Trait, class T, auto... F>
        constexpr Trait make_vtable(meta_list<F...>) {
            return {delegate_t<T, F>{}...};
        }
    } // namespace detail
} // namespace pltl

// Internal macros.
#define Zz_PLTL_RM_PAREN(...) __VA_ARGS__
#define Zz_PLTL_RET(T, expr)                                                   \
    ::pltl::enable_expr_t<decltype(expr), T> { return expr; }
#define Zz_PLTL_RET_PAREN(T, expr)                                             \
    ::pltl::enable_expr_t<decltype(expr), Zz_PLTL_RM_PAREN T> { return expr; }

// Public macros.
#define PLTL_RET(T, expr)                                                      \
    Zz_PLTL_RET(T, ::pltl::detail::get_impl<trait>(self).expr)
#define PLTL_RET_PAREN(T, expr)                                                \
    Zz_PLTL_RET_PAREN(T, ::pltl::detail::get_impl<trait>(self).expr)
#define PLTL_RET_TMP(T, tmp, expr)                                             \
    Zz_PLTL_RET(                                                               \
        T, ::pltl::detail::get_impl<trait<Zz_PLTL_RM_PAREN tmp>>(self).expr)
#define PLTL_RET_TMP_PAREN(T, tmp, expr)                                       \
    Zz_PLTL_RET_PAREN(                                                         \
        T, ::pltl::detail::get_impl<trait<Zz_PLTL_RM_PAREN tmp>>(self).expr)

#endif