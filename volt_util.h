//#ifndef VOLT_UTIL_H
//#define VOLT_UTIL_H
#pragma once

#include <atomic>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <vector>
#include <deque>
#include <queue>
#include <list>
#include <string>
#include <chrono>
#include <algorithm>
#include <memory>
#include <future>
#include <random>
#include <any>
#include <optional>
#include <execution>
#include <thread>
#include <stdexcept>
#include <unordered_map>
#include <map>
#include <filesystem>
#include <functional>
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <bitset>
#include <clocale>
#include <charconv>
#include <cstring>
#include <locale>
#include <codecvt>
#include <type_traits>
#include <typeindex>
#ifdef _MSC_VER
#include <ranges>
//#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(__linux__)
#include <dirent.h>
#include <sys/stat.h>
#endif
#include <ctime>

//#define FMT_HEADER_ONLY

enum class DeviceDisplayType {
    Unknown,
    Tablet,
    Tv,
    Medium,
    Small,
    UnSupported,
};

enum class TextWrapStyle {
    MAX_CHARS_PER_LN,
    MAX_WORDS_PER_LN
};

enum class PERFOMANCE_MODE {
    NORMAL,
    BATTERY_SAVER,
    HIGH
};

enum class Orientation {
    VERTICAL,
    HORIZONTAL,
    ANGLED
};

enum class Gravity {
    Center,
    Left,
    Right,
    TL,
    TM,
    TR,
    BL,
    BM,
    BR,
    FILL
};

enum class EdgeType {
    RECT,
    RADIAL
};


enum class FontStyle {
    Normal,
    Bold,
    Italic,
    Underline,
    StrikeThrough,
    BoldUnderline,
    BoldStrikeThrough,
    ItalicBold,
    ItalicUnderline,
    ItalicStrikeThrough,
    Custom
};


enum class IMAGE_LD_STYLE {
    NORMAL,
    CUSTOM_TEXTURE,
    ASYNC_PATH,
    ASYNC_CUSTOM_SURFACE_LOADER,
    ASYNC_DEFAULT_TEXTURE_PATH,
    ASYNC_DEFAULT_TEXTURE_CUSTOM_LOADER
};

enum class PixelSystem {
    NORMAL,
    PERCENTAGE
};

enum class ScrollDirection {
    VERTICAL,
    HORIZONTAL,
};

enum class GRIDCELL_PLACEMENT_POLICY {
    LINEAR_STACK,
    STAGGERED
};

template<class T>
struct v2d_generic {
    T x = 0;
    T y = 0;

    v2d_generic() : x(0), y(0) {}

    v2d_generic(T _x, T _y) : x(_x), y(_y) {}

    v2d_generic(const v2d_generic& v) : x(v.x), y(v.y) {}

    v2d_generic& operator=(const v2d_generic& v) = default;

    constexpr T mag() const { return T(std::sqrt(x * x + y * y)); }

    constexpr T mag2() const { return x * x + y * y; }

    v2d_generic norm() const {
        T r = 1 / mag();
        return v2d_generic(x * r, y * r);
    }

    v2d_generic perp() const { return v2d_generic(-y, x); }

    v2d_generic floor() const { return v2d_generic(std::floor(x), std::floor(y)); }

    v2d_generic ceil() const { return v2d_generic(std::ceil(x), std::ceil(y)); }

    constexpr v2d_generic max(const v2d_generic& v) const {
        return v2d_generic(std::max(x, v.x), std::max(y, v.y));
    }

    constexpr v2d_generic min(const v2d_generic& v) const {
        return v2d_generic(std::min(x, v.x), std::min(y, v.y));
    }

    v2d_generic cart() { return { std::cos(y) * x, std::sin(y) * x }; }

    v2d_generic polar() { return { mag(), std::atan2(y, x) }; }

    T dot(const v2d_generic& rhs) const { return this->x * rhs.x + this->y * rhs.y; }

    T cross(const v2d_generic& rhs) const { return this->x * rhs.y - this->y * rhs.x; }

    constexpr v2d_generic operator+(const v2d_generic& rhs) const {
        return v2d_generic(this->x + rhs.x, this->y + rhs.y);
    }

    constexpr v2d_generic operator-(const v2d_generic& rhs) const {
        return v2d_generic(this->x - rhs.x, this->y - rhs.y);
    }

    constexpr v2d_generic operator*(const T& rhs) const {
        return v2d_generic(this->x * rhs, this->y * rhs);
    }

    constexpr v2d_generic operator*(const v2d_generic& rhs) const {
        return v2d_generic(this->x * rhs.x, this->y * rhs.y);
    }

    constexpr v2d_generic operator/(const T& rhs) const {
        return v2d_generic(this->x / rhs, this->y / rhs);
    }

    constexpr v2d_generic operator/(const v2d_generic& rhs) const {
        return v2d_generic(this->x / rhs.x, this->y / rhs.y);
    }

    v2d_generic& operator+=(const v2d_generic& rhs) {
        this->x += rhs.x;
        this->y += rhs.y;
        return *this;
    }

    v2d_generic& operator-=(const v2d_generic& rhs) {
        this->x -= rhs.x;
        this->y -= rhs.y;
        return *this;
    }

    v2d_generic& operator*=(const T& rhs) {
        this->x *= rhs;
        this->y *= rhs;
        return *this;
    }

    v2d_generic& operator/=(const T& rhs) {
        this->x /= rhs;
        this->y /= rhs;
        return *this;
    }

    v2d_generic& operator*=(const v2d_generic& rhs) {
        this->x *= rhs.x;
        this->y *= rhs.y;
        return *this;
    }

    v2d_generic& operator/=(const v2d_generic& rhs) {
        this->x /= rhs.x;
        this->y /= rhs.y;
        return *this;
    }

    v2d_generic operator+() const { return { +x, +y }; }

    v2d_generic operator-() const { return { -x, -y }; }

