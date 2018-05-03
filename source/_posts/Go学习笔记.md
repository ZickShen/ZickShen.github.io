---
title: Go学习笔记
connments: true
date: 2018-02-20 17:12:49
tags: Programming
categories: Notes
desc: 学习Go的笔记
summary: 学习Go的笔记，方便使用做的
---
# 学习资料：
首推官方https://tour.go-zh.org/welcome/1
其次http://www.runoob.com/go/go-tutorial.html

# 瞎写的笔记

## 基本结构

```go
package main //必须在源文件中非注释的第一行指明这个文件属于哪个包，
			//每个 Go 应用程序都包含一个名为 main 的包。

import "fmt" //使用该包
			//fmt 包实现了格式化 IO（输入/输出）的函数。
import(
	"fmt"
    "math"	//分组导入语句
)

func main() {
    		//注释和C语言是一样的
   fmt.Println("Hello, World!")//效果和Java的Println是一个效果，但句末不需分号
   							//当同一行有多个语句时，用分号分隔，但不推荐
}
```

Go的Hello World

## 变量

```go
//当标识符（包括常量、变量、类型、函数名、结构字段等等）以一个大写字母开头，如：
var Group1 int//定义形式
//那么使用这种形式的标识符的对象就可以被外部包的代码所使用（客户端程序需要先导入这个包），这被称为导出（像面向对象语言中的 public）
//标识符如果以小写字母开头，则对包外是不可见的，但是他们在整个包的内部是可见并且可用的（像面向对象语言中的 protected ）。
/*
 *	Go中的基本类型：
 *	数字类型(u)int8/16/32/64
 *	浮点型float32/64,complex64/128
 *	其他：byte uint8 的别名
 *		 rune int32 的别名, 表示一个 Unicode 码点 
 *		 uint 32 or 64bit
 *		 int  same as uint
 *		 uintprt 无符号整型，用于存放一个指针
 */
var a = 2 int//这种定义会出错
var a = 2	//没问题
var a int	//没问题
var a int =2//没问题

//没有明确初始值的变量声明会被赋予它们的零值。
//零值是：
//数值类型为 0 ，
//布尔类型为 false ，
//字符串为 "" （空字符串）。

//三种定义形式
var a int
var a = 2//交给Go自己判断
a := 2	//左侧变量不应被提前声明过

//与Python类似的多变量声明

a, b, c := 1, 2, 3

//一次声明多个不同类型的变量
var (
	a int
    b bool
)

var e, f = 123, "hello"

//这种只能在函数体中出现
g, h := 123, "hello"
//全局变量允许声明但不实用，局部变量声明不使用会报错
//因为函数外的每个语句都必须以关键字开始（ var 、 func 等等）

//常量定义格式
const identifier [type] = value

//用作枚举
const (
	First = 0
    Second = 1
    Other = 2
)
```
```go
//特殊常量iota
//在每一个const关键字出现时，被重置为0，然后再下一个const出现之前，每出现一次iota，其所代表的数字会自动增加1。
package main

import "fmt"

func main() {
    const (
            a = iota   //0
            b          //1
            c          //2
            d = "ha"   //独立值，iota += 1
            e          //"ha"   iota += 1
            f = 100    //iota +=1
            g          //100  iota +=1
            h = iota   //7,恢复计数
            i          //8
    )
    fmt.Println(a,b,c,d,e,f,g,h,i)
}
```

不同类型的数值没有隐式转换

```go
package main

import "fmt"

func main(){
	var a int8
	var b int64
	a = 123
	b = 1234567890123456
	fmt.Println(a^b, a+b, b-a)
}
//λ go run math.go
//# command-line-arguments
//.\math.go:10:15: invalid operation: a ^ b (mismatched types int8 and int64)
//.\math.go:10:20: invalid operation: a + b (mismatched types int8 and int64)
//.\math.go:10:25: invalid operation: b - a (mismatched types int64 and int8)
```

指针变量

```go
package main

import "fmt"

func main(){
	var a int = 4
	var ptr *int
	ptr = &a    /* 'ptr' 包含了 'a' 变量的地址 */
	fmt.Printf("a 的值为  %d\n", a)
	fmt.Printf("*ptr 为 %d\n", *ptr)
	fmt.Println("ptr的值为%p",ptr)
}
//a 的值为  4
//*ptr 为 4
//ptr的值为%p 0xc042060058
```
## 运算符优先级
| 优先级     | 运算符                 |
| ---- | ---------------- |
| 7    | ^ !              |
| 6    | * / % << >> & &^ |
| 5    | + - \| ^         |
| 4    | == != < <= >= >  |
| 3    | <-               |
| 2    | &&               |
| 1    | \|\|             |

