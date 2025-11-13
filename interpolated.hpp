#pragma once
#include <chrono>

/// The currently supported transition functions
enum class TransitionFunction
{
	None,
	Linear,
	EaseInOutExponential,
	EaseOutBack,
	EaseInBack,
	EaseOutElastic,
	EaseOutQuad,
};

#include <cstdint>
#include <cmath>

float simplePow(float x, uint32_t p)
{
	float res = 1.0f;
	for (uint32_t i(p); i--;)
	{
		res *= x;
	}
	return res;
}

float linear(float t)
{
	return t;
}

float easeInOutExponential(float t)
{
	if (t < 0.5f)
	{
		return std::pow(2.0f, 20.0f * t - 10.0f) * 0.5f;
	}
	return (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) * 0.5f;
}

//float easeOutQuad(float x)
//{
//	return (1 - x) * (1 - x);
//}

// Correct easeOutQuad : f(x) = 1 - (1-x)^2 = 2x - x^2
static float easeOutQuad(float x)
{
	return 1.0f - (1.0f - x) * (1.0f - x);
}

float easeOutCubic(float x)
{
	return 1 - std::pow(1 - x, 3);
}

float easeOutQuint(float x)
{
	return 1 - std::pow(1 - x, 5);
}
/*

function easeOutCirc(x: number): number {
return Math.sqrt(1 - Math.pow(x - 1, 2));
}
*/

float easeOutBack(float t)
{
	constexpr float c1 = 1.70158f;
	constexpr float c3 = c1 + 1.0f;
	return 1.0f + c3 * simplePow(t - 1.0f, 3) + c1 * simplePow(t - 1.0f, 2);
}

float easeInOutQuint(float t)
{
	if (t < 0.5f)
	{
		return 16.0f * simplePow(t, 5);
	}
	return 1.0f - simplePow(-2.0f * t + 2, 5) * 0.5f;
}

float easeInBack(float t)
{
	float constexpr c1 = 1.70158f;
	float constexpr c3 = c1 + 1.0f;
	return c3 * t * t * t - c1 * t * t;
}

float easeOutElastic(float t)
{
	float constexpr two_pi = 2.0f * 3.14159265359f;
	float constexpr c4 = two_pi / 3.0f;
	if (t == 0.0f)
	{
		return 0.0f;
	}
	if (t == 1.0f)
	{
		return 1.0f;
	}
	return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
}

float getRatio(float t, TransitionFunction transition)
{
	switch (transition)
	{
	default:
		return t;
	case TransitionFunction::None:
		return 1.0f;
	case TransitionFunction::Linear:
		return t;
	case TransitionFunction::EaseInOutExponential:
		return easeInOutExponential(t);
	case TransitionFunction::EaseOutBack:
		return easeOutBack(t);
	case TransitionFunction::EaseOutElastic:
		return easeOutElastic(t);
	case TransitionFunction::EaseOutQuad:
		return easeOutQuad(t);
		case TransitionFunction::EaseInBack:
		return easeInBack(t);
	}
}

/** An object that implements automatic interpolation on value changes.
 *  It can be used as a drop in replacement thanks to cast and assign operators.
 */
template <typename T>
struct Interpolated
{
	/// The value at the start of the transition
	T start{};
	/// The target value
	T end{};
	/// The transition start timestamp
	float start_time{};
	/// The animation's speed
	float speed{1.0f};
	/// The transition function to use
	TransitionFunction transition{TransitionFunction::Linear};

	bool is_done = false;
	/// Initializes the value with @p initial_value
	explicit Interpolated(T const &initial_value = 0)
		: start{initial_value}, end{start}
	{
	}

	/// Returns stop watch time (should be better compared to UTC timestamps for float precision)
	[[nodiscard]] static float getCurrentTime()
	{
		// Retrieve current time
		auto const now = std::chrono::steady_clock::now();
		auto const duration = now.time_since_epoch();
		// Convert it to a decimal number of seconds
		auto const seconds = std::chrono::duration_cast<std::chrono::duration<float>>(duration);
		return seconds.count();
	}

