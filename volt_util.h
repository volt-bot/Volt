// #ifndef VOLT_UTIL_H
// #define VOLT_UTIL_H
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
#include <shared_mutex>
#include <unordered_set>
#include <bitset>
#include <clocale>
#include <charconv>
#include <cstring>
#include <cstdlib>
#include <locale>
#include <codecvt>
#include <type_traits>
#include <typeindex>
#ifdef _MSC_VER
#include <ranges>
// #define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(__linux__)
#include <dirent.h>
#include <sys/stat.h>
#endif
#include <ctime>

#include "utf8.h"

// #define FMT_HEADER_ONLY

enum class DeviceDisplayType
{
	Unknown,
	Tablet,
	Tv,
	Medium,
	Small,
	UnSupported,
};

enum class TextWrapStyle
{
	MAX_CHARS_PER_LN,
	MAX_WORDS_PER_LN
};

enum class PERFOMANCE_MODE
{
	NORMAL,
	BATTERY_SAVER,
	HIGH
};

enum class Orientation
{
	VERTICAL,
	HORIZONTAL,
	ANGLED
};

enum class Gravity
{
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

enum class EdgeType
{
	RECT,
	RADIAL
};

enum class FontStyle
{
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

enum class IMAGE_LD_STYLE
{
	NORMAL,
	CUSTOM_TEXTURE,
	ASYNC_PATH,
	ASYNC_CUSTOM_SURFACE_LOADER,
	ASYNC_DEFAULT_TEXTURE_PATH,
	ASYNC_DEFAULT_TEXTURE_CUSTOM_LOADER
};

enum class PixelSystem
{
	NORMAL,
	PERCENTAGE
};

enum class ScrollDirection
{
	VERTICAL,
	HORIZONTAL,
};

enum class GRIDCELL_PLACEMENT_POLICY
{
	LINEAR_STACK,
	STAGGERED
};

template <class T>
struct v2d_generic
{
	T x = 0;
	T y = 0;

	v2d_generic() : x(0), y(0) {}

	v2d_generic(T _x, T _y) : x(_x), y(_y) {}

	v2d_generic(const v2d_generic &v) : x(v.x), y(v.y) {}

	v2d_generic &operator=(const v2d_generic &v) = default;

	constexpr T mag() const { return T(std::sqrt(x * x + y * y)); }

	constexpr T mag2() const { return x * x + y * y; }

	v2d_generic norm() const
	{
		T r = 1 / mag();
		return v2d_generic(x * r, y * r);
	}

	v2d_generic perp() const { return v2d_generic(-y, x); }

	v2d_generic floor() const { return v2d_generic(std::floor(x), std::floor(y)); }

	v2d_generic ceil() const { return v2d_generic(std::ceil(x), std::ceil(y)); }

	constexpr v2d_generic max(const v2d_generic &v) const
	{
		return v2d_generic(std::max(x, v.x), std::max(y, v.y));
	}

	constexpr v2d_generic min(const v2d_generic &v) const
	{
		return v2d_generic(std::min(x, v.x), std::min(y, v.y));
	}

	v2d_generic cart() { return {std::cos(y) * x, std::sin(y) * x}; }

	v2d_generic polar() { return {mag(), std::atan2(y, x)}; }

	T dot(const v2d_generic &rhs) const { return this->x * rhs.x + this->y * rhs.y; }

	T cross(const v2d_generic &rhs) const { return this->x * rhs.y - this->y * rhs.x; }

	constexpr v2d_generic operator+(const v2d_generic &rhs) const
	{
		return v2d_generic(this->x + rhs.x, this->y + rhs.y);
	}

	constexpr v2d_generic operator-(const v2d_generic &rhs) const
	{
		return v2d_generic(this->x - rhs.x, this->y - rhs.y);
	}

	constexpr v2d_generic operator*(const T &rhs) const
	{
		return v2d_generic(this->x * rhs, this->y * rhs);
	}

	constexpr v2d_generic operator*(const v2d_generic &rhs) const
	{
		return v2d_generic(this->x * rhs.x, this->y * rhs.y);
	}

	constexpr v2d_generic operator/(const T &rhs) const
	{
		return v2d_generic(this->x / rhs, this->y / rhs);
	}

	constexpr v2d_generic operator/(const v2d_generic &rhs) const
	{
		return v2d_generic(this->x / rhs.x, this->y / rhs.y);
	}

	v2d_generic &operator+=(const v2d_generic &rhs)
	{
		this->x += rhs.x;
		this->y += rhs.y;
		return *this;
	}

	v2d_generic &operator-=(const v2d_generic &rhs)
	{
		this->x -= rhs.x;
		this->y -= rhs.y;
		return *this;
	}

	v2d_generic &operator*=(const T &rhs)
	{
		this->x *= rhs;
		this->y *= rhs;
		return *this;
	}

	v2d_generic &operator/=(const T &rhs)
	{
		this->x /= rhs;
		this->y /= rhs;
		return *this;
	}

	v2d_generic &operator*=(const v2d_generic &rhs)
	{
		this->x *= rhs.x;
		this->y *= rhs.y;
		return *this;
	}

	v2d_generic &operator/=(const v2d_generic &rhs)
	{
		this->x /= rhs.x;
		this->y /= rhs.y;
		return *this;
	}

	v2d_generic operator+() const { return {+x, +y}; }

	v2d_generic operator-() const { return {-x, -y}; }

	bool operator==(const v2d_generic &rhs) const
	{
		return (this->x == rhs.x && this->y == rhs.y);
	}

	bool operator!=(const v2d_generic &rhs) const
	{
		return (this->x != rhs.x || this->y != rhs.y);
	}

	auto str() const -> const std::string
	{
		return std::string("(") + std::to_string(this->x) + "," + std::to_string(this->y) + ")";
	}

	friend std::ostream &operator<<(std::ostream &os, const v2d_generic &rhs)
	{
		os << rhs.str();
		return os;
	}

	operator v2d_generic<int32_t>() const
	{
		return {static_cast<int32_t>(this->x), static_cast<int32_t>(this->y)};
	}

	operator v2d_generic<float>() const
	{
		return {static_cast<float>(this->x), static_cast<float>(this->y)};
	}

	operator v2d_generic<double>() const
	{
		return {static_cast<double>(this->x), static_cast<double>(this->y)};
	}
};

constexpr float degreesToRad(const int &degrees)
{
	return static_cast<float>(degrees) * 0.0174533f;
}

template <typename T, typename param>
class NamedType
{
  public:
	explicit NamedType(T const &value) : value_(value) {}

	explicit NamedType(T &&value) : value_(std::move(value)) {}

	// T& get() { return value_; }

	[[nodiscard]] constexpr auto get() const noexcept { return value_; }

  private:
	T value_;
};

template <typename T>
class Width
{
  public:
	explicit Width(T const &value) : value_(value) {}

	explicit Width(T &&value) : value_(std::move(value)) {}

	// T& get() { return value_.get(); }

	[[nodiscard]] constexpr auto get() const noexcept { return value_.get(); }

  private:
	NamedType<T, struct widthparam> value_;
};

template <typename T>
class Height
{
  public:
	explicit Height(T const &value) : value_(value) {}

	explicit Height(T &&value) : value_(std::move(value)) {}

	// T& get() { return value_.get(); }

	[[nodiscard]] constexpr auto get() const noexcept { return value_.get(); }

  private:
	NamedType<T, struct widthparam> value_;
};

template <typename T>
class MilliSec
{
  public:
	explicit MilliSec(T const &value) : value_(value) {}

	explicit MilliSec(T &&value) : value_(std::move(value)) {}

	// T& get() { return value_.get(); }

	[[nodiscard]] constexpr auto get() const noexcept { return value_.get(); }