## 条件语句

if类似for，没有小括号，大括号必须，同时在条件表达式前执行语句可以声明变量，该语句声明的变量作用域仅在 if 之内。

```go
package main

import (
	"fmt"
	"math"
)

func pow(x, n, lim float64) float64 {
	if v := math.Pow(x, n); v < lim {
		return v
	} else {
		fmt.Printf("%g >= %g\n", v, lim)
	}
	// 这里开始就不能使用 v 了
	return lim
}

func main() {
	fmt.Println(
		pow(3, 2, 10),
		pow(3, 3, 20),
	)
}

```

switch同理

```go
package main

import (
	"fmt"
	"runtime"
)

func main() {
	fmt.Print("Go runs on ")
	switch os := runtime.GOOS; os {
	case "darwin":
		fmt.Println("OS X.")
	case "linux":
		fmt.Println("Linux.")
	default:
		// freebsd, openbsd,
		// plan9, windows...
		fmt.Printf("%s.", os)
	}
}
//switch的case从上到下顺次执行，匹配成功时停止
```

switch可以没有条件，用起来相当于if-then-else

```go
package main

import (
	"fmt"
	"time"
)

func main() {
	t := time.Now()
	switch {
	case t.Hour() < 12:
		fmt.Println("Good morning!")
	case t.Hour() < 17:
		fmt.Println("Good afternoon.")
	default:
		fmt.Println("Good evening.")
	}
}
```

defer 语句会将函数推迟到外层函数返回之后执行。

推迟调用的函数其参数会立即求值，但直到外层函数返回前该函数都不会被调用。

推迟的函数调用会被压入一个栈中。 当外层函数返回时，被推迟的函数会按照后进先出的顺序调用。