	/// Returns the number of seconds since the last value change
	[[nodiscard]] float getElapsedSeconds()
	{
		return getCurrentTime() - start_time;
	}

	/// Sets a new target value and resets transition
	void setValue(T const &new_value)
	{
		start = 0; //getValue();
		end = new_value;
		start_time = getCurrentTime();
		is_done = false;
	}

	void setTransitionFunction(TransitionFunction _transition)
	{
		transition = _transition;
	}

	/// Returns the current value
	[[nodiscard]] T getValue()
	{
		if (is_done)
		{
			// If the transition is done, directly return the target value
			return 0; //end;
		}
		// Current transition time
		float const elapsed = getElapsedSeconds();
		float const t = elapsed * speed;
		// Check if the transition is over
		if (t >= 1.0f)
		{
			is_done = true;
			// If the transition is done, directly return the target value
			return 0; //end;
		}
		// Else compute interpolated value and return it
		T const delta{end - start};
		return /*start + */ delta * getRatio(t, transition);
	}

	/// Computes the speed given a duration
	void setDuration(float duration)
	{
		speed = 1.0f / duration;
	}

	bool isDone() const
	{
		return is_done;
	}

	void stop()
	{
		is_done = true;
	}

	/// Cast operator to use this object directly as if it was of type T
	[[nodiscard]]
	operator T()
	{
		return getValue();
	}

	/// Assign operator to ease transitions
	void operator=(T const &new_value)
	{
		setValue(new_value);
	}
};