  private:
	NamedType<T, struct widthparam> value_;
};

namespace Dict
{
using dict = std::map<std::type_index, std::any>;

template <class Name, class T>
struct key final
{
	explicit key() = default;
};

template <class Name, class T>
auto get(const dict &d, key<Name, T> k) -> std::optional<T>
{
	if (auto pos = d.find(typeid(k)); pos != d.end())
	{
		return std::any_cast<T>(pos->second);
	}
	return {};
}

template <class Name, class T, class V>
void set(dict &d, key<Name, T> k, V &&value)
{
	constexpr bool convertible = std::is_convertible_v<V, T>;
	static_assert(convertible);
	if constexpr (convertible)
	{
		d.insert_or_assign(typeid(k), T{std::forward<V>(value)});
	}
}

namespace example
{
using age_key = key<struct _age_, int>;
using gender_key = key<struct _gender_, std::pair<float, float>>;
using name_key = key<struct _name_, std::string>;

constexpr inline auto age = age_key{};
constexpr inline auto gender = gender_key{};
constexpr inline auto name = name_key{};

auto person = dict{};

/*set(person, name, "Florb");
        set(person, age, 18);
        set(person, gender, std::pair{0.5f,1.f});

        const auto a=get(person,age);
        const auto n=get(person,name);
        const auto g=get(person,gender);

        std::cout<<"name: "<<*n<<std::endl;*/

} // namespace example
} // namespace Dict

/**
 * @brief Concatenates a range of elements from one container to the back of another.
 *
 * This function is generic and works with any container that has a `value_type`
 * and can be written to using `std::back_inserter`.
 *
 * @tparam TargetContainer The type of the container to which elements will be appended.
 * Must support `push_back` (directly or via `back_inserter`).
 * @tparam InputIt         The type of the input iterator for the source range.
 *
 * @param target The container to which elements will be added.
 * @param first  An input iterator pointing to the beginning of the range to concatenate.
 * @param last   An input iterator pointing to the end of the range to concatenate (exclusive).
 */
template <typename TargetContainer, typename InputIt>
constexpr inline void concat_rng(TargetContainer &target, InputIt first, InputIt last)
{
	std::copy(first, last, std::back_inserter(target));
}

// get current date YYYY-MM-DD
std::string getDateStr()
{
	/*const std::chrono::time_point<std::chrono::system_clock> now{std::chrono::system_clock::now()};
    // Get the year, month, and day
    const std::chrono::year_month_day ymd{std::chrono::floor<std::chrono::days>(now)};
    std::stringstream dateTimeAdded;
    dateTimeAdded << ymd;
    return dateTimeAdded.str();*/
	return "";
}

// get current date YYYY-MM-DD HH:MM:SS
inline std::string getDateAndTimeStr()
{
	const std::chrono::time_point<std::chrono::system_clock> now{std::chrono::system_clock::now()};
	// Get the year, month, and day
	const std::chrono::year_month_day ymd{std::chrono::floor<std::chrono::days>(now)};

	// Get the time
	std::time_t tt = std::chrono::system_clock::to_time_t(now);
#ifdef _MSC_VER
	//std::tm* tm = nullptr;
	//localtime_s(tm,&tt);
	std::tm* tm = std::localtime(&tt);
#else
	std::tm* tm = std::localtime(&tt);
#endif
	std::stringstream timeAdded;
	timeAdded
		<< std::setfill('0') << std::setw(4) << tm->tm_year + 1900 << "-"
		<< std::setfill('0') << std::setw(2) << tm->tm_mon + 1 << "-"
		<< std::setfill('0') << std::setw(2) << tm->tm_mday << " "
		<< std::setfill('0') << std::setw(2) << tm->tm_hour << ":"
		<< std::setfill('0') << std::setw(2) << tm->tm_min << ":"
		<< std::setfill('0') << std::setw(2) << tm->tm_sec;
	// Combine date and time
	//std::stringstream dateTimeAdded;
	//dateTimeAdded <<std::to_string(ymd) << " " << timeAdded.str();
	return timeAdded.str();
}

// get date YYYY-MM-DD from string
inline std::chrono::year_month_day stringToYmd(const std::string &dateTimeStr)
{
	std::istringstream ss(dateTimeStr);
	std::tm tm = {};
	ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
	std::chrono::system_clock::time_point tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
	return std::chrono::year_month_day{std::chrono::floor<std::chrono::days>(tp)};
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

template <typename T>
inline std::string toString(const T &v, const uint8_t decimals = 2)
{
	std::stringstream sx;
	sx << std::setprecision(decimals) << std::fixed << v;
	return sx.str();
}

// to upper using 0x20 || 32
void printToUpper(std::string data) {
	for (auto& d : data) {
		std::cout << (char)(d ^ 0x20);// << std::endl;
	}
	std::cout<<std::endl;
}

template <typename... Args>
inline std::string packToString(const std::string &delim, const Args &... args)
{
	std::stringstream sx;
	bool first = true;
	auto writer = [&](const auto &arg) {
		if (!first)
		{
			sx << delim;
		}
		sx << arg;
		first = false;
	};
	(writer(args), ...);
	return sx.str();
}

inline std::string replaceStringAll(std::string str, const std::string &replace, const std::string &with)
{
	if (!replace.empty())
	{
		std::size_t pos = 0;
		while ((pos = str.find(replace, pos)) != std::string::npos)
		{
			str.replace(pos, replace.length(), with);
			pos += with.length();
		}
	}
	return str;
}

std::string getTextFromFile(const std::string &filename)
{
	std::ifstream file(filename, std::ios::binary); // Open in binary to preserve exact content
	if (!file.is_open())
	{
		std::cerr << "Error opening file: " << filename << std::endl;
		return "";
	}

	std::string text = "";
	// Read the entire file into text
	file.seekg(0, std::ios::end);
	size_t size = file.tellg();
	text.resize(size);
	file.seekg(0, std::ios::beg);
	file.read(&text[0], size);
	file.close();
	return text;
}

// Define the structure for a Suffix Trie node
struct SuffixTrieNode
{
	// Map to store children nodes.
	// Key: character, Value: unique_ptr to the child SuffixTrieNode.
	std::unordered_map<char, std::unique_ptr<SuffixTrieNode>> children;

	// Set to store indices (or IDs) of the original strings that contain
	// the substring represented by the path from the root to this node.
	std::unordered_set<int> stringIndices;

	// Constructor
	SuffixTrieNode() = default; // Use default constructor
};

// Class to manage the substring search index
class SubstringSearchIndex
{
  public:
	// Constructor: initializes the root of the Suffix Trie
	SubstringSearchIndex()
	{
		root = std::make_unique<SuffixTrieNode>();
	}

	// Adds a string to the index.
	// - text: The string to add.
	// - stringId: A unique integer identifier for this string. This ID will be returned
	//             during search if the string contains the search pattern.
	// Complexity: O(m^2) where m is the length of the text, as it inserts m suffixes,
	// and each suffix insertion can take up to O(m) time.
	void addString(const std::string &text, int stringId)
	{
		if (text.empty())
		{
			return; // Nothing to add for an empty string
		}

		// Iterate through all suffixes of the given text
		for (size_t i = 0; i < text.length(); ++i)
		{
			SuffixTrieNode *current = root.get(); // Start from the root for each suffix

			// Insert the current suffix (text[i...end]) into the Trie
			for (size_t j = i; j < text.length(); ++j)
			{
				char ch = text[j];
				// If the character does not exist as a child, create a new node
				if (current->children.find(ch) == current->children.end())
				{
					current->children[ch] = std::make_unique<SuffixTrieNode>();
				}
				// Move to the child node
				current = current->children[ch].get();
				// Add the stringId to this node, indicating that the original string
				// (identified by stringId) contains the substring formed by the path so far.
				current->stringIndices.insert(stringId);
			}
		}
	}

	// Searches for all strings in the index that contain the given pattern.
	// - pattern: The substring to search for.
	// Returns a set of string IDs that contain the pattern.
	// Complexity: O(P) where P is the length of the pattern.
	std::unordered_set<int> searchSubstring(const std::string &pattern)
	{
		if (pattern.empty())
		{
			// If the pattern is empty, it could be interpreted as matching all strings
			// or no strings. Returning empty for clarity, or this behavior can be defined.
			// To match all non-empty strings, you'd need to collect all string IDs ever added.
			// For now, let's say an empty pattern matches nothing specific this way.
			return {};
		}

		SuffixTrieNode *current = root.get(); // Start from the root

		// Traverse the Trie according to the characters in the pattern
		for (char ch : pattern)
		{
			if (current->children.find(ch) == current->children.end())
			{
				// If a character in the pattern is not found, the pattern does not exist
				// as a substring in any of the indexed strings.
				return {}; // Return an empty set
			}
			// Move to the next node
			current = current->children[ch].get();
		}

		// If the loop completes, 'current' is the node corresponding to the end of the pattern.
		// The 'stringIndices' set at this node contains the IDs of all strings
		// that have this pattern as a substring.
		return current->stringIndices;
	}

  private:
	// The root node of the Suffix Trie
	std::unique_ptr<SuffixTrieNode> root;
};

class Logger
{
  public:
	enum class Level : uint8_t
	{
		Info,
		Debug,
		Warning,
		Error,
	};

	std::ofstream file;

  public:
	Logger(const Logger &) = delete;
	Logger(const Logger &&) = delete;

	Logger()
	{
		setUpAsyncLogger();
		//file.open("logs.txt"/*, std::ios::out | std::ios::trunc*/);
	}

	Logger(std::filesystem::path fp) { setUpAsyncLogger(); }

	template <typename... Args>
	void Log(Logger::Level level, const Args &... _args)
	{
		auto msg = packToString(" ", _args...);
		std::string sLevel = "Bad-Log";
		switch (level)
		{
			//using enum Logger::Level;
		case Logger::Level::Info:
			sLevel = "Info";
			break;
		case Logger::Level::Debug:
			sLevel = "Debug";
			break;
		case Logger::Level::Warning:
			sLevel = "Warning";
			break;
		case Logger::Level::Error:
			sLevel = "Error";
			break;
		default:
			break;
		}
		std::unique_lock<std::mutex> lock(this->queue_mutex);
		logs[level].push_back(sLevel + " " + getDateAndTimeStr() + ": " + msg);
		lock.unlock();
		try
		{ 
			if (onLogFunc)
				onLogFunc(logs[level].back());
		}
		catch (std::exception ex)
		{
			//
		}
		////TODO: write to file
		//static std::ofstream file("logs.out");
		//std::cout << logs[level].back() << std::endl;
	}

	template <typename... Args>
	void AsyncLog(Logger::Level level, const Args &... _args)
	{
		// 1. Eagerly format the string on the calling thread.
		//    This creates a std::string, which owns its data.
		std::string msg = packToString(" ", _args...);

		// 2. Create a task that captures the formatted string BY COPY.
		auto task = std::make_shared<std::packaged_task<void()>>(
			// The lambda captures 'this', 'level', and 'msg'. 'msg' is copied.
			[this, level, msg]() {
				this->Log(level, msg);
			});

		// 3. Enqueue the task for the logger thread.
		// (Your existing queueing logic is fine)
		tasks.emplace([task]() { (*task)(); });
		cv.notify_one();
	}

	auto setOutputFile(std::filesystem::path fp)
	{
		// if invoke more than once on the same instance
		// if output file == currently set file return
		// else close current file and open a new one
		//std::ifstream file(fp, std::ios::binary);
		/*
		if (!file)
		{
			Log(Logger::Level::Error, "Could not open log file: " + fp.string());
		}*/
	}

	auto onLog(std::function<void(std::string)> _onLogFunc)
	{
		onLogFunc = _onLogFunc;
	}

	auto getLastError()
	{
		return logs[Logger::Level::Error].back();
	}

	auto clearAll()
	{
		logs.clear();
	}

	auto getAll()
	{
		return logs;
	}

  private:
	void setUpAsyncLogger()
	{
		/*worker = [this]
        {
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->cv.wait(lock, [this]
                                         { return this->stop || !this->tasks.empty(); });
                    if (this->stop && this->tasks.empty())
                        return;
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
        };*/
	}

  private:
	std::filesystem::path logFp;
	// Category and Log
	std::unordered_map<Logger::Level, std::vector<std::string>> logs{};
	std::function<void(std::string)> onLogFunc;
	std::thread worker;
	std::condition_variable cv;
	std::queue<std::function<void()>> tasks;
	std::mutex queue_mutex;
	bool stop = false;
};

namespace Async
{
class ThreadPool
{
  public:
	ThreadPool() : stop(false)
	{
	}

	ThreadPool(size_t numThreads) : stop(false)
	{
		init(numThreads);
	}

	~ThreadPool()
	{
		kill_all();
	}

	void kill_all()
	{
		{
			std::unique_lock<std::mutex> lock(this->queue_mutex);
			stop = true;
		}
		condition.notify_all();
		for (std::thread &worker : workers)
		{
			worker.join();
		}
	}

	void init(size_t numThreads)
	{
		numThreads = std::clamp(numThreads, size_t(1), size_t(1000000));
		for (size_t i = 0; i < numThreads; ++i)
		{
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
                    } });
		}
	}