    bool operator==(const v2d_generic& rhs) const {
        return (this->x == rhs.x && this->y == rhs.y);
    }

    bool operator!=(const v2d_generic& rhs) const {
        return (this->x != rhs.x || this->y != rhs.y);
    }

    auto str() const -> const std::string
    {
        return std::string("(") + std::to_string(this->x) + "," + std::to_string(this->y) + ")";
    }

    friend std::ostream& operator<<(std::ostream& os, const v2d_generic& rhs) {
        os << rhs.str();
        return os;
    }

    operator v2d_generic<int32_t>() const {
        return { static_cast<int32_t>(this->x), static_cast<int32_t>(this->y) };
    }

    operator v2d_generic<float>() const {
        return { static_cast<float>(this->x), static_cast<float>(this->y) };
    }

    operator v2d_generic<double>() const {
        return { static_cast<double>(this->x), static_cast<double>(this->y) };
    }
};


constexpr float degreesToRad(const int& degrees){
    return static_cast<float>(degrees) * 0.0174533f;
}

template<typename T, typename param>
class NamedType {
public:
    explicit NamedType(T const& value) : value_(value) {}

    explicit NamedType(T&& value) : value_(std::move(value)) {}

    //T& get() { return value_; }

    [[nodiscard]] constexpr auto get() const noexcept { return value_; }

private:
    T value_;
};


template<typename T>
class Width {
public:
    explicit Width(T const& value) : value_(value) {}

    explicit Width(T&& value) : value_(std::move(value)) {}

    //T& get() { return value_.get(); }

    [[nodiscard]] constexpr auto get() const noexcept { return value_.get(); }

private:
    NamedType<T, struct widthparam> value_;
};

template<typename T>
class Height {
public:
    explicit Height(T const& value) : value_(value) {}

    explicit Height(T&& value) : value_(std::move(value)) {}

    //T& get() { return value_.get(); }

    [[nodiscard]] constexpr auto get() const noexcept { return value_.get(); }

private:
    NamedType<T, struct widthparam> value_;
};

template<typename T>
class MilliSec {
public:
    explicit MilliSec(T const& value) : value_(value) {}

    explicit MilliSec(T&& value) : value_(std::move(value)) {}

    //T& get() { return value_.get(); }

    [[nodiscard]] constexpr auto get() const noexcept { return value_.get(); }

private:
    NamedType<T, struct widthparam> value_;
};




namespace Dict{
    using dict = std::map<std::type_index,std::any>;

    template<class Name, class T>
    struct key final{ explicit key()=default; };

    template<class Name, class T>
    auto get(const dict& d, key<Name,T> k)->std::optional<T>
    {
        if (auto pos = d.find(typeid(k)); pos!=d.end()){
            return std::any_cast<T>(pos->second);
        }
        return {};
    }

    template<class Name, class T, class V>
    void set(dict& d, key<Name,T> k, V&& value)
    {
        constexpr bool convertible= std::is_convertible_v<V,T>;
        static_assert(convertible);
        if constexpr(convertible){
            d.insert_or_assign(typeid(k), T{std::forward<V>(value)});
        }
    }

    namespace example{
        using age_key=key<struct _age_,int>;
        using gender_key=key<struct _gender_, std::pair<float,float>>;
        using name_key=key<struct _name_,std::string>;

        constexpr inline auto age=age_key{};
        constexpr inline auto gender=gender_key{};
        constexpr inline auto name=name_key{};

        auto person=dict{};

        /*set(person, name, "Florb");
        set(person, age, 18);
        set(person, gender, std::pair{0.5f,1.f});

        const auto a=get(person,age);
        const auto n=get(person,name);
        const auto g=get(person,gender);

        std::cout<<"name: "<<*n<<std::endl;*/



    }
}

//get current date YYYY-MM-DD
std::string getDateStr() {
    const std::chrono::time_point<std::chrono::system_clock> now{ std::chrono::system_clock::now() };
    // Get the year, month, and day
    const std::chrono::year_month_day ymd{ std::chrono::floor<std::chrono::days>(now) };
    std::stringstream dateTimeAdded;
    dateTimeAdded << ymd;
    return dateTimeAdded.str();
}

//get current date YYYY-MM-DD HH:MM:SS
std::string getDateAndTimeStr(){
    const std::chrono::time_point<std::chrono::system_clock> now{ std::chrono::system_clock::now() };
    // Get the year, month, and day
    const std::chrono::year_month_day ymd{ std::chrono::floor<std::chrono::days>(now) };

    // Get the time
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&tt);
    std::stringstream timeAdded;
    timeAdded << std::setfill('0') << std::setw(2) << tm->tm_hour << ":"
              << std::setfill('0') << std::setw(2) << tm->tm_min << ":"
              << std::setfill('0') << std::setw(2) << tm->tm_sec;
    // Combine date and time
    std::stringstream dateTimeAdded;
    dateTimeAdded << ymd << " " << timeAdded.str();
    return dateTimeAdded.str();
}

//get date YYYY-MM-DD from string
std::chrono::year_month_day stringToYmd(const std::string& dateTimeStr) {
    std::istringstream ss(dateTimeStr);
    std::tm tm = {};
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    std::chrono::system_clock::time_point tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    return std::chrono::year_month_day{ std::chrono::floor<std::chrono::days>(tp) };
}


// the number of characters in a multibyte string is the sum of mblen()'s
// note: the simpler approach is std::mbstowcs(nullptr, s.c_str(), s.size())

/*std::size_t strlen_mb(const std::string_view s)
{
    std::size_t result = 0;
    const char* ptr = s.data();
    const char* end = ptr + s.size();
    std::mblen(nullptr, 0); // reset the conversion state
    while (ptr < end) {
        int next = std::mblen(ptr, end - ptr);
        if (next == -1) {
            throw std::runtime_error("strlen_mb(): conversion error");
        }
        ptr += next;
        ++result;
    }
    return result;
}*/

