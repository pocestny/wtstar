![logo](./frontend/static/img/logo.png) 

# WT\*: a framework for SIMD algorithms design

<h3 style="text-align:center;width:100%;margin-bottom:40px;"><i> 
--- preliminary draft --- 
</i></h3>

WT\* is a framework to study and teach the design of SIMD parallel algorithms. It's
based on the ideas of [Uzi Vishkin](https://dblp.uni-trier.de/pers/hd/v/Vishkin:Uzi),
used also in the book [JáJá: Introduction to Parallel Algorithms](https://www.amazon.com/Introduction-Parallel-Algorithms-Joseph-JaJa/dp/0201548569/ref=sr_1_1).
The aim of WT\* is to provide a hardware-independent programmer-friendly language which,
at the same time, captures essential properties of parallel programs, yet can be relatively easily mapped to existing hardware-supported languages.

The WT\* programming model consists of a number of threads. The program is started with
one thread, and other threads can be dynamically created and terminated, forming a tree-like
structure of parent-child threads. All threads operate synchronously: they share 
a common code with a single PC (program counter) register. At each instant some group
of threads is *active* and they perform the current operation. 
Each thread can have its own local variables, and can also access all variables of its 
ancestors.
The complexity 
is measured in terms of *time* (the number of consecutive steps) and *work* (the overall number of instructions performed by all active threads).

The WT\* framework consists of a language and a virtual machine, with a tools to write and 
execute SIMD programs in an emulated environment.

<a name="toc"></a>

The structure of this document is as follows:

1. [Usage](#usage)
    * [Live frontend](#web)
    * [CLI tools](#cli)
    * [Building from source](#building)
2. [Description of the language](#language)
    * [Types](#types)
    * [Variables](#variables)
    * [Input and output](#io)
    * [Assignment](#ass)
    * [Expressions](#expressions)
    * [Statements](#statements)
    * [Functions](#functions)
3. [Demos](#demos)    

## Usage <a name="usage"></a>

### Web interface <a name="web"></a>

The simplest way to use the framework is via the [live web frontend](./frontend.html)

### CLI tools <a name="cli"></a>

* `wtc` is a compiler from the WT\* language to a binary format
* `wtr` runs the binary (reads input from stdin)

### Building from source <a name="building"></a>

TODO

[--toc--](#toc)

## Description of the language <a name="language"></a>

The WT\* provides a simple imperative strongly typed language, with a C-like syntax.


The program is a sequence of type definitions, variable definitions, function definitions,
and statements.


![program structure](./frontend/static/img/program.png)

### Types <a name="types"></a>

There are four basic types: `int`, `float`, `char`, and `void`. User defined types 
(a.k.a. structs, or records) can be created with the `type` keyword by combining
basic and user defined types:

![type definition](./frontend/static/img/type_definition.png)

so e.g. this is a valid code:

    type point {
      float x,y;
    } 

    type my_type {
      point p;
      int tag;
    }


### Variables and arrays <a name="variables"></a>

Variables are declared by specifying the type:

![variable declaration](./frontend/static/img/variable_declaration.png)

The *variable declarator* is in the simplest form the name of the variable, e.g.
`my_type quak;`. 
With severeal active threads, the variable declaration creates a separate copy of
the variable in each active thread.
The language supports multidimensional arrays. The number of dimensions
is fixed. The size of each
dimension 
must be specified in the declaration. So, e.g. `int B[n,n*n];` declares a 2-dimensional
array `B`.
For arrays, the keyword `dim` gives the number of dimensions, e.g.

    int A[10,20,30], z = A.dim;

results in `z == 3`. The size of the array in a given dimension can be obtained by 
`A.size(d)`; `A.size` is equivalent to `A.size(0)`.

Finally, a scalar variable (assignment to arrays is not supported) that is not 
designated as input, can be 
assigned to during the declaration. The full variable declarator
syntax is as follows:

![variable declarator](./frontend/static/img/variable_declarator.png)

The initializer can be any expression of compatible type (conversions
among `int` and `float` are performed). Compound types use the bracket 
format, so e.g.,

    type point {
      float x,y;
    }

    type goo {
      point p;
      int a;
    }

    goo gle = { {3.14, -9.6} , 47 };
    
    type ipoint {
      int a,b;
    }

    ipoint p = gle.p;

results in `p == {3, -9}`.

### Input and output <a name="io"></a>

The input and output is handled by input/output variables. Any variable definition can be 
preceded by the keyword `output` which means the variable is meant to be part of the output. 
The runtime environment is responsible for displaying the output variables. The simplest way 
is to list all the values of the output variables in the order they appear in the source file.

Similarly, the input of data is handled by prefixing a variable definition by a keyword 
`input` (at most one of the `input` `output` keywords may be present). Again, the runtime 
environment is responsible for reading the data and initializing the variables. For
obvious reasons, input variables don't support initializers.

A caveat with input arrays is that the size of the array is not known in compile time 
(only the number of dimensions is).
Instead of the size expression, the input arrays use the *don't care* symbol (`_`)
in definition.
Following is a simple program that reads a 1-dimensional array, and returns its size:

    input int A[_];
    output int x = A.size;


### Assignment <a name="ass"></a>

### Expressions <a name="expressions"></a>

### Statements <a name="statements"></a>

### Functions <a name="functions"></a>

The language supports C-like functions.

![function definition](./frontend/static/img/function_definition.png)

![parameter declarator](./frontend/static/img/parameter_declarator.png)

[--toc--](#toc)


## Demos <a name="demos"></a>

