#include <iostream>

enum debug_option {
    DEBUG_DISABLE,
    DEBUG_ENABLE
};

class Debug {
    public:
        debug_option debug_state;

        Debug() : debug_state(DEBUG_ENABLE) {} // constr
        Debug(debug_option state) : debug_state(state) {} // constr

        template<typename T>
        Debug & operator<< (T input) {
            if (this->debug_state == DEBUG_ENABLE)
                std::cout << input;
            return *this;
        }

        typedef std::ostream& (*STRFUNC)(std::ostream&);
        Debug & operator<<(STRFUNC func) {
            if (this->debug_state == DEBUG_ENABLE) {  
                func(std::cout);
            }
            return *this;
        }
};