std::string replaceStringAll(std::string str, const std::string& replace, const std::string& with) {
    if (!replace.empty()) {
        std::size_t pos = 0;
        while ((pos = str.find(replace, pos)) != std::string::npos) {
            str.replace(pos, replace.length(), with);
            pos += with.length();
        }
    }
    return str;
}

namespace Async {
    class ThreadPool {
    public:
        ThreadPool():stop(false){
        }
        
        ThreadPool(size_t numThreads):stop(false){
            init(numThreads);
        }

        ~ThreadPool(){
            kill_all();
        }

        void kill_all() {
            {
                std::unique_lock<std::mutex> lock(this->queue_mutex);
                stop = true;
            }
            condition.notify_all();
            for (std::thread& worker : workers) {
                worker.join();
            }
        }

        void init(size_t numThreads) {
            numThreads = std::clamp(numThreads, size_t(1), size_t(1000000));
            for (size_t i = 0; i < numThreads; ++i) {
                workers.emplace_back([this] {
                    while (true) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            this->condition.wait(lock, [this] {return this->stop || !this->tasks.empty(); });
                            if (this->stop && this->tasks.empty())return;
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }
                        task();
                    }
                    });
            }
        }

        template<class F, class... Args>
        void runInterval(int _interval, std::function<bool()> _stop_source,F&& func_, Args&&... args) {
            using return_type = std::invoke_result_t<F, Args...>;
            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(func_), std::forward<Args>(args)...)
                );
                //clarity for this worker's lifetime is required to avoid potential dead locks
                //should probably put the thread obj in a container like a que or a vector
                //this needs to live until timeout or stop token invoke
                std::thread intervalWorker([this,_interval,_stop_source] {
                    while (not _stop_source()) {
                        task();
                    }
                    });
        }

       template<class F, class... Args>
        auto enqueue(F&& f, Args&&... args)
            -> std::future<std::invoke_result_t<F,Args...>> {
            using return_type = std::invoke_result_t<F, Args...>;
            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
                );
            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                if (stop) 
                    throw std::runtime_error("enqueue on stopped thread pool");
                tasks.emplace([task]() {(*task)(); });
            }
            condition.notify_one();
            return res;
        }

        /*template <class R, class T, class... Args>
        auto enqueue(R (T::* func)(Args...), T* obj, Args&&... args) 
            -> std::future<std::invoke_result_t<R (T::*)(Args...), T*, Args...>> {
            using return_type = std::invoke_result_t<R (T::*)(Args...), T*, Args...>;

            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(func, obj, std::forward<Args>(args)...)
                );

            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                if (stop)
                    throw std::runtime_error("enqueue on stopped ThreadPool");

                tasks.emplace([task]() { (*task)(); });
            }
            condition.notify_one();
            return res;
        }*/
        

    private:
        std::vector<std::thread> workers;
        //std::vector<std::thread> intervalWorkers;
        std::queue<std::function<void()>> tasks;
        std::mutex queue_mutex;
        std::condition_variable condition;
        bool stop;
    };

    ThreadPool GThreadPool(std::thread::hardware_concurrency());

    std::function<bool()> defaultTrue = []()
    { return true; };

    ///NOTES: should return a handler for the dispatched recuring task(the handler can be used for control like killing the task if not needed anymore)
    /// should have a lifetime param default = infinite(this param can be a lambda/functor that returns a boolean. the lambda is invoked after avery invoke
    //calls the given method after the given interval
    //param for number of iterations before termination
    //consider making the stop source param the result of the previous return

    /*template<class F, class... Args>
    void setInterval(int interval_, std::function<bool()> stop_source_,F&& func_, Args&&... args){
        using return_type = std::invoke_result_t<F, Args...>;
            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(func_), std::forward<Args>(args)...)
                );
            //task();
    }*/


void setInterval(std::function<void(void)> func, unsigned int _interval, std::function<bool(void)> _stop_source=defaultTrue) {
    /*std::thread(func, interval, stopSource {
        while (!stopSource()) {
            func();
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
    }).detach();*/

    /*std::thread intervalWorker([func, _interval, _stop_source] {
        while (_stop_source()) {
            func();
            std::this_thread::sleep_for(std::chrono::milliseconds(_interval));
        }
    }).detach();*/
}




};

class TextProcessor {
public:
    TextProcessor(const TextProcessor &) = delete;

    TextProcessor(const TextProcessor &&) = delete;

    size_t word_count = 0;

public:
    static TextProcessor &Get() {
        static TextProcessor *instance = new TextProcessor();
        return *instance;
    }

public:

    void WrapRanges(const std::string& _text, std::deque<std::string>* _wraped_text,
        const int& _max_char_per_line, int _max_lines, const TextWrapStyle& tws)
    {
#ifdef _MSC_VER
        auto split_strings = std::string_view{ _text } | std::ranges::views::split(' ');
        for (const auto& _string : split_strings) {
            _wraped_text->emplace_back(std::string_view{ _string.begin(),_string.end() });
        }
#endif
    }

    void u8wrap_max_char(const std::u8string& _text, std::deque<std::u8string>* _wrapped_text,
        const std::size_t& _max_char_per_line, const std::size_t& _max_lines) {
        //SDL_Log("RCV_TEXT: %s", (char*)_text.c_str());
        if (_text.empty())return;
        for (int i = 0; i < _max_lines; ++i) {
            /*try {*/
                if (i * _max_char_per_line > _text.size())i = _max_lines;
                else
                    _wrapped_text->emplace_back(_text.substr(i * _max_char_per_line, _max_char_per_line));
            /*}
            catch (const std::exception& excp) {
                std::cout << excp.what() << std::endl;
            }*/
        }
    }

