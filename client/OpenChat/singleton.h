#ifndef SINGLETON_H
#define SINGLETON_H
#include <memory>
#include <iostream>
#include <mutex>
template <typename T>
class Singleton{
protected:
    //设成保护是为了子类继承可以构造
    //这是饿汉式单例
    Singleton() = default;
    Singleton(const Singleton<T>&) = delete;
    Singleton& operator=(const Singleton<T>&) = delete;
    static std::shared_ptr<T> _instance;
public:
    static std::shared_ptr<T>& GetInstance(){
        static std::once_flag s_flag;//只在第一次调用函数的时候会调用一次
        std::call_once(s_flag, [&](){
            _instance = std::shared_ptr<T>(new T);//因为makeshared会调用构造函数，该类构造函数是protected
        });
        return _instance;
    }

    void PrintAddress(){
        std::cout<<_instance.get()<<std::endl;
    }

    ~Singleton(){
        std::cout<<"this singleton destucted"<<std::endl;
    }
};

template <typename T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;
#endif // SINGLETON_H
