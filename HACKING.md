# Compiler Requirements

MSVC: Visual Studio 2013 Update 5 or newer.
gcc/llvm: C++11 support is required.

# Coding Conventions

When hacking on YUView, it helps to follow common coding conventions so that
the code is uniform.

## Files

Unix style LF. UTF8 file encoding.

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

Indentation: 2 spaces. Never use tabs since they always look different 
depending on your IDE. See example below.

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

Please try to avoid additional whitespaces within brackets at the opening and 
closing brackets. Within the brackets, however, whitespaces may highly increase readability:

    calculateSomething( a );      // No
    calculateSomething( a + 2 );  // No
    someArray[ i + 35 ] = 7;      // No

    calculateSomething(a);        // Yes
    calculateSomething(a+2);      // Yes
    calculateSomething(a + 2);    // Yes
    someArray[i+35] = 7;          // Yes
    someArray[i + 35] = 7;        // Yes

## Naming of variables and members

Since the project already uses a lot of CamelCase styled variables we will 
stick to this. There are good reasons to use snake_case but that would 
require a lot of changes to the existing code.

    int counterHighlights;
    bool isReadyToGo = true;

Don't mark class members with any prefix. If you have a modern IDE it can tell 
you what type/class the variable you are looking at is a member of. If you want 
to you can probably also configure it to highlight class members differently 
compared to local variables so there is really no reason to put it in the name:

    int iSomeName;             // Don't
    int p_privateClassMember;  // Also don't
    int m_anotherClassMember;  // No
    
    int someName;              // Yes
    int pricateClassMember;    // Yes
    int anotherClassMember;    // Yes

## Class declarations

    class Foo : public Bar, private Baz
    {
       Q_OBJECT       // These two lines on all
                      // QObject-derived classes ONLY.
    public:
       void method();

    private:
       int member;
    };

    struct Soo : Sar
    {
       void method();
       int member;
    };