    void u8wrap_max_word_count(const std::u8string& _text, std::deque<std::u8string>* _wraped_text,const std::u8string_view _delimiters,
        const std::size_t& _max_char_per_line, const std::size_t& _max_lines=0)
    {
        std::u8string line = u8"", word = u8"";
        //std::vector<const std::string::size_type> delimiter_pos;
        size_t pos_in_text_ = 0;
        bool is_first_delim = true;
        bool wrap_done = false;
        while (!wrap_done)
        {
            is_first_delim = true;
            std::u8string::size_type nearest_delim_pos = 0;
            for (const auto& _del : _delimiters) //delimiter_pos.push_back(_text.find(_del, pos_in_text_));
            {
                const auto pos = _text.find(_del, pos_in_text_);
                if (!is_first_delim)
                {
                    if (pos <= nearest_delim_pos)nearest_delim_pos = pos;
                }
                else nearest_delim_pos = pos, is_first_delim = false;
            }

            if (nearest_delim_pos != std::u8string::npos)
            {
                //check if the current line size plus the new sub-string is less than max chars per line
                //if so then add the substring at the back of the line
                if (line.size()+(nearest_delim_pos - pos_in_text_) <= _max_char_per_line)
                {
                    line += _text.substr(pos_in_text_, nearest_delim_pos + 1 - pos_in_text_);
                    pos_in_text_ = nearest_delim_pos + 1;
                    
                }
                else
                {
                    //we have a line with the max possible words so push the line into the output container
                    //then empty the line for new line processing

                    //if line is empty, then the line is not splitable so add the whole max_chars_per_line line into the output container
                    if (line.empty()) {
                        line = _text.substr(pos_in_text_, _max_char_per_line);
                        pos_in_text_ += _max_char_per_line;
                    }
                    _wraped_text->emplace_back(line);
                    //SDL_Log("mxc:%d - LNS:%d - NL:%s", _max_char_per_line, line.size(), line.c_str());
                    line.clear();
                    //check if next line is splitable if not add the whole max_chars_per_line line into the output container
                    
                }
            }
            else
            {
                if (!line.empty())
                {
                    if (line.size() < _max_char_per_line)
                    {
                        //SDL_Log("pt: %d/%d", pos_in_text_, _text.size());
                        const std::size_t rem_text_size = _max_char_per_line - line.size();
                        line += _text.substr(pos_in_text_, rem_text_size);
                        pos_in_text_ += rem_text_size+1;
                        pos_in_text_ = std::clamp(pos_in_text_, (std::size_t)0, _text.size());
                        //SDL_Log("pt: %d/%d", pos_in_text_, _text.size());
                        //SDL_Log("lns: %d/%d", line.size(), _max_char_per_line);
                    }
                    _wraped_text->emplace_back(line);
                }
                u8wrap_max_char(_text.substr(pos_in_text_, _text.size()), _wraped_text, _max_char_per_line, _max_lines);
                
                /*SDL_Log("RCV_TEXT: %s", (char*)_wraped_text->back().c_str());
                SDL_Log("SND_TEXT: %d/%d", _text.size(), pos_in_text_);
                SDL_Log("MX_CL: %d", _max_char_per_line);*/
                //if (_wraped_text->back().size() < _max_char_per_line) {
                //    const std::size_t rem_text_size = _max_char_per_line - _wraped_text->back().size();
                //    _wraped_text->back()+=(_text.substr(pos_in_text_, rem_text_size));
                //    SDL_Log("RCV_TEXT: %s", (char*)_wraped_text->back().c_str());
                //    SDL_Log("BC_SZ:: %d", _wraped_text->back().size());
                //    pos_in_text_ += rem_text_size;
                //}
                ////SDL_Log("wrap max char");
                //SDL_Log("SND_TEXT: %d/%d", _text.size(), pos_in_text_);
                ////wrap max char
                //try {
                //    u8wrap_max_char(_text.substr(pos_in_text_, _text.size()), _wraped_text, _max_char_per_line, _max_lines);
                //}
                //catch (std::exception exp) {
                //    std::cout << exp.what() << std::endl;
                //}
                wrap_done = true;
            }

        }
    }

    void wrap_max_char(const std::string& _text, std::deque<std::string>* _wrapped_text,
        const std::size_t& _max_char_per_line, const std::size_t& _max_lines) {
        //SDL_Log("RCV_TEXT: %s", (char*)_text.c_str());
        if (_text.empty())return;
        for (int i = 0; i < _max_lines; ++i) {
            /*try {*/
            if (i * _max_char_per_line > _text.size())i = _max_lines;
            else
                _wrapped_text->emplace_back(_text.substr(i * _max_char_per_line, _max_char_per_line));
            /*}
            catch (const std::exception& excp) {
                std::cout << excp.what() << std::endl;
            }*/
        }
    }
    
