#ifndef BENNU_UTILITY_SINGLETON_HPP
#define BENNU_UTILITY_SINGLETON_HPP

namespace bennu {
namespace utility {

template<class T>
class Singleton
{
public:
    static bool theExists()
    {
        return mSingleton != nullptr;
    }

    static T* the()
    {
        if (mSingleton == nullptr)
        {
            mSingleton = new T;
        }
        return mSingleton;
    }

    virtual ~Singleton() {}

protected:
    Singleton() {}

private:
    static T* mSingleton;
    Singleton(const Singleton&);
    Singleton& operator =(const Singleton&);

};

template<class T>
T* Singleton<T>::mSingleton = nullptr;

} // namespace utility
} // namespace bennu

#endif // bennu_UTILITY_SINGLETON_HPP