	template <class F, class... Args>
	void runInterval(int _interval, std::function<bool()> _stop_source, F &&func_, Args &&... args)
	{
		using return_type = std::invoke_result_t<F, Args...>;
		auto task = std::make_shared<std::packaged_task<return_type()>>(
			std::bind(std::forward<F>(func_), std::forward<Args>(args)...));
		// clarity for this worker's lifetime is required to avoid potential dead locks
		// should probably put the thread obj in a container like a que or a vector
		// this needs to live until timeout or stop token invoke
		std::thread intervalWorker([this, _interval, _stop_source] {
                    while (not _stop_source()) {
                        task();
                    } });
	}

	template <class F, class... Args>
	auto enqueue(F &&f, Args &&... args)
		-> std::future<std::invoke_result_t<F, Args...>>
	{
		using return_type = std::invoke_result_t<F, Args...>;
		auto task = std::make_shared<std::packaged_task<return_type()>>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...));
		std::future<return_type> res = task->get_future();
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			if (stop)
				throw std::runtime_error("enqueue on stopped thread pool");
			tasks.emplace([task]() { (*task)(); });
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
	// std::vector<std::thread> intervalWorkers;
	std::queue<std::function<void()>> tasks;
	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop;
};

ThreadPool GThreadPool(std::thread::hardware_concurrency() + 4);

std::function<bool()> defaultTrue = []() { return true; };

/// NOTES: should return a handler for the dispatched recuring task(the handler can be used for control like killing the task if not needed anymore)
///  should have a lifetime param default = infinite(this param can be a lambda/functor that returns a boolean. the lambda is invoked after avery invoke
// calls the given method after the given interval
// param for number of iterations before termination
// consider making the stop source param the result of the previous return

/*template<class F, class... Args>
    void setInterval(int interval_, std::function<bool()> stop_source_,F&& func_, Args&&... args){
        using return_type = std::invoke_result_t<F, Args...>;
            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(func_), std::forward<Args>(args)...)
                );
            //task();
    }*/

void setInterval(std::function<void(void)> func, unsigned int _interval, std::function<bool(void)> _stop_source = defaultTrue)
{
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

}; // namespace Async

class TextProcessor
{
  public:
	TextProcessor(const TextProcessor &) = delete;

	TextProcessor(const TextProcessor &&) = delete;

	size_t word_count = 0;

  public:
	static TextProcessor &Get()
	{
		static TextProcessor *instance = new TextProcessor();
		return *instance;
	}

  public:
	void WrapRanges(const std::string &_text, std::deque<std::string> *_wraped_text,
					const int &_max_char_per_line, int _max_lines, const TextWrapStyle &tws)
	{
#ifdef _MSC_VER
		auto split_strings = std::string_view{_text} | std::ranges::views::split(' ');
		for (const auto &_string : split_strings)
		{
			_wraped_text->emplace_back(std::string_view{_string.begin(), _string.end()});
		}
#endif
	}

	std::size_t strlen_mb(const std::string_view s)
	{
		std::mblen(nullptr, 0); // reset the conversion state
		std::size_t result = 0;
		const char *ptr = s.data();
		for (const char *const end = ptr + s.size(); ptr < end; ++result)
		{
			const int next = std::mblen(ptr, end - ptr);
			if (next == -1)
				throw std::runtime_error("strlen_mb(): conversion error");
			ptr += next;
		}
		return result;
	}

	void u8wrap_max_char(const std::u8string &_text, std::deque<std::u8string> *_wrapped_text,
						 const std::size_t &_max_char_per_line, const std::size_t &_max_lines)
	{
		// SDL_Log("RCV_TEXT: %s", (char*)_text.c_str());
		if (_text.empty())
			return;
		for (int i = 0; i < _max_lines; ++i)
		{
			/*try {*/
			if (i * _max_char_per_line > _text.size())
				i = _max_lines;
			else
				_wrapped_text->emplace_back(_text.substr(i * _max_char_per_line, _max_char_per_line));
			/*}
            catch (const std::exception& excp) {
                std::cout << excp.what() << std::endl;
            }*/
		}
	}

	void u8wrap_max_word_count(const std::u8string &_text, std::deque<std::u8string> *_wraped_text, const std::u8string_view _delimiters,
							   const std::size_t &_max_char_per_line, const std::size_t &_max_lines = 0)
	{
		std::u8string line = u8"", word = u8"";
		// std::vector<const std::string::size_type> delimiter_pos;
		size_t pos_in_text_ = 0;
		bool is_first_delim = true;
		bool wrap_done = false;
		while (!wrap_done)
		{
			is_first_delim = true;
			std::u8string::size_type nearest_delim_pos = 0;
			for (const auto &_del : _delimiters) // delimiter_pos.push_back(_text.find(_del, pos_in_text_));
			{
				const auto pos = _text.find(_del, pos_in_text_);
				if (!is_first_delim)
				{
					if (pos <= nearest_delim_pos)
						nearest_delim_pos = pos;
				}
				else
					nearest_delim_pos = pos, is_first_delim = false;
			}

			if (nearest_delim_pos != std::u8string::npos)
			{
				// check if the current line size plus the new sub-string is less than max chars per line
				// if so then add the substring at the back of the line
				if (line.size() + (nearest_delim_pos - pos_in_text_) <= _max_char_per_line)
				{
					line += _text.substr(pos_in_text_, nearest_delim_pos + 1 - pos_in_text_);
					pos_in_text_ = nearest_delim_pos + 1;
				}
				else
				{
					// we have a line with the max possible words so push the line into the output container
					// then empty the line for new line processing

					// if line is empty, then the line is not splitable so add the whole max_chars_per_line line into the output container
					if (line.empty())
					{
						line = _text.substr(pos_in_text_, _max_char_per_line);
						pos_in_text_ += _max_char_per_line;
					}
					_wraped_text->emplace_back(line);
					// SDL_Log("mxc:%d - LNS:%d - NL:%s", _max_char_per_line, line.size(), line.c_str());
					line.clear();
					// check if next line is splitable if not add the whole max_chars_per_line line into the output container
				}
			}
			else
			{
				if (!line.empty())
				{
					if (line.size() < _max_char_per_line)
					{
						// SDL_Log("pt: %d/%d", pos_in_text_, _text.size());
						const std::size_t rem_text_size = _max_char_per_line - line.size();
						line += _text.substr(pos_in_text_, rem_text_size);
						pos_in_text_ += rem_text_size + 1;
						pos_in_text_ = std::clamp(pos_in_text_, (std::size_t)0, _text.size());
						// SDL_Log("pt: %d/%d", pos_in_text_, _text.size());
						// SDL_Log("lns: %d/%d", line.size(), _max_char_per_line);
					}
					_wraped_text->emplace_back(line);
				}
				u8wrap_max_char(_text.substr(pos_in_text_, _text.size()), _wraped_text, _max_char_per_line, _max_lines);

				/*SDL_Log("RCV_TEXT: %s", (char*)_wraped_text->back().c_str());
                SDL_Log("SND_TEXT: %d/%d", _text.size(), pos_in_text_);
                SDL_Log("MX_CL: %d", _max_char_per_line);*/
				// if (_wraped_text->back().size() < _max_char_per_line) {
				//     const std::size_t rem_text_size = _max_char_per_line - _wraped_text->back().size();
				//     _wraped_text->back()+=(_text.substr(pos_in_text_, rem_text_size));
				//     SDL_Log("RCV_TEXT: %s", (char*)_wraped_text->back().c_str());
				//     SDL_Log("BC_SZ:: %d", _wraped_text->back().size());
				//     pos_in_text_ += rem_text_size;
				// }
				////SDL_Log("wrap max char");
				// SDL_Log("SND_TEXT: %d/%d", _text.size(), pos_in_text_);
				////wrap max char
				// try {
				//     u8wrap_max_char(_text.substr(pos_in_text_, _text.size()), _wraped_text, _max_char_per_line, _max_lines);
				// }
				// catch (std::exception exp) {
				//     std::cout << exp.what() << std::endl;
				// }
				wrap_done = true;
			}
		}
	}

	void wrap_max_char(const std::string &_text, std::deque<std::string> *_wrapped_text,
					   const std::size_t &_max_char_per_line, const std::size_t &_max_lines)
	{
		if (_text.empty())
			return;
		for (int i = 0; i < _max_lines; ++i)
		{
			const auto cursor_s = i * _max_char_per_line;
			if (cursor_s > _text.size())
				//    i = _max_lines;
				return;
			// else
			_wrapped_text->emplace_back(_text.substr(i * _max_char_per_line, _max_char_per_line));
			//// TODO: FIXME: not thread safe use std::mbrlen instead
			//  multibyte line size
			/*int mbline_s = 0;
            for (std::size_t cc = 0; cc < _max_char_per_line; ++cc)
            {
                // cursor pos
                const auto cp = cursor_s + cc;
                const int next = std::mblen(&_text[cp], _text.size - cp);
                if (next == -1)
                    throw std::runtime_error("strlen_mb(): conversion error");
                //  prevent incomplete mbchars at end of line
                if (mbline_s + next < _max_char_per_line)
                    mbline_s += next;
                cc += next;
            }
            _wrapped_text->emplace_back(_text.substr(cursor_s, mbline_s));*/
		}
	}

