/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QPROPERTY_H
#define QPROPERTY_H

#include <QtCore/qglobal.h>
#include <QtCore/QSharedDataPointer>
#include <QtCore/QString>
#include <functional>
#include <type_traits>
#include <variant>

#include <QtCore/qpropertyprivate.h>

#if __has_include(<source_location>) && __cplusplus >= 202002L && !defined(Q_CLANG_QDOC)
#include <experimental/source_location>
#define QT_PROPERTY_COLLECT_BINDING_LOCATION
#define QT_PROPERTY_DEFAULT_BINDING_LOCATION QPropertyBindingSourceLocation(std::source_location::current())
#elif __has_include(<experimental/source_location>) && __cplusplus >= 201703L && !defined(Q_CLANG_QDOC)
#include <experimental/source_location>
#define QT_PROPERTY_COLLECT_BINDING_LOCATION
#define QT_PROPERTY_DEFAULT_BINDING_LOCATION QPropertyBindingSourceLocation(std::experimental::source_location::current())
#else
#define QT_PROPERTY_DEFAULT_BINDING_LOCATION QPropertyBindingSourceLocation()
#endif

QT_BEGIN_NAMESPACE

struct Q_CORE_EXPORT QPropertyBindingSourceLocation
{
    const char *fileName = nullptr;
    const char *functionName = nullptr;
    quint32 line = 0;
    quint32 column = 0;
    QPropertyBindingSourceLocation() = default;
#ifdef QT_PROPERTY_COLLECT_BINDING_LOCATION
    QPropertyBindingSourceLocation(const std::experimental::source_location &cppLocation)
    {
        fileName = cppLocation.file_name();
        functionName = cppLocation.function_name();
        line = cppLocation.line();
        column = cppLocation.column();
    }
#endif
};

template <typename Functor> class QPropertyChangeHandler;
class QPropertyBindingErrorPrivate;

class Q_CORE_EXPORT QPropertyBindingError
{
public:
    enum Type {
        NoError,
        BindingLoop,
        EvaluationError,
        UnknownError
    };

    QPropertyBindingError();
    QPropertyBindingError(Type type, const QString &description = QString());

    QPropertyBindingError(const QPropertyBindingError &other);
    QPropertyBindingError &operator=(const QPropertyBindingError &other);
    QPropertyBindingError(QPropertyBindingError &&other);
    QPropertyBindingError &operator=(QPropertyBindingError &&other);
    ~QPropertyBindingError();

    bool hasError() const { return d.get() != nullptr; }
    Type type() const;
    QString description() const;

private:
    QSharedDataPointer<QPropertyBindingErrorPrivate> d;
};

class Q_CORE_EXPORT QUntypedPropertyBinding
{
public:
    // writes binding result into dataPtr
    using BindingEvaluationFunction = QtPrivate::QPropertyBindingFunction;

    QUntypedPropertyBinding();
    QUntypedPropertyBinding(QMetaType metaType, BindingEvaluationFunction function, const QPropertyBindingSourceLocation &location);
    QUntypedPropertyBinding(QUntypedPropertyBinding &&other);
    QUntypedPropertyBinding(const QUntypedPropertyBinding &other);
    QUntypedPropertyBinding &operator=(const QUntypedPropertyBinding &other);
    QUntypedPropertyBinding &operator=(QUntypedPropertyBinding &&other);
    ~QUntypedPropertyBinding();

    bool isNull() const;

    QPropertyBindingError error() const;

    QMetaType valueMetaType() const;

    explicit QUntypedPropertyBinding(QPropertyBindingPrivate *priv);
private:
    friend class QtPrivate::QPropertyBase;
    friend class QPropertyBindingPrivate;
    template <typename> friend class QPropertyBinding;
    QPropertyBindingPrivatePtr d;
};

