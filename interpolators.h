#pragma once // For header guard if saved as a .h file

#include <cmath>     // For std::pow, std::sqrt, std::fabs, std::min, std::max, std::clamp
#include <cstdint>   // For uint32_t
#include <algorithm> // For std::min, std::max, std::clamp

// Define M_PI if it's not available (e.g., on some compilers without _USE_MATH_DEFINES)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// CRITICAL ASSUMPTION: SDL_GetTicks() must be available.
// If not using SDL, replace SDL_GetTicks() with your system's millisecond timer.
// Example: #include <SDL.h> // Usually included in your project's PCH or relevant .cpp files

// --- Base Interpolator ---

class BaseInterpolator {
public:
    BaseInterpolator() = default;
    virtual ~BaseInterpolator() = default;

    // Disable copy and move semantics for simplicity, as state includes start time.
    // Can be enabled if careful consideration is given to copying/moving active interpolations.
    BaseInterpolator(const BaseInterpolator&) = delete;
    BaseInterpolator& operator=(const BaseInterpolator&) = delete;
    BaseInterpolator(BaseInterpolator&&) = delete;
    BaseInterpolator& operator=(BaseInterpolator&&) = delete;

    /**
     * @brief Starts the interpolation.
     * @param durationMillis Total duration of the interpolation in milliseconds.
     */
    virtual void start(uint32_t durationMillis) {
        m_durationMillis = durationMillis > 0 ? durationMillis : 1u; // Avoid division by zero
        m_startTimeMillis = SDL_GetTicks(); // Assumes SDL_GetTicks() is accessible
        m_isRunning = true;
        m_isFinished = false;
    }

    /**
     * @brief Stops the interpolation prematurely.
     */
    void stop() {
        m_isRunning = false;
    }

    /**
     * @brief Resets the interpolator to its initial state, ready to be started again.
     */
    virtual void reset() {
        m_isRunning = false;
        m_isFinished = false;
        m_startTimeMillis = 0;
        m_durationMillis = 1000; // Reset to a default duration
    }

    /**
     * @brief Gets the current interpolated factor (typically 0.0 to 1.0+).
     * Updates the interpolator's state. If the duration has passed, it marks as finished.
     * @return The current eased interpolation factor.
     */
    virtual float getFactor() {
        if (!m_isRunning) {
            return m_isFinished ? ease(1.0f) : ease(0.0f);
        }

        uint32_t currentTimeMillis = SDL_GetTicks();
        uint32_t elapsedTimeMillis = currentTimeMillis - m_startTimeMillis;

        if (elapsedTimeMillis >= m_durationMillis) {
            m_isRunning = false;
            m_isFinished = true;
            return ease(1.0f); // Ensure final eased value is for t=1.0
        }

        float normalizedTime = static_cast<float>(elapsedTimeMillis) / static_cast<float>(m_durationMillis);
        return ease(normalizedTime); // ease function should handle clamping if necessary for its curve
    }

    /**
     * @brief Peeks at the current interpolated factor without changing the running state.
     * Useful for querying the value if an external system drives updates.
     * @return The current eased interpolation factor.
     */
    virtual float peekFactor() const {
        if (!m_isRunning && !m_isFinished) return ease(0.0f); // Not started
        if (m_isFinished) return ease(1.0f); // Finished at target

        // If running, calculate current factor
        uint32_t currentTimeMillis = SDL_GetTicks();
        uint32_t elapsedTimeMillis = currentTimeMillis - m_startTimeMillis;

        if (elapsedTimeMillis >= m_durationMillis) {
            return ease(1.0f); // Effectively at the end
        }
        float normalizedTime = static_cast<float>(elapsedTimeMillis) / static_cast<float>(m_durationMillis);
        return ease(normalizedTime);
    }

    /**
     * @brief Checks if the interpolation is currently active and running.
     */
    bool isRunning() const noexcept {
        return m_isRunning;
    }

    /**
     * @brief Checks if the interpolation has completed its full duration at least once.
     */
    bool hasFinished() const {
        if (m_isFinished) return true;
        if (!m_isRunning) return false; // Not running and not marked finished.
        
        uint32_t currentTimeMillis = SDL_GetTicks();
        return (currentTimeMillis - m_startTimeMillis) >= m_durationMillis;
    }