	void wrap_max_word_count(const std::string &_text, std::deque<std::string> *_wraped_text, const std::string_view _delimiters,
							 const std::size_t &_max_char_per_line, const std::size_t &_max_lines = 0)
	{
		std::string line = "", word = "";
		// std::vector<const std::string::size_type> delimiter_pos;
		size_t pos_in_text_ = 0;
		bool is_first_delim = true;
		bool wrap_done = false;
		while (!wrap_done)
		{
			is_first_delim = true;
			std::string::size_type nearest_delim_pos = 0;
			for (const auto &_del : _delimiters) // delimiter_pos.push_back(_text.find(_del, pos_in_text_));
			{
				const auto pos = _text.find(_del, pos_in_text_);
				if (!is_first_delim)
				{
					if (pos <= nearest_delim_pos)
						nearest_delim_pos = pos;
				}
				else
					nearest_delim_pos = pos, is_first_delim = false;
			}

			if (nearest_delim_pos != std::string::npos)
			{
				// check if the current line size plus the new sub-string is less than max chars per line
				// if so then add the substring at the back of the line
				if (line.size() + (nearest_delim_pos - pos_in_text_) <= _max_char_per_line)
				{
					line += _text.substr(pos_in_text_, nearest_delim_pos + 1 - pos_in_text_);
					pos_in_text_ = nearest_delim_pos + 1;
				}
				else
				{
					if (_wraped_text->size() >= _max_lines)
						return;
					// we have a line with the max possible words so push the line into the output container
					// then empty the line for new line processing

					// if line is empty, then the line is not splitable so add the whole max_chars_per_line line into the output container
					if (line.empty())
					{
						line = _text.substr(pos_in_text_, _max_char_per_line);
						pos_in_text_ += _max_char_per_line;
					}
					_wraped_text->emplace_back(line);
					// SDL_Log("mxc:%d - LNS:%d - NL:%s", _max_char_per_line, line.size(), line.c_str());
					line.clear();
					// check if next line is splitable if not add the whole max_chars_per_line line into the output container
				}
			}
			else
			{
				if (_wraped_text->size() >= _max_lines)
					return;
				if (!line.empty())
				{
					if (line.size() < _max_char_per_line)
					{
						// SDL_Log("pt: %d/%d", pos_in_text_, _text.size());
						const std::size_t rem_text_size = _max_char_per_line - line.size();
						line += _text.substr(pos_in_text_, rem_text_size);
						pos_in_text_ += rem_text_size + 1;
						pos_in_text_ = std::clamp(pos_in_text_, (std::size_t)0, _text.size());
						// SDL_Log("pt: %d/%d", pos_in_text_, _text.size());
						// SDL_Log("lns: %d/%d", line.size(), _max_char_per_line);
					}
					_wraped_text->emplace_back(line);
				}
				wrap_max_char(_text.substr(pos_in_text_, _text.size()), _wraped_text, _max_char_per_line, _max_lines);

				/*SDL_Log("RCV_TEXT: %s", (char*)_wraped_text->back().c_str());
                SDL_Log("SND_TEXT: %d/%d", _text.size(), pos_in_text_);
                SDL_Log("MX_CL: %d", _max_char_per_line);*/
				// if (_wraped_text->back().size() < _max_char_per_line) {
				//     const std::size_t rem_text_size = _max_char_per_line - _wraped_text->back().size();
				//     _wraped_text->back()+=(_text.substr(pos_in_text_, rem_text_size));
				//     SDL_Log("RCV_TEXT: %s", (char*)_wraped_text->back().c_str());
				//     SDL_Log("BC_SZ:: %d", _wraped_text->back().size());
				//     pos_in_text_ += rem_text_size;
				// }
				////SDL_Log("wrap max char");
				// SDL_Log("SND_TEXT: %d/%d", _text.size(), pos_in_text_);
				////wrap max char
				// try {
				//     u8wrap_max_char(_text.substr(pos_in_text_, _text.size()), _wraped_text, _max_char_per_line, _max_lines);
				// }
				// catch (std::exception exp) {
				//     std::cout << exp.what() << std::endl;
				// }
				wrap_done = true;
			}
		}
	}

	void Wrap(const std::string &_text, std::deque<std::string> *_wraped_text,
			  const int &_max_char_per_line, int _max_lines, const TextWrapStyle &tws)
	{
		// auto start = std::chrono::high_resolution_clock::now();
		std::string line = "", word = "";
		if (tws == TextWrapStyle::MAX_WORDS_PER_LN)
		{
			int pos_in_txt = 0;
			int idf_pos = 0;
			bool wrap_done = false;
			while (_wraped_text->size() <= _max_lines)
			{
				// whitespace pos
				auto ws_pos = _text.find(" ", pos_in_txt);
				auto hy_pos = _text.find("-", pos_in_txt);
				if (ws_pos != std::string::npos || hy_pos != std::string::npos)
				{
					if (ws_pos != std::string::npos)
						idf_pos = ws_pos;
					if (hy_pos != std::string::npos)
						idf_pos = hy_pos;
					if (ws_pos != std::string::npos || hy_pos != std::string::npos)
						hy_pos < ws_pos ? idf_pos = hy_pos : idf_pos = ws_pos;
					if ((1 + idf_pos - pos_in_txt) + line.size() <= _max_char_per_line)
					{
						word = _text.substr(pos_in_txt, 1 + idf_pos - pos_in_txt);
						line += word;
						pos_in_txt += word.size();
						++word_count;
						// SDL_Log("POS: %d - LINE: %s", word.size(), line.c_str());
					}
					else
					{
						if (line.empty())
							_wraped_text->emplace_back(
								_text.substr(pos_in_txt, _max_char_per_line)),
								pos_in_txt += _max_char_per_line;
						// SDL_Log("+EmptyLINE: %s", _wraped_text->back().c_str());
						else
							_wraped_text->emplace_back(line);
						// pos_in_txt += line.size(),
						// SDL_Log("LINE: %s",line.c_str());
						line = "";
					}
					if (_wraped_text->size() >= _max_lines)
						_max_lines = _wraped_text->size() - 1;
				}
				else
				{
					word = _text.substr(pos_in_txt, _text.size() - pos_in_txt);
					// SDL_Log("REM: %s", word.c_str());
					if (line.size() + word.size() <= _max_char_per_line)
					{
						line += word,
							_wraped_text->emplace_back(line);
					}
					else
					{
						if (!line.empty())
							_wraped_text->emplace_back(line);
						try
						{
							WrapMaxChar(word, _wraped_text, _max_char_per_line,
										_max_lines - _wraped_text->size());
						}
						catch (const std::exception &exp)
						{
							// SDL_Log("%s", exp.what());
							std::cout << exp.what() << std::endl;
						}
					}
					_max_lines = _wraped_text->size() - 1;
				}
			}
		}
		else
		{
			WrapMaxChar(_text, _wraped_text, _max_char_per_line, _max_lines);
		}

		/*std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - start);
        SDL_Log("text processing finished: %f secs", dt.count());
        SDL_Log("No wrapped lines: %d", _wraped_text->size());
        SDL_Log("WordCount: %d", word_count);*/
	}

	// Helper function to count UTF-8 codepoints in a string_view
	// Requires that the data in string_view is accessible and its length is correct in bytes.
	inline size_t count_utf8_codepoints(std::string_view sv)
	{
		if (sv.empty())
		{
			return 0;
		}
		// utf8nlen counts codepoints in the first k BYTES of s.
		// sv.data() gives const char*, sv.length() gives number of bytes.
		return utf8nlen((utf8_int8_t*)sv.data(), sv.length());
	}

	// Corrected helper function to get a segment of text by a maximum number of codepoints,
	// using the provided signature for utf8codepoint.
	inline std::string_view get_segment_by_codepoints(
		const char *text_start_ptr,		  // Start of the current piece of text to consider
		const char *text_overall_end_ptr, // Absolute end of the buffer for text_start_ptr (one past last valid char)
		size_t max_codepoints_to_take,
		const char **out_actual_segment_end_ptr) // Output: where the segment actually ended
	{
		const char *current_iter_ptr = text_start_ptr;	 // Pointer that will be advanced
		const char *segment_end_marker = text_start_ptr; // Marks the end of the valid segment found
		size_t codepoints_taken = 0;
		utf8_int32_t dummy_codepoint_value; // To pass to utf8codepoint, value not used in this helper

		while (codepoints_taken < max_codepoints_to_take && current_iter_ptr < text_overall_end_ptr)
		{
			// Pre-flight safety check: Determine potential length of the current codepoint
			// to ensure utf8codepoint doesn't read past text_overall_end_ptr.
			// This replicates the initial byte-checking logic from the provided utf8codepoint.
			if (*current_iter_ptr == '\0' && current_iter_ptr < text_overall_end_ptr)
			{
				// If we encounter a null terminator before the explicit buffer end,
				// treat it as the end of the string for this segment.
				// utf8codepoint would process it as a 1-byte char.
				// Depending on desired behavior, one might break or let it process.
				// Let's let it process, as \0 is a valid (though special) codepoint.
			}

			unsigned char first_byte = static_cast<unsigned char>(*current_iter_ptr);
			size_t expected_bytes_for_codepoint = 0;

			if ((first_byte & 0xF8) == 0xF0)
			{ // 4-byte sequence (matches 11110xxx)
				expected_bytes_for_codepoint = 4;
			}
			else if ((first_byte & 0xF0) == 0xE0)
			{ // 3-byte sequence (matches 1110xxxx)
				expected_bytes_for_codepoint = 3;
			}
			else if ((first_byte & 0xE0) == 0xC0)
			{ // 2-byte sequence (matches 110xxxxx)
				expected_bytes_for_codepoint = 2;
			}
			else
			{ // 1-byte sequence (matches 0xxxxxxx)
				expected_bytes_for_codepoint = 1;
			}

			// Ensure there are enough bytes remaining in the buffer for this codepoint
			if (current_iter_ptr + expected_bytes_for_codepoint > text_overall_end_ptr)
			{
				// Not enough data left in the buffer for the character indicated by first_byte.
				// Stop here; the segment ends before this potentially incomplete character.
				break;
			}

			// Call the user-specified utf8codepoint
			const char *next_iter_ptr = reinterpret_cast<const char *>(
				utf8codepoint(
					reinterpret_cast<const utf8_int8_t *>(current_iter_ptr),
					&dummy_codepoint_value));

			// The provided utf8codepoint implementation always advances the pointer by 1-4 bytes.
			// So, next_iter_ptr should always be > current_iter_ptr if a valid codepoint was read.
			// An explicit check for (next_iter_ptr == current_iter_ptr) would indicate an issue
			// if the function could somehow fail to advance, but based on its code, it always does.

			current_iter_ptr = next_iter_ptr;	   // Advance our main iterator
			segment_end_marker = current_iter_ptr; // The segment successfully extends to this new position
			codepoints_taken++;
		}

		*out_actual_segment_end_ptr = segment_end_marker;
		return std::string_view(text_start_ptr, segment_end_marker - text_start_ptr);
	}

