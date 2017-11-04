#ifndef _MovableEnvelope_h_
#define _MovableEnvelope_h_

// defining BOOST_RESULT_OF_USE_TR1 1 forces compilers with full decltype
// support to use boost::result_of<T> for deconstruct_movable in order to
// ensure all compilers use the boost::result_of<T>.  All of the
// boost::result_of<T> framework can be removed when all supported compilers
// provide full decltype support.
#define  BOOST_RESULT_OF_USE_TR1_WITH_DECLTYPE_FALLBACK 1

#include "../util/Logger.h"

#include <boost/spirit/include/support_argument.hpp>

#include <memory>
#include <type_traits>

namespace parse { namespace detail {
    /** \p MovableEnvelope enables the boost::spirit parser to handle a
        \p T with move semantics.

        boost::spirit is designed only work with value semantics. The
        boost::phoenix actors only handle value semantics.  Types with move
        only semantics like unique_ptr will not work as expected.

        boost::spirit is designed to work with compile time polymorphism.  With
        run-time polymorphism using raw pointers to track heap objects, each
        time the parser backtracks it leaks memory.

        \p MovableEnvelope makes a \p unique_ptr<T> with move only semantics
        appear to have copy semantics, by moving it each time that it is copied.
        This allows boost::phoenix actors to handle it correctly if the movement is
        one way from creation at the parse location to consumption in a larger
        parsed component.

        \p MovableEnvelope is a work around.  It can be removed if
        boost::spirit supports move semantics, or the parser is changed to use
        compile time polymorphism.
    */
    template <typename T>
    class MovableEnvelope {
    public:
        using enveloped_type = T;

        /** \name Rule of Five constructors and operators.
            MovableEnvelope satisfies the rule of five with the following
            constructors and operator=().
         */ //@{

        // Default constructor
        MovableEnvelope() {}

        // Copy constructor
        // This leaves \p other in an emptied state
        MovableEnvelope(const MovableEnvelope& other) :
            MovableEnvelope(std::move(other.obj))
        {}

        // Move constructor
        MovableEnvelope(MovableEnvelope&& other) :
            MovableEnvelope(std::move(other.obj))
        {}

        // Move operator
        MovableEnvelope& operator= (MovableEnvelope&& other) {
            obj = std::move(other.obj);
            original_obj = other.original_obj;
            // Intentionally leave other.original_obj != other.obj.get()
            return *this;
        }

        // Copy operator
        // This leaves \p other in an emptied state
        MovableEnvelope& operator= (const MovableEnvelope& other) {
            obj = std::move(other.obj);
            original_obj = other.original_obj;
            // Intentionally leave other.original_obj != other.obj.get()
            return *this;
        }
        //@}

        virtual ~MovableEnvelope() {};

        /** \name Converting constructors and operators.
            MovableEnvelope allows conversion between compatible types with the following
            constructors and operators.
         */ //@{

        // nullptr constructor
        MovableEnvelope(std::nullptr_t) {}

        template <typename U,
                  typename std::enable_if<std::is_convertible<std::unique_ptr<U>,
                                                              std::unique_ptr<T>>::value>::type* = nullptr>
        explicit MovableEnvelope(std::unique_ptr<U>&& obj_) :
            obj(std::move(obj_)),
            original_obj(obj.get())
        {}

        // This takes ownership of obj_
        template <typename U,
                  typename std::enable_if<std::is_convertible<std::unique_ptr<U>,
                                                              std::unique_ptr<T>>::value>::type* = nullptr>
        explicit MovableEnvelope(U* obj_) :
            obj(obj_),
            original_obj(obj.get())
        {}

        // Converting copy constructor
        // This leaves \p other in an emptied state
        template <typename U,
                  typename std::enable_if<std::is_convertible<std::unique_ptr<U>,
                                                              std::unique_ptr<T>>::value>::type* = nullptr>
        MovableEnvelope(const MovableEnvelope<U>& other) :
            MovableEnvelope(std::move(other.obj))
        {}

        // Converting move constructor
        template <typename U,
                  typename std::enable_if<std::is_convertible<std::unique_ptr<U>,
                                                              std::unique_ptr<T>>::value>::type* = nullptr>
        MovableEnvelope(MovableEnvelope<U>&& other) :
            MovableEnvelope(std::move(other.obj))
        {}

        template <typename U,
                  typename std::enable_if<std::is_convertible<std::unique_ptr<U>,
                                                              std::unique_ptr<T>>::value>::type* = nullptr>
        MovableEnvelope& operator= (MovableEnvelope<U>&& other) {
            obj = std::move(other.obj);
            original_obj = other.original_obj;
            // Intentionally leave other.original_obj != other.obj.get()
            return *this;
        }

        // Copy operator
        // This leaves \p other in an emptied state
        template <typename U,
                  typename std::enable_if<std::is_convertible<std::unique_ptr<U>,
                                                              std::unique_ptr<T>>::value>::type* = nullptr>
        MovableEnvelope& operator= (const MovableEnvelope<U>& other) {
            obj = std::move(other.obj);
            original_obj = other.original_obj;
            // Intentionally leave other.original_obj != other.obj.get()
            return *this;
        }
        //@}

        // Return false if the original \p obj has been moved away from this
        // MovableEnvelope.
        bool IsEmptiedEnvelope() const
        { return (original_obj != obj.get());}

        /** OpenEnvelope returns the enclosed \p obj and throws an expectation
            exception if the wrapped pointer was already moved out of this \p
            MovableEnvelope.

            \p OpenEnvelope is a one-shot.  Calling OpenEnvelope a second time
            will throw, since the obj has already been removed.

            \p pass should refer to qi::_pass_type to facilitate the
            expectation exception.
        */

