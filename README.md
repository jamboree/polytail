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
- C++20

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
    inline const Trait vtable; // User-supplied specialization.

    template<class T, class Trait>
    concept Impl;

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
    inline auto to_str(Self&& self) -> PLTL_RET(std::string, to_str(self))

    template<class Self>
    inline auto from_str(Self&& self, std::string_view str) -> PLTL_RET(void, from_str(self, str))
}

template<class T>
inline const StrConv::trait pltl::vtable<StrConv::trait, T>
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
int i = 42;
// Without type-erasure.
s = StrConv::to_str(i);
assert(s == "42");
StrConv::from_str(i, "25");
assert(i == 25);

// With type-erasure & ADL.
pltl::dyn_ref<StrConv::trait> erased(i);
s = to_str(erased);
assert(s == "25");
from_str(erased, "1");
assert(i == 1);
```

### Compose traits on demand:
```c++
using Trait = pltl::composite<StrConv::trait, Print::trait>;
int i = 42;
pltl::dyn_ref<Trait> erased(i);
print(erased); // Print
from_str(erased, "25"); // StrConv
pltl::dyn_ref<const Print::trait> sub(erased); // Can degrade to sub-trait.
print(sub);
```

### Create boxed values:
```c++
auto p = pltl::box_unique<Trait>(42); // Or box_shared.
const pltl::boxed<Trait>& ref = *p;
pltl::dyn_ref<const StrConv::trait> sub(ref); // Can degrade to sub-trait.
assert(to_str(ref) == to_str(sub));
```

## License

    Copyright (c) 2019-2025 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)