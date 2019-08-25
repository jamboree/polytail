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
    namespace detail
    {
        template<class U>
        auto test_complete(U*) -> std::bool_constant<sizeof(U)>;
        auto test_complete(...) -> std::false_type;
    }

    template<class T>
    using is_complete = decltype(detail::test_complete(std::declval<T*>()));

    template<class T>
    constexpr bool is_complete_v = is_complete<T>::value;

    template<class Trait, class T>
    struct impl_for;

    template<class Trait, class T, std::enable_if_t<is_complete_v<impl_for<Trait, T>>, bool> = true>
    constexpr impl_for<Trait, T> get_impl(T const&) { return {}; }

    template<class Trait, class T>
    inline Trait const vtable;

    namespace detail
    {
        template<class Trait>
        struct direct_vptr_storage : Trait
        {
            direct_vptr_storage() = default;

            template<class Trait2>
            constexpr direct_vptr_storage(Trait2* p) : Trait(*p) {}

            Trait const& operator*() const noexcept { return *this; }
        };

        template<class Trait, bool multi>
        struct vptr_storage
        {
            using type = Trait const*;
        };

        template<class Trait>
        struct vptr_storage<Trait, false>
        {
            using type = direct_vptr_storage<Trait>;
        };

        template<class Trait>
        using vptr_storage_t = typename vptr_storage<Trait, (sizeof(Trait) > sizeof(void*))>::type;

        template<class Trait>
        struct trait_object
        {
            vptr_storage_t<Trait> vptr;
        };

        template<bool mut>
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

        template<class Trait, bool mut>
        struct proxy : indirect_data<mut>, trait_object<Trait>
        {
        protected:
            proxy() noexcept { this->data = 0; }

            proxy(std::uintptr_t data, vptr_storage_t<Trait> vptr) noexcept
            {
                this->data = data;
                this->vptr = vptr;
            }
        };
    }

    template<class Trait, class Trait2, std::enable_if_t<std::is_base_of_v<Trait, Trait2>, bool> = true>
    inline Trait const& get_impl(detail::trait_object<Trait2> p) { return *p.vptr; }

    template<class Trait>
    struct boxed : detail::trait_object<detail::boxed_trait<Trait>>
    {
        std::uintptr_t data() const noexcept { return (*this->vptr).data(this); }
        friend void destruct(boxed* self) noexcept { (*self->vptr).destruct(self); }
    };

    template<class Trait>
    struct dyn_ptr : detail::proxy<Trait, true>
    {
        using base_t = detail::proxy<Trait, true>;

        dyn_ptr() = default;

        template<class T, std::enable_if_t<is_complete_v<impl_for<Trait, T>>, bool> = true>
        dyn_ptr(T* p) noexcept : base_t(reinterpret_cast<std::uintptr_t>(p), &vtable<Trait, T>) {}

        template<class Trait2, std::enable_if_t<std::is_convertible_v<Trait*, Trait2*>, bool> = true>
        dyn_ptr(dyn_ptr<Trait2> p) noexcept : base_t(p.data, p.vptr) {}

        template<class Trait2, std::enable_if_t<std::is_base_of_v<Trait, Trait2>, bool> = true>
        dyn_ptr(boxed<Trait2>* p) noexcept : base_t(p->data(), p->vptr) {}

        explicit dyn_ptr(base_t base) : base_t(base) {}

        explicit operator bool() const noexcept { return !!this->data; }
    };

    template<class Trait>
    struct dyn_ptr<Trait const> : detail::proxy<Trait, false>
    {
        using base_t = detail::proxy<Trait, false>;

        dyn_ptr() = default;

        template<class T, std::enable_if_t<is_complete_v<impl_for<Trait, T>>, bool> = true>
        dyn_ptr(T const* p) noexcept : base_t(reinterpret_cast<std::uintptr_t>(p), &vtable<Trait, T>) {}

        template<class Trait2, std::enable_if_t<std::is_convertible_v<Trait*, Trait2*>, bool> = true>
        dyn_ptr(dyn_ptr<Trait2> p) noexcept : base_t(p.data, p.vptr) {}

        template<class Trait2, std::enable_if_t<std::is_base_of_v<Trait, Trait2>, bool> = true>
        dyn_ptr(boxed<Trait2> const* p) noexcept : base_t(p->data(), p->vptr) {}

        explicit dyn_ptr(base_t base) : base_t(base) {}

        explicit operator bool() const noexcept { return !!this->data; }
    };

    template<class Trait>
    struct dyn_ref : detail::proxy<Trait, true>
    {
        using base_t = detail::proxy<Trait, true>;

        template<class T, std::enable_if_t<is_complete_v<impl_for<Trait, T>>, bool> = true>
        dyn_ref(T& r) noexcept : base_t(reinterpret_cast<std::uintptr_t>(&r), &vtable<Trait, T>) {}

        template<class Trait2, std::enable_if_t<std::is_convertible_v<Trait*, Trait2*>, bool> = true>
        dyn_ref(dyn_ref<Trait2> r) noexcept : base_t(r.data, r.vptr) {}

        template<class Trait2, std::enable_if_t<std::is_base_of_v<Trait, Trait2>, bool> = true>
        dyn_ref(boxed<Trait2>& r) noexcept : base_t(r.data(), r.vptr) {}

        dyn_ptr<Trait> get_ptr() const noexcept { return dyn_ptr<Trait>(*this); }
    };

    template<class Trait>
    struct dyn_ref<Trait const> : detail::proxy<Trait, false>
    {
        using base_t = detail::proxy<Trait, false>;

        template<class T, std::enable_if_t<is_complete_v<impl_for<Trait, T>>, bool> = true>
        dyn_ref(T const& r) noexcept : base_t(reinterpret_cast<std::uintptr_t>(&r), &vtable<Trait, T>) {}

        template<class Trait2, std::enable_if_t<std::is_convertible_v<Trait*, Trait2*>, bool> = true>
        dyn_ref(dyn_ref<Trait2> r) noexcept : base_t(r.data, r.vptr) {}

        template<class Trait2, std::enable_if_t<std::is_base_of_v<Trait, Trait2>, bool> = true>
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

    template<class Expr, class T>
    struct enable_expr
    {
        using type = T;
    };

    template<class Expr, class T>
    using enable_expr_t = typename enable_expr<Expr, T>::type;
}

#define Z_POLYTAIL_RET(T, expr) ::pltl::enable_expr_t<decltype(expr), T> { return expr; }
#define POLYTAIL_RET(T, expr) Z_POLYTAIL_RET(T, ::pltl::get_impl<trait>(self).expr)

#endif