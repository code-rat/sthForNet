//#暂时使用饱汉式的单例模式 局部静态变量的实现方式

#pragma once

template<typename T>
class Singleton
{
public:
    static T& Instance(){
        static T _value;
        return _value;
    }

private:

    Singleton(/* args */);
    ~Singleton()=default;

    Singleton(const Singleton&)=delete;
    Singleton& operator=(const Singleton&)=delete;

    //不设置私有的静态变量
};