更多关于 defer 语句的信息， 请阅读[此博文](http://blog.go-zh.org/defer-panic-and-recover)。（感觉这个特性很有意思，但是是深坑的既视感）

### 神奇的select

之后补

## 循环

```go
//Go中只有for循环
//基本的 for 循环由三部分组成，它们用分号隔开：

//初始化语句：在第一次迭代前执行
//条件表达式：在每次迭代前求值
//后置语句：在每次迭代的结尾执行
//初始化语句通常为一句短变量声明，该变量声明仅在 for 语句的作用域中可见。

//一旦条件表达式的布尔值为 false，循环迭代就会终止。 注意：和 C、Java、JavaScript 之类的语言不同，Go 的 for 语句后面没有小括号，大括号 { } 则是必须的。
package main

import "fmt"

func main() {
	sum := 0
	for i := 0; i < 10; i++ {
		sum += i
	}
	fmt.Println(sum)
}
//55
```

初始化语句与后置语句是可选的部分

省略循环条件表示无限循环

## 函数

函数可以没有参数或接受多个参数，返回任意数量的返回值，如

```go
package main

import "fmt"

func add(x int, y int) int {
	return x + y
}

func main() {
	fmt.Println(add(42, 13))
}
```

同时当连续两个或多个函数的已命名形参类型相同时，除最后一个类型以外，其它都可以省略。

```go
package main

import "fmt"

func swap(x, y string) (string, string) {
	return y, x
}

func main() {
	a, b := swap("hello", "world")
	fmt.Println(a, b)
}
```

Go 的返回值可被命名，它们会被视作定义在函数顶部的变量。

返回值的名称应当具有一定的意义，它可以作为文档使用。

# 结构体

```go
package main

import "fmt"

type Vertex struct {
	X int
	Y int
}

func main() {
	v := Vertex{1, 2}
	v.X = 4
	fmt.Println(v.X)
}
//如果有指向结构体的指针那么可以通过 (*p).X 来访问其字段 X 。 不过这么写太啰嗦了，所以语言也允许我们使用隐式间接引用，直接写 p.X 就可以。
//结构体文法通过直接列出字段的值来新分配一个结构体。
var	v1 = Vertex{1, 2}  // has type Vertex
//使用 Name: 语法可以仅列出部分字段。（字段名的顺序无关。）
var v2 = Vertex{X: 1}  // Y:0 is implicit
//特殊的前缀 & 返回一个指向结构体的指针。
var p  = &Vertex{1, 2} // has type *Vertex
```

# 数组

类型 `[n]T` 表示拥有 `n` 个 `T` 类型的值的数组。

如：var a [10]int

和Python类似的切片用法，但是只返回引用切片文法

切片文法类似于没有长度的数组文法。

这是一个数组文法：

```
[3]bool{true, true, false}
```

下面这样则会创建一个和上面相同的数组，然后构建一个引用了它的切片：

```
[]bool{true, true, false}
```

```go
package main

import "fmt"

func main() {
	q := []int{2, 3, 5, 7, 11, 13}
	fmt.Println(q)

	r := []bool{true, false, true, true, false, true}
	fmt.Println(r)

	s := []struct {
		i int
		b bool
	}{
		{2, true},
		{3, false},
		{5, true},
		{7, true},
		{11, false},
		{13, true},
	}
	fmt.Println(s)
}
//[2 3 5 7 11 13]
//[true false true true false true]
//[{2 true} {3 false} {5 true} {7 true} {11 false} {13 true}]
```

切片拥有 **长度** 和 **容量** 。

切片的长度就是它所包含的元素个数。

切片的容量是从它的第一个元素开始数，到其底层数组元素末尾的个数。

切片 `s` 的长度和容量可通过表达式 `len(s)` 和 `cap(s)` 来获取。

你可以通过重新切片来扩展一个切片，给它提供足够的容量。 试着修改示例程序中的切片操作，向外扩展它的容量，看看会发生什么。

```go
package main

import "fmt"

func main() {
	s := []int{2, 3, 5, 7, 11, 13}
	printSlice(s)

	// Slice the slice to give it zero length.
	s = s[:0]
	printSlice(s)

	// Extend its length.
	s = s[:4]
	printSlice(s)

	// Drop its first two values.
	s = s[2:]
	printSlice(s)
}

func printSlice(s []int) {
	fmt.Printf("len=%d cap=%d %v\n", len(s), cap(s), s)
}
//len=6 cap=6 [2 3 5 7 11 13]
//len=0 cap=6 []
//len=4 cap=6 [2 3 5 7]
//len=2 cap=4 [5 7]
```

切片的零值是 `nil` 。

nil 切片的长度和容量为 0 且没有底层数组

切面可以用内建函数 `make` 来创建，这也是你创建动态数组的方式。

`make` 函数会分配一个元素为零值的数组并返回一个引用了它的切片：

```
a := make([]int, 5)  // len(a)=5
```

要指定它的容量，需向 `make` 传入第三个参数：

```
b := make([]int, 0, 5) // len(b)=0, cap(b)=5

b = b[:cap(b)] // len(b)=5, cap(b)=5
b = b[1:]      // len(b)=4, cap(b)=4
```

range for 每次迭代返回两个值，一个值为当前元素的下标，另一个值为该下标所对应元素的一份副本。

```go
package main

import "fmt"

var pow = []int{1, 2, 4, 8, 16, 32, 64, 128}

func main() {
	for i, v := range pow {
		fmt.Printf("2**%d = %d\n", i, v)
	}
}
//2**0 = 1
//2**1 = 2
//2**2 = 4
//2**3 = 8
//2**4 = 16
//2**5 = 32
//2**6 = 64
//2**7 = 128
```

可以将下标或值赋予 `_` 来忽略它。

# 映射

就是map

```go
package main

import "fmt"

type Vertex struct {
	Lat, Long float64
}

var m = map[string]Vertex{
	"Bell Labs": Vertex{
		40.68433, -74.39967,
	},
	"Google": Vertex{
		37.42202, -122.08408,
	},
}

func main() {
	fmt.Println(m)
}
//map[Bell Labs:{40.68433 -74.39967} Google:{37.42202 -122.08408}]
```

# 闭包

Go 函数可以是一个闭包。闭包是一个函数值，它引用了其函数体之外的变量。 该函数可以访问并赋予其引用的变量的值，换句话说，该函数被“绑定”在了这些变量上。

例如，函数 `adder` 返回一个闭包。每个闭包都被绑定在其各自的 `sum` 变量上。

```go
package main

import "fmt"

func adder() func(int) int {
	sum := 0
	return func(x int) int {
		sum += x
		return sum
	}
}

func main() {
	pos, neg := adder(), adder()
	for i := 0; i < 10; i++ {
		fmt.Println(
			pos(i),
			neg(-2*i),
		)
	}
}
//0 0
//1 -2
//3 -6
//6 -12
//10 -20
//15 -30
//21 -42
//28 -56
//36 -72
//45 -90
```

函数式编程啊！但是我不会啊！等我看完SICP再来！（滑稽XD

