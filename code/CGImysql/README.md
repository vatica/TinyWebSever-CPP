# 校验、数据库连接池

工作线程从数据库连接池取得一个连接，访问数据库中的数据，访问完毕后将连接交还连接池。

## 数据库连接池
+ 单例模式
+ 线程安全

## 校验
+ HTTP请求采用POST方式
+ 用户名密码校验
+ 线程安全