    void wrap_max_word_count(const std::string& _text, std::deque<std::string>* _wraped_text,const std::string_view _delimiters,
        const std::size_t& _max_char_per_line, const std::size_t& _max_lines=0)
    {
        std::string line = "", word = "";
        //std::vector<const std::string::size_type> delimiter_pos;
        size_t pos_in_text_ = 0;
        bool is_first_delim = true;
        bool wrap_done = false;
        while (!wrap_done)
        {
            is_first_delim = true;
            std::string::size_type nearest_delim_pos = 0;
            for (const auto& _del : _delimiters) //delimiter_pos.push_back(_text.find(_del, pos_in_text_));
            {
                const auto pos = _text.find(_del, pos_in_text_);
                if (!is_first_delim)
                {
                    if (pos <= nearest_delim_pos)nearest_delim_pos = pos;
                }
                else nearest_delim_pos = pos, is_first_delim = false;
            }

            if (nearest_delim_pos != std::string::npos)
            {
                //check if the current line size plus the new sub-string is less than max chars per line
                //if so then add the substring at the back of the line
                if (line.size()+(nearest_delim_pos - pos_in_text_) <= _max_char_per_line)
                {
                    line += _text.substr(pos_in_text_, nearest_delim_pos + 1 - pos_in_text_);
                    pos_in_text_ = nearest_delim_pos + 1;
                    
                }
                else
                {
                    //we have a line with the max possible words so push the line into the output container
                    //then empty the line for new line processing

                    //if line is empty, then the line is not splitable so add the whole max_chars_per_line line into the output container
                    if (line.empty()) {
                        line = _text.substr(pos_in_text_, _max_char_per_line);
                        pos_in_text_ += _max_char_per_line;
                    }
                    _wraped_text->emplace_back(line);
                    //SDL_Log("mxc:%d - LNS:%d - NL:%s", _max_char_per_line, line.size(), line.c_str());
                    line.clear();
                    //check if next line is splitable if not add the whole max_chars_per_line line into the output container
                    
                }
            }
            else
            {
                if (!line.empty())
                {
                    if (line.size() < _max_char_per_line)
                    {
                        //SDL_Log("pt: %d/%d", pos_in_text_, _text.size());
                        const std::size_t rem_text_size = _max_char_per_line - line.size();
                        line += _text.substr(pos_in_text_, rem_text_size);
                        pos_in_text_ += rem_text_size+1;
                        pos_in_text_ = std::clamp(pos_in_text_, (std::size_t)0, _text.size());
                        //SDL_Log("pt: %d/%d", pos_in_text_, _text.size());
                        //SDL_Log("lns: %d/%d", line.size(), _max_char_per_line);
                    }
                    _wraped_text->emplace_back(line);
                }
                wrap_max_char(_text.substr(pos_in_text_, _text.size()), _wraped_text, _max_char_per_line, _max_lines);
                
                /*SDL_Log("RCV_TEXT: %s", (char*)_wraped_text->back().c_str());
                SDL_Log("SND_TEXT: %d/%d", _text.size(), pos_in_text_);
                SDL_Log("MX_CL: %d", _max_char_per_line);*/
                //if (_wraped_text->back().size() < _max_char_per_line) {
                //    const std::size_t rem_text_size = _max_char_per_line - _wraped_text->back().size();
                //    _wraped_text->back()+=(_text.substr(pos_in_text_, rem_text_size));
                //    SDL_Log("RCV_TEXT: %s", (char*)_wraped_text->back().c_str());
                //    SDL_Log("BC_SZ:: %d", _wraped_text->back().size());
                //    pos_in_text_ += rem_text_size;
                //}
                ////SDL_Log("wrap max char");
                //SDL_Log("SND_TEXT: %d/%d", _text.size(), pos_in_text_);
                ////wrap max char
                //try {
                //    u8wrap_max_char(_text.substr(pos_in_text_, _text.size()), _wraped_text, _max_char_per_line, _max_lines);
                //}
                //catch (std::exception exp) {
                //    std::cout << exp.what() << std::endl;
                //}
                wrap_done = true;
            }

        }
    }

    void Wrap(const std::string &_text, std::deque<std::string> *_wraped_text,
              const int &_max_char_per_line, int _max_lines, const TextWrapStyle &tws) {
        //auto start = std::chrono::high_resolution_clock::now();
        std::string line = "", word = "";
        if (tws == TextWrapStyle::MAX_WORDS_PER_LN) {
            int pos_in_txt = 0;
            int idf_pos = 0;
            bool wrap_done = false;
            while (_wraped_text->size() <= _max_lines) {
                //whitespace pos
                auto ws_pos = _text.find(" ", pos_in_txt);
                auto hy_pos = _text.find("-", pos_in_txt);
                if (ws_pos != std::string::npos || hy_pos != std::string::npos) {
                    if (ws_pos != std::string::npos)idf_pos = ws_pos;
                    if (hy_pos != std::string::npos)idf_pos = hy_pos;
                    if (ws_pos != std::string::npos || hy_pos != std::string::npos)
                        hy_pos < ws_pos ? idf_pos = hy_pos : idf_pos = ws_pos;
                    if ((1 + idf_pos - pos_in_txt) + line.size() <= _max_char_per_line) {
                        word = _text.substr(pos_in_txt, 1 + idf_pos - pos_in_txt);
                        line += word;
                        pos_in_txt += word.size();
                        ++word_count;
                        //SDL_Log("POS: %d - LINE: %s", word.size(), line.c_str());
                    } else {
                        if (line.empty())
                            _wraped_text->emplace_back(
                                    _text.substr(pos_in_txt, _max_char_per_line)),
                                    pos_in_txt += _max_char_per_line;
                            //SDL_Log("+EmptyLINE: %s", _wraped_text->back().c_str());
                        else
                            _wraped_text->emplace_back(line);
                        //pos_in_txt += line.size(),
                        //SDL_Log("LINE: %s",line.c_str());
                        line = "";
                    }
                    if (_wraped_text->size() >= _max_lines)
                        _max_lines = _wraped_text->size() - 1;
                } else {
                    word = _text.substr(pos_in_txt, _text.size() - pos_in_txt);
                    //SDL_Log("REM: %s", word.c_str());
                    if (line.size() + word.size() <= _max_char_per_line) {
                        line += word,
                                _wraped_text->emplace_back(line);
                    } else {
                        if (!line.empty())_wraped_text->emplace_back(line);
                        try {
                            WrapMaxChar(word, _wraped_text, _max_char_per_line,
                                        _max_lines - _wraped_text->size());
                        }
                        catch (const std::exception &exp) {
                            //SDL_Log("%s", exp.what());
                            std::cout << exp.what() << std::endl;
                        }
                    }
                    _max_lines = _wraped_text->size() - 1;
                }
            }
        } else {
            WrapMaxChar(_text, _wraped_text, _max_char_per_line, _max_lines);
        }

        /*std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - start);
        SDL_Log("text processing finished: %f secs", dt.count());
        SDL_Log("No wrapped lines: %d", _wraped_text->size());
        SDL_Log("WordCount: %d", word_count);*/
    }

private:
    TextProcessor() {}

