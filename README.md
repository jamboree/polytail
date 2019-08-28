polytail
========
Rust-like trait-based polymorphism for C++

## Overview
One thing that Rust does better than C++ is its trait system. This library aims
to offer some important benefits of the trait system:

* Non-intrusive
* Extensible (not restricted to inheritance hierarchy)
* Semantic-based

It can be used with or without type-erasure.

### Requirements
- C++17

### Header
This is a header-only library with a single header and without extra dependancy.
```c++
#include <polytail.hpp>
```
### Synopsis
```c++
namespace pltl
{
    template<class Trait, class T>
    struct impl_for; // User-supplied specialization.

    template<class Trait, class T>
    inline Trait const vtable; // User-supplied specialization.

    template<class... Trait>
    struct composite;

    template<class Trait>
    struct boxed;

    template<class Trait>
    struct dyn_ptr;

    template<class Trait>
    struct dyn_ref;

    struct mut_this;
    struct const_this;

    template<class Trait, class T>
    inline std::unique_ptr<boxed<Trait>, box_deleter> box_unique(T val);

    template<class Trait, class T>
    inline std::shared_ptr<boxed<Trait>> box_shared(T val);
}
```

## How to
### Define a trait:
```c++
namespace StrConv
{
    struct trait
    {
        std::string(*to_str)(pltl::const_this self);
        void(*from_str)(pltl::mut_this self, std::string_view str);
    };

    template<class Self>
    inline auto to_str(Self&& self) -> POLYTAIL_RET(std::string, to_str(self))

    template<class Self>
    inline auto from_str(Self&& self, std::string_view str) -> POLYTAIL_RET(void, from_str(self, str))
}

template<class T>
inline StrConv::trait const pltl::vtable<StrConv::trait, T>
{
    delegate<T, impl_for<StrConv::trait, T>::to_str>,
    delegate<T, impl_for<StrConv::trait, T>::from_str>
};
```

### Implement a trait for a type:
```c++
template<>
struct pltl::impl_for<StrConv::trait, int>
{
    static std::string to_str(int self)
    {
        return std::to_string(self);
    }

    static void from_str(int& self, std::string_view str)
    {
        std::from_chars(str.data(), str.data() + str.size(), self);
    }
};
```

### Use a trait
```c++
std::string s;
int a = 42;
// Without type-erasure.
s = StrConv::to_str(a);
assert(s == "42");
StrConv::from_str(a, "25");
assert(a == 25);

// With type-erasure & ADL.
pltl::dyn_ref<StrConv::trait> aa(a);
s = to_str(aa);
assert(s == "25");
from_str(aa, "1");
assert(a == 1);
```

### Compose traits on demand:
```c++
using Trait = pltl::composite<StrConv::trait, Print::trait>;
int a = 42;
pltl::dyn_ref<Trait> r(a);
print(r); // Print
from_str(r, "25"); // StrConv
pltl::dyn_ref<Print::trait const> r2(a); // Can degrade to sub-trait.
print(r2);
```

### Create boxed values:
```c++
auto p = pltl::box_unique<Trait>(42); // Or box_shared.
boxed<Trait>& b = *p;
pltl::dyn_ref<Trait> r(b);
assert(StrConv::to_str(b) == to_str(r));
```

## License

    Copyright (c) 2019 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)