    /**
     * @brief Sets the total duration of the interpolation.
     * Can be called before start or during interpolation to change its length.
     * @param durationMillis The new duration in milliseconds.
     */
    void setDuration(uint32_t durationMillis) {
        m_durationMillis = durationMillis > 0 ? durationMillis : 1u;
    }

    uint32_t getDuration() const noexcept {
        return m_durationMillis;
    }

    uint32_t getStartTime() const noexcept {
        return m_startTimeMillis;
    }

protected:
    /**
     * @brief The core easing function to be implemented by derived classes.
     * @param t Normalized time, typically from 0.0 to 1.0.
     * @return Eased/interpolated factor, typically 0.0 to 1.0 (can be outside for some effects).
     */
    virtual float ease(float t) const = 0;

    uint32_t m_startTimeMillis = 0;
    uint32_t m_durationMillis = 1000; // Default duration (e.g., 1 second)
    bool m_isRunning = false;
    bool m_isFinished = false; // True if it has run to completion
};

// --- Concrete Interpolator Implementations ---

/**
 * @brief Linear interpolation: progress is constant. factor = t
 */
class LinearInterpolator : public BaseInterpolator {
protected:
    float ease(float t) const override {
        return t;
    }
};

/**
 * @brief Accelerate interpolator (Ease-In): Starts slow and speeds up. factor = t^power
 */
class AccelerateInterpolator : public BaseInterpolator {
private:
    float m_power;
public:
    explicit AccelerateInterpolator(float power = 2.0f) : m_power(std::max(0.0f, power)) {} // Power should be non-negative
    void setPower(float power) { m_power = std::max(0.0f, power); }
protected:
    float ease(float t) const override {
        return std::pow(t, m_power);
    }
};

/**
 * @brief Decelerate interpolator (Ease-Out): Starts fast and slows down. factor = 1 - (1-t)^power
 */
class DecelerateInterpolator : public BaseInterpolator {
private:
    float m_power;
public:
    explicit DecelerateInterpolator(float power = 2.0f) : m_power(std::max(0.0f, power)) {}
    void setPower(float power) { m_power = std::max(0.0f, power); }
protected:
    float ease(float t) const override {
        return 1.0f - std::pow(1.0f - t, m_power);
    }
};

/**
 * @brief Accelerate/Decelerate interpolator (Ease-In-Out): Slow start, speeds up, then slows down.
 */
class AccelerateDecelerateInterpolator : public BaseInterpolator {
private:
    float m_power;
public:
    explicit AccelerateDecelerateInterpolator(float power = 2.0f) : m_power(std::max(0.0f, power)) {}
    void setPower(float power) { m_power = std::max(0.0f, power); }
protected:
    float ease(float t) const override {
        t = std::clamp(t, 0.0f, 1.0f);
        if (t < 0.5f) {
            return std::pow(2.0f, m_power - 1.0f) * std::pow(t, m_power);
        } else {
            return 1.0f - std::pow(-2.0f * t + 2.0f, m_power) / 2.0f;
        }
    }
};

/**
 * @brief Overshoot interpolator: Goes past the target value then settles back at target.
 * This implements an EaseOutBack style, where ease(1.0) = 1.0.
 */
class OvershootInterpolator : public BaseInterpolator {
private:
    float m_tensionFactor; // Multiplier for the 's' constant in EaseOutBack
public:
    // Default tensionFactor=1.0f uses the standard s=1.70158f. Higher values give more overshoot.
    explicit OvershootInterpolator(float tensionFactor = 1.0f) : m_tensionFactor(tensionFactor) {}
    void setTensionFactor(float tensionFactor) { m_tensionFactor = tensionFactor; }
protected:
    float ease(float t) const override {
        const float s = 1.70158f * m_tensionFactor; // 's' in Penner's equations, scaled by tensionFactor
        const float c3 = s + 1.0f;
        // This is the EaseOutBack formula: 1 + c3 * (t-1)^3 + s * (t-1)^2
        float t_minus_1 = t - 1.0f;
        return 1.0f + c3 * std::pow(t_minus_1, 3) + s * std::pow(t_minus_1, 2);
    }
};

/**
 * @brief Anticipate interpolator: Moves back slightly before moving forward.
 * This implements an EaseInBack style.
 */