template <typename PropertyType>
class QPropertyBinding : public QUntypedPropertyBinding
{
    template <typename Functor>
    struct BindingAdaptor
    {
        Functor impl;
        bool operator()(QMetaType /*metaType*/, void *dataPtr)
        {
            PropertyType *propertyPtr = static_cast<PropertyType *>(dataPtr);
            PropertyType newValue = impl();
            if constexpr (QTypeTraits::has_operator_equal_v<PropertyType>) {
                if (newValue == *propertyPtr)
                    return false;
            }
            *propertyPtr = std::move(newValue);
            return true;
        }
    };

public:
    QPropertyBinding() = default;

    template<typename Functor>
    QPropertyBinding(Functor &&f, const QPropertyBindingSourceLocation &location)
        : QUntypedPropertyBinding(QMetaType::fromType<PropertyType>(), BindingAdaptor<Functor>{std::forward<Functor>(f)}, location)
    {}

    template<typename Property, typename = std::void_t<decltype(&Property::propertyBase)>>
    QPropertyBinding(const Property &property)
        : QUntypedPropertyBinding(property.propertyBase().binding())
    {}

    // Internal
    explicit QPropertyBinding(const QUntypedPropertyBinding &binding)
        : QUntypedPropertyBinding(binding)
    {}
};

namespace Qt {
    template <typename Functor>
    auto makePropertyBinding(Functor &&f, const QPropertyBindingSourceLocation &location = QT_PROPERTY_DEFAULT_BINDING_LOCATION,
                             std::enable_if_t<std::is_invocable_v<Functor>> * = 0)
    {
        return QPropertyBinding<std::invoke_result_t<Functor>>(std::forward<Functor>(f), location);
    }
}

struct QPropertyBasePointer;

template <typename T>
class QProperty
{
public:
    using value_type = T;

    QProperty() = default;
    explicit QProperty(const T &initialValue) : d(initialValue) {}
    explicit QProperty(T &&initialValue) : d(std::move(initialValue)) {}
    QProperty(QProperty &&other) : d(std::move(other.d)) { notify(); }
    QProperty &operator=(QProperty &&other) { d = std::move(other.d); notify(); return *this; }
    QProperty(const QPropertyBinding<T> &binding)
        : QProperty()
    { operator=(binding); }
    QProperty(QPropertyBinding<T> &&binding)
        : QProperty()
    { operator=(std::move(binding)); }
#ifndef Q_CLANG_QDOC
    template <typename Functor>
    explicit QProperty(Functor &&f, const QPropertyBindingSourceLocation &location = QT_PROPERTY_DEFAULT_BINDING_LOCATION,
                       typename std::enable_if_t<std::is_invocable_r_v<T, Functor&>> * = 0)
        : QProperty(QPropertyBinding<T>(std::forward<Functor>(f), location))
    {}
#else
    template <typename Functor>
    explicit QProperty(Functor &&f);
#endif
    ~QProperty() = default;

    T value() const
    {
        if (d.priv.hasBinding())
            d.priv.evaluateIfDirty();
        d.priv.registerWithCurrentlyEvaluatingBinding();
        return d.getValue();
    }

    operator T() const
    {
        return value();
    }

    void setValue(T &&newValue)
    {
        d.priv.removeBinding();
        if (d.setValueAndReturnTrueIfChanged(std::move(newValue)))
            notify();
    }

    void setValue(const T &newValue)
    {
        d.priv.removeBinding();
        if (d.setValueAndReturnTrueIfChanged(newValue))
            notify();
    }

    QProperty<T> &operator=(T &&newValue)
    {
        setValue(std::move(newValue));
        return *this;
    }

    QProperty<T> &operator=(const T &newValue)
    {
        setValue(newValue);
        return *this;
    }

    QProperty<T> &operator=(const QPropertyBinding<T> &newBinding)
    {
        setBinding(newBinding);
        return *this;
    }