	// Optimized and Unicode-aware version of wrap_max_char
	void wrap_max_char_unicode(
		const std::string &text,
		std::deque<std::string> *wrapped_text,
		const std::size_t max_codepoints_per_line,
		const std::size_t max_lines_limit // 0 for no limit on lines
	)
	{
		if (text.empty())
		{
			wrapped_text->clear();
			return;
		}
		if (max_codepoints_per_line == 0)
		{
			wrapped_text->clear();
			return;
		}

		wrapped_text->clear();

		const char *current_text_ptr = text.data(); // Use data() for consistency with length()
		const char *const overall_text_end_ptr = text.data() + text.length();
		size_t lines_added = 0;

		while (current_text_ptr < overall_text_end_ptr &&
			   (max_lines_limit == 0 || lines_added < max_lines_limit))
		{
			const char *line_actual_end_ptr = nullptr;
			std::string_view line_segment = get_segment_by_codepoints(
				current_text_ptr,
				overall_text_end_ptr,
				max_codepoints_per_line,
				&line_actual_end_ptr);

			if (line_segment.empty())
			{
				if (current_text_ptr < overall_text_end_ptr)
				{
					// std::cerr << "Warning: wrap_max_char_unicode - Empty segment returned, advancing 1 byte." << std::endl;
					current_text_ptr++;
					continue;
				}
				else
				{
					break;
				}
			}

			wrapped_text->emplace_back(line_segment.data(), line_segment.length());
			lines_added++;
			current_text_ptr = line_actual_end_ptr;
		}
	}

	// Optimized and Unicode-aware version of wrap_by_word_unicode
	void wrap_by_word_unicode(
		const std::string &text,
		std::deque<std::string> *wrapped_text,
		const std::string_view delimiters_sv,
		const std::size_t max_codepoints_per_line,
		const std::size_t max_lines_limit)
	{
		if (text.empty())
		{
			wrapped_text->clear();
			return;
		}
		if (max_codepoints_per_line == 0)
		{
			wrapped_text->clear();
			return;
		}

		wrapped_text->clear();

		std::string current_line_buffer;
		current_line_buffer.reserve(max_codepoints_per_line * 4);
		size_t current_line_codepoints = 0;
		size_t lines_added = 0;

		const char *p = text.data();
		const char *const overall_text_end_ptr = p + text.length();

		auto is_char_a_delimiter = [&](char c) {
			return delimiters_sv.find(c) != std::string_view::npos;
		};

		while (p < overall_text_end_ptr &&
			   (max_lines_limit == 0 || lines_added < max_lines_limit))
		{
			const char *segment_start_ptr = p;
			const char *segment_scan_ptr = p;

			while (segment_scan_ptr < overall_text_end_ptr && !is_char_a_delimiter(*segment_scan_ptr))
			{
				segment_scan_ptr++;
			}

			if (segment_scan_ptr < overall_text_end_ptr && is_char_a_delimiter(*segment_scan_ptr))
			{
				segment_scan_ptr++;
			}
			std::string_view segment_sv(segment_start_ptr, segment_scan_ptr - segment_start_ptr);
			size_t segment_codepoints = count_utf8_codepoints(segment_sv);

			if (segment_sv.empty())
			{
				if (p < overall_text_end_ptr)
				{
					// std::cerr << "Warning: wrap_by_word_unicode - Empty segment, advancing 1 byte." << std::endl;
					p++;
					continue;
				}
				else
				{
					break;
				}
			}

			if (current_line_codepoints + segment_codepoints <= max_codepoints_per_line)
			{
				current_line_buffer.append(segment_sv.data(), segment_sv.length());
				current_line_codepoints += segment_codepoints;
				p = segment_scan_ptr;
			}
			else
			{
				if (!current_line_buffer.empty())
				{
					wrapped_text->emplace_back(current_line_buffer);
					lines_added++;
					current_line_buffer.clear();
					current_line_codepoints = 0;
					if (max_lines_limit != 0 && lines_added >= max_lines_limit)
						break;
				}

				if (segment_codepoints <= max_codepoints_per_line)
				{
					current_line_buffer.append(segment_sv.data(), segment_sv.length());
					current_line_codepoints = segment_codepoints;
					p = segment_scan_ptr;
				}
				else
				{
					const char *hard_wrap_current_ptr = segment_start_ptr;
					while (hard_wrap_current_ptr < segment_scan_ptr &&
						   (max_lines_limit == 0 || lines_added < max_lines_limit))
					{
						const char *sub_segment_actual_end_ptr = nullptr;
						std::string_view sub_segment = get_segment_by_codepoints(
							hard_wrap_current_ptr,
							segment_scan_ptr,
							max_codepoints_per_line,
							&sub_segment_actual_end_ptr);

						if (sub_segment.empty())
						{
							if (hard_wrap_current_ptr < segment_scan_ptr)
							{
								// std::cerr << "Warning: wrap_by_word_unicode - Hard wrap yielded empty sub-segment, advancing." << std::endl;
								hard_wrap_current_ptr++;
							}
							else
							{
								break;
							}
							continue;
						}

						wrapped_text->emplace_back(sub_segment.data(), sub_segment.length());
						lines_added++;
						hard_wrap_current_ptr = sub_segment_actual_end_ptr;
					}
					p = hard_wrap_current_ptr;
					current_line_buffer.clear();
					current_line_codepoints = 0;
				}
			}
		}

		if (!current_line_buffer.empty() && (max_lines_limit == 0 || lines_added < max_lines_limit))
		{
			wrapped_text->emplace_back(current_line_buffer);
		}
	}

  private:
	TextProcessor() {}

	void U8WrapMaxChar(const std::u32string &_text, std::deque<std::u32string> *_wrapped_text,
					   const int &_max_char_per_line, const int &_max_lines)
	{
		if (_text.empty())
			return;
		std::u32string subtext;
		for (int i = 0; i < _max_lines; ++i)
		{
			try
			{
				if (i * _max_char_per_line > _text.size())
					i = _max_lines;
				else
					subtext = _text.substr(i * _max_char_per_line,
										   _max_char_per_line),
					_wrapped_text->emplace_back(subtext);
			}
			catch (const std::exception &excp)
			{
				std::cout << excp.what() << std::endl;
			}
		}
	}

	void WrapMaxChar(const std::string &_text, std::deque<std::string> *_wrapped_text,
					 const int &_max_char_per_line, const int &_max_lines)
	{
		if (_text.empty())
			return;
		std::string subtext;
		for (int i = 0; i < _max_lines; ++i)
		{
			try
			{
				if (i * _max_char_per_line > _text.size())
					i = _max_lines;
				else
					subtext = _text.substr(i * _max_char_per_line,
										   _max_char_per_line),
					_wrapped_text->emplace_back(subtext);
			}
			catch (const std::exception &excp)
			{
				std::cout << excp.what() << std::endl;
			}
		}
	}
};

std::vector<std::string> splitStringMaxCodePoints(const std::string &str, const std::string &delimiters, int maxLength, int maxSubstrings)
{
	std::vector<std::string> tokens;
	tokens.reserve(maxSubstrings);
	std::string token;

	// Create a lookup table of delimiters
	std::unordered_set<char32_t> isDelimiter;
	for (char c : delimiters)
	{
		isDelimiter.insert((char32_t)c);
	}

	std::setlocale(LC_CTYPE, "en_US.UTF-8");
	const char *p = str.c_str();
	size_t remaining = str.length();
	int numSubstrings = 0;
	while (remaining > 0 && numSubstrings < maxSubstrings)
	{
		wchar_t codePoint;
		int len = std::mbtowc(&codePoint, p, remaining);
		if (len < 0)
		{
			// error handling
		}
		if (isDelimiter.count((char32_t)codePoint) > 0)
		{
			if (token.length() > 0)
			{
				tokens.push_back(token);
				token.clear();
				numSubstrings++;
			}
		}
		else if (token.length() < maxLength)
		{
			token += codePoint;
		}
		else
		{
			tokens.push_back(token);
			token.clear();
			token += codePoint;
			numSubstrings++;
		}
		p += len;
		remaining -= len;
	}

	if (token.length() > 0 && numSubstrings < maxSubstrings)
	{
		tokens.push_back(token);
	}

	return tokens;
}