class AnticipateInterpolator : public BaseInterpolator {
private:
    float m_tensionFactor; // Multiplier for the 's' constant in EaseInBack
public:
    explicit AnticipateInterpolator(float tensionFactor = 1.0f) : m_tensionFactor(tensionFactor) {}
    void setTensionFactor(float tensionFactor) { m_tensionFactor = tensionFactor; }
protected:
    float ease(float t) const override {
        const float s = 1.70158f * m_tensionFactor; // 's' in Penner's equations
        const float c3 = s + 1.0f;
        // This is the EaseInBack formula: c3*t^3 - s*t^2
        return c3 * t * t * t - s * t * t;
    }
};

/**
 * @brief Bounce interpolator: Creates a bouncing effect at the end (EaseOutBounce).
 */
class BounceInterpolator : public BaseInterpolator {
protected:
    float ease(float t) const override {
        // EaseOutBounce formula from easings.net by Robert Penner
        const float n1 = 7.5625f;
        const float d1 = 2.75f;

        if (t < 1.0f / d1) {
            return n1 * t * t;
        } else if (t < 2.0f / d1) {
            t -= 1.5f / d1;
            return n1 * t * t + 0.75f;
        } else if (t < 2.5f / d1) {
            t -= 2.25f / d1;
            return n1 * t * t + 0.9375f;
        } else {
            t -= 2.625f / d1;
            return n1 * t * t + 0.984375f;
        }
    }
};

/**
 * @brief Anticipate/Overshoot interpolator: Combines anticipation and overshoot (EaseInOutBack).
 */
class AnticipateOvershootInterpolator : public BaseInterpolator {
private:
    float m_tensionFactor; // Single tension factor for simplicity, scales 's'
public:
    explicit AnticipateOvershootInterpolator(float tensionFactor = 1.0f) : m_tensionFactor(tensionFactor) {}
    void setTensionFactor(float tensionFactor) { m_tensionFactor = tensionFactor; }
protected:
    float ease(float t) const override {
        // EaseInOutBack formula from easings.net
        const float s = 1.70158f * m_tensionFactor;
        const float s_scaled = s * 1.525f; // Common scaling for this specific formula variant

        t = std::clamp(t, 0.0f, 1.0f); // Ensure t is within [0,1] before scaling
        
        float scaled_t = t * 2.0f;
        if (scaled_t < 1.0f) {
            return 0.5f * (scaled_t * scaled_t * ((s_scaled + 1.0f) * scaled_t - s_scaled));
        } else {
            float term = scaled_t - 2.0f; // term is in [0, -1] effectively as t goes from 0.5 to 1 for scaled_t
                                        // for original t in [0.5, 1], term is in [0, -1]
                                        // No, for original t in [0.5,1], scaled_t is [1,2], term is [-1,0]
            return 0.5f * (term * term * ((s_scaled + 1.0f) * term + s_scaled) + 2.0f);
        }
    }
};


/**
 * @brief An interpolator driven by simple physics (constant deceleration).
 * The resulting curve is a quadratic ease-out when using getFactor().
 * Allows starting by defining an initial velocity or a total distance to cover.
 */
class PhysicsDrivenInterpolator : public BaseInterpolator {
public:
    // decelerationRate: Magnitude of deceleration (e.g., pixels/sec^2).
    // Must be positive.
    explicit PhysicsDrivenInterpolator(float decelerationRate = 9.8f /* e.g., 100 units * 9.8 m/s^2 */)
        : m_decelerationRate(std::fabs(decelerationRate)) {
        if (m_decelerationRate < 0.00001f) m_decelerationRate = 1.0f; // Avoid division by zero or near-zero
    }

    /**
     * @brief Starts the interpolation with an initial velocity.
     * Duration and distance are calculated based on this velocity and the deceleration rate.
     * @param initialVelocity Pixels per second (magnitude).
     */
    void startWithVelocity(float initialVelocity) {
        m_initialVelocity = std::fabs(initialVelocity);
        if (m_initialVelocity <= 0.001f) { // Effectively zero
            BaseInterpolator::start(0); // Finishes immediately
            m_calculatedTotalDistance = 0.0f;
            return;
        }
        // Duration (t = v0 / a)
        uint32_t durationMs = static_cast<uint32_t>((m_initialVelocity / m_decelerationRate) * 1000.0f);
        // Distance (d = v0^2 / (2a))
        m_calculatedTotalDistance = (m_initialVelocity * m_initialVelocity) / (2.0f * m_decelerationRate);
        
        BaseInterpolator::start(durationMs);
    }