    QProperty<T> &operator=(QPropertyBinding<T> &&newBinding)
    {
        setBinding(std::move(newBinding));
        return *this;
    }

    QPropertyBinding<T> setBinding(const QPropertyBinding<T> &newBinding)
    {
        QPropertyBinding<T> oldBinding(d.priv.setBinding(newBinding, &d));
        notify();
        return oldBinding;
    }

    QPropertyBinding<T> setBinding(QPropertyBinding<T> &&newBinding)
    {
        QPropertyBinding<T> b(std::move(newBinding));
        QPropertyBinding<T> oldBinding(d.priv.setBinding(b, &d));
        notify();
        return oldBinding;
    }

    bool setBinding(const QUntypedPropertyBinding &newBinding)
    {
        if (newBinding.valueMetaType().id() != qMetaTypeId<T>())
            return false;
        d.priv.setBinding(newBinding, &d);
        notify();
        return true;
    }

#ifndef Q_CLANG_QDOC
    template <typename Functor>
    QPropertyBinding<T> setBinding(Functor &&f,
                                   const QPropertyBindingSourceLocation &location = QT_PROPERTY_DEFAULT_BINDING_LOCATION,
                                   std::enable_if_t<std::is_invocable_v<Functor>> * = nullptr)
    {
        return setBinding(Qt::makePropertyBinding(std::forward<Functor>(f), location));
    }
#else
    template <typename Functor>
    QPropertyBinding<T> setBinding(Functor f);
#endif

    bool hasBinding() const { return d.priv.hasBinding(); }

    QPropertyBinding<T> binding() const
    {
        return QPropertyBinding<T>(*this);
    }

    QPropertyBinding<T> takeBinding()
    {
        return QPropertyBinding<T>(d.priv.setBinding(QUntypedPropertyBinding(), &d));
    }

    template<typename Functor>
    QPropertyChangeHandler<Functor> onValueChanged(Functor f);
    template<typename Functor>
    QPropertyChangeHandler<Functor> subscribe(Functor f);

    const QtPrivate::QPropertyBase &propertyBase() const { return d.priv; }
private:
    void notify()
    {
        d.priv.notifyObservers(&d);
    }

    Q_DISABLE_COPY(QProperty)

    // Mutable because querying for the value may require evalating the binding expression, calling
    // non-const functions on QPropertyBase.
    mutable QtPrivate::QPropertyValueStorage<T> d;
};

namespace Qt {
    template <typename PropertyType>
    QPropertyBinding<PropertyType> makePropertyBinding(const QProperty<PropertyType> &otherProperty,
                                                       const QPropertyBindingSourceLocation &location =
                                                       QT_PROPERTY_DEFAULT_BINDING_LOCATION)
    {
        return Qt::makePropertyBinding([&otherProperty]() -> PropertyType { return otherProperty; }, location);
    }
}

template <typename T, auto Callback, auto ValueGuard=nullptr>
class QNotifiedProperty
{
public:
    using value_type = T;
    using Class = typename QtPrivate::detail::ExtractClassFromFunctionPointer<decltype(Callback)>::Class;
private:
    static bool constexpr ValueGuardModifiesArgument = std::is_invocable_r_v<bool, decltype(ValueGuard), Class, T&>;
    static bool constexpr CallbackAcceptsOldValue = std::is_invocable_v<decltype(Callback), Class, T>;
    static bool constexpr HasValueGuard = !std::is_same_v<decltype(ValueGuard), std::nullptr_t>;
public:
    static_assert(CallbackAcceptsOldValue || std::is_invocable_v<decltype(Callback), Class>);
    static_assert(
            std::is_invocable_r_v<bool, decltype(ValueGuard), Class, T> ||
            ValueGuardModifiesArgument ||
            !HasValueGuard,
        "Guard has wrong signature");
private:
    static constexpr QtPrivate::QPropertyGuardFunction GuardTE =
            QtPrivate::QPropertyGuardFunctionHelper<T, Class, ValueGuard>::guard;
public:

    QNotifiedProperty() = default;

    explicit QNotifiedProperty(const T &initialValue) : d(initialValue) {}
    explicit QNotifiedProperty(T &&initialValue) : d(std::move(initialValue)) {}

    QNotifiedProperty(Class *owner, const QPropertyBinding<T> &binding)
        : QNotifiedProperty()
    { setBinding(owner, binding); }
    QNotifiedProperty(Class *owner, QPropertyBinding<T> &&binding)
        : QNotifiedProperty()
    { setBinding(owner, std::move(binding)); }

#ifndef Q_CLANG_QDOC
    template <typename Functor>
    explicit QNotifiedProperty(Class *owner, Functor &&f, const QPropertyBindingSourceLocation &location = QT_PROPERTY_DEFAULT_BINDING_LOCATION,
                       typename std::enable_if_t<std::is_invocable_r_v<T, Functor&>> * = 0)
        : QNotifiedProperty(QPropertyBinding<T>(owner, std::forward<Functor>(f), location))
    {}
#else
    template <typename Functor>
    explicit QNotifiedProperty(Class *owner, Functor &&f);
#endif

    ~QNotifiedProperty() = default;

    T value() const
    {
        if (d.priv.hasBinding())
            d.priv.evaluateIfDirty();
        d.priv.registerWithCurrentlyEvaluatingBinding();
        return d.getValue();
    }

    operator T() const
    {
        return value();
    }

    template<typename S>
    auto setValue(Class *owner, S &&newValue) -> std::enable_if_t<!ValueGuardModifiesArgument && std::is_same_v<S, T>, void>
    {
        if constexpr (HasValueGuard) {
            if (!(owner->*ValueGuard)(newValue))
                return;
        }
        if constexpr (CallbackAcceptsOldValue) {
            T oldValue = value(); // TODO: kind of pointless if there was no change
            if (d.setValueAndReturnTrueIfChanged(std::move(newValue)))
                notify(owner, &oldValue);
        } else {
            if (d.setValueAndReturnTrueIfChanged(std::move(newValue)))
                notify(owner);
        }
        d.priv.removeBinding();
    }

    void setValue(Class *owner, std::conditional_t<ValueGuardModifiesArgument, T, const T &> newValue)
    {
        if constexpr (HasValueGuard) {
            if (!(owner->*ValueGuard)(newValue))
                return;
        }
        if constexpr (CallbackAcceptsOldValue) {
            // When newValue is T, we move it, if it's const T& it stays const T& and won't get moved
            T oldValue = value();
            if (d.setValueAndReturnTrueIfChanged(std::move(newValue)))
                notify(owner, &oldValue);
        } else {
            if (d.setValueAndReturnTrueIfChanged(std::move(newValue)))
                notify(owner);
        }
        d.priv.removeBinding();
    }

    QPropertyBinding<T> setBinding(Class *owner, const QPropertyBinding<T> &newBinding)
    {
        if constexpr (CallbackAcceptsOldValue) {
            T oldValue = value();
            QPropertyBinding<T> oldBinding(d.priv.setBinding(newBinding, &d, owner, [](void *o, void *oldVal) {
                (reinterpret_cast<Class *>(o)->*Callback)(*reinterpret_cast<T *>(oldVal));
            }, GuardTE));
            notify(owner, &oldValue);
            return oldBinding;
        } else {
            QPropertyBinding<T> oldBinding(d.priv.setBinding(newBinding, &d, owner, [](void *o, void *) {
                (reinterpret_cast<Class *>(o)->*Callback)();
            }, GuardTE));
            notify(owner);
            return oldBinding;
        }
    }

