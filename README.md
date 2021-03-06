# AsyncLog_muduo
基于muduo的高性能多线程异步日志库
## 简介

本文主要实现基于muduo的异步日志库。

一个高效的日志库组件是非常重要的。在分布式系统中，日志通常是事故调查的唯一线索；在开发过程中，借助日志可以更好的理解程序逻辑，调试错误。在服务端编程中，日志是必不可少的。

对于一个日志要记录的内容有：收到每条消息的id，每条消息的全文，每条日志都要有时间戳，关键内部状态变更等等...

muduo库的日志输出是C++的stream风格，使用方便，而且是线程安全的。（线程安全的Singleton）

### 日志输出格式

![image-20210729161522540](https://user-images.githubusercontent.com/50821178/127523872-516d9154-b582-49d5-8051-a71484411192.png)


示例：**20210729 08:00:00.565854Z 37081 DEBUG main debug - Logging_test.cc:65**

更多内容参考《Linux多线程服务端编程：使用muduo C++网络库》 陈硕
					 《Muduo 网络库使用手册 》陈硕

muduo日志库的LogFile会自动根据文件大小和时间来主动滚动日志文件，可以主动rolling，因此就不必支持信号。毕竟多线程程序中处理signal特别麻烦。

值得一提的是，linux上的线程标识 **pthread_t** 并不适用于在程序中作为线程的标识符。 建议使用**gettid(2)** 系统调用的返回值作为线程id。详情参见《Linux多线程服务端编程：使用muduo C++网络库》 $4.3 linux上的线程标识

## 高性能

为了实现高性能指标，muduo日志设计中的几点优化值得一提：

- **时间戳字符串中的日期和时间两部分是缓存的,一秒之内的多条日志只需格式化微妙即可**

- **日志消息 的前四段是定长的,可避免运行时字串长度的计算**

- **线程id是预先缓存好，再格式化为字符串，在输出日志时只需简单拷贝几个字节** 

- **原文件名部分通过编译期计算来获得,避免运行期strrchr()的开销**

  

## 多线程异步日志库

### 多线程日志库的要求

​		线程安全：多个线程可以并发写日志，两个线程的日志消息不会出现交织
解决方法：
​		 1、用一个全局变量mutex保护I/O，但这将导致所有的线程抢占一把锁，最终变成串行执行。
​		 2、 每个线程单独写一个日志文件。
​					由于磁盘I/O相对而言是比较耗时的，可能造成业务线程阻塞在写磁盘上，影响并发能力

muduo库采取的方法用一个背景线程负责收集日志消息，并写入日志文件。（单线程，自然是线程安全的）
其他业务线程只管往这个“日志线程”发送日志消息。这是一个典型的生产者（N个）消费者问题（1个），可以使用BlockingQueue或者互斥锁+条件变量实现。这里面优化的点是，采用多缓冲区机制multipleBuffering，这样就不用没产生一条日志消息都通知对方写，可以降低写文件的次数。

## Logger实现

 Logger类内部包装了一个 Impl类，我们可以认为Logger类负责简单的日志级别，实际的实现由Impl完成（如格式化日志）。  Impl类包含了一个LogStream成员；LogStream重载了<<； Impl借助LogStream实现输出， 此时日志只是输出到缓冲区，并没有输出到控制台或文件中（先输出到缓冲区，最后再把缓冲区中的内容输出到标准设备（只定义了标准输出及文件）输出目的地可以使用**void Logger::setOutput(OutputFunc out) ** 设置)， 最终在析构函数中执行实际输出。 因为定义的是一个匿名对象，方法调用完毕就执行析构 。 
使用示例参见：muduo/base/tests/Logging_test.cc

### 日志滚动条件

•日志滚动的条件：
文件大小（如写满1G就换一个文件）
时间（每天零点新建一个文件，不论前一个文件是否写满） 
这两个条件任意一个满足都会产生一个新的日志文件。

•通过 **Logfile** 类来实现日志滚动 

## 性能实测

硬件：i5-1135G7

1、**单线程写100万条日志的效率** 

![image-20210729160143515](https://user-images.githubusercontent.com/50821178/127523988-b44f8a5b-e92c-4ae2-8fe6-da399beac43b.png)


吞吐量每秒达269.32 MiB/s

2、**多线程写的效率**
利用线程池中的线程写，效率极高；
示例：写300W次小字节数据，所花时间。

![image-20210729174510373](https://user-images.githubusercontent.com/50821178/127524024-ac15b963-328e-49ba-85c4-25c3c61011a7.png)

### 使用实例
chmod 755 build.sh
./build.sh
或者
mkdir build; cd build; cmake ../ 