    void U8WrapMaxChar(const std::u32string& _text, std::deque<std::u32string>* _wrapped_text,
        const int& _max_char_per_line, const int& _max_lines) {
        if (_text.empty())return;
        std::u32string subtext;
        for (int i = 0; i < _max_lines; ++i) {
            try {
                if (i * _max_char_per_line > _text.size())i = _max_lines;
                else
                    subtext = _text.substr(i * _max_char_per_line,
                        _max_char_per_line),
                    _wrapped_text->emplace_back(subtext);
            }
            catch (const std::exception& excp) {
                std::cout << excp.what() << std::endl;
            }
        }
    }

    void WrapMaxChar(const std::string &_text, std::deque<std::string> *_wrapped_text,
                     const int &_max_char_per_line, const int &_max_lines) {
        if (_text.empty())return;
        std::string subtext;
        for (int i = 0; i < _max_lines; ++i) {
            try {
                if (i * _max_char_per_line > _text.size())i = _max_lines;
                else
                    subtext = _text.substr(i * _max_char_per_line,
                                           _max_char_per_line),
                            _wrapped_text->emplace_back(subtext);
            }
            catch (const std::exception &excp) {
                std::cout << excp.what() << std::endl;
            }
        }
    }
};


std::vector<std::string> splitStringMaxCodePoints(const std::string& str, const std::string& delimiters, int maxLength, int maxSubstrings) {
    std::vector<std::string> tokens;
    tokens.reserve(maxSubstrings);
    std::string token;

    // Create a lookup table of delimiters
    std::unordered_set<char32_t> isDelimiter;
    for (char c : delimiters) {
        isDelimiter.insert((char32_t)c);
    }

    std::setlocale(LC_CTYPE, "en_US.UTF-8");
    const char* p = str.c_str();
    size_t remaining = str.length();
    int numSubstrings = 0;
    while (remaining > 0 && numSubstrings < maxSubstrings) {
        wchar_t codePoint;
        int len = std::mbtowc(&codePoint, p, remaining);
        if (len < 0) {
            // error handling
        }
        if (isDelimiter.count((char32_t)codePoint) > 0) {
            if (token.length() > 0) {
                tokens.push_back(token);
                token.clear();
                numSubstrings++;
            }
        }
        else if (token.length() < maxLength) {
            token += codePoint;
        }
        else {
            tokens.push_back(token);
            token.clear();
            token += codePoint;
            numSubstrings++;
        }
        p += len;
        remaining -= len;
    }

    if (token.length() > 0 && numSubstrings < maxSubstrings) {
        tokens.push_back(token);
    }

    return tokens;
}


std::vector<std::string> splitStringMaxWords(const std::string& str, const std::string& delimiters, int maxLength, int maxSubstrings){
        std::vector<std::string> tokens;
        tokens.reserve(maxSubstrings);
        std::string token;

        // Create a lookup table of delimiters
        std::unordered_set<char32_t> isDelimiter;
        for (char c : delimiters) {
            isDelimiter.insert((char32_t)c);
        }

        std::setlocale(LC_CTYPE, "en_US.UTF-8");
        const char* p = str.c_str();
        size_t remaining = str.length();
        int numSubstrings = 0;
        while (remaining > 0 && numSubstrings < maxSubstrings) {
            wchar_t codePoint;
            int len = std::mbtowc(&codePoint, p, remaining);
            if (len < 0) {
                // error handling
            }
            if (isDelimiter.count((char32_t)codePoint) > 0) {
                if (token.length() > 0) {
                    tokens.push_back(token);
                    token.clear();
                    numSubstrings++;
                }
            }
            else {
                token += codePoint;
                if (token.length() >= maxLength) {
                    // Check if the token contains more than one full word
                    size_t spacePos = token.find(' ');
                    if (spacePos == std::string::npos) {
                        // Token contains only one full word, add it as is
                        tokens.push_back(token);
                        token.clear();
                        numSubstrings++;
                    }
                    else {
                        // Token contains more than one full word, add it up to the last full word
                        tokens.push_back(token.substr(0, spacePos));
                        token = token.substr(spacePos + 1);
                        numSubstrings++;
                    }
                }
            }
            p += len;
            remaining -= len;
        }

        if (token.length() > 0 && numSubstrings < maxSubstrings) {
            tokens.push_back(token);
        }

        return tokens;
}

std::vector<std::string> splitStringMaxWords2(const std::string& str, const std::string& delimiters, int maxCharCount) {
    std::vector<std::string> sentences;
    std::string sentence;
    std::string word;
    std::mbstate_t mbstate{};
    int codePointCount = 0;

    for (char c : str) {
        if (delimiters.find(c) != std::string::npos) {
            // The current character is a delimiter, so add it to the current word
            word += c;
            codePointCount += std::mbrlen(&c, 1, &mbstate);
            if ((sentence.size() + word.size()) > maxCharCount || codePointCount > maxCharCount) {
                // The current word and delimiter do not fit in the current sentence, so add the current sentence to the list of sentences
                // and start a new sentence with the current word and delimiter.
                sentences.push_back(sentence);
                sentence.clear();
                sentence += word;
                word.clear();
                codePointCount = 0;
            }
            else {
                // The current word and delimiter fit in the current sentence, so add them to the sentence
                sentence += word;
                word.clear();
            }
        }
        else {
            // The current character is not a delimiter, so add it to the current word
            word += c;
            codePointCount += std::mbrlen(&c, 1, &mbstate);
        }
    }

    // Add the remaining sentence to the list of sentences
    if (!sentence.empty()) {
        sentences.push_back(sentence);
    }

    return sentences;
}

