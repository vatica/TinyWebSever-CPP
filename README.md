# TinyWebSever-CPP

[![LICENSE](https://img.shields.io/badge/license-Apache2.0-green)]()
[![ubuntu](https://img.shields.io/badge/ubuntu-18.04-%237732a8)]()
[![mysql](https://img.shields.io/badge/mysql-5.7.36-blue)]()

基于C++开发的简单Web服务器:sunny:

## 更新日志
- [x] 2021.11.30 发布第一个版本，来自于[qinguoyi/TinyWebServer](https://github.com/qinguoyi/TinyWebServer)。
- [ ] TODO: 暂无。

## 目录
1. [运行](#运行)
2. [庖丁解牛](#庖丁解牛)

## 运行
### 快速运行
+ 服务器测试环境
    + Ubuntu版本18.04
    + MySQL版本5.7.36
+ 浏览器测试环境
    + Windows、Linux均可
    + Chrome、FireFox
    + 其他浏览器暂无测试
+ 测试前确认已安装MySQL数据库
    ```C++
    // 建立yourdb库
    create database yourdb;

    // 创建user表
    USE yourdb;
    CREATE TABLE user(
        username char(50) NULL,
        passwd char(50) NULL
    )ENGINE=InnoDB;

    // 添加数据
    INSERT INTO user(username, passwd) VALUES('name', 'passwd');
    ```
+ 修改[main.cpp](./code/main.cpp)中的数据库初始化信息
    ```C++
    // 数据库用户名、密码、库名
    string user = "root";
    string passwd = "root";
    string databasename = "yourdb";
    ```
+ build
    ```C++
    make server
    ```
+ 启动server
    ```C++
    ./bin/server
    ```
+ 浏览器端
    ```C++
    ip:port
    ```

### 个性化运行

```C++
./server [-p port] [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER] [-s sql_num] [-t thread_num] [-c close_log] [-a actor_model]
```

+ -p，自定义端口号
	+ 默认8081
+ -l，选择日志写入方式，默认同步写入
	+ 0，同步写入
	+ 1，异步写入
+ -m，listenfd和connfd的模式组合，默认使用LT + LT
	+ 0，表示使用LT + LT
	+ 1，表示使用LT + ET
    + 2，表示使用ET + LT
    + 3，表示使用ET + ET
+ -o，优雅关闭连接，默认不使用
	+ 0，不使用
	+ 1，使用
+ -s，数据库连接数量
	+ 默认为8
+ -t，线程数量
	+ 默认为8
+ -c，关闭日志，默认打开
	+ 0，打开日志
	+ 1，关闭日志
+ -a，选择反应堆模型，默认Proactor
	+ 0，Proactor模型
	+ 1，Reactor模型

示例
```C++
./server -p 9007 -l 1 -m 0 -o 1 -s 10 -t 10 -c 1 -a 1
```
## 庖丁解牛

+ [小白视角：一文读懂社长的TinyWebServer](https://huixxi.github.io/2020/06/02/%E5%B0%8F%E7%99%BD%E8%A7%86%E8%A7%92%EF%BC%9A%E4%B8%80%E6%96%87%E8%AF%BB%E6%87%82%E7%A4%BE%E9%95%BF%E7%9A%84TinyWebServer/#more)
+ [最新版Web服务器项目详解 - 01 线程同步机制封装类](https://mp.weixin.qq.com/s?__biz=MzAxNzU2MzcwMw==&mid=2649274278&idx=3&sn=5840ff698e3f963c7855d702e842ec47&chksm=83ffbefeb48837e86fed9754986bca6db364a6fe2e2923549a378e8e5dec6e3cf732cdb198e2&scene=0&xtrack=1#rd)
+ [最新版Web服务器项目详解 - 02 半同步半反应堆线程池（上）](https://mp.weixin.qq.com/s?__biz=MzAxNzU2MzcwMw==&mid=2649274278&idx=4&sn=caa323faf0c51d882453c0e0c6a62282&chksm=83ffbefeb48837e841a6dbff292217475d9075e91cbe14042ad6e55b87437dcd01e6d9219e7d&scene=0&xtrack=1#rd)
+ [最新版Web服务器项目详解 - 03 半同步半反应堆线程池（下）](https://mp.weixin.qq.com/s/PB8vMwi8sB4Jw3WzAKpWOQ)
+ [最新版Web服务器项目详解 - 04 http连接处理（上）](https://mp.weixin.qq.com/s/BfnNl-3jc_x5WPrWEJGdzQ)
+ [最新版Web服务器项目详解 - 05 http连接处理（中）](https://mp.weixin.qq.com/s/wAQHU-QZiRt1VACMZZjNlw)
+ [最新版Web服务器项目详解 - 06 http连接处理（下）](https://mp.weixin.qq.com/s/451xNaSFHxcxfKlPBV3OCg)
+ [最新版Web服务器项目详解 - 07 定时器处理非活动连接（上）](https://mp.weixin.qq.com/s/mmXLqh_NywhBXJvI45hchA)
+ [最新版Web服务器项目详解 - 08 定时器处理非活动连接（下）](https://mp.weixin.qq.com/s/fb_OUnlV1SGuOUdrGrzVgg)
+ [最新版Web服务器项目详解 - 09 日志系统（上）](https://mp.weixin.qq.com/s/IWAlPzVDkR2ZRI5iirEfCg)
+ [最新版Web服务器项目详解 - 10 日志系统（下）](https://mp.weixin.qq.com/s/f-ujwFyCe1LZa3EB561ehA)
+ [最新版Web服务器项目详解 - 11 数据库连接池](https://mp.weixin.qq.com/s?__biz=MzAxNzU2MzcwMw==&mid=2649274326&idx=1&sn=5af78e2bf6552c46ae9ab2aa22faf839&chksm=83ffbe8eb4883798c3abb82ddd124c8100a39ef41ab8d04abe42d344067d5e1ac1b0cac9d9a3&token=1450918099&lang=zh_CN#rd)
+ [最新版Web服务器项目详解 - 12 注册登录](https://mp.weixin.qq.com/s?__biz=MzAxNzU2MzcwMw==&mid=2649274431&idx=4&sn=7595a70f06a79cb7abaebcd939e0cbee&chksm=83ffb167b4883871ce110aeb23e04acf835ef41016517247263a2c3ab6f8e615607858127ea6&token=1686112912&lang=zh_CN#rd)
+ [最新版Web服务器项目详解 - 13 踩坑与面试题](https://mp.weixin.qq.com/s?__biz=MzAxNzU2MzcwMw==&mid=2649274431&idx=1&sn=2dd28c92f5d9704a57c001a3d2630b69&chksm=83ffb167b48838715810b27b8f8b9a576023ee5c08a8e5d91df5baf396732de51268d1bf2a4e&token=1686112912&lang=zh_CN#rd)