    QPropertyBinding<T> setBinding(Class *owner, QPropertyBinding<T> &&newBinding)
    {
        QPropertyBinding<T> b(std::move(newBinding));
        if constexpr (CallbackAcceptsOldValue) {
            T oldValue = value();
            QPropertyBinding<T> oldBinding(d.priv.setBinding(b, &d, owner, [](void *o, void *oldVal) {
                (reinterpret_cast<Class *>(o)->*Callback)(*reinterpret_cast<T *>(oldVal));
            }, GuardTE));
            notify(owner, &oldValue);
            return oldBinding;
        } else {
            QPropertyBinding<T> oldBinding(d.priv.setBinding(b, &d, owner, [](void *o, void *) {
                (reinterpret_cast<Class *>(o)->*Callback)();
            }, GuardTE));
            notify(owner);
            return oldBinding;
        }
    }

    bool setBinding(Class *owner, const QUntypedPropertyBinding &newBinding)
    {
        if (newBinding.valueMetaType().id() != qMetaTypeId<T>())
            return false;
        if constexpr (CallbackAcceptsOldValue) {
            T oldValue = value();
            d.priv.setBinding(newBinding, &d, owner, [](void *o, void *oldVal) {
                (reinterpret_cast<Class *>(o)->*Callback)(*reinterpret_cast<T *>(oldVal));
            }, GuardTE);
            notify(owner, &oldValue);
        } else {
            d.priv.setBinding(newBinding, &d, owner, [](void *o, void *) {
                (reinterpret_cast<Class *>(o)->*Callback)();
            }, GuardTE);
            notify(owner);
        }
        return true;
    }

#ifndef Q_CLANG_QDOC
    template <typename Functor>
    QPropertyBinding<T> setBinding(Class *owner, Functor &&f,
                                   const QPropertyBindingSourceLocation &location = QT_PROPERTY_DEFAULT_BINDING_LOCATION,
                                   std::enable_if_t<std::is_invocable_v<Functor>> * = nullptr)
    {
        return setBinding(owner, Qt::makePropertyBinding(std::forward<Functor>(f), location));
    }
#else
    template <typename Functor>
    QPropertyBinding<T> setBinding(Class *owner, Functor f);
#endif

    bool hasBinding() const { return d.priv.hasBinding(); }

    QPropertyBinding<T> binding() const
    {
        return QPropertyBinding<T>(*this);
    }

    QPropertyBinding<T> takeBinding()
    {
        return QPropertyBinding<T>(d.priv.setBinding(QUntypedPropertyBinding(), &d));
    }

    template<typename Functor>
    QPropertyChangeHandler<Functor> onValueChanged(Functor f);
    template<typename Functor>
    QPropertyChangeHandler<Functor> subscribe(Functor f);

    const QtPrivate::QPropertyBase &propertyBase() const { return d.priv; }
private:
    void notify(Class *owner, T *oldValue=nullptr)
    {
        d.priv.notifyObservers(&d);
        if constexpr (std::is_invocable_v<decltype(Callback), Class>) {
            Q_UNUSED(oldValue);
            (owner->*Callback)();
        } else {
            (owner->*Callback)(*oldValue);
        }
    }

    Q_DISABLE_COPY_MOVE(QNotifiedProperty)

    // Mutable because querying for the value may require evalating the binding expression, calling
    // non-const functions on QPropertyBase.
    mutable QtPrivate::QPropertyValueStorage<T> d;
};

struct QPropertyObserverPrivate;
struct QPropertyObserverPointer;

class Q_CORE_EXPORT QPropertyObserver
{
public:
    // Internal
    enum ObserverTag {
        ObserverNotifiesBinding,
        ObserverNotifiesChangeHandler,
        ObserverNotifiesAlias,
    };

    QPropertyObserver() = default;
    QPropertyObserver(QPropertyObserver &&other);
    QPropertyObserver &operator=(QPropertyObserver &&other);
    ~QPropertyObserver();