std::vector<std::string> wrapText1(const std::string& text, size_t maxLineLength) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.length()) {
        size_t end = start + maxLineLength;
        if (end > text.length()) {
            end = text.length();
        }
        else {
            // Find the last space character before the maximum line length
            size_t spacePos = text.rfind(' ', end);
            if (spacePos != std::string::npos) {
                end = spacePos;
            }
        }
        lines.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}


namespace FileSystem {
    class FileStore {
    public:
        FileStore() { dir_count = scanned_files = 0; }

        FileStore(const std::filesystem::path &root_path) {
            this->setRootPath(root_path);
        }

        FileStore &addExtensionFilter(const std::string &extension_filter_) {
            std::scoped_lock lock(m_file_sys_mux);
            this->extension_filter.insert({ extension_filter_, 0 });
            return *this;
        }

        std::string getFilePath(const uint64_t &index) {
            std::scoped_lock lock(m_file_sys_mux);
            if (index > m_files_store.size())return "";
            try {
                return {m_files_store[index].generic_string()};
            }
            catch (const std::exception &e) {
                return "";
            }
        }
        

        std::wstring getFileWPath(const unsigned int &index) {
            std::scoped_lock lock(m_file_sys_mux);
            if (index > m_files_store.size())return L"";
            try {
                return {m_files_store[index].generic_wstring()};
            }
            catch (const std::exception &e) {
                return L"";
            }
        }

        std::string getFileName(const size_t &index) {
            std::scoped_lock lock(m_file_sys_mux);
            try {
                return {m_files_store[index].stem().generic_string()};
            }
            catch (const std::exception &e) {
                std::cout << e.what() << std::endl;
                return "";
            }
        }
        
#ifdef _MSC_VER
        std::u8string getU8FilePath(const unsigned int& index) {
            std::scoped_lock lock(m_file_sys_mux);
            if (index > m_files_store.size())return u8"";
            try {
                return { m_files_store[index].generic_u8string() };
            }
            catch (const std::exception& e) {
                return u8"";
            }
        }

        std::u8string getU8FileName(const size_t& index) {
            std::scoped_lock lock(m_file_sys_mux);
            try {
                return m_files_store[index].stem().generic_u8string();
            }
            catch (const std::exception& e) {
                std::cout << e.what() << std::endl;
                return u8"";
            }
        }

#endif // _MSC_VER

        std::string getFileExtension(const int &index) {
            std::scoped_lock lock(m_file_sys_mux);
            try {
                return {m_files_store[index].extension().generic_string()};
            }
            catch (const std::exception &e) {
                return "";
            }
        }

        inline std::deque<std::filesystem::path> *getFileStore() noexcept {
            return &m_files_store;
        }

        inline FileStore &setRootPath(const std::filesystem::path &root_path)noexcept {
            this->m_root_path = root_path;
            return *this;
        }

        FileStore &scanFiles() {
            std::scoped_lock lock(m_file_sys_mux);
            nWorkerComplete = 0;
            auto start = std::chrono::high_resolution_clock::now();
#if defined(__linux__)
            internalScanFilesLinux(m_root_path.string());
#else
            //internalScanFiles(m_root_path);
            internalScanFilesThreadPool(m_root_path);
            //for (const auto& fts : futures_)fts.wait();
            //futures_.clear();
            while (nWorkerComplete > 0) { SDL_Delay(1); }
#endif
            //while (nWorkerComplete > 0) { SDL_Delay(50); }
            std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - start);
            SDL_Log("Dir scan finished: %f secs", dt.count());
            SDL_Log("No Scanned Files: %u", m_files_store.size());
            return *this;
        }