    /**
     * @brief Starts the interpolation to cover a specific distance.
     * Initial velocity and duration are calculated.
     * @param targetDistance Total pixels to travel (magnitude).
     */
    void startWithDistance(float targetDistance) {
        m_calculatedTotalDistance = std::fabs(targetDistance);
        if (m_calculatedTotalDistance <= 0.001f) {
            BaseInterpolator::start(0);
            m_initialVelocity = 0.0f;
            return;
        }
        // Initial velocity (v0 = sqrt(2ad))
        m_initialVelocity = std::sqrt(2.0f * m_decelerationRate * m_calculatedTotalDistance);
        // Duration (t = v0 / a)
        uint32_t durationMs = static_cast<uint32_t>((m_initialVelocity / m_decelerationRate) * 1000.0f);

        BaseInterpolator::start(durationMs);
    }

    /**
     * @brief Overrides base start to make it consistent with the physics model.
     * Calculates an initial velocity to cover a conceptual "distance" (which its ease function normalizes)
     * over the given duration with the set deceleration.
     * @param durationMillis The duration in milliseconds.
     */
    void start(uint32_t durationMillis) override {
        BaseInterpolator::start(durationMillis); // Sets m_startTimeMillis, m_durationMillis, m_isRunning, m_isFinished
        float durationSeconds = static_cast<float>(m_durationMillis) / 1000.0f;
        
        if (durationSeconds <= 0.00001f) { // Effectively zero duration
             m_initialVelocity = 0.0f;
             m_calculatedTotalDistance = 0.0f;
             m_isRunning = false; // Should finish immediately
             m_isFinished = true;
             return;
        }
        // From v_final = v0 - a*t => 0 = v0 - a*t_total => v0 = a*t_total
        m_initialVelocity = m_decelerationRate * durationSeconds;
        // Total distance: d = v0*t_total - 0.5*a*t_total^2
        m_calculatedTotalDistance = m_initialVelocity * durationSeconds - 0.5f * m_decelerationRate * durationSeconds * durationSeconds;
    }

    /**
     * @brief Gets the raw displacement (distance traveled so far) based on physics.
     * This is not normalized by default and represents the "physical" travel.
     * @return Current displacement in the units used for velocity/distance (e.g., pixels).
     */
    float getCurrentDisplacement() {
        if (!isRunning()) { // Check if it's running via BaseInterpolator's state
            return m_isFinished ? m_calculatedTotalDistance : 0.0f;
        }
        // getFactor() will update isRunning and isFinished, so call it to sync state.
        // However, we need the raw time for calculation here.
        uint32_t currentTimeMillis = SDL_GetTicks();
        uint32_t elapsedTimeMillis = currentTimeMillis - m_startTimeMillis;
        float tSeconds = static_cast<float>(elapsedTimeMillis) / 1000.0f;
        
        float totalDurationSeconds = static_cast<float>(m_durationMillis) / 1000.0f;

        if (tSeconds >= totalDurationSeconds) {
             // Let getFactor handle the state change if called, but for raw displacement:
            return m_calculatedTotalDistance;
        }
        // Displacement: s = v0*t - 0.5*a*t^2 (using a as positive m_decelerationRate)
        float displacement = m_initialVelocity * tSeconds - 0.5f * m_decelerationRate * tSeconds * tSeconds;
        return std::max(0.0f, std::min(displacement, m_calculatedTotalDistance)); // Clamp to [0, total_distance]
    }

    /**
     * @brief Gets the total distance this interpolator is configured to travel based on physics setup.
     */
    float getConfiguredTotalDistance() const { return m_calculatedTotalDistance; }

protected:
    // The physics of constant deceleration results in a quadratic ease-out curve.
    // Normalized time 't' (0 to 1) is mapped to this curve.
    // Formula: factor = t * (2 - t) which is equivalent to EaseOutQuad.
    float ease(float t) const override { // t is normalizedTime
        t = std::clamp(t, 0.0f, 1.0f);
        return t * (2.0f - t);
    }

private:
    float m_decelerationRate;       // Magnitude of deceleration (e.g., pixels/sec^2)
    float m_initialVelocity = 0.0f;   // Calculated or set initial velocity (pixels/sec)
    float m_calculatedTotalDistance = 0.0f; // Calculated total distance for the motion (pixels)
};