    template<typename Property, typename = std::enable_if_t<std::is_same_v<decltype(std::declval<Property>().propertyBase()), QtPrivate::QPropertyBase &>>>
    void setSource(const Property &property)
    { setSource(property.propertyBase()); }

protected:
    QPropertyObserver(void (*callback)(QPropertyObserver*, void *));
    QPropertyObserver(void *aliasedPropertyPtr);

    template<typename PropertyType>
    QProperty<PropertyType> *aliasedProperty() const
    {
        return reinterpret_cast<QProperty<PropertyType> *>(aliasedPropertyPtr);
    }

private:
    void setSource(const QtPrivate::QPropertyBase &property);

    QTaggedPointer<QPropertyObserver, ObserverTag> next;
    // prev is a pointer to the "next" element within the previous node, or to the "firstObserverPtr" if it is the
    // first node.
    QtPrivate::QTagPreservingPointerToPointer<QPropertyObserver, ObserverTag> prev;

    union {
        QPropertyBindingPrivate *bindingToMarkDirty = nullptr;
        void (*changeHandler)(QPropertyObserver*, void *);
        quintptr aliasedPropertyPtr;
    };

    QPropertyObserver(const QPropertyObserver &) = delete;
    QPropertyObserver &operator=(const QPropertyObserver &) = delete;

    friend struct QPropertyObserverPointer;
    friend struct QPropertyBasePointer;
    friend class QPropertyBindingPrivate;
};

template <typename Functor>
class QPropertyChangeHandler : public QPropertyObserver
{
    Functor m_handler;
public:
    QPropertyChangeHandler(Functor handler)
        : QPropertyObserver([](QPropertyObserver *self, void *) {
              auto This = static_cast<QPropertyChangeHandler<Functor>*>(self);
              This->m_handler();
          })
        , m_handler(handler)
    {
    }

    template<typename Property, typename = std::void_t<decltype(&Property::propertyBase)>>
    QPropertyChangeHandler(const Property &property, Functor handler)
        : QPropertyObserver([](QPropertyObserver *self, void *) {
              auto This = static_cast<QPropertyChangeHandler<Functor>*>(self);
              This->m_handler();
          })
        , m_handler(handler)
    {
        setSource(property);
    }
};

template <typename T>
template<typename Functor>
QPropertyChangeHandler<Functor> QProperty<T>::onValueChanged(Functor f)
{
#if defined(__cpp_lib_is_invocable) && (__cpp_lib_is_invocable >= 201703L)
    static_assert(std::is_invocable_v<Functor>, "Functor callback must be callable without any parameters");
#endif
    return QPropertyChangeHandler<Functor>(*this, f);
}

template <typename T>
template<typename Functor>
QPropertyChangeHandler<Functor> QProperty<T>::subscribe(Functor f)
{
#if defined(__cpp_lib_is_invocable) && (__cpp_lib_is_invocable >= 201703L)
    static_assert(std::is_invocable_v<Functor>, "Functor callback must be callable without any parameters");
#endif
    f();
    return onValueChanged(f);
}

template <typename T, auto Callback, auto ValueGuard>
template<typename Functor>
QPropertyChangeHandler<Functor> QNotifiedProperty<T, Callback, ValueGuard>::onValueChanged(Functor f)
{
#if defined(__cpp_lib_is_invocable) && (__cpp_lib_is_invocable >= 201703L)
    static_assert(std::is_invocable_v<Functor>, "Functor callback must be callable without any parameters");
#endif
    return QPropertyChangeHandler<Functor>(*this, f);
}

template <typename T, auto Callback, auto ValueGuard>
template<typename Functor>
QPropertyChangeHandler<Functor> QNotifiedProperty<T, Callback, ValueGuard>::subscribe(Functor f)
{
#if defined(__cpp_lib_is_invocable) && (__cpp_lib_is_invocable >= 201703L)
    static_assert(std::is_invocable_v<Functor>, "Functor callback must be callable without any parameters");
#endif
    f();
    return onValueChanged(f);
}