        FileStore &sort() {
            std::scoped_lock lock(m_file_sys_mux);
            const auto start = std::chrono::high_resolution_clock::now();
            std::sort(m_files_store.begin(), m_files_store.end(),
                      [](const std::filesystem::path &a, const std::filesystem::path &b) {
                          const std::string aa = a.stem().string();
                          const std::string bb = b.stem().string();
                          return std::lexicographical_compare(aa.begin(), aa.end(), bb.begin(),
                                                              bb.end(),
                                                              [](const char &c1, const char &c2) {
                                                                  return std::toupper(c1) < std::toupper(c2);
                                                              });
                      });
            const std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - start);
            SDL_Log("Dir Sort finished: %f secs", dt.count());
            return *this;
        }

        /*
         * returns vector of a string & int pair where the string is the name of the file extension
         * and the int is the count of the files with that extension from the previous scanFiles() call
         * */
        inline std::unordered_map<std::string, int>* getExtensions() {
            std::scoped_lock lock(m_file_sys_mux);
            return &extension_filter;
        }

        inline size_t size()noexcept {
            std::scoped_lock lock(m_file_sys_mux);
            return this->m_files_store.size();
        }

        inline bool empty()noexcept {
            std::scoped_lock lock(m_file_sys_mux);
            return m_files_store.empty();
        }

        inline void clear() {
            std::scoped_lock lock(m_file_sys_mux);
            m_files_store.clear();
            dir_count = scanned_files = 0;
        }

        inline int getDirCount()noexcept {
            std::scoped_lock lock(m_file_sys_mux);
            return dir_count;
        }

        inline int getScannedFilesCount()noexcept {
            std::scoped_lock lock(m_file_sys_mux);
            return scanned_files;
        }

        FileStore& clearAndReset(){
            m_root_path.clear();
            m_files_store.clear();
            extension_filter.clear();
            dir_count = scanned_files = 0;
            return *this;
        }

        /*bool compare_nocase(const std::string &a, const std::string &b) {
            unsigned int i = 0;
            while ((i < a.length()) && (i < b.length())) {
                if (tolower(a[i]) < tolower(b[i]))
                    return true;
                else if (tolower(a[i]) > tolower(b[i]))
                    return false;
                ++i;
            }
            return (a.length() < b.length());
        }*/

    private:
        /*std::function<void(const std::filesystem::path&)> addToFileStore = [&](const std::filesystem::path& path_) {
            std::lock_guard<std::mutex> lock(filestore_mutex);
            m_files_store.emplace_back(path_);
        };*/

        inline void addToFileStore(const std::filesystem::path& path_){
            std::scoped_lock lock(filestore_mutex);
            //using namespace std::chrono_literals;
            //std::this_thread::sleep_for(25ms);
            m_files_store.emplace_back(path_);
        }
        
        inline void addToFileStore(const std::deque<std::filesystem::path>& _files){
            std::scoped_lock lock(filestore_mutex);
            //using namespace std::chrono_literals;
            //std::this_thread::sleep_for(25ms);
            for (auto& file_ : _files)
                m_files_store.emplace_back(file_);
        }

        void internalScanFiles(const std::filesystem::path path_) {
            std::vector<std::future<void>> futures;
            //std::deque<std::filesystem::path> files_;
            try {
                //std::filesystem::path d_path_;
                for (const auto& dir_ent : std::filesystem::directory_iterator(path_,std::filesystem::directory_options::skip_permission_denied)) {
                    const auto& d_path_ = dir_ent.path();
                    if (std::filesystem::is_directory(d_path_))
                        futures.emplace_back(
                            /*Async::GThreadPool.enqueue(&FileStore::internalScanFilesThreadPool, this, d_path_)*/
                            std::async(std::launch::async, &FileStore::internalScanFiles, this, d_path_));
                    else {
                        if (extension_filter.empty())addToFileStore(d_path_);
                        else {
                            const auto f_extn = d_path_.extension().string();
                            addToFileStore(d_path_);
                            if (extension_filter.contains(f_extn))
                                extension_filter[f_extn] += 1;
                            /*for (auto& [extension_name, extension_count] : extension_filter) {
                                if (extension_name == f_extn) {
                                    addToFileStore(d_path_);
                                    std::scoped_lock lock(extension_filter_mux);
                                    ++extension_count;
                                    break;
                                }
                            }*/
                        }

                    }
                }
            }
            catch (std::exception e)
            {
                std::cout << e.what() << "\n";
            }
            for (const auto& fts : futures)fts.wait();
        }
        
        void internalScanFilesThreadPool(const std::filesystem::path path_) {
           nWorkerComplete += 1;
           std::vector<std::future<void>> futures;
           //std::deque<std::filesystem::path> files_;
            try {
                for (const auto& dir_ent : std::filesystem::directory_iterator(path_, std::filesystem::directory_options::skip_permission_denied)) {
                    const auto d_path_ = dir_ent.path();
                    if (std::filesystem::is_directory(d_path_))
                        futures.emplace_back(Async::GThreadPool.enqueue(&FileStore::internalScanFilesThreadPool, this, d_path_));
                    else {
                        if (extension_filter.empty())addToFileStore(d_path_);
                        else {
                            const auto f_extn = d_path_.extension().string();
                            std::scoped_lock lock(extension_filter_mux);
                            if (extension_filter.contains(f_extn)) {
                                addToFileStore(d_path_);
                                //files_.emplace_back(d_path_);
                                extension_filter[f_extn] += 1;
                            }
                            //for (auto& [extension_name, extension_count] : extension_filter) {
                            //    if (extension_name == f_extn) {
                            //        addToFileStore(d_path_);
                            //        //files_.emplace_back(d_path_);
                            //        std::scoped_lock lock(extension_filter_mux);
                            //        ++extension_count;
                            //        break;
                            //    }
                            //}
                        }

                    }
                }
                //addToFileStore(std::move(files_));
            }
            catch (std::exception e)
            {
                std::cout << e.what() << "\n";
            }
            nWorkerComplete -= 1;
        }

#ifdef __ANDROID__
        void internalScanFilesLinux(const std::string dir_name){
            nWorkerComplete += 1;
            DIR *dir;              // pointer to directory
            struct dirent *entry; // all stuff in the directory
            struct stat info;      // info about each entry

            dir = opendir(dir_name.c_str());
            std::vector<std::future<void>> futures;

            if (!dir) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Error while opening Dir: %s \n%s", dir_name.c_str(), strerror(errno));
                return;
            }

            while ((entry = readdir(dir)) != nullptr) {
                if (entry->d_name[0] != '.') {
                    std::string path = dir_name + "/" + std::string(entry->d_name);
                    stat(path.c_str(), &info);
                    if (S_ISDIR(info.st_mode)) {
                        futures.emplace_back(
                                std::async(std::launch::async, &FileStore::internalScanFilesLinux, this, path)
                            /*Async::GThreadPool.enqueue(&FileStore::internalScanFilesLinux, this, path)*/);
                    } else {
                        if (extension_filter.empty())addToFileStore(path);
                        else {
                            //const auto extension_pos=path.find_last_of('.');
                            const char *extracted_extension_ = strrchr(path.c_str(), '.');
                            if (extracted_extension_/*extension_pos!=std::string::npos*/) {
                                //const std::string_view extracted_extension_ = path.substr(extension_pos);
                                //SDL_Log("%s", extension_pos);
                                for (auto& [extension_name, extension_count] : extension_filter) {
                                    if (extension_name == extracted_extension_) {
                                        addToFileStore(path);
                                        std::scoped_lock lock(extension_filter_mux);
                                        ++extension_count;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            closedir(dir);
            //for (const auto &ft: futures)ft.wait();
            nWorkerComplete -= 1;
        }
#endif  //__ANDROID__

        std::mutex filestore_mutex;
        std::mutex extension_filter_mux;
        std::deque<std::filesystem::path> m_files_store;
        std::unordered_map<std::string, int> extension_filter;
        std::filesystem::path m_root_path;
        std::atomic_int_fast32_t dir_count, scanned_files;
        std::mutex m_file_sys_mux;
        //std::vector<std::future<void>> futures_;
        std::atomic_int_fast32_t nWorkerComplete;
    };
}

 //#endif //VOLT_UTIL_H
