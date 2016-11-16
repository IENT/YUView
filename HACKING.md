# Coding Conventions

When hacking on YUView, it helps to follow common coding conventions so that
the code is uniform.

## Header Includes and Forward Declarations

In an `.h` file, include only what's needed by the declarations themselves.
Types that are only passed by a pointer or reference can be forward-declared
unless it's hard to do:

    class Foo;
    void fooUser(const Foo &);

Here it's not necessary to include the header that declares `Foo`.

All types *held by value* must be included in the header itself.

Other than that, the order of includes is:

1. `#include "foo.h"` - only in `foo.cpp`. This must always be
   the first include in a `.cpp` file.

   There is an empty line after this include.

2. `#include <iostream>`: standard includes in alphabetical order.
3. `#include <sys/types.h>`: system includes in alphabetical order.
4. Includes for any other libraries/frameworks, in alphabetical order.
5. `#include <QClass>`: Qt includes in alphabetical order.
6. `#include "bar.h"`: YUView includes in alphabetical order.

The order of forward declarations is the same as that of includes.

All standard includes that derive from C should be included as `<cfoo>` not
`<foo.h>`:

    #include <cmath>   // GOOD
    #include <math.h>  // BAD

## Parameter Passing and Const-correctness

In-parameters: the parameters whose values are not being passed out -
should be passed by const reference, or by value if they are small built-in
types like `int`, `bool`, etc.:

    void function(int a, const QImage &image, QString &output1, QString &output2);

If there's only one out-parameter, prefer returning it unless there's a special
requirement to do otherwise:

    QString function(int c); // GOOD
    void function(int c, QString &output); // BAD unless necessary

If a method doesn't modify the state of the object, it should be marked `const`:

    class Foo {
      int m_value;
    public:
      int value() const { return m_value; }
      void setValue(int value) { m_value = value; }
    };

## Layout

Parameters: no space between `*`/`&` and parameter name.

   Type & foo(int a, int *b, int &c, const int &d);

Indentation: 2 spaces. See example below.

    if (foo)
      oneLiner();

    if (bar)
    {
      multiple();
      lines();
    }
    else if (baz)
      oneLineAlternative();
    else
    {
      multi();
      lineAlternative();
    }