template<typename T>
class QPropertyAlias : public QPropertyObserver
{
    Q_DISABLE_COPY_MOVE(QPropertyAlias)
public:
    QPropertyAlias(QProperty<T> *property)
        : QPropertyObserver(property)
    {
        if (property)
            setSource(*property);
    }

    QPropertyAlias(QPropertyAlias<T> *alias)
        : QPropertyAlias(alias->aliasedProperty<T>())
    {}

    T value() const
    {
        if (auto *p = aliasedProperty<T>())
            return p->value();
        return T();
    }

    operator T() const { return value(); }

    void setValue(T &&newValue)
    {
        if (auto *p = aliasedProperty<T>())
            p->setValue(std::move(newValue));
    }

    void setValue(const T &newValue)
    {
        if (auto *p = aliasedProperty<T>())
            p->setValue(newValue);
    }

    QPropertyAlias<T> &operator=(T &&newValue)
    {
        if (auto *p = aliasedProperty<T>())
            *p = std::move(newValue);
        return *this;
    }

    QPropertyAlias<T> &operator=(const T &newValue)
    {
        if (auto *p = aliasedProperty<T>())
            *p = newValue;
        return *this;
    }

    QPropertyAlias<T> &operator=(const QPropertyBinding<T> &newBinding)
    {
        setBinding(newBinding);
        return *this;
    }

    QPropertyAlias<T> &operator=(QPropertyBinding<T> &&newBinding)
    {
        setBinding(std::move(newBinding));
        return *this;
    }

    QPropertyBinding<T> setBinding(const QPropertyBinding<T> &newBinding)
    {
        if (auto *p = aliasedProperty<T>())
            return p->setBinding(newBinding);
        return QPropertyBinding<T>();
    }

    QPropertyBinding<T> setBinding(QPropertyBinding<T> &&newBinding)
    {
        if (auto *p = aliasedProperty<T>())
            return p->setBinding(std::move(newBinding));
        return QPropertyBinding<T>();
    }

    bool setBinding(const QUntypedPropertyBinding &newBinding)
    {
        if (auto *p = aliasedProperty<T>())
            return p->setBinding(newBinding);
        return false;
    }

#ifndef Q_CLANG_QDOC
    template <typename Functor>
    QPropertyBinding<T> setBinding(Functor &&f,
                                   const QPropertyBindingSourceLocation &location = QT_PROPERTY_DEFAULT_BINDING_LOCATION,
                                   std::enable_if_t<std::is_invocable_v<Functor>> * = nullptr)
    {
        return setBinding(Qt::makePropertyBinding(std::forward<Functor>(f), location));
    }
#else
    template <typename Functor>
    QPropertyBinding<T> setBinding(Functor f);
#endif

    bool hasBinding() const
    {
        if (auto *p = aliasedProperty<T>())
            return p->hasBinding();
        return false;
    }

    QPropertyBinding<T> binding() const
    {
        if (auto *p = aliasedProperty<T>())
            return p->binding();
        return QPropertyBinding<T>();
    }

    QPropertyBinding<T> takeBinding()
    {
        if (auto *p = aliasedProperty<T>())
            return p->takeBinding();
        return QPropertyBinding<T>();
    }

    template<typename Functor>
    QPropertyChangeHandler<Functor> onValueChanged(Functor f)
    {
        if (auto *p = aliasedProperty<T>())
            return p->onValueChanged(f);
        return QPropertyChangeHandler<Functor>(f);
    }

    template<typename Functor>
    QPropertyChangeHandler<Functor> subscribe(Functor f)
    {
        if (auto *p = aliasedProperty<T>())
            return p->subscribe(f);
        return QPropertyChangeHandler<Functor>(f);
    }

    bool isValid() const
    {
        return aliasedProperty<T>() != nullptr;
    }
};
QT_END_NAMESPACE

#endif // QPROPERTY_H