std::vector<std::string> splitStringMaxWords(const std::string &str, const std::string &delimiters, int maxLength, int maxSubstrings)
{
	std::vector<std::string> tokens;
	tokens.reserve(maxSubstrings);
	std::string token;

	// Create a lookup table of delimiters
	std::unordered_set<char32_t> isDelimiter;
	for (char c : delimiters)
	{
		isDelimiter.insert((char32_t)c);
	}

	std::setlocale(LC_CTYPE, "en_US.UTF-8");
	const char *p = str.c_str();
	size_t remaining = str.length();
	int numSubstrings = 0;
	while (remaining > 0 && numSubstrings < maxSubstrings)
	{
		wchar_t codePoint;
		int len = std::mbtowc(&codePoint, p, remaining);
		if (len < 0)
		{
			// error handling
		}
		if (isDelimiter.count((char32_t)codePoint) > 0)
		{
			if (token.length() > 0)
			{
				tokens.push_back(token);
				token.clear();
				numSubstrings++;
			}
		}
		else
		{
			token += codePoint;
			if (token.length() >= maxLength)
			{
				// Check if the token contains more than one full word
				size_t spacePos = token.find(' ');
				if (spacePos == std::string::npos)
				{
					// Token contains only one full word, add it as is
					tokens.push_back(token);
					token.clear();
					numSubstrings++;
				}
				else
				{
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

	if (token.length() > 0 && numSubstrings < maxSubstrings)
	{
		tokens.push_back(token);
	}

	return tokens;
}

std::vector<std::string> splitStringMaxWords2(const std::string &str, const std::string &delimiters, int maxCharCount)
{
	std::vector<std::string> sentences;
	std::string sentence;
	std::string word;
	std::mbstate_t mbstate{};
	int codePointCount = 0;

	for (char c : str)
	{
		if (delimiters.find(c) != std::string::npos)
		{
			// The current character is a delimiter, so add it to the current word
			word += c;
			codePointCount += std::mbrlen(&c, 1, &mbstate);
			if ((sentence.size() + word.size()) > maxCharCount || codePointCount > maxCharCount)
			{
				// The current word and delimiter do not fit in the current sentence, so add the current sentence to the list of sentences
				// and start a new sentence with the current word and delimiter.
				sentences.push_back(sentence);
				sentence.clear();
				sentence += word;
				word.clear();
				codePointCount = 0;
			}
			else
			{
				// The current word and delimiter fit in the current sentence, so add them to the sentence
				sentence += word;
				word.clear();
			}
		}
		else
		{
			// The current character is not a delimiter, so add it to the current word
			word += c;
			codePointCount += std::mbrlen(&c, 1, &mbstate);
		}
	}

	// Add the remaining sentence to the list of sentences
	if (!sentence.empty())
	{
		sentences.push_back(sentence);
	}

	return sentences;
}

std::vector<std::string> wrapText1(const std::string &text, size_t maxLineLength)
{
	std::vector<std::string> lines;
	size_t start = 0;
	while (start < text.length())
	{
		size_t end = start + maxLineLength;
		if (end > text.length())
		{
			end = text.length();
		}
		else
		{
			// Find the last space character before the maximum line length
			size_t spacePos = text.rfind(' ', end);
			if (spacePos != std::string::npos)
			{
				end = spacePos;
			}
		}
		lines.emplace_back(text.substr(start, end - start));
		start = end + 1;
	}
	return lines;
}

namespace FileSystem
{
class FileStore
{
  public:
	std::function<bool(const std::filesystem::path &)> filter = nullptr;

  public:
	FileStore() { dir_count = scanned_files = 0; }

	FileStore(const std::filesystem::path &root_path)
	{
		this->setRootPath(root_path);
	}

	FileStore &addExtensionFilter(const std::string &extension_filter_)
	{
		std::scoped_lock lock(m_file_sys_mux);
		this->extension_filter.insert({extension_filter_, 0});
		return *this;
	}

	std::string getFilePath(const uint64_t &index)
	{
		std::scoped_lock lock(m_file_sys_mux);
		if (index > m_files_store.size())
			return "";
		try
		{
			return {m_files_store[index].generic_string()};
		}
		catch (const std::exception &e)
		{
			return "";
		}
	}

	std::wstring getFileWPath(const unsigned int &index)
	{
		std::scoped_lock lock(m_file_sys_mux);
		if (index > m_files_store.size())
			return L"";
		try
		{
			return {m_files_store[index].generic_wstring()};
		}
		catch (const std::exception &e)
		{
			return L"";
		}
	}

	std::string getFileName(const size_t &index)
	{
		std::scoped_lock lock(m_file_sys_mux);
		try
		{
			return {m_files_store[index].stem().generic_string()};
		}
		catch (const std::exception &e)
		{
			std::cout << e.what() << std::endl;
			return "";
		}
	}

#ifdef _MSC_VER
	std::u8string getU8FilePath(const unsigned int &index)
	{
		std::scoped_lock lock(m_file_sys_mux);
		if (index > m_files_store.size())
			return u8"";
		try
		{
			return {m_files_store[index].generic_u8string()};
		}
		catch (const std::exception &e)
		{
			return u8"";
		}
	}

	std::u8string getU8FileName(const size_t &index)
	{
		std::scoped_lock lock(m_file_sys_mux);
		try
		{
			return m_files_store[index].stem().generic_u8string();
		}
		catch (const std::exception &e)
		{
			std::cout << e.what() << std::endl;
			return u8"";
		}
	}

#endif // _MSC_VER

	std::string getFileExtension(const int &index)
	{
		std::scoped_lock lock(m_file_sys_mux);
		try
		{
			return {m_files_store[index].extension().generic_string()};
		}
		catch (const std::exception &e)
		{
			return "";
		}
	}

	inline std::deque<std::filesystem::path> *getFileStore() noexcept
	{
		return &m_files_store;
	}

	inline FileStore &setRootPath(const std::filesystem::path &root_path) noexcept
	{
		this->m_root_path = root_path;
		return *this;
	}

	FileStore &scanFiles(std::size_t _recursion_depth = 1000000)
	{
		recursion_depth = _recursion_depth;
		std::scoped_lock lock(m_file_sys_mux);
		nWorkerComplete = 0;
		auto start = std::chrono::high_resolution_clock::now();
#if defined(__linux__)
		internalScanFilesLinux(m_root_path.string(), 0);
		/*;futures.emplace_back(
			//std::async(std::launch::async, &FileStore::internalScanFilesLinux, this, path, itter_depth + 1)
			Async::GThreadPool.enqueue(&FileStore::internalScanFilesLinux, this, m_root_path.string(), 0));*/
#else
		// internalScanFiles(m_root_path);
		//internalScanFilesThreadPool(m_root_path, 0);
		// for (const auto& fts : futures_)fts.wait();
		// futures_.clear();
		/*while (nWorkerComplete > 0)
		{
			SDL_Delay(1);
		}*/
#endif
		/*while (nWorkerComplete > 0) { SDL_Delay(50); }
		for (auto &ft : futures){
			ft.wait();
			//auto res=ft.get();
			addToFileStore(ft.get());
		}*/

		std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - start);
		SDL_Log("Dir scan finished: %f secs", dt.count());
		SDL_Log("No Scanned Files: %u", m_files_store.size());
		return *this;
	}

	inline FileStore &sort()
	{
		//std::scoped_lock lock(m_file_sys_mux);
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

	inline FileStore &sortByTime()
	{
		//std::scoped_lock lock(m_file_sys_mux);
		const auto start = std::chrono::high_resolution_clock::now();
		std::sort(m_files_store.begin(), m_files_store.end(),
				  [](std::filesystem::path &a, std::filesystem::path &b) {
					  return std::filesystem::last_write_time(a) >
							 std::filesystem::last_write_time(b);
				  });
		const std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - start);
		SDL_Log("Dir Sort finished: %f secs", dt.count());
		return *this;
	}

	/*
         * returns vector of a string & int pair where the string is the name of the file extension
         * and the int is the count of the files with that extension from the previous scanFiles() call
         * */
	inline std::unordered_map<std::string, int> *getExtensions()
	{
		std::scoped_lock lock(m_file_sys_mux);
		return &extension_filter;
	}

	inline size_t size() noexcept
	{
		std::scoped_lock lock(m_file_sys_mux);
		return this->m_files_store.size();
	}

	inline bool empty() noexcept
	{
		std::scoped_lock lock(m_file_sys_mux);
		return m_files_store.empty();
	}

	inline void clear()
	{
		std::scoped_lock lock(m_file_sys_mux);
		m_files_store.clear();
		dir_count = scanned_files = 0;
	}

	inline int getDirCount() noexcept
	{
		std::scoped_lock lock(m_file_sys_mux);
		return dir_count;
	}

	inline int getScannedFilesCount() noexcept
	{
		std::scoped_lock lock(m_file_sys_mux);
		return scanned_files;
	}

	FileStore &clearAndReset()
	{
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

	inline void addToFileStore(const std::filesystem::path &path_)
	{
		//std::scoped_lock lock(filestore_mutex);
		// using namespace std::chrono_literals;
		// std::this_thread::sleep_for(25ms);

		if (filter != nullptr)
		{
			if (!filter(path_))
				return;
		}
		m_files_store.emplace_back(path_);
	}

	inline void addToFileStore(const std::deque<std::filesystem::path> &local_files)
	{
		if (not local_files.empty())
		{
			//std::scoped_lock lock(filestore_mutex);
			// using namespace std::chrono_literals;
			// std::this_thread::sleep_for(25ms);

			for (auto &file_ : local_files)
			{
				if (filter != nullptr)
				{
					if (!filter(file_))
						return;
				}
				m_files_store.emplace_back(file_);
			}
			/*
		m_files_store.insert(m_files_store.end(),
							 std::make_move_iterator(local_files.begin()),
							 std::make_move_iterator(local_files.end()));*/
		}
	}

	/*
		void addToFileStoreBatch(std::deque<std::filesystem::path> &local_files)
	{
		if (local_files.empty())
			return;

		std::scoped_lock lock(m_mutex);
		m_files_store.insert(m_files_store.end(),
							 std::make_move_iterator(local_files.begin()),
							 std::make_move_iterator(local_files.end()));
		for (const auto &pair : local_extensions)
		{
			if (extension_filter.count(pair.first))
			{
				extension_filter[pair.first] += pair.second;
			}
		}
		local_files.clear();
		local_extensions.clear();
	}
*/

	void internalScanFiles(const std::filesystem::path path_, std::size_t itter_depth)
	{
		//std::vector<std::future<void>> futures;
		// std::deque<std::filesystem::path> files_;
		try
		{
			// std::filesystem::path d_path_;
			for (const auto &dir_ent : std::filesystem::directory_iterator(path_, std::filesystem::directory_options::skip_permission_denied))
			{
				const auto &d_path_ = dir_ent.path();
				if (std::filesystem::is_directory(d_path_))
					if (itter_depth < recursion_depth)
					{
						//futures.emplace_back(
						/*Async::GThreadPool.enqueue(&FileStore::internalScanFilesThreadPool, this, d_path_)*/
						//std::async(std::launch::async, &FileStore::internalScanFiles, this, d_path_, itter_depth + 1));
					}
					else
					{
						if (extension_filter.empty())
							addToFileStore(d_path_);
						else
						{
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
		/*for (const auto &fts : futures)
			fts.wait();*/
	}

	void internalScanFilesThreadPool(const std::filesystem::path path_)
	{
		nWorkerComplete += 1;
		std::vector<std::future<void>> futures;
		// std::deque<std::filesystem::path> files_;
		try
		{
			for (const auto &dir_ent : std::filesystem::directory_iterator(path_, std::filesystem::directory_options::skip_permission_denied))
			{
				const auto& d_path_ = dir_ent.path();
				if (std::filesystem::is_directory(d_path_))
					futures.emplace_back(Async::GThreadPool.enqueue(&FileStore::internalScanFilesThreadPool, this, d_path_));
				else
				{
					if (extension_filter.empty())
						addToFileStore(d_path_);
					else
					{
						const auto f_extn = d_path_.extension().string();
						std::scoped_lock lock(extension_filter_mux);
						if (extension_filter.contains(f_extn))
						{
							addToFileStore(d_path_);
							// files_.emplace_back(d_path_);
							extension_filter[f_extn] += 1;
						}
						// for (auto& [extension_name, extension_count] : extension_filter) {
						//     if (extension_name == f_extn) {
						//         addToFileStore(d_path_);
						//         //files_.emplace_back(d_path_);
						//         std::scoped_lock lock(extension_filter_mux);
						//         ++extension_count;
						//         break;
						//     }
						// }
					}
				}
			}
			// addToFileStore(std::move(files_));
		}
		catch (std::exception e)
		{
			std::cout << e.what() << "\n";
		}
		nWorkerComplete -= 1;
	}

#ifdef __ANDROID__
	void internalScanFilesLinux(std::string dir_name, std::size_t itter_depth)
	{
		//nWorkerComplete += 1;
		DIR *dir;			  // pointer to directory
		struct dirent *entry; // all stuff in the directory
		struct stat info;	  // info about each entry

		dir = opendir(dir_name.c_str());
		std::vector<std::future<void>> futures;
		std::deque<std::filesystem::path> local_files;

		if (!dir)
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error while opening Dir: %s \n%s", dir_name.c_str(), strerror(errno));
			return;
		}

		while ((entry = readdir(dir)) != nullptr)
		{
			//if (entry->d_name[0] != '.')
			{
				std::string path = dir_name + "/" + std::string(entry->d_name);
				stat(path.c_str(), &info);
				if (S_ISDIR(info.st_mode))
				{
					if (itter_depth < recursion_depth)
						futures.emplace_back(
							//std::async(std::launch::async, &FileStore::internalScanFilesLinux, this, path, itter_depth + 1)
							Async::GThreadPool.enqueue(&FileStore::internalScanFilesLinux, this, path, itter_depth + 1));
				}
				else
				{
					if (extension_filter.empty())
						local_files.emplace_back(std::move(path));
					else
					{
						// const auto extension_pos=path.find_last_of('.');
						const char *extracted_extension_ = strrchr(path.c_str(), '.');
						std::scoped_lock lock(extension_filter_mux);
						if (extracted_extension_ && extension_filter.contains(extracted_extension_))
						{
							local_files.emplace_back(std::move(path));
							//std::scoped_lock lock(extension_filter_mux);
							extension_filter[extracted_extension_] += 1;
						}
					}
				}
			}
		}
		closedir(dir);
		addToFileStore(local_files);
		for (const auto &ft : futures)
			ft.wait();
		//nWorkerComplete -= 1;
		//return local_files;
	}
#endif //__ANDROID__

	std::mutex filestore_mutex;
	std::shared_mutex extension_filter_mux;
	std::deque<std::filesystem::path> m_files_store;
	//std::deque<std::deque<std::filesystem::path>> m_files_store_all;
	std::unordered_map<std::string, int> extension_filter;
	//std::vector<std::future<std::deque<std::filesystem::path>>> futures;
	std::filesystem::path m_root_path;
	std::atomic_int_fast32_t dir_count, scanned_files;
	std::shared_mutex m_file_sys_mux;
	// std::vector<std::future<void>> futures_;
	std::atomic_int_fast32_t nWorkerComplete;
	std::size_t recursion_depth = 1000000;
};
} // namespace FileSystem

#include <iostream>
#include <filesystem>
#include <functional>
#include <vector>
#include <string>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <chrono>
#include <algorithm>
#include <atomic>
#include <condition_variable>

#ifdef __ANDROID__
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <cerrno>
#include <SDL2/SDL_log.h>
#endif
#ifdef _MSC_VER
#include <SDL_Log.h>
#else
#include <SDL2/SDL_log.h>
#endif

template <typename T>
class ConcurrentQueue
{
  private:
	std::deque<T> queue_;
	std::mutex mutex_;
	std::condition_variable cv_;
	std::atomic_bool finished_ = false;

  public:
	void push(T item)
	{
		{
			std::scoped_lock lock(mutex_);
			queue_.push_back(std::move(item));
		}
		cv_.notify_one();
	}

	bool pop(T &item)
	{
		std::unique_lock lock(mutex_);
		cv_.wait(lock, [this] { return !queue_.empty() || finished_.load(); });
		if (queue_.empty() && finished_.load())
		{
			return false;
		}
		if (queue_.empty())
		{
			return false;
		}
		item = std::move(queue_.front());
		queue_.pop_front();
		return true;
	}

	void finish()
	{
		finished_ = true;
		cv_.notify_all();
	}

	void reset()
	{
		std::scoped_lock lock(mutex_);
		finished_ = false;
		queue_.clear();
	}

	bool empty()
	{
		std::scoped_lock lock(mutex_);
		return queue_.empty();
	}
};

namespace FileSystem3
{
using PathTask = std::pair<std::filesystem::path, size_t>;

class FileStore
{
	// ... (Keep all public methods as they were: constructors, getters, addExtensionFilter, sort, etc.) ...
	// ... (They were mostly fine, just ensure they use m_mutex) ...
  public:
	std::function<bool(const std::filesystem::path &)> filter = nullptr;

  public:
	FileStore() = default;

	FileStore(const std::filesystem::path &root_path) : m_root_path(root_path) {}

	FileStore &addExtensionFilter(const std::string &extension_filter_)
	{
		std::scoped_lock lock(m_mutex);
		extension_filter.insert({extension_filter_, 0});
		return *this;
	}

	std::string getFilePath(const size_t &index)
	{ /* ... keep old implementation ... */
		std::scoped_lock lock(m_mutex);
		return (index < m_files_store.size()) ? m_files_store[index].generic_string() : "";
	}
	std::wstring getFileWPath(const size_t &index)
	{ /* ... keep old implementation ... */
		std::scoped_lock lock(m_mutex);
		return (index < m_files_store.size()) ? m_files_store[index].generic_wstring() : L"";
	}
	std::string getFileName(const size_t &index)
	{
		return (index < m_files_store.size()) ? m_files_store[index].stem().generic_string() : "";
	}
	std::string getFileExtension(const size_t &index)
	{
		return (index < m_files_store.size()) ? m_files_store[index].extension().generic_string() : "";
	}

	std::string getFileNameWithExtension(const size_t &index)
	{
		return getFileName(index) + getFileExtension(index);
	}

	inline const std::deque<std::filesystem::path> *getFileStore() const noexcept { return &m_files_store; }
	inline FileStore &setRootPath(const std::filesystem::path &root_path) noexcept
	{
		std::scoped_lock lock(m_mutex);
		this->m_root_path = root_path;
		return *this;
	}
	inline const std::unordered_map<std::string, int> *getExtensions() const { return &extension_filter; }
	inline size_t size() noexcept
	{
		std::scoped_lock lock(m_mutex);
		return this->m_files_store.size();
	}
	inline bool empty() noexcept
	{
		std::scoped_lock lock(m_mutex);
		return m_files_store.empty();
	}
	inline void clear()
	{
		std::scoped_lock lock(m_mutex);
		m_files_store.clear();
		dir_count = scanned_files = 0;
		for (auto &pair : extension_filter)
			pair.second = 0;
	}
	inline int getDirCount() noexcept { return dir_count.load(); }
	inline int getScannedFilesCount() noexcept { return scanned_files.load(); }
	FileStore &clearAndReset()
	{
		std::scoped_lock lock(m_mutex);
		m_root_path.clear();
		m_files_store.clear();
		extension_filter.clear();
		dir_count = scanned_files = 0;
		dir_queue.reset();
		return *this;
	}
	FileStore &sort()
	{ /* ... keep old implementation ... */
		std::scoped_lock lock(m_mutex);
		const auto start = std::chrono::high_resolution_clock::now();
		std::sort(m_files_store.begin(), m_files_store.end(), [](const std::filesystem::path &a, const std::filesystem::path &b) { const std::string aa = a.stem().string(); const std::string bb = b.stem().string(); return std::lexicographical_compare(aa.begin(), aa.end(), bb.begin(), bb.end(), [](const char &c1, const char &c2) { return std::tolower(c1) < std::tolower(c2); }); });
		const std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - start);
		SDL_Log("Dir Sort finished: %f secs", dt.count());
		return *this;
	}

	inline FileStore &sortByTime2()
	{
		//std::scoped_lock lock(m_file_sys_mux);
		const auto start = std::chrono::high_resolution_clock::now();
		std::sort(m_files_store.begin(), m_files_store.end(),
				  [](std::filesystem::path &a, std::filesystem::path &b) {
					  return std::filesystem::last_write_time(a) >
							 std::filesystem::last_write_time(b);
				  });
		const std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - start);
		SDL_Log("Dir Sort finished: %f secs", dt.count());
		return *this;
	}

	inline FileStore &sortByTime()
	{
		//std::scoped_lock lock(m_file_sys_mux); // CRITICAL: Protect access to m_files_store
		const auto start_time_measurement = std::chrono::high_resolution_clock::now();

		struct FileTimeInfo
		{
			std::filesystem::path path;
#ifdef __ANDROID__
			struct timespec modification_time;

			// Comparator for Android (newest first)
			bool operator<(const FileTimeInfo &other) const
			{
				if (modification_time.tv_sec != other.modification_time.tv_sec)
				{
					return modification_time.tv_sec > other.modification_time.tv_sec;
				}
				return modification_time.tv_nsec > other.modification_time.tv_nsec;
			}
#else
			std::filesystem::file_time_type modification_time;

			// Comparator for non-Android (newest first)
			bool operator<(const FileTimeInfo &other) const
			{
				return modification_time > other.modification_time;
			}
#endif
		};

		std::vector<FileTimeInfo> files_with_times;
		if (!m_files_store.empty())
		{
			files_with_times.reserve(m_files_store.size());
		}

		for (auto &p : m_files_store)
		{
#ifdef __ANDROID__
			struct stat64 file_stat;
			// Ensure the path string is suitable for C system calls.
			// p.string() converts to generic string format, then .c_str().
			if (stat64(p.string().c_str(), &file_stat) == 0)
			{
				files_with_times.push_back({p, file_stat.st_mtim});
			}
			else
			{
				// SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "FileStore::sortByTime (Android): stat64 failed for '%s': %s",
				//              p.string().c_str(), strerror(errno));
				files_with_times.push_back({p, {0, 0}}); // Epoch time as fallback
			}
#else
			std::error_code ec;
			auto last_write = std::filesystem::last_write_time(p, ec);
			if (!ec)
			{
				files_with_times.push_back({p, last_write});
			}
			else
			{
				// SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "FileStore::sortByTime: last_write_time failed for '%s': %s",
				//              p.string().c_str(), ec.message().c_str());
				files_with_times.push_back({p, std::filesystem::file_time_type::min()});
			}
#endif
		}

		std::sort(files_with_times.begin(), files_with_times.end());

		m_files_store.assign(files_with_times.size(), std::filesystem::path{}); // Resize deque
		// Or m_files_store.clear(); then push_back. Assign might be slightly better if size is known.
		// For deque, clear and push_back is fine and often clearer.
		m_files_store.clear();
		for (const auto &fti : files_with_times)
		{
			m_files_store.push_back(fti.path);
		}

		const std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - start_time_measurement);
		SDL_Log("Dir Sort by Time finished: %f secs, %zu files sorted", dt.count(), files_with_times.size());

		return *this;
	}

	FileStore &scanFiles(std::size_t _recursion_depth = 0)
	{
		m_recursion_depth = _recursion_depth; // Store the depth
		dir_count = 0;
		scanned_files = 0;

		{
			std::scoped_lock lock(m_mutex);
			m_files_store.clear();
			for (auto &pair : extension_filter)
				pair.second = 0;
		}

		dir_queue.reset();
		tasks_pending = 0;

		auto start = std::chrono::high_resolution_clock::now();

		// Push root path with depth 0
		tasks_pending++;
		dir_queue.push({m_root_path, 0});

		unsigned int num_threads = std::thread::hardware_concurrency();
		if (num_threads == 0)
			num_threads = 4;

		std::vector<std::thread> workers;

		for (unsigned int i = 0; i < num_threads; ++i)
		{
			workers.emplace_back([this] {
#ifdef __ANDROID__
				worker_task_linux();
#else
				worker_task_filesystem();
#endif
			});
		}

		for (auto &worker : workers)
		{
			if (worker.joinable())
				worker.join();
		}

		auto dt = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start);
		SDL_Log("Dir scan finished: %f secs", dt.count());
		SDL_Log("No Files Found: %zu", m_files_store.size());
		SDL_Log("No Dirs Scanned: %d", dir_count.load());
		SDL_Log("No Files Scanned: %d", scanned_files.load());

		return *this;
	}

  private:
	// --- Keep helper methods (addToFileStoreBatch, checkExtension, checkExtensionLinux) ---
	inline void addToFileStoreBatch(std::deque<std::filesystem::path> &local_files, std::unordered_map<std::string, int> &local_extensions)
	{
		if (local_files.empty())
			return;
		std::scoped_lock lock(m_mutex);
		m_files_store.insert(m_files_store.end(), std::make_move_iterator(local_files.begin()), std::make_move_iterator(local_files.end()));
		for (auto &pair : local_extensions)
		{
			//if (extension_filter.count(pair.first))
			//{
			extension_filter[pair.first] += pair.second;
			//}
		}
		local_files.clear();
		local_extensions.clear();
	}
	inline bool checkExtension(const std::filesystem::path &path, std::string &out_ext)
	{ /* ... keep old implementation ... */
		if (!path.has_extension())
			return false;
		out_ext = path.extension().string();
		return extension_filter.empty() || extension_filter.count(out_ext);
	}
#ifdef __ANDROID__
	inline bool checkExtensionLinux(const char *filename, std::string &out_ext)
	{ /* ... keep old implementation ... */
		const char *dot = strrchr(filename, '.');
		if (!dot || dot == filename)
			return false;
		out_ext = std::string(dot);
		return extension_filter.empty() || extension_filter.count(out_ext);
	}
#endif

	// --- Worker Tasks (Updated) ---
#ifdef __ANDROID__
	void worker_task_linux()
	{
		std::deque<std::filesystem::path> local_files;
		std::unordered_map<std::string, int> local_extensions;
		PathTask current_task;
		const size_t BATCH_SIZE = 1000;

		while (dir_queue.pop(current_task))
		{
			auto &[current_path_fs, current_depth] = current_task;
			std::string current_path_str = current_path_fs.string();
			DIR *dir = opendir(current_path_str.c_str());

			if (!dir)
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot open Dir: %s [%s]", current_path_str.c_str(), strerror(errno));
				if (--tasks_pending == 0)
				{
					dir_queue.finish();
				}
				continue;
			}

			dir_count++;
			struct dirent *entry;
			std::vector<std::filesystem::path> sub_dirs;

			while ((entry = readdir(dir)) != nullptr)
			{
				if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
					continue;

				std::string full_path_str = current_path_str + "/" + entry->d_name;
				std::filesystem::path p(full_path_str);
				bool is_dir = false;
				if (entry->d_type != DT_UNKNOWN && entry->d_type != DT_LNK)
				{
					is_dir = (entry->d_type == DT_DIR);
				}
				else
				{
					struct stat info;
					if (stat(full_path_str.c_str(), &info) == 0)
					{
						is_dir = S_ISDIR(info.st_mode);
					}
					else
					{
						SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot stat: %s [%s]", full_path_str.c_str(), strerror(errno));
						continue;
					}
				}

				if (is_dir)
				{
					if (current_depth < m_recursion_depth)
					{
						sub_dirs.push_back(p);
					}
				}
				else
				{
					scanned_files++;
					std::string ext;
					if (checkExtensionLinux(entry->d_name, ext))
					{
						if (filter == nullptr || filter(p))
						{
							local_files.emplace_back(std::move(p));
							if (!ext.empty())
								local_extensions[ext]++;
						}
					}
				}
			}
			closedir(dir);

			size_t next_depth = current_depth + 1;
			for (const auto &sub_dir : sub_dirs)
			{
				tasks_pending++;
				dir_queue.push({sub_dir, next_depth});
			}

			if (local_files.size() >= BATCH_SIZE)
			{
				addToFileStoreBatch(local_files, local_extensions);
			}

			if (--tasks_pending == 0)
			{
				dir_queue.finish();
			}
		}
		addToFileStoreBatch(local_files, local_extensions);
	}
#else
	void worker_task_filesystem()
	{
		std::deque<std::filesystem::path> local_files;
		std::unordered_map<std::string, int> local_extensions;
		PathTask current_task;
		const size_t BATCH_SIZE = 200;

		while (dir_queue.pop(current_task))
		{
			auto &[current_path, current_depth] = current_task;
			std::error_code ec;
			std::filesystem::directory_iterator dir_iter(current_path,
														 std::filesystem::directory_options::skip_permission_denied,
														 ec);

			if (ec)
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot open Dir: %s [%s]", current_path.c_str(), ec.message().c_str());
				if (--tasks_pending == 0)
				{
					dir_queue.finish();
				}
				continue;
			}

			dir_count++;
			std::vector<std::filesystem::path> sub_dirs;

			for (const auto &entry : dir_iter)
			{
				try
				{
					if (entry.is_directory(ec) && !ec)
					{
						if (current_depth < m_recursion_depth)
						{
							sub_dirs.push_back(entry.path());
						}
					}
					else if (entry.is_regular_file(ec) && !ec)
					{
						scanned_files++;
						std::string ext;
						if (checkExtension(entry.path(), ext))
						{
							if (filter == nullptr || filter(entry.path()))
							{
								local_files.emplace_back(entry.path());
								if (!ext.empty())
									local_extensions[ext]++;
							}
						}
					}
				}
				catch (const std::filesystem::filesystem_error &e)
				{
					SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Filesystem error: %s", e.what());
				}
			}

			size_t next_depth = current_depth + 1;
			for (const auto &sub_dir : sub_dirs)
			{
				tasks_pending++;
				dir_queue.push({sub_dir, next_depth});
			}

			if (local_files.size() >= BATCH_SIZE)
			{
				addToFileStoreBatch(local_files, local_extensions);
			}

			if (--tasks_pending == 0)
			{
				dir_queue.finish();
			}
		}
		addToFileStoreBatch(local_files, local_extensions);
	}
#endif // __ANDROID__

  private:
	std::deque<std::filesystem::path> m_files_store;
	std::unordered_map<std::string, int> extension_filter;
	std::filesystem::path m_root_path;
	std::atomic_int dir_count = 0;
	std::atomic_int scanned_files = 0;
	std::mutex m_mutex;
	std::size_t m_recursion_depth = 0;

	ConcurrentQueue<PathTask> dir_queue;
	std::atomic_int tasks_pending = 0;
};
} // namespace FileSystem3