        std::unique_ptr<T> OpenEnvelope(bool& pass) const {
            if (IsEmptiedEnvelope()) {
                ErrorLogger() <<
                    "The parser attempted to extract the unique_ptr from a MovableEnvelope more than once. "
                    "Until boost::spirit supports move semantics MovableEnvelope requires that unique_ptr be used only once. "
                    "Check that a parser is not back tracking over an actor containing an opened MovableEnvelope. "
                    "Check that set, map or vector parses are not repeatedly extracting the same unique_ptr<T>.";

                pass = false;
            }
            return std::move(obj);
        }

    private:
        template <typename U>
        friend class MovableEnvelope;

        mutable std::unique_ptr<T> obj = nullptr;

        mutable T* original_obj = nullptr;
    };

    /** \p construct_movable is a functor that constructs a MovableEnvelope<T> */
    struct construct_movable {
        template <typename T>
        using result_type = MovableEnvelope<T>;

        template <typename T>
        result_type<T> operator() (T* obj) const
        { return MovableEnvelope<T>(obj); }

        template <typename T>
        result_type<T> operator() (std::unique_ptr<T>&& obj) const
        { return MovableEnvelope<T>(std::move(obj)); }

        template <typename T>
        result_type<T> operator() (std::unique_ptr<T>& obj) const
        { return MovableEnvelope<T>(std::move(obj)); }

        template <typename T>
        result_type<T> operator() (MovableEnvelope<T>&& obj) const
        { return MovableEnvelope<T>(std::move(obj)); }

        template <typename T>
        result_type<T> operator() (const MovableEnvelope<T>& obj) const
        { return MovableEnvelope<T>(obj); }
    };

    /** Free functions converting containers of MovableEnvelope to unique_ptrs. */
    template <typename T>
    std::vector<std::unique_ptr<T>> OpenEnvelopes(const std::vector<MovableEnvelope<T>>& envelopes, bool& pass) {
        std::vector<std::unique_ptr<T>> retval;
        for (auto&& envelope : envelopes)
            retval.push_back(envelope.OpenEnvelope(pass));
        return retval;
    }

    template <typename T>
    std::vector<std::pair<std::string, std::unique_ptr<T>>> OpenEnvelopes(
        const std::vector<std::pair<std::string, MovableEnvelope<T>>>& in, bool& pass)
    {
        std::vector<std::pair<std::string, std::unique_ptr<T>>> retval;
        for (auto&& name_and_value : in)
            retval.emplace_back(name_and_value.first, name_and_value.second.OpenEnvelope(pass));
        return retval;
    }

    template <typename K, typename V>
    std::map<K, std::unique_ptr<V>> OpenEnvelopes(const std::map<K, MovableEnvelope<V>>& in, bool& pass) {
        std::map<K, std::unique_ptr<V>> retval;
        for (auto&& name_and_value : in)
            retval.insert(std::make_pair(name_and_value.first, name_and_value.second.OpenEnvelope(pass)));
        return std::move(retval);
    }

    /* deconstructed_type is only used to workaround compilers without robust
       decltype support. */
    template <typename T> struct deconstructed_type;

    template <typename T>
    struct deconstructed_type<MovableEnvelope<T>> {
        using type = std::unique_ptr<T>;
    };
    template <typename T>
    struct deconstructed_type<std::vector<MovableEnvelope<T>>> {
        using type = std::vector<std::unique_ptr<T>>;
    };
    template <typename T>
    struct deconstructed_type<std::vector<std::pair<std::string, MovableEnvelope<T>>>> {
        using type = std::vector<std::pair<std::string, std::unique_ptr<T>>>;
    };

    /** \p deconstruct_movable is a functor that extracts the unique_ptr from a
        MovableEnvelope<T>.  This is a one time operation that empties the
        MovableEnvelope<T>.  It is typically done while calling the constructor
        from outside of boost::spirit that expects a unique_ptr<T>*/
    class deconstruct_movable {
    public:
        /** Note: The definition of result and the dependent definitions
           satisfy the boost::result_of framework for compilers with less
           robust decltype support.  Once all supported compilers provide full
           decltype support, it can be removed. leaving only the operator() members. */
        template <typename> struct result;

        template <typename F, typename MovableEnvelope_T, typename Pass>
        struct result<F(MovableEnvelope_T&, Pass&)> {
            using type = typename deconstructed_type<typename std::decay<MovableEnvelope_T>::type>::type;
        };

        template <typename F, typename MovableEnvelope_T, typename Pass>
        struct result<const F(MovableEnvelope_T&, Pass&)> {
            using type = typename deconstructed_type<typename std::decay<MovableEnvelope_T>::type>::type;
        };

        template <typename T>
        typename result<const deconstruct_movable(
            const MovableEnvelope<T>&,     bool&)>::type operator()
        (
            const MovableEnvelope<T>& obj, bool& pass) const
        { return obj.OpenEnvelope(pass); }

        template <typename T>
        typename result<const deconstruct_movable(
            const std::vector<MovableEnvelope<T>>&,      bool&)>::type operator()
        (
            const std::vector<MovableEnvelope<T>>& objs, bool& pass) const
        { return OpenEnvelopes(objs, pass); }

        template <typename T>
        typename result<const deconstruct_movable(
            const std::vector<std::pair<std::string, MovableEnvelope<T>>>&,      bool&)>::type operator()
        (
            const std::vector<std::pair<std::string, MovableEnvelope<T>>>& objs, bool& pass) const
        { return OpenEnvelopes(objs, pass); }
    };

    } // end namespace detail
} // end namespace parse

#endif // _MovableEnvelope_h_