/*
I took a breath and reviewed the correct active file.I fixed the interpolator implementation and suggested a small, safe change to how the CellBlock starts the fling(so the "fly" / fling scroll is stable).Apply both edits.
1.	Replace interpolated.hpp with this corrected implementation(fixes: proper start / end handling, correct easing mapping, correct easeOutQuad, getValue returns end when finished, setValue uses current value as start).

cpp ..\..\Desktop\GM's\volt m win\interpolated.hpp
#pragma once
*

#include <chrono>
#include <cstdint>
#include <cmath>
#include <algorithm>

/// The currently supported transition functions
enum class TransitionFunction
{
	None,
	Linear,
	EaseInOutExponential,
	EaseOutBack,
	EaseInBack,
	EaseOutElastic,
	EaseOutQuad,
};

static float simplePow(float x, uint32_t p)
{
	float res = 1.0f;
	for (uint32_t i = 0; i < p; ++i)
		res *= x;
	return res;
}

static float linear(float t) { return t; }

static float easeInOutExponential(float t)
{
	if (t < 0.5f)
		return std::pow(2.0f, 20.0f * t - 10.0f) * 0.5f;
	return (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) * 0.5f;
}

// Correct easeOutQuad : f(x) = 1 - (1-x)^2 = 2x - x^2
static float easeOutQuad(float x)
{
	return 1.0f - (1.0f - x) * (1.0f - x);
}

static float easeOutCubic(float x)
{
	return 1.0f - std::pow(1.0f - x, 3);
}

static float easeOutQuint(float x)
{
	return 1.0f - std::pow(1.0f - x, 5);
}

static float easeOutBack(float t)
{
	constexpr float c1 = 1.70158f;
	constexpr float c3 = c1 + 1.0f;
	// standard easeOutBack: 1 + c3*(t-1)^3 + c1*(t-1)^2
	return 1.0f + c3 * simplePow(t - 1.0f, 3) + c1 * simplePow(t - 1.0f, 2);
}

static float easeInOutQuint(float t)
{
	if (t < 0.5f) return 16.0f * simplePow(t, 5);
	return 1.0f - simplePow(-2.0f * t + 2.0f, 5) * 0.5f;
}

static float easeInBack(float t)
{
	constexpr float c1 = 1.70158f;
	constexpr float c3 = c1 + 1.0f;
	return c3 * t * t * t - c1 * t * t;
}

static float easeOutElastic(float t)
{
	constexpr float two_pi = 2.0f * 3.14159265358979323846f;
	constexpr float c4 = two_pi / 3.0f;
	if (t == 0.0f) return 0.0f;
	if (t == 1.0f) return 1.0f;
	return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
}

static float getRatioFromFunction(float t, TransitionFunction transition)
{
	switch (transition)
	{
	default:
	case TransitionFunction::None:
	case TransitionFunction::Linear:
		return linear(t);
	case TransitionFunction::EaseInOutExponential:
		return easeInOutExponential(t);
	case TransitionFunction::EaseOutBack:
		return easeOutBack(t);
	case TransitionFunction::EaseOutElastic:
		return easeOutElastic(t);
	case TransitionFunction::EaseOutQuad:
		return easeOutQuad(t);
	case TransitionFunction::EaseInBack:
		return easeInBack(t);
	}
}

 //An object that implements automatic interpolation on value changes.
 // It can be used as a drop in replacement thanks to cast and assign operators.

template <typename T>
struct Interpolated
{
	/// The value at the start of the transition
	T start{};
	/// The target value
	T end{};
	/// The transition start timestamp
	float start_time{};
	/// The animation's speed (1 / duration)
	float speed{ 1.0f };
	/// The transition function to use
	TransitionFunction transition{ TransitionFunction::Linear };

	bool is_done = true; // start as done to avoid accidental animation

	explicit Interpolated(T const& initial_value = static_cast<T>(0))
		: start{ initial_value }, end{ initial_value }, start_time{ getCurrentTime() }, is_done(true)
	{
	}

	/// Returns stop watch time (steady_clock for precision)
	[[nodiscard]] static float getCurrentTime()
	{
		auto const now = std::chrono::steady_clock::now();
		auto const duration = now.time_since_epoch();
		auto const seconds = std::chrono::duration_cast<std::chrono::duration<float>>(duration);
		return seconds.count();
	}

	/// Returns the number of seconds since the last value change
	[[nodiscard]] float getElapsedSeconds() const
	{
		return getCurrentTime() - start_time;
	}

	/// Sets a new target value and resets transition. Start is taken from current computed value.
	void setValue(T const& new_value)
	{
		// compute current value as start so the animation continues smoothly from present display value
		start = getValue();
		end = new_value;
		start_time = getCurrentTime();
		is_done = false;
	}

	void setTransitionFunction(TransitionFunction _transition)
	{
		transition = _transition;
	}

	/// Returns the current value (interpolated)
	[[nodiscard]] T getValue() const
	{
		if (is_done)
		{
			return end;
		}
		float const elapsed = getElapsedSeconds();
		float const t = elapsed * speed; // speed = 1/duration
		if (t >= 1.0f)
		{
			// animation finished
			// Note: mutable is_done not possible in const; caller can call isDone(); But here return end.
			return end;
		}
		float ratio = getRatioFromFunction(t, transition);
		T delta = end - start;
		return static_cast<T>(start + delta * ratio);
	}

	/// Computes the speed given a duration in seconds (duration must be > 0)
	void setDuration(float duration_seconds)
	{
		if (duration_seconds <= 0.0f)
			duration_seconds = 0.0001f;
		speed = 1.0f / duration_seconds;
		// reset start_time relative to previous start to keep continuity if needed
	}

	bool isDone() const { return is_done; }

	void stop()
	{
		is_done = true;
		start = end;
	}

	/// Cast operator to use this object directly as if it was of type T
	[[nodiscard]] operator T()
	{
		return getValue();
	}

	/// Assign operator to ease transitions
	void operator=(T const& new_value)
	{
		setValue(new_value);
	}
};

*/